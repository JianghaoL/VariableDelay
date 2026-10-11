#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "VariableDelay.h"

#include <array>
#include <atomic>
#include <memory>

class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    bool loadAudioFile (const juce::File& file);
    void startPlayback();
    void pausePlayback();
    void stopPlayback();
    int readVisualizationSamples (float* samples, int maxSamples);
    double getTimelineDurationSeconds() const;
    int getExpectedVisualizationSamples() const;
    float getReadPointerProgress() const;
    float getWritePointerProgress() const;
    juce::String getAudioFileWildcard() const;

private:
    static constexpr int visualizationCapacity = 8192;
    static constexpr int visualizationPointCapacity = 4096;
    static constexpr double delaySeconds = 1.0;
    static constexpr auto movingSpeedId = "movingSpeed";

    void configureVisualization();
    void pushVisualizationSample (float sample);

    VariableDelay m_variableDelay;
    juce::AudioFormatManager m_formatManager;
    juce::TimeSliceThread m_readAheadThread { "Audio File Read Ahead" };
    juce::AudioTransportSource m_transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> m_readerSource;
    std::atomic<bool> m_fileLoaded { false };
    std::atomic<bool> m_playbackEnabled { false };
    std::atomic<bool> m_clearDelayHistory { false };

    juce::AbstractFifo m_visualizationFifo { visualizationCapacity };
    std::array<float, visualizationCapacity> m_processedVisualization {};
    double m_processingSampleRate = 44100.0;
    double m_fileDurationSeconds = 0.0;
    int64_t m_totalTimelineSamples = 0;
    std::atomic<int64_t> m_processedTimelineSamples { 0 };
    int m_expectedVisualizationSamples = 2;
    std::atomic<bool> m_sourceFinished { false };
    int m_visualizationInterval = 1;
    int m_visualizationCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)



private:
    static constexpr float distanceInMeters = 343.f;
    static constexpr float speedOfSound = 343.f;

    std::atomic<float>* m_movingSpeedParameter = nullptr;
    std::atomic<float> m_currentDelaySeconds { static_cast<float> (delaySeconds) };
    float m_elapsedTime = 0.0f;
};
