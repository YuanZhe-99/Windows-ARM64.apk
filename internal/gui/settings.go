package gui

import (
	"context"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/version"
	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

type settingsPage int

// Settings' pages, in the order of its list.
const (
	spGeneral settingsPage = iota
	spRuntime
	spAbout
)

// settingsState is the Settings sheet while open.
type settingsState struct {
	open  bool
	page  settingsPage
	query string
}

// openSettings opens the Settings sheet at a page.
func (a *App) openSettings(pg settingsPage) {
	a.settings = settingsState{open: true, page: pg}
	a.refreshStorage()
}

// name is a Settings page's title.
func (pg settingsPage) name() string {
	switch pg {
	case spRuntime:
		return L.PageRuntime()
	case spAbout:
		return L.PageAbout()
	}
	return L.PageGeneral()
}

// words are every setting's words on a page, for search.
func (pg settingsPage) words() string {
	switch pg {
	case spRuntime:
		return L.JavaRuntime() + " " + L.KeepUpdated() + " " + L.GraphicsTitle() + " " + L.DefaultProfile() + " " + L.NativeCodeTitle()
	case spAbout:
		return L.BasedOn() + " " + L.SourceCode() + " " + L.Licenses() + " " + L.Updates()
	}
	return L.NewAppsOpen() + " " + L.WhenClosing() + " " + L.AskFirst() + " " + L.AppData() + " " + L.Language() + " " + L.Appearance()
}

// settingsSheet is Settings over the dimmed window: search and the pages
// on the left, the page on the right (design/SettingsGeneral.html).
func (a *App) settingsSheet(c *ui.Context) {
	if !a.settings.open {
		return
	}
	p := pal(c)
	ui.DialogBase(c.Key("settings"), &a.settings.open, func(backdrop, panel ui.Element) {
		backdrop.Background(p.scrim)
		panel.WidthPercent(90).MaxWidth(1100).HeightPercent(90).MaxHeight(740).Radius(radiusDlg).Background(p.pane).Shadow(0, 18, 48, 0, ui.RGBA(10, 16, 24, 0.3)).Clip().Label(L.NavSettings()).Children(func() {
			ui.Row(c).FillWidth().Height(52).Shrink(0).Padding(0, 12, 0, 20).BorderWidth(0, 0, 1, 0).BorderColor(p.border).Children(func() {
				ui.Text(c, L.NavSettings()).FontSize(16).FontWeight(600).Grow(1).Level(1)
				if iconButton(c, "set-close", icClose, L.WindowClose()).Clicked() {
					a.settings.open = false
				}
			})
			ui.Row(c).Grow(1).FillWidth().AlignItems(ui.Stretch).Children(func() {
				a.settingsNav(c)
				ui.Scroll(c.Key("set-page")).Grow(1).MinWidth(0).Padding(28, 40).Gap(22).Children(func() {
					if a.settings.page != spAbout {
						ui.Column(c).Gap(4).MaxWidth(720).FillWidth().Children(func() {
							ui.Text(c, a.settings.page.name()).FontSize(fontTitle).FontWeight(600).Level(2)
							muted(c, a.pageAbout(), fontBody).Wrap()
						})
					}
					switch a.settings.page {
					case spRuntime:
						a.runtimePage(c)
					case spAbout:
						a.aboutPage(c)
					default:
						a.generalPage(c)
					}
				})
			})
		})
	})
}

// pageAbout is the sentence under a page's title.
func (a *App) pageAbout() string {
	switch a.settings.page {
	case spRuntime:
		return L.RuntimeAbout()
	case spAbout:
		return L.AboutAbout()
	}
	return L.GeneralAbout()
}

// settingsNav is search and the list of pages, or the pages search found.
func (a *App) settingsNav(c *ui.Context) {
	p := pal(c)
	ui.Column(c).Width(248).Shrink(0).Padding(14, 12).Gap(2).Background(p.subtle).BorderWidth(0, 1, 0, 0).BorderColor(p.border).Children(func() {
		ui.SearchField(c.Key("set-search"), &a.settings.query).Placeholder(L.SearchSettings()).Margin(0, 0, 12, 0)
		words := strings.Fields(strings.ToLower(a.settings.query))
		found := func(pg settingsPage) bool {
			text := strings.ToLower(pg.name() + " " + pg.words())
			for _, w := range words {
				if !strings.Contains(text, w) {
					return false
				}
			}
			return true
		}
		item := func(key string, pg settingsPage) {
			if !found(pg) {
				return
			}
			on := a.settings.page == pg
			b := ui.ButtonBase(c.Key(key)).FillWidth().Height(34).Padding(0, 10).Radius(radiusCtl).Selected(on).Label(pg.name())
			switch {
			case on:
				b.Background(p.tint)
			case b.Hovered():
				b.Background(p.hover)
			}
			b.Children(func() {
				t := ui.Text(c, pg.name())
				if on {
					t.FontWeight(600).TextColor(p.accentText)
				}
			})
			if b.Clicked() {
				a.settings.page = pg
			}
		}
		group := func(name string) {
			if len(words) == 0 {
				muted(c, name, fontSmall).FontWeight(600).Padding(10, 10, 4, 10)
			}
		}
		group(L.GroupPreferences())
		item("set-nav-general", spGeneral)
		group(L.GroupRunning())
		item("set-nav-runtime", spRuntime)
		if len(words) == 0 {
			b := ui.ButtonBase(c.Key("set-nav-diag")).FillWidth().Height(34).Padding(0, 10).Radius(radiusCtl).Label(L.Diagnostics())
			if b.Hovered() {
				b.Background(p.hover)
			}
			b.Children(func() {
				ui.Row(c).FillWidth().AlignItems(ui.Center).Children(func() {
					ui.Text(c, L.Diagnostics()).Grow(1)
					ui.Icon(c, icWindow).FontSize(14).TextColor(p.muted)
				})
			})
			if b.Clicked() {
				a.settings.open = false
				a.showTab(tabDiag, false)
			}
		}
		group("Windows-ARM64.apk")
		item("set-nav-about", spAbout)
		if len(words) > 0 && !found(spGeneral) && !found(spRuntime) && !found(spAbout) {
			muted(c, L.NoSettingsFound(), 13).Padding(6, 10)
		}
	})
}

// rows builds a section's rounded list of rows.
type rows struct{ n int }

// row is an icon tile, a title over one sentence, and its control.
func (r *rows) row(c *ui.Context, key string, icon *ui.SVG, title, hint string, control func()) {
	p := pal(c)
	e := ui.Row(c.Key(key)).FillWidth().Gap(14).Padding(12, 16)
	if r.n > 0 {
		e.BorderWidth(1, 0, 0, 0).BorderColor(p.hover)
	}
	r.n++
	e.Children(func() {
		tile(c, icon, 32, p.hover, p.text)
		ui.Column(c).Grow(1).MinWidth(0).Children(func() {
			ui.Text(c, title).FontWeight(600)
			if hint != "" {
				muted(c, hint, 13).Wrap()
			}
		})
		ui.Row(c).Gap(8).Shrink(0).Children(control)
	})
}

// section is a muted title over a rounded list of rows.
func section(c *ui.Context, title string, build func(r *rows)) {
	p := pal(c)
	ui.Column(c).Gap(8).MaxWidth(720).FillWidth().Children(func() {
		if title != "" {
			muted(c, title, 13).FontWeight(600)
		}
		ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Children(func() {
			build(&rows{})
		})
	})
}

func (a *App) generalPage(c *ui.Context) {
	section(c, L.SectionOpening(), func(r *rows) {
		r.row(c, "set-opens", icWindow, L.NewAppsOpen(), L.NewAppsOpenHint(), func() {
			cur := 0
			if a.prefs.OpenInWindow {
				cur = 1
			}
			if ui.Segmented(c.Key("set-opens-seg"), &cur, L.AsPane(), L.InOwnWindows()).Label(L.NewAppsOpen()).Changed() {
				pr := a.prefs
				pr.OpenInWindow = cur == 1
				a.setPrefs(pr)
			}
		})
		r.row(c, "set-close", icPower, L.WhenClosing(), L.WhenClosingHint(), func() {
			opts := []string{L.KeepWindowsRunning(), L.StopEveryApp()}
			cur := opts[0]
			if a.prefs.CloseStopsAll {
				cur = opts[1]
			}
			if ui.Select(c.Key("set-close-sel"), &cur, opts).Label(L.WhenClosing()).MinWidth(260).Changed() {
				pr := a.prefs
				pr.CloseStopsAll = cur == opts[1]
				a.setPrefs(pr)
			}
		})
		r.row(c, "set-ask", icInfo, L.AskFirst(), L.AskFirstHint(), func() {
			pr := a.prefs
			if ui.Switch(c.Key("set-ask-sw"), &pr.AskOnClose).Label(L.AskFirst()).Changed() {
				a.setPrefs(pr)
			}
		})
	})
	section(c, L.SectionStorage(), func(r *rows) {
		r.row(c, "set-data", icFolder, L.AppData(), a.appDataDir()+"\n"+L.AppDataTotalHint(mib(a.totalDataBytes)), func() {
			if button(c, "set-data-open", L.OpenFolder(), nil).Clicked() {
				dir := a.appDataDir()
				_ = os.MkdirAll(dir, 0o700)
				a.openPath(dir)
			}
			running := len(a.paneApps())+len(a.windowApps()) > 0
			b := button(c, "set-data-move", L.Move(), nil).Disabled(running)
			if running {
				b.Tooltip(L.MoveNeedsStopped())
			}
			if b.Clicked() {
				a.chooseAppData()
			}
		})
		running := len(a.paneApps())+len(a.windowApps()) > 0
		r.row(c, "set-code", icCode, L.ConvertedCode(), L.ConvertedCodeHint(mib(a.codeBytes)), func() {
			if button(c, "set-code-clear", L.Clear(), nil).Disabled(running).Clicked() {
				root := a.appDataDir()
				a.async(func() {
					_ = winapk.ClearConvertedCode(root)
					a.later(func() {
						a.codeBytes = 0
						a.refreshStorage()
						a.setToast(L.ClearDone())
					})
				})
			}
		})
	})
	section(c, L.SectionLook(), func(r *rows) {
		r.row(c, "set-lang", icLanguage, L.Language(), L.LanguageHint(), func() {
			tags := []string{"", "en", "zh-Hans"}
			opts := []string{L.FollowWindows(), "English", "简体中文"}
			cur := opts[0]
			for i, t := range tags {
				if t == a.prefs.Language {
					cur = opts[i]
				}
			}
			if ui.Select(c.Key("set-lang-sel"), &cur, opts).Label(L.Language()).MinWidth(180).Changed() {
				for i, o := range opts {
					if o == cur {
						pr := a.prefs
						pr.Language = tags[i]
						a.setPrefs(pr)
						if a.window != nil {
							mygo.App.SetLocale(tags[i])
						}
					}
				}
			}
		})
		r.row(c, "set-theme", icContrast, L.Appearance(), L.AppearanceHint(), func() {
			themes := []string{"system", "light", "dark"}
			cur := 0
			for i, t := range themes {
				if t == a.prefs.Theme {
					cur = i
				}
			}
			if ui.Segmented(c.Key("set-theme-seg"), &cur, L.ThemeSystem(), L.ThemeLight(), L.ThemeDark()).Label(L.Appearance()).Changed() {
				pr := a.prefs
				pr.Theme = themes[cur]
				a.setPrefs(pr)
				if a.window != nil {
					ApplyTheme(pr.Theme)
				}
			}
		})
	})
}

// chooseAppData asks for the folder apps keep their data in and migrates
// existing app data, converted code and logs there when all apps are stopped.
func (a *App) chooseAppData() {
	parent := a.window
	oldDir := a.appDataDir()
	a.async(func() {
		paths, err := mygo.Dialog.Open(mygo.OpenDialogOptions{Parent: parent, Title: L.AppData(), Directory: true, CreateDirectories: true})
		if err != nil || len(paths) == 0 {
			return
		}
		dst := paths[0]
		merr := winapk.MoveDataDir(oldDir, dst)
		a.later(func() {
			if merr != nil {
				a.setToast(L.ToastFailed())
				return
			}
			pr := a.prefs
			pr.AppData = dst
			a.setPrefs(pr)
			a.refreshStorage()
		})
	})
}

// javaDir is the folder of Windows-ARM64.apk's own Java runtime.
func javaDir() string {
	return winapk.DefaultRuntimeDir()
}

// chooseJavaFolder asks for a Java runtime's folder, then checks it.
func (a *App) chooseJavaFolder() {
	parent := a.window
	a.async(func() {
		paths, err := mygo.Dialog.Open(mygo.OpenDialogOptions{Parent: parent, Title: L.UseFolder(), Directory: true})
		if err != nil || len(paths) == 0 {
			return
		}
		a.later(func() {
			pr := a.prefs
			pr.JavaHome = paths[0]
			a.setPrefs(pr)
			a.check()
		})
	})
}

func (a *App) runtimePage(c *ui.Context) {
	p := pal(c)
	j := a.report.Java
	section(c, L.JavaRuntime(), func(r *rows) {
		r.row(c, "rt-use", icJava, L.JavaUse(), L.JavaUseHint(), func() {
			ownLabel := L.JavaOwn()
			if j.Found && j.Own {
				ownLabel = L.JavaOwnInstalled()
			}
			opts := []string{ownLabel, L.JavaFolder()}
			cur := opts[0]
			if a.prefs.JavaHome != "" {
				cur = opts[1]
			}
			if ui.Select(c.Key("rt-use-sel"), &cur, opts).Label(L.JavaUse()).MinWidth(300).Changed() {
				if cur == opts[1] {
					a.chooseJavaFolder()
				} else {
					pr := a.prefs
					pr.JavaHome = ""
					a.setPrefs(pr)
					a.check()
				}
			}
		})
		hint := L.JavaNotFound()
		switch {
		case j.Found && j.Own && j.InstalledAt != "":
			hint = L.JavaInstalledOn(j.InstalledAt)
		case j.Found && j.Own:
			hint = L.JavaOwnHint()
		case j.Found:
			hint = j.Path
		}
		title := L.JavaRuntime()
		if j.Found {
			title = "OpenJDK " + j.Version + " · " + arch(j.Native)
		}
		ui.Row(c.Key("rt-java")).FillWidth().Gap(14).Padding(12, 16).BorderWidth(1, 0, 0, 0).BorderColor(p.hover).AlignItems(ui.Center).Children(func() {
			if j.Found {
				tile(c, icCheck, 32, p.successBg, p.success)
			} else {
				tile(c, icWarn, 32, p.warnBg, p.warn)
			}
			ui.Column(c).Grow(1).MinWidth(0).Children(func() {
				ui.Text(c, title).FontWeight(600)
				muted(c, hint, 13).Wrap()
			})
			if a.prefs.JavaHome == "" {
				label := L.GetJava()
				if j.Found && j.Own {
					if button(c, "rt-update", L.Update(), nil).Disabled(a.install.running).Clicked() {
						a.installJava()
					}
					label = L.Reinstall()
				}
				if button(c, "rt-reinstall", label, nil).Disabled(a.install.running).Clicked() {
					a.installJava()
				}
			}
		})
		r.row(c, "rt-keep", icRotate, L.KeepUpdated(), L.KeepUpdatedHint(), func() {
			pr := a.prefs
			if ui.Switch(c.Key("rt-keep-sw"), &pr.JavaUpdates).Label(L.KeepUpdated()).Disabled(a.prefs.JavaHome != "").Changed() {
				a.setPrefs(pr)
			}
		})
	})
	section(c, L.GraphicsTitle(), func(r *rows) {
		r.row(c, "rt-gfx", icCube, L.GraphicsDefault(), L.GraphicsHint(), func() {
			pr := a.prefs
			a.graphicsSelect(c, "rt-gfx-sel", &pr.Graphics, false, func() { a.setPrefs(pr) })
		})
	})
	section(c, L.AdvancedNative(), func(r *rows) {
		r.row(c, "rt-native", icShield, L.NewAppsNativeOff(), L.NewAppsNativeOffHint(), func() {
			chip(c, L.LevelExperimental(), toneWarn)
		})
		r.row(c, "rt-profile", icChip, L.DefaultProfile(), L.DefaultProfileHint(), func() {
			profiles := []winapk.Profile{winapk.N0, winapk.N1, winapk.N2}
			cur := 1
			for i, pr := range profiles {
				if pr == a.prefs.Profile {
					cur = i
				}
			}
			if ui.Segmented(c.Key("rt-profile-seg"), &cur, "N0", "N1", "N2").Label(L.DefaultProfile()).Changed() {
				pr := a.prefs
				pr.Profile = profiles[cur]
				a.setPrefs(pr)
			}
		})
	})
	ui.Row(c).Gap(4).Children(func() {
		ui.Text(c, L.SupportsNow()).FontSize(13).TextColor(p.muted)
		if link(c, "rt-diag", L.Diagnostics()).Clicked() {
			a.settings.open = false
			a.showTab(tabDiag, false)
		}
	})
}

// arch names an architecture as Diagnostics does.
func arch(native bool) string {
	if native {
		return "ARM64"
	}
	return L.Emulated()
}

func (a *App) aboutPage(c *ui.Context) {
	ui.Row(c).MaxWidth(720).FillWidth().Gap(20).AlignItems(ui.Center).Children(func() {
		mark(c, 72)
		ui.Column(c).Grow(1).MinWidth(0).Gap(4).Children(func() {
			ui.Text(c, "Windows-ARM64.apk").FontSize(24).FontWeight(600)
			muted(c, L.AboutAbout(), fontBody).Wrap()
			ui.Row(c).Gap(8).AlignItems(ui.Center).Children(func() {
				muted(c, L.VersionRow(version.Version)+" · "+platformName(), 13)
				chip(c, L.LevelExperimental(), toneWarn)
			})
		})
		if button(c, "ab-copy", L.CopyVersion(), icDoc).Clicked() {
			c.Services().WriteClipboard("Windows-ARM64.apk " + version.Version + " (" + platformName() + ", runtime " + a.report.Version + ")")
			a.setToast(L.Copied())
		}
	})
	section(c, L.WhereFrom(), func(r *rows) {
		r.row(c, "ab-based", icInfo, L.BasedOn(), L.BasedOnHint(), func() {
			if button(c, "ab-macapk", "Mac.apk", icWindow).Clicked() {
				a.openURL("https://github.com/kksimp/Mac.apk")
			}
		})
		r.row(c, "ab-source", icCode, L.SourceCode(), L.SourceCodeHint(), func() {
			if button(c, "ab-source-open", L.Open(), icWindow).Clicked() {
				a.openURL(sourceURL)
			}
		})
	})
	section(c, L.Licenses(), func(r *rows) {
		r.row(c, "ab-agpl", icDoc, "GNU Affero General Public License 3.0", L.LicenseHint(), func() {
			if button(c, "ab-agpl-view", L.View(), nil).Clicked() {
				a.openLocalDoc("GNU Affero General Public License 3.0", "LICENSE", sourceURL+"/blob/main/LICENSE")
			}
		})
		r.row(c, "ab-notices", icDoc, L.Notices(), L.NoticesHint(), func() {
			if button(c, "ab-notices-view", L.View(), nil).Clicked() {
				a.openLocalDoc(L.Notices(), "THIRD_PARTY_NOTICES.md", sourceURL+"/blob/main/THIRD_PARTY_NOTICES.md")
			}
		})
	})
	section(c, L.Updates(), func(r *rows) {
		if mygo.Updater.Enabled() {
			r.row(c, "ab-updates", icInfo, L.SignedUpdates(), L.SignedUpdatesHint(), func() {
				if a.appUpdate != nil {
					if primary(c, "ab-update-install", L.InstallUpdate(a.appUpdate.Version), nil).Disabled(a.appUpdateBusy).Clicked() {
						a.installAppUpdate()
					}
				} else {
					if button(c, "ab-update-check", L.CheckUpdates(), nil).Disabled(a.appUpdateBusy).Clicked() {
						a.checkAppUpdate()
					}
				}
				if button(c, "ab-releases", L.Releases(), icWindow).Clicked() {
					a.openURL(sourceURL + "/releases")
				}
			})
			return
		}
		r.row(c, "ab-updates", icInfo, L.NoAutoUpdates(), L.NoAutoUpdatesHint(), func() {
			if button(c, "ab-releases", L.Releases(), icWindow).Clicked() {
				a.openURL(sourceURL + "/releases")
			}
		})
	})
}

func (a *App) checkAppUpdate() {
	if a.appUpdateBusy {
		return
	}
	a.appUpdateBusy = true
	a.async(func() {
		ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
		defer cancel()
		upd, err := mygo.Updater.Check(ctx)
		a.later(func() {
			a.appUpdateBusy = false
			if err != nil {
				a.setToast(L.ToastFailed())
				return
			}
			a.appUpdate = upd
			if upd == nil {
				a.setToast(L.AppUpToDate(version.Version))
			}
		})
	})
}

func (a *App) installAppUpdate() {
	if a.appUpdateBusy || a.appUpdate == nil {
		return
	}
	upd := a.appUpdate
	a.appUpdateBusy = true
	a.async(func() {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
		defer cancel()
		err := upd.Install(ctx, nil)
		a.later(func() {
			a.appUpdateBusy = false
			if err != nil {
				a.setToast(L.ToastFailed())
				return
			}
			mygo.App.Relaunch()
		})
	})
}

// openLocalDoc shows filename in a local dialog if bundled on disk, else opens fallbackURL.
func (a *App) openLocalDoc(title, filename, fallbackURL string) {
	var candidates []string
	if exe, err := os.Executable(); err == nil {
		dir := filepath.Dir(exe)
		candidates = append(candidates, filepath.Join(dir, filename), filepath.Join(filepath.Dir(dir), filename))
	}
	candidates = append(candidates, filename)
	for _, p := range candidates {
		if b, err := os.ReadFile(p); err == nil && len(b) > 0 {
			a.docView = docViewState{open: true, title: title, body: string(b)}
			return
		}
	}
	a.openURL(fallbackURL)
}

// sourceURL is where the source code lives.
const sourceURL = "https://github.com/YuanZhe-99/Windows-ARM64.apk"

// openURL opens a page in the browser.
func (a *App) openURL(url string) {
	a.async(func() { _ = mygo.Shell.OpenExternal(url) })
}

// platformName is the OS and architecture this build runs on.
func platformName() string {
	osName := map[string]string{"windows": "Windows", "darwin": "macOS", "linux": "Linux"}[runtime.GOOS]
	cpu := map[string]string{"arm64": "ARM64", "amd64": "x64"}[runtime.GOARCH]
	return strings.TrimSpace(osName + " " + cpu)
}
