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
    void start();
    void stop();

    void enqueFrame(const uint8_t* rgb, int numBytes);

    //==========================================================================
    void getFFmpegVersion();
    void runPipeTest();
    
    class Worker;

private:
    //=========================================================================
    bool dequeueFrame();
    bool writeFrame(const uint8_t* rgb, int numBytes);

    void launchFFmpeg();    
    juce::File locateFFmpeg();
    void setupPipe();

    //=========================================================================
    static constexpr int W = 1280;
    static constexpr int H = 720;
    static constexpr int FPS = 60;
    
    juce::NamedPipe pipe;
    juce::ChildProcess ffProcess;
    std::unique_ptr<Worker> workerThread;

    static constexpr int numSlots = 8;
    static constexpr int frameBytes = W * H * 3;
    juce::AbstractFifo frameFifo { numSlots }; // buffer for 8 frames
    std::vector<std::unique_ptr<uint8_t[]>> fifoStorage;

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