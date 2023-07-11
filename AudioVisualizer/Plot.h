#pragma once

#include <SDL.h>

#include <stddef.h>

#include <functional>
#include <tuple>
#include <vector>

class Window;

class Plot
{
public:
    Plot(Window* window);

    Plot(Plot const&) = delete;
    Plot(Plot&&) = delete;

    Plot& operator=(Plot const&) = delete;
    Plot& operator=(Plot&&) = delete;

    size_t GetNumBins() const;
    float GetBinLevel(size_t bin) const;
    float GetHatLevel(size_t bin) const;

    void Update();
    void Render();

    void CalculateBinValues();
    void CalculateSpectrumValues();

private:
    void SetBinLevel(size_t bin, float level);
    void SetHatLevel(size_t bin, float level);

    size_t CalculateSpectrumBin(size_t spectrum);

    Window* m_window;

    std::tuple<Uint8, Uint8, Uint8> m_color;

    float m_binSpacing;
    float m_binWidth;
    float m_binHeightMax;
    std::tuple<Uint8, Uint8, Uint8> m_binColorLow;
    std::tuple<Uint8, Uint8, Uint8> m_binColorHigh;
    std::vector<SDL_FRect> m_bins;

    float m_hatHeight;
    float m_hatBinSpacing;
    std::vector<SDL_FRect> m_hats;

    size_t m_frequencyLow;
    size_t m_frequencyHigh;
    std::function<float(float)> m_frequencyDistribution;

    size_t m_spectrumLow;
    size_t m_spectrumHigh;
    std::vector<std::vector<size_t>> m_binsMissed;
    std::vector<float> m_binLevelsDistributed;

    std::vector<float> m_hatLevelVelocities;

    float m_binLevelSmoothness;
    float m_hatGravity;
};
