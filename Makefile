# NaiveSystems Analyze - A tool for static code analysis
# Copyright (C) 2023  Naive Systems Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

export SHELL := /bin/bash
include scripts/Makefile.lib

all:
	bazel build $(BAZEL_OPTS) -- //...
	bazel build $(BAZEL_OPTS) @llvm-project//clang
	bazel build $(BAZEL_OPTS) @llvm-project//clang:clang-extdef-mapping
	bazel build $(BAZEL_OPTS) @llvm-project//clang-tools-extra:clang-query
	bazel build $(BAZEL_OPTS) @llvm-project//clang-tools-extra:clang-tidy
	$(MAKE) -C third_party
	go install google.golang.org/protobuf/cmd/protoc-gen-go
	go install golang.org/x/text/cmd/gotext
	$(MAKE) pb.go
	time go build -tags static ./...
	$(MAKE) -C misra_cpp_2008
	$(MAKE) -C misra_c_2012_crules
	$(MAKE) -C misra
	$(MAKE) out/bin/infer

PRUNE_AND_FIND := find . \( -path ./third_party \
-o -path ./clang-tidy \
-o -path ./misra_c_2012 \
-o -path ./out \
-o -path ./vm \
-o -path ./vscode/nsa-result-highlight/analyze-demo \
-o -path '*/_bad0*' -o -path '*/_good00*' \
-o -path '*/node_modules' \) -prune -o -type f \( ! -iname "catalog.go" \) -name
PROTO_FILES := $(shell $(PRUNE_AND_FIND) '*.proto' | fgrep .proto)

.PHONY: pb.go
pb.go: $(PROTO_FILES:.proto=.pb.go)

%.pb.go: %.proto FORCE
	$(call protoc,$<)

FORCE:

.PHONY: clean
clean:
	echo "Cleaning..."
	$(MAKE) -C misra_cpp_2008 clean
	$(MAKE) -C misra_c_2012_crules clean

# I don't understand, but the touch is needed to make it appear as up to date.
out/bin/infer: podman_image/infer-linux64-v1.1.0.tar
	time tar xf $< -C $(OUT) --strip-components=1
	touch $@

podman_image/infer-linux64-v1.1.0.tar: podman_image/infer-linux64-v1.1.0.tar.xz
	time xz --decompress --force --keep $<

podman_image/infer-linux64-v1.1.0.tar.xz:
	wget https://naivesystems-1305043249.cos.ap-shanghai.myqcloud.com/infer-linux64-v1.1.0.tar.xz.9101e7645b36 -O $@
