//go:build !windows

package winapk

import "os/exec"

func hideConsole(*exec.Cmd) {}
