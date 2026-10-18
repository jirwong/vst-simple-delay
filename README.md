# Simple Delay

A VST3/standalone audio delay effect built with JUCE 8 and CMake on Windows.

This is a learning project: a first plugin, written to understand how a host
drives a plugin and how a delay works. See `docs/superpowers/specs/` for the
design.

## Prerequisites

- Windows 10/11
- Visual Studio 2022 Build Tools with the C++ workload (MSVC 14.44 or compatible)
- CMake 3.22 or newer
- Git

### CMake is not on `PATH` on this machine

The project has been built without a system-wide CMake. You have two options.

**Option A — use the CMake bundled with the Visual Studio Build Tools.** It is
already installed and is invoked by its full path:

```powershell
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --version
```

Substitute that full path for `cmake` in every command below.

**Option B — install CMake and open a new shell.**

```powershell
winget install Kitware.CMake
```

After this, open a **new** shell so the updated `PATH` is picked up, and `cmake`
works as written.

## One-time JUCE setup

Clone JUCE 8.0.15 into `libs/JUCE` (`libs/` is git-ignored). The Steinberg VST3 SDK
is vendored inside the JUCE repository, so nothing else needs fetching; the
`--recurse-submodules --shallow-submodules` flags are defensive no-ops as of
8.0.15:

```bash
git clone --depth 1 --branch 8.0.15 --recurse-submodules --shallow-submodules https://github.com/juce-framework/JUCE.git libs/JUCE
```

`CMakeLists.txt` fails the configure with a clear message if `libs/JUCE` is
missing, so this step is hard to forget.

## Configure

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

The generator is **Visual Studio 17 2022** (msbuild), not Ninja, and the platform
must be `x64`. The VS generator locates the MSVC toolchain itself, so run this
from an ordinary shell rather than a `vcvars` one.

## Build

```bash
cmake --build build --config Release --parallel
```

## Build artefacts

After a successful build:

- VST3 plugin: `build/SimpleDelay_artefacts/Release/VST3/Simple Delay.vst3/`
- Standalone app: `build/SimpleDelay_artefacts/Release/Standalone/Simple Delay.exe`

## Running the offline tests

Two test executables are built alongside the plugin. Both print a
`<N> checks, <N> failures` line and return exit code 0 when everything passes:

```bash
build/Tests/DelayLineTests.exe
build/Tests/ProcessorTests.exe
```

- `DelayLineTests.exe` exercises the DSP only. It has no JUCE or audio-device
  dependency, so it needs no setup before running.
- `ProcessorTests.exe` exercises the plugin wrapper (`prepareToPlay`,
  `processBlock`, channel handling, sample-rate changes, state round-trip). It
  links JUCE and starts JUCE's GUI subsystem before running.

Build the tests with the same command as the plugin; they are ordinary targets in
the `build` tree.

## Installing the plugin

`COPY_PLUGIN_AFTER_BUILD` is disabled in `CMakeLists.txt` so ordinary builds do not
require elevation. Install the plugin manually with one of these routes:

1. Copy the `Simple Delay.vst3` folder into your DAW's VST3 search path
   (commonly `C:\Program Files\Common Files\VST3\`). This requires an elevated
   shell.
2. Load the built VST3 directly in your DAW by pointing it at
   `build/SimpleDelay_artefacts/Release/VST3\`.

The standalone executable can be run directly from the build output folder.

## What each file is for

```
CMakeLists.txt                  build definition: the plugin, the two tests, JUCE
README.md                       this file
Source/
  DelayLine.h                   the DSP: a circular buffer, feedback, interpolation.
                                Standard library only — no dependency on JUCE, the
                                plugin API, or an audio device, so it can be tested
                                offline.
  PluginProcessor.h/.cpp        the plugin shell: parameters, smoothing,
                                processBlock, state save/load.
  PluginEditor.h/.cpp           three sliders bound to the parameters through
                                AudioProcessorValueTreeState::SliderAttachment.
Tests/
  Check.h                       shared test macros: CHECK, CHECK_NEAR, and
                                reportAndExit().
  DelayLineTest.cpp             offline DSP tests; no JUCE or plugin includes.
  ProcessorTest.cpp             offline plugin-wrapper tests; includes
                                PluginProcessor.h.
```

`ProcessorTests` compiles both `Source/PluginProcessor.cpp` **and**
`Source/PluginEditor.cpp`, because `createEditor()` in the processor references
the editor. When you add a new source file to the plugin, add it to both targets
in `CMakeLists.txt`: the plugin target and `ProcessorTests`.

## Build configuration notes

Two choices in `CMakeLists.txt` are deliberate — do not lose them when editing:

- `COPY_PLUGIN_AFTER_BUILD FALSE`. Post-build copying into
  `C:\Program Files\Common Files\VST3` needs an elevated shell, which would break
  ordinary command-line builds, so installation is manual (see above).
- JUCE ships no generated `JuceHeader.h` here. Source files include the module
  headers they use directly, e.g.
  `#include <juce_audio_processors/juce_audio_processors.h>`. There is one fewer
  generated-file indirection to reason about while learning.

## Licensing

JUCE 8 is licensed under **AGPLv3 or a commercial licence**. Personal learning use
with no distribution is permitted under AGPLv3. If this project is ever
distributed, a licensing decision must be made deliberately (AGPLv3 obligations, or
purchase a commercial JUCE licence). Recorded here so it is not discovered late.

## Known limitations

- **NaN/infinity from upstream is not sanitized.** The delay line does not check
  its input for NaN or infinity. A NaN arriving from a plugin earlier in the chain
  would enter the feedback buffer and circulate forever, silencing the output. This
  is out of scope for v1; recorded here so it is not a surprise. A fix would be a
  `std::isfinite` guard on the input inside `processSample`.

## Exercise: hand-write the parameter smoother

The processor smooths the Time and Mix parameters with `juce::SmoothedValue` so a
stepped per-block value does not click (see `Source/PluginProcessor.cpp`,
`prepareToPlay` and `processBlock`). To see what that class does internally, write
the smoother by hand as a **one-pole filter**, then compare against
`juce::SmoothedValue`:

1. Replace each `juce::SmoothedValue<float>` with a plain `float current` state.
2. In `prepareToPlay`, compute a per-sample coefficient from the ramp time, e.g.
   `coeff = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.02))` for a ~20 ms
   ramp.
3. In `processBlock`, advance it once per sample:
   `current += coeff * (target - current);` before use.
4. Build and run the offline tests; they should still pass.
5. Put both versions on a Time automation ramp in FL Studio and listen: the
   hand-written one-pole and `SmoothedValue` should sound equivalent.
