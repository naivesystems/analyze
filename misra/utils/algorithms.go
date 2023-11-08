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

func IntMax(a, b int) int {
	if a > b {
		return a
	} else {
		return b
	}
}

func IntMin(a, b int) int {
	if a < b {
		return a
	} else {
		return b
	}
}

type RecursiveTarjanSCCParams struct {
	dfn, low         map[string]int
	dfnCnt, stackCnt int
	visited          map[string]bool
	stack            []string
	result           [][]string
	graph            *map[string](map[string]struct{})
}

// Notice: Strongly Connected Components with only one node will *NOT* be reported by this function.
func RecursiveTarjanSCC(graph *map[string](map[string]struct{})) [][]string {
	result := make([][]string, 0)
	param := RecursiveTarjanSCCParams{}
	param.dfn = make(map[string]int)
	param.low = make(map[string]int)
	param.visited = make(map[string]bool)
	param.stack = make([]string, 0)
	param.result = make([][]string, 0)
	param.graph = graph
	for node := range *graph {
		_, ok := param.dfn[node]
		if !ok {
			RecursiveTarjanSCCSubroutine(node, &param)
			result = append(result, param.result...)
			param.result = make([][]string, 0)
		}
	}
	return result
}

func RecursiveTarjanSCCSubroutine(u string, p *RecursiveTarjanSCCParams) {
	p.visited[u] = true
	p.dfn[u] = p.dfnCnt
	p.low[u] = p.dfnCnt
	p.dfnCnt += 1
	p.stack = append(p.stack[:p.stackCnt], u)
	p.stackCnt += 1
	for v := range (*p.graph)[u] {
		_, ok := p.dfn[v]
		if !ok {
			RecursiveTarjanSCCSubroutine(v, p)
			p.low[u] = IntMin(p.low[u], p.low[v])
		} else if p.visited[v] {
			p.low[u] = IntMin(p.low[u], p.low[v])
		}
	}
	if p.dfn[u] == p.low[u] {
		chain := make([]string, 0)
		chain = append(chain, u)
		for {
			v := p.stack[p.stackCnt-1]
			p.stackCnt -= 1
			p.visited[v] = false
			if v == u {
				break
			}
			chain = append(chain, v)
		}
		if len(chain) > 1 {
			p.result = append(p.result, chain)
		}
	}
}
