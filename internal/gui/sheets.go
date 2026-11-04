package gui

import (
	"path/filepath"
	"strings"

	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// addState is Add an app while open: the APK being read, then what it
// holds and how it will open.
type addState struct {
	open, reading bool
	path          string
	apk           winapk.APK
	failed        bool
	inWindow      int // 0 as a pane, 1 in a window of its own
}

// inspect reads an APK with the runtime and opens Add an app with it.
// Nothing in the file runs.
func (a *App) inspect(path string) {
	win := 0
	if a.prefs.OpenInWindow {
		win = 1
	}
	a.add = addState{open: true, reading: true, path: path, inWindow: win}
	a.async(func() {
		apk, err := a.rt.Inspect(a.ctx, path)
		a.later(func() {
			if a.add.path != path {
				return
			}
			a.add.reading, a.add.apk, a.add.failed = false, apk, err != nil
		})
	})
}

// sheet is a dialog over the dimmed window with a title and a close
// button; body fills it.
func sheet(c *ui.Context, key string, open *bool, title string, width float32, body func()) {
	p := pal(c)
	ui.DialogBase(c.Key(key), open, func(backdrop, panel ui.Element) {
		backdrop.Background(p.scrim)
		panel.Width(width).MaxWidthPercent(92).MaxHeightPercent(90).Radius(radiusDlg).Background(p.pane).Clip().Shadow(0, 18, 48, 0, ui.RGBA(10, 16, 24, 0.3)).Label(title).Children(func() {
			ui.Row(c).FillWidth().Height(52).Shrink(0).Padding(0, 12, 0, 20).BorderWidth(0, 0, 1, 0).BorderColor(p.border).Children(func() {
				ui.Text(c, title).FontSize(16).FontWeight(600).Grow(1).Level(1)
				if iconButton(c, key+"-close", icClose, L.WindowClose()).Clicked() {
					*open = false
				}
			})
			ui.Scroll(c.Key(key+"-body")).Grow(1).FillWidth().Padding(20, 24).Gap(16).Children(body)
		})
	})
}

// featureRow is a line of what an APK holds: an icon, a title over a
// sentence, and how far it works here.
func featureRow(c *ui.Context, i int, icon *ui.SVG, title, hint, word string, t tone) {
	p := pal(c)
	ui.Row(c).FillWidth().Gap(14).Padding(12, 16).BorderWidth(min(float32(i), 1), 0, 0, 0).BorderColor(p.hover).Children(func() {
		tile(c, icon, 32, p.hover, p.text)
		ui.Column(c).Grow(1).MinWidth(0).Children(func() {
			ui.Text(c, title).FontWeight(600)
			if hint != "" {
				muted(c, hint, 13).Wrap()
			}
		})
		if word != "" {
			chip(c, word, t)
		}
	})
}

// apkFeatures are the lines of what an APK holds (design/Inspect.html,
// AppDetails.html), with how far each works for e's settings.
func (a *App) apkFeatures(c *ui.Context, apk winapk.APK, native winapk.Native) {
	p := pal(c)
	ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
		i := 0
		featureRow(c, i, icJava, L.FeatJava(), L.FeatJavaHint(), L.LevelWorks(), toneGood)
		i++
		if len(apk.NativeLibs) > 0 {
			word, t := L.NativeOffChip(), toneNeutral
			switch {
			case apk.Verdict == winapk.Unsupported:
				word, t = L.NotSupported(), toneBad
			case native == winapk.NativeRestricted:
				word, t = L.LevelExperimental(), toneWarn
			}
			featureRow(c, i, icChip, L.FeatNative(strings.Join(apk.ABIs, ", ")), L.FeatNativeHint(len(apk.NativeLibs)), word, t)
			i++
		}
		if apk.GLES {
			featureRow(c, i, icCube, "OpenGL ES", L.FeatGLESHint(), L.LevelExperimental(), toneWarn)
			i++
		}
		if apk.Vulkan {
			featureRow(c, i, icMonitor, "Vulkan", L.FeatVulkanHint(), L.LevelNotYet(), toneNeutral)
		}
	})
}

// verdictCard is what this PC can do with an APK, in words.
func verdictCard(c *ui.Context, apk winapk.APK) {
	p := pal(c)
	title, body, t := L.VerdictCanRun(), L.VerdictCanRunBody(), toneGood
	switch apk.Verdict {
	case winapk.NeedsNative:
		title, body, t = L.VerdictJavaOnly(), L.VerdictJavaOnlyBody(), toneWarn
	case winapk.NotYet:
		title, body, t = L.CantRunYet(), L.VerdictNotYetBody(), toneWarn
	case winapk.Unsupported:
		title, body, t = L.NotSupported(), L.VerdictUnsupportedBody(), toneBad
	}
	fg, bg := t.colors(p)
	ui.Column(c).FillWidth().Gap(4).Padding(12, 16).Radius(radiusCard).Background(bg).Children(func() {
		ui.Text(c, title).FontWeight(600).TextColor(fg)
		ui.Text(c, body).FontSize(13).Wrap()
	})
}

// addSheet is Add an app (design/Inspect.html): what's inside the APK and
// what this PC can run, how it opens, and Add to library or Add and open.
func (a *App) addSheet(c *ui.Context) {
	if !a.add.open {
		return
	}
	p := pal(c)
	sheet(c, "add", &a.add.open, L.AddAnApp(), 640, func() {
		if a.add.reading {
			ui.Row(c).Gap(10).Children(func() {
				ui.Spinner(c)
				muted(c, L.Reading(filepath.Base(a.add.path)), fontBody)
			})
			return
		}
		if a.add.failed {
			ui.Text(c, L.ReadFailed(filepath.Base(a.add.path))).Wrap()
			return
		}
		apk := a.add.apk
		tmp := &entry{ID: apk.Package, APK: apk}
		a.loadIcon(tmp)
		ui.Row(c).FillWidth().Gap(14).Children(func() {
			appIcon(c, tmp, 48)
			ui.Column(c).Grow(1).MinWidth(0).Gap(2).Children(func() {
				ui.Text(c, apk.Label).FontSize(18).FontWeight(600)
				muted(c, L.VersionLine(apk.Version, apk.Package, mib(apk.Size)), 13)
				muted(c, L.FromFile(filepath.Base(a.add.path)), 13)
			})
			if button(c, "add-other", L.ChooseAnother(), nil).Clicked() {
				a.chooseAPK()
			}
		})
		verdictCard(c, apk)
		sectionTitle(c, L.WhatsInside())
		a.apkFeatures(c, apk, winapk.NativeOff)
		if a.entry(apk.Package) != nil {
			ui.Text(c, L.AlreadyAdded()).FontSize(13).TextColor(p.warn).Wrap()
		}
		ui.Row(c).FillWidth().Gap(12).AlignItems(ui.Center).Children(func() {
			ui.Text(c, L.Opens()).FontWeight(600).Grow(1)
			ui.Segmented(c.Key("add-opens"), &a.add.inWindow, L.AsPane(), L.InOwnWindow()).Label(L.Opens())
		})
		muted(c, L.NothingRan(), 13)
		ui.Row(c).FillWidth().Gap(8).Children(func() {
			ui.Spacer(c)
			if button(c, "add-cancel", L.Cancel(), nil).Clicked() {
				a.add.open = false
			}
			if button(c, "add-lib", L.AddToLibrary(), nil).Clicked() {
				a.addEntry(false)
			}
			if primary(c, "add-open", L.AddAndOpen(), nil).Disabled(apk.Verdict == winapk.NotYet || apk.Verdict == winapk.Unsupported || !a.report.Ready()).Clicked() {
				a.addEntry(true)
			}
		})
	})
}

// addEntry puts the APK read into the library, replacing an entry of the
// same package, and opens it when open.
func (a *App) addEntry(open bool) {
	apk := a.add.apk
	e := a.newEntry(apk)
	e.InWindow = a.add.inWindow == 1
	if old := a.entry(apk.Package); old != nil {
		e.Native, e.Profile, e.Graphics, e.Screen, e.Added = old.Native, old.Profile, old.Graphics, old.Screen, old.Added
		*old = *e
		e = old
	} else {
		a.library = append(a.library, e)
	}
	a.saveLibrary()
	a.add = addState{}
	if open {
		a.launch(e, e.InWindow, false)
	}
}

// detailsState is an app's details while open.
type detailsState struct {
	open     bool
	id       string
	advanced bool
}

// openDetails opens an app's details and settings.
func (a *App) openDetails(id string) { a.details = detailsState{open: true, id: id} }

// detailsSheet is an app's details and settings (design/AppDetails.html):
// what works here, how it opens, its screen size, its data, and under
// Advanced its native code and graphics.
func (a *App) detailsSheet(c *ui.Context) {
	if !a.details.open {
		return
	}
	e := a.entry(a.details.id)
	if e == nil {
		a.details.open = false
		return
	}
	p := pal(c)
	sheet(c, "details", &a.details.open, e.APK.Label, 680, func() {
		s := a.open[e.ID]
		ui.Row(c).FillWidth().Gap(14).AlignItems(ui.Center).Children(func() {
			appIcon(c, e, 48)
			ui.Column(c).Grow(1).MinWidth(0).Gap(2).Children(func() {
				ui.Text(c, e.APK.Label).FontSize(18).FontWeight(600)
				muted(c, L.VersionLine(e.APK.Version, e.APK.Package, mib(e.APK.Size)), 13)
			})
			state := L.StateStopped()
			if s != nil {
				state = stateWords(s, false, e, a)
			}
			muted(c, state, 13)
			if e.runnable() && (s == nil || !s.live()) {
				if primary(c, "det-open", L.Open(), nil).Disabled(!a.report.Ready()).Clicked() {
					a.details.open = false
					a.launch(e, e.InWindow, false)
				}
			}
		})
		sectionTitle(c, L.WhatWorks())
		a.apkFeatures(c, e.APK, e.Native)
		if e.LastFailed != "" {
			ui.Row(c).FillWidth().Gap(10).Padding(10, 14).Radius(radiusCard).Background(p.warnBg).Children(func() {
				ui.Text(c, L.LastFailed()).FontSize(13).Grow(1).Wrap()
				if link(c, "det-log", L.WhatHappened()).Clicked() {
					a.details.open = false
					a.diag.page, a.diag.app = 1, e.ID
					a.showTab(tabDiag, false)
				}
			})
		}
		sectionTitle(c, L.Opening())
		settingRow(c, L.OpensTitle(), L.OpensHint(), func() {
			cur := 0
			if e.InWindow {
				cur = 1
			}
			if ui.Segmented(c.Key("det-opens"), &cur, L.AsPane(), L.InOwnWindow()).Label(L.OpensTitle()).Changed() {
				e.InWindow = cur == 1
				a.saveLibrary()
			}
		})
		settingRow(c, L.ScreenSize(), L.ScreenSizeHint(), func() {
			if ui.Segmented(c.Key("det-screen"), &e.Screen, L.ScreenPhone(), L.ScreenTablet(), L.ScreenFit()).Label(L.ScreenSize()).Changed() {
				a.saveLibrary()
			}
		})
		sectionTitle(c, L.Data())
		settingRow(c, L.AppData(), L.AppDataHint(), func() {
			if button(c, "det-remove", L.RemoveApp(), nil).Clicked() {
				a.confirm = confirmState{open: true, kind: confirmRemove, id: e.ID}
			}
		})
		if len(e.APK.NativeLibs) > 0 || e.APK.GLES || e.APK.Vulkan {
			ui.Row(c).FillWidth().Children(func() {
				sectionTitle(c, L.Advanced())
				ui.Spacer(c)
				label := L.ShowAdvanced()
				if a.details.advanced {
					label = L.Hide()
				}
				if link(c, "det-adv", label).Clicked() {
					a.details.advanced = !a.details.advanced
				}
			})
			if a.details.advanced {
				a.advanced(c, e)
			}
		}
	})
}

// advanced is an app's native code and graphics settings.
func (a *App) advanced(c *ui.Context, e *entry) {
	p := pal(c)
	if len(e.APK.NativeLibs) > 0 && e.APK.Verdict != winapk.Unsupported {
		settingRow(c, L.NativeCodeTitle(), L.NativeCodeHint(), func() {
			cur := 0
			if e.Native == winapk.NativeRestricted {
				cur = 1
			}
			if ui.Segmented(c.Key("det-native"), &cur, L.NativeOff(), L.NativeRestricted()).Label(L.NativeCodeTitle()).Changed() {
				e.Native = winapk.NativeOff
				if cur == 1 {
					e.Native = winapk.NativeRestricted
				}
				a.saveLibrary()
			}
		})
		if e.Native == winapk.NativeRestricted {
			ui.Column(c).FillWidth().Gap(4).Padding(4, 16).Children(func() {
				ui.Text(c, L.ProfileTitle()).FontWeight(600)
				choices := []struct {
					p           winapk.Profile
					title, hint string
				}{
					{"", L.ProfileDefault(string(a.prefs.Profile)), L.ProfileDefaultHint()},
					{winapk.N0, L.ProfileN0(), L.ProfileN0Hint()},
					{winapk.N1, L.ProfileN1(), L.ProfileN1Hint()},
					{winapk.N2, L.ProfileN2(), L.ProfileN2Hint()},
				}
				for _, ch := range choices {
					r := ui.Radio(c.Key("det-prof-"+string(ch.p)), &e.Profile, ch.p, ch.title)
					if r.Changed() {
						a.saveLibrary()
					}
					hint := ch.hint
					if ch.p != "" && ch.p == e.APK.Profile {
						hint += " " + L.ProfileNeeded()
					}
					muted(c, hint, fontSmall).Wrap().Padding(0, 0, 6, 28)
				}
			})
		}
	}
	settingRow(c, L.GraphicsTitle(), L.GraphicsHint(), func() {
		a.graphicsSelect(c, "det-gfx", &e.Graphics, true, func() { a.saveLibrary() })
	})
	muted(c, L.ChangesNextOpen(), 13).TextColor(p.muted)
}

// graphicsSelect chooses a graphics backend; withDefault offers "Use the
// default" (an app's own choice).
func (a *App) graphicsSelect(c *ui.Context, key string, v *string, withDefault bool, changed func()) {
	ids := []string{"auto", "vulkan", "d3d11"}
	opts := []string{L.GfxAuto(), "Vulkan", "Direct3D 11"}
	if withDefault {
		ids = append([]string{""}, ids...)
		opts = append([]string{L.UseDefault()}, opts...)
	}
	cur := opts[0]
	for i, id := range ids {
		if id == *v {
			cur = opts[i]
		}
	}
	if ui.Select(c.Key(key), &cur, opts).Label(L.GraphicsTitle()).MinWidth(180).Changed() {
		for i, o := range opts {
			if o == cur {
				*v = ids[i]
			}
		}
		changed()
	}
}

// settingRow is a title over a sentence, and its control at the end.
func settingRow(c *ui.Context, title, hint string, control func()) {
	ui.Row(c).FillWidth().Gap(14).Padding(4, 0).AlignItems(ui.Center).Children(func() {
		ui.Column(c).Grow(1).MinWidth(0).Children(func() {
			ui.Text(c, title).FontWeight(600)
			if hint != "" {
				muted(c, hint, 13).Wrap()
			}
		})
		ui.Row(c).Gap(8).Shrink(0).Children(control)
	})
}

type confirmKind int

const (
	confirmClose confirmKind = iota
	confirmRemove
)

// confirmState is a confirmation while open: closing the main window with
// apps in panes, or removing an app.
type confirmState struct {
	open    bool
	kind    confirmKind
	id      string
	closing bool // the window closes now, without asking again
	dontAsk bool
}

// confirmDialog asks before what can't be undone (design/
// CloseConfirm.html).
func (a *App) confirmDialog(c *ui.Context) {
	if !a.confirm.open {
		return
	}
	p := pal(c)
	title := L.CloseTitle()
	if a.confirm.kind == confirmRemove {
		e := a.entry(a.confirm.id)
		if e == nil {
			a.confirm = confirmState{}
			return
		}
		title = L.RemoveTitle(e.APK.Label)
	}
	ui.DialogBase(c.Key("confirm"), &a.confirm.open, func(backdrop, panel ui.Element) {
		backdrop.Background(p.scrim)
		panel.Width(480).Radius(radiusDlg).Background(p.pane).Padding(24).Gap(14).Label(title).Children(func() {
			ui.Text(c, title).FontSize(18).FontWeight(600).Level(1)
			if a.confirm.kind == confirmRemove {
				muted(c, L.RemoveBody(), fontBody).Wrap()
				ui.Row(c).FillWidth().Gap(8).Children(func() {
					ui.Spacer(c)
					if button(c, "rm-cancel", L.Cancel(), nil).Clicked() {
						a.confirm = confirmState{}
					}
					if primary(c, "rm-ok", L.Remove(), nil).Clicked() {
						id := a.confirm.id
						a.confirm = confirmState{}
						a.details.open = false
						a.removeEntry(id)
					}
				})
				return
			}
			muted(c, L.CloseBody(), fontBody).Wrap()
			panes := a.paneApps()
			ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
				i := 0
				line := func(id, word string, t tone) {
					e := a.entry(id)
					if e == nil {
						return
					}
					ui.Row(c).FillWidth().Gap(10).Padding(10, 14).BorderWidth(min(float32(i), 1), 0, 0, 0).BorderColor(p.hover).Children(func() {
						appIcon(c, e, 24)
						ui.Text(c, e.APK.Label).Grow(1)
						chip(c, word, t)
					})
					i++
				}
				for _, id := range panes {
					line(id, L.Stops(), toneWarn)
				}
				for _, id := range a.windowApps() {
					if a.prefs.CloseStopsAll {
						line(id, L.Stops(), toneWarn)
					} else {
						line(id, L.KeepsRunning(), toneGood)
					}
				}
			})
			muted(c, L.CloseHint(), 13).Wrap()
			ui.Row(c).FillWidth().Gap(8).AlignItems(ui.Center).Children(func() {
				ui.Checkbox(c.Key("close-dont"), &a.confirm.dontAsk, L.DontAskAgain())
				ui.Spacer(c)
				if button(c, "close-cancel", L.Cancel(), nil).Clicked() {
					a.confirm = confirmState{}
				}
				n := len(panes)
				if a.prefs.CloseStopsAll {
					n += len(a.windowApps())
				}
				if primary(c, "close-ok", L.StopAndClose(n), nil).Clicked() {
					if a.confirm.dontAsk {
						pr := a.prefs
						pr.AskOnClose = false
						a.setPrefs(pr)
					}
					w := a.window
					a.confirm = confirmState{closing: true}
					if w != nil {
						w.Close()
					} else {
						a.closeMain()
					}
				}
			})
		})
	})
}
