#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SpeakNSpellVoice.h"

class SAMVoiceSynthesizerAudioProcessor final : public juce::AudioProcessor
{
public:
    SAMVoiceSynthesizerAudioProcessor();
    ~SAMVoiceSynthesizerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void enqueueText (const juce::String& text);

    void setParameters (const SpeakNSpellVoice::Parameters& newParams);
    SpeakNSpellVoice::Parameters getParameters() const;
    void setRealtimeControls (const SpeakNSpellVoice::RealtimeControls& controls);
    SpeakNSpellVoice::RealtimeControls getRealtimeControls() const;
    void setNodePath (const juce::String& path);
    juce::String getNodePath() const;
    void setLoopAtEnd (bool shouldLoop);
    bool getLoopAtEnd() const;

    juce::String getVoiceStatus() const;
    juce::String getUdpStatus() const;
    juce::String getUdpFeed() const;

private:
    class UdpTextReceiver;

    void appendUdpLine (const juce::String& text);

    SpeakNSpellVoice voice;
    juce::String triggerText { "Hello! This is a SAM-style voice synthesizer." };

    mutable juce::CriticalSection paramsLock;
    SpeakNSpellVoice::Parameters parameters;
    SpeakNSpellVoice::RealtimeControls realtimeControls;
    juce::String nodePath;
    bool loopAtEnd = false;
    int currentProgram = 0;

    mutable juce::CriticalSection udpStatusLock;
    juce::String udpStatus { "UDP: starting..." };

    mutable juce::CriticalSection udpFeedLock;
    juce::String udpFeed { "[waiting for UDP on port 7001]" };

    std::unique_ptr<UdpTextReceiver> udpReceiver;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SAMVoiceSynthesizerAudioProcessor)
};
