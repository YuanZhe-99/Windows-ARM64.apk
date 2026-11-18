//go:build windows

package winapk

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"image"
	"os"
	"sync"
	"sync/atomic"
	"unsafe"

	"golang.org/x/sys/windows"
)

const (
	embedMagic     = 0x5741504B
	embedMaxPixels = 2560 * 1600 // 4,096,000 pixels (~16 MiB per RGBA8 slot, ~32 MiB total)
	embedHeaderLen = 64
)

// embedFrameHeader mirrors winapk::bridge::EmbedFrameHeader in embed.h.
type embedFrameHeader struct {
	magic      uint32
	version    uint32
	activeSlot int32
	seq        int32
	width      [2]uint32
	height     [2]uint32
	maxPixels  uint32
	reserved   [7]uint32
}

// embedChannel owns the server named pipe and double-buffered shared memory
// for one `winapk run --embed <token>` child process.
type embedChannel struct {
	token     string
	pipeH     windows.Handle
	connEv    windows.Handle
	connOv    windows.Overlapped
	connected bool
	pipe      *os.File
	mapH      windows.Handle
	hdr       *embedFrameHeader
	mem       []byte
	bufs      [2]*image.RGBA
	bufIdx    int
	closed    atomic.Bool
	mu        sync.Mutex
}

// newEmbedChannel creates a unique named pipe and shared-memory frame buffer
// ready for `winapk.exe` to connect to immediately on launch.
func newEmbedChannel() (*embedChannel, error) {
	var rnd [12]byte
	if _, err := rand.Read(rnd[:]); err != nil {
		return nil, err
	}
	token := hex.EncodeToString(rnd[:])
	pipeName, err := windows.UTF16PtrFromString(`\\.\pipe\winapk-` + token)
	if err != nil {
		return nil, err
	}
	pipeH, err := windows.CreateNamedPipe(
		pipeName,
		windows.PIPE_ACCESS_DUPLEX|uint32(windows.FILE_FLAG_FIRST_PIPE_INSTANCE)|windows.FILE_FLAG_OVERLAPPED,
		windows.PIPE_TYPE_BYTE|windows.PIPE_READMODE_BYTE|windows.PIPE_WAIT,
		1, 65536, 65536, 0, nil,
	)
	if err != nil {
		return nil, fmt.Errorf("create embed pipe: %w", err)
	}
	ev, err := windows.CreateEvent(nil, 1, 0, nil)
	if err != nil {
		_ = windows.CloseHandle(pipeH)
		return nil, fmt.Errorf("create pipe event: %w", err)
	}
	var ov windows.Overlapped
	ov.HEvent = ev
	connected := false
	if err := windows.ConnectNamedPipe(pipeH, &ov); err == nil || errors.Is(err, windows.ERROR_PIPE_CONNECTED) {
		connected = true
	} else if !errors.Is(err, windows.ERROR_IO_PENDING) {
		_ = windows.CloseHandle(ev)
		_ = windows.CloseHandle(pipeH)
		return nil, fmt.Errorf("listen embed pipe: %w", err)
	}

	totalBytes := uint64(embedHeaderLen) + uint64(embedMaxPixels)*4*2
	mapName, err := windows.UTF16PtrFromString(`Local\winapk-frame-` + token)
	if err != nil {
		_ = windows.CloseHandle(ev)
		_ = windows.CloseHandle(pipeH)
		return nil, err
	}
	mapH, err := windows.CreateFileMapping(
		windows.InvalidHandle, nil, windows.PAGE_READWRITE,
		uint32(totalBytes>>32), uint32(totalBytes), mapName,
	)
	if err != nil {
		_ = windows.CloseHandle(ev)
		_ = windows.CloseHandle(pipeH)
		return nil, fmt.Errorf("create frame mapping: %w", err)
	}
	mem, hdr, err := mapSharedFrameBuffer(mapH, int(totalBytes))
	if err != nil {
		_ = windows.CloseHandle(mapH)
		_ = windows.CloseHandle(ev)
		_ = windows.CloseHandle(pipeH)
		return nil, fmt.Errorf("map frame buffer: %w", err)
	}

	hdr.magic = embedMagic
	hdr.version = 1
	hdr.activeSlot = 0
	hdr.seq = 0
	hdr.maxPixels = embedMaxPixels

	return &embedChannel{
		token:     token,
		pipeH:     pipeH,
		connEv:    ev,
		connOv:    ov,
		connected: connected,
		mapH:      mapH,
		hdr:       hdr,
		mem:       mem,
	}, nil
}

// mapSharedFrameBuffer maps mapH and converts the OS address to Go-managed header
// and byte slice headers in the same expression as the syscall.
func mapSharedFrameBuffer(mapH windows.Handle, size int) ([]byte, *embedFrameHeader, error) {
	addr, err := windows.MapViewOfFile(mapH, windows.FILE_MAP_WRITE|windows.FILE_MAP_READ, 0, 0, uintptr(size))
	if err != nil {
		return nil, nil, err
	}
	// MapViewOfFile returns non-GC OS virtual memory pinned until UnmapViewOfFile.
	var base *byte
	*(*uintptr)(unsafe.Pointer(&base)) = addr
	mem := unsafe.Slice(base, size)
	hdr := (*embedFrameHeader)(unsafe.Pointer(&mem[0]))
	return mem, hdr, nil
}

// accept waits for the child process to open the pipe (or abort when stopCh closes)
// and wraps the connected handle as an *os.File backed by Go's IOCP poller.
func (c *embedChannel) accept(stopCh <-chan struct{}) (*os.File, error) {
	if !c.connected {
		for {
			rc, err := windows.WaitForSingleObject(c.connEv, 100)
			if rc == windows.WAIT_OBJECT_0 {
				break
			}
			if rc != uint32(windows.WAIT_TIMEOUT) {
				return nil, fmt.Errorf("wait embed pipe: %w", err)
			}
			select {
			case <-stopCh:
				return nil, errors.New("process exited before connecting embed channel")
			default:
			}
		}
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed.Load() {
		return nil, errors.New("embed channel closed")
	}
	if c.connEv != 0 {
		_ = windows.CloseHandle(c.connEv)
		c.connEv = 0
	}
	f := os.NewFile(uintptr(c.pipeH), `\\.\pipe\winapk-`+c.token)
	c.pipeH = 0
	c.pipe = f
	return f, nil
}

// readFrame copies the latest published RGBA8 slot into a double-buffered *image.RGBA
// safe for MyGo's Bitmap.Update without racing against subsequent child frames.
func (c *embedChannel) readFrame() *image.RGBA {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed.Load() || c.hdr == nil || len(c.mem) < embedHeaderLen {
		return nil
	}
	slot := atomic.LoadInt32(&c.hdr.activeSlot)
	if slot < 0 || slot > 1 {
		return nil
	}
	w := int(atomic.LoadUint32(&c.hdr.width[slot]))
	h := int(atomic.LoadUint32(&c.hdr.height[slot]))
	if w <= 0 || h <= 0 || uint64(w)*uint64(h) > uint64(c.hdr.maxPixels) {
		return nil
	}
	slotBytes := int(c.hdr.maxPixels) * 4
	need := w * h * 4
	off := embedHeaderLen + int(slot)*slotBytes
	if off+need > len(c.mem) {
		return nil
	}

	dst := c.bufs[c.bufIdx]
	if dst == nil || dst.Rect.Dx() != w || dst.Rect.Dy() != h {
		dst = image.NewRGBA(image.Rect(0, 0, w, h))
		c.bufs[c.bufIdx] = dst
	}
	c.bufIdx ^= 1
	copy(dst.Pix, c.mem[off:off+need])
	return dst
}

// send writes one control command line to the connected runtime process.
func (c *embedChannel) send(line string) error {
	c.mu.Lock()
	f := c.pipe
	c.mu.Unlock()
	if f == nil || c.closed.Load() {
		return errors.New("embed channel not connected")
	}
	_, err := f.WriteString(line + "\n")
	return err
}

// close releases the pipe and shared memory handles idempotently.
func (c *embedChannel) close() {
	if !c.closed.CompareAndSwap(false, true) {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.pipe != nil {
		_ = c.pipe.Close()
		c.pipe = nil
	}
	if c.connEv != 0 {
		_ = windows.CloseHandle(c.connEv)
		c.connEv = 0
	}
	if c.pipeH != 0 {
		_ = windows.CloseHandle(c.pipeH)
		c.pipeH = 0
	}
	if len(c.mem) > 0 {
		addr := uintptr(unsafe.Pointer(&c.mem[0]))
		c.mem = nil
		c.hdr = nil
		_ = windows.UnmapViewOfFile(addr)
	}
	if c.mapH != 0 {
		_ = windows.CloseHandle(c.mapH)
		c.mapH = 0
	}
}
