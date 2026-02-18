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
    void applyParamsToUi (const SpeakNSpellVoice::Parameters& p);

    SAMVoiceSynthesizerAudioProcessor& samProcessor;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::TextEditor textEditor;
    juce::TextButton speakButton { "Speak" };

    juce::Label speedLabel, pitchLabel, mouthLabel, throatLabel;
    juce::Slider speedSlider, pitchSlider, mouthSlider, throatSlider;
    juce::ToggleButton singModeButton { "Sing Mode" };
    juce::ToggleButton phoneticModeButton { "Phonetic Input" };
    juce::ToggleButton backendModeButton { "Better SAM mode" };

    juce::Label udpLabel;
    juce::TextEditor udpEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SAMVoiceSynthesizerAudioProcessorEditor)
};
