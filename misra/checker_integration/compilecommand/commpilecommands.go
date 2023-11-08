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

package compilecommand

import (
	"encoding/json"
	"io"
	"os"

	"github.com/golang/glog"
)

type CompileCommand struct {
	Command   string   `json:"command,omitempty"`
	Arguments []string `json:"arguments,omitempty"`
	File      string   `json:"file"`
	Directory string   `json:"directory"`
	Output    string   `json:"output,omitempty"`
}

const CC1 string = "-cc1"

func (cc CompileCommand) ContainsCC1() bool {
	for _, v := range cc.Arguments {
		if v == CC1 {
			return true
		}
	}
	return false
}

func ReadCompileCommandsFromFile(compileCommandsPath string) (*[]CompileCommand, error) {
	ccFile, err := os.Open(compileCommandsPath)
	if err != nil {
		glog.Error(err)
		return nil, err
	}

	defer ccFile.Close()

	byteContent, err := io.ReadAll(ccFile)
	if err != nil {
		return nil, err
	}

	commands := []CompileCommand{}
	err = json.Unmarshal(byteContent, &commands)
	if err != nil {
		return nil, err
	}

	return &commands, nil
}
