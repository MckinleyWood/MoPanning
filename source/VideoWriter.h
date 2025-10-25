#pragma once
#include <JuceHeader.h>


class VideoWriter 
{
public:
    VideoWriter() = default;
    ~VideoWriter() = default;

    juce::File locateFFmpeg();
    void getFFmpegVersion();
    void videoSmokeTest();
};