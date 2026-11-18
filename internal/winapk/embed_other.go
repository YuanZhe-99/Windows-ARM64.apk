//go:build !windows

package winapk

import (
	"errors"
	"image"
	"os"
)

type embedChannel struct {
	token string
}

func newEmbedChannel() (*embedChannel, error) {
	return nil, errors.New("winapk: --embed requires Windows")
}

func (c *embedChannel) accept(<-chan struct{}) (*os.File, error) {
	return nil, errors.New("winapk: --embed requires Windows")
}

func (c *embedChannel) readFrame() *image.RGBA { return nil }
func (c *embedChannel) send(string) error      { return errors.New("winapk: --embed requires Windows") }
func (c *embedChannel) close()                 {}
