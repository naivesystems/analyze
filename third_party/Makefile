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

include ../scripts/Makefile.lib

KERNEL:=$(shell uname -s)

ifeq ($(KERNEL),Darwin)
NPROC:=$(shell sysctl -n hw.ncpu)
else
NPROC:=$(shell nproc)
endif

all:
	bazel build $(BAZEL_OPTS) @com_google_protobuf//:protoc
	$(MAKE) -C cppcheck -j $(NPROC)
	./build_libgit2
