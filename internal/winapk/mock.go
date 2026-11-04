package winapk

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

// Mock stands in for the runtime until it exists: sample apps, a sample
// doctor report marked Demo, and processes that only report states. No
// APK is read and nothing runs.
type Mock struct {
	mu   sync.Mutex
	java Java
	// step is how long each simulated wait takes; tests set it to zero.
	step time.Duration
}

// NewMock makes a Mock; withJava says whether this PC already has a Java
// runtime, else the interface offers to get one.
func NewMock(withJava bool, step time.Duration) *Mock {
	m := &Mock{step: step}
	if withJava {
		m.java = mockJava(false)
	}
	return m
}

func mockJava(own bool) Java {
	j := Java{Found: true, Own: own, Version: "21.0.5+11-LTS", Vendor: "Microsoft", Native: true, Path: `C:\Program Files\Microsoft\jdk-21.0.5.11-hotspot`}
	if own {
		j.Path = `%LOCALAPPDATA%\Windows-ARM64.apk\Runtime`
	}
	return j
}

// Samples are the sample apps of the design (design/README.md).
func (m *Mock) Samples() []APK { return samples() }

func samples() []APK {
	return []APK{
		{Path: "sample:NotesLite.apk", Package: "com.example.noteslite", Label: "Notes Lite", Version: "1.4", Size: 3_100_000, MinSDK: 24, TargetSDK: 34, Verdict: CanRun},
		{Path: "sample:GLTriangle.apk", Package: "com.example.gltriangle", Label: "GL Triangle", Version: "1.0", Size: 2_400_000, MinSDK: 26, TargetSDK: 34, ABIs: []string{"arm64-v8a"}, NativeLibs: []string{"lib/arm64-v8a/libgltriangle.so"}, GLES: true, Profile: N2, Verdict: CanRun},
		{Path: "sample:HelloJava.apk", Package: "com.example.hellojava", Label: "Hello Java", Version: "1.0", Size: 900_000, MinSDK: 21, TargetSDK: 34, Verdict: CanRun},
		{Path: "sample:TilePuzzle-2.3.apk", Package: "com.example.tilepuzzle", Label: "Tile Puzzle", Version: "2.3", Size: 8_400_000, MinSDK: 24, TargetSDK: 34, ABIs: []string{"arm64-v8a", "armeabi-v7a"}, NativeLibs: []string{"lib/arm64-v8a/libtiles.so", "lib/arm64-v8a/libpng16.so", "lib/armeabi-v7a/libtiles.so"}, GLES: true, Profile: N1, Verdict: NeedsNative},
		{Path: "sample:VulkanCube.apk", Package: "com.example.vulkancube", Label: "Vulkan Cube", Version: "0.9", Size: 5_200_000, MinSDK: 28, TargetSDK: 34, ABIs: []string{"arm64-v8a"}, NativeLibs: []string{"lib/arm64-v8a/libcube.so"}, Vulkan: true, Profile: N2, Verdict: NotYet},
		{Path: "sample:OldCalc.apk", Package: "com.example.oldcalc", Label: "Old Calc", Version: "3.1", Size: 1_200_000, MinSDK: 14, TargetSDK: 19, ABIs: []string{"armeabi-v7a"}, NativeLibs: []string{"lib/armeabi-v7a/libcalc.so"}, Verdict: Unsupported},
	}
}

func (m *Mock) wait(ctx context.Context, n int) error {
	select {
	case <-time.After(time.Duration(n) * m.step):
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

// Doctor returns the sample report of design/Diagnostics.html.
func (m *Mock) Doctor(ctx context.Context, javaHome string) (Report, error) {
	if err := m.wait(ctx, 1); err != nil {
		return Report{}, err
	}
	m.mu.Lock()
	j := m.java
	m.mu.Unlock()
	if javaHome != "" {
		j = mockJava(false)
		j.Path = javaHome
	}
	return Report{
		Demo: true, Version: "sample", Java: j,
		Machine: Machine{Windows: "10.0.26300.1000 · arm64", CPU: "Snapdragon X Elite · arm64", GPU: "Qualcomm Adreno X1-85", Driver: "31.0.97.0", Vulkan: "1.3.295"},
		Features: []Feature{
			{ID: "java", Level: Works, Detail: "arm64 · not emulated"},
			{ID: "native-n0", Level: Experimental, Detail: "available"},
			{ID: "native-n1", Level: Experimental, Detail: "available"},
			{ID: "native-n2", Level: Experimental, Detail: "available"},
			{ID: "gles", Level: Experimental, Detail: "ANGLE · Direct3D 11 FL 12_1 · Vulkan"},
			{ID: "vulkan", Level: Unavailable, Detail: "driver works · guestVulkan: false"},
		},
	}, nil
}

// Inspect returns a sample app's description, or for any other file one of
// a Java-only app named after it. The file is not read.
func (m *Mock) Inspect(ctx context.Context, path string) (APK, error) {
	if err := m.wait(ctx, 1); err != nil {
		return APK{}, err
	}
	base := filepath.Base(strings.TrimPrefix(path, "sample:"))
	for _, s := range samples() {
		if strings.EqualFold(filepath.Base(strings.TrimPrefix(s.Path, "sample:")), base) {
			s.Path = path
			return s, nil
		}
	}
	name := strings.TrimSuffix(base, filepath.Ext(base))
	a := APK{Path: path, Package: "com.example." + strings.ToLower(strings.ReplaceAll(name, " ", "")), Label: name, Version: "1.0", MinSDK: 24, TargetSDK: 34, Verdict: CanRun}
	if fi, err := os.Stat(path); err == nil {
		a.Size = fi.Size()
	}
	return a, nil
}

// Launch reports Starting, then Running — or, for an app with native code
// run Restricted that needs more than its sample allows, Failed with the
// category of design/StartFailed.html.
func (m *Mock) Launch(spec LaunchSpec, report func(State)) (Process, error) {
	m.mu.Lock()
	found := m.java.Found || spec.JavaHome != ""
	m.mu.Unlock()
	if !found {
		return nil, fmt.Errorf("%w: no Java runtime", ErrNoRuntime)
	}
	apk, _ := m.Inspect(context.Background(), spec.Path)
	p := &mockProcess{m: m, report: report, stop: make(chan bool, 1)}
	mode := "pane"
	if spec.Window {
		mode = "window"
	}
	native := "off"
	if spec.Native == NativeRestricted {
		native = string(spec.Profile)
		if native == "" {
			native = "default"
		}
	}
	p.log("info  launch       package %s %s, mode %s, native %s", apk.Package, apk.Version, mode, native)
	go p.run(apk, spec)
	return p, nil
}

type mockProcess struct {
	m      *Mock
	report func(State)
	stop   chan bool // true: Kill
	mu     sync.Mutex
	st     State
}

func (p *mockProcess) log(format string, args ...any) {
	p.mu.Lock()
	p.st.Log = append(p.st.Log, time.Now().Format("15:04:05.000")+"  "+fmt.Sprintf(format, args...))
	p.mu.Unlock()
}

func (p *mockProcess) set(ph Phase, category string) {
	p.mu.Lock()
	p.st.Phase, p.st.Category = ph, category
	st := p.st
	st.Log = append([]string(nil), p.st.Log...)
	p.mu.Unlock()
	p.report(st)
}

func (p *mockProcess) run(apk APK, spec LaunchSpec) {
	p.set(Starting, "")
	p.log("info  dex.convert  cache hit, 2 dex files")
	select {
	case kill := <-p.stop:
		p.end(kill)
		return
	case <-time.After(3 * p.m.step):
	}
	switch {
	case apk.Verdict == Unsupported:
		p.log("error launch       APK_UNSUPPORTED only armeabi-v7a native code")
		p.set(Failed, "APK_UNSUPPORTED")
		return
	case apk.Verdict == NotYet:
		p.log("error gfx.select   GRAPHICS_CAPABILITY_MISSING guest Vulkan is not bridged")
		p.set(Failed, "GRAPHICS_CAPABILITY_MISSING")
		return
	case spec.Native == NativeRestricted && apk.Profile == N1 && len(apk.NativeLibs) > 0:
		lib := filepath.Base(apk.NativeLibs[0])
		p.log("info  jni.load     System.loadLibrary(%q)", strings.TrimSuffix(strings.TrimPrefix(lib, "lib"), ".so"))
		p.log("error elf.link     GUEST_SYMBOL_UNRESOLVED android_set_abort_message (%s)", lib)
		p.set(Failed, "GUEST_SYMBOL_UNRESOLVED")
		return
	}
	p.log("info  jvm.start    OpenJDK 21.0.5 arm64, heap 512 MB")
	p.mu.Lock()
	switch {
	case spec.Native == NativeRestricted && apk.Vulkan:
		p.st.Graphics = "vulkan"
	case apk.GLES && spec.Native == NativeRestricted:
		p.st.Graphics = "gles"
	}
	p.mu.Unlock()
	p.set(Running, "")
	kill := <-p.stop
	p.end(kill)
}

func (p *mockProcess) end(kill bool) {
	if kill {
		p.log("info  shutdown     killed")
	} else {
		p.log("info  shutdown     stopped by request")
	}
	p.set(Stopped, "")
}

// Stop asks the sample app to close; it does after one step.
func (p *mockProcess) Stop() error {
	go func() {
		time.Sleep(p.m.step)
		select {
		case p.stop <- false:
		default:
		}
	}()
	return nil
}

// Kill ends the sample app at once.
func (p *mockProcess) Kill() error {
	select {
	case p.stop <- true:
	default:
	}
	return nil
}

// Back is logged: the sample app has no screens to go back from.
func (p *mockProcess) Back() error {
	p.log("info  input        back")
	return nil
}

// Resize is logged.
func (p *mockProcess) Resize(w, h int) error {
	p.log("info  surface      resize %d×%d", w, h)
	return nil
}

// Reattach succeeds: the sample app draws nothing to move.
func (p *mockProcess) Reattach(bool) bool { return true }

// InstallJava steps through the download and installs nothing; afterwards
// Doctor reports Windows-ARM64.apk's own runtime.
func (m *Mock) InstallJava(ctx context.Context, dir string, progress func(InstallStep, float64)) error {
	for i := 0; i <= 10; i++ {
		progress(StepDownload, float64(i)/10)
		if err := m.wait(ctx, 1); err != nil {
			return err
		}
	}
	for _, s := range []InstallStep{StepCheck, StepInstall, StepTest} {
		progress(s, 0)
		if err := m.wait(ctx, 2); err != nil {
			return err
		}
	}
	m.mu.Lock()
	m.java = mockJava(true)
	m.mu.Unlock()
	progress(StepDone, 1)
	return nil
}
