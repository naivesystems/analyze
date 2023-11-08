package terminfo

import (
	"os"
	"runtime"
	"testing"
)

func TestLoad(t *testing.T) {
	for term, file := range terms(t) {
		err := os.Setenv("TERM", term)
		if err != nil {
			t.Fatalf("could not set TERM environment variable, got: %v", err)
		}

		// open
		ti, err := LoadFromEnv()
		if err != nil {
			t.Fatalf("term %s expected no error, got: %v", term, err)
		}

		// check the name was saved correctly
		if runtime.GOOS != "darwin" && ti.File != file {
			t.Errorf("term %s should have file %s, got: %s", term, file, ti.File)
		}

		// check we have at least one name
		if len(ti.Names) < 1 {
			t.Errorf("term %s expected names to have at least one value", term)
		}
	}
}
