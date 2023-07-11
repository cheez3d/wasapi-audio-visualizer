#include "AudioTransform.h"

#include "AudioCapture.h"

#include <fftw3.h>
#include <SDL.h>

#include <stddef.h>

#include <algorithm>
#include <iostream>
#include <vector>

static inline float Hann(float x, size_t i, size_t n)
{
    return x * 0.5f * (1.f - std::cosf(2.f * (float)M_PI * i / n));
}

AudioTransform::AudioTransform(AudioCapture* capture, float decibelCutoff)
    : m_audioCapture(capture)
    , m_decibelMode(true)
    , m_decibelCutoff(decibelCutoff)
    , m_fftInput()
    , m_fftOutput()
    , m_fftPlan()
{
    if (!Initialize())
        std::cerr << "Could not initialize AudioTransform" << std::endl;
}

AudioTransform::~AudioTransform()
{
    if (m_isInitialized)
        Destroy();
}

bool AudioTransform::Initialize()
{
    if (!InitializeFFT())
        goto fail;

    m_isInitialized = true;
    return true;

fail:
    Destroy();
    return false;
}

void AudioTransform::Destroy()
{
    DestroyFFT();
}

void AudioTransform::Transform()
{
    std::copy(m_audioCapture->GetWindowData(), m_audioCapture->GetWindowData() + m_audioCapture->GetWindowNumSamples(), m_fftInput);

    for (size_t i = 0; i < m_audioCapture->GetWindowNumSamples(); ++i)
        m_fftInput[i] = Hann(m_fftInput[i], i, m_audioCapture->GetWindowNumSamples());

    fftwf_execute(m_fftPlan);

    // calculate magnitudes
    for (size_t i = 0; i < m_spectrum.size(); ++i)
        m_spectrum[i] = std::sqrtf(m_fftOutput[i][0] * m_fftOutput[i][0] + m_fftOutput[i][1] * m_fftOutput[i][1]);

    float max = *std::max_element(m_spectrum.begin(), m_spectrum.end());

    if (max > 1.f)
    {
        // normalize
        for (size_t i = 0; i < m_spectrum.size(); ++i)
            m_spectrum[i] /= max;
    }

    if (m_decibelMode)
    {
        // convert to decibels
        for (size_t i = 0; i < m_spectrum.size(); ++i)
        {
            float decibels = 20.f * std::log10f(m_spectrum[i]);

            m_spectrum[i] = (decibels + std::fabsf(m_decibelCutoff)) / std::fabsf(m_decibelCutoff);
            m_spectrum[i] = std::fmaxf(m_spectrum[i], 0.f);
        }
    }
}

float const* AudioTransform::GetSpectrum() const
{
    return m_spectrum.data();
}

size_t AudioTransform::GetSpectrumSize() const
{
    return m_spectrum.size();
}

void AudioTransform::ToggleDecibelMode()
{
    m_decibelMode = !m_decibelMode;
}

bool AudioTransform::InitializeFFT()
{
    m_fftInput = fftwf_alloc_real(m_audioCapture->GetWindowNumSamples());
    if (!m_fftInput)
        goto fail;

    // http://www.fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html
    // https://www.ehu.eus/sgi/ARCHIVOS/fftw3.pdf#One-Dimensional%20DFTs%20of%20Real%20Data
    m_fftOutput = fftwf_alloc_complex(m_audioCapture->GetWindowNumSamples() / 2 + 1);
    if (!m_fftOutput)
        goto fail;

    m_fftPlan = fftwf_plan_dft_r2c_1d((int)m_audioCapture->GetWindowNumSamples(), m_fftInput, m_fftOutput, FFTW_ESTIMATE);
    if (!m_fftPlan)
        goto fail;

    m_spectrum.resize(m_audioCapture->GetWindowNumSamples() / 2 + 1);

    return true;

fail:
    DestroyFFT();
    return false;
}

void AudioTransform::DestroyFFT()
{
    if (m_fftPlan)
        fftwf_destroy_plan(m_fftPlan);
    if (m_fftOutput)
        fftwf_free(m_fftOutput);
    if (m_fftInput)
        fftwf_free(m_fftInput);
}
