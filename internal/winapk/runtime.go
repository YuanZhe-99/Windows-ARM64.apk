package winapk

import (
	"bufio"
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"image"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"strconv"
	"strings"
	"sync"
	"time"
)

// Runtime implements Client against the real native ARM64 `winapk.exe` CLI
// and per-APK runtime processes.
type Runtime struct {
	exe string
	// JavaFeedURL overrides the OpenJDK checksum/archive base URL in tests.
	JavaFeedURL string
}

// FindExecutable locates `winapk.exe` from WINAPK_CLI, beside the running
// binary (`bin\winapk.exe` or `winapk.exe`), or in `build\windows-arm64\bin\winapk.exe`.
func FindExecutable() string {
	if p := os.Getenv("WINAPK_CLI"); p != "" {
		if fi, err := os.Stat(p); err == nil && !fi.IsDir() {
			return p
		}
	}
	if self, err := os.Executable(); err == nil {
		dir := filepath.Dir(self)
		for _, cand := range []string{
			filepath.Join(dir, "bin", "winapk.exe"),
			filepath.Join(dir, "winapk.exe"),
			filepath.Join(filepath.Dir(dir), "bin", "winapk.exe"),
		} {
			if fi, err := os.Stat(cand); err == nil && !fi.IsDir() {
				return cand
			}
		}
	}
	if cwd, err := os.Getwd(); err == nil {
		for dir := cwd; ; dir = filepath.Dir(dir) {
			cand := filepath.Join(dir, "build", "windows-arm64", "bin", "winapk.exe")
			if fi, err := os.Stat(cand); err == nil && !fi.IsDir() {
				return cand
			}
			parent := filepath.Dir(dir)
			if parent == dir {
				break
			}
		}
	}
	return ""
}

// NewRuntime creates a real Client targeting exe (or FindExecutable() when "").
func NewRuntime(exe string) *Runtime {
	if exe == "" {
		exe = FindExecutable()
	}
	return &Runtime{exe: exe}
}

// Executable returns the resolved `winapk.exe` path, or "" if none was found.
func (r *Runtime) Executable() string { return r.exe }

type doctorJSON struct {
	Version string `json:"version"`
	Host    struct {
		OSBuild     string `json:"osBuild"`
		OSArch      string `json:"osArch"`
		ProcessArch string `json:"processArch"`
		CPU         string `json:"cpu"`
		MemoryMB    uint64 `json:"memoryMB"`
	} `json:"host"`
	JVM struct {
		Home    string `json:"home"`
		Source  string `json:"source"`
		Vendor  string `json:"vendor"`
		Version string `json:"version"`
		Arch    string `json:"arch"`
		Started string `json:"started"`
	} `json:"jvm"`
	Graphics struct {
		Adapters []struct {
			Name          string `json:"name"`
			VendorID      string `json:"vendorId"`
			DeviceID      string `json:"deviceId"`
			DriverVersion string `json:"driverVersion"`
			Software      bool   `json:"software"`
		} `json:"adapters"`
		VulkanLoader          string `json:"vulkanLoader"`
		VulkanInstanceVersion string `json:"vulkanInstanceVersion"`
		Win32SurfaceExtension bool   `json:"win32SurfaceExtension"`
		Devices               []struct {
			Name              string `json:"name"`
			VendorID          string `json:"vendorId"`
			DeviceID          string `json:"deviceId"`
			DriverVersion     string `json:"driverVersion"`
			APIVersion        string `json:"apiVersion"`
			Type              string `json:"type"`
			DriverName        string `json:"driverName"`
			DriverInfo        string `json:"driverInfo"`
			Translated        bool   `json:"translated"`
			QueueFamilies     int    `json:"queueFamilies"`
			Win32Presentation bool   `json:"win32Presentation"`
		} `json:"devices"`
		Win32Present    string `json:"win32Present"`
		AngleFilesArm64 bool   `json:"angleFilesArm64"`
		AngleBackends   []struct {
			Backend    string `json:"backend"`
			GLES       string `json:"gles"`
			Renderer   string `json:"renderer"`
			Translated bool   `json:"translated"`
		} `json:"angleBackends"`
	} `json:"graphics"`
	Runtime struct {
		GuestNativeProfiles []string `json:"guestNativeProfiles"`
		GuestVulkan         bool     `json:"guestVulkan"`
		FrameworkCommit     string   `json:"frameworkCommit"`
	} `json:"runtime"`
	Limitations []Limitation `json:"limitations"`
}

// Doctor runs `winapk doctor --json` and maps its inventory into Report.
func (r *Runtime) Doctor(ctx context.Context, javaHome string) (Report, error) {
	if r.exe == "" {
		return Report{}, ErrNoRuntime
	}
	args := []string{"doctor", "--json"}
	if javaHome != "" {
		args = append(args, "--java-home", javaHome)
	}
	cmd := exec.CommandContext(ctx, r.exe, args...)
	hideConsole(cmd)
	out, err := cmd.Output()
	if err != nil {
		return Report{}, fmt.Errorf("winapk doctor: %w", err)
	}
	var raw doctorJSON
	if err := json.Unmarshal(out, &raw); err != nil {
		return Report{}, fmt.Errorf("winapk doctor parse: %w", err)
	}

	nativeJava := raw.JVM.Arch == "arm64" && raw.Host.ProcessArch == "arm64"
	javaOK := raw.JVM.Home != "" && raw.JVM.Started == "ok" && nativeJava
	own := raw.JVM.Source == "own"
	if !own && raw.JVM.Home != "" {
		own = strings.EqualFold(filepath.Clean(raw.JVM.Home), filepath.Clean(DefaultRuntimeDir()))
	}
	var installedAt string
	if own {
		installedAt = readRuntimeMeta(raw.JVM.Home).InstalledAt
	}

	var gpuName, vendorID, deviceID, driverVer string
	for _, ad := range raw.Graphics.Adapters {
		if !ad.Software || gpuName == "" {
			gpuName, vendorID, deviceID, driverVer = ad.Name, ad.VendorID, ad.DeviceID, ad.DriverVersion
			if !ad.Software {
				break
			}
		}
	}

	var vkVer, vkDev, vkDrv string
	var vkTranslated bool
	var vkQueues int
	for _, dv := range raw.Graphics.Devices {
		if dv.Type != "cpu" || vkDev == "" {
			vkDev, vkVer, vkDrv = dv.Name, dv.APIVersion, dv.DriverName
			if dv.DriverInfo != "" {
				vkDrv = strings.TrimSpace(vkDrv + " " + dv.DriverInfo)
			}
			vkTranslated = dv.Translated
			vkQueues = dv.QueueFamilies
			if dv.Type != "cpu" {
				break
			}
		}
	}
	if vkVer == "" {
		vkVer = raw.Graphics.VulkanInstanceVersion
	}

	hasProf := func(p string) Level {
		if slices.Contains(raw.Runtime.GuestNativeProfiles, p) {
			return Experimental
		}
		return Unavailable
	}
	javaLevel := Unavailable
	javaDetail := raw.JVM.Started
	if javaOK {
		javaLevel = Works
		javaDetail = "arm64 · not emulated"
	} else if raw.JVM.Home != "" && !nativeJava {
		javaDetail = raw.JVM.Arch + " · not native arm64"
	}

	glesLevel := Unavailable
	glesDetail := "ANGLE DLLs not installed"
	if raw.Graphics.AngleFilesArm64 {
		glesLevel = Experimental
		if len(raw.Graphics.AngleBackends) > 0 {
			var parts []string
			for _, ab := range raw.Graphics.AngleBackends {
				label := ab.Backend
				switch ab.Backend {
				case "d3d11":
					label = "Direct3D 11"
				case "vulkan":
					label = "Vulkan"
					if ab.Translated {
						label += " (Dozen)"
					}
				}
				if ab.GLES != "" {
					label += " (" + ab.GLES + ")"
				}
				parts = append(parts, label)
			}
			glesDetail = "ANGLE · " + strings.Join(parts, " · ")
		} else if raw.Graphics.VulkanLoader == "available" && len(raw.Graphics.Devices) > 0 {
			if vkTranslated {
				glesDetail = "ANGLE · Direct3D 11 · Vulkan (Dozen)"
			} else {
				glesDetail = "ANGLE · Direct3D 11 · Vulkan"
			}
		} else {
			glesDetail = "ANGLE · Direct3D 11"
		}
	}

	vkLevel := Unavailable
	vkDetail := "loader " + raw.Graphics.VulkanLoader
	if raw.Graphics.VulkanLoader == "available" && len(raw.Graphics.Devices) > 0 {
		if vkTranslated {
			vkDetail = "translated (Dozen on D3D12) · guestVulkan: false"
		} else {
			vkDetail = "driver works · guestVulkan: false"
		}
		if raw.Runtime.GuestVulkan {
			vkLevel = Experimental
		}
	}

	return Report{
		Version: raw.Version,
		RawJSON: bytes.TrimSpace(out),
		Java: Java{
			Found:       javaOK,
			Own:         own,
			Version:     raw.JVM.Version,
			Vendor:      raw.JVM.Vendor,
			Path:        raw.JVM.Home,
			Native:      nativeJava,
			Started:     raw.JVM.Started,
			InstalledAt: installedAt,
		},
		Machine: Machine{
			Windows:          raw.Host.OSBuild + " · " + raw.Host.OSArch,
			OSBuild:          raw.Host.OSBuild,
			OSArch:           raw.Host.OSArch,
			ProcessArch:      raw.Host.ProcessArch,
			CPU:              raw.Host.CPU,
			MemoryMB:         raw.Host.MemoryMB,
			GPU:              gpuName,
			VendorID:         vendorID,
			DeviceID:         deviceID,
			Driver:           driverVer,
			Vulkan:           vkVer,
			VulkanLoader:     raw.Graphics.VulkanLoader,
			VulkanInstance:   raw.Graphics.VulkanInstanceVersion,
			VulkanDevice:     vkDev,
			VulkanDriverName: vkDrv,
			VulkanTranslated: vkTranslated,
			VulkanQueues:     vkQueues,
			Win32Surface:     raw.Graphics.Win32SurfaceExtension,
			Win32Present:     raw.Graphics.Win32Present,
			AngleArm64:       raw.Graphics.AngleFilesArm64,
			FrameworkCommit:  raw.Runtime.FrameworkCommit,
		},
		Features: []Feature{
			{ID: "java", Level: javaLevel, Detail: javaDetail},
			{ID: "native-n0", Level: hasProf("N0"), Detail: string(hasProf("N0"))},
			{ID: "native-n1", Level: hasProf("N1"), Detail: string(hasProf("N1"))},
			{ID: "native-n2", Level: hasProf("N2"), Detail: string(hasProf("N2"))},
			{ID: "gles", Level: glesLevel, Detail: glesDetail},
			{ID: "vulkan", Level: vkLevel, Detail: vkDetail},
		},
		Limitations: raw.Limitations,
	}, nil
}

type inspectJSON struct {
	OK          bool     `json:"ok"`
	Error       string   `json:"error"`
	Package     string   `json:"package"`
	Label       string   `json:"label"`
	VersionName string   `json:"versionName"`
	Size        int64    `json:"size"`
	MinSDK      int      `json:"minSdk"`
	TargetSDK   int      `json:"targetSdk"`
	GLESVersion string   `json:"glesVersion"`
	Vulkan      bool     `json:"vulkan"`
	Profile     string   `json:"profile"`
	Verdict     string   `json:"verdict"`
	Permissions []string `json:"permissions"`
	NativeABIs  []string `json:"nativeAbis"`
	NativeLibs  []string `json:"nativeLibs"`
	IconPNG     string   `json:"iconPng"`
}

// Inspect runs `winapk inspect <path> --json` without executing any APK code.
func (r *Runtime) Inspect(ctx context.Context, path string) (APK, error) {
	if r.exe == "" {
		return APK{}, ErrNoRuntime
	}
	cmd := exec.CommandContext(ctx, r.exe, "inspect", path, "--json")
	hideConsole(cmd)
	out, err := cmd.Output()
	var raw inspectJSON
	if jerr := json.Unmarshal(out, &raw); jerr != nil {
		if err != nil {
			return APK{}, fmt.Errorf("winapk inspect: %w", err)
		}
		return APK{}, fmt.Errorf("winapk inspect parse: %w", jerr)
	}
	if !raw.OK {
		return APK{}, fmt.Errorf("winapk inspect: %s", raw.Error)
	}
	var icon []byte
	if raw.IconPNG != "" {
		icon, _ = base64.StdEncoding.DecodeString(raw.IconPNG)
	}
	ver := raw.VersionName
	if ver == "" {
		ver = "1.0"
	}
	return APK{
		Path:        path,
		Package:     raw.Package,
		Label:       raw.Label,
		Version:     ver,
		Size:        raw.Size,
		MinSDK:      raw.MinSDK,
		TargetSDK:   raw.TargetSDK,
		ABIs:        raw.NativeABIs,
		NativeLibs:  raw.NativeLibs,
		Permissions: raw.Permissions,
		GLES:        raw.GLESVersion != "",
		GLESVersion: raw.GLESVersion,
		Vulkan:      raw.Vulkan,
		Profile:     Profile(raw.Profile),
		Verdict:     Verdict(raw.Verdict),
		Icon:        icon,
		SHA256:      shortFileSHA256(path),
	}, nil
}

func shortFileSHA256(path string) string {
	f, err := os.Open(path)
	if err != nil {
		return ""
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return ""
	}
	return hex.EncodeToString(h.Sum(nil))
}

// Launch starts one `winapk run --embed` child process for spec and streams its
// state, structured logs and shared-memory RGBA8 frames until it exits.
func (r *Runtime) Launch(spec LaunchSpec, report func(State)) (Process, error) {
	if r.exe == "" {
		return nil, ErrNoRuntime
	}
	ch, err := newEmbedChannel()
	if err != nil {
		return nil, err
	}
	w, h := spec.Width, spec.Height
	if w <= 0 || h <= 0 {
		w, h = 360, 720
	}
	native := spec.Native
	if native == "" {
		native = NativeOff
	}
	gfx := spec.Graphics
	if gfx == "" {
		gfx = "auto"
	}
	dataDir := spec.DataDir
	if dataDir == "" {
		dataDir = DefaultDataDir()
	}

	args := []string{
		"run", spec.Path,
		"--embed", ch.token,
		"--native=" + string(native),
		"--graphics=" + gfx,
		"--size", fmt.Sprintf("%dx%d", w, h),
	}
	if native == NativeRestricted && spec.Profile != "" {
		args = append(args, "--profile="+string(spec.Profile))
	}
	if dataDir != "" {
		args = append(args, "--data-dir", dataDir)
	}
	if spec.JavaHome != "" {
		args = append(args, "--java-home", spec.JavaHome)
	}

	var logFile *os.File
	var logPath string
	if dataDir != "" {
		logsDir := LogsDir(dataDir)
		if os.MkdirAll(logsDir, 0o700) == nil {
			base := strings.TrimSuffix(filepath.Base(spec.Path), filepath.Ext(spec.Path))
			logPath = filepath.Join(logsDir, fmt.Sprintf("%s-%s.log", sanitizeFile(base), time.Now().Format("20060102-150405.000")))
			logFile, _ = os.OpenFile(logPath, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0o600)
		}
	}

	cmd := exec.Command(r.exe, args...)
	hideConsole(cmd)
	stderr, err := cmd.StderrPipe()
	if err != nil {
		if logFile != nil {
			_ = logFile.Close()
		}
		ch.close()
		return nil, err
	}
	if err := cmd.Start(); err != nil {
		if logFile != nil {
			_ = logFile.Close()
		}
		ch.close()
		return nil, err
	}

	p := &runtimeProcess{
		cmd:     cmd,
		ch:      ch,
		report:  report,
		logFile: logFile,
		done:    make(chan struct{}),
		st: State{
			Phase:   Starting,
			PID:     cmd.Process.Pid,
			LogPath: logPath,
		},
	}
	p.emit()
	go p.supervise(stderr)
	return p, nil
}

type runtimeProcess struct {
	cmd        *exec.Cmd
	ch         *embedChannel
	report     func(State)
	logFile    *os.File
	done       chan struct{}
	mu         sync.Mutex
	st         State
	userStop   bool
	onFrame    func(*image.RGBA)
	lastStderr string
}

func (p *runtimeProcess) emit() {
	if p.report == nil {
		return
	}
	p.mu.Lock()
	snap := p.st
	snap.Log = append([]string(nil), p.st.Log...)
	snap.Entries = append([]LogEntry(nil), p.st.Entries...)
	p.mu.Unlock()
	p.report(snap)
}

func (p *runtimeProcess) supervise(stderr io.Reader) {
	var wg sync.WaitGroup
	wg.Add(2)

	// 1. Accept and read the --embed event stream (frames, backend, running, tid-tagged logs).
	go func() {
		defer wg.Done()
		pipe, err := p.ch.accept(p.done)
		if err != nil {
			return
		}
		sc := bufio.NewScanner(pipe)
		sc.Buffer(make([]byte, 64*1024), 1024*1024)
		for sc.Scan() {
			p.handleEmbedEvent(sc.Text())
		}
	}()

	// 2. Read stderr lines (pre-JVM stages, fatal guest terminations, Java exceptions).
	go func() {
		defer wg.Done()
		sc := bufio.NewScanner(stderr)
		sc.Buffer(make([]byte, 64*1024), 1024*1024)
		for sc.Scan() {
			p.recordLine("", sc.Text(), false)
		}
	}()

	waitErr := p.cmd.Wait()
	close(p.done)
	p.ch.close()
	wg.Wait()

	exitCode := 0
	if waitErr != nil {
		var ee *exec.ExitError
		if errors.As(waitErr, &ee) {
			exitCode = ee.ExitCode()
		} else {
			exitCode = 1
		}
	}

	p.mu.Lock()
	p.st.ExitCode = exitCode
	if p.logFile != nil {
		_ = p.logFile.Close()
		p.logFile = nil
	}
	if p.userStop || exitCode == 0 {
		p.st.Phase = Stopped
		p.st.Category = ""
	} else {
		p.st.Phase = Failed
		if p.st.Category == "" {
			p.st.Category = "APP_CRASHED"
		}
	}
	p.mu.Unlock()
	p.emit()
}

func (p *runtimeProcess) handleEmbedEvent(line string) {
	switch {
	case strings.HasPrefix(line, "hello "):
		if pid, err := strconv.Atoi(strings.TrimSpace(line[6:])); err == nil && pid > 0 {
			p.mu.Lock()
			p.st.PID = pid
			p.mu.Unlock()
		}
	case strings.HasPrefix(line, "backend "):
		b := strings.TrimSpace(line[8:])
		p.mu.Lock()
		changed := p.st.Graphics != b
		p.st.Graphics = b
		p.mu.Unlock()
		if changed {
			p.emit()
		}
	case line == "state running":
		p.mu.Lock()
		changed := p.st.Phase == Starting
		if changed {
			p.st.Phase = Running
		}
		p.mu.Unlock()
		if changed {
			p.emit()
		}
	case strings.HasPrefix(line, "frame "):
		img := p.ch.readFrame()
		if img == nil {
			return
		}
		p.mu.Lock()
		wasStarting := p.st.Phase == Starting
		if wasStarting {
			p.st.Phase = Running
		}
		fn := p.onFrame
		p.mu.Unlock()
		if wasStarting {
			p.emit()
		}
		if fn != nil {
			fn(img)
		}
	case strings.HasPrefix(line, "log "):
		rest := line[4:]
		sp := strings.IndexByte(rest, ' ')
		if sp > 0 {
			p.recordLine(rest[:sp], rest[sp+1:], true)
		}
	}
}

var knownCategories = []string{
	"HOST_ARCH_UNSUPPORTED",
	"DEPENDENCY_MISSING",
	"INSTALL_FAILED_INVALID_APK",
	"DEX_CONVERSION_FAILED",
	"GUEST_NATIVE_DISABLED",
	"ELF_MALFORMED",
	"ELF_UNSUPPORTED",
	"ELF_RELOCATION_UNSUPPORTED",
	"GUEST_SYMBOL_UNRESOLVED",
	"GUEST_ABI_UNSUPPORTED",
	"GUEST_EXECUTION_FAULT",
	"HOST_MEMORY_FAILED",
	"ANGLE_DLLS_MISSING",
	"GRAPHICS_CAPABILITY_MISSING",
	"GRAPHICS_RENDER_FAILED",
	"VULKAN_DRIVER_TRANSLATED",
	"GUEST_VULKAN_NOT_IMPLEMENTED",
	"UI_THREAD_BLOCKED",
	"UI_CALL_TIMEOUT",
	"APP_CRASHED",
	"IO_ERROR",
}

func (p *runtimeProcess) recordLine(tid, raw string, fromPipe bool) {
	raw = strings.TrimRight(raw, "\r\n")
	if raw == "" {
		return
	}
	now := time.Now().Format("15:04:05.000")
	level, stage, msg, cat := classifyLogLine(raw)

	p.mu.Lock()
	// Avoid duplicating identical lines delivered on both stderr and the embed pipe.
	if !fromPipe && raw == p.lastStderr {
		p.mu.Unlock()
		return
	}
	if fromPipe {
		p.lastStderr = raw
	}
	pid := p.st.PID
	pidTid := strconv.Itoa(pid)
	if tid != "" {
		pidTid += "/" + tid
	} else {
		pidTid += "/" + strconv.Itoa(pid)
	}
	formatted := fmt.Sprintf("%s  %-5s %-12s %s", now, level, stage, msg)
	p.st.Log = append(p.st.Log, formatted)
	p.st.Entries = append(p.st.Entries, LogEntry{
		Time: now, PIDTID: pidTid, Level: level, Stage: stage, Message: msg,
	})
	if p.logFile != nil {
		_, _ = p.logFile.WriteString(formatted + "\n")
	}
	if cat != "" && cat != "UI_THREAD_BLOCKED" {
		p.st.Category = cat
		p.st.Failure.Stage = stage
		p.st.Failure.Detail = msg
		parseFailureTokens(&p.st.Failure, msg)
	}
	//keep bounded memory over long endurance runs (last 1000 lines in RAM; full log stays on disk)
	if len(p.st.Log) > 1000 {
		p.st.Log = append([]string(nil), p.st.Log[len(p.st.Log)-800:]...)
		p.st.Entries = append([]LogEntry(nil), p.st.Entries[len(p.st.Entries)-800:]...)
	}
	p.mu.Unlock()
	if level == "error" || level == "warn" {
		p.emit()
	}
}

func classifyLogLine(raw string) (level, stage, msg, category string) {
	level, stage, msg = "info", "runtime", raw
	if strings.HasPrefix(raw, "[winapk] ") {
		msg = strings.TrimPrefix(raw, "[winapk] ")
		switch {
		case strings.HasPrefix(msg, "stage inspect"), strings.HasPrefix(msg, "package "):
			stage = "launch"
		case strings.Contains(msg, "dex"):
			stage = "dex.convert"
		case strings.HasPrefix(msg, "stage jvm-created"):
			stage = "jvm.start"
		case strings.Contains(msg, "GLES"), strings.Contains(msg, "EGL"), strings.Contains(msg, "Vulkan"):
			stage = "gfx.select"
		case strings.HasPrefix(msg, "stage guest-libraries"):
			stage = "jni.load"
		case strings.HasPrefix(msg, "stage exit"):
			stage = "shutdown"
		}
	} else if len(raw) >= 4 && raw[0] == '[' && raw[2] == ']' && raw[3] == ' ' {
		switch raw[1] {
		case 'W':
			level = "warn"
		case 'E':
			level = "error"
		}
		stage = "app"
		msg = raw[4:]
	}
	for _, c := range knownCategories {
		if strings.Contains(raw, c) {
			category = c
			if c == "UI_THREAD_BLOCKED" {
				level = "warn"
			} else {
				level = "error"
			}
			switch {
			case strings.HasPrefix(c, "ELF_"):
				stage = "elf.map"
			case strings.HasPrefix(c, "GUEST_SYMBOL"), strings.HasPrefix(c, "GUEST_ABI"), c == "GUEST_NATIVE_DISABLED":
				stage = "jni.load"
			case strings.HasPrefix(c, "GRAPHICS_"), c == "ANGLE_DLLS_MISSING":
				stage = "gfx.select"
			case c == "DEX_CONVERSION_FAILED":
				stage = "dex.convert"
			}
			break
		}
	}
	return level, stage, msg, category
}

func parseFailureTokens(f *FailureInfo, msg string) {
	for _, tok := range strings.Fields(msg) {
		clean := strings.Trim(tok, "():,;\"")
		if strings.HasSuffix(clean, ".so") && f.Module == "" {
			f.Module = clean
		} else if (strings.HasPrefix(clean, "0x") || strings.HasPrefix(clean, "+0x")) && f.Where == "" {
			f.Where = clean
		}
	}
}

func sanitizeFile(s string) string {
	var b strings.Builder
	for _, r := range s {
		if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') || r == '.' || r == '-' || r == '_' {
			b.WriteRune(r)
		} else {
			b.WriteByte('_')
		}
	}
	if b.Len() == 0 {
		return "app"
	}
	return b.String()
}

// Stop asks the app window to close gracefully via the embed pipe.
func (p *runtimeProcess) Stop() error {
	p.mu.Lock()
	p.userStop = true
	p.mu.Unlock()
	return p.ch.send("stop")
}

// Kill terminates the runtime process immediately (its Job Object reaps any DEX child).
func (p *runtimeProcess) Kill() error {
	p.mu.Lock()
	p.userStop = true
	p.mu.Unlock()
	if p.cmd.Process == nil {
		return nil
	}
	return p.cmd.Process.Kill()
}

// Back sends Android KEYCODE_BACK to the app.
func (p *runtimeProcess) Back() error {
	return p.ch.send("back")
}

// Resize updates the app's screen dimensions in DIPs.
func (p *runtimeProcess) Resize(w, h int) error {
	if w <= 0 || h <= 0 {
		return nil
	}
	return p.ch.send(fmt.Sprintf("resize %d %d", w, h))
}

// Reattach returns true: shared-memory *ui.Bitmap frames move between panes
// and standalone windows without restarting the runtime process.
func (p *runtimeProcess) Reattach(bool) bool { return true }

// SetFrameHandler registers fn to receive newly published RGBA8 frames.
func (p *runtimeProcess) SetFrameHandler(fn func(*image.RGBA)) {
	p.mu.Lock()
	p.onFrame = fn
	p.mu.Unlock()
}

// SendPointer forwards a pointer event in DIPs.
func (p *runtimeProcess) SendPointer(action int, x, y float32) {
	_ = p.ch.send(fmt.Sprintf("pointer %d %.2f %.2f", action, x, y))
}

// SendWheel forwards wheel scrolling in notches at (x, y) DIPs.
func (p *runtimeProcess) SendWheel(x, y, dx, dy float32) {
	_ = p.ch.send(fmt.Sprintf("wheel %.2f %.2f %.3f %.3f", x, y, dx, dy))
}

// SendKey forwards a Windows virtual key event.
func (p *runtimeProcess) SendKey(down bool, vk, unicode, meta int) {
	act := 0
	if down {
		act = 1
	}
	_ = p.ch.send(fmt.Sprintf("key %d %d %d %d", act, vk, unicode, meta))
}

// SendText forwards UTF-8 text (hex-encoded on the wire) as Android composed text.
func (p *runtimeProcess) SendText(text string) {
	if text == "" {
		return
	}
	_ = p.ch.send("text " + hex.EncodeToString([]byte(text)))
}
