#include "VideoWriter.h"

juce::File VideoWriter::locateFFmpeg()
{
   #if JUCE_MAC
    // .../MoPanning.app/Contents/MacOS
    auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto ff  = exe.getParentDirectory().getChildFile("ThirdParty").getChildFile("ffmpeg");
    return ff;
   #elif JUCE_WINDOWS
    // next to .exe: .../MoPanning.exe -> same dir
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return exeDir.getChildFile("ffmpeg.exe");
   #else
    // Fallback: try PATH
    return juce::File("ffmpeg");
   #endif
}

void VideoWriter::getFFmpegVersion()
{
    juce::ChildProcess process;
    auto ff = locateFFmpeg();

    if (!ff.existsAsFile())
    {
        DBG("FFmpeg not found at: " + ff.getFullPathName());
        return;
    }

    // Run "ffmpeg -version" to test
    juce::StringArray args;
    args.add(ff.getFullPathName());
    args.add("-version");

    if (!process.start(args))
    {
        DBG("Failed to launch ffmpeg process.");
        return;
    }

    DBG("FFmpeg output:" << process.readAllProcessOutput(););
}

void VideoWriter::videoSmokeTest()
{
    // Generate a 2s 1280x720 color test via lavfi; proves encoding works.
    juce::ChildProcess process;
    auto ff = locateFFmpeg();

    if (!ff.existsAsFile())
    {
        DBG("ffmpeg not found at: " + ff.getFullPathName());
        return;
    }

    juce::File outFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                            .getChildFile("mopanning_ffmpeg_smoke_test.mp4");

    juce::StringArray args;
    args.add(ff.getFullPathName());
    args.add("-y");
    args.add("-hide_banner");
    args.add("-f");         args.add("lavfi");
    args.add("-i");         args.add("color=c=red:s=1280x720:d=2");
    args.add("-c:v");       args.add("libx264");
    args.add("-pix_fmt");   args.add("yuv420p");
    args.add(outFile.getFullPathName());

    if (!process.start(args))
    {
        DBG("Failed to start ffmpeg for smoke test.");
        return;
    }

    DBG(process.readAllProcessOutput());
    process.waitForProcessToFinish(15000);
}