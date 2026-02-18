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
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void enqueueText (const juce::String& text);

    void setParameters (const SpeakNSpellVoice::Parameters& newParams);
    SpeakNSpellVoice::Parameters getParameters() const;

    juce::String getVoiceStatus() const;
    juce::String getUdpStatus() const;
    juce::String getUdpFeed() const;

private:
    class UdpTextReceiver;

    void appendUdpLine (const juce::String& text);

    SpeakNSpellVoice voice;

    mutable juce::CriticalSection paramsLock;
    SpeakNSpellVoice::Parameters parameters;

    mutable juce::CriticalSection udpStatusLock;
    juce::String udpStatus { "UDP: starting..." };

    mutable juce::CriticalSection udpFeedLock;
    juce::String udpFeed { "[waiting for UDP on port 7001]" };

    std::unique_ptr<UdpTextReceiver> udpReceiver;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SAMVoiceSynthesizerAudioProcessor)
};
