package gui

import (
	"context"
	"fmt"
	"strings"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// homeView is Home: Apps with Check this PC and Add APK…, search, All or
// Open, then one row per app; empty, a place to drop an APK and what runs
// on this PC today. A dropped APK opens Add an app.
func (a *App) homeView(c *ui.Context) {
	if a.details.open {
		a.detailsView(c)
		return
	}
	p := pal(c)
	body := ui.Column(c.Key("home")).Fill()
	if apk := firstAPK(body.DroppedFiles()); apk != "" {
		a.inspect(apk)
	}
	over := body.FileDragOver()
	body.Children(func() {
		ui.Row(c).FillWidth().Padding(18, 24, 10, 24).Gap(8).Shrink(0).Children(func() {
			ui.Text(c, L.HomeApps()).FontSize(fontTitle).FontWeight(600).Grow(1).Level(1)
			if button(c, "home-check", L.CheckThisPC(), nil).Clicked() {
				a.showTab(tabDiag, false)
				a.check()
			}
			if primary(c, "home-add", L.AddAPK(), icPlus).Clicked() {
				a.chooseAPK()
			}
		})
		if len(a.library) == 0 {
			a.emptyHome(c, over)
			return
		}
		ui.Row(c).FillWidth().Padding(0, 24, 10, 24).Gap(10).Shrink(0).Children(func() {
			ui.SearchField(c.Key("home-search"), &a.homeQuery).Placeholder(L.SearchApps()).Width(260)
			ui.Segmented(c.Key("home-filter"), &a.homeFilter, L.FilterAll(), L.FilterOpen()).Label(L.FilterLabel())
			ui.Spacer(c)
			muted(c, L.AppsCount(len(a.library)), 13)
		})
		list := ui.Scroll(c.Key("home-list")).Grow(1).FillWidth().Padding(0, 24, 16, 24).Gap(0)
		if over {
			list.Background(p.tint.Alpha(0.5))
		}
		list.Children(func() {
			ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
				n := 0
				q := strings.ToLower(strings.TrimSpace(a.homeQuery))
				for _, e := range a.library {
					s := a.open[e.ID]
					if a.homeFilter == 1 && (s == nil || !s.live()) {
						continue
					}
					if q != "" && !strings.Contains(strings.ToLower(e.APK.Label+" "+e.APK.Package), q) {
						continue
					}
					a.appRow(c, e, n > 0)
					n++
				}
				if n == 0 {
					muted(c, L.NoAppsFound(), 13).Padding(16)
				}
			})
			muted(c, L.HomeFoot(), 13).Margin(12, 0, 0, 0).Wrap()
		})
	})
}

// fileExt is a path's extension with its dot.
func fileExt(path string) string {
	if i := strings.LastIndexByte(path, '.'); i >= 0 {
		return path[i:]
	}
	return ""
}

// emptyHome is Home with no apps: drop or choose an APK, and what runs on
// this PC today (design/LibraryEmpty.html).
func (a *App) emptyHome(c *ui.Context, over bool) {
	p := pal(c)
	ui.Scroll(c.Key("home-empty")).Grow(1).FillWidth().Padding(8, 24, 24, 24).Gap(20).Children(func() {
		drop := ui.Column(c).FillWidth().Padding(36, 24).Gap(10).AlignItems(ui.Center).Radius(radiusCard).Border(2, p.border).BorderStyle(ui.BorderDashed)
		if over {
			drop.Background(p.tint).BorderColor(p.accent)
		}
		drop.Children(func() {
			tile(c, icApk, 44, p.tint, p.accentText)
			ui.Text(c, L.EmptyTitle()).FontSize(18).FontWeight(600)
			muted(c, L.EmptyBody(), fontBody).Wrap().MaxWidth(480).TextAlign(ui.Center)
			if primary(c, "empty-choose", L.ChooseAPK(), nil).Clicked() {
				a.chooseAPK()
			}
		})
		sectionTitle(c, L.RunsToday())
		ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
			for i, f := range a.todayRows() {
				ui.Row(c).FillWidth().Gap(14).Padding(12, 16).BorderWidth(min(float32(i), 1), 0, 0, 0).BorderColor(p.hover).Children(func() {
					tile(c, f.icon, 32, p.hover, p.text)
					ui.Column(c).Grow(1).MinWidth(0).Children(func() {
						ui.Text(c, f.title).FontWeight(600)
						muted(c, f.hint, 13).Wrap()
					})
					chip(c, f.word, f.tone)
				})
			}
		})
		muted(c, L.TrustNote(), 13).Wrap()
		if link(c, "empty-diag", L.SeeFullCheck()).Clicked() {
			a.showTab(tabDiag, false)
		}
	})
}

// todayRow is a line of What runs on this PC today.
type todayRow struct {
	icon              *ui.SVG
	title, hint, word string
	tone              tone
}

// todayRows put doctor's features into the design's four lines.
func (a *App) todayRows() []todayRow {
	lv := func(id string) winapk.Level {
		for _, f := range a.report.Features {
			if f.ID == id {
				return f.Level
			}
		}
		return winapk.Unavailable
	}
	word := func(l winapk.Level) (string, tone) {
		switch l {
		case winapk.Works:
			return L.LevelWorks(), toneGood
		case winapk.Experimental:
			return L.LevelExperimental(), toneWarn
		}
		return L.LevelNotYet(), toneNeutral
	}
	java := winapk.Unavailable
	if a.report.Ready() {
		java = lv("java")
	}
	native := lv("native-n0")
	rows := []todayRow{
		{icJava, L.TodayJava(), L.TodayJavaHint(), "", 0},
		{icChip, L.TodayNative(), L.TodayNativeHint(), "", 0},
		{icCube, L.TodayGLES(), L.TodayGLESHint(), "", 0},
		{icMonitor, L.TodayVulkan(), L.TodayVulkanHint(), "", 0},
	}
	for i, l := range []winapk.Level{java, native, lv("gles"), lv("vulkan")} {
		rows[i].word, rows[i].tone = word(l)
	}
	return rows
}

// appRow is an app on Home: its icon and label, what it is and how it
// opens, a chip where something is limited, its state, and Open with ▾ —
// Details where it can't run here. Right-click opens its menu.
func (a *App) appRow(c *ui.Context, e *entry, border bool) {
	p := pal(c)
	s := a.open[e.ID]
	live := s != nil && s.live()
	row := ui.Row(c.Key("row-"+e.ID)).FillWidth().Gap(14).Padding(12, 16).AlignItems(ui.Center)
	if border {
		row.BorderWidth(1, 0, 0, 0).BorderColor(p.hover)
	}
	if row.Hovered() {
		row.Background(p.subtle)
	}
	row.ContextMenu(func(m *ui.Menu) { a.rowMenu(m, e) })
	row.Children(func() {
		appIcon(c, e, 36)
		ui.Column(c).Grow(1).MinWidth(0).Gap(2).Children(func() {
			ui.Text(c, e.APK.Label).FontWeight(600).Ellipsis("…").MaxLines(1)
			muted(c, a.appLine(e), 13).Ellipsis("…").MaxLines(1)
		})
		if word, t, ok := a.appChip(e); ok {
			chip(c, word, t)
		}
		state := L.StateStopped()
		if s != nil {
			state = stateWords(s, false, e, a)
		}
		ui.Row(c).Gap(6).AlignItems(ui.Center).MinWidth(100).Shrink(0).Children(func() {
			if live {
				dot(c, p.success)
				ui.Text(c, state).FontSize(13).TextColor(p.success).NoWrap()
			} else if s != nil && s.state.Phase == winapk.Failed {
				ui.Text(c, state).FontSize(13).TextColor(p.danger).NoWrap()
			} else {
				muted(c, state, 13).NoWrap()
			}
		})
		switch {
		case a.missing[e.APK.Path] && !live:
			if button(c, "locate-"+e.ID, L.Locate(), nil).Clicked() {
				a.locate(e)
			}
		case !e.runnable():
			if button(c, "details-"+e.ID, L.Details(), nil).Clicked() {
				a.openDetails(e.ID)
			}
		case live:
			a.showButton(c, e)
		default:
			a.openButton(c, e)
		}
	})
}

// showButton is the split button for an already-open app: Show and ▾.
func (a *App) showButton(c *ui.Context, e *entry) {
	p := pal(c)
	ui.Row(c).Shrink(0).Children(func() {
		b := ui.ButtonBase(c.Key("show-"+e.ID)).Height(32).Padding(0, 12).Radius(radiusCtl).Border(1, p.border).Background(p.pane).Label(L.Show())
		if b.Hovered() {
			b.Background(p.hover)
		}
		b.Children(func() { ui.Text(c, L.Show()).TextColor(p.text).NoWrap() })
		if b.Clicked() {
			a.reveal(e.ID)
		}
		m := ui.ButtonBase(c.Key("show-more-"+e.ID)).Size(28, 32).Radius(radiusCtl).Margin(0, 0, 0, 1).Border(1, p.border).Background(p.pane).Center().Label(L.More())
		if m.Hovered() {
			m.Background(p.hover)
		}
		m.Children(func() { ui.Icon(c, icChevron).FontSize(14).TextColor(p.text) })
		m.Menu(func(m *ui.Menu) { a.rowMenu(m, e) })
	})
}

// openButton is the split button: Open (the app's default way) and ▾,
// which offers the other ways once.
func (a *App) openButton(c *ui.Context, e *entry) {
	p := pal(c)
	ready := a.report.Ready()
	ui.Row(c).Shrink(0).Children(func() {
		label := L.OpenAsPane()
		if e.InWindow {
			label = L.OpenInWindow()
		}
		b := ui.ButtonBase(c.Key("open-"+e.ID)).Height(32).Padding(0, 12).Radius(radiusCtl).Background(p.accent).Label(label).Disabled(!ready)
		if !ready {
			b.Opacity(0.45).Tooltip(L.OpenNeedsJava())
		}
		b.Children(func() {
			ui.Row(c).Gap(6).AlignItems(ui.Center).Children(func() {
				ui.Icon(c, icPlay).FontSize(12).TextColor(p.onAccent)
				ui.Text(c, L.Open()).FontWeight(600).TextColor(p.onAccent).NoWrap()
			})
		})
		if b.Clicked() {
			a.launch(e, e.InWindow, false)
		}
		m := ui.ButtonBase(c.Key("open-more-"+e.ID)).Size(28, 32).Radius(radiusCtl).Margin(0, 0, 0, 1).Background(p.accent).Center().Label(L.OpenOtherWays()).Disabled(!ready)
		if !ready {
			m.Opacity(0.45)
		}
		m.Children(func() { ui.Icon(c, icChevron).FontSize(14).TextColor(p.onAccent) })
		m.Menu(func(m *ui.Menu) { a.openItems(m, e) })
	})
}

// openItems are the ways to open an app just this once, then Change how
// it opens… (design/OpenMenu.html).
func (a *App) openItems(m *ui.Menu, e *entry) {
	p1 := m.Item(L.OpenAsPane()).Icon(miPane).Subtitle(L.OpenAsPaneSub())
	if !e.InWindow {
		p1.Trailing(L.Default())
	}
	if p1.Chosen() {
		a.launch(e, false, false)
	}
	if m.Item(L.OpenBeside()).Icon(miSplitR).Subtitle(L.OpenBesideSub()).Chosen() {
		a.launch(e, false, true)
	}
	p2 := m.Item(L.OpenInWindow()).Icon(miWindow).Subtitle(L.OpenInWindowSub())
	if e.InWindow {
		p2.Trailing(L.Default())
	}
	if p2.Chosen() {
		a.launch(e, true, false)
	}
	m.Separator()
	if m.Item(L.ChangeHowOpens(e.APK.Label)).Icon(miGear).Chosen() {
		a.openDetails(e.ID)
	}
}

// rowMenu is an app's menu on Home (design/ContextMenu.html).
func (a *App) rowMenu(m *ui.Menu, e *entry) {
	s := a.open[e.ID]
	switch {
	case s != nil && s.live() && s.win != nil:
		if m.Item(L.ShowWindow()).Icon(miWindow).Trailing("Enter").Chosen() {
			a.reveal(e.ID)
		}
		if m.Item(L.BackToMain()).Icon(miDockIn).Trailing(L.AsPane()).Chosen() {
			a.toPane(e.ID)
		}
		if m.Item(L.OpenBeside()).Icon(miSplitR).Chosen() {
			a.toPane(e.ID)
			if a.cur != nil {
				a.splitWith(a.cur.focus, appTab(e.ID), true)
			}
		}
	case s != nil && s.live():
		if m.Item(L.Show()).Icon(miPane).Trailing("Enter").Chosen() {
			a.reveal(e.ID)
		}
		if m.Item(L.OpenInOwnWindow()).Icon(miWindow).Chosen() {
			a.toWindow(e.ID)
		}
	case e.runnable():
		ready := a.report.Ready()
		if m.Item(L.OpenAsPane()).Icon(miPane).Disabled(!ready).Chosen() {
			a.launch(e, false, false)
		}
		if m.Item(L.OpenBeside()).Icon(miSplitR).Disabled(!ready).Chosen() {
			a.launch(e, false, true)
		}
		if m.Item(L.OpenInWindow()).Icon(miWindow).Disabled(!ready).Chosen() {
			a.launch(e, true, false)
		}
	}
	if s != nil && s.live() {
		m.Separator()
		if m.Item(L.Stop()).Icon(miStop).Chosen() {
			a.stopApp(e.ID, false)
		}
	}
	m.Separator()
	a.appMenuItems(m, e)
	if !strings.HasPrefix(e.APK.Path, "sample:") && !a.missing[e.APK.Path] {
		if m.Item(L.OpenFolder()).Icon(miFolder).Chosen() {
			a.async(func() { mygo.Shell.ShowItemInFolder(e.APK.Path) })
		}
	}
	m.Separator()
	if m.Item(L.RemoveFromLibrary()).Icon(miTrash).Danger().Chosen() {
		a.confirm = confirmState{open: true, kind: confirmRemove, id: e.ID}
	}
}

// appLine says what an app is and how it opens.
func (a *App) appLine(e *entry) string {
	switch e.APK.Verdict {
	case winapk.NotYet:
		return L.LineNeedsVulkan()
	case winapk.Unsupported:
		return L.LineOnly32()
	}
	what := L.JavaOnly()
	if len(e.APK.NativeLibs) > 0 {
		what = L.JavaAndNative()
	}
	if e.APK.GLES {
		what += " · OpenGL ES"
	}
	how := L.OpensAsPane()
	if e.InWindow {
		how = L.OpensInWindow()
	}
	return what + " · " + how
}

// appChip is the chip of an app whose running is limited here.
func (a *App) appChip(e *entry) (string, tone, bool) {
	switch {
	case a.missing[e.APK.Path]:
		return L.FileMissing(), toneBad, true
	case e.APK.Verdict == winapk.NotYet:
		return L.CantRunYet(), toneWarn, true
	case e.APK.Verdict == winapk.Unsupported:
		return L.NotSupported(), toneBad, true
	case len(e.APK.NativeLibs) == 0:
		return "", 0, false
	case e.Native == winapk.NativeRestricted:
		return L.NativeCodeProfile(string(a.profile(e))), toneWarn, true
	}
	return L.NativeOffChip(), toneNeutral, true
}

// installState is getting Windows-ARM64.apk's own Java runtime.
type installState struct {
	running, shown bool
	step           winapk.InstallStep
	frac           float64
	failed         bool
	cancel         context.CancelFunc
}

// javaCard is the card above the panes while no Java runtime is found:
// what Windows-ARM64.apk would get and where, or how far getting it is.
func (a *App) javaCard(c *ui.Context) {
	if a.checked.IsZero() || a.report.Ready() {
		return
	}
	p := pal(c)
	ui.Column(c.Key("java-card")).FillWidth().Shrink(0).Padding(0, 0, margin, 0).Children(func() {
		ui.Row(c).FillWidth().Gap(14).Padding(14, 18).Radius(radiusCard).Background(p.tint).Border(1, p.tintBorder).AlignItems(ui.Center).Children(func() {
			tile(c, icJava, 36, p.pane, p.accentText)
			ui.Column(c).Grow(1).MinWidth(0).Gap(4).Children(func() {
				ui.Text(c, L.JavaCardTitle()).FontWeight(600).TextColor(p.accentText)
				if a.install.running {
					muted(c, installWords(a.install.step), 13)
					ui.Progress(c, a.installFrac()).FillWidth().MaxWidth(420)
					return
				}
				muted(c, L.JavaCardBody(), 13).Wrap()
			})
			if a.install.running {
				if button(c, "java-show", L.ShowProgress(), nil).Clicked() {
					a.install.shown = true
				}
				return
			}
			if button(c, "java-folder", L.UseFolder(), nil).Clicked() {
				a.chooseJavaFolder()
			}
			if primary(c, "java-get", L.GetJava(), icDownload).Clicked() {
				a.installJava()
			}
		})
	})
}

// installFrac is the whole of getting the runtime as one fraction: the
// download is most of it.
func (a *App) installFrac() float64 {
	switch a.install.step {
	case winapk.StepDownload:
		return 0.7 * a.install.frac
	case winapk.StepCheck:
		return 0.75
	case winapk.StepInstall:
		return 0.85
	case winapk.StepTest:
		return 0.95
	}
	return 1
}

// installWords says the step getting the runtime is at.
func installWords(s winapk.InstallStep) string {
	switch s {
	case winapk.StepCheck:
		return L.StepCheck()
	case winapk.StepInstall:
		return L.StepInstall()
	case winapk.StepTest:
		return L.StepTest()
	case winapk.StepDone:
		return L.StepDone()
	}
	return L.StepDownload()
}

// installJava gets Windows-ARM64.apk's own Java runtime, showing the
// steps; doctor checks again after it.
func (a *App) installJava() {
	if a.install.running {
		return
	}
	ctx, cancel := context.WithCancel(a.ctx)
	a.install = installState{running: true, shown: true, cancel: cancel}
	a.async(func() {
		err := a.rt.InstallJava(ctx, javaDir(), func(s winapk.InstallStep, f float64) {
			a.later(func() { a.install.step, a.install.frac = s, f })
		})
		a.later(func() {
			cancel()
			a.install.running = false
			if err != nil && ctx.Err() == nil {
				a.install.failed = true
				a.setToast(L.JavaFailed())
				return
			}
			a.install.shown = false
			pr := a.prefs
			pr.JavaHome = ""
			a.setPrefs(pr)
			a.check()
		})
	})
}

// installDialog shows getting the runtime step by step (design/
// RuntimeInstall.html); Hide leaves the card's progress, Cancel stops it.
func (a *App) installDialog(c *ui.Context) {
	if !a.install.shown || !a.install.running {
		return
	}
	p := pal(c)
	ui.DialogBase(c.Key("install"), &a.install.shown, func(backdrop, panel ui.Element) {
		backdrop.Background(p.scrim)
		panel.Width(520).Radius(radiusDlg).Background(p.pane).Clip().Label(L.GettingJava()).Children(func() {
			ui.Row(c).FillWidth().Height(52).Padding(0, 12, 0, 20).AlignItems(ui.Center).BorderWidth(0, 0, 1, 0).BorderColor(p.border).Children(func() {
				ui.Text(c, L.GettingJava()).FontSize(16).FontWeight(600).Grow(1).Level(1)
				if iconButton(c, "install-x", icClose, L.Hide()).Clicked() {
					a.install.shown = false
				}
			})
			ui.Column(c).FillWidth().Padding(20, 24).Gap(14).Children(func() {
				ui.Row(c).FillWidth().AlignItems(ui.Center).Children(func() {
					ui.Text(c, installWords(a.install.step)).FontWeight(600).Grow(1)
					if a.install.step == winapk.StepDownload {
						const estMB = 198
						done := int(a.install.frac * estMB)
						muted(c, L.DownloadMB(fmt.Sprintf("%d", done), fmt.Sprintf("%d", estMB)), fontSmall)
					}
				})
				ui.Progress(c, a.installFrac()).FillWidth()
				steps := []struct {
					s           winapk.InstallStep
					title, hint string
				}{
					{winapk.StepDownload, L.StepDownloadTitle(), L.StepDownloadHint()},
					{winapk.StepCheck, L.StepCheckTitle(), L.StepCheckHint()},
					{winapk.StepInstall, L.StepInstallTitle(), javaDir()},
					{winapk.StepTest, L.StepTestTitle(), ""},
				}
				ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
					for i, st := range steps {
						ui.Row(c).FillWidth().Gap(12).Padding(10, 14).BorderWidth(min(float32(i), 1), 0, 0, 0).BorderColor(p.hover).Children(func() {
							ui.Column(c).Grow(1).MinWidth(0).Children(func() {
								ui.Text(c, st.title).FontWeight(600)
								if st.hint != "" {
									muted(c, st.hint, fontSmall).Wrap()
								}
							})
							switch {
							case st.s < a.install.step:
								chip(c, L.StepDoneChip(), toneGood)
							case st.s == a.install.step:
								chip(c, L.StepNow(), toneAccent)
							default:
								chip(c, L.StepNext(), toneNeutral)
							}
						})
					}
				})
				muted(c, L.JavaPrivate(), 13).Wrap()
			})
			ui.Row(c).FillWidth().Padding(12, 24).Gap(8).Background(p.subtle).BorderWidth(1, 0, 0, 0).BorderColor(p.border).Children(func() {
				ui.Spacer(c)
				if button(c, "install-cancel", L.Cancel(), nil).Clicked() && a.install.cancel != nil {
					a.install.cancel()
					a.install.shown = false
				}
				if primary(c, "install-hide", L.Hide(), nil).Clicked() {
					a.install.shown = false
				}
			})
		})
	})
}
