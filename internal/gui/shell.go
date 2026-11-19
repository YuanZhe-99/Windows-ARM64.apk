package gui

import (
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// mainWindow returns the main window, made first when there is none: it
// exists only while shown (closing it ends it while apps in windows of
// their own may go on).
func (a *App) mainWindow() *mygo.Window {
	if a.window != nil {
		return a.window
	}
	w := mygo.NewWindow(mygo.WindowOptions{
		Title: "Windows-ARM64.apk", Width: 1280, Height: 820, MinWidth: 900, MinHeight: 600,
		TitleBarStyle: mygo.TitleBarHidden, HideWindowButtons: true,
		Vibrancy: mygo.VibrancyMica,
		StateKey: "main", Hidden: true,
		Content: ui.View(a.view),
	})
	w.OnClose(a.handleClose)
	_ = w.SetIcon(logoPNG)
	a.window = w
	return w
}

// Focus shows and raises the main window. Safe from any goroutine.
func (a *App) Focus() {
	mygo.RunOnMain(func() {
		w := a.mainWindow()
		w.Show()
		w.Focus()
	})
}

// paneApps are the open apps shown as panes of the main window.
func (a *App) paneApps() []string {
	var out []string
	for _, g := range a.groups {
		for _, t := range g.tabs() {
			if id := appOf(t); id != "" && a.open[id] != nil && a.open[id].live() {
				out = append(out, id)
			}
		}
	}
	return out
}

// windowApps are the open apps in windows of their own, in the library's
// order.
func (a *App) windowApps() []string {
	var out []string
	for _, e := range a.library {
		if s := a.open[e.ID]; s != nil && s.win != nil {
			out = append(out, e.ID)
		}
	}
	return out
}

// handleClose closes the main window: apps open as panes stop with it,
// after the confirmation that lists them while Ask first is on; apps in
// windows of their own keep running unless General says to stop every app.
func (a *App) handleClose(e *mygo.CloseEvent) {
	if a.quitting {
		return
	}
	if len(a.paneApps()) > 0 && a.prefs.AskOnClose && !a.confirm.closing {
		e.PreventDefault()
		a.confirm = confirmState{open: true, kind: confirmClose}
		return
	}
	a.closeMain()
}

// closeMain stops what closing the main window stops and lets it go.
func (a *App) closeMain() {
	a.confirm = confirmState{}
	for _, id := range a.paneApps() {
		a.closeApp(id)
	}
	if a.prefs.CloseStopsAll {
		for _, id := range a.windowApps() {
			a.closeApp(id)
		}
	}
	a.window = nil
	home := &group{}
	home.single(tabHome)
	a.groups, a.cur = []*group{home}, home
}

// view is the main window: the sidebar and the current group's panes, the
// card that gets the Java runtime above them; sheets and dialogs over it.
func (a *App) view(c *ui.Context) {
	applyTheme(c)
	p := pal(c)
	shell := ui.Row(c.Key("shell")).Fill().AlignItems(ui.Stretch).TextColor(p.text).FontSize(fontBody)
	if !c.Vibrancy() {
		shell.Background(p.base)
	}
	// An APK dropped anywhere on the window opens Add an app.
	if apk := firstAPK(shell.DroppedFiles()); apk != "" {
		a.inspect(apk)
	}
	a.retitle()
	shell.Children(func() {
		left := float32(0)
		if a.prefs.SidebarHidden {
			left = margin / 2
		} else {
			a.sidebar(c)
		}
		ui.Column(c.Key("main")).Grow(1).MinWidth(0).MinHeight(0).Padding(margin/2, margin/2, margin/2, left).Children(func() {
			a.javaCard(c)
			a.panesView(c)
		})
	})
	a.toastView(c)
	a.settingsSheet(c)
	a.addSheet(c)
	a.docViewSheet(c)
	a.installDialog(c)
	a.confirmDialog(c)
	switch {
	case c.Shortcut(ui.Cmd, ui.KeyComma):
		a.openSettings(spGeneral)
	case c.Shortcut(ui.Cmd, ui.KeyO):
		a.chooseAPK()
	case c.Shortcut(ui.Cmd, ui.KeyW) && a.cur != nil:
		// Closes the pane in focus; Home stays.
		if t := a.cur.focus; appOf(t) != "" {
			a.closeApp(appOf(t))
		} else if t == tabDiag {
			a.removeTab(t)
		}
	}
	if a.runPending() {
		c.Invalidate()
	}
}

// sidebar is Hide the sidebar and Add APK…, Home, the open groups and the
// apps in windows of their own, Settings and the runtime's state.
func (a *App) sidebar(c *ui.Context) {
	p := pal(c)
	ui.Column(c.Key("sidebar")).Width(sidebarW).Shrink(0).Padding(8, 10, 10, 10).Gap(2).Children(func() {
		ui.Row(c).FillWidth().Height(36).Gap(6).Margin(0, 0, 8, 0).Children(func() {
			if iconButton(c, "sb-hide", icSidebar, L.HideSidebar()).Clicked() {
				pr := a.prefs
				pr.SidebarHidden = true
				a.setPrefs(pr)
			}
			ui.Box(c).Grow(1).FillHeight().DragWindow()
			if button(c, "sb-add", L.AddAPK(), icPlus).Clicked() {
				a.chooseAPK()
			}
		})
		hg := a.homeGroup()
		if sbRow(c, "sb-home", a.cur == hg && !a.details.open, func() {
			mark(c, 22)
			ui.Text(c, L.NavHome()).FontWeight(600).Grow(1)
			muted(c, L.AppsCount(len(a.library)), fontSmall)
		}).Clicked() {
			a.details.open = false
			a.cur = hg
			hg.focus = tabHome
		}
		open := a.openGroups()
		ui.Row(c).FillWidth().Padding(14, 8, 4, 8).Children(func() {
			muted(c, L.SidebarOpen(), fontSmall).FontWeight(600).Grow(1)
			if n := len(a.paneApps()) + len(a.windowApps()); n > 0 {
				muted(c, strconv.Itoa(n), fontSmall)
			}
		})
		if len(open) == 0 && len(a.windowApps()) == 0 {
			muted(c, L.NothingOpen(), fontSmall).Padding(4, 8).Wrap()
		}
		list := ui.Scroll(c.Key("sb-open")).Grow(1).FillWidth().Gap(2).Radius(radiusCard)
		// A pane dropped here leaves its group for one of its own.
		if _, over := ui.DragOver[tabDrag](list); over {
			list.Background(p.accent.Alpha(0.12))
		}
		if d, ok := ui.Drop[tabDrag](list); ok {
			a.afterFrame(func() { a.detach(d.tab) })
		}
		list.Children(func() {
			for gi, g := range open {
				a.groupEntry(c, gi, g)
			}
			if ws := a.windowApps(); len(ws) > 0 {
				muted(c, L.InOwnWindows(), fontSmall).FontWeight(600).Padding(14, 8, 4, 8)
				for _, id := range ws {
					a.tabRowExt(c, "sb-win-"+id, appTab(id), false, true, func() { a.reveal(id) })
				}
			}
		})
		if sbRow(c, "sb-settings", false, func() {
			ui.Icon(c, icGear).FontSize(18)
			ui.Text(c, L.NavSettings()).Grow(1)
		}).Clicked() {
			a.openSettings(spGeneral)
		}
		ready := a.report.Ready()
		b := ui.ButtonBase(c.Key("sb-status")).FillWidth().Padding(10).Margin(6, 0, 0, 0).Radius(radiusCtl).BorderWidth(1, 0, 0, 0).BorderColor(p.border).Label(L.OpenDiagnostics())
		if b.Hovered() {
			b.Background(p.pane.Alpha(0.5))
		}
		b.Children(func() {
			ui.Row(c).Gap(10).Children(func() {
				col, word, sub := p.success, L.StatusReady(), L.StatusJavaFound()
				switch {
				case a.checking && a.checked.IsZero():
					col, word, sub = p.muted, L.StatusChecking(), ""
				case !ready:
					col, word, sub = p.warn, L.StatusNotReady(), L.StatusJavaMissing()
				}
				dot(c, col)
				ui.Column(c).Children(func() {
					ui.Text(c, word).FontSize(13).FontWeight(600)
					if sub != "" {
						muted(c, sub, fontSmall)
					}
				})
			})
		})
		if b.Clicked() {
			a.showTab(tabDiag, false)
		}
	})
}

// openGroups are the groups listed under Open: each but Home's own while
// it shows Home alone.
func (a *App) openGroups() []*group {
	var out []*group
	for _, g := range a.groups {
		if ts := g.tabs(); len(ts) == 1 && ts[0] == tabHome {
			continue
		}
		out = append(out, g)
	}
	return out
}

// groupEntry is a group in the sidebar: a pane alone as a row, several as
// a card with their names over their rows.
func (a *App) groupEntry(c *ui.Context, gi int, g *group) {
	p := pal(c)
	ts := g.tabs()
	pick := func(t string) func() { return func() { a.cur, g.focus = g, t } }
	if len(ts) == 1 {
		a.tabRow(c, "sb-g"+strconv.Itoa(gi), ts[0], a.cur == g, pick(ts[0]))
		return
	}
	names := make([]string, len(ts))
	for i, t := range ts {
		names[i] = a.tabName(t)
	}
	card := ui.Column(c.Key("sb-card-"+strconv.Itoa(gi))).FillWidth().Padding(6).Gap(2).Radius(radiusCard).Margin(2, 0)
	if a.cur == g {
		card.Background(p.pane.Alpha(0.9)).Border(1, p.border)
	} else {
		card.Background(p.pane.Alpha(0.35))
	}
	card.Children(func() {
		splitIcon := icSplitR
		if g.root != nil && !g.root.across {
			splitIcon = icSplitD
		}
		ui.Row(c).Gap(6).Padding(2, 6).AlignItems(ui.Center).Children(func() {
			ui.Icon(c, splitIcon).FontSize(14).TextColor(p.muted)
			muted(c, strings.Join(names, " + "), fontSmall).FontWeight(600).Ellipsis("…").MaxLines(1)
		})
		for _, t := range ts {
			a.tabRow(c, "sb-g"+strconv.Itoa(gi)+"-"+t, t, a.cur == g && g.focus == t, pick(t))
		}
	})
}

// tabRow is a tab's row in the sidebar: its icon, name and state in words.
func (a *App) tabRow(c *ui.Context, key, t string, selected bool, onClick func()) {
	a.tabRowExt(c, key, t, selected, false, onClick)
}

func (a *App) tabRowExt(c *ui.Context, key, t string, selected, ownWin bool, onClick func()) {
	p := pal(c)
	row := sbRow(c, key, selected, func() {
		a.tabIcon(c, t, 22)
		ui.Column(c).Grow(1).MinWidth(0).Children(func() {
			ui.Text(c, a.tabName(t)).FontWeight(600).FontSize(13).Ellipsis("…").MaxLines(1)
			if t == tabDiag {
				muted(c, L.DiagSub(), fontSmall).Ellipsis("…").MaxLines(1)
			} else if s := a.open[appOf(t)]; s != nil {
				st := ui.Text(c, stateWords(s, false, a.entry(s.id), a)).FontSize(fontSmall).Ellipsis("…").MaxLines(1)
				if s.state.Phase == winapk.Failed {
					st.TextColor(p.danger)
				} else {
					st.TextColor(p.muted)
				}
			}
		})
		if ownWin {
			ui.Icon(c, icWindow).FontSize(15).TextColor(p.muted)
		}
	})
	row.Drag(tabDrag{t})
	if e := a.entry(appOf(t)); e != nil {
		row.ContextMenu(func(m *ui.Menu) { a.rowMenu(m, e) })
	}
	if row.Clicked() {
		onClick()
	}
}

// sbRow is a row of the sidebar; selected shows the accent's bar.
func sbRow(c *ui.Context, key string, selected bool, body func()) ui.Element {
	p := pal(c)
	b := ui.ButtonBase(c.Key(key)).FillWidth().MinHeight(36).Padding(5, 10).Radius(radiusCtl).Selected(selected)
	switch {
	case selected:
		b.Background(p.pane.Alpha(0.9))
	case b.Hovered():
		b.Background(p.pane.Alpha(0.5))
	}
	b.Children(func() {
		if selected {
			ui.Box(c).Absolute().Left(0).Top(10).Size(3, 16).Radius(2).Background(p.accent)
		}
		ui.Row(c).Gap(10).FillWidth().TextColor(p.text).Children(body)
	})
	return b
}

// tabName is the name a tab shows.
func (a *App) tabName(t string) string {
	switch t {
	case tabHome:
		return L.NavHome()
	case tabDiag:
		return L.Diagnostics()
	}
	if e := a.entry(appOf(t)); e != nil {
		return e.APK.Label
	}
	return appOf(t)
}

// tabIcon is a tab's icon.
func (a *App) tabIcon(c *ui.Context, t string, size float32) {
	switch t {
	case tabHome:
		mark(c, size)
	case tabDiag:
		tile(c, icPulse, size, pal(c).hover, pal(c).text)
	default:
		if e := a.entry(appOf(t)); e != nil {
			appIcon(c, e, size)
		}
	}
}

// stateWords is an open app's state in words; full adds how its native
// code and graphics run, for its pane's header.
func stateWords(s *session, full bool, e *entry, a *App) string {
	switch s.state.Phase {
	case winapk.Starting:
		return L.StateStarting()
	case winapk.NotResponding:
		return L.StateNotResponding()
	case winapk.Stopped:
		return L.StateStopped()
	case winapk.Failed:
		return L.StateFailed()
	}
	if !s.stopAt.IsZero() && time.Since(s.stopAt) > stopWait {
		return L.StateNotResponding()
	}
	w := L.StateRunning()
	if e != nil && e.Native == winapk.NativeRestricted && len(e.APK.NativeLibs) > 0 {
		if full {
			w += " · " + L.NativeCodeProfile(string(a.profile(e)))
		} else {
			w += " · " + L.NativeCode()
		}
	}
	if full {
		switch s.state.Graphics {
		case "vulkan", "gles-vulkan":
			w += " · Vulkan"
		case "gles-d3d11":
			w += " · Direct3D 11"
		case "gles":
			w += " · OpenGL ES"
		}
	}
	return w
}

// toastView is the short message at the window's foot.
func (a *App) toastView(c *ui.Context) {
	if a.toast == "" {
		return
	}
	p := pal(c)
	ui.Box(c).Attach(ui.AnchorBottom, ui.AnchorBottom).Bottom(24).Padding(10, 16).Radius(radiusCard).Background(p.text).Role(ui.RoleStatus).Children(func() {
		ui.Text(c, a.toast).TextColor(p.pane)
	})
}

// windowButtons are minimize, maximize and close for w, drawn by the view
// after a separator: the system's own are hidden.
func windowButtons(c *ui.Context, key string, w *mygo.Window) {
	p := pal(c)
	ui.Box(c).Size(1, 20).Background(p.border).Margin(0, 4)
	caption := func(k string, glyph *ui.SVG, label string, close bool) ui.Element {
		b := ui.ButtonBase(c.Key(key+k)).Size(40, 32).Radius(radiusCtl).Center().Label(label).Tooltip(label)
		fg := p.text
		if b.Hovered() {
			if close {
				b.Background(ui.Hex("#C42B1C"))
				fg = ui.Hex("#FFFFFF")
			} else {
				b.Background(p.hover)
			}
		}
		b.Children(func() { ui.Icon(c, glyph).FontSize(10).TextColor(fg) })
		return b
	}
	if caption("-min", winMin, L.WindowMinimize(), false).Clicked() && w != nil {
		w.Minimize()
	}
	maximized := w != nil && w.IsMaximized()
	glyph, label := winMax, L.WindowMaximize()
	if maximized {
		glyph, label = winRest, L.WindowRestore()
	}
	if caption("-max", glyph, label, false).MaximizeButton().Clicked() && w != nil {
		if maximized {
			w.Unmaximize()
		} else {
			w.Maximize()
		}
	}
	if caption("-close", winClose, L.WindowClose(), true).Clicked() && w != nil {
		w.Close()
	}
}

// retitle names the main window after the pane in focus, as the taskbar
// shows it.
func (a *App) retitle() {
	if a.window == nil || a.cur == nil {
		return
	}
	title := "Windows-ARM64.apk"
	if t := a.cur.focus; t != tabHome {
		title = a.tabName(t) + " — " + title
	}
	if a.window.Title() != title {
		a.window.SetTitle(title)
	}
}

// firstAPK is the first .apk among paths, "" for none.
func firstAPK(paths []string) string {
	for _, f := range paths {
		if strings.EqualFold(fileExt(f), ".apk") {
			return f
		}
	}
	return ""
}

// OpenFiles opens Add an app with the first APK among args, as given on
// the command line or to a second launch; relative paths are taken from
// dir. It never makes the app the one that opens .apk files.
func (a *App) OpenFiles(args []string, dir string) {
	var abs []string
	for _, f := range args {
		if !filepath.IsAbs(f) && dir != "" {
			f = filepath.Join(dir, f)
		}
		abs = append(abs, f)
	}
	if apk := firstAPK(abs); apk != "" {
		a.later(func() { a.inspect(apk) })
	}
}

// chooseAPK asks for an APK and opens Add an app with it.
func (a *App) chooseAPK() {
	parent := a.window
	a.async(func() {
		paths, err := mygo.Dialog.Open(mygo.OpenDialogOptions{Parent: parent, Title: L.AddAPKTitle(), Filters: []mygo.FileFilter{{Name: L.AndroidApps(), Extensions: []string{"apk"}}}})
		if err != nil || len(paths) == 0 {
			return
		}
		a.later(func() { a.inspect(paths[0]) })
	})
}
