//
// Created by Jianghao on 10/6/2026.
//

#pragma once

#include <atomic>

class VariableDelay
{
public:
    VariableDelay();
    ~VariableDelay();

    VariableDelay(const VariableDelay& other) = delete;
    VariableDelay& operator=(const VariableDelay& other) = delete;

    VariableDelay(VariableDelay&& other) = delete;
    VariableDelay& operator=(VariableDelay&& other) = delete;

    void prepare(unsigned sampleRate, float delayInSeconds, unsigned maxDelaySamples);

    void process(float* buffer, int size);

    void clear();

    void reset();

    void setDelayInSeconds(float newDelayInSeconds);

private:
    unsigned m_sampleRate;
    std::atomic<float> m_delayInSeconds;

    unsigned m_maxDelaySamples;
    float m_delaySamplesInFloat;
    float m_maxDelayInSeconds;

    float m_readPositionInFloat;
    unsigned m_writeIndex;

    float* m_delayHistory;

    constexpr static float m_smoothing = 0.01f;
    float m_currentDelayInFloat;

    float getDelayInSamples() const;
};
