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
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
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
			select {
			case glesFirst <- img:
			default:
			}
		})
		select {
		case img := <-glesFirst:
			nonBlack := false
			for i := 0; i+3 < len(img.Pix); i += 4 {
				if img.Pix[i] != 0 || img.Pix[i+1] != 0 || img.Pix[i+2] != 0 {
					nonBlack = true
					break
				}
			}
			if !nonBlack {
				t.Fatal("embedded GLES frame was completely black")
			}
		case st := <-glesDone:
			t.Fatalf("GLES ended before frame: %s (%s): %v", st.Phase, st.Category, st.Log)
		case <-ctx.Done():
			_ = gp.Kill()
			t.Fatal("timed out waiting for embedded GLES frame")
		}
		_ = gp.Stop()
		<-glesDone
	}
}
