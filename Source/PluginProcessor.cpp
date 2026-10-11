#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <utility>

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    m_formatManager.registerBasicFormats();
    m_readAheadThread.startThread();
    m_movingSpeedParameter = apvts.getRawParameterValue (movingSpeedId);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    m_transportSource.stop();
    m_transportSource.setSource (nullptr);
    m_readerSource.reset();
    m_readAheadThread.stopThread (1000);
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    m_processingSampleRate = sampleRate;
    const auto maximumDelaySamples = static_cast<unsigned> (std::ceil (sampleRate * 2.0));
    m_variableDelay.prepare (static_cast<unsigned> (sampleRate),
                             static_cast<float> (delaySeconds),
                             maximumDelaySamples);
    m_transportSource.prepareToPlay (samplesPerBlock, sampleRate);

    configureVisualization();
    m_visualizationCounter = 0;
    m_visualizationFifo.reset();
}

void AudioPluginAudioProcessor::releaseResources()
{
    m_transportSource.releaseResources();
    m_variableDelay.reset();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    const auto fileLoaded = m_fileLoaded.load();
    auto samplesToProcess = buffer.getNumSamples();

    if (fileLoaded)
    {
        if (! m_playbackEnabled.load())
        {
            buffer.clear();
            return;
        }

        const auto remainingTimelineSamples = m_totalTimelineSamples - m_processedTimelineSamples.load();

        samplesToProcess = static_cast<int> (juce::jmin<int64_t> (buffer.getNumSamples(),
                                                                  remainingTimelineSamples));
        buffer.clear();
        if (! m_sourceFinished.load())
        {
            juce::AudioSourceChannelInfo sourceInfo (&buffer, 0, buffer.getNumSamples());
            m_transportSource.getNextAudioBlock (sourceInfo);

            if (m_transportSource.hasStreamFinished())
                m_sourceFinished.store (true);
        }
    }

    if (m_clearDelayHistory.exchange (false))
        m_variableDelay.clear();



    m_elapsedTime += static_cast<float>(samplesToProcess / m_processingSampleRate);
    const auto movingSpeed = m_movingSpeedParameter->load();
    const float alongPath = distanceInMeters - movingSpeed * m_elapsedTime;
    const float distance = std::sqrt(alongPath * alongPath + 10 * 10);
    const float delayTime = distance / speedOfSound;
    m_variableDelay.setDelayInSeconds(delayTime);
    m_currentDelaySeconds.store (delayTime);

    auto* processed = buffer.getWritePointer (0);
    m_variableDelay.process (processed, samplesToProcess);




    if (samplesToProcess < buffer.getNumSamples())
        buffer.clear (0, samplesToProcess, buffer.getNumSamples() - samplesToProcess);

    for (int sample = 0; sample < samplesToProcess; ++sample)
    {
        if (m_visualizationCounter == 0)
            pushVisualizationSample (processed[sample]);

        if (++m_visualizationCounter >= m_visualizationInterval)
            m_visualizationCounter = 0;
    }

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom (channel, 0, buffer, 0, 0, buffer.getNumSamples());

    if (fileLoaded)
    {
        const auto newPosition = m_processedTimelineSamples.fetch_add (samplesToProcess) + samplesToProcess;
        if (newPosition >= m_totalTimelineSamples)
            m_playbackEnabled.store (false);
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

bool AudioPluginAudioProcessor::loadAudioFile (const juce::File& file)
{
    auto* reader = m_formatManager.createReaderFor (file);
    if (reader == nullptr)
        return false;

    const auto sourceSampleRate = reader->sampleRate;
    m_fileDurationSeconds = static_cast<double> (reader->lengthInSamples) / sourceSampleRate;
    auto newReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    newReaderSource->setLooping (false);

    m_playbackEnabled.store (false);
    m_transportSource.stop();
    m_transportSource.setSource (nullptr);
    m_readerSource = std::move (newReaderSource);
    m_transportSource.setSource (m_readerSource.get(), 32768, &m_readAheadThread, sourceSampleRate, 2);
    m_transportSource.setPosition (0.0);
    m_clearDelayHistory.store (true);
    m_sourceFinished.store (false);
    m_processedTimelineSamples.store (0);
    m_visualizationFifo.reset();
    m_visualizationCounter = 0;
    configureVisualization();
    m_fileLoaded.store (true);
    return true;
}

void AudioPluginAudioProcessor::startPlayback()
{
    if (! m_fileLoaded.load())
        return;

    m_playbackEnabled.store (false);
    m_transportSource.stop();
    m_transportSource.setPosition (0.0);
    m_sourceFinished.store (false);
    m_processedTimelineSamples.store (0);
    m_clearDelayHistory.store (true);
    m_visualizationFifo.reset();
    m_visualizationCounter = 0;
    m_elapsedTime = 0.0f;
    m_currentDelaySeconds.store (static_cast<float> (delaySeconds));
    m_transportSource.start();
    m_playbackEnabled.store (true);
}

void AudioPluginAudioProcessor::pausePlayback()
{
    m_playbackEnabled.store (false);
    m_transportSource.stop();
    m_visualizationFifo.reset();
}

void AudioPluginAudioProcessor::stopPlayback()
{
    m_playbackEnabled.store (false);
    m_transportSource.stop();
    m_transportSource.setPosition (0.0);
    m_clearDelayHistory.store (true);
    m_sourceFinished.store (false);
    m_processedTimelineSamples.store (0);
    m_visualizationFifo.reset();
}

int AudioPluginAudioProcessor::readVisualizationSamples (float* samples, const int maxSamples)
{
    const auto read = m_visualizationFifo.read (maxSamples);
    auto count = 0;
    read.forEach ([this, samples, &count] (const int index)
    {
        samples[count++] = m_processedVisualization[static_cast<size_t> (index)];
    });
    return count;
}

double AudioPluginAudioProcessor::getTimelineDurationSeconds() const
{
    return m_fileDurationSeconds + delaySeconds;
}

int AudioPluginAudioProcessor::getExpectedVisualizationSamples() const
{
    return m_expectedVisualizationSamples;
}

float AudioPluginAudioProcessor::getReadPointerProgress() const
{
    const auto total = m_totalTimelineSamples;
    if (total <= 0)
        return 0.0f;

    const auto delay = static_cast<int64_t> (
        std::llround (m_currentDelaySeconds.load() * m_processingSampleRate));
    const auto readPosition = juce::jmax<int64_t> (0, m_processedTimelineSamples.load() - delay);
    return static_cast<float> (static_cast<double> (readPosition) / static_cast<double> (total));
}

float AudioPluginAudioProcessor::getWritePointerProgress() const
{
    const auto total = m_totalTimelineSamples;
    if (total <= 0)
        return 0.0f;

    return static_cast<float> (static_cast<double> (m_processedTimelineSamples.load())
                               / static_cast<double> (total));
}

juce::String AudioPluginAudioProcessor::getAudioFileWildcard() const
{
    return m_formatManager.getWildcardForAllFormats();
}

void AudioPluginAudioProcessor::configureVisualization()
{
    const auto sampleRate = m_processingSampleRate;
    const auto duration = m_fileDurationSeconds + delaySeconds;
    const auto totalSamples = static_cast<int64_t> (std::ceil (duration * sampleRate));
    m_totalTimelineSamples = totalSamples;

    const auto interval = static_cast<int> (
        juce::jmax<int64_t> (1, (totalSamples + visualizationPointCapacity - 1)
                                   / visualizationPointCapacity));
    m_visualizationInterval = interval;
    m_expectedVisualizationSamples = static_cast<int> (
        juce::jlimit<int64_t> (2, visualizationPointCapacity,
                               (totalSamples + interval - 1) / interval));
}

void AudioPluginAudioProcessor::pushVisualizationSample (const float sample)
{
    const auto write = m_visualizationFifo.write (1);
    write.forEach ([this, sample] (const int index)
    {
        m_processedVisualization[static_cast<size_t> (index)] = sample;
    });
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { movingSpeedId, 1 },
        "Moving Speed",
        juce::NormalisableRange<float> { 0.0f, 400.0f, 1.0f },
        200.0f));
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
