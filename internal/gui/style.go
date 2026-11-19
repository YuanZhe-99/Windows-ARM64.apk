package gui

import (
	_ "embed"
	"fmt"
	"hash/fnv"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"
)

// logoPNG is Windows-ARM64.apk's 256x256 RGBA brand icon (tools/genicon).
//
//go:embed logo.png
var logoPNG []byte

// palette is the design's colours (design/README.md, "Tokens") for one
// appearance. Views take it from pal, never from literals of their own.
type palette struct {
	base, pane, border, subtle, hover, text, muted ui.Color
	// accent fills primary buttons with onAccent on it; accentText is the
	// accent as words and links; tint is the accent's pale background.
	accent, onAccent, accentText, tint, tintBorder ui.Color
	success, successBg                             ui.Color
	warn, warnBg, warnBorder                       ui.Color
	danger, dangerBg, dangerBorder                 ui.Color
	scrim, stage                                   ui.Color
}

var (
	lightPal = palette{
		base: ui.Hex("#E9EEF3"), pane: ui.Hex("#FFFFFF"), border: ui.Hex("#DCE2E8"), subtle: ui.Hex("#F6F8FA"), hover: ui.Hex("#EEF2F6"),
		text: ui.Hex("#1B1D21"), muted: ui.Hex("#4A5260"),
		accent: ui.Hex("#0072B5"), onAccent: ui.Hex("#FFFFFF"), accentText: ui.Hex("#0069A6"), tint: ui.Hex("#E3F1FA"), tintBorder: ui.Hex("#9CC9E6"),
		success: ui.Hex("#0F7B3F"), successBg: ui.Hex("#E7F5EC"),
		warn: ui.Hex("#8A4B00"), warnBg: ui.Hex("#FFF6E5"), warnBorder: ui.Hex("#F1D49B"),
		danger: ui.Hex("#B42318"), dangerBg: ui.Hex("#FDEDEB"), dangerBorder: ui.Hex("#F0B8B2"),
		scrim: ui.RGBA(20, 26, 34, 0.42), stage: ui.Hex("#EEF2F6"),
	}
	darkPal = palette{
		base: ui.Hex("#1A1C21"), pane: ui.Hex("#23262C"), border: ui.Hex("#33363E"), subtle: ui.Hex("#2A2D34"), hover: ui.Hex("#30333B"),
		text: ui.Hex("#EDEFF2"), muted: ui.Hex("#A9AFBA"),
		accent: ui.Hex("#4DAEE8"), onAccent: ui.Hex("#06131C"), accentText: ui.Hex("#7CC6F2"), tint: ui.Hex("#173A52"), tintBorder: ui.Hex("#2B5B7C"),
		success: ui.Hex("#4CC38A"), successBg: ui.Hex("#17301F"),
		warn: ui.Hex("#F0B65A"), warnBg: ui.Hex("#352812"), warnBorder: ui.Hex("#6A4D1C"),
		danger: ui.Hex("#F2877D"), dangerBg: ui.Hex("#3A1E1C"), dangerBorder: ui.Hex("#6E3029"),
		scrim: ui.RGBA(0, 0, 0, 0.55), stage: ui.Hex("#1E2025"),
	}
)

// pal is the palette of the appearance the window shows.
func pal(c *ui.Context) *palette {
	if c.Theme().Dark {
		return &darkPal
	}
	return &lightPal
}

// applyTheme gives MyGo's own widgets (switches, selects, radios, focus
// rings) the design's accent and ground.
func applyTheme(c *ui.Context) {
	p := pal(c)
	t := *c.Theme()
	t.Accent, t.AccentText, t.Background, t.Text, t.TextMuted, t.Border = p.accent, p.onAccent, p.pane, p.text, p.muted, p.border
	t.Font = fontFamily
	c.SetTheme(&t)
}

// Sizes from the design (design/README.md, "Tokens").
const (
	fontFamily  = "Segoe UI Variable Text"
	fontMono    = "Cascadia Mono"
	fontSmall   = 12
	fontBody    = 14
	fontSection = 15
	fontTitle   = 22
	radiusCtl   = 6
	radiusCard  = 8
	radiusDlg   = 10
	sidebarW    = 248
	headerH     = 44
	margin      = 8
	// A pane is at least paneMinW × paneMinH; narrower than paneFoldW its
	// header folds the window and split buttons into ⋯.
	paneMinW  = 360
	paneMinH  = 280
	paneFoldW = 560
	maxPanes  = 4
)

// menuSVG converts a line *ui.SVG into a template menu icon.
func menuSVG(s *ui.SVG) *mygo.MenuIcon {
	ic, err := mygo.NewMenuIconSVG(s, true)
	if err != nil {
		panic(err)
	}
	return ic
}
func svg(body string) *ui.SVG {
	return ui.MustParseSVG([]byte(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">` + body + `</svg>`))
}

var (
	icHome     = svg(`<path d="M3 11l9-7 9 7v9a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z"/>`)
	icPlus     = svg(`<path d="M12 5v14M5 12h14"/>`)
	icGear     = svg(`<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1"/>`)
	icSidebar  = svg(`<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M9 4v16"/>`)
	icSearch   = svg(`<circle cx="11" cy="11" r="7"/><path d="M20 20l-4-4"/>`)
	icMore     = svg(`<circle cx="5" cy="12" r="1" fill="currentColor"/><circle cx="12" cy="12" r="1" fill="currentColor"/><circle cx="19" cy="12" r="1" fill="currentColor"/>`)
	icClose    = svg(`<path d="M6 6l12 12M18 6L6 18"/>`)
	icChevron  = svg(`<path d="M7 10l5 5 5-5"/>`)
	icSplitR   = svg(`<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M12 4v16"/>`)
	icSplitD   = svg(`<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M3 12h18"/>`)
	icWindow   = svg(`<rect x="3" y="5" width="14" height="12" rx="1.5"/><path d="M13 3h8v8"/><path d="M21 3l-7 7"/>`)
	icStop     = svg(`<rect x="6" y="6" width="12" height="12" rx="1.5"/>`)
	icMax      = svg(`<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"/>`)
	icRestore  = svg(`<path d="M9 4v5H4M15 4v5h5M9 20v-5H4M15 20v-5h5"/>`)
	icBack     = svg(`<path d="M15 5l-7 7 7 7"/>`)
	icRotate   = svg(`<path d="M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5"/>`)
	icPulse    = svg(`<path d="M3 12h4l3-7 4 14 3-7h4"/>`)
	icInfo     = svg(`<circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 8v.5"/>`)
	icWarn     = svg(`<path d="M12 3l10 18H2z"/><path d="M12 10v5M12 18v.5"/>`)
	icCheck    = svg(`<path d="M5 12l5 5 9-10"/>`)
	icDownload = svg(`<path d="M12 4v11M7 10l5 5 5-5M5 20h14"/>`)
	icFolder   = svg(`<path d="M3 6a1 1 0 0 1 1-1h5l2 2h9a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/>`)
	icCode     = svg(`<path d="M8 8l-4 4 4 4M16 8l4 4-4 4M14 5l-4 14"/>`)
	icChip     = svg(`<rect x="6" y="6" width="12" height="12" rx="1.5"/><path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/>`)
	icCube     = svg(`<path d="M12 3l8 4.5v9L12 21l-8-4.5v-9z"/><path d="M12 12l8-4.5M12 12v9M12 12L4 7.5"/>`)
	icJava     = svg(`<path d="M6 14h10v2a4 4 0 0 1-4 4h-2a4 4 0 0 1-4-4z"/><path d="M16 15h1.5a2 2 0 0 0 0-4H16M10 4c-1 2 1 3 0 6M13 5c-1 2 1 3 0 5"/>`)
	icShield   = svg(`<path d="M12 3l8 3v6c0 4.5-3.4 8-8 9-4.6-1-8-4.5-8-9V6z"/>`)
	icLanguage = svg(`<path d="M4 5h9M8.5 3v2M6 5c1 4 4 7 7 8M11 5c-1 4-4 7-7 8M13 21l4-9 4 9M14.5 18h5"/>`)
	icContrast = svg(`<circle cx="12" cy="12" r="8"/><path d="M12 4a8 8 0 0 1 0 16z" fill="currentColor"/>`)
	icDoc      = svg(`<path d="M6 3h8l4 4v14H6z"/><path d="M9 12h6M9 16h6"/>`)
	icPower    = svg(`<path d="M12 3v9M6.3 7.3a8 8 0 1 0 11.4 0"/>`)
	icMonitor  = svg(`<rect x="3" y="4" width="18" height="12" rx="1.5"/><path d="M8 20h8M12 16v4"/>`)
	icApk      = svg(`<rect x="6" y="2" width="12" height="20" rx="2.5"/><path d="M10 18h4"/>`)
	icPlay     = svg(`<path d="M8 5l11 7-11 7z"/>`)
	icCircleX  = svg(`<circle cx="12" cy="12" r="9"/><path d="M9 9l6 6M15 9l-6 6"/>`)
	icDockIn   = svg(`<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M14 4v16"/><path d="M6 12h5M9 9.5l2.5 2.5L9 14.5"/>`)
	icTrash    = svg(`<path d="M4 7h16M9 7V5a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2M7 7l1 12a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1l1-12"/>`)

	// Menu icons matching design/ menus (OpenMenu.html, ContextMenu.html, LibrarySplit.html).
	miPane   = menuSVG(icApk)
	miSplitR = menuSVG(icSplitR)
	miSplitD = menuSVG(icSplitD)
	miWindow = menuSVG(icWindow)
	miDockIn = menuSVG(icDockIn)
	miGear   = menuSVG(icGear)
	miStop   = menuSVG(icStop)
	miKill   = menuSVG(icCircleX)
	miRotate = menuSVG(icRotate)
	miMax    = menuSVG(icMax)
	miDoc    = menuSVG(icDoc)
	miFolder = menuSVG(icFolder)
	miTrash  = menuSVG(icTrash)
	miPlus   = menuSVG(icPlus)
	miHome   = menuSVG(icHome)

	// The mark: Windows-ARM64.apk's own Fluent icon (logo.png), with a vector
	// fallback if image decoding fails. Never the Mac.apk logo or the Android robot.
	brandLogo = func() ui.ImageSource {
		img, _ := ui.DecodeImage(logoPNG)
		return img
	}()
	icMark = ui.MustParseSVG([]byte(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M2 8h20"/><rect x="9" y="10" width="6" height="8" rx="1.2"/></svg>`))

	// Windows 11's caption glyphs, 10 points with lines of one.
	winMin   = ui.MustParseSVG([]byte(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><path d="M0 5h10" stroke="currentColor"/></svg>`))
	winMax   = ui.MustParseSVG([]byte(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect x="0.5" y="0.5" width="9" height="9" fill="none" stroke="currentColor"/></svg>`))
	winRest  = ui.MustParseSVG([]byte(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10" fill="none" stroke="currentColor"><rect x="0.5" y="2.5" width="7" height="7"/><path d="M2.5 2.5V0.5h7v7h-2"/></svg>`))
	winClose = ui.MustParseSVG([]byte(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><path d="M0 0l10 10M10 0L0 10" stroke="currentColor"/></svg>`))
)

// mark draws Windows-ARM64.apk's brand icon at size points square.
func mark(c *ui.Context, size float32) {
	if brandLogo != nil {
		ui.Image(c, brandLogo).Size(size, size).Shrink(0)
		return
	}
	p := pal(c)
	ui.Box(c).Size(size, size).Shrink(0).Radius(size * 0.22).Background(p.accent).Center().Children(func() {
		ui.Icon(c, icMark).FontSize(size * 0.6).TextColor(p.onAccent)
	})
}

// appIcon is an app's icon: the one its APK carries, else a stand-in, its
// label's first letter on a colour chosen by its package.
func appIcon(c *ui.Context, e *entry, size float32) {
	if e.icon != nil {
		ui.Image(c, e.icon).Size(size, size).Shrink(0).Radius(size * 0.22)
		return
	}
	h := fnv.New32a()
	h.Write([]byte(e.APK.Package))
	hues := []string{"#2F7D5B", "#B3541E", "#5B4FB3", "#1E6FA8", "#8C3A6B", "#6B6B1E"}
	initial := "?"
	for _, r := range e.APK.Label {
		initial = string(r)
		break
	}
	ui.Box(c).Size(size, size).Shrink(0).Radius(size * 0.22).Background(ui.Hex(hues[h.Sum32()%uint32(len(hues))])).Center().Children(func() {
		ui.Text(c, initial).FontSize(size * 0.45).FontWeight(600).TextColor(ui.Hex("#FFFFFF"))
	})
}

// tile is an icon on a square tile, as Settings' rows start.
func tile(c *ui.Context, icon *ui.SVG, size float32, bg, fg ui.Color) {
	ui.Box(c).Size(size, size).Shrink(0).Radius(size * 0.22).Background(bg).Center().Children(func() {
		ui.Icon(c, icon).FontSize(size * 0.5).TextColor(fg)
	})
}

// dot is a status dot.
func dot(c *ui.Context, col ui.Color) {
	ui.Box(c).Size(8, 8).Shrink(0).Radius(4).Background(col)
}

// tone is one of the design's chip colours.
type tone int

const (
	toneNeutral tone = iota
	toneGood
	toneWarn
	toneBad
	toneAccent
)

// colors are a tone's text and background.
func (t tone) colors(p *palette) (fg, bg ui.Color) {
	switch t {
	case toneGood:
		return p.success, p.successBg
	case toneWarn:
		return p.warn, p.warnBg
	case toneBad:
		return p.danger, p.dangerBg
	case toneAccent:
		return p.accentText, p.tint
	}
	return p.muted, p.hover
}

// chip is a short label in a rounded pill of a tone.
func chip(c *ui.Context, text string, t tone) {
	fg, bg := t.colors(pal(c))
	ui.Box(c).Padding(2, 8).Radius(10).Background(bg).Shrink(0).Children(func() {
		ui.Text(c, text).FontSize(fontSmall).FontWeight(600).TextColor(fg).NoWrap()
	})
}

// button is the design's secondary button: an outline on the pane.
// Returns: the button, whose Clicked tells a click.
func button(c *ui.Context, key, label string, icon *ui.SVG) ui.Element {
	p := pal(c)
	b := ui.ButtonBase(c.Key(key)).Height(32).Padding(0, 14).Radius(radiusCtl).Border(1, p.border).Background(p.pane).Label(label)
	if b.Hovered() {
		b.Background(p.hover)
	}
	if b.IsDisabled() {
		b.Opacity(0.5)
	}
	b.Children(func() {
		ui.Row(c).Gap(8).Children(func() {
			if icon != nil {
				ui.Icon(c, icon).FontSize(14).TextColor(p.text)
			}
			ui.Text(c, label).TextColor(p.text).NoWrap()
		})
	})
	return b
}

// primary is the design's primary button, filled with the accent.
func primary(c *ui.Context, key, label string, icon *ui.SVG) ui.Element {
	p := pal(c)
	b := ui.ButtonBase(c.Key(key)).Height(32).Padding(0, 14).Radius(radiusCtl).Background(p.accent).Label(label)
	if b.Hovered() {
		b.Opacity(0.9)
	}
	if b.IsDisabled() {
		b.Opacity(0.45)
	}
	b.Children(func() {
		ui.Row(c).Gap(8).Children(func() {
			if icon != nil {
				ui.Icon(c, icon).FontSize(14).TextColor(p.onAccent)
			}
			ui.Text(c, label).FontWeight(600).TextColor(p.onAccent).NoWrap()
		})
	})
	return b
}

// iconButton is a square button showing only an icon, its label as
// tooltip and for assistive technology.
func iconButton(c *ui.Context, key string, icon *ui.SVG, label string) ui.Element {
	p := pal(c)
	b := ui.ButtonBase(c.Key(key)).Size(32, 32).Shrink(0).Radius(radiusCtl).Center().Label(label).Tooltip(label)
	if b.Hovered() {
		b.Background(p.hover)
	}
	fg := p.text
	if b.IsDisabled() {
		fg = p.muted.Alpha(0.6)
	}
	b.Children(func() { ui.Icon(c, icon).FontSize(16).TextColor(fg) })
	return b
}

// link is words in the accent that act as a button.
func link(c *ui.Context, key, label string) ui.Element {
	p := pal(c)
	b := ui.ButtonBase(c.Key(key)).Label(label)
	b.Children(func() {
		t := ui.Text(c, label).FontSize(13).TextColor(p.accentText)
		if b.Hovered() {
			t.Underline()
		}
	})
	return b
}

// sectionTitle is a section's heading.
func sectionTitle(c *ui.Context, text string) {
	ui.Text(c, text).FontSize(fontSection).FontWeight(600).Level(2)
}

// muted is secondary text.
func muted(c *ui.Context, text string, size float32) ui.Element {
	return ui.Text(c, text).FontSize(size).TextColor(pal(c).muted)
}

// dangerButton is an outline button tinted for destructive actions.
func dangerButton(c *ui.Context, key, label string) ui.Element {
	p := pal(c)
	b := ui.ButtonBase(c.Key(key)).Height(32).Padding(0, 14).Radius(radiusCtl).Border(1, p.dangerBorder).Background(p.pane).Label(label)
	if b.Hovered() {
		b.Background(p.dangerBg)
	}
	if b.IsDisabled() {
		b.Opacity(0.5)
	}
	b.Children(func() {
		ui.Text(c, label).FontWeight(600).TextColor(p.danger).NoWrap()
	})
	return b
}

// mib is a byte count in megabytes, as the design shows sizes.
func mib(b int64) string {
	if b >= 100_000_000 {
		return fmt.Sprintf("%.0f MB", float64(b)/1e6)
	}
	return fmt.Sprintf("%.1f MB", float64(b)/1e6)
}
