//
// (c) Bit Parallel Ltd (Max van Daalen), March 2022
//

#include <cstdint>
#include <iostream>
#include <vector>

#include <opencv2/opencv.hpp>

#include "gige-video-capture.hpp"

//
// note, as GigE cameras have high network utilisation it may be necessary to increase the network receiver buffer size, use:
//   sudo sysctl -w net.core.rmem_max=10485760
//   sudo sysctl -w net.core.rmem_default=10485760
//
//   see, https://www.flir.co.uk/support-center/iis/machine-vision/knowledge-base/lost-ethernet-data-packets-on-linux-systems/
//

inline static const bool USE_CAMERA_TRIGGER = false;

int32_t main(int32_t argc, char* argv[])
{
    // set the gstreamer default logging level, remove for no logging but always init gstreamer
    // for further details, see https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html
    //
    gst_debug_set_default_threshold(GST_LEVEL_WARNING);
    gst_init(&argc, &argv);

    try
    {
        // notes 1, the following formats are supported by the DFM-25G445-ML camera
        //          (a) video/x-bayer, gbrg (CV_8U, 1), 30/1, 20/1, 15/1, 15/2, 15/4
        //          (b) video/x-raw, GRAY8 (CV_8U, 1), 30/1, 20/1, 15/1, 15/2, 15/4
        //       2, do not use "tcambin" as this includes a bayer conversion and will filter out the frame meta data (i.e. the camera timestamp and framerate)
        //       3, when using trigger mode, set the maximum frame rate otherwise grab() will alias with the camera and potentially miss frames
        //
//      const auto pipeline = "tcamsrc serial=30610380 ! video/x-raw,format=GRAY8,width=1280,height=960,framerate=30/1 ! appsink";
//      const auto pipeline = "tcamsrc serial=30610380 ! video/x-raw,format=GRAY8,width=1280,height=960,framerate=15/1 ! tcamautoexposure ! appsink";
        const auto pipeline = "tcamsrc serial=30610380 ! video/x-bayer,format=gbrg,width=1280,height=960,framerate=15/1 ! tcamautoexposure ! tcamwhitebalance ! appsink";
        auto capture = GigEVideoCapture(pipeline, CV_8U, 1);

        // displaying for reference only, useful when setting pipeline properties
        //
        std::cout << "Pipeline Component Names:\n";
        for (std::string_view name : capture.getPipelineComponentNames()) std::cout << "  " << name << "\n";

        if (USE_CAMERA_TRIGGER)
        {
            // the trigger needs to be disabled before starting the gstreamer pipeline
            //
            capture.setStringProperty("tcamsrc0", "Trigger Source", "Line1");
            capture.setStringProperty("tcamsrc0", "Trigger Activation", "RisingEdge");
            capture.setStringProperty("tcamsrc0", "Trigger Mode", "Off");
        }

        // set any appropriate pre-start pipeline properties
        // notes 1, some properties can be set before the pipeline has been started, others must be set afterwards
        //       2, the default state of "whitebalance-module-enabled" is true
        //       3, white balance only works with colour images
        //       4, it's best to turn off auto white balance when using trigger mode
        //
        capture.setBooleanProperty("tcamwhitebalance0", "whitebalance-module-enabled", true);

        // starts the gstreamer pipline
        //
        if (capture.start())
        {
            // note, it's best to turn off auto exposure and gain (also auto white ballance) when using trigger mode
            // set any appropriate post-start pipeline properties
            //       1, the default state of "Exposure Auto" is  true
            //       2, the default state of "Gain Auto" is  true
            //
            capture.setBooleanProperty("tcamautoexposure0", "Exposure Auto", true);
            capture.setIntegerProperty("tcamautoexposure0", "Brightness Reference", 80);
            capture.setBooleanProperty("tcamautoexposure0", "Gain Auto", true);
//          capture.setIntegerProperty("tcamsrc0", "Exposure", 800);
//          capture.setDoubleProperty("tcamsrc0", "Gain", 2.97);

            auto frame = cv::Mat(960, 1280, CV_8UC1, cv::Scalar(0, 0, 0));
            cv::namedWindow("Live Frame", 1);
            if (USE_CAMERA_TRIGGER)
            {
                // enable the trigger after setting manual exposure and gain values
                // FIXME! needs testing with automatic values, not sure when they get applied, hopefully not after a trigger...
                //
                capture.setStringProperty("tcamsrc0", "Trigger Mode", "On");
                std::cout << "Trigger Mode: On\n";

                // note, it takes quiter a bit of time to open and display the first highgui window
                //       this is only apparent when in trigger mode
                //
                cv::imshow("Live Frame", frame);
                cv::waitKey(350);
            }

            uint64_t previousTimestamp = 0;
            while (true)
            {
                auto success = capture.grab(frame);
                if (!success) std::cout << "The frame grab() failed, using the last valid frame\n";

                cv::cvtColor(frame, frame, cv::COLOR_BayerGBRG2BGR);
//              cv::cvtColor(frame, frame, cv::COLOR_BayerGBRG2GRAY);
                cv::imshow("Live Frame", frame);

                const uint64_t timestamp = capture.getCameraTimestamp();
                std::cout << "Timestamp: " << ((timestamp - previousTimestamp) / 1000000.0) << "\n";
                previousTimestamp = timestamp;

                const int32_t ch = cv::waitKey(1) & 0xff;
                if (ch == 27) break;
            }

            if (USE_CAMERA_TRIGGER) capture.setStringProperty("tcamsrc0", "Trigger Mode", "Off");

            // stop the pipeline and free up its memory
            //
            capture.stop();
            cv::destroyAllWindows();
        }
    }
    catch (const std::string& exception)
    {
        std::cout << "Exception: " << exception << "\n";
    }

    return 0;
}
