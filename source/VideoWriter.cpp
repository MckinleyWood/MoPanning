#include "VideoWriter.h"

//=============================================================================
void VideoWriter::prepare(double newSampleRate, int newSamplesPerBlock, int newNumChannels)
{

    sampleRate = (int)newSampleRate;
    samplesPerBlock = newSamplesPerBlock;
    numChannels = newNumChannels;
}

//=============================================================================
void VideoWriter::start()
{
    // Initialize FIFO storage
    videoFIFOStorage.reserve(numVideoSlots);
    for (int i = 0; i < numVideoSlots; ++i)
        videoFIFOStorage.push_back(std::make_unique<uint8_t[]>(frameBytes));

    audioFIFOStorage.reserve(numAudioSlots);
    for (int i = 0; i < numAudioSlots; ++i)
        audioFIFOStorage.push_back(std::make_unique<float[]>(numChannels * samplesPerBlock));

    blockBytes = samplesPerBlock * numChannels * sizeof(float);
    
    // Set up the pipes
    setupPipes();

    // Launch an FFmpeg process to read from the pipes
    launchFFmpeg();
    juce::Thread::sleep(100);

    // Start the worker threads
    videoWorkerThread = std::make_unique<Worker>(*this, Worker::Format::video);
    videoWorkerThread->startThread();

    audioWorkerThread = std::make_unique<Worker>(*this, Worker::Format::audio);
    audioWorkerThread->startThread();

    recording = true;
}

void VideoWriter::stop()
{
    // This will also signal FFmpeg that we are done giving it frames
    if (videoPipe.isOpen())
        videoPipe.close();
    
    if (audioPipe.isOpen())
        audioPipe.close();
    
    // Stop the worker threads
    if (videoWorkerThread != nullptr)
        videoWorkerThread->stopThread(-1);

    if (audioWorkerThread != nullptr)
        audioWorkerThread->stopThread(-1);

    // Wait for FFmpeg to finish writing the file
    if (ffProcess.isRunning())
        ffProcess.waitForProcessToFinish(5000);

    DBG("FFmpeg log:\n" << ffProcess.readAllProcessOutput());

    if (recording == true)
        saveVideo();

    recording = false;
}

//=============================================================================
void VideoWriter::enqueueVideoFrame(const uint8_t* rgb, int numBytes)
{
    // Get the next write position and advance the write pointer
    const auto scope = videoFIFOManager.write(1);

    // Copy the frame data into the FIFO storage
    if (scope.blockSize1 > 0)
        std::memcpy(videoFIFOStorage[scope.startIndex1].get(), rgb, (size_t)numBytes);

    videoWorkerThread->notify();
}

void VideoWriter::enqueueAudioBlock(const float* const* newBlock)
{
    // Get the next write position and advance the write pointer
    const auto scope = audioFIFOManager.write(1);

    if (scope.blockSize1 <= 0)
        return;

    float* dst = audioFIFOStorage[scope.startIndex1].get();

    // Interleave and copy data into the FIFO storage
    for (int i = 0; i < samplesPerBlock; ++i)
        for (int ch = 0; ch < numChannels; ++ch)
            *dst++ = newBlock[ch][i];

    audioWorkerThread->notify();
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

void VideoWriter::runPipeTest()
{
    setupPipes();

    launchFFmpeg();

    // Wait a moment so FFmpeg opens pipe
    juce::Thread::sleep(300);

    std::vector<uint8_t> frame(W * H * 3);
    for (int f = 0; f < 60; ++f)
    {
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                int i = (y * W + x) * 3;
                frame[i + 0] = (x + f) % 256;
                frame[i + 1] = (y) % 256;
                frame[i + 2] = 64;
            }
        }
        writeVideoFrame(frame.data(), (int)frame.size());
        juce::Thread::sleep(1000 / FPS);
    }

    stop();
    DBG("Done — video saved to desktop.");
}

//=============================================================================
bool VideoWriter::dequeueVideoFrame()
{
    // Get the next read position and advance the read pointer
    const auto scope = videoFIFOManager.read(1);

    // Copy the frame data from FIFO storage into the named pipe
    if (scope.blockSize1 > 0)
    {
        return writeVideoFrame(videoFIFOStorage[scope.startIndex1].get(), frameBytes);
    }

    else
    {
        // No frame to dequeue
        return false;
    }
}

bool VideoWriter::dequeueAudioBlock()
{
    // Get the next read position and advance the read pointer
    const auto scope = audioFIFOManager.read(1);

    // Copy the frame data into the FIFO storage
    if (scope.blockSize1 > 0)
    {
        return writeAudioBlock(audioFIFOStorage[scope.startIndex1].get(), blockBytes);
    }

    else
    {
        // No frame to dequeue
        return false;
    }
}

bool VideoWriter::writeVideoFrame(const uint8_t* rgb, int numBytes)
{
    if (videoPipe.isOpen() == false)
    {
        DBG("Pipe not open :(");
        return false;
    }

    int bytesWritten = videoPipe.write(rgb, numBytes, 50000);
    return bytesWritten == numBytes;
}

bool VideoWriter::writeAudioBlock(const float* block, int numBytes)
{
    if (audioPipe.isOpen() == false)
    {
        DBG("Pipe not open :(");
        return false;
    }

    int bytesWritten = audioPipe.write(block, numBytes, 50000);
    if (bytesWritten == -1)
    DBG("Audio write failed :(");
    return bytesWritten == numBytes;
}

//=============================================================================
void VideoWriter::launchFFmpeg()
{
   #if JUCE_WINDOWS
    juce::String videoInputPath = videoPipe.getName();
    juce::String audioInputPath = audioPipe.getName();
   #else
    juce::String videoInputPath = videoPipe.getName() + "_out";
    juce::String audioInputPath = audioPipe.getName() + "_out";
   #endif
   
    juce::String outputPath = tempVideo.getFullPathName();

    juce::File ffExecutable = locateFFmpeg();

    if (ffExecutable.existsAsFile() == false)
    {
        DBG("FFmpeg not found at: " + ffExecutable.getFullPathName());
        return;
    }

    juce::StringArray args;
    args.add(ffExecutable.getFullPathName());
    args.add("-y");
    args.add("-hide_banner");

    // Input: raw RGB frames via pipe
    args.add("-f");             args.add("rawvideo");
    args.add("-pixel_format");  args.add("rgb24");
    args.add("-video_size");    args.add("1280x720");  // width x height
    args.add("-framerate");     args.add("60");
    args.add("-i");             args.add(videoInputPath); 

    // Input: raw audio via another pipe
    args.add("-f");             args.add("f32le");      // Raw PCM input
    args.add("-ar");            args.add(juce::String(sampleRate)); 
    args.add("-ac");            args.add(juce::String(numChannels));
    args.add("-i");             args.add(audioInputPath);

    // Output: H.264 MP4
    args.add("-c:v");           args.add("libx264");
    args.add("-preset");        args.add("veryfast"); // tune as you like
    args.add("-crf");           args.add("18");       // 0–51 (lower = better)
    args.add("-pix_fmt");       args.add("yuv420p");  // for wide play

    args.add("-c:a");           args.add("aac");
    args.add("-b:a");           args.add("320k");

    args.add("-loglevel");      args.add("info");

    args.add(outputPath);

    const int flags = juce::ChildProcess::wantStdErr;       // capture ffmpeg logs

    if (ffProcess.start(args, flags) == false)
    {
        DBG("Failed to start FFmpeg process.");
        return;
    }
}

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

bool VideoWriter::setupPipes()
{
   #if JUCE_MAC
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto videoPipeName = tmp.getChildFile("mopanning_video_pipe").getFullPathName();
    auto audioPipeName = tmp.getChildFile("mopanning_audio_pipe").getFullPathName();
   #elif JUCE_WINDOWS
    auto videoPipeName = R"(\\.\pipe\mopanning_video_pipe)";
    auto audioPipeName = R"(\\.\pipe\mopanning_audio_pipe)";
   #endif

   return videoPipe.createNewPipe(videoPipeName) && audioPipe.createNewPipe(audioPipeName);
}

void VideoWriter::saveVideo()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Video As...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile("mopanning_output.mp4"),
        "*.mp4");

    // auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    // // Launch an asynchronous dialog window
    // chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
    // {
    //     juce::ignoreUnused(chooser);
    //     auto choice = fc.getResult();

    //     // Move the temp video file to the user-selected location
    //     if (tempVideo.existsAsFile())
    //     {
    //         bool result = tempVideo.moveFileTo(choice);
    //         if (result == true)
    //             DBG("Video saved to: " + choice.getFullPathName());
    //         else
    //             DBG("Failed to save video :(");
    //     }

    //     recording = false;
    // });

    if (chooser->browseForFileToSave(true) == true)
    {
        auto choice = chooser->getResult();

        // Move the temp video file to the user-selected location
        if (tempVideo.existsAsFile())
        {
            bool result = tempVideo.moveFileTo(choice);
            if (result == true)
                DBG("Video saved to: " + choice.getFullPathName());
            else
                DBG("Failed to save video :(");
        }
    }
}

//=============================================================================
VideoWriter::Worker::Worker(VideoWriter& vw, Format f)
    : juce::Thread("VideoWorker"), parent(vw), format(f) {}

void VideoWriter::Worker::run()
{
    if (format == video)
    {
        while (threadShouldExit() == false) 
        {
            bool frameWritten = parent.dequeueVideoFrame();

            if (frameWritten == false)
            {
                // No frame to write, sleep briefly
                juce::Thread::wait(1);
            }
        }
    }
    
    else if (format == audio)
    {
        while (threadShouldExit() == false) 
        {
            bool blockWritten = parent.dequeueAudioBlock();

            if (blockWritten == false)
            {
                // No block to write, sleep briefly
                juce::Thread::wait(1);
            }
        }
    }

    else
    {
        jassertfalse;
    }
    
}