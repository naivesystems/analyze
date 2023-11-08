package terminfo

import (
	"os"
	"path/filepath"
	"regexp"
	"sync"
	"testing"
)

var termNameCache = struct {
	names map[string]string
	sync.Mutex
}{}

var fileRE = regexp.MustCompile("^([0-9]+|[a-zA-Z])/")

func terms(t *testing.T) map[string]string {
	termNameCache.Lock()
	defer termNameCache.Unlock()

	if termNameCache.names == nil {
		termNameCache.names = make(map[string]string)
		for _, dir := range termDirs() {
			werr := filepath.Walk(dir, func(file string, fi os.FileInfo, err error) error {
				if err != nil {
					return err
				}
				if fi.IsDir() || !fileRE.MatchString(file[len(dir)+1:]) || fi.Mode()&os.ModeSymlink != 0 {
					return nil
				}

				term := filepath.Base(file)
				/*if term != "kterm" {
					return nil
				}*/

				if term == "xterm-old" {
					return nil
				}

				termNameCache.names[term] = file
				return nil
			})
			if werr != nil {
				t.Fatalf("could not walk directory, got: %v", werr)
			}
		}
	}

	return termNameCache.names
}

func termDirs() []string {
	var dirs []string
	for _, dir := range []string{"/lib/terminfo", "/usr/share/terminfo"} {
		fi, err := os.Stat(dir)
		if err != nil && os.IsNotExist(err) {
			continue
		} else if err != nil {
			panic(err)
		}
		if fi.IsDir() {
			dirs = append(dirs, dir)
		}
	}

	return dirs
}
