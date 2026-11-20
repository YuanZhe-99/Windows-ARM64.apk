package gui

import (
	"errors"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/egoist/mygo/i18n"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// harness is the interface over a Mock: work runs inline, and what the
// Mock's processes report waits in a queue until settle runs it, as the
// main thread would.
type harness struct {
	a *App
	q chan func()
}

func newHarness(t *testing.T, withJava, samples bool) *harness {
	t.Helper()
	m := winapk.NewMock(withJava, 0)
	h := &harness{q: make(chan func(), 256)}
	h.a = New(m, "", true)
	h.a.later = func(fn func()) { h.q <- fn }
	h.a.async = func(fn func()) { fn() }
	if samples {
		h.a.AddSamples(m.Samples())
	}
	h.a.Start()
	h.settle()
	return h
}

// settle runs what was reported until nothing more comes for a moment.
func (h *harness) settle() {
	for {
		select {
		case fn := <-h.q:
			fn()
		case <-time.After(30 * time.Millisecond):
			return
		}
	}
}

// shoot writes the window to SCREENSHOT_DIR when it is set, to compare
// with design/.
func shoot(t *testing.T, tt *ui.Tester, name string) {
	t.Helper()
	dir := os.Getenv("SCREENSHOT_DIR")
	if dir == "" {
		return
	}
	f, err := os.Create(filepath.Join(dir, name+".png"))
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	if err := png.Encode(f, tt.Image()); err != nil {
		t.Fatal(err)
	}
}

// TestScreensRender builds each screen of design/ in both appearances and
// both languages: none panics, and each shows what makes it that screen.
func TestScreensRender(t *testing.T) {
	screens := []struct {
		name     string
		withJava bool
		samples  bool
		setup    func(a *App)
		want     func(L.Messages) string
	}{
		{"library", true, true, func(a *App) {}, L.Messages.HomeApps},
		{"library-empty", true, false, func(a *App) {}, L.Messages.EmptyTitle},
		{"missing-runtime", false, true, func(a *App) {}, L.Messages.JavaCardTitle},
		{"split", true, true, func(a *App) {
			a.sizes[tabHome] = size{1000, 700}
			a.launch(a.entry("com.example.noteslite"), false, false)
			a.sizes[appTab("com.example.noteslite")] = size{1000, 700}
			a.launch(a.entry("com.example.gltriangle"), false, true)
		}, L.Messages.ScreenSize},
		{"start-failed", true, true, func(a *App) { a.launch(a.entry("com.example.tilepuzzle"), false, false) }, L.Messages.TryNativeOff},
		{"inspect", true, true, func(a *App) { a.inspect("TilePuzzle-2.3.apk") }, L.Messages.WhatsInside},
		{"details", true, true, func(a *App) {
			a.openDetails("com.example.gltriangle")
			a.details.advanced = true
		}, L.Messages.ProfileTitle},
		{"diagnostics", true, true, func(a *App) { a.showTab(tabDiag, false) }, L.Messages.CantRunYetTitle},
		{"diagnostics-logs", true, true, func(a *App) {
			a.launch(a.entry("com.example.tilepuzzle"), false, false)
			a.diag.page = 1
			a.showTab(tabDiag, false)
		}, L.Messages.ErrorsOnly},
		{"settings-general", true, true, func(a *App) { a.openSettings(spGeneral) }, L.Messages.WhenClosing},
		{"settings-runtime", true, true, func(a *App) { a.openSettings(spRuntime) }, L.Messages.KeepUpdated},
		{"settings-about", true, true, func(a *App) { a.openSettings(spAbout) }, L.Messages.BasedOn},
		{"close-confirm", true, true, func(a *App) {
			a.launch(a.entry("com.example.noteslite"), false, false)
			a.confirm = confirmState{open: true, kind: confirmClose}
		}, L.Messages.DontAskAgain},
		{"remove-confirm", true, true, func(a *App) {
			a.openRemoveConfirm("com.example.noteslite")
		}, L.Messages.RemoveKeepData},
		{"runtime-install", false, true, func(a *App) {
			a.install = installState{running: true, shown: true, step: winapk.StepDownload, frac: 0.4}
		}, L.Messages.GettingJava},
		{"native-pane", true, true, func(a *App) {
			a.launch(a.entry("com.example.gltriangle"), false, false)
		}, L.Messages.DontAskAgain},
	}
	for _, lang := range []string{"en", "zh-Hans"} {
		for _, dark := range []bool{false, true} {
			for _, sc := range screens {
				name := sc.name + "-" + lang
				if dark {
					name += "-dark"
				}
				t.Run(name, func(t *testing.T) {
					i18n.SetLocaleFunc(func() string { return lang })
					defer i18n.SetLocaleFunc(nil)
					h := newHarness(t, sc.withJava, sc.samples)
					tt := ui.NewTester(h.a.view, 1280, 820)
					tt.SetLocale(lang)
					tt.SetDark(dark)
					tt.Frame()
					sc.setup(h.a)
					h.settle()
					tt.Frame()
					tt.Frame()
					if want := sc.want(L.In(lang)); !tt.HasText(want) {
						t.Fatalf("%s does not show %q", sc.name, want)
					}
					shoot(t, tt, name)
				})
			}
		}
	}
}

// TestOpenAsPaneThenClose: Open makes the app a pane of its own and an
// entry of its own; × stops it and takes the pane away.
func TestOpenAsPaneThenClose(t *testing.T) {
	h := newHarness(t, true, true)
	a := h.a
	e := a.entry("com.example.noteslite")
	a.launch(e, false, false)
	h.settle()
	s := a.open[e.ID]
	if s == nil || s.state.Phase != winapk.Running {
		t.Fatalf("not running: %+v", s)
	}
	if a.cur == a.homeGroup() || a.cur.focus != appTab(e.ID) {
		t.Fatal("the app is not shown as a group of its own")
	}
	a.closeApp(e.ID)
	h.settle()
	if a.open[e.ID] != nil || a.groupOf(appTab(e.ID)) != nil {
		t.Fatal("the pane stayed after ×")
	}
	if a.cur != a.homeGroup() {
		t.Fatal("the main window does not fall back to Home")
	}
}

// TestSplitKeepsRoomAndLimit: a split is offered only with room for two
// panes of at least 360 × 280, and a group shows at most four.
func TestSplitKeepsRoomAndLimit(t *testing.T) {
	if canSplit(size{2*paneMinW - 1, 900}, true) {
		t.Fatal("split beside without room")
	}
	if !canSplit(size{2*paneMinW + margin, 900}, true) {
		t.Fatal("no split beside with room")
	}
	if canSplit(size{900, 2 * paneMinH}, false) {
		t.Fatal("split below without room")
	}
	g := &group{}
	g.single("a")
	for _, x := range []string{"b", "c", "d"} {
		if !g.split(x, g.focus, true) {
			t.Fatalf("split %s refused", x)
		}
	}
	if g.split("e", "a", false) {
		t.Fatal("a fifth pane was split in")
	}
	if got := g.tabs(); len(got) != 4 || got[0] != "a" {
		t.Fatalf("tabs %v", got)
	}
	g.remove("a")
	if got := g.tabs(); len(got) != 3 || g.focus == "a" {
		t.Fatalf("after remove: %v, focus %q", got, g.focus)
	}
}

// TestFailedStartKeepsCategoryOutOfTheWords: a start that fails says so in
// words in the pane, and its category reaches only Diagnostics and the
// entry (AGENTS.md: no error codes in the main interface).
func TestFailedStartKeepsCategoryOutOfTheWords(t *testing.T) {
	h := newHarness(t, true, true)
	a := h.a
	e := a.entry("com.example.tilepuzzle")
	a.launch(e, false, false)
	h.settle()
	if got := a.open[e.ID].state; got.Phase != winapk.Failed || got.Category != "GUEST_SYMBOL_UNRESOLVED" {
		t.Fatalf("state %+v", got)
	}
	if e.LastFailed != "GUEST_SYMBOL_UNRESOLVED" {
		t.Fatal("the entry doesn't keep the failure")
	}
	tt := ui.NewTester(a.view, 1280, 820)
	tt.Frame()
	if tt.HasText("GUEST_SYMBOL_UNRESOLVED") {
		t.Fatal("the category shows in the pane")
	}
	a.diag.page, a.diag.app = 1, e.ID
	a.showTab(tabDiag, false)
	tt.Frame()
	tt.Frame()
	if !tt.HasText("GUEST_SYMBOL_UNRESOLVED") {
		t.Fatal("Diagnostics doesn't show the category")
	}
}

// TestNoJavaNoOpen: without a Java runtime Open is unavailable and getting
// Windows-ARM64.apk's own makes the PC ready.
func TestNoJavaNoOpen(t *testing.T) {
	h := newHarness(t, false, true)
	a := h.a
	if a.report.Ready() {
		t.Fatal("ready without Java")
	}
	a.launch(a.entry("com.example.noteslite"), false, false)
	if len(a.open) != 0 {
		t.Fatal("an app started without Java")
	}
	a.installJava()
	h.settle()
	if !a.report.Ready() || !a.report.Java.Own {
		t.Fatalf("not ready after getting Java: %+v", a.report.Java)
	}
}

// TestNewAppsNativeDefaults: pure-Java apps start with native off, while
// needs-native APKs pre-select Restricted Profile N2 (with user override).
func TestNewAppsNativeDefaults(t *testing.T) {
	h := newHarness(t, true, false)
	a := h.a
	a.inspect("HelloJava.apk")
	h.settle()
	a.addEntry(false)
	if ej := a.entry("com.example.hellojava"); ej == nil || ej.Native != winapk.NativeOff {
		t.Fatalf("HelloJava entry %+v, want NativeOff", ej)
	}

	a.inspect("TilePuzzle-2.3.apk")
	h.settle()
	a.addEntry(false)
	e := a.entry("com.example.tilepuzzle")
	if e == nil || e.Native != winapk.NativeRestricted || a.profile(e) != winapk.N2 {
		t.Fatalf("TilePuzzle entry %+v profile=%s, want Restricted N2", e, a.profile(e))
	}

	// Explicit user opt-out in Add sheet is honored.
	a.inspect("TilePuzzle-2.3.apk")
	h.settle()
	a.add.nativeMode = 0
	a.addEntry(false)
	if e = a.entry("com.example.tilepuzzle"); e == nil || e.Native != winapk.NativeOff {
		t.Fatalf("TilePuzzle opt-out entry %+v, want NativeOff", e)
	}
}

// TestCloseStopsPanesKeepsWindows: closing the main window stops the apps
// in panes and, by default, keeps apps in windows of their own running.
func TestCloseStopsPanesKeepsWindows(t *testing.T) {
	h := newHarness(t, true, true)
	a := h.a
	a.launch(a.entry("com.example.noteslite"), false, false)
	h.settle()
	// A window of its own needs the app's loop; it is stood in for here.
	s := &session{id: "com.example.hellojava", state: winapk.State{Phase: winapk.Running}}
	proc, _ := a.rt.Launch(winapk.LaunchSpec{Path: "sample:HelloJava.apk", Window: true}, func(winapk.State) {})
	s.proc = proc
	a.open[s.id] = s
	if got := a.paneApps(); len(got) != 1 {
		t.Fatalf("panes %v", got)
	}
	a.closeMain()
	h.settle()
	if a.open["com.example.noteslite"] != nil {
		t.Fatal("the pane's app kept running")
	}
	if a.open["com.example.hellojava"] == nil {
		t.Fatal("the window's app was stopped")
	}
}

// TestDropPlacesPanes: a pane dropped on another's edge joins its group on
// that side; in its middle it takes its place, the other going to a group
// of its own; within a group two panes trade places; on the sidebar's list
// a pane leaves its group.
func TestDropPlacesPanes(t *testing.T) {
	h := newHarness(t, true, true)
	a := h.a
	notes, gl := appTab("com.example.noteslite"), appTab("com.example.gltriangle")
	a.launch(a.entry("com.example.noteslite"), false, false)
	a.launch(a.entry("com.example.gltriangle"), false, false)
	h.settle()
	a.dropTab(notes, tabHome, zoneRight)
	g := a.homeGroup()
	if got := g.tabs(); len(got) != 2 || got[0] != tabHome || got[1] != notes || !g.root.across {
		t.Fatalf("edge drop: %v", got)
	}
	a.dropTab(gl, notes, zoneMiddle)
	if got := g.tabs(); len(got) != 2 || got[1] != gl {
		t.Fatalf("middle drop: %v", got)
	}
	if og := a.groupOf(notes); og == nil || og == g || len(og.tabs()) != 1 {
		t.Fatal("the pane replaced has no group of its own")
	}
	a.dropTab(tabHome, gl, zoneMiddle)
	if got := g.tabs(); got[0] != gl || got[1] != tabHome {
		t.Fatalf("swap: %v", got)
	}
	a.detach(gl)
	if og := a.groupOf(gl); og == g || len(og.tabs()) != 1 || len(g.tabs()) != 1 {
		t.Fatal("detach left the pane in its group")
	}
}

// TestDragRowOntoPane drags an open app's sidebar row onto Home's right
// edge in the window: it splits Home there.
func TestDragRowOntoPane(t *testing.T) {
	h := newHarness(t, true, true)
	a := h.a
	a.launch(a.entry("com.example.noteslite"), false, false)
	h.settle()
	a.cur = a.homeGroup()
	tt := ui.NewTester(a.view, 1280, 820)
	tt.Frame()
	tt.Frame()
	src, ok := tt.Find("Notes Lite")
	if !ok {
		t.Fatal("no sidebar row")
	}
	tt.Press(src.X+10, src.Y+5)
	for i := 1; i <= 10; i++ {
		f := float32(i) / 10
		tt.Move(src.X+10+(1230-src.X-10)*f, src.Y+5+(400-src.Y-5)*f)
		tt.Frame()
	}
	if !tt.HasText(L.DropRight()) {
		t.Fatal("no drop zones while dragging")
	}
	tt.Release(1230, 400)
	tt.Frame()
	tt.Frame()
	if got := a.homeGroup().tabs(); len(got) != 2 || got[1] != appTab("com.example.noteslite") {
		t.Fatalf("after drop: %v", got)
	}
}

// TestRunsKeepTheirLogs: a run's log stays in Diagnostics after its pane
// closes, and Errors and warnings only keeps just those lines.
func TestRunsKeepTheirLogs(t *testing.T) {
	h := newHarness(t, true, true)
	a := h.a
	e := a.entry("com.example.tilepuzzle")
	a.launch(e, false, false)
	h.settle()
	a.closeApp(e.ID)
	h.settle()
	a.diag.app = e.ID
	r := a.diagRun()
	if r == nil || r.state.Phase != winapk.Failed || len(r.state.Log) == 0 {
		t.Fatalf("run %+v", r)
	}
	for _, l := range logLines(r.state.Log, true) {
		if lv := logLevel(l); lv != "error" && lv != "warn" {
			t.Fatalf("kept %q", l)
		}
	}
	if len(logLines(r.state.Log, true)) == 0 {
		t.Fatal("the error line was dropped")
	}
}

// TestMissingFileAsksWhere: an app whose APK is gone says so on Home and
// isn't launched.
func TestMissingFileAsksWhere(t *testing.T) {
	h := newHarness(t, true, false)
	a := h.a
	gone := filepath.Join(t.TempDir(), "Gone.apk")
	a.library = append(a.library, a.newEntry(winapk.APK{Path: gone, Package: "com.example.gone", Label: "Gone", Verdict: winapk.CanRun}))
	a.async = func(fn func()) {} // no file dialog
	a.checking = false
	h.a.async = func(fn func()) { fn() }
	a.check()
	h.settle()
	if !a.missing[gone] {
		t.Fatal("the missing file isn't noticed")
	}
	a.async = func(fn func()) {}
	a.launch(a.entry("com.example.gone"), false, false)
	if len(a.open) != 0 {
		t.Fatal("an app without its file started")
	}
}

// TestOpenFilesTakesAnAPK: the command line's first APK opens Add an app,
// relative to the second launch's folder; other files are ignored.
func TestOpenFilesTakesAnAPK(t *testing.T) {
	h := newHarness(t, true, false)
	h.a.OpenFiles([]string{"notes.txt", "Hello.apk"}, `C:\Downloads`)
	h.settle()
	if !h.a.add.open || h.a.add.path != filepath.Join(`C:\Downloads`, "Hello.apk") {
		t.Fatalf("add %+v", h.a.add)
	}
}

// TestScreenSizeToldOnce: the app is told its screen when it opens and
// again when it changes.
func TestScreenSizeToldOnce(t *testing.T) {
	h := newHarness(t, true, true)
	a := h.a
	e := a.entry("com.example.noteslite")
	a.launch(e, false, false)
	h.settle()
	tt := ui.NewTester(a.view, 1280, 820)
	tt.Frame()
	tt.Frame()
	s := a.open[e.ID]
	if s.told != (size{360, 720}) {
		t.Fatalf("told %v", s.told)
	}
	a.rotate(s, e)
	tt.Frame()
	if s.told != (size{720, 360}) {
		t.Fatalf("after rotating, told %v", s.told)
	}
	tt.Frame()
	tt.Frame()
	if s.told != (size{720, 360}) {
		t.Fatalf("told %v without a change", s.told)
	}
}

type tapRecord struct {
	winapk.Process
	onFrame func(*image.RGBA)
	actions []int
	xs, ys  []float32
}

func (r *tapRecord) SetFrameHandler(fn func(*image.RGBA)) { r.onFrame = fn }
func (r *tapRecord) SendPointer(act int, x, y float32) {
	r.actions = append(r.actions, act)
	r.xs = append(r.xs, x)
	r.ys = append(r.ys, y)
}

type frameClient struct {
	*winapk.Mock
	rec *tapRecord
}

func (f *frameClient) Launch(spec winapk.LaunchSpec, report func(winapk.State)) (winapk.Process, error) {
	p, err := f.Mock.Launch(spec, report)
	if err != nil {
		return nil, err
	}
	f.rec = &tapRecord{Process: p}
	return f.rec, nil
}

// TestSurfaceBitmapAndPointerForwarding verifies that live RGBA8 frames update
// the session bitmap and clicks on the scaled phone surface forward DIPs.
func TestSurfaceBitmapAndPointerForwarding(t *testing.T) {
	fc := &frameClient{Mock: winapk.NewMock(true, 0)}
	h := &harness{q: make(chan func(), 256)}
	h.a = New(fc, "", true)
	h.a.later = func(fn func()) { h.q <- fn }
	h.a.async = func(fn func()) { fn() }
	h.a.AddSamples(fc.Samples())
	h.a.Start()
	h.settle()

	e := h.a.entry("com.example.noteslite")
	h.a.launch(e, false, false)
	h.settle()

	rgba := image.NewRGBA(image.Rect(0, 0, 360, 720))
	rgba.SetRGBA(10, 10, color.RGBA{R: 46, G: 125, B: 79, A: 255})
	fc.rec.onFrame(rgba)
	h.settle()

	s := h.a.open[e.ID]
	if s.frame == nil {
		t.Fatal("session bitmap was not created from RGBA8 frame")
	}

	tt := ui.NewTester(h.a.view, 1280, 820)
	tt.Frame()
	tt.Frame()
	// Tap near the center of the main pane where the phone surface sits.
	tt.Press(760, 410)
	tt.Release(760, 410)
	tt.Frame()
	if len(fc.rec.actions) < 2 || fc.rec.actions[0] != winapk.PointerDown || fc.rec.actions[1] != winapk.PointerUp {
		t.Fatalf("expected pointer down/up forwarded, got %v", fc.rec.actions)
	}
	if fc.rec.xs[0] <= 0 || fc.rec.xs[0] >= 360 || fc.rec.ys[0] <= 0 || fc.rec.ys[0] >= 720 {
		t.Fatalf("mapped DIP (%v, %v) out of 360x720 phone bounds", fc.rec.xs[0], fc.rec.ys[0])
	}
}

type launchRecordClient struct {
	*winapk.Mock
	specs []winapk.LaunchSpec
}

func (c *launchRecordClient) Launch(spec winapk.LaunchSpec, report func(winapk.State)) (winapk.Process, error) {
	c.specs = append(c.specs, spec)
	return c.Mock.Launch(spec, report)
}

// TestCopyAPKToAppDataAndUninstallModes verifies that added (and legacy-migrated)
// APKs are copied into AppData, survive deletion of the original download, and
// respect both Keep-data and Delete-all removal choices.
func TestCopyAPKToAppDataAndUninstallModes(t *testing.T) {
	cfgDir := t.TempDir()
	dataDir := t.TempDir()
	srcDir := t.TempDir()
	srcAPK := filepath.Join(srcDir, "SampleApp.apk")
	if err := os.WriteFile(srcAPK, []byte("PK\x03\x04sample-apk-v1"), 0o600); err != nil {
		t.Fatal(err)
	}

	lc := &launchRecordClient{Mock: winapk.NewMock(true, 0)}
	h := &harness{q: make(chan func(), 256)}
	h.a = New(lc, cfgDir, false)
	h.a.later = func(fn func()) { h.q <- fn }
	h.a.async = func(fn func()) { fn() }
	pr := h.a.Prefs()
	pr.AppData = dataDir
	h.a.setPrefs(pr)
	h.a.Start()
	h.settle()

	// 1. Add APK -> copied to <AppData>\apks\<package>.apk.
	h.a.inspect(srcAPK)
	h.settle()
	h.a.addEntry(false)
	h.settle()

	const pkg = "com.example.sampleapp"
	e := h.a.entry(pkg)
	if e == nil {
		t.Fatal("added entry missing")
	}
	managed, err := winapk.ManagedAPKPath(dataDir, pkg)
	if err != nil {
		t.Fatal(err)
	}
	if e.APK.Path != managed {
		t.Fatalf("entry APK.Path = %q, want managed %q", e.APK.Path, managed)
	}
	if b, err := os.ReadFile(managed); err != nil || string(b) != "PK\x03\x04sample-apk-v1" {
		t.Fatalf("managed copy = (%q, %v)", b, err)
	}

	// 2. Delete original download -> check() does not mark missing and Launch uses managed copy.
	if err := os.Remove(srcAPK); err != nil {
		t.Fatal(err)
	}
	h.a.check()
	h.settle()
	if h.a.missing[e.APK.Path] || len(h.a.missing) != 0 {
		t.Fatalf("unexpected missing entries after deleting original APK: %+v", h.a.missing)
	}
	h.a.launch(e, false, false)
	h.settle()
	if len(lc.specs) != 1 || lc.specs[0].Path != managed {
		t.Fatalf("Launch specs = %+v, want Path=%q", lc.specs, managed)
	}
	if h.a.open[pkg] == nil || h.a.open[pkg].state.Phase != winapk.Running {
		t.Fatalf("app not running from managed APK: %+v", h.a.open[pkg])
	}

	// Create saved app data & converted code so removal modes can be verified.
	pkgDataDir := winapk.AppDataDir(dataDir, pkg)
	pkgCodeDir := filepath.Join(dataDir, "code", pkg)
	if err := os.MkdirAll(pkgDataDir, 0o700); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(pkgCodeDir, 0o700); err != nil {
		t.Fatal(err)
	}
	savedFile := filepath.Join(pkgDataDir, "prefs.xml")
	if err := os.WriteFile(savedFile, []byte("<map/>"), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(pkgCodeDir, "classes.jar"), []byte("jar"), 0o600); err != nil {
		t.Fatal(err)
	}

	// 3. Remove confirmation defaults to Keep data; removing deletes only the managed APK.
	h.a.openRemoveConfirm(pkg)
	h.settle()
	if !h.a.confirm.keepData {
		t.Fatal("expected confirmRemove to default to keepData=true")
	}
	tt := ui.NewTester(h.a.view, 1280, 820)
	tt.Frame()
	tt.Frame()
	if !tt.HasText(L.RemoveKeepData()) || !tt.HasText(L.RemoveDeleteData()) || !tt.HasText(L.RemoveAPKOnly()) {
		t.Fatal("remove confirmation did not render both keep/delete choices")
	}
	h.a.confirm = confirmState{}
	h.a.removeEntry(pkg, true)
	h.settle()

	if h.a.entry(pkg) != nil {
		t.Fatal("entry stayed in library after remove")
	}
	if _, err := os.Stat(managed); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("managed APK still present after remove: %v", err)
	}
	if b, err := os.ReadFile(savedFile); err != nil || string(b) != "<map/>" {
		t.Fatalf("saved app data was lost on keep-data remove: (%q, %v)", b, err)
	}

	// 4. Reinstalling the APK restores access to the preserved app data, then Delete-all clears everything.
	reinstallAPK := filepath.Join(srcDir, "SampleApp.apk")
	if err := os.WriteFile(reinstallAPK, []byte("PK\x03\x04sample-apk-v2"), 0o600); err != nil {
		t.Fatal(err)
	}
	h.a.inspect(reinstallAPK)
	h.settle()
	h.a.addEntry(true)
	h.settle()
	if b, err := os.ReadFile(managed); err != nil || string(b) != "PK\x03\x04sample-apk-v2" {
		t.Fatalf("reinstalled managed copy = (%q, %v)", b, err)
	}
	if _, err := os.Stat(savedFile); err != nil {
		t.Fatalf("preserved saved data missing after reinstall: %v", err)
	}

	h.a.openRemoveConfirm(pkg)
	h.settle()
	h.a.confirm.keepData = false
	tt.Frame()
	if !tt.HasText(L.RemoveAndDeleteData()) {
		t.Fatal("delete-all button text missing when keepData=false")
	}
	h.a.confirm = confirmState{}
	h.a.removeEntry(pkg, false)
	h.settle()

	if _, err := os.Stat(managed); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("managed APK still present after delete-all: %v", err)
	}
	if _, err := os.Stat(pkgDataDir); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("app data dir still present after delete-all: %v", err)
	}
	if _, err := os.Stat(pkgCodeDir); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("converted code dir still present after delete-all: %v", err)
	}
	if _, err := os.Stat(reinstallAPK); err != nil {
		t.Fatalf("external user APK must never be deleted: %v", err)
	}

	// 5. Pre-existing external entry in library.json migrates automatically on check().
	legacyAPK := filepath.Join(srcDir, "LegacyApp.apk")
	if err := os.WriteFile(legacyAPK, []byte("PK\x03\x04legacy"), 0o600); err != nil {
		t.Fatal(err)
	}
	const legacyPkg = "com.example.legacy"
	h.a.library = append(h.a.library, h.a.newEntry(winapk.APK{
		Path: legacyAPK, Package: legacyPkg, Label: "Legacy", Verdict: winapk.CanRun,
	}))
	h.a.check()
	h.settle()
	legacyManaged, err := winapk.ManagedAPKPath(dataDir, legacyPkg)
	if err != nil {
		t.Fatal(err)
	}
	if got := h.a.entry(legacyPkg).APK.Path; got != legacyManaged {
		t.Fatalf("legacy entry APK.Path after check = %q, want %q", got, legacyManaged)
	}
	if err := os.Remove(legacyAPK); err != nil {
		t.Fatal(err)
	}
	h.a.check()
	h.settle()
	if len(h.a.missing) != 0 {
		t.Fatalf("migrated legacy entry reported missing: %+v", h.a.missing)
	}
}
