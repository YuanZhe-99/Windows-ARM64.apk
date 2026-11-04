package gui

import (
	"encoding/json"
	"os"
	"strings"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	L "github.com/YuanZhe-99/Windows-ARM64.apk/locales"
)

// diagState is the Diagnostics pane's page and the app whose log shows.
type diagState struct {
	page int // 0 this PC, 1 app logs
	app  string
}

// diagView is Diagnostics as a pane (design/Diagnostics.html): this PC as
// doctor reports it, or an app's log with its error category. Only here
// may codes, paths and categories show.
func (a *App) diagView(c *ui.Context) {
	ui.Column(c.Key("diag")).Fill().Children(func() {
		ui.Row(c).FillWidth().Padding(14, 20, 10, 20).Gap(8).Shrink(0).AlignItems(ui.Center).Children(func() {
			ui.Tabs(c.Key("diag-tabs"), &a.diag.page, L.ThisPC(), L.AppLogs())
			ui.Spacer(c)
			if a.diag.page == 0 {
				if button(c, "diag-check", L.CheckAgain(), nil).Disabled(a.checking).Clicked() {
					a.check()
				}
				if button(c, "diag-copy", L.CopyReport(), nil).Clicked() {
					b, _ := json.MarshalIndent(a.report, "", "  ")
					c.Services().WriteClipboard(string(b))
					a.setToast(L.Copied())
				}
				if button(c, "diag-save", L.SaveJSON(), nil).Clicked() {
					a.saveReport()
				}
			} else if s := a.open[a.diag.app]; s != nil {
				if button(c, "diag-copylog", L.CopyLog(), nil).Clicked() {
					c.Services().WriteClipboard(strings.Join(s.state.Log, "\n"))
					a.setToast(L.Copied())
				}
			}
		})
		ui.Scroll(c.Key("diag-body")).Grow(1).FillWidth().Padding(4, 20, 20, 20).Gap(16).Children(func() {
			if a.diag.page == 0 {
				a.thisPC(c)
			} else {
				a.appLogs(c)
			}
		})
	})
}

// kv is a field of a report: its name and its value in the mono face.
func kv(c *ui.Context, name, value string) {
	ui.Row(c).FillWidth().Gap(12).Padding(3, 0).Children(func() {
		muted(c, name, 13).Width(150).Shrink(0)
		ui.Text(c, value).Font(fontMono).FontSize(fontSmall).Grow(1).Wrap().Selectable()
	})
}

// diagCard is a titled card of fields with a chip.
func diagCard(c *ui.Context, title, word string, t tone, body func()) {
	p := pal(c)
	ui.Column(c).FillWidth().Gap(6).Padding(14, 16).Border(1, p.border).Radius(radiusCard).Children(func() {
		ui.Row(c).FillWidth().Gap(8).Children(func() {
			ui.Text(c, title).FontWeight(600).Grow(1)
			if word != "" {
				chip(c, word, t)
			}
		})
		body()
	})
}

// thisPC is the doctor report.
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
	head := L.HeadNotReady()
	if r.Ready() {
		head = L.HeadReady()
	}
	ui.Column(c).Gap(4).Children(func() {
		ui.Text(c, head).FontSize(18).FontWeight(600)
		muted(c, L.CheckedAt(a.checked.Format("15:04")), 13)
	})
	diagCard(c, "Windows", L.NativeARM64(), toneGood, func() {
		kv(c, L.OSBuild(), r.Machine.Windows)
		kv(c, "CPU", r.Machine.CPU)
		kv(c, L.ThisProcess(), platformName())
	})
	jw, jt := L.JavaMissingChip(), toneBad
	if r.Java.Found {
		jw, jt = L.Found(), toneGood
	}
	diagCard(c, L.JavaRuntime(), jw, jt, func() {
		if !r.Java.Found {
			muted(c, L.JavaNotFound(), 13)
			return
		}
		kv(c, L.Vendor(), r.Java.Vendor)
		kv(c, L.Version(), r.Java.Version)
		kv(c, L.Architecture(), arch(r.Java.Native))
		kv(c, L.Location(), r.Java.Path)
	})
	for _, f := range r.Features {
		if f.ID == "java" {
			continue
		}
		word, t := L.LevelNotYet(), toneNeutral
		switch f.Level {
		case winapk.Works:
			word, t = L.LevelWorks(), toneGood
		case winapk.Experimental:
			word, t = L.LevelExperimental(), toneWarn
		}
		diagCard(c, featureName(f.ID), word, t, func() { kv(c, "doctor", f.Detail) })
	}
	diagCard(c, "GPU", "", 0, func() {
		kv(c, L.Device(), r.Machine.GPU)
		kv(c, L.Driver(), r.Machine.Driver)
		kv(c, "Vulkan", r.Machine.Vulkan)
	})
	sectionTitle(c, L.CantRunYetTitle())
	for _, s := range []string{L.Cant32(), L.CantVulkan(), L.CantDevices(), L.CantPlay(), L.CantOutside(), L.CantSandbox()} {
		ui.Row(c).Gap(8).Children(func() {
			ui.Text(c, "•").TextColor(p.muted)
			ui.Text(c, s).FontSize(13).Wrap()
		})
	}
}

// featureName names a doctor feature.
func featureName(id string) string {
	switch id {
	case "native-n0":
		return L.ProfileN0()
	case "native-n1":
		return L.ProfileN1()
	case "native-n2":
		return L.ProfileN2()
	case "gles":
		return "OpenGL ES (ANGLE)"
	case "vulkan":
		return L.VulkanForApps()
	}
	return id
}

// appLogs is an open app's log, with the category of a failure.
func (a *App) appLogs(c *ui.Context) {
	p := pal(c)
	var ids []string
	var names []string
	for _, e := range a.library {
		if a.open[e.ID] != nil {
			ids = append(ids, e.ID)
			names = append(names, e.APK.Label)
		}
	}
	if len(ids) == 0 {
		muted(c, L.NoLogs(), fontBody).Wrap()
		return
	}
	cur := ""
	for i, id := range ids {
		if id == a.diag.app {
			cur = names[i]
		}
	}
	if cur == "" {
		a.diag.app, cur = ids[0], names[0]
	}
	if ui.Select(c.Key("diag-app"), &cur, names).Label(L.AppLogs()).MinWidth(220).Changed() {
		for i, n := range names {
			if n == cur {
				a.diag.app = ids[i]
			}
		}
	}
	s := a.open[a.diag.app]
	if s == nil {
		return
	}
	if s.state.Phase == winapk.Failed {
		diagCard(c, L.StoppedWhileStarting(), s.state.Category, toneBad, func() {
			muted(c, failedWords(s.state.Category), 13).Wrap()
		})
	}
	ui.Column(c).FillWidth().Padding(10, 12).Radius(radiusCard).Background(p.subtle).Gap(2).Children(func() {
		for _, line := range s.state.Log {
			t := ui.Text(c, line).Font(fontMono).FontSize(fontSmall).Wrap().Selectable()
			if strings.Contains(line, "error") {
				t.TextColor(p.danger)
			}
		}
	})
	muted(c, L.LogsPrivate(), 13).Wrap()
}

// saveReport writes the doctor report as JSON where the person chooses.
func (a *App) saveReport() {
	parent := a.window
	b, _ := json.MarshalIndent(a.report, "", "  ")
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
