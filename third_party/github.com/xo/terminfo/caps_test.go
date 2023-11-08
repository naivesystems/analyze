package terminfo

import (
	"testing"
)

func TestCapSizes(t *testing.T) {
	if CapCountBool*2 != len(boolCapNames) {
		t.Fatalf("boolCapNames should have same length as twice CapCountBool")
	}
	if CapCountNum*2 != len(numCapNames) {
		t.Fatalf("numCapNames should have same length as twice CapCountNum")
	}
	if CapCountString*2 != len(stringCapNames) {
		t.Fatalf("stringCapNames should have same length as twice CapCountString")
	}
}

func TestCapNames(t *testing.T) {
	for i := 0; i < CapCountBool; i++ {
		n, s := BoolCapName(i), BoolCapNameShort(i)
		if n == "" {
			t.Errorf("Bool cap %d should have name", i)
		}
		if s == "" {
			t.Errorf("Bool cap %d should have short name", i)
		}
		if n == s {
			t.Errorf("Bool cap %d name and short name should not equal (%s==%s)", i, n, s)
		}
	}
	for i := 0; i < CapCountNum; i++ {
		n, s := NumCapName(i), NumCapNameShort(i)
		if n == "" {
			t.Errorf("Num cap %d should have name", i)
		}
		if s == "" {
			t.Errorf("Num cap %d should have short name", i)
		}
		if n == s && n != "lines" {
			t.Errorf("Num cap %d name and short name should not equal (%s==%s)", i, n, s)
		}
	}
	for i := 0; i < CapCountString; i++ {
		n, s := StringCapName(i), StringCapNameShort(i)
		if n == "" {
			t.Errorf("String cap %d should have name", i)
		}
		if s == "" {
			t.Errorf("String cap %d should have short name", i)
		}
		if n == s && n != "tone" && n != "pulse" {
			t.Errorf("String cap %d name and short name should not equal (%s==%s)", i, n, s)
		}
	}
}
