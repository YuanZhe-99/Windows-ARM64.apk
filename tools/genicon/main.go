// Command genicon renders Windows-ARM64.apk's brand icon deterministically:
//   - resources/icon.png (1024x1024 RGBA, read by `mygo build` for PE + NSIS)
//   - internal/gui/logo.png (256x256 RGBA, embedded for the UI mark and window icons)
//   - macapk/runtime/platform/windows/winapk.ico (multi-size 16..256 ICO for winapk.exe)
//
// Design (design/README.md, AGENTS.md): Windows-ARM64.apk's own Fluent icon — a
// deep cobalt-to-azure squircle holding a translucent desktop window pane, a
// luminous vertical phone frame, and a faceted 3D cyan-emerald GPU prism. Never
// the Mac.apk logo, Apple symbols, or the Android robot.
package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"math"
	"os"
	"path/filepath"
	"runtime"
	"sync"
)

type vec2 struct{ x, y float64 }

func (a vec2) sub(b vec2) vec2 { return vec2{a.x - b.x, a.y - b.y} }
func (a vec2) len() float64    { return math.Hypot(a.x, a.y) }

type rgbaF struct{ r, g, b, a float64 }

func hexCol(hex uint32, a float64) rgbaF {
	return rgbaF{
		r: float64((hex>>16)&0xff) / 255.0,
		g: float64((hex>>8)&0xff) / 255.0,
		b: float64(hex&0xff) / 255.0,
		a: a,
	}
}

func clamp01(v float64) float64 {
	if v < 0 {
		return 0
	}
	if v > 1 {
		return 1
	}
	return v
}

func smoothstep(e0, e1, x float64) float64 {
	t := clamp01((x - e0) / (e1 - e0))
	return t * t * (3 - 2*t)
}

func mixF(a, b, t float64) float64 { return a + (b-a)*clamp01(t) }

func mixCol(a, b rgbaF, t float64) rgbaF {
	t = clamp01(t)
	return rgbaF{
		r: a.r + (b.r-a.r)*t,
		g: a.g + (b.g-a.g)*t,
		b: a.b + (b.b-a.b)*t,
		a: a.a + (b.a-a.a)*t,
	}
}

// over composites premultiplied-alpha foreground src over dst.
func over(dst, src rgbaF) rgbaF {
	sa := clamp01(src.a)
	inv := 1.0 - sa
	return rgbaF{
		r: src.r*sa + dst.r*inv,
		g: src.g*sa + dst.g*inv,
		b: src.b*sa + dst.b*inv,
		a: sa + dst.a*inv,
	}
}

// sdRoundRect returns signed distance in normalized [0..1024] space to a rounded box.
func sdRoundRect(p, center, half vec2, radius float64) float64 {
	qx := math.Abs(p.x-center.x) - (half.x - radius)
	qy := math.Abs(p.y-center.y) - (half.y - radius)
	ox := math.Max(qx, 0)
	oy := math.Max(qy, 0)
	return math.Hypot(ox, oy) + math.Min(math.Max(qx, qy), 0) - radius
}

// sdSquircle approximates a smooth Fluent superellipse (n=4.6) signed distance.
func sdSquircle(p, center, half vec2) float64 {
	dx := math.Abs(p.x-center.x) / half.x
	dy := math.Abs(p.y-center.y) / half.y
	if dx == 0 && dy == 0 {
		return -math.Min(half.x, half.y)
	}
	const n = 4.6
	r := math.Pow(math.Pow(dx, n)+math.Pow(dy, n), 1.0/n)
	return (r - 1.0) * math.Min(half.x, half.y) * 0.86
}

// sdTriangle returns exact signed distance to triangle (a, b, c).
func sdTriangle(p, a, b, c vec2) float64 {
	e0 := b.sub(a)
	e1 := c.sub(b)
	e2 := a.sub(c)
	v0 := p.sub(a)
	v1 := p.sub(b)
	v2 := p.sub(c)

	pq0 := v0.sub(vec2{e0.x * clamp01((v0.x*e0.x+v0.y*e0.y)/(e0.x*e0.x+e0.y*e0.y)), e0.y * clamp01((v0.x*e0.x+v0.y*e0.y)/(e0.x*e0.x+e0.y*e0.y))})
	pq1 := v1.sub(vec2{e1.x * clamp01((v1.x*e1.x+v1.y*e1.y)/(e1.x*e1.x+e1.y*e1.y)), e1.y * clamp01((v1.x*e1.x+v1.y*e1.y)/(e1.x*e1.x+e1.y*e1.y))})
	pq2 := v2.sub(vec2{e2.x * clamp01((v2.x*e2.x+v2.y*e2.y)/(e2.x*e2.x+e2.y*e2.y)), e2.y * clamp01((v2.x*e2.x+v2.y*e2.y)/(e2.x*e2.x+e2.y*e2.y))})

	s := 1.0
	if e0.x*e2.y-e0.y*e2.x < 0 {
		s = -1.0
	}
	d0, s0 := pq0.x*pq0.x+pq0.y*pq0.y, s*(v0.x*e0.y-v0.y*e0.x)
	d1, s1 := pq1.x*pq1.x+pq1.y*pq1.y, s*(v1.x*e1.y-v1.y*e1.x)
	d2, s2 := pq2.x*pq2.x+pq2.y*pq2.y, s*(v2.x*e2.y-v2.y*e2.x)

	d := math.Min(d0, math.Min(d1, d2))
	minS := math.Min(s0, math.Min(s1, s2))
	sign := 1.0
	if minS > 0 {
		sign = -1.0
	}
	return math.Sqrt(d) * sign
}

func sdSegment(p, a, b vec2) float64 {
	pa := p.sub(a)
	ba := b.sub(a)
	h := clamp01((pa.x*ba.x + pa.y*ba.y) / (ba.x*ba.x + ba.y*ba.y))
	return vec2{pa.x - ba.x*h, pa.y - ba.y*h}.len()
}

// shadeSample evaluates the icon in 1024x1024 design coordinates at subpixel p.
func shadeSample(p vec2) rgbaF {
	var out rgbaF
	center := vec2{512, 506}
	tileHalf := vec2{436, 436}

	// 0. Soft ambient drop shadow beneath the squircle tile.
	dShadow := sdSquircle(p, vec2{512, 524}, vec2{434, 434})
	if dShadow < 42 {
		shA := (1.0 - smoothstep(-8, 40, dShadow)) * 0.26
		out = over(out, rgbaF{0.02, 0.06, 0.12, shA})
	}

	// 1. Base Fluent squircle tile: cobalt -> ocean -> vibrant sky azure.
	dTile := sdSquircle(p, center, tileHalf)
	if dTile < 1.5 {
		mask := 1.0 - smoothstep(-0.8, 0.8, dTile)
		tDiag := clamp01(((p.x-100)*0.55 + (p.y-80)*0.85) / 840.0)
		topCol := hexCol(0x36A8F5, 1)
		midCol := hexCol(0x0072B5, 1)
		botCol := hexCol(0x043B6B, 1)
		var base rgbaF
		if tDiag < 0.5 {
			base = mixCol(topCol, midCol, tDiag*2.0)
		} else {
			base = mixCol(midCol, botCol, (tDiag-0.5)*2.0)
		}
		// Subtle cyan aurora glow from bottom-right GPU accent.
		auroraDist := p.sub(vec2{640, 660}).len() / 520.0
		base = mixCol(base, hexCol(0x16B5D8, 1), (1.0-smoothstep(0, 1, auroraDist))*0.28)
		// Soft top-left specular glow.
		specDist := p.sub(vec2{290, 220}).len() / 460.0
		base = mixCol(base, hexCol(0x7AD3FF, 1), (1.0-smoothstep(0, 1, specDist))*0.22)

		// Crisp inner bevel rim along the top edge of the squircle.
		rim := (1.0 - smoothstep(0, 5.5, math.Abs(dTile+2.5))) * clamp01((560-p.y)/380.0) * 0.38
		base = mixCol(base, rgbaF{1, 1, 1, 1}, rim)
		base.a = mask
		out = over(out, base)

		// Clip interior layers strictly inside the squircle mask.
		var inner rgbaF

		// 2. Back Layer: Translucent Windows 11 Desktop Window Pane (Kyokai sidebar + stage).
		winCenter := vec2{444, 468}
		winHalf := vec2{274, 214}
		winRad := 44.0

		// Window drop shadow onto the squircle.
		dWinSh := sdRoundRect(p, vec2{winCenter.x + 8, winCenter.y + 20}, winHalf, winRad)
		if dWinSh < 34 {
			inner = over(inner, rgbaF{0.01, 0.07, 0.16, (1.0 - smoothstep(-6, 32, dWinSh)) * 0.36})
		}

		dWin := sdRoundRect(p, winCenter, winHalf, winRad)
		if dWin < 1.5 {
			wMask := 1.0 - smoothstep(-0.8, 0.8, dWin)
			wt := clamp01((p.y - (winCenter.y - winHalf.y)) / (winHalf.y * 2))
			glass := mixCol(rgbaF{0.88, 0.96, 1.0, 0.38}, rgbaF{0.60, 0.84, 0.99, 0.20}, wt)

			winLeft := winCenter.x - winHalf.x
			winTop := winCenter.y - winHalf.y
			headerBottom := winTop + 64.0
			sidebarRight := winLeft + 132.0

			// Window header band & left Mica sidebar tint.
			if p.y < headerBottom+1.5 {
				hAlpha := 1.0 - smoothstep(headerBottom-0.8, headerBottom+0.8, p.y)
				glass = over(glass, rgbaF{0.96, 0.99, 1.0, 0.22 * hAlpha})
			} else if p.x < sidebarRight+1.5 {
				sAlpha := 1.0 - smoothstep(sidebarRight-0.8, sidebarRight+0.8, p.x)
				glass = over(glass, rgbaF{0.92, 0.97, 1.0, 0.12 * sAlpha})
			}
			// Header horizontal divider and sidebar vertical divider.
			if math.Abs(p.y-headerBottom) < 2.2 {
				lAlpha := (1.0 - smoothstep(0.4, 2.0, math.Abs(p.y-headerBottom))) * 0.34
				glass = over(glass, rgbaF{1, 1, 1, lAlpha})
			}
			if p.y >= headerBottom && math.Abs(p.x-sidebarRight) < 2.2 {
				lAlpha := (1.0 - smoothstep(0.4, 2.0, math.Abs(p.x-sidebarRight))) * 0.26
				glass = over(glass, rgbaF{1, 1, 1, lAlpha})
			}
			// Clean titlebar pill on the exposed left side + two subtle sidebar nav pills.
			for _, pill := range []struct {
				c, h vec2
				a    float64
			}{
				{vec2{winLeft + 66, winTop + 32}, vec2{34, 7}, 0.56},
				{vec2{winLeft + 66, headerBottom + 48}, vec2{38, 7}, 0.42},
				{vec2{winLeft + 58, headerBottom + 88}, vec2{30, 7}, 0.28},
			} {
				dp := sdRoundRect(p, pill.c, pill.h, pill.h.y)
				if dp < 1.5 {
					glass = over(glass, rgbaF{1, 1, 1, (1.0 - smoothstep(-0.8, 0.8, dp)) * pill.a})
				}
			}
			// Frosted glass specular border.
			border := (1.0 - smoothstep(0, 3.8, math.Abs(dWin+1.8))) * 0.56
			glass = over(glass, rgbaF{1, 1, 1, border})
			glass.a *= wMask
			inner = over(inner, glass)
		}

		// 3. Foreground Layer: Crisp Vertical Phone Pane (balanced lower-right for depth).
		phCenter := vec2{574, 534}
		phHalf := vec2{172, 270}
		phRad := 52.0

		// Phone cast shadow onto the glass desktop window & base tile.
		dPhSh := sdRoundRect(p, vec2{phCenter.x - 6, phCenter.y + 22}, phHalf, phRad)
		if dPhSh < 44 {
			inner = over(inner, rgbaF{0.01, 0.06, 0.15, (1.0 - smoothstep(-8, 42, dPhSh)) * 0.50})
		}

		// Phone outer porcelain-ice bezel.
		dPh := sdRoundRect(p, phCenter, phHalf, phRad)
		if dPh < 1.5 {
			phMask := 1.0 - smoothstep(-0.8, 0.8, dPh)
			pt := clamp01((p.y - (phCenter.y - phHalf.y)) / (phHalf.y * 2))
			bezel := mixCol(hexCol(0xFFFFFF, 1), hexCol(0xD2EAFC, 1), pt)
			// Bezel highlight rim.
			bRim := (1.0 - smoothstep(0, 3.2, math.Abs(dPh+1.6))) * 0.68
			bezel = mixCol(bezel, rgbaF{1, 1, 1, 1}, bRim)
			bezel.a = phMask
			inner = over(inner, bezel)

			// Phone inner dark-sapphire AMOLED screen.
			scrHalf := vec2{phHalf.x - 22, phHalf.y - 22}
			scrRad := phRad - 16.0
			dScr := sdRoundRect(p, phCenter, scrHalf, scrRad)
			if dScr < 1.5 {
				sMask := 1.0 - smoothstep(-0.8, 0.8, dScr)
				st := clamp01((p.y - (phCenter.y - scrHalf.y)) / (scrHalf.y * 2))
				scr := mixCol(hexCol(0x0A2642, 1), hexCol(0x051425, 1), st)

				// Radial GPU glow behind the 3D crystal prism inside the screen.
				prismCenter := vec2{phCenter.x + 10, phCenter.y + 2}
				glowDist := p.sub(prismCenter).len() / 180.0
				scr = mixCol(scr, hexCol(0x1182D0, 1), (1.0-smoothstep(0, 1, glowDist))*0.56)

				// Subtle top speaker / camera pill.
				dPill := sdRoundRect(p, vec2{phCenter.x, phCenter.y - scrHalf.y + 22}, vec2{28, 4.5}, 4.5)
				if dPill < 1.5 {
					scr = mixCol(scr, hexCol(0x5B8BB2, 1), (1.0-smoothstep(-0.8, 0.8, dPill))*0.55)
				}

				// 4. Hero Centerpiece: Multifaceted 3D GPU Play-Prism (4 lit facets).
				vTop := vec2{phCenter.x - 58, phCenter.y - 122}
				vBot := vec2{phCenter.x - 58, phCenter.y + 126}
				vRight := vec2{phCenter.x + 110, phCenter.y + 2}
				vMid := vec2{phCenter.x - 2, phCenter.y + 2}
				vLeft := vec2{phCenter.x - 84, phCenter.y + 2}

				// Soft bloom under prism.
				dMainTri := math.Min(
					sdTriangle(p, vTop, vRight, vBot),
					math.Min(sdTriangle(p, vTop, vMid, vLeft), sdTriangle(p, vLeft, vMid, vBot)),
				)
				if dMainTri < 22 {
					scr = mixCol(scr, hexCol(0x3CF2DE, 1), (1.0-smoothstep(-2, 20, dMainTri))*0.26)
				}

				// Facet A (upper-right forward face): bright electric cyan -> ice white-cyan.
				dFA := sdTriangle(p, vTop, vRight, vMid)
				if dFA < 1.2 {
					faMask := 1.0 - smoothstep(-0.7, 0.7, dFA)
					ft := clamp01((p.x - vTop.x) / (vRight.x - vTop.x))
					colFA := mixCol(hexCol(0x7CF9EC, 1), hexCol(0x2CD5F8, 1), ft)
					colFA.a = faMask
					scr = over(scr, colFA)
				}

				// Facet B (lower-right forward face): vibrant azure -> deep cobalt-blue.
				dFB := sdTriangle(p, vMid, vRight, vBot)
				if dFB < 1.2 {
					fbMask := 1.0 - smoothstep(-0.7, 0.7, dFB)
					ft := clamp01((p.y - vMid.y) / (vBot.y - vMid.y))
					colFB := mixCol(hexCol(0x24A8FF, 1), hexCol(0x0067C0, 1), ft)
					colFB.a = fbMask
					scr = over(scr, colFB)
				}

				// Facet C (upper-left bevel): emerald-mint highlight.
				dFC := sdTriangle(p, vTop, vMid, vLeft)
				if dFC < 1.2 {
					fcMask := 1.0 - smoothstep(-0.7, 0.7, dFC)
					ft := clamp01((p.y - vTop.y) / (vMid.y - vTop.y))
					colFC := mixCol(hexCol(0xA8FFF2, 1), hexCol(0x23C9B0, 1), ft)
					colFC.a = fcMask
					scr = over(scr, colFC)
				}

				// Facet D (lower-left bevel): rich deep teal-emerald shadow facet.
				dFD := sdTriangle(p, vLeft, vMid, vBot)
				if dFD < 1.2 {
					fdMask := 1.0 - smoothstep(-0.7, 0.7, dFD)
					ft := clamp01((p.y - vMid.y) / (vBot.y - vMid.y))
					colFD := mixCol(hexCol(0x139E96, 1), hexCol(0x0A5E7A, 1), ft)
					colFD.a = fdMask
					scr = over(scr, colFD)
				}

				// Crisp specular ridge along vMid -> vRight and vTop -> vMid.
				ridge := math.Min(sdSegment(p, vMid, vRight), sdSegment(p, vTop, vMid)) - 1.4
				if ridge < 1.2 && dMainTri < 1.0 {
					ra := (1.0 - smoothstep(-0.6, 0.8, ridge)) * 0.55
					scr = over(scr, rgbaF{1, 1, 1, ra})
				}

				// Subtle bottom gesture indicator bar inside the phone screen.
				dBar := sdRoundRect(p, vec2{phCenter.x, phCenter.y + scrHalf.y - 18}, vec2{48, 3.5}, 3.5)
				if dBar < 1.5 {
					scr = mixCol(scr, hexCol(0x8AC7F2, 1), (1.0-smoothstep(-0.8, 0.8, dBar))*0.42)
				}

				scr.a = sMask * phMask
				inner = over(inner, scr)
			}
		}

		inner.a *= mask
		out = over(out, inner)
	}

	return out
}

// renderMaster renders the 1024x1024 master NRGBA image with 4x4 supersampling.
func renderMaster(size int) *image.NRGBA {
	img := image.NewNRGBA(image.Rect(0, 0, size, size))
	scale := 1024.0 / float64(size)
	const ss = 4
	invSS := 1.0 / float64(ss*ss)

	workers := runtime.NumCPU()
	var wg sync.WaitGroup
	rows := make(chan int, size)
	for range workers {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for y := range rows {
				for x := 0; x < size; x++ {
					var sumR, sumG, sumB, sumA float64
					for sy := 0; sy < ss; sy++ {
						py := (float64(y) + (float64(sy)+0.5)/float64(ss)) * scale
						for sx := 0; sx < ss; sx++ {
							px := (float64(x) + (float64(sx)+0.5)/float64(ss)) * scale
							c := shadeSample(vec2{px, py})
							// Premultiplied accumulation prevents color fringes on alpha edges.
							sumR += c.r * c.a
							sumG += c.g * c.a
							sumB += c.b * c.a
							sumA += c.a
						}
					}
					a := clamp01(sumA * invSS)
					var r, g, b float64
					if a > 1e-6 {
						r = clamp01((sumR * invSS) / a)
						g = clamp01((sumG * invSS) / a)
						b = clamp01((sumB * invSS) / a)
					}
					img.SetNRGBA(x, y, color.NRGBA{
						R: uint8(math.Round(r * 255)),
						G: uint8(math.Round(g * 255)),
						B: uint8(math.Round(b * 255)),
						A: uint8(math.Round(a * 255)),
					})
				}
			}
		}()
	}
	for y := 0; y < size; y++ {
		rows <- y
	}
	close(rows)
	wg.Wait()
	return img
}

// downsampleBox performs exact premultiplied-alpha box downsampling from a power-of-two master.
func downsampleBox(src *image.NRGBA, dstSize int) *image.NRGBA {
	srcSize := src.Bounds().Dx()
	factor := srcSize / dstSize
	dst := image.NewNRGBA(image.Rect(0, 0, dstSize, dstSize))
	inv := 1.0 / float64(factor*factor)
	for y := 0; y < dstSize; y++ {
		for x := 0; x < dstSize; x++ {
			var sr, sg, sb, sa float64
			for dy := 0; dy < factor; dy++ {
				for dx := 0; dx < factor; dx++ {
					c := src.NRGBAAt(x*factor+dx, y*factor+dy)
					a := float64(c.A) / 255.0
					sr += (float64(c.R) / 255.0) * a
					sg += (float64(c.G) / 255.0) * a
					sb += (float64(c.B) / 255.0) * a
					sa += a
				}
			}
			a := clamp01(sa * inv)
			var r, g, b float64
			if a > 1e-6 {
				r = clamp01((sr * inv) / a)
				g = clamp01((sg * inv) / a)
				b = clamp01((sb * inv) / a)
			}
			dst.SetNRGBA(x, y, color.NRGBA{
				R: uint8(math.Round(r * 255)),
				G: uint8(math.Round(g * 255)),
				B: uint8(math.Round(b * 255)),
				A: uint8(math.Round(a * 255)),
			})
		}
	}
	return dst
}

func encodePNG(img image.Image) ([]byte, error) {
	var buf bytes.Buffer
	enc := png.Encoder{CompressionLevel: png.BestCompression}
	if err := enc.Encode(&buf, img); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

// writeICO writes a multi-resolution Windows .ico containing PNG-encoded frames.
func writeICO(path string, frames []*image.NRGBA) error {
	payloads := make([][]byte, len(frames))
	for i, f := range frames {
		b, err := encodePNG(f)
		if err != nil {
			return err
		}
		payloads[i] = b
	}
	var out bytes.Buffer
	_ = binary.Write(&out, binary.LittleEndian, [3]uint16{0, 1, uint16(len(frames))})
	offset := uint32(6 + 16*len(frames))
	for i, f := range frames {
		sz := f.Bounds().Dx()
		wByte := uint8(sz)
		if sz >= 256 {
			wByte = 0
		}
		var entry [16]byte
		entry[0] = wByte
		entry[1] = wByte
		binary.LittleEndian.PutUint16(entry[4:6], 1)
		binary.LittleEndian.PutUint16(entry[6:8], 32)
		binary.LittleEndian.PutUint32(entry[8:12], uint32(len(payloads[i])))
		binary.LittleEndian.PutUint32(entry[12:16], offset)
		out.Write(entry[:])
		offset += uint32(len(payloads[i]))
	}
	for _, p := range payloads {
		out.Write(p)
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	return os.WriteFile(path, out.Bytes(), 0o644)
}

func writeFilePNG(path string, img image.Image) error {
	b, err := encodePNG(img)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	return os.WriteFile(path, b, 0o644)
}

func main() {
	master := renderMaster(1024)
	master768 := renderMaster(768) // exact 16x multiple of 48

	if err := writeFilePNG("resources/icon.png", master); err != nil {
		fmt.Fprintf(os.Stderr, "genicon: %v\n", err)
		os.Exit(1)
	}
	img256 := downsampleBox(master, 256)
	if err := writeFilePNG("internal/gui/logo.png", img256); err != nil {
		fmt.Fprintf(os.Stderr, "genicon: %v\n", err)
		os.Exit(1)
	}
	icoFrames := []*image.NRGBA{
		img256,
		downsampleBox(master, 64),
		downsampleBox(master768, 48),
		downsampleBox(master, 32),
		downsampleBox(master, 16),
	}
	if err := writeICO("macapk/runtime/platform/windows/winapk.ico", icoFrames); err != nil {
		fmt.Fprintf(os.Stderr, "genicon: %v\n", err)
		os.Exit(1)
	}
	fmt.Println("Generated resources/icon.png (1024x1024), internal/gui/logo.png (256x256), and winapk.ico (16..256).")
}
