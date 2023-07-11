#pragma once

#include "IInitializable.h"
#include "IRunnable.h"

#include <Windows.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include <stddef.h>

#include <tuple>

class AudioCapture;
class AudioTransform;
class Plot;

class Window
    : public IInitializable
    , public IRunnable
{
public:
    Window(bool isScreenSaver);
    Window(HWND hWndPreview);

    Window(Window const&) = delete;
    Window(Window&&) = delete;

    Window& operator=(Window const&) = delete;
    Window& operator=(Window&&) = delete;

    virtual ~Window() override;

    int GetWidth() const;
    int GetHeight() const;
    float GetDeltaTimeTarget() const;
    float GetDeltaTime() const;

    bool IsFullScreen() const;
    void ToggleFullScreen();

    SDL_Renderer* GetRenderer() const;
    AudioCapture* GetAudioCapture() const;
    AudioTransform* GetAudioTransform() const;

    bool Run() override;

private:
    bool Initialize() override;
    void Destroy() override;

    void Tick();

    int m_width;
    int m_height;
    int m_widthMin;
    int m_heightMin;

    bool m_isScreenSaver;
    size_t m_screenSaverInputCount;

    std::tuple<Uint8, Uint8, Uint8> m_backgroundColor;

    SDL_Window* m_window;
    SDL_SysWMinfo m_wmInfo;
    SDL_Renderer* m_renderer;
    SDL_DisplayMode m_displayMode;

    AudioCapture* m_audioCapture;
    AudioTransform* m_audioTransform;
    Plot* m_plot;

    Uint32 m_frameTime;

    HWND m_hWndPreview;
};
