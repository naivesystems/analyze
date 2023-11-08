include ../scripts/Makefile.lib

all:
	bazel build $(BAZEL_OPTS) //misra/...

clean:
	git clean -fdx rule_*
	git clean -fdx dir_*
