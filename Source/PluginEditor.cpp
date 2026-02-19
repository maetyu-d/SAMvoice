#include "PluginEditor.h"

SAMVoiceSynthesizerAudioProcessorEditor::SAMVoiceSynthesizerAudioProcessorEditor (SAMVoiceSynthesizerAudioProcessor& p)
    : AudioProcessorEditor (&p), samProcessor (p)
{
    setSize (980, 620);

    titleLabel.setText ("SAM-STYLE VOICE SYNTHESIZER", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions ("Verdana", 30.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (47, 42, 73));
    addAndMakeVisible (titleLabel);

    statusLabel.setFont (juce::FontOptions ("Verdana", 13.0f, juce::Font::plain));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (73, 69, 103));
    addAndMakeVisible (statusLabel);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::bold));
    presetLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (52, 46, 82));
    addAndMakeVisible (presetLabel);

    for (int i = 0; i < SpeakNSpellVoice::getNumFactoryPresets(); ++i)
        presetBox.addItem (SpeakNSpellVoice::getFactoryPresetName (i), i + 1);
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour::fromRGB (255, 248, 231));
    presetBox.setColour (juce::ComboBox::textColourId, juce::Colour::fromRGB (40, 36, 60));
    presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colour::fromRGB (84, 78, 120));
    presetBox.setColour (juce::ComboBox::arrowColourId, juce::Colour::fromRGB (84, 78, 120));
    presetBox.onChange = [this]
    {
        applyPreset (presetBox.getSelectedItemIndex());
    };
    addAndMakeVisible (presetBox);

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, int mn, int mx, int dv)
    {
        l.setText (name.toUpperCase(), juce::dontSendNotification);
        l.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour::fromRGB (52, 46, 82));
        addAndMakeVisible (l);

        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        s.setRange (mn, mx, 1.0);
        s.setValue (dv, juce::dontSendNotification);
        s.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (255, 230, 178));
        s.setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (255, 122, 98));
        s.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (78, 201, 255));
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB (255, 248, 231));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colour::fromRGB (40, 36, 60));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB (84, 78, 120));
        addAndMakeVisible (s);
    };

    setupSlider (speedSlider, speedLabel, "Speed", 1, 255, 72);
    setupSlider (pitchSlider, pitchLabel, "Pitch", 0, 255, 64);
    setupSlider (mouthSlider, mouthLabel, "Mouth", 0, 255, 128);
    setupSlider (throatSlider, throatLabel, "Throat", 0, 255, 128);
    setupSlider (rtSpeedSlider, rtSpeedLabel, "Playback Speed", 25, 200, 100);
    setupSlider (rtPitchSlider, rtPitchLabel, "Repitch (st)", -24, 24, 0);
    setupSlider (formantSlider, formantLabel, "Formant Warp", 0, 100, 0);
    setupSlider (gateSlider, gateLabel, "Glitch Gate", 0, 100, 0);
    setupSlider (crushSlider, crushLabel, "Bit Crush", 0, 100, 0);
    setupSlider (loopSlider, loopLabel, "Micro Loop", 0, 100, 0);
    setupSlider (tiltSlider, tiltLabel, "Spectral Tilt", 0, 100, 50);
    setupSlider (ringSlider, ringLabel, "Ring Mod", 0, 100, 0);
    setupSlider (shiftSlider, shiftLabel, "Freq Shift", 0, 100, 0);
    setupSlider (jitterSlider, jitterLabel, "Repitch Jitter", 0, 100, 0);
    setupSlider (mutationSlider, mutationLabel, "Phoneme Mutation", 0, 100, 0);
    rtSpeedSlider.setTextValueSuffix (" %");

    auto updateRealtime = [this]
    {
        samProcessor.setRealtimeControls (gatherRealtimeControls());
    };
    rtSpeedSlider.onValueChange = updateRealtime;
    rtPitchSlider.onValueChange = updateRealtime;
    formantSlider.onValueChange = updateRealtime;
    gateSlider.onValueChange = updateRealtime;
    crushSlider.onValueChange = updateRealtime;
    loopSlider.onValueChange = updateRealtime;
    tiltSlider.onValueChange = updateRealtime;
    ringSlider.onValueChange = updateRealtime;
    shiftSlider.onValueChange = updateRealtime;
    jitterSlider.onValueChange = updateRealtime;
    mutationSlider.onValueChange = updateRealtime;

    auto styleToggle = [] (juce::ToggleButton& b)
    {
        b.setColour (juce::ToggleButton::textColourId, juce::Colour::fromRGB (24, 22, 38));
        b.setColour (juce::ToggleButton::tickColourId, juce::Colour::fromRGB (20, 20, 20));
        b.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB (65, 65, 65));
    };
    styleToggle (singModeButton);
    styleToggle (phoneticModeButton);
    styleToggle (backendModeButton);
    styleToggle (loopEndButton);
    addAndMakeVisible (singModeButton);
    addAndMakeVisible (phoneticModeButton);
    addAndMakeVisible (backendModeButton);
    addAndMakeVisible (loopEndButton);

    loopEndButton.onClick = [this]
    {
        samProcessor.setLoopAtEnd (loopEndButton.getToggleState());
    };

    nodePathLabel.setText ("NODE PATH", juce::dontSendNotification);
    nodePathLabel.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::bold));
    nodePathLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (52, 46, 82));
    addAndMakeVisible (nodePathLabel);

    nodePathEditor.setMultiLine (false);
    nodePathEditor.setReturnKeyStartsNewLine (false);
    nodePathEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (255, 248, 231));
    nodePathEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (40, 36, 60));
    nodePathEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (84, 78, 120));
    nodePathEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (78, 201, 255));
    nodePathEditor.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::plain));
    nodePathEditor.setTextToShowWhenEmpty ("Optional: C:\\Program Files\\nodejs\\node.exe", juce::Colour::fromRGB (140, 140, 140));
    nodePathEditor.onReturnKey = [this]
    {
        samProcessor.setNodePath (gatherNodePath());
    };
    nodePathEditor.onFocusLost = [this]
    {
        samProcessor.setNodePath (gatherNodePath());
    };
    addAndMakeVisible (nodePathEditor);

    textEditor.setMultiLine (true, false);
    textEditor.setReturnKeyStartsNewLine (false);
    textEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (255, 248, 231));
    textEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (47, 42, 73));
    textEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (84, 78, 120));
    textEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (78, 201, 255));
    textEditor.setFont (juce::FontOptions ("Verdana", 13.0f, juce::Font::plain));
    textEditor.setTextToShowWhenEmpty ("TYPE OR FIRE UDP:7001", juce::Colour::fromRGB (160, 160, 160));
    textEditor.onReturnKey = [this]
    {
        samProcessor.setParameters (gatherParams());
        samProcessor.setRealtimeControls (gatherRealtimeControls());
        samProcessor.setNodePath (gatherNodePath());
        samProcessor.setLoopAtEnd (loopEndButton.getToggleState());
        samProcessor.enqueueText (textEditor.getText());
    };
    addAndMakeVisible (textEditor);

    speakButton.addListener (this);
    speakButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (255, 122, 98));
    speakButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (236, 95, 69));
    speakButton.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (255, 248, 231));
    speakButton.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (255, 255, 255));
    speakButton.setButtonText ("SPEAK");
    addAndMakeVisible (speakButton);

    udpLabel.setText ("UDP INBOX", juce::dontSendNotification);
    udpLabel.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::bold));
    udpLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (52, 46, 82));
    addAndMakeVisible (udpLabel);

    udpEditor.setMultiLine (true, true);
    udpEditor.setReadOnly (true);
    udpEditor.setScrollbarsShown (true);
    udpEditor.setCaretVisible (false);
    udpEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (255, 248, 231));
    udpEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (47, 42, 73));
    udpEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (84, 78, 120));
    udpEditor.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::plain));
    addAndMakeVisible (udpEditor);

    applyParamsToUi (samProcessor.getParameters());
    applyRealtimeControlsToUi (samProcessor.getRealtimeControls());
    applyNodePathToUi (samProcessor.getNodePath());
    loopEndButton.setToggleState (samProcessor.getLoopAtEnd(), juce::dontSendNotification);
    presetBox.setSelectedItemIndex (samProcessor.getCurrentProgram(), juce::dontSendNotification);
    samProcessor.setNodePath (gatherNodePath());
    samProcessor.setRealtimeControls (gatherRealtimeControls());
    startTimerHz (12);
}

SAMVoiceSynthesizerAudioProcessorEditor::~SAMVoiceSynthesizerAudioProcessorEditor()
{
    speakButton.removeListener (this);
}

void SAMVoiceSynthesizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    constexpr int outerPad = 22;
    constexpr int headerBlock = 72;

    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bg (juce::Colour::fromRGB (255, 233, 158), 0.0f, 0.0f,
                             juce::Colour::fromRGB (170, 236, 255), bounds.getWidth(), bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    juce::Rectangle<float> frame = bounds.reduced (10.0f);
    g.setColour (juce::Colour::fromRGB (84, 78, 120));
    g.fillRoundedRectangle (frame, 18.0f);
    g.setColour (juce::Colour::fromRGB (255, 248, 231));
    g.fillRoundedRectangle (frame.reduced (4.0f), 15.0f);

    auto content = getLocalBounds().reduced (outerPad);
    content.removeFromTop (headerBlock);
    auto left = content.removeFromLeft (juce::roundToInt (content.getWidth() / 3.0f)).toFloat().reduced (4.0f);
    auto right = content.toFloat().reduced (4.0f);

    g.setColour (juce::Colour::fromRGB (255, 195, 113));
    g.fillRoundedRectangle (left, 14.0f);
    g.setColour (juce::Colour::fromRGB (173, 229, 255));
    g.fillRoundedRectangle (right, 14.0f);
    g.setColour (juce::Colour::fromRGB (84, 78, 120));
    g.drawRoundedRectangle (left, 14.0f, 2.0f);
    g.drawRoundedRectangle (right, 14.0f, 2.0f);

}

void SAMVoiceSynthesizerAudioProcessorEditor::resized()
{
    constexpr int outerPad = 22;
    constexpr int headerTitleH = 34;
    constexpr int headerTopH = 24;
    constexpr int rowH = 22;
    constexpr int rowGap = 4;
    constexpr int blockGap = 8;

    auto area = getLocalBounds().reduced (outerPad);
    titleLabel.setBounds (area.removeFromTop (headerTitleH));
    auto headerRow = area.removeFromTop (headerTopH);
    auto presetCell = headerRow.removeFromRight (280);
    presetLabel.setBounds (presetCell.removeFromLeft (70));
    presetBox.setBounds (presetCell);
    statusLabel.setBounds (headerRow);
    area.removeFromTop (6);

    auto leftPanel = area.removeFromLeft (juce::roundToInt (area.getWidth() / 3.0f)).reduced (10);
    auto rightPanel = area.reduced (10);

    auto leftContent = leftPanel.reduced (14);
    const auto textH = juce::jmax (90, juce::jmin (150, leftContent.getHeight() - 56));
    textEditor.setBounds (leftContent.removeFromTop (textH));
    leftContent.removeFromTop (8);
    speakButton.setBounds (leftContent.removeFromTop (34).removeFromRight (140));

    auto rightContent = rightPanel.reduced (14);
    auto placeRow = [&] (juce::Label& label, juce::Slider& slider, int labelW)
    {
        auto row = rightContent.removeFromTop (rowH);
        label.setBounds (row.removeFromLeft (labelW));
        slider.setBounds (row);
        rightContent.removeFromTop (rowGap);
    };

    placeRow (speedLabel, speedSlider, 90);
    placeRow (pitchLabel, pitchSlider, 90);
    placeRow (mouthLabel, mouthSlider, 90);
    placeRow (throatLabel, throatSlider, 90);
    placeRow (rtSpeedLabel, rtSpeedSlider, 130);
    placeRow (rtPitchLabel, rtPitchSlider, 130);

    auto fxArea = rightContent.removeFromTop ((rowH * 5) + (rowGap * 4));
    auto fxLeft = fxArea.removeFromLeft (fxArea.getWidth() / 2);
    fxArea.removeFromLeft (8);
    auto fxRight = fxArea;

    auto placeFx = [&] (juce::Rectangle<int>& col, juce::Label& label, juce::Slider& slider)
    {
        auto row = col.removeFromTop (rowH);
        label.setBounds (row.removeFromLeft (124));
        slider.setBounds (row);
        col.removeFromTop (rowGap);
    };

    placeFx (fxLeft, formantLabel, formantSlider);
    placeFx (fxLeft, crushLabel, crushSlider);
    placeFx (fxLeft, tiltLabel, tiltSlider);
    placeFx (fxLeft, shiftLabel, shiftSlider);
    placeFx (fxLeft, mutationLabel, mutationSlider);
    placeFx (fxRight, gateLabel, gateSlider);
    placeFx (fxRight, loopLabel, loopSlider);
    placeFx (fxRight, ringLabel, ringSlider);
    placeFx (fxRight, jitterLabel, jitterSlider);

    rightContent.removeFromTop (blockGap);
    auto toggleRow = rightContent.removeFromTop (24);
    auto tA = toggleRow.removeFromLeft (toggleRow.getWidth() / 4);
    auto tB = toggleRow.removeFromLeft (toggleRow.getWidth() / 3);
    auto tC = toggleRow.removeFromLeft (toggleRow.getWidth() / 2);
    singModeButton.setBounds (tA);
    phoneticModeButton.setBounds (tB);
    backendModeButton.setBounds (tC);
    loopEndButton.setBounds (toggleRow);

    rightContent.removeFromTop (blockGap);
    nodePathLabel.setBounds (rightContent.removeFromTop (22));
    rightContent.removeFromTop (4);
    nodePathEditor.setBounds (rightContent.removeFromTop (24));

    rightContent.removeFromTop (blockGap);
    udpLabel.setBounds (rightContent.removeFromTop (22));
    rightContent.removeFromTop (6);
    udpEditor.setBounds (rightContent);
}

void SAMVoiceSynthesizerAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button != &speakButton)
        return;

    samProcessor.setParameters (gatherParams());
    samProcessor.setRealtimeControls (gatherRealtimeControls());
    samProcessor.setNodePath (gatherNodePath());
    samProcessor.setLoopAtEnd (loopEndButton.getToggleState());
    samProcessor.enqueueText (textEditor.getText());
}

void SAMVoiceSynthesizerAudioProcessorEditor::timerCallback()
{
    presetBox.setSelectedItemIndex (samProcessor.getCurrentProgram(), juce::dontSendNotification);
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

SpeakNSpellVoice::RealtimeControls SAMVoiceSynthesizerAudioProcessorEditor::gatherRealtimeControls() const
{
    SpeakNSpellVoice::RealtimeControls c;
    c.playbackSpeed = static_cast<float> (rtSpeedSlider.getValue() / 100.0);
    c.repitchSemitones = static_cast<float> (rtPitchSlider.getValue());
    c.formantWarp = static_cast<float> (formantSlider.getValue() / 100.0);
    c.glitchGate = static_cast<float> (gateSlider.getValue() / 100.0);
    c.bitCrush = static_cast<float> (crushSlider.getValue() / 100.0);
    c.microLoop = static_cast<float> (loopSlider.getValue() / 100.0);
    c.spectralTilt = static_cast<float> (tiltSlider.getValue() / 100.0);
    c.ringMod = static_cast<float> (ringSlider.getValue() / 100.0);
    c.freqShift = static_cast<float> (shiftSlider.getValue() / 100.0);
    c.repitchJitter = static_cast<float> (jitterSlider.getValue() / 100.0);
    c.mutation = static_cast<float> (mutationSlider.getValue() / 100.0);
    return c;
}

juce::String SAMVoiceSynthesizerAudioProcessorEditor::gatherNodePath() const
{
    return nodePathEditor.getText().trim();
}

void SAMVoiceSynthesizerAudioProcessorEditor::applyPreset (int presetIndex)
{
    samProcessor.setCurrentProgram (presetIndex);
    applyParamsToUi (samProcessor.getParameters());
    applyRealtimeControlsToUi (samProcessor.getRealtimeControls());
    loopEndButton.setToggleState (samProcessor.getLoopAtEnd(), juce::dontSendNotification);
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

void SAMVoiceSynthesizerAudioProcessorEditor::applyRealtimeControlsToUi (const SpeakNSpellVoice::RealtimeControls& controls)
{
    rtSpeedSlider.setValue (controls.playbackSpeed * 100.0f, juce::dontSendNotification);
    rtPitchSlider.setValue (controls.repitchSemitones, juce::dontSendNotification);
    formantSlider.setValue (controls.formantWarp * 100.0f, juce::dontSendNotification);
    gateSlider.setValue (controls.glitchGate * 100.0f, juce::dontSendNotification);
    crushSlider.setValue (controls.bitCrush * 100.0f, juce::dontSendNotification);
    loopSlider.setValue (controls.microLoop * 100.0f, juce::dontSendNotification);
    tiltSlider.setValue (controls.spectralTilt * 100.0f, juce::dontSendNotification);
    ringSlider.setValue (controls.ringMod * 100.0f, juce::dontSendNotification);
    shiftSlider.setValue (controls.freqShift * 100.0f, juce::dontSendNotification);
    jitterSlider.setValue (controls.repitchJitter * 100.0f, juce::dontSendNotification);
    mutationSlider.setValue (controls.mutation * 100.0f, juce::dontSendNotification);
}

void SAMVoiceSynthesizerAudioProcessorEditor::applyNodePathToUi (const juce::String& path)
{
    nodePathEditor.setText (path, juce::dontSendNotification);
}
