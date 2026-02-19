#include "PluginProcessor.h"
#include "PluginEditor.h"

class SAMVoiceSynthesizerAudioProcessor::UdpTextReceiver final : private juce::Thread
{
public:
    explicit UdpTextReceiver (std::function<void (juce::String)> onTextIn,
                              std::function<void (juce::String)> onStatusIn)
        : juce::Thread ("SAMUdpReceiver"),
          onText (std::move (onTextIn)),
          onStatus (std::move (onStatusIn))
    {
    }

    bool start (int portNumber)
    {
        stop();

        socket = std::make_unique<juce::DatagramSocket> (false);
        if (! socket->bindToPort (portNumber))
        {
            socket.reset();
            onStatus ("UDP: bind failed on port " + juce::String (portNumber));
            return false;
        }

        onStatus ("UDP: listening on port " + juce::String (portNumber));
        startThread();
        return true;
    }

    void stop()
    {
        signalThreadShouldExit();
        if (socket != nullptr)
            socket->shutdown();
        stopThread (800);
        socket.reset();
    }

    ~UdpTextReceiver() override
    {
        stop();
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            if (socket == nullptr)
                return;

            if (socket->waitUntilReady (true, 250) <= 0)
                continue;

            char payload[4096] {};
            const auto bytes = socket->read (payload, static_cast<int> (sizeof (payload) - 1), false);
            if (bytes <= 0)
                continue;

            auto text = juce::String::fromUTF8 (payload, bytes).trim();
            if (text.isNotEmpty())
                onText (text);
        }
    }

    std::function<void (juce::String)> onText;
    std::function<void (juce::String)> onStatus;
    std::unique_ptr<juce::DatagramSocket> socket;
};

SAMVoiceSynthesizerAudioProcessor::SAMVoiceSynthesizerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    setCurrentProgram (0);
    setNodePath (juce::SystemStats::getEnvironmentVariable ("SAM_NODE_PATH", {}));

    udpReceiver = std::make_unique<UdpTextReceiver> (
        [this] (juce::String text)
        {
            appendUdpLine (text);
            enqueueText (text);
        },
        [this] (juce::String status)
        {
            const juce::ScopedLock sl (udpStatusLock);
            udpStatus = status;
        });

    udpReceiver->start (7001);
}

SAMVoiceSynthesizerAudioProcessor::~SAMVoiceSynthesizerAudioProcessor()
{
    udpReceiver.reset();
}

void SAMVoiceSynthesizerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    voice.setSampleRate (sampleRate);
}

void SAMVoiceSynthesizerAudioProcessor::releaseResources()
{
}

bool SAMVoiceSynthesizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    return (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo());
}

void SAMVoiceSynthesizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const auto textToSpeak = triggerText.trim();
            if (textToSpeak.isNotEmpty())
                voice.queueText (textToSpeak, getParameters());
        }
    }

    voice.setRealtimeControls (getRealtimeControls());
    voice.setLoopAtEnd (getLoopAtEnd());
    voice.render (buffer, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SAMVoiceSynthesizerAudioProcessor::createEditor()
{
    return new SAMVoiceSynthesizerAudioProcessorEditor (*this);
}

int SAMVoiceSynthesizerAudioProcessor::getNumPrograms()
{
    return SpeakNSpellVoice::getNumFactoryPresets();
}

int SAMVoiceSynthesizerAudioProcessor::getCurrentProgram()
{
    const juce::ScopedLock sl (paramsLock);
    return currentProgram;
}

void SAMVoiceSynthesizerAudioProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    SpeakNSpellVoice::Parameters p;
    SpeakNSpellVoice::RealtimeControls r;
    SpeakNSpellVoice::applyFactoryPreset (index, p, r);
    {
        const juce::ScopedLock sl (paramsLock);
        currentProgram = index;
        parameters = p;
        realtimeControls = r;
    }
}

const juce::String SAMVoiceSynthesizerAudioProcessor::getProgramName (int index)
{
    return SpeakNSpellVoice::getFactoryPresetName (index);
}

void SAMVoiceSynthesizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto p = getParameters();
    juce::ValueTree state ("SAMPluginState");
    state.setProperty ("speed", p.speed, nullptr);
    state.setProperty ("pitch", p.pitch, nullptr);
    state.setProperty ("mouth", p.mouth, nullptr);
    state.setProperty ("throat", p.throat, nullptr);
    state.setProperty ("singMode", p.singMode, nullptr);
    state.setProperty ("phoneticInput", p.phoneticInput, nullptr);
    state.setProperty ("backend", static_cast<int> (p.backend), nullptr);
    const auto rt = getRealtimeControls();
    state.setProperty ("rtSpeed", rt.playbackSpeed, nullptr);
    state.setProperty ("rtPitchSemitones", rt.repitchSemitones, nullptr);
    state.setProperty ("rtFormant", rt.formantWarp, nullptr);
    state.setProperty ("rtGate", rt.glitchGate, nullptr);
    state.setProperty ("rtCrush", rt.bitCrush, nullptr);
    state.setProperty ("rtLoop", rt.microLoop, nullptr);
    state.setProperty ("rtTilt", rt.spectralTilt, nullptr);
    state.setProperty ("rtRing", rt.ringMod, nullptr);
    state.setProperty ("rtShift", rt.freqShift, nullptr);
    state.setProperty ("rtJitter", rt.repitchJitter, nullptr);
    state.setProperty ("rtMutation", rt.mutation, nullptr);
    state.setProperty ("nodePath", getNodePath(), nullptr);
    state.setProperty ("loopAtEnd", getLoopAtEnd(), nullptr);
    state.setProperty ("currentProgram", getCurrentProgram(), nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SAMVoiceSynthesizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;

    SpeakNSpellVoice::Parameters p;
    p.speed = static_cast<int> (state.getProperty ("speed", p.speed));
    p.pitch = static_cast<int> (state.getProperty ("pitch", p.pitch));
    p.mouth = static_cast<int> (state.getProperty ("mouth", p.mouth));
    p.throat = static_cast<int> (state.getProperty ("throat", p.throat));
    p.singMode = static_cast<bool> (state.getProperty ("singMode", p.singMode));
    p.phoneticInput = static_cast<bool> (state.getProperty ("phoneticInput", p.phoneticInput));
    p.backend = static_cast<SpeakNSpellVoice::Parameters::Backend> (static_cast<int> (state.getProperty ("backend", static_cast<int> (p.backend))));
    setParameters (p);

    SpeakNSpellVoice::RealtimeControls rt;
    rt.playbackSpeed = static_cast<float> (state.getProperty ("rtSpeed", rt.playbackSpeed));
    rt.repitchSemitones = static_cast<float> (state.getProperty ("rtPitchSemitones", rt.repitchSemitones));
    rt.formantWarp = static_cast<float> (state.getProperty ("rtFormant", rt.formantWarp));
    rt.glitchGate = static_cast<float> (state.getProperty ("rtGate", rt.glitchGate));
    rt.bitCrush = static_cast<float> (state.getProperty ("rtCrush", rt.bitCrush));
    rt.microLoop = static_cast<float> (state.getProperty ("rtLoop", rt.microLoop));
    rt.spectralTilt = static_cast<float> (state.getProperty ("rtTilt", rt.spectralTilt));
    rt.ringMod = static_cast<float> (state.getProperty ("rtRing", rt.ringMod));
    rt.freqShift = static_cast<float> (state.getProperty ("rtShift", rt.freqShift));
    rt.repitchJitter = static_cast<float> (state.getProperty ("rtJitter", rt.repitchJitter));
    rt.mutation = static_cast<float> (state.getProperty ("rtMutation", rt.mutation));
    setRealtimeControls (rt);
    setLoopAtEnd (static_cast<bool> (state.getProperty ("loopAtEnd", false)));
    if (state.hasProperty ("currentProgram"))
    {
        const auto programIndex = static_cast<int> (state.getProperty ("currentProgram", 0));
        {
            const juce::ScopedLock sl (paramsLock);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, programIndex);
        }
    }
    if (state.hasProperty ("nodePath"))
        setNodePath (state.getProperty ("nodePath").toString());
}

void SAMVoiceSynthesizerAudioProcessor::enqueueText (const juce::String& text)
{
    triggerText = text;
    voice.queueText (text, getParameters());
}

void SAMVoiceSynthesizerAudioProcessor::setParameters (const SpeakNSpellVoice::Parameters& newParams)
{
    const juce::ScopedLock sl (paramsLock);
    parameters = newParams;
}

SpeakNSpellVoice::Parameters SAMVoiceSynthesizerAudioProcessor::getParameters() const
{
    const juce::ScopedLock sl (paramsLock);
    return parameters;
}

void SAMVoiceSynthesizerAudioProcessor::setRealtimeControls (const SpeakNSpellVoice::RealtimeControls& controls)
{
    const juce::ScopedLock sl (paramsLock);
    realtimeControls = controls;
}

SpeakNSpellVoice::RealtimeControls SAMVoiceSynthesizerAudioProcessor::getRealtimeControls() const
{
    const juce::ScopedLock sl (paramsLock);
    return realtimeControls;
}

void SAMVoiceSynthesizerAudioProcessor::setNodePath (const juce::String& path)
{
    const auto trimmed = path.trim();
    {
        const juce::ScopedLock sl (paramsLock);
        nodePath = trimmed;
    }
    voice.setCustomNodePath (trimmed);
}

juce::String SAMVoiceSynthesizerAudioProcessor::getNodePath() const
{
    const juce::ScopedLock sl (paramsLock);
    return nodePath;
}

void SAMVoiceSynthesizerAudioProcessor::setLoopAtEnd (bool shouldLoop)
{
    {
        const juce::ScopedLock sl (paramsLock);
        loopAtEnd = shouldLoop;
    }
    voice.setLoopAtEnd (shouldLoop);
}

bool SAMVoiceSynthesizerAudioProcessor::getLoopAtEnd() const
{
    const juce::ScopedLock sl (paramsLock);
    return loopAtEnd;
}

juce::String SAMVoiceSynthesizerAudioProcessor::getVoiceStatus() const
{
    return voice.getStatusText();
}

juce::String SAMVoiceSynthesizerAudioProcessor::getUdpStatus() const
{
    const juce::ScopedLock sl (udpStatusLock);
    return udpStatus;
}

juce::String SAMVoiceSynthesizerAudioProcessor::getUdpFeed() const
{
    const juce::ScopedLock sl (udpFeedLock);
    return udpFeed;
}

void SAMVoiceSynthesizerAudioProcessor::appendUdpLine (const juce::String& text)
{
    const juce::ScopedLock sl (udpFeedLock);

    if (udpFeed.contains ("[waiting for UDP on port 7001]"))
        udpFeed.clear();

    udpFeed << text << "\n";
    constexpr int maxChars = 12000;
    if (udpFeed.length() > maxChars)
        udpFeed = udpFeed.substring (udpFeed.length() - maxChars);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SAMVoiceSynthesizerAudioProcessor();
}
