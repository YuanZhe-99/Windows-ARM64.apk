package gui

import (
	"time"

	"github.com/egoist/mygo/transfer"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// panesView is the current group: its split, or the pane maximized over
// it.
func (a *App) panesView(c *ui.Context) {
	g := a.cur
	if g == nil {
		return
	}
	// Esc is Back for the app in focus.
	if s := a.open[appOf(g.focus)]; s != nil && s.live() && c.Shortcut(0, ui.KeyEscape) {
		a.back(s)
	}
	area := ui.Column(c.Key("panes")).Grow(1).FillWidth().MinHeight(0)
	// The zones stay one frame after the drag leaves: the frame a drop
	// lands in has to build the zone it lands on.
	last := a.dragging
	a.dragging, a.zonesFor = "", ""
	if d, over := ui.DragOver[tabDrag](area); over {
		a.dragging = d.tab
	}
	a.zonesFor = a.dragging
	if a.zonesFor == "" {
		a.zonesFor = last
	}
	area.Children(func() {
		if g.zoom != "" {
			a.tabPane(c, g, g.zoom, true, true)
			return
		}
		a.nodeView(c, g, g.root, "p", true, true)
	})
}

// nodeView draws one node of the split: a pane, or its two halves with a
// divider between them. top and right tell whether the node touches the
// window's top and right edges, where the caption buttons go.
func (a *App) nodeView(c *ui.Context, g *group, n *node, path string, top, right bool) {
	if n == nil {
		return
	}
	if n.first == nil {
		a.tabPane(c, g, n.tab, top, right)
		return
	}
	t := c.Theme()
	quiet := *t
	quiet.Border = ui.Transparent
	c.SetTheme(&quiet)
	first := func() {
		c.SetTheme(t)
		a.nodeView(c, g, n.first, path+"a", top, right && !n.across)
	}
	second := func() {
		c.SetTheme(t)
		a.nodeView(c, g, n.second, path+"b", top && n.across, right)
	}
	if n.across {
		ui.SplitPercent(c.Key(path), &n.ratio, first, second).Fill()
	} else {
		ui.SplitVerticalPercent(c.Key(path), &n.ratio, first, second).Fill()
	}
	c.SetTheme(t)
}

// tabPane is a floating pane: rounded, its header and its tab's content;
// in a group of several, an accent edge on the pane in focus.
func (a *App) tabPane(c *ui.Context, g *group, t string, top, right bool) {
	p := pal(c)
	several := len(g.tabs()) > 1 && g.zoom == ""
	ui.Column(c.Key("at-" + t)).Fill().MinWidth(0).MinHeight(0).Padding(margin / 2).Children(func() {
		box := ui.Column(c.Key("pane-"+t)).Grow(1).FillWidth().MinWidth(0).MinHeight(0).Radius(radiusCard).Clip().
			Background(p.pane).Border(1, p.border).Shadow(0, 6, 18, 0, ui.RGBA(20, 30, 45, 0.08))
		r := box.LayoutBounds()
		a.sizes[t] = size{r.W, r.H}
		if several && g.focus == t {
			box.Border(2, p.accent)
		}
		if box.FocusWithin() && g.focus != t {
			g.focus = t
		}
		box.Children(func() {
			a.paneHeader(c, g, t, top, right, r.W)
			ui.Column(c.Key("body-" + t)).Grow(1).FillWidth().MinHeight(0).Children(func() {
				switch t {
				case tabHome:
					a.homeView(c)
				case tabDiag:
					a.diagView(c)
				default:
					a.appBody(c, appOf(t), false)
				}
			})
			if a.zonesFor != "" && a.zonesFor != t {
				a.dropZonesView(c, g, t)
			}
		})
	})
}

// tabDrag is what a dragged pane or sidebar row carries: its tab.
type tabDrag struct{ tab string }

// dropZonesView shows, while a pane is dragged over t's pane, where it may
// go: a pane of the same group only trades places; from elsewhere beside t
// on each side with room, or in its place.
func (a *App) dropZonesView(c *ui.Context, g *group, t string) {
	p := pal(c)
	from := a.zonesFor
	var zones []zoneRect
	middle := L.DropHere()
	switch r := a.sizes[t]; {
	case a.groupOf(from) == g:
		zones, middle = dropZones(false, false), L.DropSwap()
	case len(g.tabs()) >= maxPanes:
		zones = dropZones(false, false)
	default:
		zones = dropZones(canSplit(r, true), canSplit(r, false))
	}
	for _, z := range zones {
		zone := ui.Box(c.Key("drop-" + t + "-" + string(rune('0'+z.zone)))).Absolute().
			LeftPercent(z.x).TopPercent(z.y).WidthPercent(z.w).HeightPercent(z.h).Radius(radiusCard).Center()
		fg := p.muted
		if d, on := ui.DragOver[tabDrag](zone); on {
			a.dragging = d.tab
			zone.Background(p.accent.Alpha(0.25)).Border(2, p.accent)
			fg = p.accentText
		}
		name := middle
		switch z.zone {
		case zoneLeft:
			name = L.DropLeft()
		case zoneRight:
			name = L.DropRight()
		case zoneTop:
			name = L.DropAbove()
		case zoneBottom:
			name = L.DropBelow()
		}
		zone.Children(func() {
			ui.Text(c, name).FontSize(fontSmall).FontWeight(600).TextColor(fg).NoWrap().Padding(4, 10).Radius(12).Background(p.pane).Border(1, p.border)
		})
		if d, ok := ui.Drop[tabDrag](zone); ok {
			zz := z.zone
			a.afterFrame(func() { a.dropFrom(d.tab, t, zz) })
		}
	}
}

// dropFrom drops a dragged tab on target's pane; an app in a window of its
// own comes back into the main window first.
func (a *App) dropFrom(t, target string, z dropZone) {
	if s := a.open[appOf(t)]; s != nil && s.win != nil {
		a.toPane(s.id)
	}
	a.dropTab(t, target, z)
}

// outsideMain tells whether a screen point is outside the main window:
// letting go of a pane there gives it a window of its own; let go inside,
// on no zone, it stays where it was.
func (a *App) outsideMain(x, y int) bool {
	if a.window == nil {
		return false
	}
	b := a.window.Bounds()
	return x < b.X || y < b.Y || x >= b.X+b.Width || y >= b.Y+b.Height
}

// afterFrame runs fn once the frame being built is done: drops change the
// groups being drawn.
func (a *App) afterFrame(fn func()) { a.pending = append(a.pending, fn) }

// runPending runs what afterFrame kept. Returns: whether anything ran.
func (a *App) runPending() bool {
	fns := a.pending
	a.pending = nil
	for _, fn := range fns {
		fn()
	}
	return len(fns) > 0
}

// dragHandle makes el drag t: onto a pane or the sidebar's list in the
// main window, or, for an app, out of it into a window of its own.
func (a *App) dragHandle(el ui.Element, t string) {
	el.Drag(tabDrag{t})
	id := appOf(t)
	if id == "" {
		return
	}
	el.DragData(transfer.TextData(a.tabName(t)), transfer.DragOptions{
		Done: func(r transfer.Result) {
			if r.Operation == transfer.None && !r.Escaped && a.outsideMain(r.Point.X, r.Point.Y) {
				if s := a.open[id]; s != nil && s.win == nil {
					a.toWindow(id)
				}
			}
		},
	})
}

// paneHeader is a pane's 44-point header: the tab's icon, name and state,
// then its buttons; the window's buttons at the end of the top-right
// pane, Show the sidebar at the start of the top-left one while it is
// hidden. An empty part moves the window.
func (a *App) paneHeader(c *ui.Context, g *group, t string, top, right bool, width float32) {
	p := pal(c)
	id := appOf(t)
	s := a.open[id]
	e := a.entry(id)
	fold := width > 0 && width < paneFoldW
	ui.Row(c.Key("head-"+t)).Height(headerH).Shrink(0).FillWidth().Padding(0, 6, 0, 12).Gap(4).AlignItems(ui.Center).BorderWidth(0, 0, 1, 0).BorderColor(p.border).Children(func() {
		if top && a.prefs.SidebarHidden && g.root != nil && a.firstLeaf(g) == t {
			if iconButton(c, "show-sb-"+t, icSidebar, L.ShowSidebar()).Clicked() {
				pr := a.prefs
				pr.SidebarHidden = false
				a.setPrefs(pr)
			}
		}
		title := ui.Row(c.Key("title-" + t)).FillHeight().Gap(8).AlignItems(ui.Center).MinWidth(0).Shrink(1).Clip().Label(a.tabName(t)).Tooltip(L.DragHint())
		a.dragHandle(title, t)
		title.Children(func() {
			a.tabIcon(c, t, 22)
			ui.Text(c, a.tabName(t)).FontWeight(600).NoWrap()
			if s != nil {
				muted(c, stateWords(s, true, e, a), 13).Ellipsis("…").MaxLines(1).Shrink(1).MinWidth(0)
			}
		})
		ui.Box(c).Grow(1).FillHeight().DragWindow()
		r := a.sizes[t]
		splitR, splitD := canSplit(r, true), canSplit(r, false)
		full := len(g.tabs()) >= maxPanes
		if !fold {
			if id != "" && iconButton(c, "towin-"+t, icWindow, L.OpenInOwnWindow()).Clicked() {
				a.toWindow(id)
			} else if t == tabDiag && iconButton(c, "towin-diag", icWindow, L.OpenInOwnWindow()).Clicked() {
				a.openDiagWindow()
			}
			a.splitButton(c, g, t, true, splitR && !full)
			a.splitButton(c, g, t, false, splitD && !full)
			if len(g.tabs()) > 1 {
				if g.zoom == t {
					if iconButton(c, "rest-"+t, icRestore, L.RestorePane()).Clicked() {
						g.zoom = ""
					}
				} else if iconButton(c, "max-"+t, icMax, L.MaximizePane()).Tooltip(L.MaximizeHint()).Clicked() {
					g.zoom, g.focus = t, t
				}
			}
		}
		if s != nil && s.live() && iconButton(c, "stop-"+t, icStop, L.Stop()).Clicked() {
			a.stopApp(id, false)
		} else if s != nil && s.state.Phase == winapk.Failed && e != nil && iconButton(c, "hdr-retry-"+t, icRotate, L.TryAgain()).Clicked() {
			inWin := s.win != nil
			delete(a.open, id)
			a.removeTab(t)
			a.launch(e, inWin, false)
		}
		if t != tabHome {
			more := iconButton(c, "more-"+t, icMore, L.More())
			more.Menu(func(m *ui.Menu) {
				if fold {
					if id != "" && m.Item(L.OpenInOwnWindow()).Icon(miWindow).Chosen() {
						a.toWindow(id)
					} else if t == tabDiag && m.Item(L.OpenInOwnWindow()).Icon(miWindow).Chosen() {
						a.openDiagWindow()
					}
					a.splitMenu(m, g, t, true, L.SplitRight(), splitR && !full)
					a.splitMenu(m, g, t, false, L.SplitDown(), splitD && !full)
					if len(g.tabs()) > 1 {
						if g.zoom == t {
							if m.Item(L.RestorePane()).Icon(miMax).Chosen() {
								g.zoom = ""
							}
						} else if m.Item(L.MaximizePane()).Icon(miMax).Trailing(L.MaximizeHint()).Chosen() {
							g.zoom, g.focus = t, t
						}
					}
					if e != nil {
						m.Separator()
					}
				}
				if e != nil {
					a.appMenuItems(m, e)
				}
			})
			if iconButton(c, "close-"+t, icClose, L.ClosePane()).Clicked() {
				if id != "" {
					a.closeApp(id)
				} else {
					a.removeTab(t)
				}
			}
		} else if fold {
			more := iconButton(c, "more-"+t, icMore, L.More())
			more.Menu(func(m *ui.Menu) {
				a.splitMenu(m, g, t, true, L.SplitRight(), splitR && !full)
				a.splitMenu(m, g, t, false, L.SplitDown(), splitD && !full)
			})
		}
		if top && right {
			windowButtons(c, "win-"+t, a.window)
		}
	})
}

// firstLeaf is the group's top-left pane.
func (a *App) firstLeaf(g *group) string {
	n := g.root
	if g.zoom != "" {
		return g.zoom
	}
	for n != nil && n.first != nil {
		n = n.first
	}
	if n == nil {
		return ""
	}
	return n.tab
}

// splitButton is Split right or Split down: a menu of what may go there.
// Without room it is greyed out and says why.
func (a *App) splitButton(c *ui.Context, g *group, t string, across, room bool) {
	icon, label, why := icSplitD, L.SplitDown(), L.NoRoomBelow()
	if across {
		icon, label, why = icSplitR, L.SplitRight(), L.NoRoomBeside()
	}
	key := "split-d-" + t
	if across {
		key = "split-r-" + t
	}
	b := iconButton(c, key, icon, label)
	if !room {
		b.Disabled(true).Tooltip(why)
		return
	}
	b.Menu(func(m *ui.Menu) { a.splitItems(m, g, t, across) })
}

// splitMenu is Split right or Split down as a submenu of ⋯.
func (a *App) splitMenu(m *ui.Menu, g *group, t string, across bool, label string, room bool) {
	icon := miSplitD
	why := L.NoRoomBelow()
	if across {
		icon = miSplitR
		why = L.NoRoomBeside()
	}
	if !room {
		m.Item(label).Icon(icon).Trailing(why).Disabled(true)
		return
	}
	m.Submenu(label, func(m *ui.Menu) { a.splitItems(m, g, t, across) }).Icon(icon)
}

// splitItems are what a split may show beside t: apps in standalone windows,
// panes of other groups and Home, then Open an app… with apps not open yet.
func (a *App) splitItems(m *ui.Menu, g *group, t string, across bool) {
	head := L.BelowHeading(a.tabName(t))
	if across {
		head = L.BesideHeading(a.tabName(t))
	}
	m.Item(head).Disabled(true)
	in := map[string]bool{}
	for _, x := range g.tabs() {
		in[x] = true
	}
	for _, id := range a.windowApps() {
		if e := a.entry(id); e != nil {
			if m.Item(e.APK.Label).Trailing(L.FromItsWindow()).Chosen() {
				a.toPane(id)
				a.splitWith(t, appTab(id), across)
			}
		}
	}
	for _, og := range a.groups {
		for _, x := range og.tabs() {
			if in[x] {
				continue
			}
			it := m.Item(a.tabName(x))
			if x == tabHome {
				it.Icon(miHome)
			}
			if it.Chosen() {
				a.splitWith(t, x, across)
			}
		}
	}
	m.Separator()
	m.Submenu(L.OpenAnApp(), func(m *ui.Menu) {
		for _, e := range a.library {
			if s := a.open[e.ID]; (s != nil && s.live()) || !e.runnable() {
				continue
			}
			if m.Item(e.APK.Label).Disabled(!a.report.Ready()).Chosen() {
				a.launch(e, false, false)
				a.splitWith(t, appTab(e.ID), across)
			}
		}
	}).Icon(miPlus)
}

// appMenuItems are an app's own items, in its pane's ⋯ and on Home.
func (a *App) appMenuItems(m *ui.Menu, e *entry) {
	s := a.open[e.ID]
	if s != nil && s.live() {
		if m.Item(L.Restart()).Icon(miRotate).Chosen() {
			inWin := s.win != nil
			a.closeApp(e.ID)
			a.launch(e, inWin, false)
		}
		if m.Item(L.ForceStop()).Icon(miKill).Trailing(L.EndsAtOnce()).Chosen() {
			a.stopApp(e.ID, true)
		}
		m.Separator()
	}
	if m.Item(L.DetailsAndSettings()).Icon(miGear).Chosen() {
		a.openDetails(e.ID)
	}
	if m.Item(L.ShowLog()).Icon(miDoc).Chosen() {
		a.diag.page, a.diag.app = 1, e.ID
		a.showTab(tabDiag, false)
	}
}

// screenSize is the size an app is told its screen is, in DIPs: the
// screen chosen, turned when rotated; fit takes the room given.
func screenSize(e *entry, rotated bool, room size) (w, h float32) {
	switch e.Screen {
	case 1:
		w, h = 600, 960
	case 2:
		w, h = room.w, room.h
	default:
		w, h = 360, 720
	}
	if rotated && e.Screen != 2 {
		w, h = h, w
	}
	return w, h
}

// appBody is an open app: on a soft ground, a phone-style frame around its
// surface (in a window of its own, the surface alone), and below it Back,
// Rotate and the screen size.
func (a *App) appBody(c *ui.Context, id string, own bool) {
	p := pal(c)
	s, e := a.open[id], a.entry(id)
	if s == nil || e == nil {
		return
	}
	if s.state.Phase == winapk.Failed {
		a.failedView(c, s, e, own)
		return
	}
	ui.Column(c.Key("stage-" + id)).Fill().Background(p.stage).Children(func() {
		area := ui.Row(c.Key("area-" + id)).Grow(1).FillWidth().MinHeight(0).Center().Gap(24)
		r := area.LayoutBounds()
		if own || e.Screen == 2 {
			a.tell(s, size{r.W, r.H})
		} else {
			w, h := screenSize(e, s.rotated, size{})
			a.tell(s, size{w, h})
		}
		showNotice := !own && e.Screen != 2 && r.W >= 760 && s.live() &&
			e.Native == winapk.NativeRestricted && !a.prefs.HideNativeNotice && !s.hideNotice
		area.Children(func() {
			if own || e.Screen == 2 {
				ui.Column(c.Key("surface-" + id)).Fill().Background(ui.Hex("#FFFFFF")).Children(func() {
					a.surface(c, s, e, r.W, r.H)
				})
				return
			}
			w, h := screenSize(e, s.rotated, size{})
			availW := r.W - 48
			if showNotice {
				availW -= 290
			}
			scale := min(1, availW/(w+20), (r.H-24)/(h+20))
			if scale <= 0 {
				scale = 0.5
			}
			dispW, dispH := w*scale, h*scale
			ui.Box(c.Key("frame-"+id)).Size(dispW+20*scale, dispH+20*scale).Padding(10*scale).Radius(30*scale).Background(ui.Hex("#0E1014")).Shadow(0, 12, 32, 0, ui.RGBA(0, 0, 0, 0.25)).Children(func() {
				ui.Column(c.Key("surface-" + id)).Fill().Radius(22 * scale).Clip().Background(ui.Hex("#FFFFFF")).Children(func() {
					a.surface(c, s, e, dispW, dispH)
				})
			})
			if showNotice {
				a.nativeNoticeCard(c, s, e)
			}
		})
		if own {
			return
		}
		ui.Row(c.Key("bar-"+id)).FillWidth().Padding(0, 12, 10, 12).Shrink(0).Center().Children(func() {
			ui.Row(c).Height(34).Padding(0, 8).Gap(4).AlignItems(ui.Center).Radius(17).Background(p.pane).Border(1, p.border).Children(func() {
				if iconButton(c, "back-"+id, icBack, L.Back()).Disabled(!s.live()).Clicked() {
					a.back(s)
				}
				if iconButton(c, "rot-"+id, icRotate, L.Rotate()).Disabled(e.Screen == 2).Clicked() {
					a.rotate(s, e)
				}
				ui.Box(c).Size(1, 18).Background(p.border).Margin(0, 2)
				opts := []string{L.ScreenPhone(), L.ScreenTablet(), L.ScreenFit()}
				cur := opts[min(max(e.Screen, 0), 2)]
				if ui.Select(c.Key("scr-"+id), &cur, opts).Label(L.ScreenSize()).MinWidth(180).Changed() {
					for i, o := range opts {
						if o == cur {
							e.Screen = i
						}
					}
					a.saveLibrary()
				}
			})
		})
	})
}

// nativeNoticeCard is the dismissible experimental native-code reminder shown
// beside the phone in a wide pane (design/LibraryDark.html).
func (a *App) nativeNoticeCard(c *ui.Context, s *session, e *entry) {
	p := pal(c)
	prof := string(a.profile(e))
	ui.Column(c.Key("nat-note-"+e.ID)).Width(260).Padding(12, 14).Gap(8).Radius(radiusCard).Background(p.warnBg).Border(1, p.warnBorder).Children(func() {
		ui.Row(c).Gap(8).AlignItems(ui.Center).Children(func() {
			ui.Icon(c, icChip).FontSize(16).TextColor(p.warn)
			ui.Text(c, L.NativeNoticeTitle(prof)).FontSize(13).FontWeight(600).TextColor(p.warn).Grow(1).Wrap()
		})
		ui.Text(c, L.NativeNoticeBody(prof)).FontSize(12).TextColor(p.text).Wrap()
		ui.Row(c).Gap(8).Children(func() {
			if button(c, "nat-hide-"+e.ID, L.Hide(), nil).Clicked() {
				s.hideNotice = true
			}
			if link(c, "nat-dont-"+e.ID, L.DontAskAgain()).Clicked() {
				pr := a.prefs
				pr.HideNativeNotice = true
				a.setPrefs(pr)
			}
		})
	})
}

// back sends the app Back.
func (a *App) back(s *session) {
	if proc := s.proc; proc != nil {
		a.async(func() { _ = proc.Back() })
	}
}

// rotate turns the app's screen; a window of its own turns with it.
func (a *App) rotate(s *session, e *entry) {
	s.rotated = !s.rotated
	if s.win != nil && e.Screen != 2 {
		w, h := ownSize(e, s.rotated)
		s.win.SetContentSize(w, h)
	}
}

// tell tells a running app its screen's size when it differs from what it
// was told last.
func (a *App) tell(s *session, sz size) {
	if s.proc == nil || !s.live() || sz.w <= 0 || sz.h <= 0 || sz == s.told {
		return
	}
	s.told = sz
	proc := s.proc
	a.async(func() { _ = proc.Resize(int(sz.w), int(sz.h)) })
}

// ownSize is the content size of an app's window of its own: its screen
// and the header above it.
func ownSize(e *entry, rotated bool) (w, h int) {
	sw, sh := screenSize(e, rotated, size{360, 720})
	return int(sw), int(sh) + headerH
}

// surface draws the app's live RGBA8 bitmap (forwarding pointer, wheel, key
// and IME events) or its non-running state.
func (a *App) surface(c *ui.Context, s *session, e *entry, dispW, dispH float32) {
	dark := ui.Hex("#1B1D21")
	grey := ui.Hex("#4A5260")
	notResp := !s.stopAt.IsZero() && time.Since(s.stopAt) > stopWait
	if s.state.Phase == winapk.Running && !notResp && s.frame != nil {
		img := ui.Image(c.Key("bmp-"+e.ID), s.frame).Fill().Fit(ui.Contain).Focusable()
		img.HandleInput(func(ev ui.InputEvent) bool {
			return a.forwardInput(s, ev, dispW, dispH)
		})
		return
	}
	ui.Column(c).Fill().Center().Gap(10).Padding(24).Children(func() {
		switch s.state.Phase {
		case winapk.Starting:
			ui.Spinner(c)
			ui.Text(c, L.Starting(e.APK.Label)).TextColor(dark)
		case winapk.Stopped:
			ui.Text(c, L.AppStopped(e.APK.Label)).FontWeight(600).TextColor(dark)
			if primary(c, "reopen-"+e.ID, L.OpenAgain(), nil).Clicked() {
				inWin := s.win != nil
				delete(a.open, e.ID)
				if inWin {
					w := s.win
					s.win = nil
					a.launch(e, true, false)
					w.Close()
				} else {
					a.launch(e, false, false)
				}
			}
		default:
			if notResp {
				ui.Text(c, L.NotRespondingBody(e.APK.Label)).TextColor(dark).Wrap().MaxWidth(280)
				if button(c, "kill-"+e.ID, L.ForceStop(), nil).Clicked() {
					a.stopApp(e.ID, true)
				}
				return
			}
			appIcon(c, e, 56)
			ui.Text(c, e.APK.Label).FontSize(18).FontWeight(600).TextColor(dark)
			if a.report.Demo {
				ui.Text(c, L.SampleSurface()).FontSize(13).TextColor(grey).Wrap().MaxWidth(260).TextAlign(ui.Center)
			}
		}
	})
}

// forwardInput translates MyGo surface events into Android DIP coordinates,
// virtual keys and IME text on the runtime's duplex control pipe.
func (a *App) forwardInput(s *session, ev ui.InputEvent, dispW, dispH float32) bool {
	proc := s.proc
	if proc == nil || !s.live() {
		return false
	}
	mapPt := func(x, y float32) (float32, float32) {
		tw, th := s.told.w, s.told.h
		if tw <= 0 || th <= 0 || dispW <= 0 || dispH <= 0 {
			return x, y
		}
		return x * tw / dispW, y * th / dispH
	}
	switch ev.Kind {
	case ui.InputPointerDown:
		if ev.Button != 0 {
			return false
		}
		s.pressing = true
		x, y := mapPt(ev.X, ev.Y)
		proc.SendPointer(winapk.PointerDown, x, y)
		return true
	case ui.InputPointerMove:
		if !s.pressing {
			return false
		}
		x, y := mapPt(ev.X, ev.Y)
		proc.SendPointer(winapk.PointerMove, x, y)
		return true
	case ui.InputPointerUp:
		if !s.pressing {
			return false
		}
		s.pressing = false
		x, y := mapPt(ev.X, ev.Y)
		act := winapk.PointerUp
		if ev.Canceled {
			act = winapk.PointerCancel
		}
		proc.SendPointer(act, x, y)
		return true
	case ui.InputScroll:
		x, y := mapPt(ev.X, ev.Y)
		// MyGo positive DY moves down content; Android wheel notch positive scrolls up.
		proc.SendWheel(x, y, -ev.DX/40, -ev.DY/40)
		return true
	case ui.InputText:
		if ev.Text != "" {
			proc.SendText(ev.Text)
			return true
		}
	case ui.InputKeyDown, ui.InputKeyUp:
		vk, navOnly := winVK(ev.Key)
		if vk == 0 {
			return false
		}
		// Printable keys without Ctrl/Alt arrive as InputText so IME works without double insertion.
		if !navOnly && ev.Mods&(ui.Ctrl|ui.Alt) == 0 {
			return false
		}
		meta := 0
		if ev.Mods&ui.Shift != 0 {
			meta |= 0x1
		}
		if ev.Mods&ui.Ctrl != 0 {
			meta |= 0x1000
		}
		proc.SendKey(ev.Kind == ui.InputKeyDown, vk, 0, meta)
		return true
	}
	return false
}

// winVK maps ui.Key to a Win32 virtual-key code and reports whether the key is
// non-printable navigation/editing rather than ordinary text.
func winVK(k ui.Key) (vk int, navOnly bool) {
	switch k {
	case ui.KeyEnter:
		return 0x0D, true
	case ui.KeyBackspace:
		return 0x08, true
	case ui.KeyDelete:
		return 0x2E, true
	case ui.KeyTab:
		return 0x09, true
	case ui.KeyUp:
		return 0x26, true
	case ui.KeyDown:
		return 0x28, true
	case ui.KeyLeft:
		return 0x25, true
	case ui.KeyRight:
		return 0x27, true
	case ui.KeyHome:
		return 0x24, true
	case ui.KeyEnd:
		return 0x23, true
	case ui.KeySpace:
		return 0x20, false
	}
	if k >= ui.KeyA && k <= ui.KeyZ {
		return int('A') + int(k-ui.KeyA), false
	}
	if k >= ui.Key0 && k <= ui.Key9 {
		return int('0') + int(k-ui.Key0), false
	}
	return 0, false
}

// failedView is an app that couldn't start, in plain words; the category
// shows only in Diagnostics (design/StartFailed.html).
func (a *App) failedView(c *ui.Context, s *session, e *entry, own bool) {
	p := pal(c)
	ui.Column(c.Key("failed-"+e.ID)).Fill().Background(p.stage).AlignItems(ui.Center).Padding(64, 24, 20, 24).Children(func() {
		ui.Column(c).MaxWidth(420).AlignItems(ui.Center).Gap(12).Children(func() {
			ui.Box(c).Size(56, 56).Shrink(0).Radius(28).Background(p.dangerBg).Center().Children(func() {
				ui.Icon(c, icCircleX).FontSize(22).TextColor(p.danger)
			})
			ui.Text(c, L.FailedTitle(e.APK.Label)).FontSize(20).FontWeight(600).TextAlign(ui.Center).Wrap()
			muted(c, failedWords(s.state.Category)+" "+L.FailedSafe(), fontBody).TextAlign(ui.Center).Wrap()
			ui.Row(c).Gap(8).Margin(6, 0, 0, 0).Children(func() {
				if primary(c, "retry-"+e.ID, L.TryAgain(), nil).Clicked() {
					delete(a.open, e.ID)
					if own && s.win != nil {
						w := s.win
						s.win = nil
						a.launch(e, true, false)
						w.Close()
					} else {
						a.removeTab(appTab(e.ID))
						a.launch(e, false, false)
					}
				}
				if button(c, "faillog-"+e.ID, L.ShowDetails(), nil).Clicked() {
					a.diag.page, a.diag.app = 1, e.ID
					a.showTab(tabDiag, false)
					if own {
						a.Focus()
					}
				}
			})
		})
		if e.Native == winapk.NativeRestricted {
			ui.Row(c).Width(420).MaxWidthPercent(100).Margin(28, 0, 0, 0).Padding(14, 16).Gap(12).Radius(radiusCard).Background(p.pane).Border(1, p.border).Children(func() {
				ui.Icon(c, icInfo).FontSize(18).TextColor(p.muted)
				ui.Column(c).Grow(1).MinWidth(0).Gap(6).Children(func() {
					ui.Text(c, L.TryNativeOff()).FontWeight(600)
					muted(c, L.TryNativeOffHint(), 13).Wrap()
					ui.Row(c).Gap(8).Children(func() {
						if a.profile(e) != winapk.N2 {
							if primary(c, "failn2-"+e.ID, L.SwitchProfileN2Retry(), nil).Clicked() {
								e.Profile = winapk.N2
								a.saveLibrary()
								a.launch(e, own, false)
							}
						}
						if button(c, "failset-"+e.ID, L.OpenItsSettings(), nil).Clicked() {
							a.openDetails(e.ID)
							if own {
								a.Focus()
							}
						}
					})
				})
			})
		} else if s.state.Category == "GUEST_NATIVE_DISABLED" && len(e.APK.NativeLibs) > 0 && e.APK.Verdict != winapk.Unsupported {
			ui.Row(c).Width(420).MaxWidthPercent(100).Margin(28, 0, 0, 0).Padding(14, 16).Gap(12).Radius(radiusCard).Background(p.pane).Border(1, p.border).Children(func() {
				ui.Icon(c, icChip).FontSize(18).TextColor(p.accentText)
				ui.Column(c).Grow(1).MinWidth(0).Gap(6).Children(func() {
					ui.Text(c, L.TryNativeRestricted()).FontWeight(600)
					muted(c, L.TryNativeRestrictedHint(), 13).Wrap()
					ui.Row(c).Gap(8).Children(func() {
						if primary(c, "failn2-"+e.ID, L.SwitchProfileN2Retry(), nil).Clicked() {
							e.Native = winapk.NativeRestricted
							e.Profile = maxProfile(winapk.N2, e.APK.Profile)
							a.saveLibrary()
							a.launch(e, own, false)
						}
						if button(c, "failset-"+e.ID, L.OpenItsSettings(), nil).Clicked() {
							a.openDetails(e.ID)
							if own {
								a.Focus()
							}
						}
					})
				})
			})
		}
		ui.Spacer(c)
		if !own {
			muted(c, L.OtherAppsRunning(), fontSmall)
		}
	})
}

// failedWords says what a failure's category means for the person.
func failedWords(category string) string {
	switch category {
	case "GUEST_NATIVE_DISABLED":
		return L.FailedNativeDisabled()
	case "GUEST_SYMBOL_UNRESOLVED", "ELF_RELOCATION_UNSUPPORTED", "GUEST_ABI_UNSUPPORTED":
		return L.FailedNative()
	case "GRAPHICS_CAPABILITY_MISSING":
		return L.FailedGraphics()
	case "APK_UNSUPPORTED", "HOST_ARCH_UNSUPPORTED":
		return L.FailedUnsupported()
	case "DEPENDENCY_MISSING":
		return L.FailedMissing()
	}
	return L.FailedOther()
}

// newAppWindow shows an app in a window of its own: one app, no sidebar,
// never split; closing it stops the app.
func (a *App) newAppWindow(s *session) {
	e := a.entry(s.id)
	if e == nil {
		return
	}
	id := s.id
	cw, ch := ownSize(e, s.rotated)
	w := mygo.NewWindow(mygo.WindowOptions{
		Title: e.APK.Label, Width: cw, Height: ch, UseContentSize: true, MinWidth: paneMinW, MinHeight: paneMinH,
		TitleBarStyle: mygo.TitleBarHidden, HideWindowButtons: true,
		Content: ui.View(func(c *ui.Context) { a.appWindowView(c, id) }),
	})
	w.OnClose(func(*mygo.CloseEvent) {
		if cur := a.open[id]; cur != nil && cur.win == w {
			cur.win = nil
			a.stopApp(id, false)
			delete(a.open, id)
		}
	})
	if len(e.APK.Icon) == 0 || w.SetIcon(e.APK.Icon) != nil {
		_ = w.SetIcon(logoPNG)
	}
	s.win = w
	w.Show()
	w.Focus()
}

// appWindowView is an app's window of its own: a header with its icon and
// name, Back, Rotate, Back to the main window, ⋯ and the window's buttons;
// its surface fills the rest.
func (a *App) appWindowView(c *ui.Context, id string) {
	applyTheme(c)
	p := pal(c)
	s, e := a.open[id], a.entry(id)
	ui.Column(c.Key("own")).Fill().Background(p.pane).TextColor(p.text).FontSize(fontBody).Children(func() {
		if s == nil || e == nil {
			return
		}
		ui.Row(c.Key("own-head")).Height(headerH).Shrink(0).FillWidth().Padding(0, 6, 0, 12).Gap(4).AlignItems(ui.Center).BorderWidth(0, 0, 1, 0).BorderColor(p.border).Children(func() {
			appIcon(c, e, 22)
			ui.Text(c, e.APK.Label).FontWeight(600).NoWrap().Margin(0, 0, 0, 4)
			ui.Box(c).Grow(1).FillHeight().DragWindow()
			if iconButton(c, "own-back", icBack, L.Back()).Disabled(!s.live()).Clicked() {
				a.back(s)
			}
			if iconButton(c, "own-rot", icRotate, L.Rotate()).Clicked() {
				a.rotate(s, e)
			}
			if iconButton(c, "own-pane", icDockIn, L.BackToMain()).Tooltip(L.BackToMainHint()).Clicked() {
				a.toPane(id)
			}
			iconButton(c, "own-more", icMore, L.More()).Menu(func(m *ui.Menu) { a.appMenuItems(m, e) })
			windowButtons(c, "own-win", s.win)
		})
		a.appBody(c, id, true)
	})
	if c.Shortcut(0, ui.KeyEscape) && s != nil && s.live() {
		a.back(s)
	}
}
