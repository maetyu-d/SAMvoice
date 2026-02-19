#include "MainComponent.h"

class MainComponent::UdpTextReceiver final : private juce::Thread
{
public:
    explicit UdpTextReceiver (std::function<void (juce::String)> onTextIn,
                              std::function<void (juce::String)> onStatusIn)
        : juce::Thread ("UdpTextReceiver"),
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

        port = portNumber;
        onStatus ("UDP: listening on port " + juce::String (port));
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
    int port = 0;
};

MainComponent::MainComponent()
{
    setSize (980, 620);

    titleLabel.setText ("SAM-STYLE VOICE SYNTHESIZER", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions ("Verdana", 30.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (47, 42, 73));
    addAndMakeVisible (titleLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::FontOptions ("Verdana", 13.0f, juce::Font::plain));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (73, 69, 103));
    statusLabel.setText ("Status: Initializing...", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
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

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, double min, double max, double value)
    {
        l.setText (name.toUpperCase(), juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour::fromRGB (52, 46, 82));
        addAndMakeVisible (l);

        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        s.setRange (min, max, 1.0);
        s.setValue (value, juce::dontSendNotification);
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
        speechSource.setRealtimeControls (getRealtimeControls());
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

    singModeButton.setToggleState (false, juce::dontSendNotification);
    styleToggle (singModeButton);
    addAndMakeVisible (singModeButton);

    phoneticModeButton.setToggleState (false, juce::dontSendNotification);
    styleToggle (phoneticModeButton);
    addAndMakeVisible (phoneticModeButton);

    backendModeButton.setToggleState (false, juce::dontSendNotification);
    styleToggle (backendModeButton);
    addAndMakeVisible (backendModeButton);

    loopEndButton.setToggleState (false, juce::dontSendNotification);
    styleToggle (loopEndButton);
    loopEndButton.onClick = [this]
    {
        speechSource.setLoopAtEnd (loopEndButton.getToggleState());
    };
    addAndMakeVisible (loopEndButton);

    nodePathLabel.setText ("NODE PATH", juce::dontSendNotification);
    nodePathLabel.setJustificationType (juce::Justification::centredLeft);
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
    nodePathEditor.setText (juce::SystemStats::getEnvironmentVariable ("SAM_NODE_PATH", {}), juce::dontSendNotification);
    nodePathEditor.onReturnKey = [this]
    {
        speechSource.setCustomNodePath (getNodePathInput());
    };
    nodePathEditor.onFocusLost = [this]
    {
        speechSource.setCustomNodePath (getNodePathInput());
    };
    addAndMakeVisible (nodePathEditor);

    udpFeedLabel.setText ("UDP INBOX", juce::dontSendNotification);
    udpFeedLabel.setJustificationType (juce::Justification::centredLeft);
    udpFeedLabel.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::bold));
    udpFeedLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (52, 46, 82));
    addAndMakeVisible (udpFeedLabel);

    udpFeedEditor.setMultiLine (true, true);
    udpFeedEditor.setReadOnly (true);
    udpFeedEditor.setScrollbarsShown (true);
    udpFeedEditor.setCaretVisible (false);
    udpFeedEditor.setPopupMenuEnabled (true);
    udpFeedEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (255, 248, 231));
    udpFeedEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (47, 42, 73));
    udpFeedEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (84, 78, 120));
    udpFeedEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (84, 78, 120));
    udpFeedEditor.setFont (juce::FontOptions ("Verdana", 12.0f, juce::Font::plain));
    udpFeedEditor.setText ("[waiting for UDP on port 7001]");
    addAndMakeVisible (udpFeedEditor);

    textEditor.setMultiLine (true, false);
    textEditor.setReturnKeyStartsNewLine (false);
    textEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (255, 248, 231));
    textEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (47, 42, 73));
    textEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (84, 78, 120));
    textEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (78, 201, 255));
    textEditor.setColour (juce::TextEditor::highlightColourId, juce::Colour::fromRGB (173, 229, 255));
    textEditor.setColour (juce::TextEditor::highlightedTextColourId, juce::Colour::fromRGB (255, 255, 255));
    textEditor.setFont (juce::FontOptions ("Verdana", 13.0f, juce::Font::plain));
    textEditor.setTextToShowWhenEmpty ("TYPE OR FIRE UDP:7001", juce::Colour::fromRGB (160, 160, 160));
    textEditor.setText ("Hello! This is a SAM-style voice synthesizer.");
    textEditor.onReturnKey = [this]
    {
        speakText (textEditor.getText());
    };
    addAndMakeVisible (textEditor);

    speakButton.addListener (this);
    speakButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (255, 122, 98));
    speakButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (236, 95, 69));
    speakButton.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (255, 248, 231));
    speakButton.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (255, 255, 255));
    speakButton.setButtonText ("SPEAK");
    speakButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    addAndMakeVisible (speakButton);

    sourcePlayer.setSource (&speechSource);
    const auto initError = deviceManager.initialise (0, 2, nullptr, true);
    audioStatus = initError.isEmpty() ? "Audio OK" : ("Audio init failed: " + initError);
    deviceManager.addAudioCallback (&sourcePlayer);
    speechSource.setRealtimeControls (getRealtimeControls());
    speechSource.setCustomNodePath (getNodePathInput());
    speechSource.setLoopAtEnd (loopEndButton.getToggleState());

    udpReceiver = std::make_unique<UdpTextReceiver> (
        [safe = juce::Component::SafePointer<MainComponent> (this)] (juce::String text)
        {
            juce::MessageManager::callAsync ([safe, text]
            {
                if (safe == nullptr)
                    return;
                safe->textEditor.setText (text, juce::dontSendNotification);
                safe->appendUdpLine (text);
                safe->speakText (text);
            });
        },
        [safe = juce::Component::SafePointer<MainComponent> (this)] (juce::String status)
        {
            juce::MessageManager::callAsync ([safe, status]
            {
                if (safe == nullptr)
                    return;
                const juce::ScopedLock sl (safe->udpStatusLock);
                safe->udpStatus = status;
            });
        });
    udpReceiver->start (7001);

    presetBox.setSelectedItemIndex (0, juce::sendNotificationSync);
    startTimerHz (10);
}

MainComponent::~MainComponent()
{
    udpReceiver.reset();
    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
    speakButton.removeListener (this);
}

void MainComponent::paint (juce::Graphics& g)
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
    auto left = content.removeFromLeft (juce::roundToInt (content.getWidth() * 0.30f)).toFloat().reduced (4.0f);
    auto right = content.toFloat().reduced (4.0f);

    g.setColour (juce::Colour::fromRGB (255, 195, 113));
    g.fillRoundedRectangle (left, 14.0f);
    g.setColour (juce::Colour::fromRGB (173, 229, 255));
    g.fillRoundedRectangle (right, 14.0f);

    g.setColour (juce::Colour::fromRGB (84, 78, 120));
    g.drawRoundedRectangle (left, 14.0f, 2.0f);
    g.drawRoundedRectangle (right, 14.0f, 2.0f);

    const auto y = 18.0f;
    g.setColour (juce::Colour::fromRGB (255, 122, 98));
    g.fillEllipse (26.0f, y, 14.0f, 14.0f);
    g.setColour (juce::Colour::fromRGB (78, 201, 255));
    g.fillEllipse (48.0f, y, 14.0f, 14.0f);
    g.setColour (juce::Colour::fromRGB (255, 214, 102));
    g.fillEllipse (70.0f, y, 14.0f, 14.0f);
}

void MainComponent::resized()
{
    constexpr int outerPad = 22;
    constexpr int headerTitleH = 34;
    constexpr int headerTopH = 24;
    constexpr int panelInset = 10;
    constexpr int panelInner = 14;
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

    auto leftPanel = area.removeFromLeft (juce::roundToInt (area.getWidth() * 0.30f)).reduced (panelInset);
    auto rightPanel = area.reduced (panelInset);

    auto leftContent = leftPanel.reduced (panelInner);
    const auto textH = juce::jmax (90, juce::jmin (150, leftContent.getHeight() - 56));
    textEditor.setBounds (leftContent.removeFromTop (textH));
    leftContent.removeFromTop (8);
    speakButton.setBounds (leftContent.removeFromTop (34).removeFromRight (140));

    auto rightContent = rightPanel.reduced (panelInner);
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
    auto fxLeft = fxArea.removeFromLeft (fxArea.getWidth() / 2).reduced (0, 0);
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
    udpFeedLabel.setBounds (rightContent.removeFromTop (22));
    rightContent.removeFromTop (6);
    udpFeedEditor.setBounds (rightContent);
}

void MainComponent::buttonClicked (juce::Button* button)
{
    if (button != &speakButton)
        return;

    const auto text = textEditor.getText();
    speakText (text);
}

void MainComponent::timerCallback()
{
    juce::String udp;
    {
        const juce::ScopedLock sl (udpStatusLock);
        udp = udpStatus;
    }
    statusLabel.setText ("Status: " + audioStatus + " | " + udp + " | " + speechSource.getStatusText(), juce::dontSendNotification);
}

void MainComponent::speakText (const juce::String& text)
{
    if (text.trim().isEmpty())
        return;

    speechSource.setCustomNodePath (getNodePathInput());
    speechSource.setRealtimeControls (getRealtimeControls());
    speechSource.setLoopAtEnd (loopEndButton.getToggleState());
    speechSource.queueText (text, getCurrentParameters());
}

SpeakNSpellVoice::Parameters MainComponent::getCurrentParameters() const
{
    return {
        static_cast<int> (speedSlider.getValue()),
        static_cast<int> (pitchSlider.getValue()),
        static_cast<int> (mouthSlider.getValue()),
        static_cast<int> (throatSlider.getValue()),
        singModeButton.getToggleState(),
        phoneticModeButton.getToggleState(),
        backendModeButton.getToggleState()
            ? SpeakNSpellVoice::Parameters::Backend::betterSam
            : SpeakNSpellVoice::Parameters::Backend::classicSam
    };
}

SpeakNSpellVoice::RealtimeControls MainComponent::getRealtimeControls() const
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

void MainComponent::applyParametersToUi (const SpeakNSpellVoice::Parameters& p)
{
    speedSlider.setValue (p.speed, juce::dontSendNotification);
    pitchSlider.setValue (p.pitch, juce::dontSendNotification);
    mouthSlider.setValue (p.mouth, juce::dontSendNotification);
    throatSlider.setValue (p.throat, juce::dontSendNotification);
    singModeButton.setToggleState (p.singMode, juce::dontSendNotification);
    phoneticModeButton.setToggleState (p.phoneticInput, juce::dontSendNotification);
    backendModeButton.setToggleState (p.backend == SpeakNSpellVoice::Parameters::Backend::betterSam, juce::dontSendNotification);
}

void MainComponent::applyRealtimeControlsToUi (const SpeakNSpellVoice::RealtimeControls& c)
{
    rtSpeedSlider.setValue (c.playbackSpeed * 100.0f, juce::dontSendNotification);
    rtPitchSlider.setValue (c.repitchSemitones, juce::dontSendNotification);
    formantSlider.setValue (c.formantWarp * 100.0f, juce::dontSendNotification);
    gateSlider.setValue (c.glitchGate * 100.0f, juce::dontSendNotification);
    crushSlider.setValue (c.bitCrush * 100.0f, juce::dontSendNotification);
    loopSlider.setValue (c.microLoop * 100.0f, juce::dontSendNotification);
    tiltSlider.setValue (c.spectralTilt * 100.0f, juce::dontSendNotification);
    ringSlider.setValue (c.ringMod * 100.0f, juce::dontSendNotification);
    shiftSlider.setValue (c.freqShift * 100.0f, juce::dontSendNotification);
    jitterSlider.setValue (c.repitchJitter * 100.0f, juce::dontSendNotification);
    mutationSlider.setValue (c.mutation * 100.0f, juce::dontSendNotification);
}

void MainComponent::applyPreset (int presetIndex)
{
    SpeakNSpellVoice::Parameters p;
    SpeakNSpellVoice::RealtimeControls c;
    SpeakNSpellVoice::applyFactoryPreset (presetIndex, p, c);
    applyParametersToUi (p);
    applyRealtimeControlsToUi (c);
    speechSource.setRealtimeControls (c);
}

juce::String MainComponent::getNodePathInput() const
{
    return nodePathEditor.getText().trim();
}

void MainComponent::appendUdpLine (const juce::String& text)
{
    auto current = udpFeedEditor.getText();
    if (current.contains ("[waiting for UDP on port 7001]"))
        current.clear();

    current << text << "\n";

    constexpr int maxChars = 12000;
    if (current.length() > maxChars)
        current = current.substring (current.length() - maxChars);

    udpFeedEditor.setText (current, juce::dontSendNotification);
    udpFeedEditor.moveCaretToEnd();
}
