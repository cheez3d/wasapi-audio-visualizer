#include "AudioCapture.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <Windows.h>

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <cstring>

#include <algorithm>
#include <iostream>
#include <numeric>

CLSID const CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
IID const IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
IID const IID_IMMNotificationClient = __uuidof(IMMNotificationClient);
IID const IID_IAudioClient = __uuidof(IAudioClient);
IID const IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

AudioCapture::AudioCapture(REFERENCE_TIME windowDuration)
    : m_windowDuration(windowDuration)
    , m_enumerator()
    , m_notificationClient()
    , m_device()
    , m_audioClient()
    , m_wfx()
    , m_requestedCaptureDuration(REFTIMES_PER_SEC)
    , m_audioCaptureClient()
{
    if (!Initialize())
        std::cerr << "Could not initialize AudioCapture" << std::endl;
}

AudioCapture::~AudioCapture()
{
    if (m_isInitialized)
        Destroy();
}

bool AudioCapture::Initialize()
{
    HRESULT hr;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
        IID_IMMDeviceEnumerator, (LPVOID*)&m_enumerator);
    if (FAILED(hr))
        goto fail;

    m_notificationClient = new AudioCaptureNotify();

    hr = m_enumerator->RegisterEndpointNotificationCallback(m_notificationClient);
    if (FAILED(hr))
        goto fail;

    if (!InitializeDefaultDeviceCapture())
        goto fail;

    m_isInitialized = true;
    return true;

fail:
    Destroy();
    return false;
}

void AudioCapture::Destroy()
{
    DestroyDefaultDeviceCapture();

    if (m_notificationClient)
        m_enumerator->UnregisterEndpointNotificationCallback(m_notificationClient);
    if (m_enumerator)
        m_enumerator->Release();
}

bool AudioCapture::Capture()
{
    static auto AddSampleToBuffer = [this](float sample)
    {
        m_buffer[(m_windowOffset++) + m_windowSize] = sample;

        if (m_windowOffset >= m_windowSize)
        {
            std::copy(m_buffer.begin() + m_windowSize, m_buffer.end(), m_buffer.begin());
            m_windowOffset = 0;
        }
    };

    static UINT32 lastNumFramesAvailable = 0;

    HRESULT hr;
    UINT32 numFramesInNextPacket;
    UINT32 numFramesAvailable;
    BYTE* data;
    DWORD flags;
    float averagedSample;

    hr = m_audioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
    if (FAILED(hr))
        return false;

    if (numFramesInNextPacket == 0)
    {
        for (size_t i = 0; i < lastNumFramesAvailable; ++i)
            AddSampleToBuffer(0.f);
    }

    while (numFramesInNextPacket > 0)
    {
        hr = m_audioCaptureClient->GetBuffer(&data, &numFramesAvailable, &flags, nullptr, nullptr);
        if (FAILED(hr))
            return false;

        lastNumFramesAvailable = numFramesAvailable;

        for (size_t i = 0; i < numFramesAvailable; ++i)
        {
            std::memcpy(m_aux.data(), &data[i * m_wfx->nBlockAlign], m_wfx->nBlockAlign);
            averagedSample = std::accumulate(m_aux.begin(), m_aux.end(), 0.f) / m_wfx->nChannels;

            AddSampleToBuffer(averagedSample);
        }

        hr = m_audioCaptureClient->ReleaseBuffer(numFramesAvailable);
        if (FAILED(hr))
            return false;

        hr = m_audioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
        if (FAILED(hr))
            return false;
    }

    return true;
}

float const* AudioCapture::GetWindowData() const
{
    return m_buffer.data() + m_windowOffset;
}

size_t AudioCapture::GetWindowSize() const
{
    return m_windowSize;
}

size_t AudioCapture::GetSampleRate() const
{
    return m_wfx->nSamplesPerSec;
}

size_t AudioCapture::GetSampleSize() const
{
    return m_wfx->nBlockAlign / m_wfx->nChannels;
}

size_t AudioCapture::GetWindowNumSamples() const
{
    return GetWindowSize() / GetSampleSize();
}

bool AudioCapture::DidDefaultDeviceChange()
{
    if (m_notificationClient->m_didDefaultDeviceChange)
    {
        m_notificationClient->m_didDefaultDeviceChange = false;
        return true;
    }
    return false;
}

bool AudioCapture::InitializeDefaultDeviceCapture()
{
    HRESULT hr;
    size_t bufferSize;

    hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    if (FAILED(hr))
        goto fail;

    hr = m_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&m_audioClient);
    if (FAILED(hr))
        goto fail;

    hr = m_audioClient->GetMixFormat(&m_wfx);
    if (FAILED(hr))
        goto fail;

    if (!(m_wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        m_wfxt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
        m_wfx->wBitsPerSample == 32 && m_wfxt->Samples.wValidBitsPerSample == 32))
    {
        OLECHAR guid[64];

        std::cerr << "Unsupported audio format: " << m_wfx->wFormatTag;
        if (m_wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && StringFromGUID2(m_wfxt->SubFormat, guid, 64))
            std::wcerr
                << ":" << guid
                << ":" << m_wfxt->Samples.wValidBitsPerSample
                << "/" << m_wfx->wBitsPerSample;
        std::cerr << std::endl;

        goto fail;
    }

    m_aux.resize(m_wfx->nChannels);

    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        m_requestedCaptureDuration, 0, m_wfx, nullptr);
    if (FAILED(hr))
        goto fail;

    hr = m_audioClient->GetBufferSize(&m_numBufferFrames);
    if (FAILED(hr))
        goto fail;

    m_actualCaptureDuration = (REFERENCE_TIME)(REFTIMES_PER_SEC * (float)m_numBufferFrames / GetSampleRate());

    hr = m_audioClient->GetService(IID_IAudioCaptureClient, (void**)&m_audioCaptureClient);
    if (FAILED(hr))
        goto fail;

    hr = m_audioClient->Start();
    if (FAILED(hr))
        goto fail;

    m_windowSize = (size_t)std::ceilf((float)m_windowDuration / REFTIMES_PER_SEC * GetSampleRate() * GetSampleSize());
    m_windowOffset = 0;

    bufferSize = m_windowSize * 2;

    m_buffer.resize(bufferSize, 0);

    return true;

fail:
    DestroyDefaultDeviceCapture();
    return false;
}

void AudioCapture::DestroyDefaultDeviceCapture()
{
    if (m_audioClient)
        m_audioClient->Stop();
    if (m_audioCaptureClient)
        m_audioCaptureClient->Release();
    if (m_wfx)
        CoTaskMemFree(m_wfx);
    if (m_audioClient)
        m_audioClient->Release();
    if (m_device)
        m_device->Release();
}

AudioCaptureNotify::AudioCaptureNotify()
    : m_numRefs(1)
    , m_didDefaultDeviceChange(false)
{
}

HRESULT STDMETHODCALLTYPE AudioCaptureNotify::QueryInterface(REFIID riid, void** ppvInterface)
{
    if (riid == IID_IUnknown)
    {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if (riid == IID_IMMNotificationClient)
    {
        AddRef();
        *ppvInterface = (IMMNotificationClient*)this;
    }
    else
    {
        *ppvInterface = nullptr;
        return E_NOINTERFACE;
    }
    return S_OK;
}

ULONG STDMETHODCALLTYPE AudioCaptureNotify::AddRef()
{
    return InterlockedIncrement(&m_numRefs);
}

ULONG STDMETHODCALLTYPE AudioCaptureNotify::Release()
{
    ULONG numRefs = InterlockedDecrement(&m_numRefs);
    if (numRefs == 0)
        delete this;
    return numRefs;
}

HRESULT STDMETHODCALLTYPE AudioCaptureNotify::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AudioCaptureNotify::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AudioCaptureNotify::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AudioCaptureNotify::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
{
    if (flow == eRender && role == eConsole)
        m_didDefaultDeviceChange = true;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE AudioCaptureNotify::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, PROPERTYKEY const key)
{
    return S_OK;
}
