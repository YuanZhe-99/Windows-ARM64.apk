package winapk

import (
	"archive/zip"
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"
)

const (
	defaultJdkBaseURL   = "https://aka.ms/download-jdk"
	defaultJdkSumPath   = "microsoft-jdk-21-windows-aarch64.zip.sha256sum.txt"
	pinnedJdkArchive    = "microsoft-jdk-21.0.13.0.2-windows-aarch64.zip"
	pinnedJdkSHA256     = "9c1f36b1660d97ab228787294433eefaaad3e3e8963bdd3cf975b02171137ecc"
	runtimeMetaFilename = ".winapk-runtime.json"
)

type runtimeMeta struct {
	Version     string `json:"version"`
	SHA256      string `json:"sha256"`
	Archive     string `json:"archive"`
	InstalledAt string `json:"installedAt"`
}

func readRuntimeMeta(dir string) runtimeMeta {
	var m runtimeMeta
	if dir == "" {
		return m
	}
	if b, err := os.ReadFile(filepath.Join(dir, runtimeMetaFilename)); err == nil {
		_ = json.Unmarshal(b, &m)
	}
	return m
}

// resolveJdkArchive fetches Microsoft's OpenJDK 21 ARM64 SHA-256 manifest,
// falling back to the build-locked archive when the metadata request fails.
func (r *Runtime) resolveJdkArchive(ctx context.Context) (url, archive, wantSHA string, err error) {
	base := strings.TrimRight(r.JavaFeedURL, "/")
	if base == "" {
		base = defaultJdkBaseURL
	}
	reqCtx, cancel := context.WithTimeout(ctx, 15*time.Second)
	defer cancel()
	req, err := http.NewRequestWithContext(reqCtx, http.MethodGet, base+"/"+defaultJdkSumPath, nil)
	if err == nil {
		if resp, rerr := http.DefaultClient.Do(req); rerr == nil {
			body, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
			_ = resp.Body.Close()
			if resp.StatusCode == http.StatusOK {
				fields := strings.Fields(string(body))
				if len(fields) >= 2 && len(fields[0]) == 64 {
					name := filepath.Base(fields[1])
					if strings.HasPrefix(name, "microsoft-jdk-21") && strings.HasSuffix(name, "-windows-aarch64.zip") {
						return base + "/" + name, name, strings.ToLower(fields[0]), nil
					}
				}
			}
		}
	}
	if r.JavaFeedURL != "" {
		return "", "", "", errors.New("winapk: invalid Java runtime checksum feed")
	}
	return base + "/" + pinnedJdkArchive, pinnedJdkArchive, pinnedJdkSHA256, nil
}

// CheckJavaUpdate reports whether a newer Microsoft OpenJDK 21 ARM64 build is
// published for the private runtime installed in dir.
func (r *Runtime) CheckJavaUpdate(ctx context.Context, dir string) (bool, string, error) {
	if dir == "" {
		dir = DefaultRuntimeDir()
	}
	cur := readRuntimeMeta(dir)
	if cur.SHA256 == "" {
		return false, "", nil
	}
	_, archive, sum, err := r.resolveJdkArchive(ctx)
	if err != nil {
		return false, "", err
	}
	if strings.EqualFold(sum, cur.SHA256) {
		return false, "", nil
	}
	ver := strings.TrimSuffix(strings.TrimPrefix(archive, "microsoft-jdk-"), "-windows-aarch64.zip")
	return true, ver, nil
}

// InstallJava downloads, SHA-256-verifies, extracts and doctor-tests the
// private ARM64 OpenJDK 21 runtime in dir without touching system PATH or JAVA_HOME.
func (r *Runtime) InstallJava(ctx context.Context, dir string, progress func(InstallStep, float64)) error {
	if dir == "" {
		dir = DefaultRuntimeDir()
	}
	if progress == nil {
		progress = func(InstallStep, float64) {}
	}
	dlURL, archiveName, wantSHA, err := r.resolveJdkArchive(ctx)
	if err != nil {
		return err
	}
	parent := filepath.Dir(dir)
	if err := os.MkdirAll(parent, 0o700); err != nil {
		return err
	}
	tmpZip, err := os.CreateTemp(parent, "winapk-jdk-*.zip")
	if err != nil {
		return err
	}
	tmpZipPath := tmpZip.Name()
	defer func() {
		_ = tmpZip.Close()
		_ = os.Remove(tmpZipPath)
	}()

	progress(StepDownload, 0)
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, dlURL, nil)
	if err != nil {
		return err
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("winapk: download HTTP %d", resp.StatusCode)
	}

	hasher := sha256.New()
	total := resp.ContentLength
	var got int64
	buf := make([]byte, 128*1024)
	for {
		n, rerr := resp.Body.Read(buf)
		if n > 0 {
			if _, werr := tmpZip.Write(buf[:n]); werr != nil {
				return werr
			}
			_, _ = hasher.Write(buf[:n])
			got += int64(n)
			if total > 0 {
				progress(StepDownload, min(float64(got)/float64(total), 1))
			}
		}
		if errors.Is(rerr, io.EOF) {
			break
		}
		if rerr != nil {
			return rerr
		}
	}
	if err := tmpZip.Close(); err != nil {
		return err
	}

	progress(StepCheck, 0)
	gotSHA := hex.EncodeToString(hasher.Sum(nil))
	if !strings.EqualFold(gotSHA, wantSHA) {
		return fmt.Errorf("winapk: SHA-256 mismatch (got %s, expected %s)", gotSHA, wantSHA)
	}
	progress(StepCheck, 1)

	progress(StepInstall, 0)
	stage := dir + ".staging"
	_ = os.RemoveAll(stage)
	if err := extractJdkZip(ctx, tmpZipPath, stage); err != nil {
		_ = os.RemoveAll(stage)
		return err
	}
	jvmDLL := filepath.Join(stage, "bin", "server", "jvm.dll")
	if !isArm64PE(jvmDLL) {
		_ = os.RemoveAll(stage)
		return errors.New("winapk: extracted jvm.dll is not a native ARM64 PE")
	}

	progress(StepTest, 0)
	var ver string
	if r.exe != "" {
		rep, derr := r.Doctor(ctx, stage)
		if derr != nil || !rep.Java.Found || !rep.Java.Native || rep.Java.Started != "ok" {
			_ = os.RemoveAll(stage)
			if derr != nil {
				return derr
			}
			return fmt.Errorf("winapk: staged Java runtime self-test failed (%s)", rep.Java.Started)
		}
		ver = rep.Java.Version
	}
	meta := runtimeMeta{
		Version:     ver,
		SHA256:      gotSHA,
		Archive:     archiveName,
		InstalledAt: time.Now().Format("2 January 2006"),
	}
	if mb, merr := json.MarshalIndent(meta, "", "  "); merr == nil {
		_ = os.WriteFile(filepath.Join(stage, runtimeMetaFilename), mb, 0o600)
	}

	old := dir + ".old"
	_ = os.RemoveAll(old)
	if _, serr := os.Stat(dir); serr == nil {
		if err := os.Rename(dir, old); err != nil {
			_ = os.RemoveAll(stage)
			return err
		}
	}
	if err := os.Rename(stage, dir); err != nil {
		_ = os.Rename(old, dir)
		_ = os.RemoveAll(stage)
		return err
	}
	_ = os.RemoveAll(old)
	progress(StepDone, 1)
	return nil
}

func extractJdkZip(ctx context.Context, zipPath, dest string) error {
	zr, err := zip.OpenReader(zipPath)
	if err != nil {
		return err
	}
	defer zr.Close()

	// Strip a single common top-level directory (e.g. "jdk-21.0.13+11/").
	prefix := ""
	for i, f := range zr.File {
		name := strings.TrimLeft(filepath.ToSlash(f.Name), "/")
		slash := strings.IndexByte(name, '/')
		if slash <= 0 {
			prefix = ""
			break
		}
		top := name[:slash+1]
		if i == 0 {
			prefix = top
		} else if top != prefix {
			prefix = ""
			break
		}
	}

	cleanDest := filepath.Clean(dest)
	if err := os.MkdirAll(cleanDest, 0o700); err != nil {
		return err
	}
	for _, f := range zr.File {
		if err := ctx.Err(); err != nil {
			return err
		}
		rel := strings.TrimPrefix(strings.TrimLeft(filepath.ToSlash(f.Name), "/"), prefix)
		if rel == "" {
			continue
		}
		target := filepath.Join(cleanDest, filepath.FromSlash(rel))
		if target != cleanDest && !strings.HasPrefix(target, cleanDest+string(filepath.Separator)) {
			return errors.New("winapk: unsafe archive path")
		}
		if f.FileInfo().IsDir() {
			if err := os.MkdirAll(target, 0o700); err != nil {
				return err
			}
			continue
		}
		if err := os.MkdirAll(filepath.Dir(target), 0o700); err != nil {
			return err
		}
		rc, err := f.Open()
		if err != nil {
			return err
		}
		out, err := os.OpenFile(target, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0o700)
		if err != nil {
			_ = rc.Close()
			return err
		}
		_, cerr := io.Copy(out, io.LimitReader(rc, 256<<20))
		_ = rc.Close()
		if err := out.Close(); cerr == nil {
			cerr = err
		}
		if cerr != nil {
			return cerr
		}
	}
	return nil
}

// isArm64PE verifies that path's COFF machine field is IMAGE_FILE_MACHINE_ARM64 (0xAA64).
func isArm64PE(path string) bool {
	f, err := os.Open(path)
	if err != nil {
		return false
	}
	defer f.Close()
	var mz [64]byte
	if _, err := io.ReadFull(f, mz[:]); err != nil || mz[0] != 'M' || mz[1] != 'Z' {
		return false
	}
	peOff := int64(binary.LittleEndian.Uint32(mz[60:64]))
	var pe [6]byte
	if _, err := f.ReadAt(pe[:], peOff); err != nil {
		return false
	}
	if pe[0] != 'P' || pe[1] != 'E' || pe[2] != 0 || pe[3] != 0 {
		return false
	}
	return binary.LittleEndian.Uint16(pe[4:6]) == 0xAA64
}
