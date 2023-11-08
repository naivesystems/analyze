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

package podman

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/golang/glog"
	"google.golang.org/protobuf/encoding/protojson"
	pb "naive.systems/analyzer/analyzer/proto"
)

func Run(podmanBinPath,
	rulesPath,
	workingDir,
	logDir,
	imageName,
	invocationPath string,
	checkerConfig *pb.CheckerConfiguration,
	srcDir string,
	ignorePatterns []string,
	projectType,
	qtProPath,
	scriptContents,
	projectName string,
) error {
	marshalOptions := &protojson.MarshalOptions{}
	marshalOptions.UseProtoNames = true
	args := []string{
		"run",
		"--rm",
		"-v", fmt.Sprintf("%s:%s:Z", rulesPath, "/config"),
		"-v", fmt.Sprintf("%s:%s:O", workingDir, "/src"),
		"-v", fmt.Sprintf("%s:%s:Z", logDir, "/output"),
		"-w", "/src",
	}
	if projectType == "maven" || projectType == "other" {
		args = append(args, "--network=private")
		args = append(args, "--userns=keep-id:uid=1000,gid=1000")
	}
	args = append(args, imageName)
	args = append(args, invocationPath)
	if checkerConfig != nil && checkerConfig.CsaSystemLibOptions != "" {
		args = append(args, fmt.Sprintf("--csa_system_lib_options=%s", checkerConfig.CsaSystemLibOptions))
	}
	if srcDir != "" {
		args = append(args, fmt.Sprintf("--src_dir=%s", string(srcDir)))
	}
	if len(ignorePatterns) != 0 {
		for _, ignorePattern := range ignorePatterns {
			args = append(args, fmt.Sprintf("--ignore_dir=/src/%s", ignorePattern))
		}
	}
	if len(projectType) != 0 {
		args = append(args, fmt.Sprintf("--project_type=%s", string(projectType)))
	}
	if len(projectName) != 0 {
		args = append(args, fmt.Sprintf("--project_name=%s", string(projectName)))
	}
	if len(qtProPath) != 0 {
		args = append(args, fmt.Sprintf("--qt_pro_path=%s", qtProPath))
	}
	if len(scriptContents) != 0 {
		args = append(args, fmt.Sprintf("--script_contents=%s", scriptContents))
	}
	args = append(args, "-alsologtostderr")
	cmd := exec.Command(podmanBinPath, args...)
	glog.Infof("executing: $ %s", cmd.String())
	var stderr bytes.Buffer
	cmd.Stderr = &stderr
	err := cmd.Run()
	if err != nil {
		glog.Errorf("%v: %s", err, stderr.String())
		return err
	}
	f, err := os.Create(filepath.Join(logDir, "error.txt"))
	if err != nil {
		glog.Errorf("create file failed: %v", err)
	}
	n, err := f.WriteString(stderr.String())
	if err != nil {
		glog.Errorf("write file failed: %v", err)
	}
	if n < len(stderr.String()) {
		glog.Errorf("write file failed: %v", io.ErrShortWrite)
	}
	err = f.Close()
	if err != nil {
		glog.Errorf("close file failed: %v", err)
	}
	return nil
}

func LoadImage(podmanBinPath,
	imageName string,
	podmanLoadURLs []string,
) error {
	// verify if a image exists
	cmd := exec.Command(podmanBinPath, "image", "exists", imageName)
	glog.Infof("executing: $ %s", cmd.String())
	err := cmd.Run()
	if err == nil {
		glog.Infof("image: %s exists", imageName)
		return nil
	}
	// load image from urls
	for _, url := range podmanLoadURLs {
		if url == "" {
			continue
		}
		cmd = exec.Command(podmanBinPath, "load", "-i", url)
		glog.Infof("executing: $ %s", cmd.String())
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		err = cmd.Run()
		if err != nil {
			glog.Infof("Load image from url %s fails", url)
			continue
		} else {
			glog.Infof("Load image from %s succeed", url)
			return nil
		}
	}
	return err
}
