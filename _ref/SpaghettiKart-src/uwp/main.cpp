// SpaghettiKart UWP launcher
// Mirrors worleydl/shipdev -- bridges SDL WinRT into the engine's SDL_main.
#include <Windows.h>
#include "SDL2/SDL.h"

// SDL_main is exported by Spaghettify.dll
extern "C" int SDL_main(int argc, char** argv);
extern "C" __declspec(dllimport) void* uwp_GetWindowReference();

static int bootstrap(int argc, char** argv)
{
    uwp_GetWindowReference(); // Cache CoreWindow reference for other threads
    return SDL_main(argc, argv);
}

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return SDL_WinRTRunApp(bootstrap, NULL);
}
