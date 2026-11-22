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
	"image"
	"os"
	"path/filepath"
	"slices"
	"strings"
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
	// work off it; relaunch restarts the app after an update. Tests
	// override them.
	later    func(func())
	async    func(func())
	relaunch func()

	prefs   Prefs
	library []*entry
	open    map[string]*session // by entry ID
	// runs are each app's runs since the interface started, newest first,
	// for Diagnostics' logs.
	runs map[string][]*session

	// missing are the library's APK files not found at the last check.
	missing  map[string]bool
	staging  map[string]stageJob
	stageSeq map[string]uint64
	report   winapk.Report
	checked  time.Time
	checking bool
	install  installState

	window    *mygo.Window
	diagWin   *mygo.Window
	groups    []*group
	cur       *group
	sizes     map[string]size // each pane's room in the last frame
	panesRoom size            // panes stage container bounds in the last frame
	lastStage size            // last measured app stage bounds in the main window
	// dragging is the tab dragged over the panes, "" for none; pending is
	// what runs once the frame is built (afterFrame).
	dragging, zonesFor string
	pending            []func()
	quitting           bool
	sbClickAt          time.Time // last click time on sb-hide for single vs double click

	homeQuery  string
	homeFilter int // 0 all, 1 open
	diag       diagState
	settings   settingsState
	add        addState
	details    detailsState
	confirm    confirmState
	docView    docViewState
	toast      string
	toastAt    time.Time

	totalDataBytes int64
	codeBytes      int64
	appDataBytes   map[string]int64
	checkedUpdate  bool
	appUpdate      *mygo.Update
	appUpdateBusy  bool  // true while checking for or installing an app update
	appUpdateDone  int64 // bytes of the update downloaded so far, while installing
	appUpdateTotal int64 // total update archive bytes, while installing
}

// stageJob tracks one background CopyAPK operation for an entry ID.
type stageJob struct {
	seq  uint64
	done chan struct{}
}

// docViewState displays a local license or third-party notice file in a sheet.
type docViewState struct {
	open        bool
	title, body string
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
	Profile          winapk.Profile `json:"profile"`
	SidebarHidden    bool           `json:"sidebarHidden"`
	SidebarCompact   bool           `json:"sidebarCompact,omitempty"`
	HideNativeNotice bool           `json:"hideNativeNotice"`
}

func defaultPrefs() Prefs {
	return Prefs{Theme: "system", AskOnClose: true, JavaUpdates: true, Graphics: "auto", Profile: winapk.N2}
}

func profileRank(p winapk.Profile) int {
	switch p {
	case winapk.N0:
		return 1
	case winapk.N1:
		return 2
	case winapk.N2:
		return 3
	default:
		return 0
	}
}

func maxProfile(a, b winapk.Profile) winapk.Profile {
	if profileRank(b) > profileRank(a) {
		return b
	}
	if a == "" {
		return winapk.N2
	}
	return a
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
	return maxProfile(a.prefs.Profile, e.APK.Profile)
}

// session is an open app: its runtime process and where it shows.
type session struct {
	id    string
	proc  winapk.Process
	state winapk.State
	frame *ui.Bitmap
	// win is its window of its own, nil while it is a pane.
	win        *mygo.Window
	rotated    bool
	pressing   bool
	hideNotice bool
	native     winapk.Native
	profile    winapk.Profile
	reqGfx     string
	// stopAt is when Stop was asked, to offer Force stop when it gets no
	// answer.
	stopAt  time.Time
	started time.Time
	// told is the screen size confirmed sent while Running; lastSize is the
	// latest quantized DIP size requested for pointer mapping before Running.
	told, lastSize size
}

// updateFrame uploads a newly published RGBA8 frame into the session's GPU bitmap.
func (s *session) updateFrame(rgba *image.RGBA) {
	if rgba == nil {
		return
	}
	if s.frame == nil {
		s.frame = ui.NewBitmap(rgba)
	} else {
		s.frame.Update(rgba)
	}
}

// live tells whether the app's process runs.
// stopWait is how long Stop waits for an answer before the app shows as
// not responding, with Force stop.
const stopWait = 5 * time.Second

// maxRuns is how many runs of an app Diagnostics keeps.
const maxRuns = 10

func (s *session) live() bool {
	return s.proc != nil && s.state.Phase != winapk.Stopped && s.state.Phase != winapk.Failed
}

// New makes the interface for a runtime client; dir holds its files.
func New(rt winapk.Client, dir string, demo bool) *App {
	ctx, cancel := context.WithCancel(context.Background())
	a := &App{
		rt: rt, dir: dir, demo: demo, ctx: ctx, cancel: cancel,
		open: map[string]*session{}, runs: map[string][]*session{},
		staging: map[string]stageJob{}, stageSeq: map[string]uint64{},
		sizes: map[string]size{}, appDataBytes: map[string]int64{},
	}
	a.later = a.onMain
	a.async = func(fn func()) { go fn() }
	a.relaunch = func() { mygo.App.Relaunch() }
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
// General says. Apps whose static inspection requires native code default to
// Restricted Profile N2 so Flutter/GLES/media engines launch out of the box.
func (a *App) newEntry(apk winapk.APK) *entry {
	nat := winapk.NativeOff
	var prof winapk.Profile
	if apk.Verdict == winapk.NeedsNative && len(apk.NativeLibs) > 0 {
		nat = winapk.NativeRestricted
		prof = maxProfile(winapk.N2, apk.Profile)
	}
	e := &entry{ID: apk.Package, APK: apk, InWindow: a.prefs.OpenInWindow, Native: nat, Profile: prof, Graphics: "", Added: time.Now()}
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
		if a.diagWin != nil {
			a.diagWin.Invalidate()
		}
		for _, s := range a.open {
			if s.win != nil {
				s.win.Invalidate()
			}
		}
	})
}

// refreshStorage updates cached disk usage figures off the UI thread.
func (a *App) refreshStorage() {
	if a.demo {
		a.totalDataBytes = 46_000_000
		a.codeBytes = 182_000_000
		for _, e := range a.library {
			a.appDataBytes[e.ID] = 2_100_000
		}
		return
	}
	root := a.appDataDir()
	var pkgs []string
	for _, e := range a.library {
		pkgs = append(pkgs, e.APK.Package)
	}
	a.async(func() {
		tot := winapk.TotalAppDataSize(root)
		code := winapk.ConvertedCodeSize(root)
		per := make(map[string]int64, len(pkgs))
		for _, p := range pkgs {
			per[p] = winapk.AppDataSize(root, p)
		}
		a.later(func() {
			a.totalDataBytes, a.codeBytes = tot, code
			for k, v := range per {
				a.appDataBytes[k] = v
			}
		})
	})
}

// Start checks this PC.
func (a *App) Start() { a.check() }

// Stop ends the background work.
func (a *App) Stop() { a.cancel() }

// check runs the runtime's doctor, migrates readable external APKs into AppData
// storage, and refreshes missing-file state.
func (a *App) check() {
	if a.checking {
		return
	}
	a.checking = true
	home := a.prefs.JavaHome
	demo := a.demo
	dataDir := a.appDataDir()
	type checkItem struct {
		id, pkg, path string
		staging       bool
	}
	items := make([]checkItem, 0, len(a.library))
	for _, e := range a.library {
		_, st := a.staging[e.ID]
		items = append(items, checkItem{id: e.ID, pkg: e.APK.Package, path: e.APK.Path, staging: st})
	}
	a.async(func() {
		r, err := a.rt.Doctor(a.ctx, home)
		migrated := map[string]string{}
		paths := make([]string, 0, len(items))
		for _, it := range items {
			if it.staging {
				continue
			}
			p := it.path
			if !demo && it.pkg != "" && p != "" && !strings.HasPrefix(p, "sample:") {
				if managed, merr := winapk.ManagedAPKPath(dataDir, it.pkg); merr == nil && !samePath(p, managed) {
					if _, serr := os.Stat(p); serr == nil {
						if dst, cerr := winapk.CopyAPK(dataDir, it.pkg, p); cerr == nil {
							migrated[it.id] = dst
							p = dst
						}
					} else if _, merr2 := os.Stat(managed); merr2 == nil {
						migrated[it.id] = managed
						p = managed
					}
				}
			}
			paths = append(paths, p)
		}
		gone := missingFiles(paths)
		a.later(func() {
			a.checking = false
			changed := false
			for _, it := range items {
				dst, ok := migrated[it.id]
				if !ok {
					continue
				}
				if e := a.entry(it.id); e != nil && e.APK.Path == it.path {
					if _, st := a.staging[it.id]; !st {
						e.APK.Path = dst
						changed = true
					}
				}
			}
			if changed {
				a.saveLibrary()
			}
			a.missing = gone
			if err != nil {
				a.report = winapk.Report{}
				a.setToast(L.ToastNoRuntime())
				return
			}
			a.report, a.checked = r, time.Now()
			a.refreshStorage()
			if !a.demo && a.prefs.JavaUpdates && r.Java.Own && len(a.open) == 0 && !a.checkedUpdate {
				a.checkedUpdate = true
				if rt, ok := a.rt.(*winapk.Runtime); ok {
					a.async(func() {
						if upd, _, err := rt.CheckJavaUpdate(a.ctx, r.Java.Path); err == nil && upd {
							a.later(func() {
								if len(a.open) == 0 && !a.install.running {
									a.installJava()
								}
							})
						}
					})
				}
			}
		})
	})
}

func samePath(a, b string) bool {
	if a == "" || b == "" {
		return false
	}
	return strings.EqualFold(filepath.Clean(a), filepath.Clean(b))
}

// stageAPK copies e's APK into `<AppData>\apks\<package>.apk` off the UI thread
// so deleting or moving the original file does not break future launches.
func (a *App) stageAPK(e *entry) chan struct{} {
	if a.demo || e == nil || e.APK.Package == "" || e.APK.Path == "" || strings.HasPrefix(e.APK.Path, "sample:") {
		return nil
	}
	dataDir := a.appDataDir()
	managed, err := winapk.ManagedAPKPath(dataDir, e.APK.Package)
	if err != nil {
		return nil
	}
	srcPath := e.APK.Path
	if cur, ok := a.staging[e.ID]; !ok || cur.done == nil {
		if samePath(srcPath, managed) {
			return nil
		}
	}
	id := e.ID
	pkg := e.APK.Package
	prev := a.staging[id].done
	a.stageSeq[id]++
	seq := a.stageSeq[id]
	ch := make(chan struct{})
	a.staging[id] = stageJob{seq: seq, done: ch}
	a.async(func() {
		defer close(ch)
		if prev != nil {
			<-prev
		}
		dst, err := winapk.CopyAPK(dataDir, pkg, srcPath)
		a.later(func() {
			if cur, ok := a.staging[id]; ok && cur.seq == seq {
				delete(a.staging, id)
			}
			en := a.entry(id)
			if en == nil {
				if a.stageSeq[id] == seq {
					_ = winapk.RemoveManagedAPK(dataDir, pkg)
				}
				return
			}
			if err != nil {
				return
			}
			if a.stageSeq[id] == seq && en.APK.Path == srcPath {
				en.APK.Path = dst
				delete(a.missing, srcPath)
				a.saveLibrary()
				a.refreshStorage()
			}
		})
	})
	return ch
}

// missingFiles are the APK files of paths no longer where they were; the
// demo's sample apps have none.
func missingFiles(paths []string) map[string]bool {
	out := map[string]bool{}
	for _, p := range paths {
		if strings.HasPrefix(p, "sample:") {
			continue
		}
		if _, err := os.Stat(p); err != nil {
			out[p] = true
		}
	}
	return out
}

// locate asks where an app's APK went, and reads it there: the entry keeps
// its settings when it is the same package.
func (a *App) locate(e *entry) {
	parent := a.window
	a.async(func() {
		paths, err := mygo.Dialog.Open(mygo.OpenDialogOptions{Parent: parent, Title: L.LocateTitle(e.APK.Label), Filters: []mygo.FileFilter{{Name: L.AndroidApps(), Extensions: []string{"apk"}}}})
		if err != nil || len(paths) == 0 {
			return
		}
		a.later(func() { a.inspect(paths[0]) })
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
	if a.missing[e.APK.Path] {
		a.locate(e)
		return
	}
	if !e.runnable() || !a.report.Ready() {
		a.openDetails(e.ID)
		return
	}
	gfx := e.Graphics
	if gfx == "" {
		gfx = a.prefs.Graphics
	}
	if gfx == "" {
		gfx = "auto"
	}
	s := &session{
		id: e.ID, state: winapk.State{Phase: winapk.Starting}, started: time.Now(),
		native: e.Native, profile: a.profile(e), reqGfx: gfx,
	}
	a.open[e.ID] = s
	a.runs[e.ID] = append([]*session{s}, a.runs[e.ID]...)
	if len(a.runs[e.ID]) > maxRuns {
		a.runs[e.ID] = a.runs[e.ID][:maxRuns]
	}
	a.details.open = false
	if inWindow {
		a.newAppWindow(s)
	} else {
		a.showTab(appTab(e.ID), beside)
	}
	waitCh := a.staging[e.ID].done
	if waitCh == nil {
		waitCh = a.stageAPK(e)
	}
	if waitCh == nil {
		a.startRuntime(e, s)
		return
	}
	id := e.ID
	a.async(func() {
		select {
		case <-waitCh:
		case <-a.ctx.Done():
			return
		}
		a.later(func() {
			if a.open[id] != s {
				return
			}
			cur := a.entry(id)
			if cur == nil {
				return
			}
			a.startRuntime(cur, s)
		})
	})
}

// launchRoom returns the target stage dimensions (DIPs) to launch e with when
// Fit the pane is selected so reopened apps start directly at pane size.
func (a *App) launchRoom(e *entry, s *session) size {
	const barH = 44
	if s != nil && s.win != nil {
		return size{fitDefaultW, fitDefaultH}
	}
	if g := a.groupOf(appTab(e.ID)); g != nil {
		if r, ok := a.sizes[appTab(e.ID)]; ok && r.w > 2 && r.h > headerH+barH+2 {
			return size{r.w - 2, r.h - headerH - barH - 2}
		}
		if len(g.tabs()) == 1 && a.panesRoom.w > margin+2 && a.panesRoom.h > margin+headerH+barH+2 {
			return size{a.panesRoom.w - margin - 2, a.panesRoom.h - margin - headerH - barH - 2}
		}
	}
	if a.lastStage.w > 0 && a.lastStage.h > 0 {
		return a.lastStage
	}
	if a.panesRoom.w > margin+2 && a.panesRoom.h > margin+headerH+barH+2 {
		return size{a.panesRoom.w - margin - 2, a.panesRoom.h - margin - headerH - barH - 2}
	}
	return size{fitDefaultW, fitDefaultH}
}

func (a *App) startRuntime(e *entry, s *session) {
	id := e.ID
	if !a.demo && e.APK.Path != "" && !strings.HasPrefix(e.APK.Path, "sample:") {
		if _, err := os.Stat(e.APK.Path); err != nil {
			if a.missing == nil {
				a.missing = map[string]bool{}
			}
			a.missing[e.APK.Path] = true
			a.closeApp(id)
			a.locate(e)
			return
		}
	}
	sw, sh := screenSize(e, s.rotated, a.launchRoom(e, s))
	s.lastSize = size{sw, sh}
	var dataDir string
	if !a.demo {
		dataDir = a.appDataDir()
	}
	spec := winapk.LaunchSpec{
		Path: e.APK.Path, JavaHome: a.prefs.JavaHome, DataDir: dataDir,
		Native: e.Native, Graphics: s.reqGfx, Width: int(sw), Height: int(sh), Window: s.win != nil,
	}
	if e.Native == winapk.NativeRestricted {
		spec.Profile = a.profile(e)
	}
	proc, err := a.rt.Launch(spec, func(st winapk.State) {
		a.later(func() { a.setState(id, s, st) })
	})
	if err != nil {
		st := winapk.State{Phase: winapk.Failed, Category: "DEPENDENCY_MISSING", Log: []string{err.Error()}}
		a.setState(id, s, st)
	} else if proc != nil {
		proc.SetFrameHandler(func(rgba *image.RGBA) {
			a.later(func() {
				if a.open[id] == s {
					s.updateFrame(rgba)
				}
			})
		})
	}
	s.proc = proc
}

// setState keeps a process's new state; a start that failed stays on its
// entry.
func (a *App) setState(id string, s *session, st winapk.State) {
	s.state = st // an older run's log still shows in Diagnostics
	if a.open[id] != s {
		return // an older process of an app opened again
	}
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
		// Not responding shows once Stop has waited this long.
		time.AfterFunc(stopWait+100*time.Millisecond, func() { a.later(func() {}) })
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

// removeEntry stops an app, removes it from the library and deletes its
// managed APK copy in AppData; when keepData is false it also clears all saved
// app data and converted code for that package.
func (a *App) removeEntry(id string, keepData bool) {
	e := a.entry(id)
	if e == nil {
		return
	}
	pkg := e.APK.Package
	apkPath := e.APK.Path
	demo := a.demo
	dataDir := a.appDataDir()
	hadLiveProc := a.open[id] != nil && a.open[id].live()
	waitStage := a.staging[id].done
	a.stageSeq[id]++
	delete(a.staging, id)

	a.closeApp(id)
	a.library = slices.DeleteFunc(a.library, func(x *entry) bool { return x.ID == id })
	delete(a.missing, apkPath)
	a.saveLibrary()
	if !keepData {
		delete(a.appDataBytes, id)
	}
	if !demo && pkg != "" {
		a.async(func() {
			if waitStage != nil {
				<-waitStage
			}
			cleanup := func() error {
				if keepData {
					return winapk.RemoveManagedAPK(dataDir, pkg)
				}
				return winapk.ClearAppArtifacts(dataDir, pkg)
			}
			err := cleanup()
			for i := 0; err != nil && hadLiveProc && i < 20; i++ {
				time.Sleep(100 * time.Millisecond)
				err = cleanup()
			}
			a.later(func() { a.refreshStorage() })
		})
	}
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
	return winapk.DefaultDataDir()
}
