# A C++ VideoCapture class for OpenCV and GigE Cameras

#### Prerequisites
- Linux
- C++ 2017
- Tiscamera API (https://github.com/TheImagingSource/tiscamera.git)
- OpenCV 4.55 (older versions are OK)
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
- This is very much a work in progess and is likely to evolve
- Tested on a Raspberry Pi 4 running the official 64-bit OS and using a DFM-25G445-ML GigE camera (obtained from The Imaging Source)
- Please contact me at max.vandaalen@bitparallel.com if you have any questions
