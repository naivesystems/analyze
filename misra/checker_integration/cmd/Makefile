include ../../../scripts/Makefile.lib

dep:
	$(PROTOC) -I=$(ROOT) --go_out=$(ROOT) $(ROOT)/analyzer/proto/analyzer.proto
	$(PROTOC) -I=$(ROOT) --go_out=$(ROOT) $(ROOT)/config/config.proto

run: dep
	go run main.go -check_rules=$(check_rules) -compile_commands_path=$(compile_commands_path) -alsologtostderr
