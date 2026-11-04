// windows-arm64-apk is Windows-ARM64.apk's interface: its main window,
// apps in panes or windows of their own, Settings and Diagnostics
// (design/, doc/en-us/gui.md). Until the runtime exists it runs against
// winapk.Mock, which shows sample apps and says so; WINAPK_FRESH=1 starts
// with an empty library and no Java runtime, as on a first run.
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
	fresh := os.Getenv("WINAPK_FRESH") != ""
	mock := winapk.NewMock(!fresh, 400*time.Millisecond)
	// The sample library is the demo's: it stays in memory, and its
	// settings in a folder of their own.
	dir := filepath.Join(base, "Windows-ARM64.apk", "demo")
	a := gui.New(mock, dir, true)
	if !fresh {
		a.AddSamples(mock.Samples())
	}
	gui.ApplyTheme(a.Prefs().Theme)
	if lang := a.Prefs().Language; lang != "" {
		mygo.App.SetLocale(lang)
	}
	mygo.App.OnSecondInstance(func([]string, string) { a.Focus() })
	mygo.App.WhenReady(func() {
		a.Focus()
		a.Start()
	})
	if err := mygo.App.Run(); err != nil {
		log.Fatal(err)
	}
	a.Stop()
}
