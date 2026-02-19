#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "SpeakNSpellVoice.h"

class SpeakNSpellAudioSource final : public juce::AudioSource
{
public:
    void queueText (const juce::String& text, SpeakNSpellVoice::Parameters params)
    {
        voice.queueText (text, params);
    }

    juce::String getStatusText() const
    {
        return voice.getStatusText();
    }

    void setRealtimeControls (const SpeakNSpellVoice::RealtimeControls& controls)
    {
        voice.setRealtimeControls (controls);
    }

    void setCustomNodePath (const juce::String& path)
    {
        voice.setCustomNodePath (path);
    }

    void setLoopAtEnd (bool shouldLoop)
    {
        voice.setLoopAtEnd (shouldLoop);
    }

    juce::String getCustomNodePath() const
    {
        return voice.getCustomNodePath();
    }

    void prepareToPlay (int, double sampleRate) override
    {
        voice.setSampleRate (sampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();
        voice.render (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
    }

private:
    SpeakNSpellVoice voice;
};

class MainComponent final : public juce::Component,
                            private juce::Button::Listener,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class UdpTextReceiver;

    void buttonClicked (juce::Button* button) override;
    void timerCallback() override;
    void speakText (const juce::String& text);
    SpeakNSpellVoice::Parameters getCurrentParameters() const;
    SpeakNSpellVoice::RealtimeControls getRealtimeControls() const;
    void applyParametersToUi (const SpeakNSpellVoice::Parameters& p);
    void applyRealtimeControlsToUi (const SpeakNSpellVoice::RealtimeControls& c);
    void applyPreset (int presetIndex);
    juce::String getNodePathInput() const;
    void appendUdpLine (const juce::String& text);

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::TextEditor textEditor;
    juce::TextButton speakButton { "Speak" };
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::ToggleButton singModeButton { "Sing Mode" };
    juce::ToggleButton phoneticModeButton { "Phonetic Input" };
    juce::ToggleButton backendModeButton { "Better SAM mode" };
    juce::ToggleButton loopEndButton { "Loop At End" };
    juce::Label speedLabel, pitchLabel, mouthLabel, throatLabel;
    juce::Slider speedSlider, pitchSlider, mouthSlider, throatSlider;
    juce::Label rtSpeedLabel, rtPitchLabel;
    juce::Slider rtSpeedSlider, rtPitchSlider;
    juce::Label formantLabel, gateLabel, crushLabel, loopLabel, tiltLabel, ringLabel, shiftLabel, jitterLabel, mutationLabel;
    juce::Slider formantSlider, gateSlider, crushSlider, loopSlider, tiltSlider, ringSlider, shiftSlider, jitterSlider, mutationSlider;
    juce::Label nodePathLabel;
    juce::TextEditor nodePathEditor;
    juce::Label udpFeedLabel;
    juce::TextEditor udpFeedEditor;
    juce::String audioStatus;

    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    SpeakNSpellAudioSource speechSource;

    std::unique_ptr<UdpTextReceiver> udpReceiver;
    juce::CriticalSection udpStatusLock;
    juce::String udpStatus { "UDP: starting..." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
