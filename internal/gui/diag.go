package gui

import (
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// diagState is the Diagnostics pane's page and the app whose log shows.
type diagState struct {
	page       int // 0 this PC, 1 app logs
	app        string
	run        int // the app's run shown, 0 the newest
	errorsOnly bool
}

// openDiagWindow moves Diagnostics out of its pane into a standalone window.
func (a *App) openDiagWindow() {
	a.removeTab(tabDiag)
	if a.diagWin != nil {
		a.diagWin.Show()
		a.diagWin.Focus()
		return
	}
	w := mygo.NewWindow(mygo.WindowOptions{
		Title: L.Diagnostics() + " — Windows-ARM64.apk", Width: 980, Height: 720, MinWidth: 640, MinHeight: 480,
		TitleBarStyle: mygo.TitleBarHidden, HideWindowButtons: true,
		Content: ui.View(func(c *ui.Context) {
			applyTheme(c)
			p := pal(c)
			ui.Column(c.Key("diag-own")).Fill().Background(p.pane).TextColor(p.text).FontSize(fontBody).Children(func() {
				ui.Row(c.Key("diag-own-head")).Height(headerH).Shrink(0).FillWidth().Padding(0, 6, 0, 12).Gap(8).AlignItems(ui.Center).BorderWidth(0, 0, 1, 0).BorderColor(p.border).Children(func() {
					tile(c, icPulse, 24, p.hover, p.text)
					ui.Text(c, L.Diagnostics()).FontWeight(600).NoWrap()
					ui.Box(c).Grow(1).FillHeight().DragWindow()
					if iconButton(c, "diag-dock", icDockIn, L.BackToMain()).Clicked() {
						dw := a.diagWin
						a.diagWin = nil
						a.showTab(tabDiag, false)
						if dw != nil {
							dw.Close()
						}
					}
					windowButtons(c, "diag-win", a.diagWin)
				})
				a.diagView(c)
			})
		}),
	})
	w.OnClose(func(*mygo.CloseEvent) {
		if a.diagWin == w {
			a.diagWin = nil
		}
	})
	_ = w.SetIcon(logoPNG)
	a.diagWin = w
	w.Show()
	w.Focus()
}

// diagView is Diagnostics as a pane (design/Diagnostics.html,
// DiagnosticsLogs.html): this PC as doctor reports it, or an app's log with
// its error category. Only here may codes, paths and categories show.
func (a *App) diagView(c *ui.Context) {
	ui.Column(c.Key("diag")).Fill().Children(func() {
		ui.Row(c).FillWidth().Padding(14, 20, 10, 20).Gap(8).Shrink(0).AlignItems(ui.Center).Children(func() {
			ui.Segmented(c.Key("diag-tabs"), &a.diag.page, L.ThisPC(), L.AppLogs()).Label(L.Diagnostics())
			ui.Spacer(c)
			if a.diag.page == 0 {
				if button(c, "diag-check", L.CheckAgain(), icRotate).Disabled(a.checking).Clicked() {
					a.check()
				}
				if button(c, "diag-copy", L.CopyReport(), icDoc).Clicked() {
					c.Services().WriteClipboard(string(a.reportJSON()))
					a.setToast(L.Copied())
				}
				if button(c, "diag-save", L.SaveJSON(), nil).Clicked() {
					a.saveReport()
				}
			} else {
				r := a.diagRun()
				if button(c, "diag-copylog", L.CopyLog(), icDoc).Disabled(r == nil).Clicked() && r != nil {
					c.Services().WriteClipboard(strings.Join(logLines(r.state.Log, a.diag.errorsOnly), "\n"))
					a.setToast(L.Copied())
				}
				if button(c, "diag-logdir", L.OpenLogFolder(), icFolder).Clicked() {
					dir := winapk.LogsDir(a.appDataDir())
					a.async(func() {
						_ = os.MkdirAll(dir, 0o700)
						_ = mygo.Shell.OpenPath(dir)
					})
				}
				if button(c, "diag-savelog", L.SaveLog(), nil).Disabled(r == nil).Clicked() && r != nil {
					a.saveLog(r)
				}
			}
		})
		ui.Scroll(c.Key("diag-body")).Grow(1).FillWidth().Padding(4, 20, 20, 20).Gap(14).Children(func() {
			if a.diag.page == 0 {
				a.thisPC(c)
			} else {
				a.appLogs(c)
			}
		})
	})
}

func (a *App) reportJSON() []byte {
	if len(a.report.RawJSON) > 0 {
		return a.report.RawJSON
	}
	b, _ := json.MarshalIndent(a.report, "", "  ")
	return b
}

// kv is a field of a report: its name and its value in the mono face.
func kv(c *ui.Context, name, value string) {
	ui.Row(c).FillWidth().Gap(10).Padding(2, 0).AlignItems(ui.Start).Children(func() {
		muted(c, name, fontSmall).Width(118).Shrink(0)
		ui.Text(c, value).Font(fontMono).FontSize(fontSmall).Grow(1).Wrap().Selectable()
	})
}

// diagCard is a titled card of fields with one or two chips.
func diagCard(c *ui.Context, title string, chips []struct {
	word string
	t    tone
}, body func()) ui.Element {
	p := pal(c)
	card := ui.Column(c).FillWidth().Gap(6).Padding(14, 16).Border(1, p.border).Radius(radiusCard).Background(p.pane)
	card.Children(func() {
		ui.Row(c).FillWidth().Gap(6).AlignItems(ui.Center).Children(func() {
			ui.Text(c, title).FontWeight(600).Grow(1)
			for _, ch := range chips {
				if ch.word != "" {
					chip(c, ch.word, ch.t)
				}
			}
		})
		body()
	})
	return card
}

// thisPC renders the 3-column Diagnostics › This PC report (design/Diagnostics.html).
func (a *App) thisPC(c *ui.Context) {
	p := pal(c)
	r := a.report
	if a.checked.IsZero() {
		ui.Row(c).Gap(10).Children(func() {
			ui.Spinner(c)
			muted(c, L.StatusChecking(), fontBody)
		})
		return
	}
	if r.Demo {
		ui.Row(c).FillWidth().Gap(8).Padding(8, 12).Radius(radiusCtl).Background(p.subtle).Children(func() {
			ui.Icon(c, icInfo).FontSize(14).TextColor(p.muted)
			muted(c, L.DemoBanner(), fontSmall).Wrap()
		})
	}

	// Summary banner.
	ready := r.Ready()
	head, bt := L.HeadNotReady(), toneWarn
	if ready {
		head, bt = L.HeadReady(), toneGood
	}
	bfg, bbg := bt.colors(p)
	ui.Row(c).FillWidth().Gap(12).Padding(12, 16).Radius(radiusCard).Background(bbg).AlignItems(ui.Start).Children(func() {
		icon := icWarn
		if ready {
			icon = icCheck
		}
		ui.Icon(c, icon).FontSize(18).TextColor(bfg).Shrink(0)
		ui.Column(c).Grow(1).MinWidth(0).Gap(2).Children(func() {
			ui.Text(c, head).FontWeight(600).TextColor(bfg)
			muted(c, L.CheckedAt(a.checked.Format("15:04"))+" "+L.DiagHeadSub(), 13).Wrap()
		})
	})

	lv := func(id string) winapk.Level {
		for _, f := range r.Features {
			if f.ID == id {
				return f.Level
			}
		}
		return winapk.Unavailable
	}

	grid := ui.Grid(c.Key("diag-grid")).FillWidth().Gap(14)
	cols := 3
	if grid.LayoutBounds().W > 0 && grid.LayoutBounds().W < 780 {
		cols = 1
	}
	grid.Columns(cols).Children(func() {
		// 1. Windows
		osBuild := r.Machine.OSBuild
		if osBuild == "" {
			osBuild = r.Machine.Windows
		}
		osArch := r.Machine.OSArch
		if osArch == "" {
			osArch = "arm64"
		}
		procText := L.NotEmulated()
		if r.Machine.ProcessArch != "" && r.Machine.ProcessArch != "arm64" {
			procText = r.Machine.ProcessArch + " · " + L.Emulated()
		}
		diagCard(c, "Windows", []struct {
			word string
			t    tone
		}{{L.NativeARM64(), toneGood}}, func() {
			kv(c, L.OSBuild(), osBuild)
			kv(c, L.OSArch(), osArch)
			kv(c, L.ThisProcess(), procText)
			kv(c, L.RuntimeProcess(), L.OnePerApp())
		})

		// 2. Java runtime
		jw, jt := L.JavaMissingChip(), toneBad
		if r.Java.Found {
			jw, jt = L.Found(), toneGood
		}
		diagCard(c, L.JavaRuntime(), []struct {
			word string
			t    tone
		}{{jw, jt}}, func() {
			if !r.Java.Found {
				muted(c, L.JavaNotFound(), 13)
				return
			}
			kv(c, L.Vendor(), r.Java.Vendor)
			kv(c, L.Version(), r.Java.Version)
			kv(c, L.Architecture(), arch(r.Java.Native))
			kv(c, L.Location(), r.Java.Path)
		})

		// 3. App native code
		ntWord, ntTone := L.LevelNotYet(), toneNeutral
		if lv("native-n0") == winapk.Experimental {
			ntWord, ntTone = L.LevelExperimental(), toneWarn
		}
		var profParts []string
		for _, pName := range []string{"N0", "N1", "N2"} {
			st := string(lv("native-" + strings.ToLower(pName)))
			if st == "experimental" {
				st = "available"
			}
			profParts = append(profParts, pName+" "+st)
		}
		fw := "Mac.apk main"
		if r.Machine.FrameworkCommit != "" {
			fw += " · " + r.Machine.FrameworkCommit
		}
		diagCard(c, L.TodayNative(), []struct {
			word string
			t    tone
		}{{ntWord, ntTone}}, func() {
			kv(c, L.ProfilesLabel(), strings.Join(profParts, " · "))
			kv(c, L.DefaultLabel(), L.OffEveryApp())
			kv(c, L.UnsupportedABIs(), L.NotSupported())
			kv(c, L.FrameworkLabel(), fw)
		})

		// 4. Vulkan (spans 2 columns on wide layout)
		vkDrvChip, vkDrvTone := L.JavaMissingChip(), toneNeutral
		if r.Machine.VulkanLoader == "available" || r.Machine.Vulkan != "" {
			if r.Machine.VulkanTranslated {
				vkDrvChip, vkDrvTone = L.DriverTranslated(), toneWarn
			} else {
				vkDrvChip, vkDrvTone = L.DriverWorks(), toneGood
			}
		}
		loaderLine := "vulkan-1.dll"
		if r.Machine.VulkanInstance != "" {
			loaderLine += " " + r.Machine.VulkanInstance
		}
		if r.Machine.VulkanLoader != "" {
			loaderLine += " · " + r.Machine.VulkanLoader
		}
		devLine := r.Machine.GPU
		if r.Machine.VulkanDevice != "" {
			devLine = r.Machine.VulkanDevice
		}
		if r.Machine.Driver != "" {
			devLine += " · driver " + r.Machine.Driver
		}
		if r.Machine.Vulkan != "" {
			devLine += " · API " + r.Machine.Vulkan
		}
		wsiLine := "not supported"
		if r.Machine.Win32Surface {
			wsiLine = "supported"
			if r.Machine.Win32Present != "" {
				wsiLine += " · " + r.Machine.Win32Present
			}
		}
		vkCard := diagCard(c, "Vulkan", []struct {
			word string
			t    tone
		}{{vkDrvChip, vkDrvTone}, {L.AppsNotYet(), toneNeutral}}, func() {
			kv(c, "Loader", loaderLine)
			kv(c, L.Device(), devLine)
			kv(c, L.Win32Surface(), wsiLine)
			kv(c, L.VulkanForApps(), L.FeatVulkanHint()+" (guestVulkan: false)")
		})
		if cols == 3 {
			vkCard.ColumnSpan(2)
		}

		// 5. OpenGL ES (ANGLE)
		glWord, glTone := L.LevelNotYet(), toneNeutral
		if lv("gles") == winapk.Experimental {
			glWord, glTone = L.LevelExperimental(), toneWarn
		}
		d3d11Str, vkAngleStr, autoStr := "unavailable", "unavailable", "off"
		if r.Machine.AngleArm64 {
			d3d11Str = "available"
			if r.Machine.VulkanLoader == "available" || r.Machine.Vulkan != "" {
				vkAngleStr = "available"
				autoStr = "vulkan"
			} else {
				autoStr = "d3d11"
			}
		}
		diagCard(c, L.DiagGLESTitle(), []struct {
			word string
			t    tone
		}{{glWord, glTone}}, func() {
			kv(c, "Direct3D 11", d3d11Str)
			kv(c, "Vulkan", vkAngleStr)
			kv(c, L.AutoUses(), autoStr)
		})
	})

	// What this PC can't run yet (2-column bullet card).
	ui.Column(c).FillWidth().Padding(14, 16).Gap(8).Border(1, p.border).Radius(radiusCard).Background(p.pane).Children(func() {
		ui.Text(c, L.CantRunYetTitle()).FontWeight(600)
		bcols := 2
		if cols == 1 {
			bcols = 1
		}
		ui.Grid(c).Columns(bcols).Gap(6).FillWidth().Children(func() {
			for _, s := range []string{L.Cant32(), L.CantVulkan(), L.CantDevices(), L.CantPlay(), L.CantOutside(), L.CantSandbox()} {
				ui.Row(c).Gap(8).Children(func() {
					ui.Text(c, "•").TextColor(p.muted)
					muted(c, s, 13).Wrap()
				})
			}
		})
	})
}

// appLogs renders Diagnostics › App logs (design/DiagnosticsLogs.html).
func (a *App) appLogs(c *ui.Context) {
	p := pal(c)
	var ids, names []string
	for _, e := range a.library {
		if len(a.runs[e.ID]) > 0 {
			ids = append(ids, e.ID)
			names = append(names, e.APK.Label)
		}
	}
	if len(ids) == 0 {
		muted(c, L.NoLogs(), fontBody).Wrap()
		return
	}
	if len(a.runs[a.diag.app]) == 0 {
		a.diag.app, a.diag.run = ids[0], 0
	}

	curAppIdx := 0
	for i, id := range ids {
		if id == a.diag.app {
			curAppIdx = i
		}
	}
	curAppName := names[curAppIdx]

	runs := a.runs[a.diag.app]
	if a.diag.run < 0 || a.diag.run >= len(runs) {
		a.diag.run = 0
	}
	runOpts := make([]string, len(runs))
	for i, rn := range runs {
		runOpts[i] = runWords(rn)
	}
	curRunName := runOpts[a.diag.run]

	// Top selector toolbar: App select, Run select, Errors and warnings only.
	ui.Row(c).FillWidth().Gap(10).AlignItems(ui.Center).Children(func() {
		if ui.Select(c.Key("diag-app-sel"), &curAppName, names).Label(L.AppLabel()).MinWidth(220).Changed() {
			for i, nm := range names {
				if nm == curAppName {
					a.diag.app, a.diag.run = ids[i], 0
				}
			}
		}
		if ui.Select(c.Key("diag-run-sel"), &curRunName, runOpts).Label(L.RunLabel()).MinWidth(280).Changed() {
			for i, ro := range runOpts {
				if ro == curRunName {
					a.diag.run = i
				}
			}
		}
		ui.Spacer(c)
		ui.Checkbox(c.Key("diag-errors"), &a.diag.errorsOnly, L.ErrorsOnly())
	})

	r := a.diagRun()
	if r == nil {
		return
	}
	e := a.entry(a.diag.app)

	// 8-field failure summary card when the selected run failed.
	if r.state.Phase == winapk.Failed {
		ui.Column(c).FillWidth().Padding(14, 16).Gap(10).Radius(radiusCard).Background(p.dangerBg).Border(1, p.dangerBorder).Children(func() {
			ui.Row(c).FillWidth().Gap(8).AlignItems(ui.Center).Children(func() {
				ui.Icon(c, icCircleX).FontSize(18).TextColor(p.danger)
				title := L.StoppedWhileStarting()
				if strings.HasPrefix(r.state.Category, "GUEST_") || strings.HasPrefix(r.state.Category, "ELF_") {
					title = L.StoppedLoadingNative()
				}
				ui.Text(c, title).FontWeight(600).Grow(1)
				if r.state.Category != "" {
					chip(c, r.state.Category, toneBad)
				}
			})
			f := r.state.Failure
			stage := dashEmpty(f.Stage)
			mod := dashEmpty(f.Module)
			sym := dashEmpty(f.Symbol)
			where := dashEmpty(f.Where)
			prof := string(r.native)
			if r.native == winapk.NativeRestricted && r.profile != "" {
				prof = string(r.profile) + " (restricted)"
			} else if prof == "" {
				prof = "off"
			}
			gfx := dashEmpty(r.reqGfx)
			if r.state.Graphics != "" {
				gfx += " → " + r.state.Graphics
			}
			procStr := fmt.Sprintf("pid %d · exit %d", r.state.PID, r.state.ExitCode)
			apkSha := "—"
			if e != nil && e.APK.SHA256 != "" {
				sha := e.APK.SHA256
				if len(sha) > 12 {
					sha = sha[:6] + "…" + sha[len(sha)-4:]
				}
				apkSha = "sha256 " + sha
			}
			ui.Grid(c).Columns(2).Gap(4).FillWidth().Children(func() {
				kv(c, L.FailStage(), stage)
				kv(c, L.FailModule(), mod)
				kv(c, L.FailSymbol(), sym)
				kv(c, L.FailNeededAt(), where)
				kv(c, L.FailProfile(), prof)
				kv(c, L.FailGraphics(), gfx)
				kv(c, L.FailProcess(), procStr)
				kv(c, L.FailAPK(), apkSha)
			})
			muted(c, failedWords(r.state.Category), 13).Wrap()
		})
	}

	// 5-column monospace log table: Time | pid/tid | Level | Stage | Message.
	entries := logEntries(r.state, a.diag.errorsOnly)
	ui.Column(c).FillWidth().Border(1, p.border).Radius(radiusCard).Clip().Background(p.pane).Children(func() {
		ui.Row(c).FillWidth().Padding(8, 14).Gap(12).Background(p.subtle).BorderWidth(0, 0, 1, 0).BorderColor(p.hover).Children(func() {
			muted(c, L.ColTime(), fontSmall).Font(fontMono).FontWeight(600).Width(92).Shrink(0)
			muted(c, L.ColPID(), fontSmall).Font(fontMono).FontWeight(600).Width(104).Shrink(0)
			muted(c, L.ColLevel(), fontSmall).Font(fontMono).FontWeight(600).Width(56).Shrink(0)
			muted(c, L.ColStage(), fontSmall).Font(fontMono).FontWeight(600).Width(120).Shrink(0)
			muted(c, L.ColMessage(), fontSmall).Font(fontMono).FontWeight(600).Grow(1)
		})
		ui.Column(c).FillWidth().Padding(8, 14).Gap(4).Children(func() {
			if len(entries) == 0 {
				muted(c, L.NoLines(), fontSmall)
				return
			}
			for _, en := range entries {
				col := p.text
				weight := 400
				switch en.Level {
				case "error":
					col = p.danger
					weight = 600
				case "warn":
					col = p.warn
				}
				ui.Row(c).FillWidth().Gap(12).AlignItems(ui.Start).Children(func() {
					ui.Text(c, en.Time).Font(fontMono).FontSize(fontSmall).TextColor(col).Width(92).Shrink(0).Selectable()
					ui.Text(c, en.PIDTID).Font(fontMono).FontSize(fontSmall).TextColor(p.muted).Width(104).Shrink(0).Selectable()
					ui.Text(c, en.Level).Font(fontMono).FontSize(fontSmall).FontWeight(weight).TextColor(col).Width(56).Shrink(0).Selectable()
					ui.Text(c, en.Stage).Font(fontMono).FontSize(fontSmall).TextColor(p.muted).Width(120).Shrink(0).Selectable()
					ui.Text(c, en.Message).Font(fontMono).FontSize(fontSmall).TextColor(col).Grow(1).Wrap().Selectable()
				})
			}
		})
	})
	muted(c, L.LogsPrivate(), fontSmall).Wrap()
}

func dashEmpty(s string) string {
	if s == "" {
		return "—"
	}
	return s
}

func logEntries(st winapk.State, errorsOnly bool) []winapk.LogEntry {
	src := st.Entries
	if len(src) == 0 && len(st.Log) > 0 {
		pid := strconv.Itoa(st.PID)
		if st.PID == 0 {
			pid = "0"
		}
		for _, line := range st.Log {
			f := strings.Fields(line)
			en := winapk.LogEntry{PIDTID: pid + "/" + pid, Level: "info", Stage: "runtime", Message: line}
			if len(f) >= 4 {
				en.Time, en.Level, en.Stage = f[0], f[1], f[2]
				en.Message = strings.Join(f[3:], " ")
			}
			src = append(src, en)
		}
	}
	if !errorsOnly {
		return src
	}
	var out []winapk.LogEntry
	for _, en := range src {
		if en.Level == "error" || en.Level == "warn" {
			out = append(out, en)
		}
	}
	return out
}

// diagRun is the run Diagnostics shows, nil for none.
func (a *App) diagRun() *session {
	rs := a.runs[a.diag.app]
	if a.diag.run < 0 || a.diag.run >= len(rs) {
		return nil
	}
	return rs[a.diag.run]
}

// runWords says when a run started and how it ended.
func runWords(s *session) string {
	end := L.StateRunning()
	switch {
	case s.state.Phase == winapk.Failed:
		end = L.StateFailed()
	case s.state.Phase == winapk.Stopped && !s.stopAt.IsZero():
		end = L.StoppedByYou()
	case s.state.Phase == winapk.Stopped:
		end = L.StateStopped()
	case s.state.Phase == winapk.Starting:
		end = L.StateStarting()
	}
	return s.started.Format("15:04") + " · " + end
}

// logLevel is a log line's level, as the runtime writes it after the time.
func logLevel(line string) string {
	f := strings.Fields(line)
	if len(f) < 2 {
		return ""
	}
	return f[1]
}

// logLines are a run's lines, only its errors and warnings when errorsOnly.
func logLines(log []string, errorsOnly bool) []string {
	if !errorsOnly {
		return log
	}
	var out []string
	for _, l := range log {
		if lv := logLevel(l); lv == "error" || lv == "warn" {
			out = append(out, l)
		}
	}
	return out
}

// saveLog writes a run's log where the person chooses, for a bug report.
func (a *App) saveLog(r *session) {
	parent := a.window
	text := []byte(strings.Join(r.state.Log, "\r\n") + "\r\n")
	name := r.id + "-" + r.started.Format("20060102-150405") + ".log"
	a.async(func() {
		path, err := mygo.Dialog.Save(mygo.SaveDialogOptions{Parent: parent, Title: L.SaveLog(), DefaultPath: name})
		if err != nil || path == "" {
			return
		}
		if err := os.WriteFile(path, text, 0o644); err != nil {
			a.later(func() { a.setToast(L.ToastFailed()) })
		}
	})
}

// saveReport writes the doctor report as JSON where the person chooses.
func (a *App) saveReport() {
	parent := a.window
	b := a.reportJSON()
	a.async(func() {
		path, err := mygo.Dialog.Save(mygo.SaveDialogOptions{Parent: parent, Title: L.SaveJSON(), DefaultPath: "doctor.json"})
		if err != nil || path == "" {
			return
		}
		if err := os.WriteFile(path, b, 0o644); err != nil {
			a.later(func() { a.setToast(L.ToastFailed()) })
		}
	})
}
