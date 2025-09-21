#include "MainController.h"

//=============================================================================
MainController::MainController()
{
    // Allocate analyzer and engince
    analyzer = std::make_unique<AudioAnalyzer>();
    engine = std::make_unique<AudioEngine>();

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
                if (engine)
                    engine->setInputType(static_cast<InputType>(value));
            }
        },
        // transform
        { 
            "transform", "Frequency Transform",
            "Which frequency transform to use for analysis.", 
            ParameterDescriptor::Type::Choice, 1, {}, 
            {"FFT", "CQT"}, "",
            [this](float value) 
            {
                analyzer->setTransform(static_cast<Transform>(value));
            }
        },
        // panMethod
        {
            "panMethod", "Panning Method", 
            "What cue(s) to use for spatializing audio.", 
            ParameterDescriptor::Type::Choice, 0, {},
            {"Level Difference", "Time Difference", "Both"}, "",
            [this](float value) 
            {
                if (analyzer)
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
                if (analyzer)
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
                int newMinFreq;
                switch (static_cast<int>(value))
                {
                    case 0: newMinFreq = 5; break;
                    case 1: newMinFreq = 20; break;
                    case 2: newMinFreq = 50; break;
                    case 3: newMinFreq = 100; break;
                    default: newMinFreq = 20; break;
                }
                if (!analyzer || !visualizer)
                    return;
                analyzer->setMinFrequency(newMinFreq);
                visualizer->setMinFrequency(newMinFreq);
            }
        },
        // peakAmplitude
        {
            "peakAmplitude", "Peak Amplitude",
            "The maximum expected amplitude of the input signal.",
            ParameterDescriptor::Type::Float, 1.f,
            juce::NormalisableRange<float>(0.000001f, 1.f), {}, "",
            [this](float value) 
            {
                if (analyzer)
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
                if (analyzer)
                    analyzer->setThreshold(value);
            }
        },
        // a-weighting
        {
            "aWeighting", "A-Weighting",
            "Apply A-weighting to frequency bands.",
            ParameterDescriptor::Type::Choice, 0, {},
            {"Off", "On"}, "",
            [this](float value) 
            {
                if (analyzer)
                {
                    bool AWeighting = (static_cast<int>(value) == 1);
                    analyzer->setAWeighting(AWeighting);
                }
            }
        },
        // dimension
        {
            "dimension", "Dimension", "Visualization dimension.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"2D", "3D"}, "",
            [this](float value) 
            {
                if (visualizer)
                    visualizer->setDimension(static_cast<Dimension>(value));
            }
        },
        // colourScheme
        {
            "colourScheme", "Colour Scheme", 
            "Colour scheme for visualization.",
            ParameterDescriptor::Type::Choice, 1, {},
            {"Greyscale", "Rainbow"}, "",
            [this](float value) 
            {
                if (visualizer)
                    visualizer->setColourScheme(
                        static_cast<ColourScheme>(value));
            }
        },
        // recedeSpeed
        {
            "recedeSpeed", "Recede Speed", 
            "How fast the particles recede into the distance.",
            ParameterDescriptor::Type::Float, 5.f,
            juce::NormalisableRange<float>(0.1f, 20.f), {}, "m/s",
            [this](float value) 
            {
                if (visualizer)
                    visualizer->setRecedeSpeed(value);
            }
        },
        // dotSize
        {
            "dotSize", "Particle Size", 
            "Size of each particle in the visualization.",
            ParameterDescriptor::Type::Float, 0.15f,
            juce::NormalisableRange<float>(0.01f, 1.f), {}, "",
            [this](float value) 
            {
                if (visualizer)
                    visualizer->setDotSize(value);
            }
        },
        // ampScale
        {
            "ampScale", "Amplitude Scale", 
            "Compression/expansion factor applied to the amplitude values.",
            ParameterDescriptor::Type::Float, 1.f,
            juce::NormalisableRange<float>(0.1f, 10.f), {}, "",
            [this](float value) 
            {
                if (visualizer)
                    visualizer->setAmpScale(value);
            }
        },
        // fadeEndZ
        {
            "fadeEndZ", "Fade Distance", 
            "Distance at which particles are fully faded out.",
            ParameterDescriptor::Type::Float, 5.f,
            juce::NormalisableRange<float>(0.1f, 10.f), {}, "m",
            [this](float value) 
            {
                if (visualizer)
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
void MainController::startAudio()
{
    // Initialize all parameters to their default values
    for (auto& d : parameterDescriptors)
    {
        if (d.onChanged)
            d.onChanged(d.defaultValue);
    }

    // Set up the audio device manager and start audio callback
    auto& dm = engine->getDeviceManager();
    dm.addAudioCallback(this);
    dm.initialise(2, 2, nullptr, true);
}

void MainController::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    
    // Delegate to the audio engine
    engine->fillAudioBuffers(inputChannelData, numInputChannels,
                            outputChannelData, numOutputChannels,
                            numSamples, buffer);

    // Pass the buffer to the analyzer
    analyzer->enqueueBlock(&buffer);

    juce::ignoreUnused(context);
}

void MainController::audioDeviceAboutToStart(juce::AudioIODevice* device) 
{
    double sampleRate = device->getCurrentSampleRate();
    int samplesPerBlock = device->getCurrentBufferSizeSamples();
    
    // engine->setInputType(static_cast<InputType>(getInputType()));
    engine->prepareToPlay(samplesPerBlock, sampleRate);
    analyzer->setPrepared(false);
    analyzer->prepareToPlay(samplesPerBlock, sampleRate);
    visualizer->prepareToPlay(samplesPerBlock, sampleRate);
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

bool MainController::loadFile(const juce::File& f)
{
    return engine->loadFile(f);
}

void MainController::togglePlayback()
{
    engine->togglePlayback();
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

std::vector<frequency_band> MainController::getLatestResults() const
{
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
    DBG("Parameter changed: " << paramID << " -> " << newValue);

    // Find the corresponding descriptor and call its onChanged callback
    auto it = std::find_if(parameterDescriptors.begin(), 
                           parameterDescriptors.end(),
                           [&paramID](const ParameterDescriptor& d) 
                               { return d.id == paramID; });

    if (it != parameterDescriptors.end() && it->onChanged)
    {
        it->onChanged(newValue);
    }

    analyzer->prepareToPlay();
}