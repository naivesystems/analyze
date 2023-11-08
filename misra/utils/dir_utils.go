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

package utils

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/golang/glog"
)

func CleanCache(dir string, filesToIgnore []string) error {
	glog.Info("cleaning cache in ", dir)
	filesSaveMap := make(map[string]bool)
	for _, fileName := range filesToIgnore {
		filesSaveMap[fileName] = true
	}
	dirInfo, err := os.ReadDir(dir)
	if err != nil {
		return err
	}
	for _, d := range dirInfo {
		_, ok := filesSaveMap[d.Name()]
		if !ok {
			glog.Infof("remove %s", filepath.Join(dir, d.Name()))
			err = os.RemoveAll(filepath.Join(dir, d.Name()))
			if err != nil {
				return err
			}
		}
	}
	glog.Info("cleaned ", dir)
	return nil
}

func ResovleBinaryPath(binPath string) (string, error) {
	if filepath.IsAbs(binPath) {
		if _, err := os.Stat(binPath); err != nil {
			return binPath, fmt.Errorf("when resolving %s, os.Stat failed: %v", binPath, err)
		}
		return binPath, nil
	}
	// exec.LookPath will silently allow relative path, so we manually check it.
	if strings.Contains(binPath, string(filepath.Separator)) {
		// If invoked by checker_integration.cmd.main, the clang bin might be a relative path instead of executable in $PATH.
		absBinPath, err := filepath.Abs(binPath)
		if err != nil {
			return binPath, fmt.Errorf("when resolving %s, failed to convert to abs path: %v", binPath, err)
		}
		if _, err := os.Stat(absBinPath); err != nil {
			return absBinPath, fmt.Errorf("when resolving %s, os.Stat failed: %v", binPath, err)
		}
		return absBinPath, nil
	} else {
		if _, err := exec.LookPath(binPath); err != nil {
			return binPath, fmt.Errorf("when resolving %s, not found in $PATH: %v", binPath, err)
		}
		return binPath, nil // The binary is in $PATH, just leave it alone
	}
}
