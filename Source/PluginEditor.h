#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SAMVoiceSynthesizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                      private juce::Button::Listener,
                                                      private juce::Timer
{
public:
    explicit SAMVoiceSynthesizerAudioProcessorEditor (SAMVoiceSynthesizerAudioProcessor&);
    ~SAMVoiceSynthesizerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked (juce::Button* button) override;
    void timerCallback() override;

    SpeakNSpellVoice::Parameters gatherParams() const;
    SpeakNSpellVoice::RealtimeControls gatherRealtimeControls() const;
    juce::String gatherNodePath() const;
    void applyPreset (int presetIndex);
    void applyParamsToUi (const SpeakNSpellVoice::Parameters& p);
    void applyRealtimeControlsToUi (const SpeakNSpellVoice::RealtimeControls& controls);
    void applyNodePathToUi (const juce::String& path);

    SAMVoiceSynthesizerAudioProcessor& samProcessor;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextEditor textEditor;
    juce::TextButton speakButton { "Speak" };

    juce::Label speedLabel, pitchLabel, mouthLabel, throatLabel;
    juce::Slider speedSlider, pitchSlider, mouthSlider, throatSlider;
    juce::Label rtSpeedLabel, rtPitchLabel;
    juce::Slider rtSpeedSlider, rtPitchSlider;
    juce::Label formantLabel, gateLabel, crushLabel, loopLabel, tiltLabel, ringLabel, shiftLabel, jitterLabel, mutationLabel;
    juce::Slider formantSlider, gateSlider, crushSlider, loopSlider, tiltSlider, ringSlider, shiftSlider, jitterSlider, mutationSlider;
    juce::ToggleButton singModeButton { "Sing Mode" };
    juce::ToggleButton phoneticModeButton { "Phonetic Input" };
    juce::ToggleButton backendModeButton { "Better SAM mode" };
    juce::ToggleButton loopEndButton { "Loop At End" };
    juce::Label nodePathLabel;
    juce::TextEditor nodePathEditor;

    juce::Label udpLabel;
    juce::TextEditor udpEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SAMVoiceSynthesizerAudioProcessorEditor)
};
