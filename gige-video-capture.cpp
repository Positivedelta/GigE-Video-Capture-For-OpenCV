//
// (c) Bit Parallel Ltd (Max van Daalen), March 2022
//

#include <sstream>

#include <gstmetatcamstatistics.h>
#include <gst/video/video.h>
#include <tcamprop.h>

#include "gige-video-capture.hpp"

// FIXME! make this a singleton as it currently relies on a static frame handler, perhaps there is another way... investigate...
//
GigEVideoCapture::GigEVideoCapture(const std::string_view pipeline, const int32_t imageBaseType, const int32_t imageChannels)
{
    type = CV_MAKETYPE(imageBaseType, imageChannels);
    channels = imageChannels;

    // expecting a pipeline similar to "tcamsrc ! video/x-bayer,format=gbrg,width=1280,height=960,framerate=30/1 ! tcamautoexposure ! tcamwhitebalance ! appsink"
    //
    GError* err = nullptr;
    gstPipeline = gst_parse_launch(pipeline.data(), &err);
    if (gstPipeline == nullptr) throw std::string("Could not create pipeline, reason: ") + std::string(err->message);

    // map the pipeline element names to their corresponding objects
    // used when setting element properties
    // note, the code below avoids having to specify a name attribute on each of the pipeline elements in order to access them
    //       i.e. source = gst_bin_get_by_name(GST_BIN(gstPipeline), "source");
    //
    GValue pipelineItem = G_VALUE_INIT;
    GstIterator* pipelineIterator = gst_bin_iterate_elements(GST_BIN(gstPipeline));
    bool done = false;
    while (!done)
    {
        switch (gst_iterator_next(pipelineIterator, &pipelineItem))
        {
            case GST_ITERATOR_OK:
            {
                GstElement* element = GST_ELEMENT(g_value_peek_pointer(&pipelineItem));
                const auto name = std::string(gst_element_get_name(element));
                pipelineMap[name] = element;
                g_value_reset(&pipelineItem);
                break;
            }

            // note, GST_ITERATOR_RESYNC is not required here, so will be treated as an error
            //
            case GST_ITERATOR_RESYNC:
            case GST_ITERATOR_ERROR:
                g_value_unset(&pipelineItem);
                gst_iterator_free(pipelineIterator);
                throw std::string("Unable to iterate pipeline elements");

            case GST_ITERATOR_DONE:
                done = true;
                break;
        }
    }

    gst_iterator_free(pipelineIterator);

    // finally, configure the appsink pipeline element
    // notes 1, to notify the pipeline each time it receives a new image
    //       2, register the handler to be invoked by the above notification
    //       3, not as tidy, but could add a name attribute to the 'appsink' element and access the element
    //          i.e. GstElement* sink = gst_bin_get_by_name(GST_BIN(gstPipeline), "sink");
    //
    try
    {
        // FIXME! this is slightly fragile... but OK for the moment
        //        notes 1, check for a appsink name parameter, if found use it instead of the hard coded value
        //              2, could use a sink iterator but then you'd have to assume that there would only ever be one of them
        //
        GstElement* sink = pipelineMap.at("appsink0");
        g_object_set(G_OBJECT(sink), "emit-signals", TRUE, nullptr);
        g_signal_connect(sink, "new-sample", G_CALLBACK(GigEVideoCapture::handler), nullptr);
    }
    catch (const std::out_of_range& exception)
    {
        throw std::string("Unable to locate the required appsink pipeline element");
    }
}

GstFlowReturn GigEVideoCapture::handler(GstElement* sink, gpointer userData)
{
    if (!doGrab) return GST_FLOW_OK;

    GstSample* sample = nullptr;
    g_signal_emit_by_name(sink, "pull-sample", &sample, nullptr);
    if (sample)
    {
        GstBuffer* buffer = gst_sample_get_buffer(sample);

        GstMapInfo info;
        if (gst_buffer_map(buffer, &info, GST_MAP_READ))
        {
            GstVideoInfo* videoInfo = gst_video_info_new();
            if (!gst_video_info_from_caps(videoInfo, gst_sample_get_caps(sample)))
            {
                // unable to parse video info, this should not happen...
                //
                g_warning("Failed to parse video info");
                return GST_FLOW_ERROR;
            }

            // note, the pipeline is likely to be configured to generate a bayer GBRG 1 channel image
            //
            grabbedFrame.create(videoInfo->height, videoInfo->width, type);
            memcpy(grabbedFrame.data, info.data, videoInfo->width * videoInfo->height * channels);

            // grab the required frame meta data
            //
            GstMeta* gstMeta = gst_buffer_get_meta(buffer, g_type_from_name("TcamStatisticsMetaApi"));
            if (gstMeta)
            {
                GstStructure* metaData = ((TcamStatisticsMeta*)gstMeta)->structure;
                gst_structure_get_uint64(metaData, "camera_time_ns", &cameraTimestamp);
                gst_structure_get_double(metaData, "framerate", &cameraFrameRate);
            }

            // minimise the required lock scope
            //
            {
                std::scoped_lock<std::mutex> lock(lockMutex);
                doGrab = false;
                condition.notify_one();
            }

            // tidy up...
            //
            gst_buffer_unmap(buffer, &info);
            gst_video_info_free(videoInfo);
        }

        gst_sample_unref(sample);
    }

    return GST_FLOW_OK;
}

cv::Mat GigEVideoCapture::grab()
{
    doGrab = true;
    std::unique_lock<std::mutex> lock(lockMutex);
    while (doGrab) condition.wait(lock);

    return grabbedFrame;
}

uint64_t GigEVideoCapture::getCameraTimestamp() const
{
    return cameraTimestamp;
}

double GigEVideoCapture::getCameraFrameRate() const
{
    return cameraFrameRate;
}

bool GigEVideoCapture::start()
{
    if (gst_element_set_state(gstPipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        g_error("GigEVideoCapture::start() failed to set GST_STATE_PLAYING");
        return false;
    }

    return waitForStateChange(gstPipeline);
}

bool GigEVideoCapture::stop()
{
    if (gst_element_set_state(gstPipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
    {
        g_error("GigEVideoCapture::stop() failed to set GST_STATE_NULL");
        return false;
    }

    return waitForStateChange(gstPipeline);
}

bool GigEVideoCapture::setBooleanProperty(const std::string& component, const std::string& name, const bool value)
{
    bool success = false;
    try
    {
        GstElement* element = pipelineMap.at(component);

        GValue setValue = G_VALUE_INIT;
        g_value_init(&setValue, G_TYPE_BOOLEAN);
        g_value_set_boolean(&setValue, value);
        success = tcam_prop_set_tcam_property(TCAM_PROP(element), name.c_str(), &setValue);
        if (!success)
        {
            auto message = "Error setting boolean property: " + name + " = " + std::to_string(value) + ", for component: " + component;
            g_error(message.c_str());
        }

        g_value_unset(&setValue);
    }
    catch (const std::out_of_range& exception)
    {
        auto message = "Pipeline component key \"" + component + "\" does not exist";
        g_error(message.c_str());
    }

    return success;
}

bool GigEVideoCapture::setIntegerProperty(const std::string& component, const std::string& name, const int32_t value)
{
    bool success = false;
    try
    {
        GstElement* element = pipelineMap.at(component);

        GValue setValue = G_VALUE_INIT;
        g_value_init(&setValue, G_TYPE_INT);
        g_value_set_int(&setValue, value);
        success = tcam_prop_set_tcam_property(TCAM_PROP(element), name.c_str(), &setValue);
        if (!success)
        {
            auto message = "Error setting integer property: " + name + " = " + std::to_string(value) + ", for component: " + component;
            g_error(message.c_str());
        }

        g_value_unset(&setValue);
    }
    catch (const std::out_of_range& exception)
    {
        auto message = "Pipeline component key \"" + component + "\" does not exist";
        g_error(message.c_str());
    }

    return success;
}

bool GigEVideoCapture::setDoubleProperty(const std::string& component, const std::string& name, const double value)
{
    bool success = false;
    try
    {
        GstElement* element = pipelineMap.at(component);

        GValue setValue = G_VALUE_INIT;
        g_value_init(&setValue, G_TYPE_DOUBLE);
        g_value_set_double(&setValue, value);
        success = tcam_prop_set_tcam_property(TCAM_PROP(element), name.c_str(), &setValue);
        if (!success)
        {
            auto message = "Error setting double property: " + name + " = " + std::to_string(value) + ", for component: " + component;
            g_error(message.c_str());
        }

        g_value_unset(&setValue);
    }
    catch (const std::out_of_range& exception)
    {
        auto message = "Pipeline component key \"" + component + "\" does not exist";
        g_error(message.c_str());
    }

    return success;
}

std::vector<std::string> GigEVideoCapture::getPipelineComponentNames() const
{
    auto names = std::vector<std::string>();
    for(auto& component : pipelineMap) names.emplace_back(component.first);

    return names;
}

bool GigEVideoCapture::waitForStateChange(GstElement* gstPipeline)
{
    bool success = false;
    while (!success)
    {
        // wait 0.1 seconds for the pipeline to change state
        //
        GstState state;
        GstState pending;
        const GstStateChangeReturn status = gst_element_get_state(gstPipeline ,&state, &pending, 100000000);

        if (status == GST_STATE_CHANGE_SUCCESS)
        {
            success = true;
        }
        else if (status == GST_STATE_CHANGE_FAILURE)
        {

            std::stringstream ss;
            ss << "The pipeline failed to change state, details: ";
            ss << gst_element_state_change_return_get_name(status) << ", ";
            ss << gst_element_state_get_name(state) << ", ";
            ss << gst_element_state_get_name(pending);
            g_error(ss.str().c_str());
        }
    }

    return success;
}

GigEVideoCapture::~GigEVideoCapture()
{
    // note, this will also free all of the allocated pipeline elements
    //
    gst_object_unref(gstPipeline);
}
