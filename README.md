# A C++ VideoCapture class for OpenCV and GigE Cameras

#### Prerequisites
- Linux
- C++ 2017
- Tiscamera (https://github.com/TheImagingSource/tiscamera.git)
- OpenCV 4.55
- GStreamer Video 1.0 (gstreamer-video-1.0)
- GObject Introspection (gobject-introspection-1.0)

#### Build Instructions
To build and run the test application

```
make clean
make -j4
./live-stream
```

#### Notes
- This is very much a work in progess...
- Tested on a Raspberry Pi (64-bit OS) using a DFM-25G445-ML GigE camera (from The Imaging Source)
- Please contact me at max.vandaalen@bitparallel.com if you have any questions
