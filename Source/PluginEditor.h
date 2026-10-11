#pragma once

#include "PluginProcessor.h"

#include <array>
#include <functional>

class OriginalWaveformDisplay final : public juce::Component,
                                      private juce::ChangeListener
{
public:
    OriginalWaveformDisplay();
    ~OriginalWaveformDisplay() override;

    void setFile (const juce::File& file, double timelineLengthSeconds);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 1 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };
    double timelineLength = 0.0;
};

class ProcessedWaveformDisplay final : public juce::Component
{
public:
    void pushSamples (const float* samples, int numSamples);
    void clear();
    void setExpectedSamples (int newExpectedSamples);
    void setPointerPositions (float newReadPosition, float newWritePosition);
    void paint (juce::Graphics&) override;

private:
    static constexpr int historySize = 4096;

    float readPosition = 0.0f;
    float writePosition = 0.0f;
    int validSamples = 0;
    int expectedSamples = historySize;
    std::array<float, historySize> history {};
};

class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseAudioFile();

    AudioPluginAudioProcessor& processorRef;
    OriginalWaveformDisplay originalWaveform;
    ProcessedWaveformDisplay processedWaveform;
    juce::TextButton playButton { "Play" };
    juce::TextButton pauseButton { "Pause" };
    juce::TextButton stopButton { "Stop" };
    juce::Slider movingSpeedSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment movingSpeedAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
