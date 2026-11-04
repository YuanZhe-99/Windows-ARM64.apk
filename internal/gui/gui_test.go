package gui

import (
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
		{"settings-general", true, true, func(a *App) { a.openSettings(spGeneral) }, L.Messages.WhenClosing},
		{"settings-runtime", true, true, func(a *App) { a.openSettings(spRuntime) }, L.Messages.KeepUpdated},
		{"settings-about", true, true, func(a *App) { a.openSettings(spAbout) }, L.Messages.BasedOn},
		{"close-confirm", true, true, func(a *App) {
			a.launch(a.entry("com.example.noteslite"), false, false)
			a.confirm = confirmState{open: true, kind: confirmClose}
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

// TestNewAppsNativeOff: an app added starts with native code off, whatever
// it holds (AGENTS.md: guest native is off by default).
func TestNewAppsNativeOff(t *testing.T) {
	h := newHarness(t, true, false)
	a := h.a
	a.inspect("TilePuzzle-2.3.apk")
	h.settle()
	a.addEntry(false)
	e := a.entry("com.example.tilepuzzle")
	if e == nil || e.Native != winapk.NativeOff {
		t.Fatalf("entry %+v", e)
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
