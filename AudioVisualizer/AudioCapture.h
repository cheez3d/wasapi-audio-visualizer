#pragma once

#include "IInitializable.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>

#include <stddef.h>

#include <vector>

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

class AudioCaptureNotify;

class AudioCapture : public IInitializable
{
public:
    AudioCapture(REFERENCE_TIME windowDuration = 25 * REFTIMES_PER_MILLISEC);

    AudioCapture(AudioCapture const&) = delete;
    AudioCapture(AudioCapture&&) = delete;

    AudioCapture& operator=(AudioCapture const&) = delete;
    AudioCapture& operator=(AudioCapture&&) = delete;

    virtual ~AudioCapture() override;

    bool Capture();

    float const* GetWindowData() const;
    size_t GetWindowSize() const;
    size_t GetSampleRate() const;
    size_t GetSampleSize() const;
    size_t GetWindowNumSamples() const;

    bool DidDefaultDeviceChange();
    bool InitializeDefaultDeviceCapture();
    void DestroyDefaultDeviceCapture();

private:
    bool Initialize() override;
    void Destroy() override;

    REFERENCE_TIME m_windowDuration;

    IMMDeviceEnumerator* m_enumerator;
    AudioCaptureNotify* m_notificationClient;

    IMMDevice* m_device;
    IAudioClient* m_audioClient;

    union
    {
        WAVEFORMATEX* m_wfx;
        WAVEFORMATEXTENSIBLE* m_wfxt;
    };

    REFERENCE_TIME m_requestedCaptureDuration;
    UINT32 m_numBufferFrames;
    REFERENCE_TIME m_actualCaptureDuration;

    IAudioCaptureClient* m_audioCaptureClient;

    std::vector<float> m_aux;
    std::vector<float> m_buffer;

    size_t m_windowSize;
    size_t m_windowOffset;
};

class AudioCaptureNotify : public IMMNotificationClient
{
    friend class AudioCapture;

public:
    AudioCaptureNotify();

    AudioCaptureNotify(AudioCaptureNotify const&) = delete;
    AudioCaptureNotify(AudioCaptureNotify&&) = delete;

    AudioCaptureNotify& operator=(AudioCaptureNotify const&) = delete;
    AudioCaptureNotify& operator=(AudioCaptureNotify&&) = delete;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvInterface) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, PROPERTYKEY const key) override;

private:
    LONG m_numRefs;

    bool volatile m_didDefaultDeviceChange;
};
