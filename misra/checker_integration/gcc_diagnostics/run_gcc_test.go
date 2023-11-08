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

import (
	"strings"
	"testing"
)

func TestFilterClangSpecificOptions(t *testing.T) {
	unfilteredArgs := "/usr/bin/gcc -c -fdiagnostics-format=json /src/scripts/mod/devicetable-offsets.c -I./arch/x86/include -I./arch/x86/include/generated -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -D__KERNEL__ -DCC_USING_NOP_MCOUNT -DCC_USING_FENTRY -DKBUILD_MODFILE=\"scripts/mod/devicetable-offsets\" -DKBUILD_BASENAME=\"devicetable_offsets\" -DKBUILD_MODNAME=\"devicetable_offsets\" -D__KBUILD_MODNAME=kmod_devicetable_offsets -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -m64 -mno-80387 -mstack-alignment=8 -mtune=generic -mno-red-zone -mcmodel=kernel -mretpoline-external-thunk -mfentry"
	filteredArgs := strings.Join((FilterClangSpecificOptions(strings.Fields(unfilteredArgs))), " ")
	expectedFilteredArgs := "/usr/bin/gcc -c -fdiagnostics-format=json /src/scripts/mod/devicetable-offsets.c -I./arch/x86/include -I./arch/x86/include/generated -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -D__KERNEL__ -DCC_USING_NOP_MCOUNT -DCC_USING_FENTRY -DKBUILD_MODFILE=\"scripts/mod/devicetable-offsets\" -DKBUILD_BASENAME=\"devicetable_offsets\" -DKBUILD_MODNAME=\"devicetable_offsets\" -D__KBUILD_MODNAME=kmod_devicetable_offsets -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -m64 -mno-80387 -mtune=generic -mno-red-zone -mcmodel=kernel -mfentry"
	if filteredArgs != expectedFilteredArgs {
		t.Errorf("failed to filter clang specific options, expected: %s, actual: %s", expectedFilteredArgs, filteredArgs)
	}
}

func TestTrimTeminationMark(t *testing.T) {
	for _, testCase := range [...]struct {
		name           string
		rawOutput      string
		expectedOutput string
	}{
		{
			name: "with a termination mark",
			rawOutput: `[{"kind": "warning", "locations": [{"finish": {"byte-column": 9, "display-column": 9, "line": 5, "file": "bad1.c", "column": 9}, "caret": {"byte-column": 3, "display-column": 3, "line": 5, "file": "bad1.c", "column": 3}}], "column-origin": 1, "option": "-Wstringop-overflow=", "children": [{"kind": "note", "locations": [{"finish": {"byte-column": 9, "display-column": 9, "line": 5, "file": "bad1.c", "column": 9}, "caret": {"byte-column": 3, "display-column": 3, "line": 5, "file": "bad1.c", "column": 3}}], "message": "referencing argument 1 of type ‘int *’"}, {"kind": "note", "locations": [{"caret": {"byte-column": 6, "display-column": 6, "line": 1, "file": "bad1.c", "column": 6}}], "message": "in a call to function ‘f’"}], "option_url": "https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wstringop-overflow=", "message": "‘f’ accessing 16 bytes in a region of size 12"}]
			compilation terminated.`,
			expectedOutput: `[{"kind": "warning", "locations": [{"finish": {"byte-column": 9, "display-column": 9, "line": 5, "file": "bad1.c", "column": 9}, "caret": {"byte-column": 3, "display-column": 3, "line": 5, "file": "bad1.c", "column": 3}}], "column-origin": 1, "option": "-Wstringop-overflow=", "children": [{"kind": "note", "locations": [{"finish": {"byte-column": 9, "display-column": 9, "line": 5, "file": "bad1.c", "column": 9}, "caret": {"byte-column": 3, "display-column": 3, "line": 5, "file": "bad1.c", "column": 3}}], "message": "referencing argument 1 of type ‘int *’"}, {"kind": "note", "locations": [{"caret": {"byte-column": 6, "display-column": 6, "line": 1, "file": "bad1.c", "column": 6}}], "message": "in a call to function ‘f’"}], "option_url": "https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wstringop-overflow=", "message": "‘f’ accessing 16 bytes in a region of size 12"}]`,
		},
		{
			name:           "nil output",
			rawOutput:      "",
			expectedOutput: "",
		},
		{
			name:           "empty diagnostic",
			rawOutput:      "[]",
			expectedOutput: "[]",
		},
		{
			name:           "empty diagnostic with space",
			rawOutput:      "[] ",
			expectedOutput: "[]",
		},
		{
			name:           "empty diagnostic with new line",
			rawOutput:      "[]\n",
			expectedOutput: "[]",
		},
	} {
		t.Run(testCase.name, func(t *testing.T) {
			out := []byte(testCase.rawOutput)
			TrimTeminationMark(&out)
			if string(out) != testCase.expectedOutput {
				t.Fatalf("unexpected output. parsed: %s, expected: %s.", string(out), testCase.expectedOutput)
			}
		})
	}
}
