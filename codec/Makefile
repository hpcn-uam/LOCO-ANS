
all: loco_ans_codec

# LIBS := -lopencv_highgui -lopencv_imgcodecs -lopencv_core -lopencv_imgproc
sources :=  $(wildcard src/*.cc)
deps  := $(wildcard src/*.cc) $(wildcard src/*.h) $(wildcard src/ANS_tables/*.dat )
loco_ans_codec: $(deps)
	g++   -Wall -std=c++14 -o "$@" $(sources) `pkg-config --cflags --libs  opencv` -O3 

clean:
	rm -f loco_ans_codec