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

#include "VideoWriter.h"

//=============================================================================
void VideoWriter::prepare(double newSampleRate, int newSamplesPerBlock, int newNumChannels)
{
    sampleRate = (int)newSampleRate;
    samplesPerBlock = newSamplesPerBlock;
    numChannels = newNumChannels;
    blockBytes = samplesPerBlock * numChannels * sizeof(float);
}

//=============================================================================
void VideoWriter::start()
{
    // Set up the video output stream
    framesOut = std::make_unique<juce::FileOutputStream>(rawFrames);
    jassert(framesOut != nullptr && framesOut->openedOk());

    // Initialize the WAV writer for audio capture
    startWavWriter();

    // Start the video worker thread
    videoWorkerThread = std::make_unique<Worker>(*this);
    videoWorkerThread->startThread();

    recording = true;
}

void VideoWriter::stop()
{
    // Stop the video worker thread
    if (videoWorkerThread != nullptr)
        videoWorkerThread->stopThread(-1);

    if (framesOut != nullptr)
    {
        framesOut->flush();
        framesOut.reset();
    }

    // Unpublish the WAV writer so the audio thread stops using it
    auto* tw = wavWriterPtr.exchange(nullptr, std::memory_order_acq_rel);

    // Destroy the ThreadedWriter and finalize the .wav header
    wavWriter.reset();

    // Stop the audio worker thread
    if (wavThread != nullptr)
        wavThread->stopThread(-1);

    // Finalize and save the completed video in a user-specified location
    if (recording == true)
    {
        juce::File outputLocation = promptUserForSaveLocation();
        if (outputLocation.getFileName().isEmpty() == false)
        {
            runFFmpeg(outputLocation);
        }
    }

    // Clean up temporary files
    if (rawFrames.existsAsFile())
        rawFrames.deleteFile();
    
    if (wavAudio.existsAsFile())
        wavAudio.deleteFile();

    recording = false;
}

//=============================================================================
void VideoWriter::setFrameQueuePointer(FrameQueue* frameQueuePtr)
{
    frameQueue = frameQueuePtr;
}

//=============================================================================
void VideoWriter::enqueueAudioBlock(const float* const* newBlock, int numSamples)
{
    // Ensure the WAV writer is ready and we have valid data
    auto* tw = wavWriterPtr.load(std::memory_order_acquire);
    if (tw == nullptr || numSamples <= 0 || numChannels <= 0) return;

    jassert(audioTmp.getNumChannels() == numChannels || audioTmp.getNumSamples() >= numSamples);

    // Copy non-interleaved device data into our stable buffer
    for (int ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::copy(audioTmp.getWritePointer(ch),
                                          newBlock[ch], numSamples);

    // Queue it to the background writer
    tw->write(audioTmp.getArrayOfReadPointers(), numSamples);
}

//=============================================================================
bool VideoWriter::isRecording()
{
    return recording;
}

//=============================================================================
void VideoWriter::getFFmpegVersion()
{
    juce::ChildProcess process;
    juce::File ffExecutable = locateFFmpeg();

    if (ffExecutable.existsAsFile() == false)
    {
        DBG("FFmpeg not found at: " + ffExecutable.getFullPathName());
        return;
    }

    // Run "ffmpeg -version" to test
    juce::StringArray args;
    args.add(ffExecutable.getFullPathName());
    args.add("-version");

    if (!process.start(args))
    {
        DBG("Failed to launch ffmpeg process.");
        return;
    }

    DBG("FFmpeg output:" << process.readAllProcessOutput(););
}

//=============================================================================
bool VideoWriter::dequeueVideoFrame()
{
    // Get the buffer data from the FIFO
    const uint8_t* data = frameQueue->readNextBuffer();

    if (data == nullptr)
    {
        // No frame available
        return false; 
    }

    // Write the RGB24 frame
    framesOut->write(data, (size_t)Constants::frameBytes);
    frameQueue->finishRead();

    if (framesOut->getStatus().failed())
    {
        DBG("framesOut write failed");
        return false;
    }

    videoBytesWritten += Constants::frameBytes;
    ++frameCount;
    return true;
}

//=============================================================================
juce::File VideoWriter::locateFFmpeg()
{
   #if JUCE_MAC
    // .../MoPanning.app/Contents/MacOS
    auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto ffExecutable  = exe.getParentDirectory().getChildFile("ThirdParty").getChildFile("ffmpeg");
    return ffExecutable;
   #elif JUCE_WINDOWS
    // next to .exe: .../MoPanning.exe -> same dir
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return exeDir.getChildFile("ffmpeg.exe");
   #else
    // Fallback: try PATH
    return juce::File("ffmpeg");
   #endif
}

//=============================================================================
void VideoWriter::startWavWriter()
{
    const unsigned int bitsPerSample = 24;

    auto options = AudioFormatWriterOptions()
        .withSampleRate((double)sampleRate)
        .withNumChannels(numChannels)
        .withBitsPerSample((int)bitsPerSample);

    auto fileStream = wavAudio.createOutputStream();
    jassert(fileStream && fileStream->openedOk());

    std::unique_ptr<OutputStream> outStream;
    outStream.reset(fileStream.release());

    juce::WavAudioFormat wav;
    auto rawWriter = wav.createWriterFor(outStream, options);
    jassert(rawWriter != nullptr);

    if (wavThread == nullptr)
        wavThread = std::make_unique<juce::TimeSliceThread>("WavWriterThread");
    
    wavThread->startThread();

    const int fifoSamples = (int)juce::roundToInt(sampleRate * 0.2); // 200 ms
    wavWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        rawWriter.release(),
        *wavThread,
        fifoSamples);

    // Publish the raw pointer for the audio callback (fast nullptr check)
    wavWriterPtr.store(wavWriter.get(), std::memory_order_release);

    // Prepare temporary buffer for audio blocks
    audioTmp.setSize(numChannels, samplesPerBlock);
}

//=============================================================================
juce::File VideoWriter::promptUserForSaveLocation()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Video As...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile("mopanning_output.mp4"),
        "*.mp4");

    /*  Asynchronous version - this would be better, but would need more 
        refactoring to handle the async nature of the callback properly.
    
    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    // Launch an asynchronous dialog window
    chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
    {
        juce::ignoreUnused(chooser);
        auto choice = fc.getResult();

        recording = false;
    }); */

    // Modal dialog - blocks the message thread until the user makes a choice
    if (chooser->browseForFileToSave(true) == true)
    {
        return chooser->getResult();
    }

    return juce::File();
}

void VideoWriter::runFFmpeg(juce::File destination)
{
    juce::String outputPath = tempVideo.getFullPathName();

    juce::File ffExecutable = locateFFmpeg();
    if (! ffExecutable.existsAsFile())
    {
        DBG("FFmpeg not found at: " + ffExecutable.getFullPathName());
        return;
    }

    juce::StringArray args;
    args.add(ffExecutable.getFullPathName());
    args.add("-y");
    args.add("-hide_banner");

    // Input 0: raw RGB frames
    args.add("-f");             args.add("rawvideo");
    args.add("-pixel_format");  args.add("rgb24");
    args.add("-video_size");    args.add(juce::String(Constants::W) + "x" + juce::String(Constants::H));
    args.add("-framerate");     args.add(juce::String(Constants::FPS));
    args.add("-i");             args.add(rawFrames.getFullPathName());

    // Input 1: WAV audio
    args.add("-i");             args.add(wavAudio.getFullPathName());

   #if JUCE_MAC
    // Hardware encode on macOS (much faster)
    args.add("-c:v");           args.add("h264_videotoolbox");
    args.add("-pix_fmt");       args.add("yuv420p");
    args.add("-b:v");           args.add("8M");
    args.add("-maxrate");       args.add("10M");
    args.add("-bufsize");       args.add("20M");
   #else
    // CPU x264
    args.add("-c:v");           args.add("libx264");
    args.add("-preset");        args.add("veryfast");
    args.add("-crf");           args.add("18");
    args.add("-pix_fmt");       args.add("yuv420p");
   #endif

    args.add("-c:a");           args.add("aac");
    args.add("-b:a");           args.add("320k");

    args.add("-loglevel");      args.add("info");

    // Output file
    args.add(outputPath);

    const int flags = juce::ChildProcess::wantStdErr; // capture logs
    if (! ffProcess.start(args, flags))
    {
        DBG("Failed to start FFmpeg process.");
        return;
    }

    // Launch rendering window thread
    RenderingWindow renderingWindow(*this);
    if (renderingWindow.runThread())
    {
        // Process completed successfully
        DBG("FFmpeg rendering completed.");

        // Move the temp video file to the user-selected location
        if (tempVideo.existsAsFile() && tempVideo.moveFileTo(destination))
        {
            DBG("Video saved to: " + destination.getFullPathName());
            return;
        }

        DBG("Failed to save video :(");
    }
    else
    {
        // User cancelled the operation
        DBG("FFmpeg rendering cancelled by user.");

        ffProcess.kill();
        if (tempVideo.existsAsFile())
            tempVideo.deleteFile();
    }
}

//=============================================================================
void VideoWriter::Worker::run()
{
    while (threadShouldExit() == false) 
    {
        bool frameWritten = parent.dequeueVideoFrame();

        if (frameWritten == false)
        {
            // No frame to write, sleep briefly
            juce::Thread::sleep(1);
        }
    }
}

void VideoWriter::RenderingWindow::run()
{
    while (threadShouldExit() == false)
    {
        if (parent.ffProcess.isRunning() == false)
        {
            // getAlertWindow()->getButton("Cancel")->setVisible(false);
            // setStatusMessage("Complete!");
            // juce::Thread::sleep(500);
            break;
        }

        juce::Thread::sleep(1);
    }
}