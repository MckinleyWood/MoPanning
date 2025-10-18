# MoPanning
MoPanning is a perception-based real-time visualization program for stereo audio including music. Below are instructions on how to install and use it. After you have tried it out, we would love your feedback! We have a Google form at https://forms.gle/uvojXPtnCtWrd9Zg8, or you can reach us at mopanning@outlook.com.

## Installation
We have installers for the latest version of MoPanning for macOS and Windows on our [releases page](https://github.com/MckinleyWood/MoPanning/releases). Just download and run the appropriate one to install MoPanning. Your computer might tell you the app is from an unidentified developer. On Mac, you can go to your privacy and security settings and hit 'open anyway' if you trust us. On Windows, just hit 'Yes'. When you open MoPanning, make sure you select 'allow' when prompted for access to the microphone.

## Using MoPanning
On launching MoPanning, you might see a blank window at first. This is expected. In order to generate a visualization, you will first need to set up your settings. To open the settings panel, press '⌘ ,' ('ctrl ,' on Windows) or select 'MoPanning > Settings...'. 

### Audio Devices
The first and most important settings are your input and output devices. Your output device should be set to your headphones, speakers, or audio interface, and your input device should be set to where you would like to receive your audio from (if in Streaming mode).

### Input Types
MoPanning offers two methods for getting audio into the application for visualization and playback. In File mode, you can load an audio file with '⌘ o' ('ctrl o' on Windows) or 'File > Open...' and it will play back with a visualization. You can pause and start playback using the space bar.

In Streaming mode, the audio from your input device is streamed to the visualizer and your output device. Often you may want to route audio from another application (for example a streaming service or DAW) to MoPanning in this mode; instructions on how to do that are given in the Virtual Audio Devices section.

### Other Settings
The other settings MoPanning offers change aspects of the analysis algorithms and the visualization. We encourage you to tinker with the settings to find what looks the best to you.

## Virtual Audio Devices
In order to route audio from other apps to MoPanning, we recommend the BlackHole virtual audio device from Existential Audio ([link](https://existential.audio/blackhole/)) for Mac users and the VB-Cable virtual audio device from VB-Audio ([link](https://vb-audio.com/Cable/)) for Windows users. Both pieces of software are free / by donation, and the installation instructions are on the official pages.

In order to use your virtual audio device with MoPanning, set your default audio output device to the input of the device ('BlackHole Xch' or 'CABLE Input') in your computer's sound settings, and set your input device in MoPanning to the output of the virtual audio device ('BlackHole Xch' or 'CABLE Output'). Now your computer's regular audio output will be routed to MoPanning for visualization and then out to the output device you set in MoPanning. To route the output of most DAWs and other audio applications, you will need to change the application's output device instead of your default output device.