include ../scripts/Makefile.lib

LIMIT = 0
DEBUG ?= 0
ifeq ($(DEBUG), 0)
	BAZELBUILD = bazel build $(BAZEL_OPTS) -c opt
else
	BAZELBUILD = bazel build $(BAZEL_OPTS)
endif

.PHONY: all
all:
	$(MAKE) build
	$(MAKE) clean

.PHONY: bigmain_googlecpp
bigmain_googlecpp:
	$(BAZELBUILD) bigmain/bigmain_googlecpp

.PHONY: bigmain_autosar
bigmain_autosar:
	$(BAZELBUILD) bigmain/bigmain_autosar

.PHONY: bigmain_misra_c_2012
bigmain_misra_c_2012:
	$(BAZELBUILD) bigmain/bigmain_misra_c_2012

.PHONY: bigmain_misra_cpp_2008
bigmain_misra_cpp_2008:
	$(BAZELBUILD) bigmain/bigmain_misra_cpp_2008

.PHONY: build-base
build-base:
	$(BAZELBUILD) @llvm-project//clang && cp ../bazel-bin/external/llvm-project/clang/clang clang
	$(BAZELBUILD) @llvm-project//clang:clang-extdef-mapping && cp ../bazel-bin/external/llvm-project/clang/clang-extdef-mapping clang-extdef-mapping
	$(BAZELBUILD) @llvm-project//clang-tools-extra:clang-query && cp ../bazel-bin/external/llvm-project/clang-tools-extra/clang-query clang-query
	$(BAZELBUILD) @llvm-project//clang-tools-extra:clang-tidy && cp ../bazel-bin/external/llvm-project/clang-tools-extra/clang-tidy clang-tidy
	$(BAZELBUILD) @com_google_protobuf//:protoc
	go install google.golang.org/protobuf/cmd/protoc-gen-go
	make -C ../ pb.go
	make -C ../third_party/cppcheck && make -C ../misra/analyzer/cmd/ LIMIT=$(LIMIT) NUM_WORKERS=$(NUM_WORKERS) LANG=$(LANG)
	mkdir -p tmpWorkSpace
	$(MAKE) bigmain_misra_c_2012
	cp ../bazel-bin/podman_image/bigmain/bigmain_misra_c_2012 ./tmpWorkSpace
	cp ../misra/analyzer/cmd/misra_analyzer ./tmpWorkSpace
	cp bear/f36/bear-3.0.21-2.fc36.x86_64.rpm ./tmpWorkSpace
	cp clang ./tmpWorkSpace
	cp clang-extdef-mapping ./tmpWorkSpace
	cp clang-query ./tmpWorkSpace
	cp clang-tidy ./tmpWorkSpace
	tar xvf infer-linux64-v1.1.0.tar.xz -C ./tmpWorkSpace
	mv ./tmpWorkSpace/infer-linux64-v1.1.0 ./tmpWorkSpace/infer
	cp -r ../third_party/cppcheck ./tmpWorkSpace
	cp -r ../third_party/github.com/phodina/ProjectConverter ./tmpWorkSpace
	cp -r ../third_party/github.com/cpplint/cpplint/cpplint.py ./tmpWorkSpace
	cp bigmain_symlink ./tmpWorkSpace
	podman build -f ./Containerfile.misrac100k4 -t $(IMAGE_NAME) ./tmpWorkSpace
	$(MAKE) clean

.PHONY: build
build:
	$(MAKE) build-base LIMIT=100000 NUM_WORKERS=4 LANG="zh" IMAGE_NAME="naive.systems/analyzer/misra:dev"

.PHONY: build-en
build-en:
	$(MAKE) build-base LIMIT=100000 NUM_WORKERS=4 LANG="en" IMAGE_NAME="naive.systems/analyzer/misra:dev_en"

.PHONY: build-googlecpp-en
build-googlecpp-en:
	$(MAKE) build-en
	mkdir -p tmpWorkSpace
	$(MAKE) bigmain_googlecpp
	cp ../bazel-bin/podman_image/bigmain/bigmain_googlecpp ./tmpWorkSpace
	podman build -f ./Containerfile.googlecpp --build-arg base_tag=dev_en -t naive.systems/analyzer/googlecpp:dev_en ./tmpWorkSpace
	$(MAKE) clean

.PHONY: build-autosar-en
build-autosar-en:
	$(MAKE) build-misra-cpp-en
	mkdir -p tmpWorkSpace
	$(MAKE) bigmain_autosar
	cp ../bazel-bin/podman_image/bigmain/bigmain_autosar ./tmpWorkSpace
	podman build -f ./Containerfile.autosar --build-arg base_tag=dev_en -t naive.systems/analyzer/autosar:dev_en ./tmpWorkSpace
	$(MAKE) clean

.PHONY: build-misra-cpp-en
build-misra-cpp-en:
	$(MAKE) build
	make -C ../misra/analyzer/cmd/ LIMIT=300000 NUM_WORKERS=0 LANG="en"
	mkdir -p tmpWorkSpace
	cp ../misra/analyzer/cmd/misra_analyzer ./tmpWorkSpace
	$(MAKE) bigmain_misra_cpp_2008
	cp ../bazel-bin/podman_image/bigmain/bigmain_misra_cpp_2008 ./tmpWorkSpace
	podman build -f ./Containerfile.misracpp300k0qt -t naive.systems/analyzer/misracpp:dev_en ./tmpWorkSpace
	$(MAKE) clean

.PHONY: clean
clean:
	rm -rf tmpWorkSpace
	rm -rf naivesystems-analyzer-installer
	rm -f clang
	rm -f clang-extdef-mapping
	rm -f clang-query
	rm -f clang-tidy
	rm -rf misra
	rm -rf misra_cpp_2008
