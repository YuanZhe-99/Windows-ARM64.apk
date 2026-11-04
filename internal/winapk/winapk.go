// Package winapk is what the front end expects of the runtime: the
// runtime ↔ front-end contract (AGENTS.md, "Division of work") as the
// interface needs it, written from its side before the runtime exists.
// The runtime agent turns it into the `winapk` CLI's JSON outputs and the
// per-APK runtime process; where the runtime has to differ, this package
// and the views change in the same change set.
//
// Until then the interface runs against Mock, which shows sample data and
// says so (Report.Demo).
package winapk

import (
	"context"
	"errors"
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
	// Demo marks sample data: no runtime answered.
	Demo bool `json:"demo"`
	// Version is the runtime's version; the front end's must match it.
	Version string  `json:"version"`
	Java    Java    `json:"java"`
	Machine Machine `json:"machine"`
	// Features are what this PC can run, in the order Diagnostics shows them.
	Features []Feature `json:"features"`
}

// Ready tells whether apps can run: a Java runtime was found and works.
func (r Report) Ready() bool { return r.Java.Found }

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
}

// Machine is this PC as `doctor` describes it.
type Machine struct {
	Windows string `json:"windows"`
	CPU     string `json:"cpu"`
	GPU     string `json:"gpu"`
	Driver  string `json:"driver"`
	Vulkan  string `json:"vulkan"`
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
	ABIs       []string `json:"abis"`
	NativeLibs []string `json:"nativeLibs"`
	GLES       bool     `json:"gles"`
	Vulkan     bool     `json:"vulkan"`
	// Profile is the least native profile its code needs, "" for none.
	Profile Profile `json:"profile"`
	Verdict Verdict `json:"verdict"`
	// Icon is the launcher icon as PNG, nil when it has none.
	Icon []byte `json:"icon"`
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
	Native   Native
	// Profile is the native profile under Restricted, "" for the default.
	Profile Profile
	// Window: the app opens in a window of its own; else in a pane, whose
	// surface the front end hands over.
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

// State is a runtime process's state as it reports it.
type State struct {
	Phase Phase `json:"phase"`
	// Category is the stable error category of a failure (PLAN.md §14.2),
	// shown only in Diagnostics.
	Category string `json:"category"`
	// Graphics is the backend the app draws with ("", "gles", "vulkan").
	Graphics string `json:"graphics"`
	// Log is the process's log so far.
	Log []string `json:"log"`
}

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
