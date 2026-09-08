cmake_minimum_required(VERSION 3.15)

# -----------------------------------------------------------------------
# UWP platform -- must be set BEFORE project()
# -----------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME WindowsStore)
set(CMAKE_SYSTEM_VERSION 10.0.26100.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

project(spaghettikart-uwp LANGUAGES CXX)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})

# -----------------------------------------------------------------------
# Paths to the pre-built Win32 engine
# -----------------------------------------------------------------------
set(SK_BUILD_DIR "" CACHE PATH "Path to the SpaghettiKart Win32 CMake build directory")
set(SK_EXE_DIR  "" CACHE PATH "Path to the SpaghettiKart Win32 Release/Debug output directory")

if(NOT SK_BUILD_DIR OR NOT SK_EXE_DIR)
  message(FATAL_ERROR
    "SK_BUILD_DIR and SK_EXE_DIR must point to the Win32 engine build.\n"
    "  -DSK_BUILD_DIR=<path to build-win32>\n"
    "  -DSK_EXE_DIR=<path to build-win32/Release>")
endif()

# -----------------------------------------------------------------------
# Fetch UWP-compatible runtime DLLs (SDL2 UWP, Mesa OpenGL->D3D11, libuwp)
# Sourced from worleydl/uwp-dep -- same set used by the SoH Xbox build.
# -----------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(UwpLibs
  GIT_REPOSITORY https://github.com/worleydl/uwp-dep.git
  GIT_TAG        0ec87d2406b47743772e2787654275a6f6c0e284
)
FetchContent_MakeAvailable(UwpLibs)
set(UWP_DEP_DIR "${uwplibs_SOURCE_DIR}/x64")

# -----------------------------------------------------------------------
# Launcher executable (UWP WIN32 subsystem)
# -----------------------------------------------------------------------
add_executable(${PROJECT_NAME} WIN32
  main.cpp
)

# -----------------------------------------------------------------------
# Binary deps -- engine EXE + UWP runtime shims
# Note: SpaghettiKart builds as an EXE (not DLL), so we bundle it as
# a content file and the launcher just bootstraps SDL WinRT which calls
# into the engine via SDL_main.
# -----------------------------------------------------------------------
set(BinDeps
  # Engine DLL
  "${SK_EXE_DIR}/Spaghettify.dll"

  # Game archives
  "${SK_EXE_DIR}/mk64.o2r"
  "${SK_EXE_DIR}/spaghetti.o2r"

  # UWP OpenGL/SDL2/zlib shims
  "${UWP_DEP_DIR}/bin/dxil.dll"
  "${UWP_DEP_DIR}/bin/glfw3.dll"
  "${UWP_DEP_DIR}/bin/libgallium_wgl.dll"
  "${UWP_DEP_DIR}/bin/libuwp.dll"
  "${UWP_DEP_DIR}/bin/opengl32.dll"
  "${UWP_DEP_DIR}/bin/SDL2.dll"
  "${UWP_DEP_DIR}/bin/z-1.dll"

  # Manifest
  Package.appxmanifest
)

source_group("BinDeps" FILES ${BinDeps})
set_source_files_properties(${BinDeps} PROPERTIES
  VS_COPY_TO_OUT_DIR Always
  VS_DEPLOYMENT_CONTENT TRUE
  VS_DEPLOYMENT_LOCATION "."
)

# -----------------------------------------------------------------------
# UWP package assets (logos/splash -- required by the manifest)
# -----------------------------------------------------------------------
set(PkgAssets
  Assets/LockScreenLogo.scale-200.png
  Assets/Square44x44Logo.targetsize-24_altform-unplated.png
  Assets/Square44x44Logo.scale-200.png
  Assets/Square150x150Logo.scale-200.png
  Assets/Wide310x150Logo.scale-200.png
  Assets/SplashScreen.scale-200.png
  Assets/StoreLogo.png
)
source_group("PkgAssets" FILES ${PkgAssets})
set_source_files_properties(${PkgAssets} PROPERTIES
  VS_DEPLOYMENT_CONTENT TRUE
  VS_DEPLOYMENT_LOCATION "Assets"
)

# -----------------------------------------------------------------------
# Link
# -----------------------------------------------------------------------
target_sources(${PROJECT_NAME} PRIVATE
  ${BinDeps}
  ${PkgAssets}
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${UWP_DEP_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  "${SK_EXE_DIR}/Spaghettify.lib"
  "${UWP_DEP_DIR}/lib/libuwp.lib"
  "${UWP_DEP_DIR}/lib/SDL2.lib"
  WindowsApp.lib
  OneCore.lib
)

# -----------------------------------------------------------------------
# UWP target platform version
# -----------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES
  VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION 10.0.19041.0
)
