#
# (c) Bit Parallel Ltd (Max van Daalen), March 2022
#

CC=g++

# FIXME! is gstreamer-1.0 needed?
#
CC_COMPILE_FLAGS=-std=c++17 -O3 -I . `pkg-config --cflags tcam gstreamer-video-1.0 gobject-introspection-1.0 opencv4`
CC_LINK_FLAGS=`pkg-config --libs tcam gstreamer-video-1.0 gobject-introspection-1.0 opencv4`

all: gige-video-capture.o live-stream.o
	$(CC) $(CC_LINK_FLAGS) gige-video-capture.o live-stream.o -o live-stream

gige-video-capture.o: gige-video-capture.cpp
	$(CC) $(CC_COMPILE_FLAGS) -c gige-video-capture.cpp

live-stream.o: live-stream.cpp
	$(CC) $(CC_COMPILE_FLAGS) -c live-stream.cpp

.PHONY clean:
clean:
	rm -f *.o live-stream
