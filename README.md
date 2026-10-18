# Simple Delay

A VST3/standalone audio delay effect built with JUCE 8 and CMake on Windows.

## Prerequisites

- Windows 10/11
- Visual Studio 2022 Build Tools with the C++ workload (MSVC 14.44 or compatible)
- CMake 3.22 or newer
- Git

If `cmake` is not on your PATH, use the CMake bundled with the Visual Studio Build Tools:

```powershell
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
```

## One-time JUCE setup

Clone JUCE 8.0.15 into `libs/JUCE`:

```bash
git clone --depth 1 --branch 8.0.15 --recurse-submodules --shallow-submodules https://github.com/juce-framework/JUCE.git libs/JUCE
```

## Configure

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

## Build

```bash
cmake --build build --config Release --parallel
```

## Build artefacts

After a successful build:

- VST3 plugin: `build/SimpleDelay_artefacts/Release/VST3/Simple Delay.vst3/`
- Standalone app: `build/SimpleDelay_artefacts/Release/Standalone/Simple Delay.exe`

## Installing the plugin

`COPY_PLUGIN_AFTER_BUILD` is disabled in `CMakeLists.txt` so ordinary builds do not require elevation. Install the plugin manually with one of these routes:

1. Copy the `Simple Delay.vst3` folder into your DAW's VST3 search path (commonly `C:\Program Files\Common Files\VST3\`). This requires an elevated shell.
2. Load the built VST3 directly in your DAW by pointing it at `build/SimpleDelay_artefacts/Release/VST3\`.

The standalone executable can be run directly from the build output folder.
