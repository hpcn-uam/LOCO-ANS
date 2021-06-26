
all: release
CFLAGS = -Wall -std=c++14 $(sources) `pkg-config --cflags  opencv`

sources :=  $(wildcard src/*.cc)
deps  := $(wildcard src/*.cc) $(wildcard src/*.h) $(wildcard src/ANS_tables/*.dat )


debug: CFLAGS += -g -DDEBUG -DANALYSIS_CODE
debug: loco_ans_codec

release: CFLAGS += -DNDEBUG -O3
release: loco_ans_codec


loco_ans_codec: $(deps)
	g++ $(CFLAGS)  -o "$@"  `pkg-config  --libs  opencv`

clean:
	rm -f loco_ans_codec