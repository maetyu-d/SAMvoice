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
    titleLabel.setFont (juce::FontOptions ("Menlo", 34.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (245, 245, 245));
    addAndMakeVisible (titleLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::FontOptions ("Menlo", 14.0f, juce::Font::plain));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (255, 82, 82));
    statusLabel.setText ("Status: Initializing...", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, double min, double max, double value)
    {
        l.setText (name.toUpperCase(), juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (juce::FontOptions ("Menlo", 14.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour::fromRGB (236, 236, 236));
        addAndMakeVisible (l);

        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        s.setRange (min, max, 1.0);
        s.setValue (value, juce::dontSendNotification);
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

    singModeButton.setToggleState (false, juce::dontSendNotification);
    styleToggle (singModeButton);
    addAndMakeVisible (singModeButton);

    phoneticModeButton.setToggleState (false, juce::dontSendNotification);
    styleToggle (phoneticModeButton);
    addAndMakeVisible (phoneticModeButton);

    backendModeButton.setToggleState (false, juce::dontSendNotification);
    styleToggle (backendModeButton);
    addAndMakeVisible (backendModeButton);

    udpFeedLabel.setText ("UDP INBOX", juce::dontSendNotification);
    udpFeedLabel.setJustificationType (juce::Justification::centredLeft);
    udpFeedLabel.setFont (juce::FontOptions ("Menlo", 14.0f, juce::Font::bold));
    udpFeedLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (236, 236, 236));
    addAndMakeVisible (udpFeedLabel);

    udpFeedEditor.setMultiLine (true, true);
    udpFeedEditor.setReadOnly (true);
    udpFeedEditor.setScrollbarsShown (true);
    udpFeedEditor.setCaretVisible (false);
    udpFeedEditor.setPopupMenuEnabled (true);
    udpFeedEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0, 0, 0));
    udpFeedEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (239, 239, 239));
    udpFeedEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (255, 255, 255));
    udpFeedEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (255, 255, 255));
    udpFeedEditor.setFont (juce::FontOptions ("Menlo", 13.0f, juce::Font::plain));
    udpFeedEditor.setText ("[waiting for UDP on port 7001]");
    addAndMakeVisible (udpFeedEditor);

    textEditor.setMultiLine (true, false);
    textEditor.setReturnKeyStartsNewLine (false);
    textEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0, 0, 0));
    textEditor.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (255, 255, 255));
    textEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (255, 255, 255));
    textEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (255, 48, 48));
    textEditor.setColour (juce::TextEditor::highlightColourId, juce::Colour::fromRGB (255, 48, 48));
    textEditor.setColour (juce::TextEditor::highlightedTextColourId, juce::Colour::fromRGB (255, 255, 255));
    textEditor.setFont (juce::FontOptions ("Menlo", 20.0f, juce::Font::plain));
    textEditor.setTextToShowWhenEmpty ("TYPE OR FIRE UDP:7001", juce::Colour::fromRGB (160, 160, 160));
    textEditor.setText ("Hello! This is a SAM-style voice synthesizer.");
    textEditor.onReturnKey = [this]
    {
        speakText (textEditor.getText());
    };
    addAndMakeVisible (textEditor);

    speakButton.addListener (this);
    speakButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (255, 48, 48));
    speakButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (200, 24, 24));
    speakButton.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (10, 10, 10));
    speakButton.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (255, 255, 255));
    speakButton.setButtonText ("SPEAK");
    speakButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    addAndMakeVisible (speakButton);

    sourcePlayer.setSource (&speechSource);
    const auto initError = deviceManager.initialise (0, 2, nullptr, true);
    audioStatus = initError.isEmpty() ? "Audio OK" : ("Audio init failed: " + initError);
    deviceManager.addAudioCallback (&sourcePlayer);

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
    constexpr int headerBlock = 88;
    constexpr int panelGap = 8;
    constexpr int borderWidth = 3;

    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour::fromRGB (8, 8, 8));

    juce::Rectangle<float> frame = bounds.reduced (12.0f);
    g.setColour (juce::Colour::fromRGB (242, 242, 242));
    g.drawRect (frame, 4.0f);

    auto content = getLocalBounds().reduced (outerPad);
    content.removeFromTop (headerBlock);
    auto left = content.removeFromLeft (juce::roundToInt (content.getWidth() * 0.58f));
    auto right = content;
    left.reduce (4, 4);
    right.reduce (4, 4);
    g.setColour (juce::Colour::fromRGB (0, 0, 0));
    g.fillRect (left);
    g.fillRect (right);
    g.setColour (juce::Colour::fromRGB (242, 242, 242));
    g.drawRect (left, borderWidth);
    g.drawRect (right, borderWidth);

    auto sepX = static_cast<float> (left.getRight() + panelGap);
    g.setColour (juce::Colour::fromRGB (255, 48, 48));
    g.drawLine (sepX, static_cast<float> (left.getY()), sepX, static_cast<float> (left.getBottom()), 3.0f);
}

void MainComponent::resized()
{
    constexpr int outerPad = 22;
    constexpr int headerTitleH = 44;
    constexpr int headerStatusH = 28;
    constexpr int panelInset = 10;
    constexpr int panelInner = 14;
    constexpr int rowH = 30;
    constexpr int rowGap = 10;
    constexpr int blockGap = 16;

    auto area = getLocalBounds().reduced (outerPad);
    titleLabel.setBounds (area.removeFromTop (headerTitleH));
    statusLabel.setBounds (area.removeFromTop (headerStatusH));
    area.removeFromTop (12);

    auto leftPanel = area.removeFromLeft (juce::roundToInt (area.getWidth() * 0.58f)).reduced (panelInset);
    auto rightPanel = area.reduced (panelInset);

    auto leftContent = leftPanel.reduced (panelInner);
    auto speakRow = leftContent.removeFromBottom (52);
    textEditor.setBounds (leftContent);
    auto speakBounds = speakRow.removeFromRight (220).reduced (0, 4);
    speakButton.setBounds (speakBounds);

    auto rightContent = rightPanel.reduced (panelInner);
    auto rowA = rightContent.removeFromTop (rowH);
    speedLabel.setBounds (rowA.removeFromLeft (90));
    speedSlider.setBounds (rowA);

    rightContent.removeFromTop (rowGap);
    auto rowB = rightContent.removeFromTop (rowH);
    pitchLabel.setBounds (rowB.removeFromLeft (90));
    pitchSlider.setBounds (rowB);

    rightContent.removeFromTop (rowGap);
    auto rowC = rightContent.removeFromTop (rowH);
    mouthLabel.setBounds (rowC.removeFromLeft (90));
    mouthSlider.setBounds (rowC);

    rightContent.removeFromTop (rowGap);
    auto rowD = rightContent.removeFromTop (rowH);
    throatLabel.setBounds (rowD.removeFromLeft (90));
    throatSlider.setBounds (rowD);

    rightContent.removeFromTop (blockGap);
    singModeButton.setBounds (rightContent.removeFromTop (26));
    rightContent.removeFromTop (8);
    phoneticModeButton.setBounds (rightContent.removeFromTop (26));
    rightContent.removeFromTop (8);
    backendModeButton.setBounds (rightContent.removeFromTop (26));

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
