/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2023  Naive Systems Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

package diff

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
)

type Hunk struct {
	OldPos, OldLines, NewPos, NewLines int
}

type File struct {
	NewName string
	OldName string
	Hunks   []*Hunk
}

type Patch struct {
	Files []*File
}

/*
Parse parses the diff into a patch struct.

It goes over the lines in the diff and maintain an implicit state machine. It
only cares about lines that start with "--- ", "+++ ", or "@@ -", and ignores
everything else.

For a particular file in the diff, there are three cases to consider:

1. File modification

It usually looks like this:

	diff --git a/docs/Makefile b/docs/Makefile
	index 602565a30b39..9ff7b4d33b07 100644
	--- a/docs/Makefile
	+++ b/docs/Makefile
	@@ -2,12 +2,11 @@ all:
	 		$(MAKE) -C weixin

	 deploy:
	-	ssh staging-docs rm -f docs.tar.xz ...
	-	rsync -aP staging-docs:docs.tar.xz ...
	+	mv ../docs_website/docs_site/docs.tar.xz ...
	 	rsync -aP docs.tar.xz docs.naivesystems.com: ...
	 	ssh docs.naivesystems.com ...

Lines starting with "diff" or "index" are ignored.

2. File addition

Example:

	diff --git a/docs_site/README.txt b/docs_site/README.txt
	new file mode 100644
	index 000000000000..dad6695563d6
	--- /dev/null
	+++ b/docs_site/README.txt
	@@ -0,0 +1,27 @@
	+DO NOT BUILD DIRECTLY. OTHERWISE YOU WILL SEE ERRORS:
	+
	+    Attempting to create page at XXX, but ...
	+

OldName is set to the empty string in this case.

3. File deletion

Example:

	diff --git a/.ruby-version b/.ruby-version
	deleted file mode 100644
	index a603bb50a29e..000000000000
	--- a/.ruby-version
	+++ /dev/null
	@@ -1 +0,0 @@
	-2.7.5

NewName is set to the empty string in this case.
*/
func Parse(diff string) (*Patch, error) {
	re := regexp.MustCompile(`@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@`)
	lines := strings.Split(diff, "\n")
	var p Patch
	var f *File
	for i, line := range lines {
		if strings.HasPrefix(line, "--- ") {
			f = &File{}
			if line == "--- /dev/null" {
				// file addition
				f.OldName = ""
			} else if strings.HasPrefix(line, "--- a/") {
				f.OldName = strings.TrimPrefix(line, "--- a/")
			} else {
				return nil, fmt.Errorf("invalid line %d '%s'", i, line)
			}
			p.Files = append(p.Files, f)
		} else if strings.HasPrefix(line, "+++ ") {
			if f == nil || len(f.Hunks) > 0 {
				return nil, fmt.Errorf("unexpected line %d '%s'", i, line)
			}
			if line == "+++ /dev/null" {
				// file deletion
				f.NewName = ""
			} else if strings.HasPrefix(line, "+++ b/") {
				f.NewName = strings.TrimPrefix(line, "+++ b/")
			} else {
				return nil, fmt.Errorf("invalid line %d '%s'", i, line)
			}
		} else if strings.HasPrefix(line, "@@ -") {
			match := re.FindStringSubmatch(line)
			if match == nil {
				return nil, fmt.Errorf("could not extract hunk info from line '%s'", line)
			}
			oldpos, err := strconv.Atoi(match[1])
			if err != nil {
				return nil, fmt.Errorf("error converting oldpos to integer in '%s': %v", line, err)
			}
			oldlines := 1
			if match[2] != "" {
				oldlines, err = strconv.Atoi(match[2])
				if err != nil {
					return nil, fmt.Errorf("error converting oldlines to integer in '%s': %v", line, err)
				}
			}
			newpos, err := strconv.Atoi(match[3])
			if err != nil {
				return nil, fmt.Errorf("error converting newpos to integer in '%s': %v", line, err)
			}
			newlines := 1
			if match[4] != "" {
				newlines, err = strconv.Atoi(match[4])
				if err != nil {
					return nil, fmt.Errorf("error converting newlines to integer in '%s': %v", line, err)
				}
			}
			if f == nil {
				return nil, fmt.Errorf("f is nil but line %d is '%s'", i, line)
			}
			f.Hunks = append(f.Hunks, &Hunk{oldpos, oldlines, newpos, newlines})
		}
	}
	return &p, nil
}
