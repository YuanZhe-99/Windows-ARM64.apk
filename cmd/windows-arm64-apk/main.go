// windows-arm64-apk is Windows-ARM64.apk's native interface: its main window,
// apps in panes or standalone windows, Settings and Diagnostics
// (design/, doc/en-us/gui.md). By default it connects to the native ARM64
// `winapk.exe` runtime; setting WINAPK_DEMO=1 runs against winapk.Mock with
// the design's sample apps (and WINAPK_FRESH=1 starts the demo empty).
package main

import (
	"log"
	"os"
	"path/filepath"
	"time"

	"github.com/egoist/mygo"

	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/gui"
	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/version"
	"github.com/YuanZhe-99/Windows-ARM64.apk/internal/winapk"
	_ "github.com/YuanZhe-99/Windows-ARM64.apk/locales" // the catalogs make their languages the app's
)

func main() {
	mygo.App.SetName("Windows-ARM64.apk")
	mygo.App.SetVersion(version.Version)
	if !mygo.App.RequestSingleInstanceLock() {
		return
	}
	base, err := os.UserConfigDir()
	if err != nil {
		log.Fatal(err)
	}

	demo := os.Getenv("WINAPK_DEMO") != ""
	var client winapk.Client
	var dir string
	var samples []winapk.APK
	if demo {
		fresh := os.Getenv("WINAPK_FRESH") != ""
		mock := winapk.NewMock(!fresh, 400*time.Millisecond)
		client = mock
		dir = filepath.Join(base, "Windows-ARM64.apk", "demo")
		if !fresh {
			samples = mock.Samples()
		}
	} else {
		client = winapk.NewRuntime("")
		dir = filepath.Join(base, "Windows-ARM64.apk")
	}

	a := gui.New(client, dir, demo)
	if len(samples) > 0 {
		a.AddSamples(samples)
	}
	gui.ApplyTheme(a.Prefs().Theme)
	if lang := a.Prefs().Language; lang != "" {
		mygo.App.SetLocale(lang)
	}
	// An APK named on the command line, or given to a second launch, opens
	// Add an app.
	mygo.App.OnSecondInstance(func(args []string, cwd string) {
		a.Focus()
		if len(args) > 1 {
			a.OpenFiles(args[1:], cwd)
		}
	})
	mygo.App.WhenReady(func() {
		a.Focus()
		a.Start()
		a.OpenFiles(os.Args[1:], "")
	})
	if err := mygo.App.Run(); err != nil {
		log.Fatal(err)
	}
	a.Stop()
}
