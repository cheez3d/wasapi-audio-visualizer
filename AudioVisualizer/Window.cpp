#include "Window.h"

#include "AudioCapture.h"
#include "AudioTransform.h"
#include "Plot.h"

#include <Windows.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include <stddef.h>

#include <cmath>

#include <iostream>

Window::Window(bool isScreenSaver)
    : m_widthMin(200)
    , m_heightMin(100)
    , m_isScreenSaver(isScreenSaver)
    , m_screenSaverInputCount()
    , m_backgroundColor(0, 0, 0)
    , m_window()
    , m_renderer()
    , m_audioCapture()
    , m_audioTransform()
    , m_plot()
    , m_hWndPreview(nullptr)
{
    if (!Initialize())
        std::cerr << SDL_GetError() << std::endl;
}

Window::Window(HWND hWndPreview)
    : m_widthMin(100)
    , m_heightMin(50)
    , m_isScreenSaver(true)
    , m_screenSaverInputCount()
    , m_backgroundColor(0, 0, 0)
    , m_window()
    , m_renderer()
    , m_audioCapture()
    , m_audioTransform()
    , m_plot()
    , m_hWndPreview(hWndPreview)
{
    if (!Initialize())
        std::cerr << SDL_GetError() << std::endl;
}

Window::~Window()
{
    if (m_isInitialized)
        Destroy();
}

bool Window::Initialize()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        return false;

    if (m_hWndPreview)
    {
        WNDCLASSEX wc;
        wc.cbSize = sizeof(wc);
        wc.style = 0;
        wc.lpfnWndProc = DefWindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = nullptr;
        wc.hIcon = nullptr;
        wc.hCursor = nullptr;
        wc.hbrBackground = nullptr;
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = TEXT(WINDOW_NAME);
        wc.hIconSm = nullptr;
        if (!RegisterClassEx(&wc))
            return false;

        RECT rect;
        GetClientRect(m_hWndPreview, &rect);

        HWND hWnd = CreateWindowEx(0, TEXT(WINDOW_NAME), TEXT(WINDOW_NAME),
            WS_CHILD | WS_VISIBLE,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            m_hWndPreview, nullptr, nullptr, nullptr);
        if (!hWnd)
            return false;

        m_window = SDL_CreateWindowFrom(hWnd);
        if (!m_window)
            return false;

        SDL_EnableScreenSaver();
    }
    else
    {
        Uint32 flags = SDL_WINDOW_SHOWN;
        if (m_isScreenSaver)
        {
            while (ShowCursor(FALSE) >= 0);
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }

        m_window = SDL_CreateWindow(WINDOW_NAME,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH, WINDOW_HEIGHT, flags);
        if (!m_window)
            return false;

        if (m_isScreenSaver)
        {
            SDL_DisableScreenSaver();
            SDL_SetWindowResizable(m_window, SDL_FALSE);
        }
        else
        {
            SDL_EnableScreenSaver();
            SDL_SetWindowResizable(m_window, SDL_TRUE);
        }
    }

    SDL_SetWindowMinimumSize(m_window, m_widthMin, m_heightMin);

    SDL_VERSION(&m_wmInfo.version);
    SDL_GetWindowWMInfo(m_window, &m_wmInfo);

    m_renderer = SDL_CreateRenderer(m_window, -1, 0);
    if (!m_renderer)
        return false;
    if (SDL_GetRendererOutputSize(m_renderer, &m_width, &m_height) < 0)
        return false;

    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(m_window), &m_displayMode) < 0)
        return false;

    m_audioCapture = new AudioCapture();
    if (!m_audioCapture->IsInitialized())
        return false;

    m_audioTransform = new AudioTransform(m_audioCapture);
    if (!m_audioTransform->IsInitialized())
        return false;

    m_plot = new Plot(this);

    m_isInitialized = true;
    return true;
}

void Window::Destroy()
{
    if (m_plot)
        delete m_plot;
    if (m_audioTransform)
        delete m_audioTransform;
    if (m_audioCapture)
        delete m_audioCapture;
    if (m_renderer)
        SDL_DestroyRenderer(m_renderer);
    if (m_window)
        SDL_DestroyWindow(m_window);

    UnregisterClass(TEXT(WINDOW_NAME), nullptr);

    SDL_Quit();
}

int Window::GetWidth() const
{
    return m_width;
}

int Window::GetHeight() const
{
    return m_height;
}

float Window::GetDeltaTimeTarget() const
{
    return 1000.f / m_displayMode.refresh_rate;
}

float Window::GetDeltaTime() const
{
    return m_frameTime >= GetDeltaTimeTarget() ? m_frameTime : GetDeltaTimeTarget();
}

bool Window::IsFullScreen() const
{
    return SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN;
}

void Window::ToggleFullScreen()
{
    if (!IsFullScreen())
    {
        while (ShowCursor(FALSE) >= 0);
        SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else
    {
        SDL_SetWindowFullscreen(m_window, 0);
        ShowCursor(TRUE);
    }
}

SDL_Renderer* Window::GetRenderer() const
{
    return m_renderer;
}

AudioCapture* Window::GetAudioCapture() const
{
    return m_audioCapture;
}

AudioTransform* Window::GetAudioTransform() const
{
    return m_audioTransform;
}

bool Window::Run()
{
    SDL_Event event;

    m_isRunning = true;

    while (m_isRunning)
    {
        if (m_isScreenSaver && m_hWndPreview && !IsWindow(m_hWndPreview))
        {
            m_isRunning = false;
            break;
        }

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_DISPLAYEVENT:
            case SDL_WINDOWEVENT:
            case SDL_RENDER_TARGETS_RESET:
            {
                SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(m_window), &m_displayMode);

                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    SDL_GetRendererOutputSize(m_renderer, &m_width, &m_height);

                    m_plot->CalculateBinValues();
                    m_plot->CalculateSpectrumValues();
                }

                break;
            }
            case SDL_KEYDOWN:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            {
                if (m_isScreenSaver)
                {
                    if (!m_hWndPreview && m_screenSaverInputCount++ > 0)
                        m_isRunning = false;

                    break;
                }

                if (event.key.keysym.sym == SDLK_d)
                {
                    m_audioTransform->ToggleDecibelMode();
                }
                else if (event.button.clicks == 2 ||
                    event.key.keysym.sym == SDLK_F11 ||
                    event.key.keysym.mod & KMOD_ALT && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) ||
                    event.key.keysym.sym == SDLK_ESCAPE && IsFullScreen())
                {
                    ToggleFullScreen();
                }

                break;
            }
            case SDL_QUIT:
                m_isRunning = false;
                break;
            }
        }

        Tick();
    }

    return true;
}

void Window::Tick()
{
    Uint8 r, g, b;

    Uint64 startTicks = SDL_GetTicks64();

    std::tie(r, g, b) = m_backgroundColor;
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_RenderClear(m_renderer);

    if (m_audioCapture->IsInitialized() && m_audioTransform->IsInitialized())
    {
        if (m_audioCapture->DidDefaultDeviceChange())
        {
            m_audioCapture->DestroyDefaultDeviceCapture();
            m_audioCapture->InitializeDefaultDeviceCapture();

            m_audioTransform->DestroyFFT();
            m_audioTransform->InitializeFFT();

            m_plot->CalculateSpectrumValues();
        }

        if (m_audioCapture->Capture())
        {
            m_audioTransform->Transform();
            m_plot->Update();
        }
    }

    m_plot->Render();

    SDL_RenderPresent(m_renderer);

    m_frameTime = (Uint32)(SDL_GetTicks64() - startTicks);
    if (GetDeltaTimeTarget() > m_frameTime)
        SDL_Delay((Uint32)std::roundf(GetDeltaTimeTarget() - m_frameTime));
}
