LIMIT=0
NUM_WORKERS=0
LICENSED_TO="测试用户"
LANG="zh"
all:
	go generate naive.systems/analyzer/misra/...
	go build -tags static -o misra_analyzer -ldflags "-X main.linesLimitStr=$(LIMIT) -X main.licensedEntity=$(LICENSED_TO) -X main.numWorkersStr=$(NUM_WORKERS) -X main.lang=$(LANG)"
