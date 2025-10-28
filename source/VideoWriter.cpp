#include "VideoWriter.h"

//=============================================================================
void VideoWriter::start()
{
    // Initialize FIFO storage
    fifoStorage.resize(numSlots);
    for (int i = 0; i < numSlots; ++i)
        fifoStorage[i].reset(new uint8_t[frameBytes]);
    
    // Set up the pipe
    setupPipe();

    // Launch an FFmpeg process to read from the pipe
    launchFFmpeg();

    // Start the worker thread
    workerThread = std::make_unique<Worker>(*this);
    workerThread->startThread();

    recording = true;
}

void VideoWriter::stop()
{
    if (workerThread != nullptr)
        workerThread->stopThread(2000);

    // This will also signal FFmpeg that we are done giving it frames
    if (pipe.isOpen())
        pipe.close();
    
    // Wait for FFmpeg to finish writing the file
    if (ffProcess.isRunning())
        ffProcess.waitForProcessToFinish(5000);

    if (recording == true)
        saveVideo();

    recording = false;
}

//=============================================================================
void VideoWriter::enqueFrame(const uint8_t* rgb, int numBytes)
{
    // Get the next write position and advance the write pointer
    const auto scope = fifoManager.write(1);

    // Copy the frame data into the FIFO storage
    if (scope.blockSize1 > 0)
        std::memcpy(fifoStorage[scope.startIndex1].get(), rgb, (size_t)numBytes);
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
    setupPipe();

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
        writeFrame(frame.data(), (int)frame.size());
        juce::Thread::sleep(1000 / FPS);
    }

    stop();
    DBG("Done — video saved to desktop.");
}

//=============================================================================
bool VideoWriter::dequeueFrame()
{
    // Get the next read position and advance the read pointer
    const auto scope = fifoManager.read(1);

    // Copy the frame data into the FIFO storage
    if (scope.blockSize1 > 0)
    {
        return writeFrame(fifoStorage[scope.startIndex1].get(), frameBytes);
    }

    else
    {
        // No frame to dequeue
        return false;
    }
}

bool VideoWriter::writeFrame(const uint8_t* rgb, int numBytes)
{
    if (pipe.isOpen() == false)
    {
        DBG("Pipe not open :(");
        return false;
    }

    int bytesWritten = pipe.write(rgb, numBytes, 5000);
    return bytesWritten == numBytes;
}

//=============================================================================
void VideoWriter::launchFFmpeg()
{
   #if JUCE_WINDOWS
    juce::String inputPath = pipe.getName();
   #else
    juce::String inputPath = pipe.getName() + "_out";
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
    args.add("-f");            args.add("rawvideo");
    args.add("-pixel_format"); args.add("rgb24");
    args.add("-video_size");   args.add("1280x720");  // width x height
    args.add("-framerate");    args.add("60");
    args.add("-i");            args.add(inputPath); 

    // Output: H.264 MP4
    args.add("-c:v");          args.add("libx264");
    args.add("-preset");       args.add("veryfast"); // tune as you like
    args.add("-crf");          args.add("18");       // 0–51 (lower = better)
    args.add("-pix_fmt");      args.add("yuv420p");  // for wide player compatibility
    args.add(outputPath);

    const int flags = juce::ChildProcess::wantStdErr;       // capture ffmpeg logs

    if (!ffProcess.start(args, flags))
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

bool VideoWriter::setupPipe()
{
   #if JUCE_MAC
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto pipeName = tmp.getChildFile("mopanning_ffmpeg_pipe").getFullPathName();
   #elif JUCE_WINDOWS
    auto pipeName = R"(\\.\pipe\mopanning_ffmpeg)";
   #endif

   return pipe.createNewPipe(pipeName);
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
void VideoWriter::Worker::run()
{
    while (threadShouldExit() == false) 
    {
        bool frameWritten = parent.dequeueFrame();

        if (!frameWritten)
        {
            // No frame to write, sleep briefly
            juce::Thread::sleep(1);
        }
    }
}