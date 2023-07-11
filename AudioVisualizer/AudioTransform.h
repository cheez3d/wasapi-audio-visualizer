#pragma once

#include "IInitializable.h"

#include <fftw3.h>

#include <vector>

class AudioCapture;

class AudioTransform : public IInitializable
{
public:
    AudioTransform(AudioCapture* capture, float decibelCutoff = -40.f);

    AudioTransform(AudioTransform const&) = delete;
    AudioTransform(AudioTransform&&) = delete;

    AudioTransform& operator=(AudioTransform const&) = delete;
    AudioTransform& operator=(AudioTransform&&) = delete;

    virtual ~AudioTransform() override;

    void Transform();

    float const* GetSpectrum() const;
    size_t GetSpectrumSize() const;

    void ToggleDecibelMode();

    bool InitializeFFT();
    void DestroyFFT();

private:
    bool Initialize() override;
    void Destroy() override;

    AudioCapture* m_audioCapture;
    bool m_decibelMode;
    float m_decibelCutoff;

    float* m_fftInput;
    fftwf_complex* m_fftOutput;
    fftwf_plan m_fftPlan;
    std::vector<float> m_spectrum;
};
