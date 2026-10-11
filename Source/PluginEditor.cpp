#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

OriginalWaveformDisplay::OriginalWaveformDisplay()
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener (this);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

OriginalWaveformDisplay::~OriginalWaveformDisplay()
{
    thumbnail.removeChangeListener (this);
}

void OriginalWaveformDisplay::setFile (const juce::File& file, const double timelineLengthSeconds)
{
    timelineLength = timelineLengthSeconds;
    thumbnail.setSource (new juce::FileInputSource (file));
}

void OriginalWaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (14.0f);
    g.drawText ("Original", bounds.removeFromTop (20.0f), juce::Justification::centredLeft, false);

    if (thumbnail.getTotalLength() > 0.0)
    {
        if (timelineLength > thumbnail.getTotalLength())
            bounds.setWidth (bounds.getWidth()
                             * static_cast<float> (thumbnail.getTotalLength() / timelineLength));

        g.setColour (getLookAndFeel().findColour (juce::Slider::trackColourId));
        thumbnail.drawChannels (g, bounds.toNearestInt(), 0.0, thumbnail.getTotalLength(), 1.0f);
    }
}

void OriginalWaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

void OriginalWaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void ProcessedWaveformDisplay::pushSamples (const float* samples, const int numSamples)
{
    const auto samplesToCopy = juce::jmin (numSamples, historySize - validSamples);
    std::copy_n (samples, samplesToCopy, history.begin() + validSamples);
    validSamples += samplesToCopy;

    repaint();
}

void ProcessedWaveformDisplay::clear()
{
    history.fill (0.0f);
    validSamples = 0;
    repaint();
}

void ProcessedWaveformDisplay::setExpectedSamples (const int newExpectedSamples)
{
    expectedSamples = juce::jlimit (2, historySize, newExpectedSamples);
}

void ProcessedWaveformDisplay::setPointerPositions (const float newReadPosition, const float newWritePosition)
{
    readPosition = juce::jlimit (0.0f, 1.0f, newReadPosition);
    writePosition = juce::jlimit (0.0f, 1.0f, newWritePosition);
    repaint();
}

void ProcessedWaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (14.0f);
    g.drawText ("Processed", bounds.removeFromTop (20.0f), juce::Justification::centredLeft, false);

    if (validSamples < 2 || bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f)
        return;

    juce::Path waveform;
    const auto centreY = bounds.getCentreY();
    const auto amplitude = bounds.getHeight() * 0.48f;

    for (int i = 0; i < validSamples; ++i)
    {
        const auto x = juce::jmap (static_cast<float> (i),
                                  0.0f,
                                  static_cast<float> (expectedSamples - 1),
                                  bounds.getX(),
                                  bounds.getRight());
        const auto y = centreY - juce::jlimit (-1.0f, 1.0f, history[static_cast<size_t> (i)]) * amplitude;

        if (i == 0)
            waveform.startNewSubPath (x, y);
        else
            waveform.lineTo (x, y);
    }

    g.setColour (getLookAndFeel().findColour (juce::Slider::trackColourId));
    g.strokePath (waveform, juce::PathStrokeType (1.25f));

    const auto writeX = bounds.getX() + bounds.getWidth() * writePosition;
    const auto readX = bounds.getX() + bounds.getWidth() * readPosition;

    g.setColour (juce::Colours::deepskyblue);
    g.drawVerticalLine (juce::roundToInt (readX), bounds.getY(), bounds.getBottom());
    g.setColour (juce::Colours::orange);
    g.drawVerticalLine (juce::roundToInt (writeX), bounds.getY(), bounds.getBottom());
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      movingSpeedAttachment (p.apvts, "movingSpeed", movingSpeedSlider)
{
    addAndMakeVisible (originalWaveform);
    addAndMakeVisible (processedWaveform);
    addAndMakeVisible (playButton);
    addAndMakeVisible (pauseButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (movingSpeedSlider);

    movingSpeedSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    movingSpeedSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);

    originalWaveform.onClick = [this] { chooseAudioFile(); };
    playButton.onClick = [this]
    {
        processorRef.startPlayback();
        processedWaveform.clear();
        processedWaveform.setExpectedSamples (processorRef.getExpectedVisualizationSamples());
    };
    pauseButton.onClick = [this] { processorRef.pausePlayback(); };
    stopButton.onClick = [this] { processorRef.stopPlayback(); };

    setSize (640, 360);
    startTimerHz (30);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() = default;

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (12);
    const auto gap = 8;
    auto controls = bounds.removeFromBottom (28);
    bounds.removeFromBottom (gap);
    const auto waveformHeight = (bounds.getHeight() - gap) / 2;

    originalWaveform.setBounds (bounds.removeFromTop (waveformHeight));
    bounds.removeFromTop (gap);
    processedWaveform.setBounds (bounds);

    movingSpeedSlider.setBounds (controls.removeFromLeft (260));

    const auto buttonWidth = 72;
    const auto controlsWidth = buttonWidth * 3 + gap * 2;
    controls = controls.withSizeKeepingCentre (controlsWidth, controls.getHeight());
    playButton.setBounds (controls.removeFromLeft (buttonWidth));
    controls.removeFromLeft (gap);
    pauseButton.setBounds (controls.removeFromLeft (buttonWidth));
    controls.removeFromLeft (gap);
    stopButton.setBounds (controls);
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    std::array<float, 1024> processed {};
    const auto count = processorRef.readVisualizationSamples (processed.data(),
                                                               static_cast<int> (processed.size()));

    if (count > 0)
        processedWaveform.pushSamples (processed.data(), count);

    processedWaveform.setPointerPositions (processorRef.getReadPointerProgress(),
                                           processorRef.getWritePointerProgress());
}

void AudioPluginAudioProcessorEditor::chooseAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (juce::String(), juce::File(),
                                                       processorRef.getAudioFileWildcard());

    auto safeThis = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto file = chooser.getResult();
                                  if (file.existsAsFile())
                                  {
                                      if (safeThis->processorRef.loadAudioFile (file))
                                      {
                                          safeThis->originalWaveform.setFile (
                                              file, safeThis->processorRef.getTimelineDurationSeconds());
                                          safeThis->processedWaveform.clear();
                                          safeThis->processedWaveform.setExpectedSamples (
                                              safeThis->processorRef.getExpectedVisualizationSamples());
                                      }
                                  }
                              });
}
