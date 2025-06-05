## A. Current Architecture ­- “What’s there and what it does”

```text
app/Main.cpp
└─► GuiAppApplication
      └─► MainWindow
              └─► MainComponent
                       │ UI (visualiser + settings panes)
                       └─► MainController  ←─ created inside MC
                                  │ façade / traffic-cop
                                  ├─► AudioEngine    (I/O & playback)
                                  └─► AudioAnalyzer  (DSP -- stub for now)
```

| File                              | Role                                                                                                                                              | Key points                                                                                                                                                                                                                                                                                                                                                                                                                            |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Main.cpp**                      | Boiler-plate JUCE application entry. Builds the desktop window and embeds **MainComponent**.                                                      | Only touches UI, **not** audio yet.                                                                                                                                                                                                                                                                                                                                                                                                   |
| **MainComponent.\[h/cpp]**        | Top-level UI container. Holds:<br>• `GLVisualizer` (OpenGL canvas)<br>• `SettingsComponent` (sidebar)<br>• `MainController& controller` reference | Passes user actions to `controller`.<br>Switches between *Focus* (full visualiser) and *Split* (sidebar visible) views.                                                                                                                                                                                                                                                                                                               |
| **SettingsComponent.\[h/cpp]**    | GUI controls (currently only stubs).                                                                                                              | Will present the *Load File* button / menu action.                                                                                                                                                                                                                                                                                                                                                                                    |
| **GLVisualizer.\[h/cpp]**         | Renders the point-cloud via OpenGL.                                                                                                               | Right now just a blank component calling `setContinuousRepainting (true);`                                                                                                                                                                                                                                                                                                                                                            |
| **MainController.\[h/cpp]**       | **Single façade** for everything non-GUI. Owns:<br>• `AudioEngine engine;`<br>• `AudioAnalyzer analyzer;` (stub)                                  | Implements `AudioSource` so that (later) the whole app can feed on exactly the same audio blocks. Exposes `bool loadFile (File)` to UI.                                                                                                                                                                                                                                                                                               |
| **AudioEngine.\[h/cpp]**          | Smallest possible I/O pipe:<br>`AudioTransportSource → AudioSourcePlayer → AudioDeviceManager`                                                    | *Constructor*<br>  1. Registers basic file formats.<br>  2. Opens default output device (0 ins, 2 outs).<br>  3. Wires `AudioSourcePlayer` to the device and sets itself (`transport`) as the source.<br>*loadFile()*<br>  • Creates a reader, wraps it in `AudioFormatReaderSource`, attaches to `transport`, starts playback.<br>*implements `AudioSource`* → lets `MainController` (and later the analyser) pull the same samples. |
| **AudioAnalyzer.\[h/cpp]** (stub) | Will hold STFT, ILD, clustering, etc.                                                                                                             | Not compiled yet — only a placeholder in `MainController`.                                                                                                                                                                                                                                                                                                                                                                            |

**Compilation layout**

```
source/
    MainComponent.*
    MainController.*
    GLVisualizer.*
    SettingsComponent.*
    io/
        AudioEngine.*
    dsp/
        AudioAnalyzer.*   (stub)
app/
    Main.cpp
```

Everything in `source/` (and sub-folders) is added to the CMake target, so only adding a `.cpp` file is enough to compile it.

---

## B. What’s left to **load and play a file from the UI**

| Step                                                                           | Who does it?                                                                                                                                                                                                  | Changes                                                                                                        |
| ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| **1. Provide a UI entry** – button, menu item, keyboard shortcut.              | `SettingsComponent` (or `MainComponent` menu)                                                                                                                                                                 | Use a `juce::FileChooser`:<br>`File file; if (fc.browseForFileToOpen()) controller.loadFile (fc.getResult());` |
| **2. Error handling**                                                          | Same place                                                                                                                                                                                                    | If `loadFile` returns `false`, show an alert.                                                                  |
| **3. (Optional) Pass controller to audio hardware instead of raw AudioEngine** | `Main.cpp` **only if** you want analyser & playback to share the identical block. Two-line change: <br>`cpp<br>deviceManager.initialise (0,2,nullptr,true);<br>audioPlayer.setSource (controller.get());<br>` | Otherwise you can leave Main.cpp untouched – the current AudioEngine constructor already feeds the device.     |
| **4. Pause / Stop controls (optional)**                                        | UI + small wrappers in `MainController`                                                                                                                                                                       | Expose `play()`, `stop()`, `isPlaying()` that forward to `engine.transport`.                                   |
| **5. Analyzer hookup (later)**                                                 | `MainController::getNextAudioBlock()`                                                                                                                                                                         | Currently commented-out: put your STFT etc. here and send data to `GLVisualizer` through a lock-free queue.    |

**Bottom line:**
*Backend is complete.*
As soon as the UI calls `controller.loadFile (someFile)` the chosen file will start playing through the Mac’s default output and buffers will flow through `MainController::getNextAudioBlock()`.

Hand this checklist to your teammate — they only need to wire a `FileChooser` to that single function call, and playback will work.
