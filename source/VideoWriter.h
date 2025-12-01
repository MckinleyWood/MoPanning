/*=============================================================================

    This file is part of the MoPanning audio visuaization tool.
    Copyright (C) 2025 Owen Ohlson and Mckinley Wood

    This program is free software: you can redistribute it and/or modify 
    it under the terms of the GNU Affero General Public License as 
    published by the Free Software Foundation, either version 3 of the 
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but 
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
    Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public 
    License along with this program. If not, see 
    <https://www.gnu.org/licenses/>.

=============================================================================*/

#pragma once
#include <JuceHeader.h>
#include "Utils.h"
#include "FrameQueue.h"


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
    ~VideoWriter() = default;

    //=========================================================================
    /*  Sets the audio parameters the VideoWriter needs.

        This function must be called before starting to write video.
    */
    void prepare(double sampleRateIn, int samplesPerBlockIn, int numChannelsIn);

    //=========================================================================
    /*  Starts the video writing process. 
    
        This function initializes the FIFO storage, output streams,
        and worker threads. It should be called when the user would like
        to begin recording video.
    */
    void start();

    /*  Stops video writing and cleans up resources.
    
        This function stops the worker threads, finalizes the video
        file, and prompts the user to save the completed video.
    */
    void stop();

    //=========================================================================
    /*  Sets the pointer to the shared queue for video writing output.
    */
    void setFrameQueuePointer(FrameQueue* frameQueuePtr);

    //=========================================================================
    /*  Enques an RGB frame for writing.
    
        This function should be called from the GL thread whenever a new
        frame is ready to be written. It copies the provided RGB data
        into the FIFO buffer for the worker thread to process.
    */
    void enqueueVideoFrame(const uint8_t* rgb, int numBytes);

    /*  Enques an audio block for writing.
    
        This function should be called from the audio thread whenever a 
        new block is ready to be written. It copies the provided data
        to the wav writer to write to disk on a background thread.
    */
    void enqueueAudioBlock(const float* const* newBlock, int numSamples);

    //=========================================================================
    /*  Returns true if recording or writing is in progress. 
    */
    bool isRecording();

    //=========================================================================
    /*  Runs an FFmpeg version check.
    
        This function launches an FFmpeg process with the "-version"
        argument to retrieve and log the installed FFmpeg version.
    */
    void getFFmpegVersion();
    
    //=========================================================================
    /*  Forward declaration of the worker thread class. */
    class Worker;
    class RenderingWindow;

private:
    //=========================================================================
    /*  Dequeues a frame from the FIFO and writes it to a temp file.
    
        This function is called by the worker thread to retrieve the
        next available frame from the FIFO buffer and write it to disk
        for FFmpeg to process later.
    */
    bool dequeueVideoFrame();

    //=========================================================================
    /*  Locates the FFmpeg executable on the system.
    
        This function attempts to find the FFmpeg binary based
        on the operating system and expected installation paths.
        It returns a File object pointing to the executable.
    */
    juce::File locateFFmpeg();

    /*  Initializes the WAV audio writer.
    
        This function sets up the WAV file writer to record
        audio data to a temporary .wav file.
    */
    void startWavWriter();

    //=========================================================================
    /*  Prompts the user to select a save location for the completed video. 
    */
    juce::File promptUserForSaveLocation();

    /*  Runs an FFmpeg process to finalize the video.
    
        This function constructs the command-line arguments for
        FFmpeg to mux raw RGB frames and .wav audio into a final .mp4
        video file, starts the process, and launches a dialog box 
        propting the user to wait for it to finish or cancel the 
        operation.
    */
    void runFFmpeg(juce::File destination);

    //=========================================================================

    // Audio parameters - set via prepare()
    int sampleRate;
    int samplesPerBlock;
    int numChannels;
    int blockBytes;

    bool recording = false;

    // Temporary file locations
    juce::File temp = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File rawFrames = temp.getChildFile("mopanning_frames.rgb");
    juce::File wavAudio = temp.getChildFile("mopanning_audio.wav");
    juce::File tempVideo = temp.getChildFile("mopanning_temp_video.mp4");
    
    // .wav audio writer
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> wavWriter;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> wavWriterPtr { nullptr };
    juce::AudioBuffer<float> audioTmp;
    std::unique_ptr<juce::TimeSliceThread> wavThread;

    // Video output stream, FIFO, and worker thread
    std::unique_ptr<juce::FileOutputStream> framesOut;
    std::atomic<int64_t> videoBytesWritten {0};
    std::atomic<int> frameCount {0};

    FrameQueue* frameQueue;

    std::unique_ptr<Worker> videoWorkerThread;

    juce::ChildProcess ffProcess;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoWriter)
};


//=============================================================================
/*  Runs a loop to dequeue video frames and write them to a .rgb file.
    
    This worker thread continuously checks for new frames in the FIFO 
    buffer, dequeues them, and writes them to disk.
*/
class VideoWriter::Worker : public juce::Thread
{
public:
    //=========================================================================
    Worker(VideoWriter& vw)  : juce::Thread("VideoWorker"), parent(vw) 
    {
    }

    void run() override;

private:
    //=========================================================================
    VideoWriter& parent;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Worker)
};


//=============================================================================
/*  Shows a window with a cancel button while processing video.
    
    This class creates a modal progress window that repeatedly checks
    the FFmpeg process status and allows the user to cancel the 
    operation.
*/
class VideoWriter::RenderingWindow : public juce::ThreadWithProgressWindow
{
public:
    //=========================================================================
    RenderingWindow(VideoWriter& vw) 
        : ThreadWithProgressWindow("Writing the video file...", false, true), parent(vw)
    {
    }

    void run() override;

private:
    //=========================================================================
    VideoWriter& parent;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RenderingWindow)
};


