#include "MainController.h"

//=============================================================================
MainController::MainController()
{
    // Initialize audio analyzer and engine
    analyzer = std::make_unique<AudioAnalyzer>();
    engine = std::make_unique<AudioEngine>();

    trackGains.resize(8, 1.0f);

    // Set up parameter descriptors - all parameters should be listed here
    parameterDescriptors =
    {
        // inputType
        { 
            "inputType", "Input Type", "Where to receive audio input from.", 
            ParameterDescriptor::Type::Choice, 1, {}, 
            {"File", "Streaming"}, "",
            [this](float value) 
            {
                if (engine != nullptr)
                    engine->setInputType(static_cast<InputType>(value));
            }
        },
        // windowSize
        {
            "windowSize", "Window Size", 
            "The length of the analysis window in samples.",
            ParameterDescriptor::Type::Choice, 2, {},
            {"256", "512", "1024", "2048", "4096"}, "",
            [this](float value) 
            {
                int newSize;
                switch (static_cast<int>(value))
                {
                    case 0: newSize = 256; break;
                    case 1: newSize = 512; break;
                    case 2: newSize = 1024; break;
                    case 3: newSize = 2048; break;
                    case 4: newSize = 4096; break;
                    default: newSize = 1024; break;
                }
                if (analyzer != nullptr)
                    analyzer->setWindowSize(newSize);
            },
            true
        },
        // hopSize
        {
            "hopSize", "Hop Size", 
            "The number of samples between analysis windows.",
            ParameterDescriptor::Type::Choice, 2, {},
            {"128", "256", "512", "1024", "2048", "4096"}, "",
            [this](float value) 
            {
                int newSize;
                switch (static_cast<int>(value))
                {
                    case 0: newSize = 128; break;
                    case 1: newSize = 256; break;
                    case 2: newSize = 512; break;
                    case 3: newSize = 1024; break;
                    default: newSize = 256; break;
                }
                if (analyzer != nullptr)
                    analyzer->setHopSize(newSize);
            },
            true
        },
        // transform
        { 
            "transform", "Frequency Transform",
            "Which frequency transform to use for analysis.", 
            ParameterDescriptor::Type::Choice, 1, {}, 
            {"FFT", "CQT"}, "",
            [this](float value) 
            {
                if (analyzer != nullptr)
                    analyzer->setTransform(static_cast<Transform>(value));
            },
            false
        },
        // panMethod
        {
            "panMethod", "Panning Method", 
            "What cue(s) to use for spatializing audio.", 
            ParameterDescriptor::Type::Choice, 0, {},
            {"Level Difference", "Time Difference", "Both"}, "",
            [this](float value) 
            {
                if (analyzer != nullptr)
                    analyzer->setPanMethod(static_cast<PanMethod>(value));
            }
        },
        // numCQTbins
        {
            "numCQTbins", "Number of CQT Bins", 
            "Number of frequency bins in the Constant-Q Transform.",
            ParameterDescriptor::Type::Choice, 2, {},
            {"64", "128", "256", "512", "1024"}, "",
            [this](float value) 
            {
                int newNumBins;
                switch (static_cast<int>(value))
                {
                    case 0: newNumBins = 64; break;
                    case 1: newNumBins = 128; break;
                    case 2: newNumBins = 256; break;
                    case 3: newNumBins = 512; break;
                    case 4: newNumBins = 1024; break;
                    default: newNumBins = 128; break;
                }
                if (analyzer != nullptr)
                    analyzer->setNumCQTBins(newNumBins);
            }
        },
        // minFrequency
        {
            "minFrequency", "Minimum Frequency", 
            "Minimum frequency (Hz) to include in the analysis.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"5Hz", "20Hz", "50Hz", "100Hz"}, "",
            [this](float value) 
            {
                float newMinFreq;
                switch ((int)value)
                {
                    case 0: newMinFreq = 5.0f; break;
                    case 1: newMinFreq = 20.0f; break;
                    case 2: newMinFreq = 50.0f; break;
                    case 3: newMinFreq = 100.0f; break;
                    default: newMinFreq = 20.0f; break;
                }

                grid->setMinFrequency(newMinFreq);
                visualizer->setMinFrequency(newMinFreq);
                analyzer->setMinFrequency(newMinFreq);
                updateGridTexture();
            }

        },
        // peakAmplitude
        {
            "peakAmplitude", "Peak Amplitude",
            "The maximum expected amplitude of the input signal.",
            ParameterDescriptor::Type::Float, 1.f,
            juce::NormalisableRange<float>(0.000001f, 1.f, 0.0f, 0.5f), {}, "",
            [this](float value) 
            {
                if (analyzer != nullptr)
                    analyzer->setMaxAmplitude(value);
            }
        },
        // threshold
        {
            "threshold", "Amplitude Threshold",
            "The amplitude level (dB relative to peak) below which frequency bands are ignored.",
            ParameterDescriptor::Type::Float, -60.f,
            juce::NormalisableRange<float>(-120.f, -20.f), {}, "dB",
            [this](float value) 
            {
                if (analyzer != nullptr)
                    analyzer->setThreshold(value);
            }
        },
        // freqWeighting
        {
            "freqWeighting", "Frequency Weighting",
            "Choose a frequency weighting curve to apply to the input signal.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"None", "A-Weighting"}, "",
            [this](float value) 
            {

                if (analyzer != nullptr)
                    analyzer->setFreqWeighting(static_cast<FrequencyWeighting> 
                                                    (value));
            }
        },
        // dimension
        {
            "dimension", "Dimension", "Visualization dimension.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"2D", "3D"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setDimension(static_cast<Dimension>(value));
                if (onDimChanged)
                    onDimChanged(static_cast<int>(value));
            }
        },
        // colourSchemeTrack1
        {
            "track1ColourScheme", "Track 1 Colour Scheme", 
            "Colour scheme for visualization of track 1.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 0);
            },
            true
        },
        // gainTrack1
        {
            "track1Gain", "Track 1 Gain", 
            "Gain of track 1.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[0])
                    trackGains[0] = value;
            }
        },
        // colourSchemeTrack2
        {
            "track2ColourScheme", "Track 2 Colour Scheme", 
            "Colour scheme for visualization of track 2.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 1);
            }
        },
        // gainTrack2
        {
            "track2Gain", "Track 2 Gain", 
            "Gain of track 2.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[1])
                    trackGains[1] = value;
            }
        },
        // colourSchemeTrack3
        {
            "track3ColourScheme", "Track 3 Colour Scheme", 
            "Colour scheme for visualization of track 3.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 2);
            }
        },
        // gainTrack3
        {
            "track3Gain", "Track 3 Gain", 
            "Gain of track 3.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[2])
                    trackGains[2] = value;
            }
        },
        // colourSchemeTrack4
        {
            "track4ColourScheme", "Track 4 Colour Scheme", 
            "Colour scheme for visualization of track 4.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 3);
            }
        },
        // gainTrack4
        {
            "track4Gain", "Track 4 Gain", 
            "Gain of track 4.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[3])
                    trackGains[3] = value;
            }
        },
        // colourSchemeTrack5
        {
            "track5ColourScheme", "Track 5 Colour Scheme", 
            "Colour scheme for visualization of track 5.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 4);
            }
        },
        // gainTrack5
        {
            "track5Gain", "Track 5 Gain", 
            "Gain of track 5.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[4])
                    trackGains[4] = value;
            }
        },
        // colourSchemeTrack6
        {
            "track6ColourScheme", "Track 6 Colour Scheme", 
            "Colour scheme for visualization of track 6.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 5);
            }
        },
        // gainTrack6
        {
            "track6Gain", "Track 6 Gain", 
            "Gain of track 6.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[5])
                    trackGains[5] = value;
            }
        },
        // colourSchemeTrack7
        {
            "track7ColourScheme", "Track 7 Colour Scheme", 
            "Colour scheme for visualization of track 7.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 6);
            }
        },
        // gainTrack7
        {
            "track7Gain", "Track 7 Gain", 
            "Gain of track 7.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[6])
                    trackGains[6] = value;
            }
        },
        // colourSchemeTrack8
        {
            "track8ColourScheme", "Track 8 Colour Scheme", 
            "Colour scheme for visualization of track 8.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow", "Red", "Orange", "Yellow", "Light Green", "Dark Green", "Light Blue", "Dark Blue", "Purple", "Pink", "Warm", "Cool"}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setTrackColourScheme(
                        static_cast<ColourScheme>(value), 7);
            }
        },
        // gainTrack8
        {
            "track8Gain", "Track 8 Gain", 
            "Gain of track 8.",
            ParameterDescriptor::Type::Float, 1.0f,
            juce::NormalisableRange<float>(0.000001f, 1.0f), {}, "",
            [this](float value) 
            {
                if (trackGains[7])
                    trackGains[7] = value;
            }
        },
        // showGrid
        {
            "showGrid", "Show Grid", 
            "Toggle display of the grid.",
            ParameterDescriptor::Type::Choice, 0, {},
            {"Off", "On"}, "",
            [this](float value) 
            {
                bool showGrid = (static_cast<int>(value) == 1);
                grid->setGridVisible(showGrid);
                if (visualizer != nullptr)
                    visualizer->setShowGrid(showGrid);
            },
            false
        },
        // recedeSpeed
        {
            "recedeSpeed", "Recede Speed", 
            "How fast the particles recede into the distance.",
            ParameterDescriptor::Type::Float, 5.f,
            juce::NormalisableRange<float>(0.1f, 20.f), {}, "m/s",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setRecedeSpeed(value);
            }
        },
        // dotSize
        {
            "dotSize", "Particle Size", 
            "Size of each particle in the visualization.",
            ParameterDescriptor::Type::Float, 0.1f,
            juce::NormalisableRange<float>(0.01f, 1.f), {}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setDotSize(value);
            }
        },
        // ampScale
        {
            "ampScale", "Amplitude Scale", 
            "Compression/expansion factor applied to the amplitude values.",
            ParameterDescriptor::Type::Float, 1.f,
            juce::NormalisableRange<float>(0.0f, 1.0f), {}, "",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setAmpScale(value);
            },
            false
        },
        // fadeEndZ
        {
            "fadeEndZ", "Fade Distance", 
            "Distance at which particles are fully faded out.",
            ParameterDescriptor::Type::Float, 5.f,
            juce::NormalisableRange<float>(0.1f, 10.f), {}, "m",
            [this](float value) 
            {
                if (visualizer != nullptr)
                    visualizer->setFadeEndZ(value);
            }
        }
    };

    // Set up AudioProcessorValueTreeState
    auto layout = makeParameterLayout(parameterDescriptors);
    processor = std::make_unique<MiniAudioProcessor>(std::move(layout));

    apvts = &processor->getValueTreeState();
    apvts->state.addListener(this);
}

MainController::~MainController()
{
    apvts->state.removeListener(this);
    auto& dm = engine->getDeviceManager();
    dm.removeAudioCallback(this);
}

//=============================================================================
/*  Initializes the device manager and starts audio playback. */
void MainController::startAudio()
{
    // Set up the audio device manager
    auto& dm = engine->getDeviceManager();
    dm.initialise(2, 2, nullptr, true);

    // Set buffer size to 512 samples as a default
    auto setup = dm.getAudioDeviceSetup();
    setup.bufferSize = 512;
    dm.setAudioDeviceSetup(setup, true);

    // Register this as an audio callback - audio starts now
    dm.addAudioCallback(this);

    // Prepare internal buffers
    buffers.resize(2);
    for (auto& buf : buffers)
        buf.setSize(2, 512);
}

/*  The function that is called every time there is a new audio block to
    process. The AudioEngine will handle the audio data according to the 
    current input type (file or streaming). Output to the audio device 
    goes through outputChannelData and the AudioAnalyzer receives a copy 
    of the audio in 'buffer' for analysis.
*/
void MainController::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    numTracks = numInputChannels / 2;


    for (int track = 0; track < numTracks; track++)
    {
        const float* selectedChannels[2] = {
            inputChannelData[2*track],
            inputChannelData[2*track + 1]
        };

        bool isFirstTrack = (track == 0);

        // Delegate to the audio engine
        engine->fillAudioBuffers(selectedChannels, 2,
                                outputChannelData, numOutputChannels,
                                numSamples, buffers[track], isFirstTrack, trackGains[track]);

        // Pass the buffer to the analyzer
        analyzer->enqueueBlock(&buffers[track], track);
    }
    
    juce::ignoreUnused(context);
}

void MainController::audioDeviceAboutToStart(juce::AudioIODevice* device) 
{
    double sampleRate = device->getCurrentSampleRate();
    int samplesPerBlock = device->getCurrentBufferSizeSamples();
    int numInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
    numTracks = (numInputChannels > 0) ? numInputChannels / 2 : 1;  // Fallback to 1 if no inputs

    if (onNumTracksChanged)
        onNumTracksChanged(numTracks);

    // Ensure buffers vector matches number of stereo tracks
    if ((int)buffers.size() != numTracks)
    {
        const int oldSize = (int)buffers.size();
        buffers.resize(numTracks);

        // Initialize any newly added buffers right away
        for (int i = oldSize; i < numTracks; ++i)
            buffers[i].setSize(2, samplesPerBlock, false, false, true);
    }
    
    engine->prepareToPlay(samplesPerBlock, sampleRate);
    analyzer->setPrepared(false);
    analyzer->prepare(sampleRate, numTracks);
    visualizer->setSampleRate(sampleRate);
    grid->setSampleRate(sampleRate);
}

void MainController::audioDeviceStopped() 
{
    engine->releaseResources(); 
}

//=============================================================================
ParamLayout MainController::makeParameterLayout(
    const std::vector<ParameterDescriptor>& descriptors)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (auto& d : descriptors)
    {
        if (d.type == ParameterDescriptor::Type::Float)
        {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                d.id, d.displayName, d.range, d.defaultValue));
        }
        else if (d.type == ParameterDescriptor::Type::Choice)
        {
            int defaultIndex = static_cast<int>(d.defaultValue);
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                d.id, d.displayName, d.choices, defaultIndex));
        }
    }

    return layout;
}

//=============================================================================
void MainController::registerVisualizer(GLVisualizer* v)
{
    visualizer = v;
}

void MainController::registerGrid(GridComponent* g)
{
    grid = g;
}

void MainController::setDefaultParameters()
{
    for (auto& d : parameterDescriptors)
    {
        if (d.onChanged)
            d.onChanged(d.defaultValue);
    }
}

bool MainController::loadFile(const juce::File& f)
{
    return engine->loadFile(f);
}

void MainController::togglePlayback()
{
    engine->togglePlayback();
}

void MainController::updateGridTexture()
{
    visualizer->createGridImageFromComponent(grid);
}

//=============================================================================
std::vector<ParameterDescriptor> MainController::getParameterDescriptors() const
{
    return parameterDescriptors;
}

juce::AudioProcessorValueTreeState& MainController::getAPVTS() noexcept
{
    return processor->getValueTreeState();;
}

std::vector<std::vector<frequency_band>> MainController::getLatestResults() const
{
    std::lock_guard<std::mutex> lock(analyzer->getResultsMutex());
    return analyzer->getLatestResults();
}

juce::AudioDeviceManager& MainController::getDeviceManager()
{
    return engine->getDeviceManager();
}

//=============================================================================
void MainController::valueTreePropertyChanged(juce::ValueTree& tree, 
                                              const juce::Identifier& id)
{
    if (id.toString() != "value") return;

    juce::String paramID = tree.getProperty("id").toString(); 
    float newValue = tree.getProperty("value");
    // DBG("Parameter changed: " << paramID << " -> " << newValue);

    // Find the corresponding descriptor and call its onChanged callback
    auto it = std::find_if(parameterDescriptors.begin(), 
                           parameterDescriptors.end(),
                           [&paramID](const ParameterDescriptor& d) 
                               { return d.id == paramID; });

    if (it != parameterDescriptors.end() && it->onChanged)
    {
        it->onChanged(newValue);
    }

    analyzer->prepare();
}