#pragma once
#include <JuceHeader.h>

//=============================================================================
class VideoWriter 
{
public:
    //=========================================================================
    VideoWriter() = default;
    ~VideoWriter() = default;

    //==========================================================================
    void runPipeTest();
    void start();
    void stop();

    //==========================================================================
    void getFFmpegVersion();
    
    class Worker;

private:
    //=========================================================================
    void writeFrame(const uint8_t* rgb, int numBytes);
    void launchFFmpeg();    
    juce::File locateFFmpeg();
    void setupPipe();

    //=========================================================================
    juce::NamedPipe pipe;
    juce::ChildProcess ffProcess;
    std::unique_ptr<Worker> workerThread;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoWriter)
};

//=============================================================================
class VideoWriter::Worker : public juce::Thread
{
public:
    //=========================================================================
    Worker(VideoWriter& vw) : juce::Thread("VideoWorker"), parent(vw) {}

    void run() override;

private:
    //=========================================================================
    VideoWriter& parent;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Worker)
};