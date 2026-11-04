// Package gui is Windows-ARM64.apk's interface (design/, doc/en-us/gui.md):
// the main window with its sidebar, Home and the apps' panes, apps in
// windows of their own, Settings and Diagnostics. It keeps the library
// and its own settings; everything about running an app is the runtime's,
// reached through package winapk.
//
// Every field of App is read and written on the main thread only: views
// run there, and work done elsewhere hands its result back through later.
package gui

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"slices"
	"time"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// App is the interface's state.
type App struct {
	rt  winapk.Client
	dir string // holds gui.json and library.json; "" keeps nothing (tests)
	// demo keeps the library in memory: it holds sample apps.
	demo   bool
	ctx    context.Context
	cancel context.CancelFunc
	// later runs a function on the main thread and redraws; async runs
	// work off it. Tests run both inline.
	later func(func())
	async func(func())

	prefs   Prefs
	library []*entry
	open    map[string]*session // by entry ID

	report   winapk.Report
	checked  time.Time
	checking bool
	install  installState

	window   *mygo.Window
	groups   []*group
	cur      *group
	sizes    map[string]size // each pane's room in the last frame
	quitting bool

	homeQuery  string
	homeFilter int // 0 all, 1 open
	diag       diagState
	settings   settingsState
	add        addState
	details    detailsState
	confirm    confirmState
	toast      string
	toastAt    time.Time
}

// Prefs are the interface's own settings (gui.json).
type Prefs struct {
	Theme    string `json:"theme"`    // "system", "light" or "dark"
	Language string `json:"language"` // "" follows Windows
	// OpenInWindow: new apps open in a window of their own.
	OpenInWindow bool `json:"openInWindow"`
	// CloseStopsAll: closing the main window stops apps in windows of
	// their own too.
	CloseStopsAll bool `json:"closeStopsAll"`
	// AskOnClose: list the apps that would stop before the window closes.
	AskOnClose  bool   `json:"askOnClose"`
	AppData     string `json:"appData"`     // "" is the default folder
	JavaHome    string `json:"javaHome"`    // "" is Windows-ARM64.apk's own
	JavaUpdates bool   `json:"javaUpdates"` // keep the own Java runtime up to date
	Graphics    string `json:"graphics"`    // "auto", "vulkan" or "d3d11"
	// Profile is the default native profile of apps whose native code runs
	// Restricted.
	Profile       winapk.Profile `json:"profile"`
	SidebarHidden bool           `json:"sidebarHidden"`
}

func defaultPrefs() Prefs {
	return Prefs{Theme: "system", AskOnClose: true, JavaUpdates: true, Graphics: "auto", Profile: winapk.N1}
}

// entry is an app of the library (library.json).
type entry struct {
	ID  string     `json:"id"` // the package name
	APK winapk.APK `json:"apk"`
	// InWindow: Open opens it in a window of its own, else as a pane.
	InWindow bool           `json:"inWindow"`
	Native   winapk.Native  `json:"native"`
	Profile  winapk.Profile `json:"profile"` // "" uses Prefs.Profile
	Graphics string         `json:"graphics"`
	Screen   int            `json:"screen"` // 0 phone, 1 small tablet, 2 fit the pane
	Added    time.Time      `json:"added"`
	// LastFailed is the category of the last start that failed, "" if none.
	LastFailed string `json:"lastFailed"`

	icon ui.ImageSource
}

// runnable tells whether Open is offered: the verdict lets it run here.
func (e *entry) runnable() bool {
	return e.APK.Verdict == winapk.CanRun || e.APK.Verdict == winapk.NeedsNative
}

// profile is the native profile the app runs with under Restricted.
func (a *App) profile(e *entry) winapk.Profile {
	if e.Profile != "" {
		return e.Profile
	}
	return a.prefs.Profile
}

// session is an open app: its runtime process and where it shows.
type session struct {
	id    string
	proc  winapk.Process
	state winapk.State
	// win is its window of its own, nil while it is a pane.
	win     *mygo.Window
	rotated bool
	// stopAt is when Stop was asked, to offer Force stop when it gets no
	// answer.
	stopAt time.Time
}

// live tells whether the app's process runs.
func (s *session) live() bool {
	return s.proc != nil && s.state.Phase != winapk.Stopped && s.state.Phase != winapk.Failed
}

// New makes the interface for a runtime client; dir holds its files.
func New(rt winapk.Client, dir string, demo bool) *App {
	ctx, cancel := context.WithCancel(context.Background())
	a := &App{rt: rt, dir: dir, demo: demo, ctx: ctx, cancel: cancel, open: map[string]*session{}, sizes: map[string]size{}}
	a.later = a.onMain
	a.async = func(fn func()) { go fn() }
	a.prefs = loadJSON(dir, "gui.json", defaultPrefs())
	if !demo {
		a.library = loadJSON[[]*entry](dir, "library.json", nil)
	}
	for _, e := range a.library {
		a.loadIcon(e)
	}
	home := &group{}
	home.single(tabHome)
	a.groups, a.cur = []*group{home}, home
	return a
}

// AddSamples puts the runtime's sample apps in the library (demo).
func (a *App) AddSamples(apks []winapk.APK) {
	for _, apk := range apks {
		e := a.newEntry(apk)
		switch apk.Package {
		case "com.example.hellojava":
			e.InWindow = true
		case "com.example.gltriangle":
			e.Native, e.Profile = winapk.NativeRestricted, winapk.N2
		case "com.example.tilepuzzle":
			e.Native = winapk.NativeRestricted
		}
		a.library = append(a.library, e)
	}
}

// newEntry is an APK as a new entry of the library, opening as Settings'
// General says.
func (a *App) newEntry(apk winapk.APK) *entry {
	e := &entry{ID: apk.Package, APK: apk, InWindow: a.prefs.OpenInWindow, Native: winapk.NativeOff, Graphics: "", Added: time.Now()}
	a.loadIcon(e)
	return e
}

// loadIcon decodes the icon the APK carries.
func (a *App) loadIcon(e *entry) {
	if len(e.APK.Icon) > 0 {
		if img, err := ui.DecodeImage(e.APK.Icon); err == nil {
			e.icon = img
		}
	}
}

// loadJSON reads name in dir over def; a missing or broken file leaves def.
func loadJSON[T any](dir, name string, def T) T {
	if dir == "" {
		return def
	}
	if b, err := os.ReadFile(filepath.Join(dir, name)); err == nil {
		_ = json.Unmarshal(b, &def)
	}
	return def
}

// saveJSON writes v as name in dir atomically: a crash leaves the old file.
func saveJSON(dir, name string, v any) {
	if dir == "" {
		return
	}
	b, _ := json.MarshalIndent(v, "", "  ")
	tmp := filepath.Join(dir, name+".tmp")
	if os.MkdirAll(dir, 0o700) == nil && os.WriteFile(tmp, b, 0o600) == nil {
		_ = os.Rename(tmp, filepath.Join(dir, name))
	}
}

// setPrefs keeps p and writes gui.json.
func (a *App) setPrefs(p Prefs) {
	a.prefs = p
	saveJSON(a.dir, "gui.json", p)
}

// saveLibrary writes library.json; the demo's sample library stays in
// memory.
func (a *App) saveLibrary() {
	if !a.demo {
		saveJSON(a.dir, "library.json", a.library)
	}
}

// Prefs returns the interface's settings (main thread).
func (a *App) Prefs() Prefs { return a.prefs }

// entry finds an app of the library by ID.
func (a *App) entry(id string) *entry {
	for _, e := range a.library {
		if e.ID == id {
			return e
		}
	}
	return nil
}

// onMain runs fn on the main thread, redrawing every window after it.
func (a *App) onMain(fn func()) {
	mygo.RunOnMain(func() {
		fn()
		if a.window != nil {
			a.window.Invalidate()
		}
		for _, s := range a.open {
			if s.win != nil {
				s.win.Invalidate()
			}
		}
	})
}

// Start checks this PC.
func (a *App) Start() { a.check() }

// Stop ends the background work.
func (a *App) Stop() { a.cancel() }

// check runs the runtime's doctor and shows its report.
func (a *App) check() {
	if a.checking {
		return
	}
	a.checking = true
	home := a.prefs.JavaHome
	a.async(func() {
		r, err := a.rt.Doctor(a.ctx, home)
		a.later(func() {
			a.checking = false
			if err != nil {
				a.report = winapk.Report{}
				a.setToast(L.ToastNoRuntime())
				return
			}
			a.report, a.checked = r, time.Now()
		})
	})
}

// setToast shows a short message at the foot of the window for a few
// seconds.
func (a *App) setToast(msg string) {
	a.toast, a.toastAt = msg, time.Now()
	at := a.toastAt
	a.async(func() {
		time.Sleep(4 * time.Second)
		a.later(func() {
			if a.toastAt == at {
				a.toast = ""
			}
		})
	})
}

// launch opens an app: in a window of its own when inWindow, else as a
// pane — beside the pane in focus when beside. An app already open comes
// forward where it is.
func (a *App) launch(e *entry, inWindow, beside bool) {
	if s := a.open[e.ID]; s != nil && s.live() {
		a.reveal(e.ID)
		return
	}
	if !e.runnable() || !a.report.Ready() {
		a.openDetails(e.ID)
		return
	}
	s := &session{id: e.ID, state: winapk.State{Phase: winapk.Starting}}
	a.open[e.ID] = s
	spec := winapk.LaunchSpec{Path: e.APK.Path, JavaHome: a.prefs.JavaHome, Native: e.Native, Window: inWindow}
	if e.Native == winapk.NativeRestricted {
		spec.Profile = a.profile(e)
	}
	id := e.ID
	proc, err := a.rt.Launch(spec, func(st winapk.State) {
		a.later(func() { a.setState(id, s, st) })
	})
	if err != nil {
		s.state = winapk.State{Phase: winapk.Failed, Category: "DEPENDENCY_MISSING", Log: []string{err.Error()}}
	}
	s.proc = proc
	if inWindow {
		a.newAppWindow(s)
		return
	}
	a.showTab(appTab(id), beside)
}

// setState keeps a process's new state; a start that failed stays on its
// entry.
func (a *App) setState(id string, s *session, st winapk.State) {
	if a.open[id] != s {
		return // an older process of an app opened again
	}
	s.state = st
	if e := a.entry(id); e != nil {
		switch st.Phase {
		case winapk.Failed:
			e.LastFailed = st.Category
			a.saveLibrary()
		case winapk.Running:
			if e.LastFailed != "" {
				e.LastFailed = ""
				a.saveLibrary()
			}
		}
	}
}

// stopApp asks an app to close; force ends its process at once.
func (a *App) stopApp(id string, force bool) {
	s := a.open[id]
	if s == nil || !s.live() {
		return
	}
	proc := s.proc
	if !force {
		s.stopAt = time.Now()
	}
	a.async(func() {
		if force {
			_ = proc.Kill()
		} else {
			_ = proc.Stop()
		}
	})
}

// closeApp stops an app and takes its pane or window away.
func (a *App) closeApp(id string) {
	s := a.open[id]
	a.stopApp(id, false)
	a.removeTab(appTab(id))
	delete(a.open, id)
	if s != nil && s.win != nil {
		w := s.win
		s.win = nil
		w.Close()
	}
}

// reveal brings an open app forward: its window, or its pane's group.
func (a *App) reveal(id string) {
	s := a.open[id]
	if s == nil {
		return
	}
	if s.win != nil {
		s.win.Show()
		s.win.Focus()
		return
	}
	if g := a.groupOf(appTab(id)); g != nil {
		a.cur, g.focus = g, appTab(id)
		return
	}
	a.showTab(appTab(id), false)
}

// toWindow moves an app's pane into a window of its own, keeping it
// running where the runtime can re-attach its surface, else restarting it.
func (a *App) toWindow(id string) {
	s, e := a.open[id], a.entry(id)
	if s == nil || e == nil {
		return
	}
	a.removeTab(appTab(id))
	if s.live() && s.proc.Reattach(true) {
		a.newAppWindow(s)
		return
	}
	a.stopApp(id, false)
	delete(a.open, id)
	a.launch(e, true, false)
}

// toPane moves an app's window back into the main window as a pane of its
// own.
func (a *App) toPane(id string) {
	s, e := a.open[id], a.entry(id)
	if s == nil || e == nil {
		return
	}
	w := s.win
	s.win = nil
	if s.live() && s.proc.Reattach(false) {
		a.showTab(appTab(id), false)
	} else {
		a.stopApp(id, false)
		delete(a.open, id)
		a.launch(e, false, false)
	}
	if w != nil {
		w.Close()
	}
	a.Focus()
}

// removeEntry takes an app out of the library, closing it first.
func (a *App) removeEntry(id string) {
	a.closeApp(id)
	a.library = slices.DeleteFunc(a.library, func(e *entry) bool { return e.ID == id })
	a.saveLibrary()
}

// openPath opens a folder in Explorer.
func (a *App) openPath(path string) {
	a.async(func() {
		if err := mygo.Shell.OpenPath(path); err != nil {
			a.later(func() { a.setToast(L.ToastFailed()) })
		}
	})
}

// ApplyTheme sets the appearance: "system", "light" or "dark".
func ApplyTheme(theme string) {
	switch theme {
	case "light":
		mygo.Theme.SetSource(mygo.ThemeLight)
	case "dark":
		mygo.Theme.SetSource(mygo.ThemeDark)
	default:
		mygo.Theme.SetSource(mygo.ThemeSystem)
	}
}

// appDataDir is where apps keep their data.
func (a *App) appDataDir() string {
	if a.prefs.AppData != "" {
		return a.prefs.AppData
	}
	return filepath.Join(os.Getenv("LOCALAPPDATA"), "Windows-ARM64.apk", "Apps")
}
