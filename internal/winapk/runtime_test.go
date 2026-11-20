package winapk

import (
	"archive/zip"
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"image"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"runtime"
	"sync/atomic"
	"testing"
	"time"
)

// TestStorageAndJavaInstall verifies AppData/code storage helpers and private
// Java runtime SHA-256 / ARM64 PE enforcement.
func TestStorageAndJavaInstall(t *testing.T) {
	src := t.TempDir()
	pkgDir := AppDataDir(src, "com.example.notes")
	codeDir := filepath.Join(src, "code", "com.example.notes")
	if err := os.MkdirAll(pkgDir, 0o700); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(codeDir, 0o700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(pkgDir, "db.sqlite"), make([]byte, 4096), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(codeDir, "classes.jar"), make([]byte, 8192), 0o600); err != nil {
		t.Fatal(err)
	}
	if got := AppDataSize(src, "com.example.notes"); got != 4096 {
		t.Fatalf("AppDataSize = %d, want 4096", got)
	}
	if got := ConvertedCodeSize(src); got != 8192 {
		t.Fatalf("ConvertedCodeSize = %d, want 8192", got)
	}
	if err := ClearConvertedCode(src); err != nil {
		t.Fatal(err)
	}
	if got := ConvertedCodeSize(src); got != 0 {
		t.Fatalf("ConvertedCodeSize after clear = %d, want 0", got)
	}
	dst := t.TempDir()
	if err := MoveDataDir(src, dst); err != nil {
		t.Fatal(err)
	}
	if got := AppDataSize(dst, "com.example.notes"); got != 4096 {
		t.Fatalf("AppDataSize after move = %d, want 4096", got)
	}

	// Build a minimal synthetic ZIP whose jvm.dll header carries ARM64 (0xAA64) or x64 (0x8664).
	makeZip := func(machine uint16) ([]byte, string) {
		var buf bytes.Buffer
		zw := zip.NewWriter(&buf)
		w, err := zw.Create("jdk-21.0.99+1/bin/server/jvm.dll")
		if err != nil {
			t.Fatal(err)
		}
		var pe [128]byte
		pe[0], pe[1] = 'M', 'Z'
		binary.LittleEndian.PutUint32(pe[60:64], 64)
		copy(pe[64:68], "PE\x00\x00")
		binary.LittleEndian.PutUint16(pe[68:70], machine)
		_, _ = w.Write(pe[:])
		_ = zw.Close()
		sum := sha256.Sum256(buf.Bytes())
		return buf.Bytes(), hex.EncodeToString(sum[:])
	}

	armZip, armSum := makeZip(0xAA64)
	x64Zip, x64Sum := makeZip(0x8664)

	// 1. Negative: wrong SHA-256 is refused before extraction.
	badSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/"+defaultJdkSumPath {
			_, _ = fmt.Fprintf(w, "%064x  microsoft-jdk-21.0.99-windows-aarch64.zip\n", 1)
			return
		}
		_, _ = w.Write(armZip)
	}))
	defer badSrv.Close()
	rt := &Runtime{JavaFeedURL: badSrv.URL}
	if err := rt.InstallJava(context.Background(), filepath.Join(t.TempDir(), "Runtime"), nil); err == nil {
		t.Fatal("expected SHA-256 mismatch error")
	}

	// 2. Negative: x64 PE inside archive is refused ("Native means native").
	x64Srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/"+defaultJdkSumPath {
			_, _ = fmt.Fprintf(w, "%s  microsoft-jdk-21.0.99-windows-aarch64.zip\n", x64Sum)
			return
		}
		_, _ = w.Write(x64Zip)
	}))
	defer x64Srv.Close()
	rt.JavaFeedURL = x64Srv.URL
	if err := rt.InstallJava(context.Background(), filepath.Join(t.TempDir(), "Runtime"), nil); err == nil {
		t.Fatal("expected non-ARM64 PE refusal")
	}

	// 3. Positive extraction & metadata check (without doctor exe set).
	okSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/"+defaultJdkSumPath {
			_, _ = fmt.Fprintf(w, "%s  microsoft-jdk-21.0.99-windows-aarch64.zip\n", armSum)
			return
		}
		_, _ = w.Write(armZip)
	}))
	defer okSrv.Close()
	rt.JavaFeedURL = okSrv.URL
	target := filepath.Join(t.TempDir(), "Runtime")
	if err := rt.InstallJava(context.Background(), target, nil); err != nil {
		t.Fatalf("InstallJava: %v", err)
	}
	if upd, _, err := rt.CheckJavaUpdate(context.Background(), target); err != nil || upd {
		t.Fatalf("CheckJavaUpdate = (%v, %v), want (false, nil)", upd, err)
	}
}

// TestRealRuntimeEmbed exercises Doctor, Inspect and `--embed` frame/input/resize/stop
// against the built ARM64 `winapk.exe` and `hello-java.apk` when present.
func TestRealRuntimeEmbed(t *testing.T) {
	if runtime.GOOS != "windows" || runtime.GOARCH != "arm64" {
		t.Skip("requires Windows ARM64 host")
	}
	rt := NewRuntime("")
	if rt.Executable() == "" {
		t.Skip("winapk.exe not built")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 45*time.Second)
	defer cancel()

	rep, err := rt.Doctor(ctx, "")
	if err != nil {
		t.Fatalf("Doctor: %v", err)
	}
	if !rep.Ready() || rep.Demo || !rep.Java.Native {
		t.Fatalf("Doctor report not ready: %+v", rep)
	}

	hello := filepath.Join("..", "..", "tests", "fixtures", "hello-java.apk")
	if _, err := os.Stat(hello); err != nil {
		t.Skip("hello-java.apk fixture not built")
	}
	apk, err := rt.Inspect(ctx, hello)
	if err != nil {
		t.Fatalf("Inspect: %v", err)
	}
	if apk.Label != "Hello Java" || apk.Verdict != CanRun || apk.Package != "org.winapk.fixture.hello" {
		t.Fatalf("Inspect unexpected APK: %+v", apk)
	}

	var frames atomic.Int32
	firstFrame := make(chan *image.RGBA, 1)
	stopped := make(chan State, 1)

	proc, err := rt.Launch(LaunchSpec{
		Path:    hello,
		DataDir: t.TempDir(),
		Native:  NativeOff,
		Width:   360,
		Height:  720,
	}, func(s State) {
		if s.Phase == Stopped || s.Phase == Failed {
			select {
			case stopped <- s:
			default:
			}
		}
	})
	if err != nil {
		t.Fatalf("Launch: %v", err)
	}
	proc.SetFrameHandler(func(img *image.RGBA) {
		if frames.Add(1) == 1 {
			firstFrame <- img
		}
	})

	select {
	case img := <-firstFrame:
		if img.Rect.Dx() <= 0 || img.Rect.Dy() <= 0 {
			t.Fatalf("empty frame bounds: %v", img.Rect)
		}
	case st := <-stopped:
		t.Fatalf("process ended before first frame: phase=%s cat=%s log=%v", st.Phase, st.Category, st.Log)
	case <-ctx.Done():
		_ = proc.Kill()
		t.Fatal("timed out waiting for first embedded RGBA8 frame")
	}

	if !proc.Reattach(true) || !proc.Reattach(false) {
		t.Fatal("Reattach returned false")
	}
	proc.SendPointer(PointerDown, 180, 360)
	proc.SendPointer(PointerUp, 180, 360)
	_ = proc.Resize(720, 360)
	_ = proc.Back()
	if err := proc.Stop(); err != nil {
		t.Fatalf("Stop: %v", err)
	}

	select {
	case st := <-stopped:
		if st.Phase != Stopped {
			t.Fatalf("expected Stopped, got %s (%s): %v", st.Phase, st.Category, st.Log)
		}
	case <-ctx.Done():
		_ = proc.Kill()
		t.Fatal("timed out waiting for graceful Stop")
	}

	absDiff := func(a, b int) int {
		if a < b {
			return b - a
		}
		return a - b
	}

	glesApk := filepath.Join("..", "..", "tests", "fixtures", "hello-guest-gles.apk")
	if _, err := os.Stat(glesApk); err == nil && rep.Machine.AngleArm64 {
		glesFirst := make(chan *image.RGBA, 1)
		glesDone := make(chan State, 1)
		gp, err := rt.Launch(LaunchSpec{
			Path:     glesApk,
			DataDir:  t.TempDir(),
			Native:   NativeRestricted,
			Profile:  N2,
			Graphics: "d3d11",
			Width:    360,
			Height:   720,
		}, func(s State) {
			if s.Phase == Stopped || s.Phase == Failed {
				select {
				case glesDone <- s:
				default:
				}
			}
		})
		if err != nil {
			t.Fatalf("Launch GLES: %v", err)
		}
		gp.SetFrameHandler(func(img *image.RGBA) {
			c := img.RGBAAt(img.Rect.Dx()/2, img.Rect.Dy()/2)
			// `gles.c` blends 50%-alpha red over opaque blue -> RGBA ~(128, 0, 128, 255).
			if absDiff(int(c.R), 128) <= 8 && c.G <= 4 && absDiff(int(c.B), 128) <= 8 {
				select {
				case glesFirst <- img:
				default:
				}
			}
		})
		select {
		case <-glesFirst:
		case st := <-glesDone:
			t.Fatalf("GLES ended before frame: %s (%s): %v", st.Phase, st.Category, st.Log)
		case <-ctx.Done():
			_ = gp.Kill()
			t.Fatal("timed out waiting for embedded GLES frame")
		}
		_ = gp.Stop()
		<-glesDone
	}

	vkApk := filepath.Join("..", "..", "tests", "fixtures", "hello-vulkan-wsi.apk")
	if _, err := os.Stat(vkApk); err == nil && rep.Machine.VulkanLoader == "available" && rep.Machine.Win32Surface {
		vkMatched := make(chan *image.RGBA, 1)
		vkDone := make(chan State, 1)
		var lastColor atomic.Uint32
		var lastLog atomic.Value
		vp, err := rt.Launch(LaunchSpec{
			Path:     vkApk,
			DataDir:  t.TempDir(),
			Native:   NativeRestricted,
			Profile:  N2,
			Graphics: "vulkan",
			Width:    360,
			Height:   720,
		}, func(s State) {
			lastLog.Store(s.Log)
			if s.Phase == Stopped || s.Phase == Failed {
				select {
				case vkDone <- s:
				default:
				}
			}
		})
		if err != nil {
			t.Fatalf("Launch Vulkan: %v", err)
		}
		vp.SetFrameHandler(func(img *image.RGBA) {
			c := img.RGBAAt(img.Rect.Dx()/2, img.Rect.Dy()/2)
			lastColor.Store(uint32(c.R)<<24 | uint32(c.G)<<16 | uint32(c.B)<<8 | uint32(c.A))
			// `show_frame(0.75f, 0.125f)` clears swapchain to RGBA ~(191, 64, 32, 255) (distinct from starting window #EFEDF1).
			if absDiff(int(c.R), 191) <= 8 && absDiff(int(c.G), 64) <= 8 && absDiff(int(c.B), 32) <= 8 {
				select {
				case vkMatched <- img:
				default:
				}
			}
		})
		vkTimer := time.NewTimer(15 * time.Second)
		defer vkTimer.Stop()
		select {
		case <-vkMatched:
		case st := <-vkDone:
			t.Fatalf("Vulkan ended before matching WSI frame: %s (%s): %v", st.Phase, st.Category, st.Log)
		case <-vkTimer.C:
			_ = vp.Kill()
			t.Fatalf("timed out waiting for embedded Vulkan WSI frame: lastRGBA=0x%08x log=%v", lastColor.Load(), lastLog.Load())
		}
		_ = vp.Stop()
		<-vkDone
	}

	// GPU-P6: verify live pane <-> standalone `Reattach` and landscape rotation `Resize`
	// on both `d3d11` and `vulkan` GPU backends without restarting the runtime process.
	javaGlesApk := filepath.Join("..", "..", "tests", "fixtures", "hello-gles.apk")
	if _, err := os.Stat(javaGlesApk); err == nil && rep.Machine.AngleArm64 {
		for _, backend := range []string{"d3d11", "vulkan"} {
			if backend == "vulkan" && (rep.Machine.VulkanLoader != "available" || !rep.Machine.Win32Surface) {
				continue
			}
			portraitMatched := make(chan struct{}, 1)
			landscapeMatched := make(chan struct{}, 1)
			done := make(chan State, 1)
			p, err := rt.Launch(LaunchSpec{
				Path:     javaGlesApk,
				DataDir:  t.TempDir(),
				Native:   NativeOff,
				Graphics: backend,
				Width:    360,
				Height:   720,
			}, func(s State) {
				if s.Phase == Stopped || s.Phase == Failed {
					select {
					case done <- s:
					default:
					}
				}
			})
			if err != nil {
				t.Fatalf("Launch hello-gles (%s): %v", backend, err)
			}
			p.SetFrameHandler(func(img *image.RGBA) {
				w, h := img.Rect.Dx(), img.Rect.Dy()
				left := img.RGBAAt(w/4, h/2)
				right := img.RGBAAt(3*w/4, h/2)
				// `hello-gles` clears left half to (64, 128, 191) and right half to (191, 64, 128).
				if absDiff(int(left.R), 64) <= 6 && absDiff(int(left.G), 128) <= 6 && absDiff(int(left.B), 191) <= 6 &&
					absDiff(int(right.R), 191) <= 6 && absDiff(int(right.G), 64) <= 6 && absDiff(int(right.B), 128) <= 6 {
					if w == 360 && h == 720 {
						select {
						case portraitMatched <- struct{}{}:
						default:
						}
					} else if w == 720 && h == 360 {
						select {
						case landscapeMatched <- struct{}{}:
						default:
						}
					}
				}
			})
			timer := time.NewTimer(15 * time.Second)
			select {
			case <-portraitMatched:
			case st := <-done:
				timer.Stop()
				t.Fatalf("hello-gles (%s) ended before portrait frame: %s (%s)", backend, st.Phase, st.Category)
			case <-timer.C:
				_ = p.Kill()
				t.Fatalf("hello-gles (%s) timed out waiting for portrait frame", backend)
			}
			timer.Stop()

			if !p.Reattach(true) || !p.Reattach(false) {
				_ = p.Kill()
				t.Fatalf("hello-gles (%s) Reattach roundtrip failed", backend)
			}
			if err := p.Resize(720, 360); err != nil {
				_ = p.Kill()
				t.Fatalf("hello-gles (%s) Resize(720,360): %v", backend, err)
			}
			rotTimer := time.NewTimer(10 * time.Second)
			select {
			case <-landscapeMatched:
			case st := <-done:
				rotTimer.Stop()
				t.Fatalf("hello-gles (%s) ended during rotation: %s (%s)", backend, st.Phase, st.Category)
			case <-rotTimer.C:
				_ = p.Kill()
				t.Fatalf("hello-gles (%s) timed out waiting for rotated 720x360 frame", backend)
			}
			rotTimer.Stop()
			_ = p.Stop()
			<-done
		}
	}
}
