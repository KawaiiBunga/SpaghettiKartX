#include <Windows.h>
#include <delayimp.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>

#include "SDL2/SDL.h"
#include "bootmenu.h"

static FARPROC WINAPI DliHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
    if (dliNotify == dliNotePreLoadLibrary)
    {
        if (pdli && pdli->szDll)
        {
            int len = MultiByteToWideChar(CP_UTF8, 0, pdli->szDll, -1, nullptr, 0);
            if (len > 0)
            {
                std::wstring wDll(len, 0);
                MultiByteToWideChar(CP_UTF8, 0, pdli->szDll, -1, &wDll[0], len);
                HMODULE h = GetModuleHandleW(wDll.c_str());
                if (!h)
                {
                    h = LoadPackagedLibrary(wDll.c_str(), 0);
                }
                if (h != NULL)
                {
                    return reinterpret_cast<FARPROC>(h);
                }
            }
        }
    }
    return NULL;
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = DliHook;

extern "C" {
    int SDL_main(int argc, char** argv);
    __declspec(dllimport) void* uwp_GetWindowReference();
    __declspec(dllimport) void uwp_set_aux_root(const char* path);
    __declspec(dllimport) const char* uwp_get_aux_root();
}

void LogToLocalState(const std::string& msg) {
    try {
        auto appData = winrt::Windows::Storage::ApplicationData::Current();
        if (appData) {
            auto localFolder = appData.LocalFolder();
            if (localFolder) {
                std::filesystem::path logFile = std::filesystem::path(localFolder.Path().c_str()) / "launch.log";
                std::ofstream out(logFile, std::ios::app);
                out << msg << "\n";
                out.flush();
            }
        }
    } catch (...) {}
}

static void ValidateDllImports(const wchar_t* dllPath) {
    HMODULE hFile = LoadLibraryExW(dllPath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hFile) {
        LogToLocalState("ValidateDllImports: Could not inspect DLL. Error: " + std::to_string(GetLastError()));
        return;
    }
    auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hFile);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        FreeLibrary(hFile);
        return;
    }
    auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(hFile) + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        FreeLibrary(hFile);
        return;
    }
    auto importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.Size == 0 || importDir.VirtualAddress == 0) {
        FreeLibrary(hFile);
        return;
    }
    auto importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(reinterpret_cast<BYTE*>(hFile) + importDir.VirtualAddress);
    while (importDesc->Name != 0) {
        const char* modName = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(hFile) + importDesc->Name);
        HMODULE hMod = LoadLibraryExA(modName, NULL, 0);
        if (!hMod) {
            LogToLocalState(std::string("  [PE Validation] MISSING DEPENDENT DLL: ") + modName + " (Error: " + std::to_string(GetLastError()) + ")");
        } else {
            auto thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hFile) + (importDesc->OriginalFirstThunk ? importDesc->OriginalFirstThunk : importDesc->FirstThunk));
            int missingCount = 0;
            while (thunk->u1.AddressOfData != 0) {
                if (!(thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    auto importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(reinterpret_cast<BYTE*>(hFile) + thunk->u1.AddressOfData);
                    const char* funcName = importByName->Name;
                    FARPROC proc = GetProcAddress(hMod, funcName);
                    if (!proc) {
                        LogToLocalState(std::string("  [PE Validation] MISSING PROC: ") + modName + " :: " + funcName);
                        missingCount++;
                    }
                }
                thunk++;
            }
            if (missingCount == 0) {
                LogToLocalState(std::string("  [PE Validation] All imports OK for: ") + modName);
            }
            FreeLibrary(hMod);
        }
        importDesc++;
    }
    FreeLibrary(hFile);
}

namespace {
    enum class StorageLocation
    {
        LocalState,
        DDrive,
        EDrive
    };

    StorageLocation GetStorageLocation() {
        try {
            auto appData = winrt::Windows::Storage::ApplicationData::Current();
            if (!appData) return StorageLocation::DDrive;
            auto localSettings = appData.LocalSettings();
            if (!localSettings) return StorageLocation::DDrive;
            auto container = localSettings.Containers().TryLookup(L"Settings");
            if (container) {
                auto value = container.Values().TryLookup(L"StorageLocation");
                if (value) {
                    int location = value.as<int>();
                    return static_cast<StorageLocation>(location);
                }
            }
        } catch (...) {
        }
        return StorageLocation::DDrive;
    }

    std::filesystem::path GetAuxRoot() {
        StorageLocation location = GetStorageLocation();

        switch (location) {
            case StorageLocation::LocalState: {
                try {
                    auto appData = winrt::Windows::Storage::ApplicationData::Current();
                    if (!appData) return std::filesystem::path("D:/SpaghettiKart/");
                    auto localFolder = appData.LocalFolder();
                    if (!localFolder) return std::filesystem::path("D:/SpaghettiKart/");
                    std::wstring localPath = localFolder.Path().c_str();
                    std::string auxPath = std::filesystem::path(localPath).string() + "\\SpaghettiKart";
                    return std::filesystem::path(auxPath);
                } catch (...) {
                    return std::filesystem::path("D:/SpaghettiKart/");
                }
            }
            case StorageLocation::DDrive:
                return std::filesystem::path("D:/SpaghettiKart/");
            case StorageLocation::EDrive:
                return std::filesystem::path("E:/SpaghettiKart/");
            default:
                return std::filesystem::path("D:/SpaghettiKart/");
        }
    }

    std::string GetAppBundlePath() {
        try {
            auto pkg = winrt::Windows::ApplicationModel::Package::Current();
            if (pkg) {
                std::wstring wpath = pkg.InstalledLocation().Path().c_str();
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), (int)wpath.length(), NULL, 0, NULL, NULL);
                std::string strTo(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), (int)wpath.length(), &strTo[0], size_needed, NULL, NULL);
                return strTo;
            }
        } catch (...) {}
        return ".";
    }
}

static DWORD g_lastExceptionCode = 0;

static int RunSDLMainSEH(int argc, char** argv)
{
    __try {
        return SDL_main(argc, argv);
    } __except (g_lastExceptionCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

static int CallSetAuxRootSEH(const char* path)
{
    __try {
        uwp_set_aux_root(path);
        return 0;
    } __except (g_lastExceptionCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

static int bootstrap(int argc, char** argv)
{
    LogToLocalState("=== SpaghettiKart bootstrap starting ===");
    uwp_GetWindowReference(); // Cache CoreWindow reference for other threads

    // Pre-load runtime shims into process address space
    LoadPackagedLibrary(L"dxil.dll", 0);
    LoadPackagedLibrary(L"libgallium_wgl.dll", 0);
    LoadPackagedLibrary(L"opengl32.dll", 0);
    LoadPackagedLibrary(L"libuwp.dll", 0);
    LoadPackagedLibrary(L"SDL2.dll", 0);

    // Pre-load Spaghettify.dll via LoadPackagedLibrary to ensure it is in the address space
    HMODULE hSpaghettify = LoadPackagedLibrary(L"Spaghettify.dll", 0);
    if (hSpaghettify) {
        LogToLocalState("LoadPackagedLibrary(Spaghettify.dll) succeeded: handle " + std::to_string(reinterpret_cast<uintptr_t>(hSpaghettify)));
    } else {
        DWORD err = GetLastError();
        LogToLocalState("LoadPackagedLibrary(Spaghettify.dll) failed with error: " + std::to_string(err));
        LogToLocalState("Starting PE import validation to identify missing DLLs/symbols...");
        ValidateDllImports(L"Spaghettify.dll");
    }

    auto auxRoot = GetAuxRoot();
    LogToLocalState("Storage auxRoot: " + auxRoot.string());
    std::error_code ec;
    std::filesystem::create_directories(auxRoot, ec);
    const std::filesystem::path mk64O2rPath = auxRoot / "mk64.o2r";

    // Copy spaghetti.o2r from app bundle to auxRoot if available
    std::string bundleDir = GetAppBundlePath();
    LogToLocalState("App bundle directory: " + bundleDir);
    std::filesystem::path bundledSpaghetti = std::filesystem::path(bundleDir) / "spaghetti.o2r";
    std::filesystem::path auxSpaghetti = auxRoot / "spaghetti.o2r";
    if (std::filesystem::exists(bundledSpaghetti, ec) && !std::filesystem::exists(auxSpaghetti, ec)) {
        std::filesystem::copy_file(bundledSpaghetti, auxSpaghetti, std::filesystem::copy_options::overwrite_existing, ec);
    }

    // Check if mk64.o2r already exists and is valid (>= 1MB)
    bool o2rExists = false;
    if (std::filesystem::exists(mk64O2rPath, ec)) {
        auto o2rSize = std::filesystem::file_size(mk64O2rPath, ec);
        LogToLocalState("Found mk64.o2r at: " + mk64O2rPath.string() + " (size: " + std::to_string(o2rSize) + " bytes)");
        if (!ec && o2rSize >= 1024 * 1024) {
            o2rExists = true;
        }
    } else {
        LogToLocalState("mk64.o2r does not exist yet at: " + mk64O2rPath.string());
    }

    // If still not found, invoke boot menu to prompt user for ROM
    if (!o2rExists) {
        LogToLocalState("Prompting boot menu for ROM selection & extraction...");
        void* windowHandle = uwp_GetWindowReference();
        if (windowHandle != nullptr) {
            int windowWidth = 1920;
            int windowHeight = 1080;

            bool shouldContinue = bootmenu::BootSelect(windowHandle, windowWidth, windowHeight);
            LogToLocalState("bootmenu::BootSelect returned shouldContinue = " + std::string(shouldContinue ? "true" : "false"));
            if (!shouldContinue) {
                LogToLocalState("User exited boot menu or extraction failed. Terminating bootstrap.");
                return 1;
            }

            auxRoot = GetAuxRoot();
            const std::filesystem::path mk64O2rPathAfterBoot = auxRoot / "mk64.o2r";
            if (!std::filesystem::exists(mk64O2rPathAfterBoot, ec)) {
                LogToLocalState("ERROR: mk64.o2r does not exist after boot menu!");
                return 1;
            }
            auto o2rSize = std::filesystem::file_size(mk64O2rPathAfterBoot, ec);
            if (ec || o2rSize < 1024 * 1024) {
                LogToLocalState("ERROR: mk64.o2r size is invalid: " + std::to_string(o2rSize));
                return 1;
            }
            LogToLocalState("mk64.o2r verified after extraction (size: " + std::to_string(o2rSize) + " bytes)");
        } else {
            LogToLocalState("ERROR: windowHandle is null! Cannot launch boot menu.");
            return 1;
        }
    }

    // Ensure logs and mods directories exist
    std::filesystem::create_directories(auxRoot / "logs", ec);
    std::filesystem::create_directories(auxRoot / "mods", ec);

    // Set aux root for the engine so all paths (config, saves, logs, mods, mk64.o2r) route there
    std::string auxRootStr = auxRoot.string();
    LogToLocalState("Setting uwp_set_aux_root: " + auxRootStr);
    if (CallSetAuxRootSEH(auxRootStr.c_str()) != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "FATAL: uwp_set_aux_root threw exception 0x%08X! Delay-load failed.", g_lastExceptionCode);
        LogToLocalState(buf);
        return 1;
    }

    LogToLocalState(">>> Entering SDL_main...");
    int res = RunSDLMainSEH(argc, argv);
    if (res == -1 && g_lastExceptionCode != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "FATAL CRASH in SDL_main! Exception code: 0x%08X", g_lastExceptionCode);
        LogToLocalState(buf);
    } else {
        LogToLocalState("<<< SDL_main completed with code: " + std::to_string(res));
    }
    return res;
}

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return SDL_WinRTRunApp(bootstrap, NULL);
}
