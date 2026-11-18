// Package winapk defines and implements the runtime ↔ front-end contract
// (AGENTS.md, "Division of work"; doc/en-us/gui.md): running `winapk doctor`
// and `winapk inspect`, launching one runtime process per APK over `--embed`
// shared-memory frames and a duplex control pipe, installing the private ARM64
// OpenJDK 21 runtime, and managing app data and converted-code storage.
//
// Mock remains available for unit tests and explicit sample previews
// (`WINAPK_DEMO=1`).
package winapk

import (
	"context"
	"errors"
	"image"
)

// Client is the runtime as the front end reaches it. Methods may block on
// I/O and are called off the UI thread.
type Client interface {
	// Doctor checks this PC (`winapk doctor --json`, PLAN.md §7, §18);
	// javaHome is the folder the person chose, "" to look for one.
	Doctor(ctx context.Context, javaHome string) (Report, error)
	// Inspect reads an APK without running anything in it
	// (`winapk inspect --json`).
	Inspect(ctx context.Context, path string) (APK, error)
	// Launch starts an APK in a runtime process of its own; changes of its
	// state go to report until the process ends.
	Launch(spec LaunchSpec, report func(State)) (Process, error)
	// InstallJava downloads, checks, installs and tests Windows-ARM64.apk's
	// own Java runtime in dir, calling progress as it goes.
	InstallJava(ctx context.Context, dir string, progress func(InstallStep, float64)) error
}

// Report is what `doctor` found on this PC.
type Report struct {
	// Demo marks sample data: no runtime answered, or demo mode was requested.
	Demo bool `json:"demo"`
	// Version is the runtime's version; the front end's must match it.
	Version string  `json:"version"`
	Java    Java    `json:"java"`
	Machine Machine `json:"machine"`
	// Features are what this PC can run, in the order Diagnostics shows them.
	Features []Feature `json:"features"`
	// Limitations are stable ID + reason pairs reported by `doctor`.
	Limitations []Limitation `json:"limitations,omitempty"`
	// RawJSON is the verbatim `winapk doctor --json` document when available.
	RawJSON []byte `json:"-"`
}

// Ready tells whether apps can run: an ARM64 Java runtime was found and started.
func (r Report) Ready() bool { return r.Java.Found && r.Java.Native }

// Limitation is one capability boundary reported by `doctor`.
type Limitation struct {
	ID     string `json:"id"`
	Reason string `json:"reason"`
}

// Java is the Java runtime the runtime uses.
type Java struct {
	Found bool `json:"found"`
	// Own: Windows-ARM64.apk's own, installed in its folder.
	Own     bool   `json:"own"`
	Version string `json:"version"`
	Vendor  string `json:"vendor"`
	Path    string `json:"path"`
	// Native: an ARM64 build, not an emulated one (AGENTS.md, "Native
	// means native").
	Native bool `json:"native"`
	// Started is doctor's JVM start outcome ("ok", "not-run: ...", "failed: ...").
	Started string `json:"started,omitempty"`
	// InstalledAt is the date Windows-ARM64.apk installed its private runtime, if recorded.
	InstalledAt string `json:"installedAt,omitempty"`
}

// Machine is this PC as `doctor` describes it.
type Machine struct {
	Windows          string `json:"windows"`
	OSBuild          string `json:"osBuild,omitempty"`
	OSArch           string `json:"osArch,omitempty"`
	ProcessArch      string `json:"processArch,omitempty"`
	CPU              string `json:"cpu"`
	MemoryMB         uint64 `json:"memoryMB,omitempty"`
	GPU              string `json:"gpu"`
	VendorID         string `json:"vendorId,omitempty"`
	DeviceID         string `json:"deviceId,omitempty"`
	Driver           string `json:"driver"`
	Vulkan           string `json:"vulkan"`
	VulkanLoader     string `json:"vulkanLoader,omitempty"`
	VulkanInstance   string `json:"vulkanInstance,omitempty"`
	VulkanDevice     string `json:"vulkanDevice,omitempty"`
	VulkanDriverName string `json:"vulkanDriverName,omitempty"`
	VulkanTranslated bool   `json:"vulkanTranslated,omitempty"`
	VulkanQueues     int    `json:"vulkanQueues,omitempty"`
	Win32Surface     bool   `json:"win32Surface,omitempty"`
	Win32Present     string `json:"win32Present,omitempty"`
	AngleArm64       bool   `json:"angleArm64,omitempty"`
	FrameworkCommit  string `json:"frameworkCommit,omitempty"`
}

// Level is how far a feature works on this PC.
type Level string

const (
	Works        Level = "works"
	Experimental Level = "experimental"
	Unavailable  Level = "unavailable"
)

// Feature is one thing an app may need, and how far it works here.
type Feature struct {
	ID    string `json:"id"` // "java", "native-n0", "native-n1", "native-n2", "gles", "vulkan"
	Level Level  `json:"level"`
	// Detail is doctor's own words, shown only in Diagnostics.
	Detail string `json:"detail"`
}

// Verdict is what this PC can do with an APK.
type Verdict string

const (
	// CanRun: Java only, or native code within a profile that works here.
	CanRun Verdict = "can-run"
	// NeedsNative: native code the app needs, off until turned on.
	NeedsNative Verdict = "needs-native"
	// NotYet: needs a feature not available here yet (Vulkan for apps).
	NotYet Verdict = "not-yet"
	// Unsupported: never runs here (only 32-bit native code).
	Unsupported Verdict = "unsupported"
)

// APK is what `inspect` read in an APK.
type APK struct {
	Path      string `json:"path"`
	Package   string `json:"package"`
	Label     string `json:"label"`
	Version   string `json:"version"`
	Size      int64  `json:"size"`
	MinSDK    int    `json:"minSdk"`
	TargetSDK int    `json:"targetSdk"`
	// ABIs are the native code's ABIs, empty for a Java-only APK.
	ABIs        []string `json:"abis"`
	NativeLibs  []string `json:"nativeLibs"`
	Permissions []string `json:"permissions,omitempty"`
	GLES        bool     `json:"gles"`
	GLESVersion string   `json:"glesVersion,omitempty"`
	Vulkan      bool     `json:"vulkan"`
	// Profile is the least native profile its code needs, "" for none.
	Profile Profile `json:"profile"`
	Verdict Verdict `json:"verdict"`
	// Icon is the launcher icon as PNG, nil when it has none.
	Icon   []byte `json:"icon"`
	SHA256 string `json:"sha256,omitempty"`
}

// Profile is a native profile (PLAN.md §1, N0–N2).
type Profile string

const (
	N0 Profile = "N0"
	N1 Profile = "N1"
	N2 Profile = "N2"
)

// Native is how an app may run its native code.
type Native string

const (
	NativeOff        Native = "off"
	NativeRestricted Native = "restricted"
)

// LaunchSpec is how an APK starts.
type LaunchSpec struct {
	Path string
	// JavaHome is the folder the person chose, "" for the one Doctor found.
	JavaHome string
	// DataDir is the App data root, "" for DefaultDataDir().
	DataDir string
	Native  Native
	// Profile is the native profile under Restricted, "" for the default.
	Profile Profile
	// Graphics is "auto", "vulkan", "d3d11" or "off" ("" defaults to "auto").
	Graphics string
	// Width and Height are the initial screen size in DIPs (0 uses 360×720).
	Width, Height int
	// Window: the app opens in a window of its own; else in a pane.
	Window bool
}

// Phase is where a runtime process is in its life.
type Phase string

const (
	Starting      Phase = "starting"
	Running       Phase = "running"
	NotResponding Phase = "not-responding"
	Stopped       Phase = "stopped"
	Failed        Phase = "failed"
)

// LogEntry is one structured log record for Diagnostics › App logs.
type LogEntry struct {
	Time    string `json:"time"`
	PIDTID  string `json:"pidTid"`
	Level   string `json:"level"`
	Stage   string `json:"stage"`
	Message string `json:"message"`
}

// FailureInfo summarizes why a run failed for Diagnostics › App logs.
type FailureInfo struct {
	Stage  string `json:"stage,omitempty"`
	Module string `json:"module,omitempty"`
	Symbol string `json:"symbol,omitempty"`
	Where  string `json:"where,omitempty"`
	Detail string `json:"detail,omitempty"`
}

// State is a runtime process's state as it reports it.
type State struct {
	Phase Phase `json:"phase"`
	// Category is the stable error category of a failure (PLAN.md §14.2),
	// shown only in Diagnostics.
	Category string `json:"category"`
	// Graphics is the backend the app draws with ("", "java2d", "gles", "gles-vulkan", "gles-d3d11", "vulkan").
	Graphics string `json:"graphics"`
	PID      int    `json:"pid,omitempty"`
	ExitCode int    `json:"exitCode,omitempty"`
	LogPath  string `json:"logPath,omitempty"`
	// Log is the process's formatted log lines so far.
	Log     []string    `json:"log"`
	Entries []LogEntry  `json:"entries,omitempty"`
	Failure FailureInfo `json:"failure,omitempty"`
}

// Pointer actions matching Android MotionEvent constants.
const (
	PointerDown   = 0
	PointerUp     = 1
	PointerMove   = 2
	PointerCancel = 3
)

// Process is a running APK's runtime process.
type Process interface {
	// Stop asks the app to close.
	Stop() error
	// Kill ends the process at once.
	Kill() error
	// Back sends the app Android's Back.
	Back() error
	// Resize tells the app its screen's size in DIPs, as rotating or
	// choosing a screen size changes it.
	Resize(w, h int) error
	// Reattach moves the app's surface between a pane and a window of its
	// own without restarting, and reports false where the runtime can't.
	Reattach(window bool) bool
	// SetFrameHandler registers a callback invoked off the UI thread with
	// each newly presented RGBA8 frame. The caller owns the *image.RGBA until
	// the next frame arrives.
	SetFrameHandler(fn func(*image.RGBA))
	// SendPointer forwards a pointer press, drag, release or cancel in DIPs.
	SendPointer(action int, x, y float32)
	// SendWheel forwards mouse-wheel scrolling in notches at (x, y) DIPs.
	SendWheel(x, y, dx, dy float32)
	// SendKey forwards a Windows virtual-key press or release.
	SendKey(down bool, vk, unicode, meta int)
	// SendText forwards committed UTF-8 text (such as IME input).
	SendText(text string)
}

// InstallStep is a step of getting the Java runtime.
type InstallStep int

const (
	StepDownload InstallStep = iota
	StepCheck
	StepInstall
	StepTest
	StepDone
)

// ErrNoRuntime: no runtime answered; the interface says it can't run apps.
var ErrNoRuntime = errors.New("winapk: no runtime")
