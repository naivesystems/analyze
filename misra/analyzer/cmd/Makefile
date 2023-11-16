LIMIT=0
NUM_WORKERS=0
LANG="zh"
all:
	go generate naive.systems/analyzer/misra/...
	go build -tags static -o misra_analyzer -ldflags "-X main.linesLimitStr=$(LIMIT) -X main.numWorkersStr=$(NUM_WORKERS) -X main.lang=$(LANG)"
