# MoPanning Instructions
Thanks for installing MoPanning, a perception-based real-time visualization program for stereo audio including music. Below are instructions on how to install and use MoPanning. 

After you have tried it out, we would love your feedback! We have a Google form at [form link], or you can reach us at mopanning@outlook.com.

## Installation
To install, drag and drop the MoPanning icon into the Applications folder. The app can then be run from there. Since we haven't paid for an Apple developer account, your Mac will tell you the app might be malware. You can go to your privacy and security settings and hit 'open anyway' if you trust us. When you open MoPanning, make sure you hit 'allow' for access to the microphone.

## Using MoPanning
On launching MoPanning, you might see a blank window at first. This is expected. In order to generate a visualization, you will first need to set up your settings. To open the settings panel, press '⌘ ,' or select 'MoPanning > Settings...'. 

### Audio Devices
The first and most important settings are your input and output devices. Your output device should be set to your headphones, speakers, or audio interface, and your input device should be set to where you would like to receive your audio from (if in Streaming mode).

### Input Types
MoPanning offers two methods for getting audio into the application for visualization and playback. In File mode, you can load an audio file with '⌘ O' or 'File > Open...' and it will play back with a visualization. You can pause and start playback using the space bar.

In Streaming mode, the audio from your input device is streamed to the visualizer and your output device. Often you may want to route audio from another application (for example a streaming service or DAW) to MoPanning in this mode; instructions on how to do that are given in the "BlackHole" section.

### Other Settings
The other settings MoPanning offers change aspects of the analysis algorithms and the visualization. We encourage you to tinker with the settings to find what looks the best to you.

## BlackHole
In order to route audio from other apps to MoPanning, we recommend the BlackHole virtual audio device from Existential Audio ([link](https://existential.audio/blackhole/)). The software is free or by donation and easy to install (instructions on the official page). 

In order to use BlackHole with MoPanning, set BlackHole as your default audio output device (through sound settings), and as your input device in MoPanning. Now your computer's regular audio output will be routed to MoPanning for visualization and then out to the output device you set in MoPanning. In DAWs and some other audio applications, you should set the output application's output device to BlackHole instead of your default output device.