TOP_DIR := $(shell pwd)
APP = camera_h264encode

CROSS	 = 
CC	     = $(CROSS)gcc
CPP	     = $(CROSS)g++
STRIP	 = $(CROSS)strip
CFLAGS = -g -Wall

DEP_LIBS = -L./x264
LDFLAGS += -lpthread $(DEP_LIBS) -lx264 -lm -ldl 
INC = -I./include 

all: $(APP)

OBJS = main.o video_capture.o h264encoder.o
SRC = main.cpp video_capture.cpp h264encoder.cpp

$(APP):$(OBJS)
	$(CPP) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) 
	$(STRIP) $@
%.o:%.cpp
	$(CPP) $(INC) $(CFLAGS) -c $< -o $@ 

clean:
	rm -f *.o $(APP) *.264
