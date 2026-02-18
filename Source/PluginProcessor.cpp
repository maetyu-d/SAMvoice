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

void SAMVoiceSynthesizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    voice.render (buffer, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SAMVoiceSynthesizerAudioProcessor::createEditor()
{
    return new SAMVoiceSynthesizerAudioProcessorEditor (*this);
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
}

void SAMVoiceSynthesizerAudioProcessor::enqueueText (const juce::String& text)
{
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
