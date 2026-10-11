//
// Created by Jianghao on 10/6/2026.
//

#include "VariableDelay.h"

#include <cmath>
#include <stdexcept>

VariableDelay::VariableDelay()
{
    m_sampleRate = 0;

    m_delayHistory = nullptr;
    m_maxDelaySamples = 0;
    m_delayInSeconds = 0.f;
    m_delaySamplesInFloat = 0.f;

    m_writeIndex = 0;
    m_readPositionInFloat = 0.f;

    m_maxDelayInSeconds = 0.f;
    m_currentDelayInFloat = 0.f;
}

VariableDelay::~VariableDelay()
{
    delete[] m_delayHistory;
    m_delayHistory = nullptr;
}

void VariableDelay::prepare(const unsigned sampleRate,
                            float delayInSeconds,
                            unsigned maxDelaySamples)
{
    if (sampleRate == 0)
        throw std::invalid_argument("Sample rate must be greater than zero");

    if (maxDelaySamples == 0)
        throw std::invalid_argument("Max delay must be greater than zero");

    const float delaySamples = delayInSeconds * static_cast<float>(sampleRate);

    if (delaySamples < 1.0f || delaySamples > static_cast<float>(maxDelaySamples))
        throw std::invalid_argument("Delay is outside buffer range");

    reset();

    m_sampleRate = sampleRate;
    m_delayInSeconds = delayInSeconds;
    m_maxDelaySamples = maxDelaySamples;
    m_maxDelayInSeconds = static_cast<float>(maxDelaySamples) / static_cast<float>(sampleRate);
    m_delaySamplesInFloat = getDelayInSamples();

    m_delayHistory = new float[m_maxDelaySamples];
    for (unsigned i = 0; i < m_maxDelaySamples; i++)
    {
        m_delayHistory[i] = 0.f;
    }

    m_writeIndex = 0;
    m_readPositionInFloat = static_cast<float>(m_maxDelaySamples) - m_delaySamplesInFloat;
    m_currentDelayInFloat = m_delaySamplesInFloat;
}

void VariableDelay::process(float* buffer, const int size)
{
    if (buffer == nullptr)
        return;

    if (size <= 0)
        return;

    m_delaySamplesInFloat = getDelayInSamples();
    const float targetDelay = m_delaySamplesInFloat;
    const float deltaDelay = (targetDelay - m_currentDelayInFloat) / static_cast<float>(size);

    for (int i = 0; i < size; ++i)
    {
        const auto indexA = static_cast<unsigned> (std::floor (m_readPositionInFloat));
        const unsigned indexB = (indexA + 1) % m_maxDelaySamples;

        const float fraction = m_readPositionInFloat - static_cast<float>(indexA);

        const float historyA = m_delayHistory[indexA];
        const float historyB = m_delayHistory[indexB];
        const float history = historyA * (1.f - fraction) + historyB * fraction;

        const float currentSample = buffer[i];
        buffer[i] = history;
        m_delayHistory[m_writeIndex] = currentSample;

        m_writeIndex = (m_writeIndex + 1) % m_maxDelaySamples;
        m_readPositionInFloat = static_cast<float>(m_writeIndex) - m_currentDelayInFloat;
        if (m_readPositionInFloat < 0)
            m_readPositionInFloat += static_cast<float>(m_maxDelaySamples);

        m_currentDelayInFloat += deltaDelay;

    }
}

void VariableDelay::clear()
{
    if (m_delayHistory == nullptr || m_maxDelaySamples == 0)
        return;

    for (unsigned i = 0; i < m_maxDelaySamples; ++i)
        m_delayHistory[i] = 0.0f;

    m_writeIndex = 0;
    m_currentDelayInFloat = getDelayInSamples();
    m_readPositionInFloat = static_cast<float> (m_maxDelaySamples) - m_currentDelayInFloat;
}

void VariableDelay::reset()
{
    m_sampleRate = 0;

    m_readPositionInFloat = 0.f;
    m_writeIndex = 0;

    m_maxDelaySamples = 0;
    m_delayInSeconds = 0.f;
    m_delaySamplesInFloat = 0.f;

    delete[] m_delayHistory;
    m_delayHistory = nullptr;
}

void VariableDelay::setDelayInSeconds(const float newDelayInSeconds)
{
    if (newDelayInSeconds > m_maxDelayInSeconds || newDelayInSeconds <= 0.f)
        return;

    m_delayInSeconds.store(newDelayInSeconds);
}

float VariableDelay::getDelayInSamples() const
{
    const auto delayInSeconds = m_delayInSeconds.load();
    return delayInSeconds * static_cast<float>(m_sampleRate);
}
