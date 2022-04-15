//
// (c) Bit Parallel Ltd (Max van Daalen), March 2022
//

#ifndef H_GIGE_VIDEO_CAPTURE
#define H_GIGE_VIDEO_CAPTURE

#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <gst/gst.h>
#include <opencv2/opencv.hpp>

class GigEVideoCapture
{
    private:
        GstElement* gstPipeline;
        std::unordered_map<std::string, GstElement*> pipelineMap;

        int32_t type, channels;
        cv::Mat grabbedFrame = cv::Mat();
        uint64_t cameraTimestamp = 0;
        double cameraFrameRate = 0.0;
        bool doGrab = false;
        bool doGrabSuccess = false;
        std::mutex lockMutex;
        std::condition_variable condition;

    public:
        GigEVideoCapture(const std::string_view pipeline, const int32_t imageBaseType, const int32_t imageChannels);

        bool start();
        bool grab(cv::Mat& frame);
        uint64_t getCameraTimestamp() const;
        double getCameraFrameRate() const;
        bool stop();

        bool setBooleanProperty(const std::string& component, const std::string& name, const bool value);
        bool setIntegerProperty(const std::string& component, const std::string& name, const int32_t value);
        bool setDoubleProperty(const std::string& component, const std::string& name, const double value);
        bool setStringProperty(const std::string& component, const std::string& name, const std::string& value);

        std::vector<std::string> getPipelineComponentNames() const;

        ~GigEVideoCapture();

    private:
        bool waitForStateChange(GstElement* gstPipeline);
        static GstFlowReturn handler(GstElement* sink, gpointer userData);
};

#endif
