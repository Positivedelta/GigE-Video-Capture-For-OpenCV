//
// (c) Bit Parallel Ltd (Max van Daalen), March 2022
//

#include <cstdint>
#include <iostream>
#include <vector>

#include <opencv2/opencv.hpp>

#include "gige-video-capture.hpp"

int32_t main(int32_t argc, char *argv[])
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
        //
        const auto pipeline = "tcamsrc serial=30610380 ! video/x-bayer,format=gbrg,width=1280,height=960,framerate=30/1 ! tcamautoexposure ! tcamwhitebalance ! appsink";
//      const auto pipeline = "tcamsrc serial=30610380 ! video/x-raw,format=GRAY8,width=1280,height=960,framerate=15/1 ! tcamautoexposure ! appsink";
        auto capture = GigEVideoCapture(pipeline, CV_8U, 1);

        // displaying for reference only, useful when setting pipeline properties
        //
        std::cout << "Pipeline Component Names:\n";
        for (std::string_view name : capture.getPipelineComponentNames()) std::cout << "  " << name << "\n";

        // set any appropriate pre-start pipeline properties
        // notes 1, some properties can be set before the pipeline has been started, others must be set afterwards
        //       2, the default state of "whitebalance-module-enabled" is true
        //       3, white balance only works with colour images
        //
        capture.setBooleanProperty("tcamwhitebalance0", "whitebalance-module-enabled", true);

        // start the pipline
        //
        if (capture.start())
        {
            // set any appropriate post-start pipeline properties
            //       1, the default state of "Exposure Auto" is  true
            //       2, the default state of "Gain Auto" is  true
            //
            capture.setBooleanProperty("tcamautoexposure0", "Exposure Auto", true);
//          capture.setIntegerProperty("tcamautoexposure0", "Exposure", 100);
            capture.setIntegerProperty("tcamautoexposure0", "Brightness Reference", 50);
            capture.setBooleanProperty("tcamautoexposure0", "Gain Auto", true);
//          capture.setDoubleProperty("tcamautoexposure0", "Gain", 2.97);

            cv::namedWindow("Live Frame", 1);
            while (true)
            {
                cv::Mat frame = capture.grab();
                cv::cvtColor(frame, frame, cv::COLOR_BayerGBRG2BGR);
//              cv::cvtColor(frame, frame, cv::COLOR_BayerGBRG2GRAY);
//              cv::Canny(frame, frame, 80, 255);
                cv:imshow("Live Frame", frame);

                std::cout << "Timestamp: " << capture.getCameraTimestamp() << "\n";
                std::cout << "Framerate: " << capture.getCameraFrameRate() << "\n";

                const int32_t ch = cv::waitKey(1) & 0xff;
                if (ch == 27) break;
            }

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
