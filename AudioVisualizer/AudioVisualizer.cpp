#include "Window.h"

#include <Windows.h>

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <string>

static std::string const screenSaverExt(".scr");

static std::initializer_list<std::string> const screenSaverConfigSwitches = { "/C", "/c", "\\C", "\\c", "-C", "-c"};
static std::initializer_list<std::string> const screenSaverPreviewSwitches = { "/P", "/p", "\\P", "\\p", "-P", "-p"};
static std::initializer_list<std::string> const screenSaverShowSwitches = { "/S", "/s", "\\S", "\\s", "-S", "-s"};

static void ScreenSaverConfigureDialog()
{
    MessageBox(nullptr,
        L"This screen saver has no options that you can set.", TEXT(WINDOW_NAME),
        MB_OK | MB_ICONINFORMATION);
}

int main(int argc, char** argv)
{
    HRESULT hr;

    if (argc < 1)
        return EXIT_FAILURE;

    std::string program(argv[0]);
    bool isScreenSaver = program.length() >= screenSaverExt.length() &&
        std::equal(screenSaverExt.rbegin(), screenSaverExt.rend(), program.rbegin());

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
        return EXIT_FAILURE;

    std::unique_ptr<Window> window;

    if (isScreenSaver)
    {
        if (argc < 2)
        {
            ScreenSaverConfigureDialog();

            CoUninitialize();
            return EXIT_SUCCESS;
        }

        std::string switchOne(argv[1]);

        if (std::find(screenSaverConfigSwitches.begin(), screenSaverConfigSwitches.end(), switchOne.substr(0, 2)) != screenSaverConfigSwitches.end())
        {
            ScreenSaverConfigureDialog();

            CoUninitialize();
            return EXIT_SUCCESS;
        }
        else if (std::find(screenSaverPreviewSwitches.begin(), screenSaverPreviewSwitches.end(), switchOne.substr(0, 2)) != screenSaverPreviewSwitches.end())
        {
            if (argc < 3)
            {
                CoUninitialize();
                return EXIT_FAILURE;
            }

            std::string switchTwo(argv[2]);

            try
            {
                HWND hWndPreview = (HWND)std::stoll(switchTwo);

                window.reset(new Window(hWndPreview));
            }
            catch (...)
            {
                CoUninitialize();
                return EXIT_FAILURE;
            }
        }
        else if (std::find(screenSaverShowSwitches.begin(), screenSaverShowSwitches.end(), switchOne.substr(0, 2)) != screenSaverShowSwitches.end())
        {
            window.reset(new Window(true));
        }
    }
    else
    {
        window.reset(new Window(false));
    }

    if (!window->IsInitialized())
    {
        CoUninitialize();
        return EXIT_FAILURE;
    }

    if (!window->Run())
    {
        CoUninitialize();
        return EXIT_FAILURE;
    }

    CoUninitialize();
    return EXIT_SUCCESS;
}
