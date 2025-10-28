#pragma once
#include <JuceHeader.h>

//=============================================================================
/*  Handles writing video files from MoPanning output.

    It works by launching an FFmpeg process and sending raw RGB frames
    to it via a named pipe. Frames are enqued from the GL thread into
    a FIFO buffer, and a worker thread dequeues them and writes them
    to the pipe for FFmpeg to encode.
*/

class VideoWriter 
{
public:
    //=========================================================================
    VideoWriter() = default;
    ~VideoWriter();

    //=========================================================================
    /*  Starts the video writing process. 
    
        This function initializes the FIFO buffer, sets up the named
        pipe, launches FFmpeg, and starts the worker thread. It must be 
        called before attempting to enque frames from another thread.
    */
    void start();

    /*  Stops video writing and cleans up resources.
    
        This function signals the worker thread to exit, waits for it
        to finish, closes the named pipe, and waits for FFmpeg to
        complete encoding. It should be called when video writing is
        no longer needed.
    */
    void stop();

    //=========================================================================
    /*  Enques an RGB frame for writing.
    
        This function should be called from the GL thread whenever a new
        frame is ready to be written. It copies the provided RGB data
        into the FIFO buffer for the worker thread to process.
    */
    void enqueFrame(const uint8_t* rgb, int numBytes);

    //=========================================================================
    /*  Runs an FFmpeg version check.
    
        This function launches an FFmpeg process with the "-version"
        argument to retrieve and log the installed FFmpeg version.
    */
    void getFFmpegVersion();

    /*  Runs a pipe test to verify pipe functionality.
    
        This function creates a temporary named pipe, writes test data
        to it. The result should be a funky little video file on the 
        desktop.
    */
    void runPipeTest();
    
    //=========================================================================
    /*  Forward declaration of the worker thread class. */
    class Worker;

private:
    //=========================================================================
    /*  Dequeues a frame from the FIFO and writes it to the pipe.
    
        This function is called by the worker thread to retrieve the
        next available frame from the FIFO buffer and write it to
        the named pipe for FFmpeg to encode.
    */
    bool dequeueFrame();

    /*  Writes a single RGB frame to the named pipe.
    
        This function handles the low-level writing of RGB data
        to the named pipe. It returns true if the write was successful.
    */
    bool writeFrame(const uint8_t* rgb, int numBytes);

    //=========================================================================
    /*  Launches the FFmpeg process with appropriate arguments.
    
        This function constructs the command-line arguments for
        FFmpeg to read raw RGB frames from the named pipe and
        encode them into an MP4 video file. It then starts the
        FFmpeg process.
    */
    void launchFFmpeg();

    /*  Locates the FFmpeg executable on the system.
    
        This function attempts to find the FFmpeg binary based
        on the operating system and expected installation paths.
        It returns a File object pointing to the executable.
    */
    juce::File locateFFmpeg();

    /*  Sets up the named pipe for communication with FFmpeg.
    
        This function creates a named pipe that FFmpeg will
        read raw RGB frame data from and returns true is successful. 
        The pipe is used for nter-process communication between this 
        application and the spawned FFmpeg process.
    */
    bool setupPipe();

    //=========================================================================
    static constexpr int W = 1280;
    static constexpr int H = 720;
    static constexpr int FPS = 60;
    
    juce::NamedPipe pipe;
    juce::ChildProcess ffProcess;
    std::unique_ptr<Worker> workerThread;

    static constexpr int numSlots = 8;
    static constexpr int frameBytes = W * H * 3;
    juce::AbstractFifo fifoManager { numSlots };
    std::vector<std::unique_ptr<uint8_t[]>> fifoStorage;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoWriter)
};

//=============================================================================
/*  Runs a loop to dequeue frames and write them to the pipe.
    
    This worker thread continuously checks for new frames in the FIFO 
    buffer, dequeues them, and writes them to the named pipe for FFmpeg 
    to encode.
*/
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