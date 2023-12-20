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

package sender

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/golang/glog"
	"github.com/google/uuid"
)

const (
	receiverURL = "https://naivesystems.com/receiver.php"
	maxRetries  = 3
	retryDelay  = 5 * time.Second
)

type jsonObject = map[string]any

var q = make(chan jsonObject, 1000)

var pendingMessages sync.WaitGroup

var (
	waitStartMutex sync.Mutex
	waitStarted    bool
	waitStartTime  time.Time
)

var initData = jsonObject{}

var sessionID = uuid.NewString()

func initUtsname() {
	var utsname syscall.Utsname

	err := syscall.Uname(&utsname)
	if err != nil {
		return
	}

	initData["utsname"] = jsonObject{
		"sysname":    charArrayToString(utsname.Sysname[:]),
		"nodename":   charArrayToString(utsname.Nodename[:]),
		"release":    charArrayToString(utsname.Release[:]),
		"version":    charArrayToString(utsname.Version[:]),
		"machine":    charArrayToString(utsname.Machine[:]),
		"domainname": charArrayToString(utsname.Domainname[:]),
	}
}

// extractCPUModelName reads /proc/cpuinfo and extracts the CPU model name.
func extractCPUModelName() (string, error) {
	file, err := os.Open("/proc/cpuinfo")
	if err != nil {
		return "", err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "model name") {
			parts := strings.SplitN(line, ":", 2)
			if len(parts) == 2 {
				return strings.TrimSpace(parts[1]), nil
			}
		}
	}

	if err := scanner.Err(); err != nil {
		return "", err
	}

	return "", nil // No model name found
}

func initCPUInfo() {
	cpuModelName, err := extractCPUModelName()
	if err != nil {
		return
	}

	// Ensure the nested maps exist
	if _, ok := initData["proc"]; !ok {
		initData["proc"] = make(jsonObject)
	}
	procInfo, _ := initData["proc"].(jsonObject)

	if _, ok := procInfo["cpuinfo"]; !ok {
		procInfo["cpuinfo"] = make(jsonObject)
	}
	cpuInfo, _ := procInfo["cpuinfo"].(jsonObject)

	// Add model name to the map
	cpuInfo["model name"] = cpuModelName
}

func initDMIInfo() {
	baseDir := "/sys/class/dmi/id/"

	entries, err := os.ReadDir(baseDir)
	if err != nil {
		return
	}

	// Ensure the nested maps exist
	if _, ok := initData["sys"]; !ok {
		initData["sys"] = make(jsonObject)
	}
	sysInfo, _ := initData["sys"].(jsonObject)

	if _, ok := sysInfo["class"]; !ok {
		sysInfo["class"] = make(jsonObject)
	}
	classInfo, _ := sysInfo["class"].(jsonObject)

	if _, ok := classInfo["dmi"]; !ok {
		classInfo["dmi"] = make(jsonObject)
	}
	dmiInfo, _ := classInfo["dmi"].(jsonObject)

	if _, ok := dmiInfo["id"]; !ok {
		dmiInfo["id"] = make(jsonObject)
	}
	idInfo, _ := dmiInfo["id"].(jsonObject)

	for _, entry := range entries {
		if entry.IsDir() {
			// Skip directories
			continue
		}

		filePath := filepath.Join(baseDir, entry.Name())

		content, err := os.ReadFile(filePath)
		if err != nil {
			// Skip files that cannot be read (e.g., due to permissions)
			continue
		}

		// Trim space and add to map
		idInfo[entry.Name()] = strings.TrimSpace(string(content))
	}
}

func newHTTPClient() *http.Client {
	return &http.Client{
		Transport: &http.Transport{
			TLSHandshakeTimeout:   retryDelay,
			DisableKeepAlives:     true,
			MaxIdleConns:          1,
			MaxConnsPerHost:       1,
			IdleConnTimeout:       1 * time.Millisecond,
			ResponseHeaderTimeout: retryDelay,
		},
		Timeout: 30 * time.Second,
	}
}

func init() {
	hostname, err := os.Hostname()
	if err == nil {
		initData["hostname"] = hostname
	}

	if runtime.GOOS == "linux" {
		initUtsname()
		initCPUInfo()
		initDMIInfo()
	}

	go func() {
		firstMessage := true
		client := newHTTPClient()

		for data := range q {
			if firstMessage {
				for key, value := range initData {
					data[key] = value
				}
			}

			jsonData, err := json.Marshal(data)
			if err != nil {
				continue
			}

			retryCount := 0

			for {
				if hasWaitedTooLong() {
					break
				}

				// Attempt to send POST request.
				err := sendPostRequest(client, jsonData)
				client.CloseIdleConnections()
				if err == nil {
					// If successful, break the loop.
					firstMessage = false
					break
				}

				// Create a new client if an error occurred.
				client = newHTTPClient()

				retryCount++
				if retryCount >= maxRetries {
					// If max retries exceeded, give up on this message.
					break
				}

				// Wait for a bit before retrying.
				time.Sleep(retryDelay)
			}

			pendingMessages.Done()
		}
	}()
}

func sendPostRequest(client *http.Client, jsonData []byte) error {
	// Create a new POST request with jsonData as body.
	req, err := http.NewRequest("POST", receiverURL, bytes.NewBuffer(jsonData))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")

	// Send the request.
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 400 {
		return fmt.Errorf("resp status %d %s", resp.StatusCode, resp.Status)
	}

	return nil
}

func charArrayToString(ca []int8) string {
	var bs []byte
	for _, c := range ca {
		if c == 0 {
			break
		}
		bs = append(bs, byte(c))
	}
	return string(bs)
}

func Send(msg string, args ...interface{}) {
	// Initialize a map to hold the key-value pairs.
	data := jsonObject{
		"session_id": sessionID,
		"id":         uuid.NewString(),
		"msg":        msg,
	}

	// Iterate over the args to populate the map.
	// Assume that args are provided in key-value pairs.
	for i := 0; i < len(args); i += 2 {
		key, ok := args[i].(string)
		if !ok || i+1 >= len(args) {
			glog.Errorf("invalid argument at position %d", i)
			return
		}
		data[key] = args[i+1]
	}

	// Send the JSON to the channel.
	pendingMessages.Add(1)
	q <- data
}

func Wait() {
	setWaitStarted()
	pendingMessages.Wait()
}

func setWaitStarted() {
	waitStartMutex.Lock()
	defer waitStartMutex.Unlock()
	waitStarted = true
	waitStartTime = time.Now()
}

func hasWaitedTooLong() bool {
	waitStartMutex.Lock()
	defer waitStartMutex.Unlock()
	return waitStarted && time.Since(waitStartTime) > 1*time.Minute
}
