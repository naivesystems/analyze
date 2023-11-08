include ../scripts/Makefile.lib

all:
	$(MAKE) -C analyzer
	bazel build $(BAZEL_OPTS) //misra/...
