#include "Plot.h"

#include "AudioCapture.h"
#include "AudioTransform.h"
#include "Easing.h"
#include "Window.h"

#include <SDL.h>

#include <stddef.h>

#include <cmath>

#include <algorithm>
#include <numeric>
#include <tuple>
#include <vector>
#include <utility>

Plot::Plot(Window* window)
    : m_window(window)
    , m_color(255, 255, 255)
    , m_binSpacing(2.f)
    , m_binColorLow(171, 43, 98)
    , m_binColorHigh(82, 107, 238)
    , m_hatHeight(4.f)
    , m_hatBinSpacing(1.f)
    , m_frequencyLow(20)
    , m_frequencyHigh(20000)
    , m_frequencyDistribution(EaseOutExp)
    , m_binLevelSmoothness(0.96f)
    , m_hatGravity(0.75f)
{
    CalculateBinValues();
    CalculateSpectrumValues();
}

size_t Plot::GetNumBins() const
{
    return m_bins.size();
}

float Plot::GetBinLevel(size_t bin) const
{
    return m_bins[bin].h / m_binHeightMax;
}

float Plot::GetHatLevel(size_t bin) const
{
    return 1.f - (m_hats[bin].y - m_binSpacing) / m_binHeightMax;
}

void Plot::SetBinLevel(size_t bin, float level)
{
    m_bins[bin].y = m_binSpacing + (m_hatHeight + m_hatBinSpacing) + m_binHeightMax * (1.f - level);
    m_bins[bin].h = m_binHeightMax * level;
}

void Plot::SetHatLevel(size_t bin, float level)
{
    m_hats[bin].y = m_binSpacing + m_binHeightMax * (1.f - level);
}

size_t Plot::CalculateSpectrumBin(size_t spectrum)
{
    return (size_t)std::roundf((GetNumBins() - 1) * m_frequencyDistribution((float)(spectrum - m_spectrumLow) / (m_spectrumHigh - m_spectrumLow)));
}

void Plot::Update()
{
    float dt = m_window->GetDeltaTime() / 1000.f;
    float f = m_window->GetDeltaTime() / m_window->GetDeltaTimeTarget();

    float const* spectrum = m_window->GetAudioTransform()->GetSpectrum();
    size_t spectrumSize = m_window->GetAudioTransform()->GetSpectrumSize();

    std::fill(m_binLevelsDistributed.begin(), m_binLevelsDistributed.end(), 0.f);

    for (size_t i = 0; i < m_spectrumLow; ++i)
        m_binLevelsDistributed[0] += spectrum[i];

    for (size_t i = m_spectrumLow; i <= m_spectrumHigh; ++i)
        m_binLevelsDistributed[CalculateSpectrumBin(i)] += spectrum[i];

    for (size_t i = m_spectrumHigh + 1; i < spectrumSize; ++i)
        m_binLevelsDistributed[GetNumBins() - 1] += spectrum[i];

    for (auto&& bins : m_binsMissed)
    {
        size_t binFirst = bins[0];
        size_t binLast = bins[bins.size() - 1];

        size_t binLow = binFirst == 0 ? 0 : binFirst - 1;
        size_t binHigh = binLast == GetNumBins() - 1 ? GetNumBins() - 1 : binLast + 1;

        float binLevelLow = m_binLevelsDistributed[binLow];
        float binLevelHigh = m_binLevelsDistributed[binHigh];

        float step = 1.f / (bins.size() + 1);

        for (size_t bin = binFirst; bin <= binLast; ++bin)
        {
            float position = step * (bin - binFirst + 1);

            position = binLevelLow > binLevelHigh ? 1.f - m_frequencyDistribution(1.f - position) : m_frequencyDistribution(position);

            m_binLevelsDistributed[bin] = Lerp(position, binLevelLow, binLevelHigh);
        }
    }

    float levelMax = *std::max_element(m_binLevelsDistributed.begin(), m_binLevelsDistributed.end());

    if (levelMax > 1.f)
    {
        for (size_t bin = 0; bin < GetNumBins(); ++bin)
            m_binLevelsDistributed[bin] /= levelMax;
    }

    for (size_t bin = 0; bin < GetNumBins(); ++bin)
    {
        SetBinLevel(bin, Lerp((1.f - m_binLevelSmoothness) * f, GetBinLevel(bin), m_binLevelsDistributed[bin]));

        if (GetHatLevel(bin) > GetBinLevel(bin))
        {
            m_hatLevelVelocities[bin] -= m_hatGravity * dt;

            float hatLevelNext = GetHatLevel(bin) + m_hatLevelVelocities[bin] * dt;
            if (hatLevelNext > GetBinLevel(bin))
            {
                SetHatLevel(bin, hatLevelNext);
            }
            else
            {
                m_hatLevelVelocities[bin] = 0.f;
                SetHatLevel(bin, GetBinLevel(bin));
            }
        }
        else
        {
            SetHatLevel(bin, GetBinLevel(bin));
        }
    }
}

void Plot::Render()
{
    static auto CalculateBinColor = [this](size_t bin, float level)
    {
        float position = m_frequencyDistribution((float)bin / (GetNumBins() - 1));

        return std::make_tuple(
            (Uint8)Lerp(level, std::get<0>(m_color), Lerp(position, std::get<0>(m_binColorLow), std::get<0>(m_binColorHigh))),
            (Uint8)Lerp(level, std::get<1>(m_color), Lerp(position, std::get<1>(m_binColorLow), std::get<1>(m_binColorHigh))),
            (Uint8)Lerp(level, std::get<2>(m_color), Lerp(position, std::get<2>(m_binColorLow), std::get<2>(m_binColorHigh))));
    };

    for (size_t bin = 0; bin < GetNumBins(); ++bin)
    {
        Uint8 r, g, b;

        float level = GetBinLevel(bin);

        std::tie(r, g, b) = CalculateBinColor(bin, level);
        SDL_SetRenderDrawColor(m_window->GetRenderer(), r, g, b, 255);

        SDL_RenderFillRectF(m_window->GetRenderer(), &m_bins[bin]);

        std::tie(r, g, b) = m_color;
        SDL_SetRenderDrawColor(m_window->GetRenderer(), r, g, b, 255);

        SDL_RenderFillRectF(m_window->GetRenderer(), &m_hats[bin]);
    }
}

void Plot::CalculateBinValues()
{
    size_t numBinsOld = GetNumBins();
    std::vector<float> binLevelsDistributedOld(m_binLevelsDistributed);
    std::vector<float> hatLevelVelocitiesOld(m_hatLevelVelocities);

    m_bins.resize(m_window->GetWidth() / 16);
    m_hats.resize(GetNumBins());

    m_binLevelsDistributed.resize(GetNumBins(), 0.f);
    m_hatLevelVelocities.resize(GetNumBins(), 0.f);

    if (numBinsOld > 0)
    {
        std::fill(m_binLevelsDistributed.begin(), m_binLevelsDistributed.end(), 0.f);
        std::fill(m_hatLevelVelocities.begin(), m_hatLevelVelocities.end(), 0.f);

        for (size_t bin = 0; bin < GetNumBins(); ++bin)
        {
            size_t binOld = (size_t)std::roundf((float)bin / (GetNumBins() - 1) * (numBinsOld - 1));

            m_binLevelsDistributed[bin] += binLevelsDistributedOld[binOld];
            m_hatLevelVelocities[bin] = hatLevelVelocitiesOld[binOld];
        }

        float levelMax = *std::max_element(m_binLevelsDistributed.begin(), m_binLevelsDistributed.end());

        if (levelMax > 1.f)
        {
            for (size_t bin = 0; bin < GetNumBins(); ++bin)
                m_binLevelsDistributed[bin] /= levelMax;
        }
    }

    m_binWidth = (m_window->GetWidth() - (GetNumBins() + 1) * m_binSpacing) / GetNumBins();
    m_binHeightMax = m_window->GetHeight() - 2.f * m_binSpacing - (m_hatHeight + m_hatBinSpacing);

    for (size_t bin = 0; bin < GetNumBins(); ++bin)
    {
        m_bins[bin].x = m_binSpacing + (m_binWidth + m_binSpacing) * bin;
        m_bins[bin].w = m_binWidth;

        m_hats[bin].x = m_bins[bin].x;
        m_hats[bin].w = m_bins[bin].w;
        m_hats[bin].h = m_hatHeight;

        SetBinLevel(bin, m_binLevelsDistributed[bin]);
        SetHatLevel(bin, GetBinLevel(bin));
    }
}

void Plot::CalculateSpectrumValues()
{
    size_t numFrequencies = m_window->GetAudioCapture()->GetSampleRate() / 2;
    size_t spectrumSize = m_window->GetAudioTransform()->GetSpectrumSize();

    m_spectrumLow = numFrequencies > m_frequencyLow ? m_frequencyLow : 0;
    m_spectrumHigh = (std::min)(m_frequencyHigh, numFrequencies);

    m_spectrumLow = (size_t)std::floorf((float)m_spectrumLow / numFrequencies * spectrumSize);
    m_spectrumHigh = (size_t)std::ceilf((float)m_spectrumHigh / numFrequencies * spectrumSize);

    m_binsMissed.clear();

    size_t binPrev = 0;

    for (size_t i = m_spectrumLow; i <= m_spectrumHigh; ++i)
    {
        size_t bin = CalculateSpectrumBin(i);

        if (bin - binPrev > 1)
        {
            std::vector<size_t> bins(bin - binPrev - 1);

            std::iota(bins.begin(), bins.end(), binPrev + 1);

            m_binsMissed.push_back(std::move(bins));
        }

        binPrev = bin;
    }
}
