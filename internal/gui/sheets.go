package gui

import (
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/egoist/mygo"
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
	nativeMode    int // 0 Off (default per AGENTS.md), 1 Restricted
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
			if err == nil {
				if old := a.entry(apk.Package); old != nil {
					if old.Native == winapk.NativeRestricted {
						a.add.nativeMode = 1
					} else {
						a.add.nativeMode = 0
					}
				} else if apk.Verdict == winapk.NeedsNative && len(apk.NativeLibs) > 0 {
					a.add.nativeMode = 1
				}
			}
		})
	})
}

// sheet is a dialog over the dimmed window with a title and a close
// button; body fills it and foot (optional) anchors at the bottom.
func sheet(c *ui.Context, key string, open *bool, title string, width float32, body func(), foot ...func()) {
	p := pal(c)
	ui.DialogBase(c.Key(key), open, func(backdrop, panel ui.Element) {
		backdrop.Background(p.scrim)
		panel.Width(width).MaxWidthPercent(92).MaxHeightPercent(90).Radius(radiusDlg).Background(p.pane).Clip().Shadow(0, 18, 48, 0, ui.RGBA(10, 16, 24, 0.3)).Label(title).Children(func() {
			ui.Row(c).FillWidth().Height(52).Shrink(0).Padding(0, 12, 0, 20).AlignItems(ui.Center).BorderWidth(0, 0, 1, 0).BorderColor(p.border).Children(func() {
				ui.Text(c, title).FontSize(16).FontWeight(600).Grow(1).Level(1)
				if iconButton(c, key+"-close", icClose, L.WindowClose()).Clicked() {
					*open = false
				}
			})
			ui.Scroll(c.Key(key+"-body")).Grow(1).FillWidth().Padding(20, 24).Gap(16).Children(body)
			if len(foot) > 0 && foot[0] != nil {
				ui.Row(c).FillWidth().Shrink(0).Padding(12, 24).Gap(8).AlignItems(ui.Center).Background(p.subtle).BorderWidth(1, 0, 0, 0).BorderColor(p.border).Children(foot[0])
			}
		})
	})
}

// featureRow is a line of what an APK holds: an icon, a title over a
// sentence, and how far it works here.
func featureRow(c *ui.Context, i int, icon *ui.SVG, title, hint, word string, t tone) {
	p := pal(c)
	ui.Row(c).FillWidth().Gap(14).Padding(10, 16).AlignItems(ui.Center).BorderWidth(min(float32(i), 1), 0, 0, 0).BorderColor(p.hover).Children(func() {
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

// permissionWords formats Android permission identifiers into readable words.
func permissionWords(perms []string) string {
	var words []string
	seen := map[string]bool{}
	other := 0
	add := func(w string) {
		if !seen[w] {
			seen[w] = true
			words = append(words, w)
		}
	}
	for _, p := range perms {
		switch p {
		case "android.permission.INTERNET":
			add(L.PermInternet())
		case "android.permission.ACCESS_NETWORK_STATE", "android.permission.ACCESS_WIFI_STATE":
			add(L.PermNetwork())
		case "android.permission.VIBRATE":
			add(L.PermVibrate())
		case "android.permission.READ_EXTERNAL_STORAGE", "android.permission.WRITE_EXTERNAL_STORAGE",
			"android.permission.READ_MEDIA_IMAGES", "android.permission.READ_MEDIA_VIDEO", "android.permission.READ_MEDIA_AUDIO":
			add(L.PermStorage())
		case "android.permission.RECORD_AUDIO":
			add(L.PermAudio())
		case "android.permission.CAMERA":
			add(L.PermCamera())
		case "android.permission.ACCESS_FINE_LOCATION", "android.permission.ACCESS_COARSE_LOCATION":
			add(L.PermLocation())
		case "android.permission.POST_NOTIFICATIONS":
			add(L.PermNotifications())
		case "android.permission.BLUETOOTH", "android.permission.BLUETOOTH_CONNECT", "android.permission.BLUETOOTH_SCAN":
			add(L.PermBluetooth())
		default:
			other++
		}
	}
	if other > 0 {
		words = append(words, L.PermOther(other))
	}
	return strings.Join(words, ", ")
}

// apkFeatures are the lines of what an APK holds (design/Inspect.html,
// AppDetails.html), with how far each works for e's settings.
func (a *App) apkFeatures(c *ui.Context, apk winapk.APK, native winapk.Native, showPerms bool) {
	p := pal(c)
	ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
		i := 0
		featureRow(c, i, icJava, L.FeatJava(), L.FeatJavaHint(), L.LevelWorks(), toneGood)
		i++
		if len(apk.NativeLibs) > 0 {
			word, t := L.NativeOffChip(), toneWarn
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
			glesTitle := "OpenGL ES"
			if apk.GLESVersion != "" {
				glesTitle += " " + apk.GLESVersion
			}
			featureRow(c, i, icCube, glesTitle, L.FeatGLESHint(), L.LevelExperimental(), toneWarn)
			i++
		}
		if apk.Vulkan {
			vkWord, vkTone := L.LevelNotYet(), toneNeutral
			if a.report.Machine.VulkanLoader == "available" {
				vkWord, vkTone = L.LevelExperimental(), toneWarn
			}
			featureRow(c, i, icMonitor, "Vulkan", L.FeatVulkanHint(), vkWord, vkTone)
			i++
		}
		if showPerms && len(apk.Permissions) > 0 {
			if pw := permissionWords(apk.Permissions); pw != "" {
				featureRow(c, i, icShield, L.AsksFor(), L.AsksForHint(pw), "", toneNeutral)
			}
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
	icon := icCheck
	if t != toneGood {
		icon = icWarn
	}
	ui.Row(c).FillWidth().Gap(12).Padding(12, 16).Radius(radiusCard).Background(bg).Children(func() {
		ui.Icon(c, icon).FontSize(18).TextColor(fg).Shrink(0)
		ui.Column(c).Grow(1).MinWidth(0).Gap(3).Children(func() {
			ui.Text(c, title).FontWeight(600).TextColor(fg)
			ui.Text(c, body).FontSize(13).Wrap()
		})
	})
}

// addSheet is Add an app (design/Inspect.html): what's inside the APK and
// what this PC can run, how it opens, and Add to library or Add and open.
func (a *App) addSheet(c *ui.Context) {
	if !a.add.open {
		return
	}
	p := pal(c)
	apk := a.add.apk
	ready := !a.add.reading && !a.add.failed
	sheet(c, "add", &a.add.open, L.AddAnApp(), 680, func() {
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
		tmp := &entry{ID: apk.Package, APK: apk}
		a.loadIcon(tmp)
		ui.Row(c).FillWidth().Gap(16).AlignItems(ui.Center).Children(func() {
			appIcon(c, tmp, 52)
			ui.Column(c).Grow(1).MinWidth(0).Gap(2).Children(func() {
				ui.Text(c, apk.Label).FontSize(20).FontWeight(600)
				muted(c, L.VersionLine(apk.Version, apk.Package, mib(apk.Size)), 13)
				muted(c, L.FromFile(filepath.Base(a.add.path)), 13)
			})
			if button(c, "add-other", L.ChooseAnother(), nil).Clicked() {
				a.chooseAPK()
			}
		})
		verdictCard(c, apk)
		sectionTitle(c, L.WhatsInside())
		selNative := winapk.NativeOff
		if a.add.nativeMode == 1 {
			selNative = winapk.NativeRestricted
		}
		a.apkFeatures(c, apk, selNative, true)
		if a.entry(apk.Package) != nil {
			ui.Text(c, L.AlreadyAdded()).FontSize(13).TextColor(p.warn).Wrap()
		}
		if len(apk.NativeLibs) > 0 && apk.Verdict != winapk.Unsupported {
			ui.Row(c).FillWidth().Gap(12).AlignItems(ui.Center).Children(func() {
				muted(c, L.NativeCodeTitle(), 13).Grow(1)
				ui.Segmented(c.Key("add-native"), &a.add.nativeMode, L.NativeOff(), L.NativeRestricted()).Label(L.NativeCodeTitle())
			})
		}
		ui.Row(c).FillWidth().Gap(12).AlignItems(ui.Center).Children(func() {
			muted(c, L.Opens(), 13).Grow(1)
			ui.Segmented(c.Key("add-opens"), &a.add.inWindow, L.AsPane(), L.InOwnWindow()).Label(L.Opens())
		})
	}, func() {
		ui.Icon(c, icInfo).FontSize(16).TextColor(p.muted)
		muted(c, L.NothingRan(), 13).Grow(1)
		if button(c, "add-cancel", L.Cancel(), nil).Clicked() {
			a.add.open = false
		}
		if ready {
			if button(c, "add-lib", L.AddToLibrary(), nil).Clicked() {
				a.addEntry(false)
			}
			if primary(c, "add-open", L.AddAndOpen(), nil).Disabled(apk.Verdict == winapk.NotYet || apk.Verdict == winapk.Unsupported || !a.report.Ready()).Clicked() {
				a.addEntry(true)
			}
		}
	})
}

// addEntry puts the APK read into the library, replacing an entry of the
// same package, and opens it when open.
func (a *App) addEntry(open bool) {
	apk := a.add.apk
	e := a.newEntry(apk)
	e.InWindow = a.add.inWindow == 1
	if len(apk.NativeLibs) > 0 && apk.Verdict != winapk.Unsupported {
		if a.add.nativeMode == 1 {
			e.Native = winapk.NativeRestricted
			e.Profile = maxProfile(winapk.N2, apk.Profile)
			if e.Graphics == "" {
				e.Graphics = "auto"
			}
		} else {
			e.Native = winapk.NativeOff
			e.Profile = ""
		}
	}
	if old := a.entry(apk.Package); old != nil {
		delete(a.missing, old.APK.Path)
		if e.Graphics == "" {
			e.Graphics = old.Graphics
		}
		e.Screen, e.Added = old.Screen, old.Added
		if !a.demo && apk.SHA256 != "" && apk.SHA256 == old.APK.SHA256 {
			if managed, err := winapk.ManagedAPKPath(a.appDataDir(), apk.Package); err == nil && samePath(old.APK.Path, managed) {
				if _, err := os.Stat(managed); err == nil {
					e.APK.Path = managed
				}
			}
		}
		*old = *e
		e = old
	} else {
		a.library = append(a.library, e)
	}
	a.saveLibrary()
	delete(a.missing, e.APK.Path)
	a.add = addState{}
	a.stageAPK(e)
	if open {
		a.launch(e, e.InWindow, false)
	}
}

// detailsState is an app's details while open on Home.
type detailsState struct {
	open     bool
	id       string
	advanced bool
}

// openDetails opens an app's details and settings inside Home (design/AppDetails.html).
func (a *App) openDetails(id string) {
	a.details = detailsState{open: true, id: id, advanced: true}
	a.refreshStorage()
	a.showTab(tabHome, false)
}

func addedWords(t time.Time) string {
	if t.IsZero() {
		return L.AddedToday()
	}
	now := time.Now()
	if t.Year() == now.Year() && t.YearDay() == now.YearDay() {
		return L.AddedToday()
	}
	return L.AddedOn(t.Format("2 Jan 2006"))
}

// detailsView is an app's two-column details and settings inside Home
// (design/AppDetails.html).
func (a *App) detailsView(c *ui.Context) {
	e := a.entry(a.details.id)
	if e == nil {
		a.details.open = false
		return
	}
	p := pal(c)
	s := a.open[e.ID]
	live := s != nil && s.live()

	ui.Column(c.Key("details-" + e.ID)).Fill().Children(func() {
		// Header bar: < Back, 48pt icon, title & metadata, state, Open/Show ▾, ⋯
		ui.Row(c).FillWidth().Padding(14, 24).Gap(12).Shrink(0).AlignItems(ui.Center).BorderWidth(0, 0, 1, 0).BorderColor(p.hover).Children(func() {
			if iconButton(c, "det-back", icBack, L.BackToApps()).Clicked() {
				a.details.open = false
			}
			appIcon(c, e, 48)
			ui.Column(c).Grow(1).MinWidth(0).Gap(2).Children(func() {
				ui.Text(c, e.APK.Label).FontSize(22).FontWeight(600).Ellipsis("…").MaxLines(1)
				muted(c, L.Version()+" "+e.APK.Version+" · "+e.APK.Package+" · "+addedWords(e.Added), 13).Ellipsis("…").MaxLines(1)
			})
			state := L.StateStopped()
			if s != nil {
				state = stateWords(s, false, e, a)
			}
			muted(c, state, 13).NoWrap()
			if live {
				a.showButton(c, e)
			} else if e.runnable() {
				a.openButton(c, e)
			}
			iconButton(c, "det-more", icMore, L.More()).Menu(func(m *ui.Menu) { a.rowMenu(m, e) })
		})

		scroll := ui.Scroll(c.Key("det-scroll")).Grow(1).FillWidth().Padding(18, 24).Gap(24)
		cols := 2
		if scroll.LayoutBounds().W > 0 && scroll.LayoutBounds().W < 780 {
			cols = 1
		}
		scroll.Children(func() {
			ui.Grid(c.Key("det-grid")).Columns(cols).Gap(24).FillWidth().Children(func() {
				// Left column: What works on this PC, Opening, Data.
				ui.Column(c).Gap(18).MinWidth(0).Children(func() {
					ui.Column(c).Gap(8).FillWidth().Children(func() {
						sectionTitle(c, L.WhatWorks())
						a.apkFeatures(c, e.APK, e.Native, false)
						if e.LastFailed != "" {
							ui.Row(c).Gap(6).AlignItems(ui.Center).Children(func() {
								muted(c, L.LastFailed(), 13)
								if link(c, "det-log", L.WhatHappened()).Clicked() {
									a.details.open = false
									a.diag.page, a.diag.app = 1, e.ID
									a.showTab(tabDiag, false)
								}
							})
						}
					})

					ui.Column(c).Gap(8).FillWidth().Children(func() {
						sectionTitle(c, L.Opening())
						ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
							ui.Row(c).FillWidth().Gap(14).Padding(10, 16).AlignItems(ui.Center).Children(func() {
								ui.Column(c).Grow(1).MinWidth(0).Children(func() {
									ui.Text(c, L.OpensTitle()).FontWeight(600)
									muted(c, L.OpensHint(), 13).Wrap()
								})
								cur := 0
								if e.InWindow {
									cur = 1
								}
								if ui.Segmented(c.Key("det-opens"), &cur, L.AsPane(), L.InOwnWindow()).Label(L.OpensTitle()).Changed() {
									e.InWindow = cur == 1
									a.saveLibrary()
								}
							})
							ui.Row(c).FillWidth().Gap(14).Padding(10, 16).AlignItems(ui.Center).BorderWidth(1, 0, 0, 0).BorderColor(p.hover).Children(func() {
								ui.Column(c).Grow(1).MinWidth(0).Children(func() {
									ui.Text(c, L.ScreenSize()).FontWeight(600)
									muted(c, L.ScreenSizeHint(), 13).Wrap()
								})
								opts := []string{L.ScreenPhone(), L.ScreenTablet(), L.ScreenFit()}
								cur := opts[min(max(e.Screen, 0), 2)]
								if ui.Select(c.Key("det-screen"), &cur, opts).Label(L.ScreenSize()).MinWidth(175).Changed() {
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

					ui.Column(c).Gap(8).FillWidth().Children(func() {
						sectionTitle(c, L.Data())
						dataSz := mib(a.appDataBytes[e.ID])
						ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
							ui.Row(c).FillWidth().Gap(10).Padding(10, 16).AlignItems(ui.Center).Children(func() {
								tile(c, icFolder, 32, p.hover, p.text)
								ui.Column(c).Grow(1).MinWidth(0).Children(func() {
									ui.Text(c, L.AppDataSize(dataSz)).FontWeight(600)
									muted(c, L.AppDataHint(), 13).Wrap()
								})
								if iconButton(c, "det-folder", icFolder, L.OpenFolder()).Clicked() {
									dir := winapk.AppDataDir(a.appDataDir(), e.APK.Package)
									a.async(func() {
										_ = os.MkdirAll(dir, 0o700)
										_ = mygo.Shell.OpenPath(dir)
									})
								}
								if button(c, "det-clear", L.Clear(), nil).Disabled(live).Clicked() {
									pkg := e.APK.Package
									root := a.appDataDir()
									a.async(func() {
										_ = winapk.ClearAppData(root, pkg)
										a.later(func() {
											a.appDataBytes[pkg] = 0
											a.refreshStorage()
											a.setToast(L.ClearDone())
										})
									})
								}
								if dangerButton(c, "det-remove", L.RemoveApp()).Clicked() {
									a.openRemoveConfirm(e.ID)
								}
							})
						})
					})
				})

				// Right column: Advanced (native profile & graphics backend).
				ui.Column(c).Gap(8).MinWidth(0).Children(func() {
					ui.Row(c).FillWidth().AlignItems(ui.Center).Children(func() {
						sectionTitle(c, L.Advanced())
						ui.Spacer(c)
						advLabel := L.ShowAdvanced()
						if a.details.advanced {
							advLabel = L.Hide()
						}
						if link(c, "det-adv", advLabel).Clicked() {
							a.details.advanced = !a.details.advanced
						}
					})
					if a.details.advanced {
						a.advanced(c, e)
					}
				})
			})
		})
	})
}

// advanced is an app's native code and graphics settings card (design/AppDetails.html).
func (a *App) advanced(c *ui.Context, e *entry) {
	p := pal(c)
	ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
		rowIdx := 0
		if len(e.APK.NativeLibs) > 0 && e.APK.Verdict != winapk.Unsupported {
			ui.Column(c).FillWidth().Padding(12, 16).Gap(10).Children(func() {
				ui.Row(c).FillWidth().Gap(14).AlignItems(ui.Start).Children(func() {
					tile(c, icChip, 32, p.hover, p.text)
					ui.Column(c).Grow(1).MinWidth(0).Children(func() {
						ui.Text(c, L.NativeCodeTitle()).FontWeight(600)
						muted(c, L.NativeCodeHint(), 13).Wrap()
					})
					cur := 0
					if e.Native == winapk.NativeRestricted {
						cur = 1
					}
					if ui.Segmented(c.Key("det-native"), &cur, L.NativeOff(), L.NativeRestricted()).Label(L.NativeCodeTitle()).Changed() {
						e.Native = winapk.NativeOff
						if cur == 1 {
							e.Native = winapk.NativeRestricted
							if profileRank(e.Profile) < profileRank(maxProfile(winapk.N2, e.APK.Profile)) {
								e.Profile = maxProfile(winapk.N2, e.APK.Profile)
							}
						}
						a.saveLibrary()
					}
				})
				if e.Native == winapk.NativeRestricted {
					ui.Column(c).FillWidth().Padding(0, 0, 0, 46).Gap(4).Children(func() {
						ui.Text(c, L.ProfileTitle()).FontSize(13).FontWeight(600)
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
							on := e.Profile == ch.p
							hint := ch.hint
							if ch.p != "" && ch.p == e.APK.Profile {
								hint += " " + L.ProfileNeeded()
							}
							card := ui.ButtonBase(c.Key("det-prof-"+string(ch.p))).FillWidth().Padding(8, 10).Radius(radiusCtl).Selected(on).Label(ch.title)
							if on {
								card.Background(p.tint)
							} else if card.Hovered() {
								card.Background(p.subtle)
							}
							card.Children(func() {
								ui.Row(c).FillWidth().Gap(10).AlignItems(ui.Start).Children(func() {
									ringCol := p.muted
									if on {
										ringCol = p.accent
									}
									ui.Box(c).Size(16, 16).Radius(8).Border(1.5, ringCol).Center().Margin(2, 0, 0, 0).Shrink(0).Children(func() {
										if on {
											ui.Box(c).Size(8, 8).Radius(4).Background(p.accent)
										}
									})
									ui.Column(c).Grow(1).MinWidth(0).Gap(1).Children(func() {
										ui.Text(c, ch.title).FontSize(13).FontWeight(600).TextColor(p.text)
										muted(c, hint, fontSmall).Wrap()
									})
								})
							})
							if card.Clicked() && e.Profile != ch.p {
								e.Profile = ch.p
								a.saveLibrary()
							}
						}
					})
				}
			})
			rowIdx++
		}
		ui.Column(c).FillWidth().Padding(12, 16).Gap(8).BorderWidth(min(float32(rowIdx), 1), 0, 0, 0).BorderColor(p.hover).Children(func() {
			ui.Row(c).FillWidth().Gap(14).AlignItems(ui.Start).Children(func() {
				tile(c, icCube, 32, p.hover, p.text)
				ui.Column(c).Grow(1).MinWidth(0).Children(func() {
					ui.Text(c, L.GraphicsTitle()).FontWeight(600)
					muted(c, L.GraphicsHint(), 13).Wrap()
				})
				a.graphicsSelect(c, "det-gfx", &e.Graphics, true, func() { a.saveLibrary() })
			})
			effGfx := e.Graphics
			if effGfx == "" {
				effGfx = a.prefs.Graphics
			}
			if effGfx == "vulkan" && a.report.Machine.VulkanTranslated {
				ui.Text(c, L.VulkanTranslatedNote()).FontSize(12).TextColor(p.warn).Padding(0, 0, 0, 46).Wrap()
			}
		})
	})
	muted(c, L.ChangesNextOpen(), 13).TextColor(p.muted)
}

// graphicsSelect chooses a graphics backend via a segmented control matching design/.
func (a *App) graphicsSelect(c *ui.Context, key string, v *string, withDefault bool, changed func()) {
	effective := *v
	if withDefault && effective == "" {
		effective = a.prefs.Graphics
		if effective == "" {
			effective = "auto"
		}
	}
	ids := []string{"auto", "vulkan", "d3d11"}
	cur := 0
	for i, id := range ids {
		if id == effective {
			cur = i
		}
	}
	if ui.Segmented(c.Key(key), &cur, L.GfxAuto(), "Vulkan", "Direct3D 11").Label(L.GraphicsTitle()).Changed() {
		*v = ids[cur]
		changed()
	}
}

// docViewSheet shows a local text document (LICENSE or THIRD_PARTY_NOTICES.md).
func (a *App) docViewSheet(c *ui.Context) {
	if !a.docView.open {
		return
	}
	sheet(c, "docview", &a.docView.open, a.docView.title, 740, func() {
		ui.Text(c, a.docView.body).Font(fontMono).FontSize(fontSmall).Wrap()
	}, func() {
		ui.Spacer(c)
		if primary(c, "docview-ok", L.CloseDoc(), nil).Clicked() {
			a.docView.open = false
		}
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
	open     bool
	kind     confirmKind
	id       string
	closing  bool // the window closes now, without asking again
	dontAsk  bool
	keepData bool // for confirmRemove: keep app data when deleting the managed APK
}

// openRemoveConfirm opens the remove confirmation dialog defaulting to keeping
// saved app data while refreshing per-app storage sizes.
func (a *App) openRemoveConfirm(id string) {
	a.confirm = confirmState{open: true, kind: confirmRemove, id: id, keepData: true}
	a.refreshStorage()
}

// confirmDialog asks before what can't be undone (design/
// CloseConfirm.html).
func (a *App) confirmDialog(c *ui.Context) {
	if !a.confirm.open {
		return
	}
	p := pal(c)
	title := L.CloseTitle()
	var removeEntry *entry
	if a.confirm.kind == confirmRemove {
		removeEntry = a.entry(a.confirm.id)
		if removeEntry == nil {
			a.confirm = confirmState{}
			return
		}
		title = L.RemoveTitle(removeEntry.APK.Label)
	}
	ui.DialogBase(c.Key("confirm"), &a.confirm.open, func(backdrop, panel ui.Element) {
		backdrop.Background(p.scrim)
		panel.Width(480).Radius(radiusDlg).Background(p.pane).Padding(24).Gap(14).Label(title).Children(func() {
			ui.Text(c, title).FontSize(18).FontWeight(600).Level(1)
			if a.confirm.kind == confirmRemove {
				muted(c, L.RemoveBody(), fontBody).Wrap()
				delHint := L.RemoveDeleteDataHintEmpty()
				if sz := a.appDataBytes[removeEntry.ID]; sz > 0 {
					delHint = L.RemoveDeleteDataHint(mib(sz))
				}
				choices := []struct {
					key, title, hint string
					keep             bool
				}{
					{"rm-keep-data", L.RemoveKeepData(), L.RemoveKeepDataHint(), true},
					{"rm-del-data", L.RemoveDeleteData(), delHint, false},
				}
				ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Padding(4).Gap(4).Children(func() {
					for _, ch := range choices {
						on := a.confirm.keepData == ch.keep
						card := ui.ButtonBase(c.Key(ch.key)).FillWidth().Padding(8, 10).Radius(radiusCtl).Selected(on).Label(ch.title)
						if on {
							card.Background(p.tint)
						} else if card.Hovered() {
							card.Background(p.subtle)
						}
						card.Children(func() {
							ui.Row(c).FillWidth().Gap(10).AlignItems(ui.Start).Children(func() {
								ringCol := p.muted
								if on {
									ringCol = p.accent
								}
								ui.Box(c).Size(16, 16).Radius(8).Border(1.5, ringCol).Center().Margin(2, 0, 0, 0).Shrink(0).Children(func() {
									if on {
										ui.Box(c).Size(8, 8).Radius(4).Background(p.accent)
									}
								})
								ui.Column(c).Grow(1).MinWidth(0).Gap(1).Children(func() {
									ui.Text(c, ch.title).FontSize(13).FontWeight(600).TextColor(p.text)
									muted(c, ch.hint, fontSmall).Wrap()
								})
							})
						})
						if card.Clicked() && a.confirm.keepData != ch.keep {
							a.confirm.keepData = ch.keep
						}
					}
				})
				ui.Row(c).FillWidth().Gap(8).Children(func() {
					ui.Spacer(c)
					if button(c, "rm-cancel", L.Cancel(), nil).Clicked() {
						a.confirm = confirmState{}
					}
					if a.confirm.keepData {
						if primary(c, "rm-ok", L.RemoveAPKOnly(), nil).Clicked() {
							id := a.confirm.id
							a.confirm = confirmState{}
							a.details.open = false
							a.removeEntry(id, true)
						}
					} else {
						if dangerButton(c, "rm-ok", L.RemoveAndDeleteData()).Clicked() {
							id := a.confirm.id
							a.confirm = confirmState{}
							a.details.open = false
							a.removeEntry(id, false)
						}
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
					a.confirm = confirmState{closing: true}
					if a.window != nil {
						a.window.Close()
					}
				}
			})
		})
	})
}
