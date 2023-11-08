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

package baseline

import pb "naive.systems/analyzer/analyzer/proto"

type Locations []*pb.ErrorLocation

func (l Locations) Len() int {
	return len(l)
}

func (l Locations) Less(i, j int) bool {
	if l[i].Path == l[j].Path {
		return l[i].LineNumber < l[j].LineNumber
	} else {
		return l[i].Path < l[j].Path
	}
}

func (l Locations) Swap(i, j int) {
	l[i], l[j] = l[j], l[i]
}
