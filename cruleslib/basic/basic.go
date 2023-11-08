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

/*
This package should not import any packages of other analyzers to
avoid recursive import.
*/
package basic

import (
	"archive/tar"
	"bufio"
	"bytes"
	"compress/gzip"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/golang/glog"
	"golang.org/x/text/message"
)

type combinedOutput struct {
	Output []byte
	Error  error
}

func PrintfWithTimeStamp(format string, arg ...any) {
	prefix := fmt.Sprintf("%v ", time.Now().Format("2006-01-02 15:04:05"))
	message := fmt.Sprintf(prefix+format, arg...)
	fmt.Println(message)
	glog.Info(message)
}

func GetPercentString(v1, v2 int) string {
	percent := (int)((v1 * 100) / v2)
	return fmt.Sprintf("%d%%", percent)
}

func FormatTimeDuration(d time.Duration) string {
	s := d / time.Second
	d -= s * time.Second
	ms := d / time.Millisecond
	if ms == 0 {
		return fmt.Sprintf("%ds", s)
	}
	ms = ms % time.Microsecond
	for ms%10 == 0 && ms != 0 {
		ms = ms / 10
	}
	return fmt.Sprintf("%d.%ds", s, ms)
}

// print checking process serialized, goroutine safe
type CheckingProcessPrinter struct {
	mutex                sync.Mutex
	startedAt            time.Time
	timeElapsed          map[string]time.Time
	startAnalyzeTaskNum  int
	finishAnalyzeTaskNum int
	totalTaskNum         int
}

func NewCheckingProcessPrinter(totalTaskNum int) CheckingProcessPrinter {
	return CheckingProcessPrinter{
		totalTaskNum: totalTaskNum,
		timeElapsed:  make(map[string]time.Time),
		startedAt:    time.Now(),
	}
}

// Called before start checking a rule
func (c *CheckingProcessPrinter) StartAnalyzeTask(ruleName string, printer *message.Printer) {
	c.mutex.Lock()
	c.startAnalyzeTaskNum++
	PrintfWithTimeStamp(printer.Sprintf("Start analyzing for %s (%v/%v)", ruleName, c.startAnalyzeTaskNum, c.totalTaskNum))
	c.timeElapsed[ruleName] = time.Now()
	c.mutex.Unlock()
}

// Called after finish checking a rule
func (c *CheckingProcessPrinter) FinishAnalyzeTask(ruleName string, printer *message.Printer) {
	c.mutex.Lock()
	elapsed := time.Since(c.timeElapsed[ruleName])
	c.finishAnalyzeTaskNum++
	percent := GetPercentString(c.finishAnalyzeTaskNum, c.totalTaskNum) + "%"
	currentFinishedNumber := c.finishAnalyzeTaskNum
	totalRuleNumber := c.totalTaskNum
	timeUsed := FormatTimeDuration(elapsed)
	PrintfWithTimeStamp(printer.Sprintf("Analysis of %s completed (%s, %v/%v) [%s]", ruleName, percent, currentFinishedNumber, totalRuleNumber, timeUsed))
	c.mutex.Unlock()
}

func (c *CheckingProcessPrinter) GetPercentString() string {
	return GetPercentString(c.finishAnalyzeTaskNum, c.totalTaskNum)
}

func (c *CheckingProcessPrinter) GetStartedAt() time.Time {
	return c.startedAt
}

func toInt(raw string) int {
	if raw == "" {
		return 0
	}
	res, err := strconv.Atoi(raw)
	if err != nil {
		panic(err)
	}
	return res
}

func GetTotalAvailMem() (int, error) {
	file, err := os.Open("/proc/meminfo")
	if err != nil {
		return 0, err
	}
	defer file.Close()
	bufio.NewScanner(file)
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		text := strings.ReplaceAll(scanner.Text()[:len(scanner.Text())-2], " ", "")
		keyValue := strings.Split(text, ":")
		if keyValue[0] == "MemAvailable" {
			return toInt(keyValue[1]), nil
		}
	}
	return 0, fmt.Errorf("failed to find MemAvailable")
}

func AppendToFile(fileName, contents string) error {
	file, err := os.OpenFile(fileName, os.O_APPEND|os.O_WRONLY|os.O_CREATE, 0600)
	if err != nil {
		return err
	}
	if _, err = file.WriteString(contents); err != nil {
		return err
	}
	return nil
}

// Get the cgroup node folder of the running analyzer
// /proc/self/cgroup records the cgroup of the current process like:
// 0::/naivesystems_analyzer
func GetSelfCgroupPath() (string, error) {
	selfCgroupFile, err := os.Open("/proc/self/cgroup")
	if err != nil {
		return "", fmt.Errorf("failed to open /proc/self/cgroup")
	}
	defer selfCgroupFile.Close()

	scanner := bufio.NewScanner(selfCgroupFile)
	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.SplitN(line, "::", 2)
		if parts[0] == "0" {
			return filepath.Join("/sys/fs/cgroup", parts[1]), nil
		}
	}
	return "", fmt.Errorf("failed to get the cgroup of the running analyzer")
}

// Create leaf node naivesystems_analyzer, and move the running analyzer and its parent processes to it
func InitCgroup() error {
	selfCgroupPath, err := GetSelfCgroupPath()
	if err != nil {
		return err
	}

	if filepath.Base(selfCgroupPath) == "naivesystems_analyzer" {
		selfCgroupPath = filepath.Join(selfCgroupPath, "../")
	}

	// create cgroup naivesystems_analyzer
	cmd := exec.Command("mkdir", "-p", "naivesystems_analyzer")
	cmd.Dir = selfCgroupPath
	out, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("in %v, falied to execute %v:\n%v", cmd.Dir, cmd.String(), string(out))
	}

	// put pid and ppid of the running analyzer into the cgroup naivesystems_analyzer
	err = AppendToFile(filepath.Join(selfCgroupPath, "naivesystems_analyzer/cgroup.procs"), fmt.Sprint(os.Getppid()))
	if err != nil {
		return fmt.Errorf("failed to modify naivesystems_analyzer/cgroup.procs: %v", err)
	}
	err = AppendToFile(filepath.Join(selfCgroupPath, "naivesystems_analyzer/cgroup.procs"), fmt.Sprint(os.Getpid()))
	if err != nil {
		return fmt.Errorf("failed to modify naivesystems_analyzer/cgroup.procs: %v", err)
	}

	// enable memory control for subtrees of the root cgroup
	err = AppendToFile(filepath.Join(selfCgroupPath, "cgroup.subtree_control"), "+memory")
	if err != nil {
		return fmt.Errorf("failed to modify cgroup.subtree_control: %v", err)
	}

	return nil
}

// Create leaf node for each rule and limit the memory usage for external programmes
func CreateCgroup(taskName string, availMem int) error {
	selfCgroupPath, err := GetSelfCgroupPath()
	if err != nil {
		return err
	}
	if filepath.Base(selfCgroupPath) != "naivesystems_analyzer" {
		err := InitCgroup()
		if err != nil {
			return err
		}
	} else {
		cmd := exec.Command("cat", filepath.Join(selfCgroupPath, "../cgroup.subtree_control"))
		out, err := cmd.CombinedOutput()
		if err != nil {
			return fmt.Errorf("failed to execute %v:\n%v", cmd.String(), string(out))
		}
		if strings.Trim(string(out), "\n") == "" {
			err := InitCgroup()
			if err != nil {
				return err
			}
		}
	}

	selfCgroupPath = strings.Split(selfCgroupPath, "naivesystems_analyzer")[0]

	cmd := exec.Command("mkdir", "-p", taskName)
	cmd.Dir = selfCgroupPath
	err = cmd.Run()
	if err != nil {
		return fmt.Errorf("failed to create cgroup: %v", err)
	}

	taskCgroupPath := filepath.Join(selfCgroupPath, taskName)

	err = os.WriteFile(filepath.Join(taskCgroupPath, "memory.swap.max"), []byte("0"), 0644)
	if err != nil {
		return fmt.Errorf("failed to disable swap memory: %v", err)
	}

	err = os.WriteFile(filepath.Join(taskCgroupPath, "memory.max"), []byte(fmt.Sprint(availMem*1024)), 0644)
	if err != nil {
		return fmt.Errorf("failed to limit memory usage: %v", err)
	}

	glog.Infof("%v KB memory is available for %s", availMem, taskName)
	return nil
}

// Based on the implementation of exec.Cmd.Run, add pid to the
// corresponding cgroup to limit the memory usage of external programmes.
func Run(c *exec.Cmd, taskName string, limitMemory bool) error {
	if !limitMemory {
		return c.Run()
	}
	selfCgroupPath, err := GetSelfCgroupPath()
	if err != nil {
		return err
	}
	selfCgroupPath = strings.Split(selfCgroupPath, "naivesystems_analyzer")[0]
	err = c.Start()
	if err == nil {
		err = c.Process.Signal(syscall.SIGTSTP)
		if err != nil {
			glog.Errorf("failed to send syscall.SIGTSTP to command %s", c.String())
		}
		err = AppendToFile(filepath.Join(selfCgroupPath, taskName, "cgroup.procs"), fmt.Sprint(c.Process.Pid))
		if err != nil {
			glog.Errorf("failed to add %s to cgroup %s", fmt.Sprint(c.Process.Pid), taskName)
		}
		err = c.Process.Signal(syscall.SIGCONT)
		if err != nil {
			glog.Errorf("failed to send syscall.SIGCONT to command %s", c.String())
		}
		err = c.Wait()
	}
	return err
}

// Based on the implementation of exec.Cmd.CombinedOutput, add pid to the
// corresponding cgroup to limit the memory usage of external programmes.
func getCombinedOutput(c *exec.Cmd, taskName string, limitMemory bool) combinedOutput {
	if !limitMemory {
		output, err := c.CombinedOutput()
		return combinedOutput{Output: output, Error: err}
	}
	selfCgroupPath, err := GetSelfCgroupPath()
	if err != nil {
		return combinedOutput{Output: nil, Error: err}
	}
	selfCgroupPath = strings.Split(selfCgroupPath, "naivesystems_analyzer")[0]
	if c.Stdout != nil {
		return combinedOutput{Output: nil, Error: errors.New("exec: Stdout already set")}
	}
	if c.Stderr != nil {
		return combinedOutput{Output: nil, Error: errors.New("exec: Stderr already set")}
	}
	var b bytes.Buffer
	c.Stdout = &b
	c.Stderr = &b
	err = c.Start()
	if err == nil {
		err = c.Process.Signal(syscall.SIGTSTP)
		if err != nil {
			glog.Errorf("failed to send syscall.SIGTSTP to command %s", c.String())
		}
		err = AppendToFile(filepath.Join(selfCgroupPath, taskName, "cgroup.procs"), fmt.Sprint(c.Process.Pid))
		if err != nil {
			glog.Errorf("failed to add %s to cgroup %s", fmt.Sprint(c.Process.Pid), taskName)
		}
		err = c.Process.Signal(syscall.SIGCONT)
		if err != nil {
			glog.Errorf("failed to send syscall.SIGCONT to command %s", c.String())
		}
		err = c.Wait()
	}
	return combinedOutput{Output: b.Bytes(), Error: err}
}

func CombinedOutput(c *exec.Cmd, taskName string, limitMemory bool, timeoutNormal, timeoutOom int) ([]byte, error) {
	var timeoutMinute int = timeoutNormal
	if limitMemory && strings.Contains(c.String(), "infer") {
		timeoutMinute = timeoutOom
	}
	result := make(chan combinedOutput, 1)
	go func() {
		result <- getCombinedOutput(c, taskName, limitMemory)
	}()
	select {
	case <-time.After(time.Duration(timeoutMinute) * time.Minute):
		err := c.Process.Kill()
		if err != nil {
			return nil, fmt.Errorf("failed to kill %v: %v", c.Process.Pid, err)
		}
		return nil, fmt.Errorf("%v timed out: over %v minutes", taskName, timeoutMinute)
	case result := <-result:
		return result.Output, result.Error
	}
}

func TarFile(srcDir string, fileName string) error {
	fw, err := os.Create(fileName)
	if err != nil {
		return fmt.Errorf("create zip file error: %v", err)
	}
	defer fw.Close()
	gw := gzip.NewWriter(fw)
	defer gw.Close()
	tw := tar.NewWriter(gw)
	defer tw.Close()
	err = filepath.Walk(srcDir, func(path string, info fs.FileInfo, err error) error {
		if err != nil {
			return fmt.Errorf("walk func failed: %v", err)
		}
		hdr, err := tar.FileInfoHeader(info, "")
		if err != nil {
			return fmt.Errorf("get header failed: %v", err)
		}
		hdr.Name, err = filepath.Rel(filepath.Dir(srcDir), path)
		if err != nil {
			return fmt.Errorf("failed to get relative path of %v to %v: %v", path, filepath.Dir(srcDir), err)
		}
		err = tw.WriteHeader(hdr)
		if err != nil {
			return fmt.Errorf("write header failed: %v", err)
		}
		if !info.Mode().IsRegular() {
			return nil
		}
		fr, err := os.Open(path)
		if err != nil {
			glog.Errorf("Open failed: %v", err)
		}
		defer fr.Close()
		_, err = io.CopyN(tw, fr, info.Size())
		if err != nil {
			glog.Errorf("Copy failed: %v", err)
		}
		return nil
	})
	if err != nil {
		return fmt.Errorf("walk func error: %v", err)
	}
	return nil
}

func GetOperatingSystemType() (string, string, error) {
	file, err := os.Open("/etc/os-release")
	if err != nil {
		return "", "", fmt.Errorf("cannot read /etc/os-release: %v", err)
	}

	defer file.Close()

	scanner := bufio.NewScanner(file)
	var osType, osVersion string
	for scanner.Scan() {
		if strings.HasPrefix(scanner.Text(), "ID=") {
			osType = strings.Trim(strings.TrimPrefix(scanner.Text(), "ID="), "\"")
		} else if strings.HasPrefix(scanner.Text(), "VERSION_ID=") {
			osVersion = strings.Trim(strings.TrimPrefix(scanner.Text(), "VERSION_ID="), "\"")
		}
	}

	if err := scanner.Err(); err != nil {
		return "", "", fmt.Errorf("cannot read /etc/os-release: %v", err)
	}

	return osType, osVersion, nil
}

func ConvertRelativePathToAbsolute(dir, path string) (string, error) {
	if !strings.HasPrefix(path, "/") {
		fullpath := filepath.Join(dir, path)
		_, err := os.Stat(fullpath) // Sanity Check: This file should exist.
		if errors.Is(err, os.ErrNotExist) {
			return path, fmt.Errorf("convertRelativePathToAbsolute: %v", err)
		}
		return fullpath, nil
	} else {
		return path, nil
	}
}
