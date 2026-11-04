package winapk

import (
	"context"
	"testing"
	"time"
)

// TestMockStates: the Mock's processes end as the interface's sample
// screens expect, and none starts without a Java runtime.
func TestMockStates(t *testing.T) {
	cases := []struct {
		path     string
		native   Native
		phase    Phase
		category string
	}{
		{"sample:NotesLite.apk", NativeOff, Running, ""},
		{"sample:TilePuzzle-2.3.apk", NativeOff, Running, ""},
		{"sample:TilePuzzle-2.3.apk", NativeRestricted, Failed, "GUEST_SYMBOL_UNRESOLVED"},
		{"sample:VulkanCube.apk", NativeRestricted, Failed, "GRAPHICS_CAPABILITY_MISSING"},
		{"sample:OldCalc.apk", NativeOff, Failed, "APK_UNSUPPORTED"},
	}
	for _, tc := range cases {
		m := NewMock(true, 0)
		got := make(chan State, 16)
		if _, err := m.Launch(LaunchSpec{Path: tc.path, Native: tc.native}, func(s State) { got <- s }); err != nil {
			t.Fatal(err)
		}
		var last State
		for done := false; !done; {
			select {
			case last = <-got:
				done = last.Phase != Starting
			case <-time.After(time.Second):
				t.Fatalf("%s: no state after Starting", tc.path)
			}
		}
		if last.Phase != tc.phase || last.Category != tc.category {
			t.Errorf("%s %s: %s %q, want %s %q", tc.path, tc.native, last.Phase, last.Category, tc.phase, tc.category)
		}
	}
	if _, err := NewMock(false, 0).Launch(LaunchSpec{Path: "sample:NotesLite.apk"}, func(State) {}); err == nil {
		t.Fatal("started without a Java runtime")
	}
	m := NewMock(false, 0)
	if err := m.InstallJava(context.Background(), t.TempDir(), func(InstallStep, float64) {}); err != nil {
		t.Fatal(err)
	}
	if r, _ := m.Doctor(context.Background(), ""); !r.Ready() || !r.Java.Own || !r.Demo {
		t.Fatalf("after InstallJava: %+v", r.Java)
	}
}
