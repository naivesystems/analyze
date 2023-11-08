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

package atomic

import (
	"fmt"
	"os"
	"path/filepath"
)

func Write(name string, data []byte) error {
	pattern := "tmp-*-" + filepath.Base(name)
	f, err := os.CreateTemp(filepath.Dir(name), pattern)
	if err != nil {
		return fmt.Errorf("os.CreateTemp: %v", err)
	}
	defer os.Remove(f.Name())
	// Explicitly set the permissions of the temporary file to 0644
	if err := os.Chmod(f.Name(), 0644); err != nil {
		return fmt.Errorf("os.Chmod: %v", err)
	}
	_, err = f.Write(data)
	if err != nil {
		return fmt.Errorf("failed to write to file %s: %v", f.Name(), err)
	}
	err = os.Rename(f.Name(), name)
	if err != nil {
		return fmt.Errorf("failed to rename file %s to %s: %v", f.Name(), name, err)
	}
	return nil
}
