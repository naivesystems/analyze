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

package cpumem

import (
	"fmt"
	"sync"
	"time"

	"github.com/golang/glog"
	"naive.systems/analyzer/cruleslib/basic"
)

var (
	remainLock sync.Mutex
	remainCond *sync.Cond
	remainCpu  int
	remainMem  int
	totalCpu   int
	totalMem   int
)

func Init(cpu, mem int) {
	remainCond = sync.NewCond(&remainLock)
	remainCpu = cpu
	remainMem = mem
	totalCpu = cpu
	totalMem = mem
}

func Acquire(cpu, mem int, taskName string) error {
	cpuExceedMessage := ""
	memExceedMessage := ""
	if cpu > totalCpu {
		cpuExceedMessage = fmt.Sprintf("%s aquired %d cpus, but total %d cpus available\n", taskName, cpu, totalCpu)
	}
	if mem > totalMem {
		memExceedMessage = fmt.Sprintf("%s aquired %d KB memory, but total %d KB memory available\n", taskName, cpu, totalCpu)
	}
	if cpuExceedMessage+memExceedMessage != "" {
		return fmt.Errorf(cpuExceedMessage + memExceedMessage)
	}
	start := time.Now()
	remainLock.Lock()
	for remainCpu < cpu || remainMem < mem {
		remainCond.Wait()
	}
	remainCpu -= cpu
	remainMem -= mem
	remainLock.Unlock()
	elapsed := time.Since(start)
	glog.Infof("%s waited for [%s] to acquire resources", taskName, basic.FormatTimeDuration(elapsed))
	remainCond.Signal()
	return nil
}

func Release(cpu, mem int) {
	remainLock.Lock()
	remainCpu += cpu
	remainMem += mem
	remainLock.Unlock()
	remainCond.Signal()
}

func GetTotalMem() int {
	return totalMem
}
