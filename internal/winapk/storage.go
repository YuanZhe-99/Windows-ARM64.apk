package winapk

import (
	"errors"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
)

// DefaultDataDir returns the default App data root
// (`%LOCALAPPDATA%\Windows-ARM64.apk\Apps`).
func DefaultDataDir() string {
	if local := os.Getenv("LOCALAPPDATA"); local != "" {
		return filepath.Join(local, "Windows-ARM64.apk", "Apps")
	}
	cfg, err := os.UserConfigDir()
	if err != nil {
		return ""
	}
	return filepath.Join(cfg, "Windows-ARM64.apk", "Apps")
}

// DefaultRuntimeDir returns the private ARM64 Java runtime folder
// (`%LOCALAPPDATA%\Windows-ARM64.apk\Runtime`).
func DefaultRuntimeDir() string {
	if local := os.Getenv("LOCALAPPDATA"); local != "" {
		return filepath.Join(local, "Windows-ARM64.apk", "Runtime")
	}
	cfg, err := os.UserConfigDir()
	if err != nil {
		return ""
	}
	return filepath.Join(cfg, "Windows-ARM64.apk", "Runtime")
}

// AppDataDir returns one package's private Android data folder under root.
func AppDataDir(root, pkg string) string {
	if root == "" {
		root = DefaultDataDir()
	}
	if pkg == "" {
		return filepath.Join(root, "android", "data", "data")
	}
	return filepath.Join(root, "android", "data", "data", pkg)
}

// LogsDir returns the runtime log directory under root.
func LogsDir(root string) string {
	if root == "" {
		root = DefaultDataDir()
	}
	return filepath.Join(root, "logs")
}

// dirSize sums regular file byte sizes beneath dir; missing paths return 0.
func dirSize(dir string) int64 {
	if dir == "" {
		return 0
	}
	var sum int64
	_ = filepath.WalkDir(dir, func(_ string, d fs.DirEntry, err error) error {
		if err != nil || d.IsDir() {
			return nil
		}
		if fi, err := d.Info(); err == nil {
			sum += fi.Size()
		}
		return nil
	})
	return sum
}

// AppDataSize returns the byte size of pkg's saved Android data under root.
func AppDataSize(root, pkg string) int64 {
	if pkg == "" {
		return 0
	}
	return dirSize(AppDataDir(root, pkg))
}

// TotalAppDataSize returns the combined byte size of all app-saved data under root.
func TotalAppDataSize(root string) int64 {
	if root == "" {
		root = DefaultDataDir()
	}
	return dirSize(filepath.Join(root, "android"))
}

// ConvertedCodeSize returns the byte size of cached converted DEX/native artifacts under root.
func ConvertedCodeSize(root string) int64 {
	if root == "" {
		root = DefaultDataDir()
	}
	return dirSize(filepath.Join(root, "code"))
}

// ClearAppData removes pkg's saved Android data folder under root.
// Side effects: deletes `<root>/android/data/data/<pkg>` when pkg is a safe package name.
func ClearAppData(root, pkg string) error {
	if pkg == "" || strings.ContainsAny(pkg, `/\:`) || strings.Contains(pkg, "..") {
		return errors.New("winapk: invalid package name")
	}
	return os.RemoveAll(AppDataDir(root, pkg))
}

// ClearAppArtifacts removes both saved data and converted code for pkg when removing an app.
func ClearAppArtifacts(root, pkg string) error {
	if err := ClearAppData(root, pkg); err != nil {
		return err
	}
	if root == "" {
		root = DefaultDataDir()
	}
	return os.RemoveAll(filepath.Join(root, "code", pkg))
}

// ClearConvertedCode removes `<root>/code` so converted jars are recreated on demand.
func ClearConvertedCode(root string) error {
	if root == "" {
		root = DefaultDataDir()
	}
	code := filepath.Join(root, "code")
	if err := os.RemoveAll(code); err != nil {
		return err
	}
	return os.MkdirAll(code, 0o700)
}

// MoveDataDir migrates `android`, `code` and `logs` from src to dst when no apps are running.
// Side effects: copies across volumes when os.Rename cannot move directly, then removes src's moved subtrees.
func MoveDataDir(src, dst string) error {
	if src == "" {
		src = DefaultDataDir()
	}
	srcClean, dstClean := filepath.Clean(src), filepath.Clean(dst)
	if strings.EqualFold(srcClean, dstClean) {
		return nil
	}
	rel, err := filepath.Rel(srcClean, dstClean)
	if err == nil && rel != ".." && !strings.HasPrefix(rel, ".."+string(filepath.Separator)) {
		return errors.New("winapk: destination lies inside the current folder")
	}
	if err := os.MkdirAll(dstClean, 0o700); err != nil {
		return err
	}
	for _, sub := range []string{"android", "code", "logs"} {
		from := filepath.Join(srcClean, sub)
		if _, err := os.Stat(from); errors.Is(err, os.ErrNotExist) {
			continue
		}
		to := filepath.Join(dstClean, sub)
		if err := os.Rename(from, to); err == nil {
			continue
		}
		if err := copyTree(from, to); err != nil {
			return err
		}
		_ = os.RemoveAll(from)
	}
	return nil
}

func copyTree(src, dst string) error {
	return filepath.WalkDir(src, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(src, p)
		if err != nil {
			return err
		}
		target := filepath.Join(dst, rel)
		if d.IsDir() {
			return os.MkdirAll(target, 0o700)
		}
		in, err := os.Open(p)
		if err != nil {
			return err
		}
		defer in.Close()
		fi, err := d.Info()
		mode := fs.FileMode(0o600)
		if err == nil {
			mode = fi.Mode().Perm()
		}
		out, err := os.OpenFile(target, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
		if err != nil {
			return err
		}
		_, cerr := io.Copy(out, in)
		if err := out.Close(); cerr == nil {
			cerr = err
		}
		return cerr
	})
}
