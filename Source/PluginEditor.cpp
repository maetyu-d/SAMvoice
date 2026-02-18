#include "PluginEditor.h"

SAMVoiceSynthesizerAudioProcessorEditor::SAMVoiceSynthesizerAudioProcessorEditor (SAMVoiceSynthesizerAudioProcessor& p)
    : AudioProcessorEditor (&p), samProcessor (p)
{
    setSize (980, 620);

    titleLabel.setText ("SAM-STYLE VOICE SYNTHESIZER", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions ("Menlo", 34.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (245, 245, 245));
    addAndMakeVisible (titleLabel);

    statusLabel.setFont (juce::FontOptions ("Menlo", 14.0f, juce::Font::plain));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (255, 82, 82));
    addAndMakeVisible (statusLabel);

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, int mn, int mx, int dv)
    {
        l.setText (name.toUpperCase(), juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Menlo", 14.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour::fromRGB (236, 236, 236));
        addAndMakeVisible (l);

        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        s.setRange (mn, mx, 1.0);
        s.setValue (dv, juce::dontSendNotification);
        s.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (18, 18, 18));
        s.setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (255, 255, 255));
        s.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (255, 48, 48));
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB (0, 0, 0));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colour::fromRGB (255, 255, 255));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB (255, 255, 255));
        addAndMakeVisible (s);
    };

    setupSlider (speedSlider, speedLabel, "Speed", 1, 255, 72);
    setupSlider (pitchSlider, pitchLabel, "Pitch", 0, 255, 64);
    setupSlider (mouthSlider, mouthLabel, "Mouth", 0, 255, 128);
    setupSlider (throatSlider, throatLabel, "Throat", 0, 255, 128);

    auto styleToggle = [] (juce::ToggleButton& b)
    {
        b.setColour (juce::ToggleButton::textColourId, juce::Colour::fromRGB (240, 240, 240));
        b.setColour (juce::ToggleButton::tickColourId, juce::Colour::fromRGB (255, 48, 48));
    };
    styleToggle (singModeButton);
    styleToggle (phoneticModeButton);
    styleToggle (backendModeButton);
    addAndMakeVisible (singModeButton);
    addAndMakeVisible (phoneticModeButton);
    addAndMakeVisible (backendModeButton);

    textEditor.setMultiLine (true, false);
    textEditor.setReturnKeyStartsNewLine (false);
    textEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0, 0, 0));
    textEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (255, 255, 255));
    textEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (255, 255, 255));
    textEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (255, 48, 48));
    textEditor.setFont (juce::FontOptions ("Menlo", 20.0f, juce::Font::plain));
    textEditor.setTextToShowWhenEmpty ("TYPE OR FIRE UDP:7001", juce::Colour::fromRGB (160, 160, 160));
    textEditor.onReturnKey = [this]
    {
        samProcessor.setParameters (gatherParams());
        samProcessor.enqueueText (textEditor.getText());
    };
    addAndMakeVisible (textEditor);

    speakButton.addListener (this);
    speakButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (255, 48, 48));
    speakButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (200, 24, 24));
    speakButton.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (10, 10, 10));
    speakButton.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (255, 255, 255));
    speakButton.setButtonText ("SPEAK");
    addAndMakeVisible (speakButton);

    udpLabel.setText ("UDP INBOX", juce::dontSendNotification);
    udpLabel.setFont (juce::FontOptions ("Menlo", 14.0f, juce::Font::bold));
    udpLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (236, 236, 236));
    addAndMakeVisible (udpLabel);

    udpEditor.setMultiLine (true, true);
    udpEditor.setReadOnly (true);
    udpEditor.setScrollbarsShown (true);
    udpEditor.setCaretVisible (false);
    udpEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0, 0, 0));
    udpEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (239, 239, 239));
    udpEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (255, 255, 255));
    udpEditor.setFont (juce::FontOptions ("Menlo", 13.0f, juce::Font::plain));
    addAndMakeVisible (udpEditor);

    applyParamsToUi (samProcessor.getParameters());
    startTimerHz (12);
}

SAMVoiceSynthesizerAudioProcessorEditor::~SAMVoiceSynthesizerAudioProcessorEditor()
{
    speakButton.removeListener (this);
}

void SAMVoiceSynthesizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (8, 8, 8));
    auto frame = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (juce::Colour::fromRGB (242, 242, 242));
    g.drawRect (frame, 4.0f);

    auto content = getLocalBounds().reduced (22);
    content.removeFromTop (88);
    auto left = content.removeFromLeft (juce::roundToInt (content.getWidth() * 0.58f));
    auto right = content;
    left.reduce (4, 4);
    right.reduce (4, 4);

    g.setColour (juce::Colour::fromRGB (0, 0, 0));
    g.fillRect (left);
    g.fillRect (right);
    g.setColour (juce::Colour::fromRGB (242, 242, 242));
    g.drawRect (left, 3);
    g.drawRect (right, 3);
    g.setColour (juce::Colour::fromRGB (255, 48, 48));
    g.drawLine (static_cast<float> (left.getRight() + 8), static_cast<float> (left.getY()),
                static_cast<float> (left.getRight() + 8), static_cast<float> (left.getBottom()), 3.0f);
}

void SAMVoiceSynthesizerAudioProcessorEditor::resized()
{
    constexpr int outerPad = 22;
    constexpr int headerTitleH = 44;
    constexpr int headerStatusH = 28;

    auto area = getLocalBounds().reduced (outerPad);
    titleLabel.setBounds (area.removeFromTop (headerTitleH));
    statusLabel.setBounds (area.removeFromTop (headerStatusH));
    area.removeFromTop (12);

    auto leftPanel = area.removeFromLeft (juce::roundToInt (area.getWidth() * 0.58f)).reduced (10);
    auto rightPanel = area.reduced (10);

    auto leftContent = leftPanel.reduced (14);
    auto speakRow = leftContent.removeFromBottom (52);
    textEditor.setBounds (leftContent);
    speakButton.setBounds (speakRow.removeFromRight (220).reduced (0, 4));

    auto rightContent = rightPanel.reduced (14);
    auto rowA = rightContent.removeFromTop (30);
    speedLabel.setBounds (rowA.removeFromLeft (90));
    speedSlider.setBounds (rowA);
    rightContent.removeFromTop (10);

    auto rowB = rightContent.removeFromTop (30);
    pitchLabel.setBounds (rowB.removeFromLeft (90));
    pitchSlider.setBounds (rowB);
    rightContent.removeFromTop (10);

    auto rowC = rightContent.removeFromTop (30);
    mouthLabel.setBounds (rowC.removeFromLeft (90));
    mouthSlider.setBounds (rowC);
    rightContent.removeFromTop (10);

    auto rowD = rightContent.removeFromTop (30);
    throatLabel.setBounds (rowD.removeFromLeft (90));
    throatSlider.setBounds (rowD);

    rightContent.removeFromTop (16);
    singModeButton.setBounds (rightContent.removeFromTop (26));
    rightContent.removeFromTop (8);
    phoneticModeButton.setBounds (rightContent.removeFromTop (26));
    rightContent.removeFromTop (8);
    backendModeButton.setBounds (rightContent.removeFromTop (26));

    rightContent.removeFromTop (16);
    udpLabel.setBounds (rightContent.removeFromTop (22));
    rightContent.removeFromTop (6);
    udpEditor.setBounds (rightContent);
}

void SAMVoiceSynthesizerAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button != &speakButton)
        return;

    samProcessor.setParameters (gatherParams());
    samProcessor.enqueueText (textEditor.getText());
}

void SAMVoiceSynthesizerAudioProcessorEditor::timerCallback()
{
    statusLabel.setText (
        "Status: " + samProcessor.getUdpStatus() + " | " + samProcessor.getVoiceStatus(),
        juce::dontSendNotification);

    udpEditor.setText (samProcessor.getUdpFeed(), juce::dontSendNotification);
}

SpeakNSpellVoice::Parameters SAMVoiceSynthesizerAudioProcessorEditor::gatherParams() const
{
    SpeakNSpellVoice::Parameters p;
    p.speed = static_cast<int> (speedSlider.getValue());
    p.pitch = static_cast<int> (pitchSlider.getValue());
    p.mouth = static_cast<int> (mouthSlider.getValue());
    p.throat = static_cast<int> (throatSlider.getValue());
    p.singMode = singModeButton.getToggleState();
    p.phoneticInput = phoneticModeButton.getToggleState();
    p.backend = backendModeButton.getToggleState()
        ? SpeakNSpellVoice::Parameters::Backend::betterSam
        : SpeakNSpellVoice::Parameters::Backend::classicSam;
    return p;
}

void SAMVoiceSynthesizerAudioProcessorEditor::applyParamsToUi (const SpeakNSpellVoice::Parameters& p)
{
    speedSlider.setValue (p.speed, juce::dontSendNotification);
    pitchSlider.setValue (p.pitch, juce::dontSendNotification);
    mouthSlider.setValue (p.mouth, juce::dontSendNotification);
    throatSlider.setValue (p.throat, juce::dontSendNotification);
    singModeButton.setToggleState (p.singMode, juce::dontSendNotification);
    phoneticModeButton.setToggleState (p.phoneticInput, juce::dontSendNotification);
    backendModeButton.setToggleState (p.backend == SpeakNSpellVoice::Parameters::Backend::betterSam, juce::dontSendNotification);
}
