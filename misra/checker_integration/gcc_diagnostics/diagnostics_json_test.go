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

package gcc_diagnostics

import "testing"

func TestParseDiagnosticsJson(t *testing.T) {
	for _, testCase := range [...]struct {
		name              string
		raw               string
		expectedLength    int
		expectedKind      string
		expectedMessage   string
		expectedOption    string
		expectedLocLine   int
		expectedLocColumn int
		expectedLocFile   string
	}{
		{
			name:              "basic test",
			raw:               `[{"kind": "warning", "locations": [{"finish": {"byte-column": 9, "display-column": 9, "line": 5, "file": "bad1.c", "column": 9}, "caret": {"byte-column": 3, "display-column": 3, "line": 5, "file": "bad1.c", "column": 3}}], "column-origin": 1, "option": "-Wstringop-overflow=", "children": [{"kind": "note", "locations": [{"finish": {"byte-column": 9, "display-column": 9, "line": 5, "file": "bad1.c", "column": 9}, "caret": {"byte-column": 3, "display-column": 3, "line": 5, "file": "bad1.c", "column": 3}}], "message": "referencing argument 1 of type ‘int *’"}, {"kind": "note", "locations": [{"caret": {"byte-column": 6, "display-column": 6, "line": 1, "file": "bad1.c", "column": 6}}], "message": "in a call to function ‘f’"}], "option_url": "https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wstringop-overflow=", "message": "‘f’ accessing 16 bytes in a region of size 12"}]`,
			expectedLength:    1,
			expectedKind:      "warning",
			expectedMessage:   "‘f’ accessing 16 bytes in a region of size 12",
			expectedOption:    "-Wstringop-overflow=",
			expectedLocLine:   5,
			expectedLocColumn: 3,
			expectedLocFile:   "bad1.c",
		},
		{
			name:              "fmt project",
			raw:               `[{"kind": "error", "column-origin": 1, "children": [{"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}], "locations": [{"finish": {"byte-column": 8, "display-column": 8, "line": 3, "file": "/fmt/src/fmt.cc", "column": 8}, "caret": {"byte-column": 4, "display-column": 4, "line": 3, "file": "/fmt/src/fmt.cc", "column": 4}}], "message": "#error Module not supported."}, {"kind": "error", "column-origin": 1, "children": [], "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "column-origin": 1, "children": [{"kind": "error", "locations": [{"caret": {"byte-column": 42, "display-column": 42, "line": 30, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 42}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 42, "display-column": 42, "line": 55, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 42}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 19, "display-column": 19, "line": 18, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 19}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 42, "display-column": 42, "line": 30, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 42}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 42, "display-column": 42, "line": 55, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 42}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "error", "locations": [{"caret": {"byte-column": 39, "display-column": 39, "line": 41, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 39}}], "message": "missing binary operator before token \"(\""}, {"kind": "fatal error", "locations": [{"finish": {"byte-column": 21, "display-column": 21, "line": 84, "file": "/fmt/src/fmt.cc", "column": 21}, "caret": {"byte-column": 10, "display-column": 10, "line": 84, "file": "/fmt/src/fmt.cc", "column": 10}}], "message": "fmt/args.h: No such file or directory"}], "locations": [{"caret": {"byte-column": 19, "display-column": 19, "line": 18, "file": "/usr/lib64/clang/13/include/stddef.h", "column": 19}}], "message": "missing binary operator before token \"(\""}]`,
			expectedLength:    3,
			expectedKind:      "error",
			expectedMessage:   "#error Module not supported.",
			expectedOption:    "",
			expectedLocLine:   3,
			expectedLocColumn: 4,
			expectedLocFile:   "/fmt/src/fmt.cc",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			if testCase.raw == "" {
				t.Fatal("output is empty")
			}
			diagnostics, err := ParseDiagnosticsJson([]byte(testCase.raw))
			if err != nil || diagnostics == nil {
				t.Fatal(err)
			}
			if len(diagnostics) != testCase.expectedLength {
				t.Fatalf("wrong diagnostics length. parsed: %d, expected: %d.", len(diagnostics), testCase.expectedLength)
			}
			diag := diagnostics[0]
			if diag.Kind != testCase.expectedKind {
				t.Fatalf("wrong diagnostic kind. parsed: %s, expected: %s.", diag.Kind, testCase.expectedKind)
			}
			if diag.Message != testCase.expectedMessage {
				t.Fatalf("wrong diagnostic message. parsed: %s, expected: %s.", diag.Message, testCase.expectedMessage)
			}
			if diag.Option != testCase.expectedOption {
				t.Fatalf("wrong diagnostic option. parsed: %s, expected: %s.", diag.Option, testCase.expectedOption)
			}
			loc := diag.Locations[0].Caret
			if loc.Line != testCase.expectedLocLine {
				t.Fatalf("wrong location line. parsed: %d, expected: %d.", loc.Line, testCase.expectedLocLine)
			}
			if loc.Column != testCase.expectedLocColumn {
				t.Fatalf("wrong location column. parsed: %d, expected: %d.", loc.Column, testCase.expectedLocColumn)
			}
			if loc.File != testCase.expectedLocFile {
				t.Fatalf("wrong location file. parsed: %s, expected: %s.", loc.File, testCase.expectedLocFile)
			}
		})
	}
}

func TestParseJSONWithError(t *testing.T) {
	raw := `[{"kind": "fatal error", "column-origin": 1, "children": [], "locations": [{"finish": {"byte-column": 17, "display-column": 17, "line": 1, "file": "bad1.cc", "column": 17}, "caret": {"byte-column": 10, "display-column": 10, "line": 1, "file": "bad1.cc", "column": 10}}], "message": "classc: No such file or directory"}]`
	if raw == "" {
		t.Fatal()
	}
	braw := []byte(raw)
	diagnostics, err := ParseDiagnosticsJson(braw)
	if err != nil || diagnostics == nil {
		t.Error(err)
		t.Fatal()
	}
	if len(diagnostics) != 1 {
		t.Fatalf("wrong diagnostics length")
	}
	diag := diagnostics[0]
	if diag.Kind != "fatal error" || diag.Message != "classc: No such file or directory" {
		t.Fatalf("wrong diagnostic")
	}
	caret := diag.Locations[0].Caret
	if caret.Line != 1 || caret.Column != 10 || caret.File != "bad1.cc" {
		t.Fatalf("wrong location value")
	}
}
