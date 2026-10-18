# Simple Delay VST3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a working stereo delay VST3 plugin for Windows that loads in FL Studio, so the author can learn audio DSP and plugin architecture first-hand.

**Architecture:** One CMake project produces a VST3 and a Standalone app from the same source. A hand-written `DelayLine` class (circular buffer + feedback + linear interpolation, no JUCE dependency) holds all the DSP and is tested offline. A `juce::AudioProcessor` subclass wraps it with parameters, smoothing, mixing, and host callbacks. A minimal editor binds three sliders to parameters.

**Tech Stack:** C++17, JUCE 8, CMake, MSVC (Visual Studio 17 2022 generator), FL Studio 2025 for in-host verification.

**Spec:** `docs/superpowers/specs/2026-10-19-simple-delay-design.md`

## Global Constraints

- Platform: Windows x64 only. No macOS/Linux/AU/AAX/VST2 work.
- C++ standard: C++17. Allowed dependencies: JUCE and the C++ standard library. Nothing else.
- JUCE: version 8, pinned to an exact release tag. Fetched once into `libs/JUCE`, which is gitignored.
- CMake: minimum version 3.22. Generator: `Visual Studio 17 2022`, platform `x64`. Do not use Ninja.
- Parameter IDs are the stable strings `"time"`, `"feedback"`, `"mix"`. They are saved into user projects and must not change.
- Parameter ranges and defaults, exact: `time` 1.0–2000.0 ms, default 375.0, skew 0.25; `feedback` 0.0–0.95, default 0.40; `mix` 0.0–1.0, default 0.35.
- Delay buffer capacity is `maxDelayMs = 2000`. Feedback is clamped to `[0, 0.95]` in two places: the parameter range and `DelayLine::setFeedback`.
- `processBlock` must not allocate, lock, or perform string/file I/O. `DelayLine::prepare` is called only from `prepareToPlay`.
- The plugin must not report latency via `setLatencySamples`. A delay is the audible effect; reporting it as latency would make the host compensate and cancel it.
- Company name `jirwo`, product name `Simple Delay`, CMake base target `SimpleDelay` (JUCE derives `SimpleDelay_VST3` and `SimpleDelay_Standalone`).
- No build warnings from `juce_recommended_warning_flags` in code we write.
- Include JUCE module headers directly, for example `#include <juce_audio_utils/juce_audio_utils.h>`. Do not introduce a generated `JuceHeader.h`.
- Build commands in this plan assume the VS generator's default multi-config layout. Test executables are pinned to `build/Tests/` by an explicit target property so every path in this plan is deterministic.

## Review Focus

Failure modes the spec implies but no single task's tests obviously cover. Each is pinned by a test in the task that owns the code:

1. **Mono input, stereo output** — the host aliases the absent input channel to the output buffer, so an unchecked plugin emits stale buffer contents (or last pass's audio) in the right channel. Expected: right channel is silent. Test in Task 5; the clearing code is Task 4.
2. **Delay time driven out of range by automation** — a host curve can push `time` to its extremes or a host can hand back a sample count beyond the buffer. Expected: the read position stays inside the buffer. Tests in Task 2.
3. **Feedback at its maximum with a continuous input** — the loop must stay bounded at 0.95 rather than grow. Test in Task 3.
4. **The host calls `prepareToPlay` again at a different sample rate** — the delay must still be the requested number of milliseconds, and stale audio must not survive. Tests in Task 3 (96 kHz) and Task 5 (re-prepare).
5. **Project save and reload** — the three knob values must come back exactly. Test in Task 5.

Deliberately not covered: sanitizing NaN or infinity entering from an upstream plugin. Documented as a known limitation in the README during Task 7.

---

## Task 1: Project scaffold — a plugin that builds, loads, and makes sound

Closes the entire pipeline (configure → build → load → hear → parameter reaches the audio thread) using no DSP, so any failure here is unambiguously a build or host-integration problem.

**Files:**
- Create: `CMakeLists.txt`
- Create: `.gitignore`
- Create: `README.md`
- Create: `Source/PluginProcessor.h`
- Create: `Source/PluginProcessor.cpp`

Task 1 deliberately writes **no editor class**: `createEditor()` returns `juce::GenericAudioProcessorEditor`, one line, zero UI code. Task 6 replaces it with the real editor and creates `Source/PluginEditor.h/.cpp` then.

**Interfaces:**
- Consumes: nothing.
- Produces: `createParameterLayout()`, the class `SimpleDelayAudioProcessor`, its member `juce::AudioProcessorValueTreeState apvts`, the parameter IDs `"time"`, `"feedback"`, `"mix"`, and the type `juce::AudioProcessorValueTreeState::SliderAttachment` usage pattern. Task 4 and Task 6 rely on all of these.

- [ ] **Step 1: Install CMake and confirm it is on PATH**

```bash
winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
```

Then, **in a new shell** (winget updates PATH, and the current shell will not see it):

```bash
cmake --version
```

Expected: `cmake version 3.2x.x` or higher, at least 3.22. If the new shell still does not find it, note the absolute path and use it everywhere below.

- [ ] **Step 2: Identify the exact JUCE release tag to pin**

```bash
git ls-remote --tags --refs https://github.com/juce-framework/JUCE.git | grep -o 'refs/tags/[0-9.]*$' | sort -V | tail -5
```

Expected: a list of version tags. Choose the highest `8.x.y` that contains no suffix (ignore any `-` prerelease tag). Call it `<JUCE_TAG>` for the rest of this plan.

- [ ] **Step 3: Fetch JUCE once into `libs/JUCE`**

```bash
git clone --depth 1 --branch <JUCE_TAG> --recurse-submodules --shallow-submodules https://github.com/juce-framework/JUCE.git libs/JUCE
```

This recurse step matters: the Steinberg VST3 SDK is a git submodule, and without it VST3 builds fail with a missing-header error.

Verify: `ls libs/JUCE/modules/juce_audio_processors/format_types/VST3_SDK` is not empty.

- [ ] **Step 4: Write `.gitignore` and a first `README.md`**

```gitignore
build/
libs/
.vs/
*.vst3
*.user
.superpowers/
```

`.superpowers/` holds the SDD workspace (ledger and task briefs). It is git-ignored scratch, so it is not part of the deliverable. `.gitignore` may already exist with exactly this content from workspace setup; if so, leave it alone.

`README.md` starts minimal and Task 7 expands it. At this point it must contain: the prerequisites, the one-time JUCE clone command from Step 3 with its pinned tag, the configure and build commands from Step 9, the two artefact paths from Step 10, and both install routes described in Step 5. `CMakeLists.txt` aborts with a message pointing at this file, so a two-line stub is not enough.

- [ ] **Step 5: Write `CMakeLists.txt`**

Exact content — every argument below is one the implementer cannot reasonably guess:

```cmake
cmake_minimum_required(VERSION 3.22)

project(SimpleDelay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at libs/JUCE. See README.md for the one-time clone command.")
endif()

add_subdirectory(libs/JUCE)

juce_add_plugin(SimpleDelay
    COMPANY_NAME "jirwo"
    PLUGIN_MANUFACTURER_CODE Jirw
    PLUGIN_CODE Sdly
    PRODUCT_NAME "Simple Delay"
    FORMATS Standalone VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SimpleDelay PRIVATE
    Source/PluginProcessor.cpp)

target_include_directories(SimpleDelay PRIVATE Source)

target_compile_definitions(SimpleDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SimpleDelay PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
```

`COPY_PLUGIN_AFTER_BUILD FALSE` is deliberate: copying into `C:\Program Files\Common Files\VST3` requires an elevated shell and would make ordinary builds fail. README.md documents both install routes instead.

- [ ] **Step 6: Write `Source/PluginProcessor.h`**

Declares `SimpleDelayAudioProcessor : public juce::AudioProcessor`, all in the `public:` section — Tasks 5 and 6 reach these from outside the class:

```cpp
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
juce::AudioProcessorValueTreeState apvts;
static constexpr const char* timeID     = "time";
static constexpr const char* feedbackID = "feedback";
static constexpr const char* mixID      = "mix";
```

Override `getStateInformation` and `setStateInformation` (Task 4 fills them in; declare them now).

The constructor must pass a `BusesProperties` object describing stereo in and stereo out; without it `getTotalNumInputChannels()` returns 0 and nothing is processed at all:

```cpp
SimpleDelayAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}
```

Include JUCE modules directly rather than a generated `JuceHeader.h`: `#include <juce_audio_processors/juce_audio_processors.h>` in the header of a file that only needs the processor API, and `#include <juce_audio_utils/juce_audio_utils.h>` wherever `GenericAudioProcessorEditor` is used.

- [ ] **Step 7: Write `Source/PluginProcessor.cpp` with the three parameters**

`createParameterLayout()` builds a `std::vector<std::unique_ptr<juce::RangedAudioParameter>>` containing exactly:

- `juce::AudioParameterFloat` ID `"time"`, name `"Time"`, range `juce::NormalisableRange<float> (1.0f, 2000.0f, 0.01f, 0.25f)`, default `375.0f`, label `"ms"`.
- `juce::AudioParameterFloat` ID `"feedback"`, name `"Feedback"`, range `juce::NormalisableRange<float> (0.0f, 0.95f, 0.001f)`, default `0.40f`.
- `juce::AudioParameterFloat` ID `"mix"`, name `"Mix"`, range `juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f)`, default `0.35f`.

Construct `apvts` as `juce::AudioProcessorValueTreeState (*this, nullptr, "PARAMETERS", createParameterLayout())`.

`processBlock` for this task implements a **temporary** gain, so the pipeline is audible end to end:

```cpp
juce::ScopedNoDenormals noDenormals;
const auto timeMs = apvts.getRawParameterValue (timeID)->load();
const auto gain   = juce::jlimit (0.0f, 1.0f, timeMs / 1000.0f);
for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    buffer.applyGain (ch, 0, buffer.getNumSamples(), gain);
```

Mark this block with the comment `// TEMPORARY scaffold: replaced by the delay line in Task 4.`

`prepareToPlay` is empty for now. `isBusesLayoutSupported` returns the spec's rule from §6.4: input and output each `mono` or `stereo`, and input channels ≤ output channels.

- [ ] **Step 8: Make `createEditor()` return the generic editor**

In `Source/PluginProcessor.cpp`:

```cpp
juce::AudioProcessorEditor* SimpleDelayAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
```

with `#include <juce_audio_utils/juce_audio_utils.h>` at the top of the file. This builds real controls for all three parameters with no UI code of our own, which is all Task 1 needs, and it is deleted outright in Task 6.

- [ ] **Step 9: Configure and build**

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Expected: finishes with `Build succeeded` / exit code 0. The first build compiles all of JUCE and takes several minutes; subsequent builds are fast.

- [ ] **Step 10: Verify both artifacts exist**

```bash
ls "build/SimpleDelay_artefacts/Release/VST3/"
ls "build/SimpleDelay_artefacts/Release/Standalone/"
```

Expected: `Simple Delay.vst3/` and `Simple Delay.exe` respectively.

- [ ] **Step 11: Verify it makes sound**

Run `build/SimpleDelay_artefacts/Release/Standalone/"Simple Delay.exe"`. In Options → Audio/MIDI Settings, select an output device. Open the plugin's own settings to confirm audio flows.

Expected: with any audio passing through the standalone, reducing the Time slider makes the output quieter and raising it makes it louder. Silence or no change means the pipeline is broken — fix it before continuing, because every later task depends on this loop working.

- [ ] **Step 12: Commit**

```bash
git add .gitignore CMakeLists.txt README.md Source
git commit -m "Add plugin scaffold with three parameters and temporary gain"
```

---

## Task 2: `DelayLine` core — integer delay, feedback, clamping

**Files:**
- Create: `Source/DelayLine.h`
- Create: `Tests/Check.h`
- Create: `Tests/DelayLineTest.cpp`
- Modify: `CMakeLists.txt` (add the test target)

**Interfaces:**
- Consumes: nothing.
- Produces: class `DelayLine`, and the test helper `impulseResponse` that Task 3 reuses.

```cpp
class DelayLine
{
public:
    void  prepare (double sampleRate, float maxDelayMs);
    void  reset();
    void  setDelaySamples (float samples);
    void  setFeedback (float feedback);
    float processSample (float input);
    int          getBufferSize() const noexcept;
    const float* bufferData()    const noexcept;
};
```

- [ ] **Step 1: Add the test target to `CMakeLists.txt`**

```cmake
add_executable(DelayLineTests Tests/DelayLineTest.cpp)
target_include_directories(DelayLineTests PRIVATE Source)
target_compile_features(DelayLineTests PRIVATE cxx_std_17)
set_target_properties(DelayLineTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/Tests"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Tests"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/Tests")
```

This target links neither JUCE nor anything else. That is the point: the DSP is testable in under a second. The `RUNTIME_OUTPUT_DIRECTORY_*` properties override the multi-config generator's default `build/Release/` layout, so the executable is always at `build/Tests/DelayLineTests.exe` and every command later in this plan is exact.

- [ ] **Step 2: Write the failing test**

Create `Tests/Check.h` — the shared harness for both test executables. It includes only the C++ standard library, never JUCE, so the delay tests keep their zero-dependency property:

```cpp
#pragma once

#include <cmath>
#include <cstdio>

inline int gChecks = 0;
inline int gFailures = 0;

#define CHECK(cond) do { ++gChecks; if (!(cond)) { ++gFailures; \
    std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_NEAR(a, b, tol) do { ++gChecks; \
    const double _a = (a), _b = (b); \
    if (std::fabs (_a - _b) > (tol)) { ++gFailures; \
        std::printf ("FAIL %s:%d  %s = %.9g, expected %.9g\n", \
                     __FILE__, __LINE__, #a, _a, _b); } } while (0)

inline int reportAndExit()
{
    std::printf ("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
```

`inline` variables are C++17, which the project already requires. Then create `Tests/DelayLineTest.cpp`, which includes `Check.h` from the same directory (no extra include path is needed) and holds the first test:

```cpp
#include "Check.h"
#include "DelayLine.h"

#include <vector>

static constexpr double kSampleRate = 48000.0;
static constexpr float  kMaxDelayMs = 2000.0f;

// Sends an impulse (1.0 then zeros) through the line and returns the output.
static std::vector<float> impulseResponse (DelayLine& line, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        out[(size_t) i] = line.processSample (i == 0 ? 1.0f : 0.0f);
    return out;
}

int main()
{
    // --- the delay is the length it claims ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 400);

        for (int i = 0; i < 100; ++i)
            CHECK_NEAR (out[(size_t) i], 0.0, 1e-9);
        CHECK_NEAR (out[100], 1.0, 1e-6);
    }

    return reportAndExit();
}
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cmake --build build --config Release --target DelayLineTests
```

Expected: FAILS TO COMPILE with `Cannot open include file: 'DelayLine.h'`. A compile error is the correct failure here — the class does not exist yet.

- [ ] **Step 4: Implement `Source/DelayLine.h`**

Header-only, standard library only. Members: `std::vector<float> buffer`, `int bufferSize`, `int writeIndex`, `float delaySamples`, `float feedback`. All with in-class initializers, but note that **`prepare()` must be called before `processSample()`** — before `prepare`, `bufferSize` is 0 and the wrap modulo would divide by zero.

- `prepare(sampleRate, maxDelayMs)` — allocate `bufferSize = (int) std::ceil (sampleRate * maxDelayMs / 1000.0) + 4`, fill with zeros, `writeIndex = 0`.
- `reset()` — `std::fill` the buffer with zeros, `writeIndex = 0`.
- `setDelaySamples(samples)` — clamp to `[1.0f, (float) (bufferSize - 4)]`.
- `setFeedback(fb)` — clamp to `[0.0f, 0.95f]`.
- `getBufferSize()` / `bufferData()` — read-only accessors.

`processSample`, in exactly this order (this ordering is the algorithm; do not reorder):

```cpp
const float readIndexF = wrap ((float) writeIndex - delaySamples);
const int   i0  = (int) readIndexF;
const float out = buffer[(size_t) i0];      // integer read for this task
buffer[(size_t) writeIndex] = input + out * feedback;
writeIndex = (writeIndex + 1) % bufferSize;
return out;
```

where `wrap` adds `bufferSize` while the value is negative.

- [ ] **Step 5: Run the test to verify it passes**

```bash
cmake --build build --config Release --target DelayLineTests
./build/Tests/DelayLineTests.exe
```

Expected: `N checks, 0 failures`, exit code 0. Confirm the exit code with `echo $?` — the harness returning non-zero on failure is what makes this usable in a build pipeline.

- [ ] **Step 6: Add the feedback-decay test**

Appends a second block to `main()`, before `return reportAndExit();`:

```cpp
    // --- feedback math produces a geometric decay, not just "several echoes" ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.5f);
        auto out = impulseResponse (line, 500);

        CHECK_NEAR (out[100], 1.0,  1e-6);   // the impulse itself
        CHECK_NEAR (out[200], 0.5,  1e-6);
        CHECK_NEAR (out[300], 0.25, 1e-6);
        CHECK_NEAR (out[400], 0.125, 1e-6);
    }
```

- [ ] **Step 7: Run the test to verify it passes**

Same command as Step 5. Expected: `0 failures`. If `out[200]` is wrong, the read is happening *after* the write instead of before — see Step 4.

- [ ] **Step 8: Add the clamping and reset tests** (Review Focus item 2)

```cpp
    // --- an out-of-range delay time cannot read outside the buffer ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (1.0e9f);            // absurd, as automation might send
        line.setFeedback (0.5f);
        auto out = impulseResponse (line, 1000);
        for (auto v : out) CHECK (std::isfinite (v));

        line.setDelaySamples (0.0f);              // and the other extreme
        auto out2 = impulseResponse (line, 1000);
        for (auto v : out2) CHECK (std::isfinite (v));
    }

    // --- feedback above the cap is clamped, and the cap is real ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (5.0f);                  // a bad automation value
        auto out = impulseResponse (line, 500);
        CHECK_NEAR (out[100], 1.0,  1e-6);
        CHECK_NEAR (out[200], 0.95, 1e-6);        // clamped, not 5.0
    }

    // --- reset clears state ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.8f);
        impulseResponse (line, 2000);

        line.reset();
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 300);
        for (int i = 0; i < 300; ++i)
            CHECK_NEAR (out[(size_t) i], i == 100 ? 1.0 : 0.0, 1e-9);
    }
```

- [ ] **Step 9: Run the tests to verify they pass**

Same command as Step 5. Expected: `0 failures`. The reference implementation reaches 0 failures on the first run; a failure here means `setFeedback` or `setDelaySamples` is not clamping.

- [ ] **Step 10: Add the no-allocation test**

```cpp
    // --- nothing allocates in the audio path ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        const float* before = line.bufferData();
        const int    sizeBefore = line.getBufferSize();
        for (int i = 0; i < 1000; ++i)
            line.processSample (0.1f);
        CHECK (line.bufferData() == before);
        CHECK (line.getBufferSize() == sizeBefore);
    }
```

- [ ] **Step 11: Run the full test suite**

Same command as Step 5. Expected: `0 failures`, and the reported check count grows with each block added.

- [ ] **Step 12: Commit**

```bash
git add CMakeLists.txt Tests/DelayLineTest.cpp Source/DelayLine.h
git commit -m "Add DelayLine with integer delay, feedback, and clamping"
```

---

## Task 3: Fractional delay and stability

Interpolation is what stops the delay from clicking when Time moves. The stability and sample-rate tests pin the two remaining Review Focus items.

**Files:**
- Modify: `Source/DelayLine.h`
- Modify: `Tests/DelayLineTest.cpp`

**Interfaces:**
- Consumes: everything from Task 2, including the `impulseResponse` helper and the CHECK macros.
- Produces: same public interface as Task 2 — no signatures change. `processSample` gains interpolation internally.

- [ ] **Step 1: Add the failing interpolation test**

```cpp
    // --- a fractional delay splits energy across the two adjacent samples ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.5f);
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 300);

        CHECK_NEAR (out[100], 0.5, 1e-3);
        CHECK_NEAR (out[101], 0.5, 1e-3);
        CHECK_NEAR (out[100] + out[101], 1.0, 1e-3);   // energy is preserved
        CHECK_NEAR (out[99],  0.0, 1e-3);
        CHECK_NEAR (out[102], 0.0, 1e-3);
    }
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build build --config Release --target DelayLineTests && ./build/Tests/DelayLineTests.exe
```

Expected: FAIL on the `out[100]` check — with an integer read, the whole impulse lands on one sample (`1.0` at index 100 and `0` at 101, or vice versa).

- [ ] **Step 3: Add interpolation to `processSample`**

Replace the integer read from Task 2 Step 4:

```cpp
const float readIndexF = wrap ((float) writeIndex - delaySamples);
const int   i0   = (int) readIndexF;
const int   i1   = (i0 + 1) % bufferSize;
const float frac = readIndexF - (float) i0;
const float out  = buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
buffer[(size_t) writeIndex] = input + out * feedback;
writeIndex = (writeIndex + 1) % bufferSize;
return out;
```

`i1` must wrap, because at the moment the read head sits just before the write head, `i0 + 1` is the write index itself and `i0 + 1 == bufferSize` is possible. The `+ 4` samples of slack in the buffer size and the clamp to `bufferSize - 4` are what make `i1` always a valid, unwritten index.

- [ ] **Step 4: Run the test to verify it passes**

Same command as Step 2. Expected: `0 failures`, including the earlier integer-delay tests, which must still pass — an integer delay is the `frac == 0` case.

- [ ] **Step 5: Add the sample-rate test** (Review Focus item 4)

```cpp
    // --- the delay is the same number of samples regardless of sample rate ---
    {
        DelayLine line;
        line.prepare (96000.0, kMaxDelayMs);
        line.setDelaySamples (1000.0f);
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 2000);

        CHECK_NEAR (out[999],  0.0, 1e-9);
        CHECK_NEAR (out[1000], 1.0, 1e-6);
    }
```

- [ ] **Step 6: Add the stability test** (Review Focus item 3)

```cpp
    // --- maximum feedback decays instead of running away ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.95f);

        float peak = 0.0f, tail = 0.0f;
        const int total = (int) (kSampleRate * 10.0);   // ten seconds
        for (int i = 0; i < total; ++i)
        {
            const float v = line.processSample (i == 0 ? 1.0f : 0.0f);
            peak = std::max (peak, std::fabs (v));
            tail = std::fabs (v);
        }
        CHECK (std::isfinite (peak));
        CHECK (peak < 2.0f);
        CHECK (tail < 1e-4f);
    }
```

This block needs `#include <algorithm>` for `std::max`. Do not call `CHECK` inside the ten-second loop — it would run 480 000 times and bury a real failure in output. The `std::isfinite (peak)` check catches a non-finite value only if the peak itself became non-finite; to catch a transient infinity that later recovers, add an `int nonFinite = 0;` counter incremented when `! std::isfinite (v)` and `CHECK (nonFinite == 0)` after the loop.

- [ ] **Step 7: Add the time-sweep test**

```cpp
    // --- moving the delay time mid-run produces no NaN or infinity ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setFeedback (0.5f);
        for (int i = 0; i < 48000; ++i)
        {
            line.setDelaySamples (1.0f + (float) i);     // sweep across the whole range
            CHECK (std::isfinite (line.processSample (0.5f)));
        }
    }
```

- [ ] **Step 8: Run the full suite**

Same command as Step 2. Expected: `0 failures`. This is the complete offline DSP suite from spec §7.1.

- [ ] **Step 9: Commit**

```bash
git add Source/DelayLine.h Tests/DelayLineTest.cpp
git commit -m "Add fractional delay interpolation and stability tests"
```

---

## Task 4: Wire the delay into the plugin

Replaces the temporary gain with the real signal path. This is where the real-time rules stop being theory.

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `DelayLine` (all of Task 2 and 3), `apvts` and the parameter IDs from Task 1.
- Produces: a plugin whose three parameters control a working delay. Task 5 tests this class directly; Task 6's editor binds to the same `apvts`.

- [ ] **Step 1: Add the DSP members to `PluginProcessor.h`**

```cpp
std::vector<DelayLine> delayLines;
juce::SmoothedValue<float> timeSmoother;   // in samples, not ms
juce::SmoothedValue<float> mixSmoother;
double currentSampleRate = 44100.0;
```

- [ ] **Step 2: Implement `prepareToPlay`**

```cpp
currentSampleRate = sampleRate;

const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
delayLines.resize ((size_t) numChannels);
for (auto& line : delayLines)
    line.prepare (sampleRate, 2000.0f);        // spec §5.1: 2000 ms of capacity

timeSmoother.reset (sampleRate, 0.02);         // 20 ms ramp
mixSmoother.reset (sampleRate, 0.02);
```

`resize` here is correct precisely because `prepareToPlay` is not the audio callback. Never do this in `processBlock`. Note that `prepareToPlay` may be called again with a different sample rate — the loop re-runs `prepare`, which reallocates and zeroes each buffer, so no stale audio survives.

- [ ] **Step 3: Implement `processBlock`**

Replace the temporary-gain block entirely, and delete its `// TEMPORARY scaffold` comment.

```cpp
juce::ScopedNoDenormals noDenormals;

const int numIn      = getTotalNumInputChannels();
const int numOut     = getTotalNumOutputChannels();
const int numSamples = buffer.getNumSamples();

// Channels with no input channel aliased to them hold stale data. Clear them.
for (int ch = numIn; ch < numOut; ++ch)
    buffer.clear (ch, 0, numSamples);

const float timeMs   = apvts.getRawParameterValue (timeID)->load();
const float feedback = apvts.getRawParameterValue (feedbackID)->load();
const float mix      = apvts.getRawParameterValue (mixID)->load();

timeSmoother.setTargetValue (timeMs * 0.001f * (float) currentSampleRate);
mixSmoother.setTargetValue (mix);

const int numActive = juce::jmin (numIn, numOut);
for (auto& line : delayLines)
    line.setFeedback (feedback);

for (int i = 0; i < numSamples; ++i)
{
    const float delayNow = timeSmoother.getNextValue();
    const float mixNow   = mixSmoother.getNextValue();

    for (int ch = 0; ch < numActive; ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        delayLines[(size_t) ch].setDelaySamples (delayNow);
        const float wet = delayLines[(size_t) ch].processSample (x[i]);
        x[i] = x[i] * (1.0f - mixNow) + wet * mixNow;
    }
}
```

Four things in there are load-bearing and must not be simplified away:

- Parameter values are read **once per block**, not per sample. `getRawParameterValue` returns a pointer to an atomic; calling it per sample would mean a lookup per sample.
- `setFeedback` is called once per block rather than per sample, because it does not need smoothing and the loop is per-sample.
- The `numIn`/`numOut` clearing loop is Review Focus item 1.
- `x[i]` is read for the dry signal and written once, so the in-place mix is safe.

- [ ] **Step 4: Implement state save and load**

```cpp
void SimpleDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SimpleDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
```

Do **not** call `setLatencySamples`. Reporting the delay as latency would make the host compensate for it and cancel the effect.

- [ ] **Step 5: Build and confirm the DSP tests still pass**

```bash
cmake --build build --config Release --parallel
./build/Tests/DelayLineTests.exe
```

Expected: build succeeds with no warnings from our code; `0 failures`.

- [ ] **Step 6: Verify the delay is audible**

Run `build/SimpleDelay_artefacts/Release/Standalone/"Simple Delay.exe"` with an audio device selected.

Expected, by ear:
- At Mix above zero, repeats are audible behind the source.
- Feedback increases the number of repeats; at maximum it rings for a long time but decays.
- Time changes the gap between repeats, and dragging Time produces a glide rather than a click.

If the output is silent, check `numIn` first: a standalone with no input configured will silence everything, and that is a device setting, not a bug.

- [ ] **Step 7: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "Wire delay line into processBlock with parameter smoothing"
```

---

## Task 5: Offline tests for the plugin wrapper

The DSP is proven; this proves the wrapper. It pins the three failure modes that only exist at the plugin level: mono-in/stereo-out silence, repeated `prepareToPlay`, and state round-trip.

This task adds a JUCE-linked console app. It is an addition to spec §7, which only specified offline tests for `DelayLine` — the spec's success criteria 5 (state restores) and §6.5 (aliased channels) would otherwise rest on manual testing alone.

**Files:**
- Create: `Tests/ProcessorTest.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `SimpleDelayAudioProcessor` from Task 1 and Task 4 — its constructor, `prepareToPlay`, `processBlock`, `getStateInformation`, `setStateInformation`, `apvts`, and the parameter IDs.
- Produces: nothing later tasks depend on.

- [ ] **Step 1: Add the console target**

```cmake
juce_add_console_app (ProcessorTests PRODUCT_NAME "ProcessorTests")
target_sources (ProcessorTests PRIVATE Tests/ProcessorTest.cpp)
target_include_directories (ProcessorTests PRIVATE Source)
target_compile_definitions (ProcessorTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries (ProcessorTests PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
set_target_properties (ProcessorTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/Tests"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Tests"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/Tests")
```

`juce_add_console_app` needs `juce::juce_audio_utils` for the `AudioProcessor` base class, but no audio device is opened — the tests call `prepareToPlay` and `processBlock` directly. `PRODUCT_NAME` is set to `ProcessorTests` so the executable is `build/Tests/ProcessorTests.exe`; a display name with a space would change the filename and break the commands below.

- [ ] **Step 2: Write the failing tests**

`Tests/ProcessorTest.cpp` includes `Check.h` for the same `CHECK` / `CHECK_NEAR` macros and `reportAndExit()` convention as `DelayLineTest.cpp`. Its `main()` must construct `juce::ScopedJuceInitialiser_GUI` as its first statement, before any processor is created: this binary links the GUI modules through `juce_audio_utils`, and JUCE's subsystems are not initialised otherwise. All assertions use one processor instance per block:

```cpp
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ...
    return reportAndExit();
}
```

```cpp
// --- mono in, stereo out: the silent channel stays silent ---
{
    SimpleDelayAudioProcessor p;
    p.setPlayConfigDetails (1, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, 512);
    buf.clear();
    buf.applyGain (0, 0, 512, 0.5f);      // signal on the left only
    juce::MidiBuffer midi;
    p.processBlock (buf, midi);

    for (int i = 0; i < 512; ++i)
        CHECK_NEAR (buf.getSample (1, i), 0.0, 1e-6);
}
```

Then, in the same file:

- **Mix at 0 is dry passthrough** — set `mix` to 0 via `p.apvts.getParameter ("mix")->setValueNotifyingHost (0.0f)`, feed a constant, assert output equals input within 1e-6. This also catches a delay line that has been wired in backwards.
- **Mix at 1 with silence in gives silence out** — set `mix` to 1, feed silence for one second, assert no output exceeds 1e-6. Catches a buffer that is noisy rather than zeroed.
- **Repeated `prepareToPlay` at a new sample rate** (Review Focus item 4) — prepare at 44100, process a block, then prepare at 96000 and process again; assert all output is finite, and that a 100 ms delay still arrives 9600 samples later at 96 kHz. To set `time` to exactly 100 ms, go through the parameter's own range rather than guessing a normalized value, because the range is skewed:

```cpp
auto* timeParam = dynamic_cast<juce::AudioParameterFloat*> (p.apvts.getParameter ("time"));
CHECK (timeParam != nullptr);
timeParam->setValueNotifyingHost (timeParam->range.convertTo0to1 (100.0f));
```

Then send an impulse and locate the peak of the output; it must be within a few samples of index 9600 at 96 kHz, and near 4410 at 44.1 kHz.

- **State round-trip** (Review Focus item 5) — set `time`/`feedback`/`mix` to identifiable values through `setValueNotifyingHost`, call `getStateInformation` into a `juce::MemoryBlock`, construct a *second* processor, call its `setStateInformation` with that block, and assert all three `getRawParameterValue (...)->load()` results match the originals within 1e-6.

- [ ] **Step 3: Run the tests to verify they fail**

```bash
cmake --build build --config Release --target ProcessorTests
```

Expected: the target builds, and then at least one assertion fails. These are characterization tests for code that already exists, so a compile error is possible (a wrong API name) but a clean build is the normal outcome. **If every check passes on the first run, prove the tests can fail before trusting them:** temporarily insert `CHECK (false);` at the top of `main`, rebuild, confirm the failure, then remove it.

- [ ] **Step 4: Fix whatever the tests expose**

Expected fixes are small and localized: a channel loop bound, the parameter ID string, or a smoother not reset in `prepareToPlay`. Fixing wiring like that is the point of this task. What is not acceptable is changing the delay algorithm (`DelayLine`) to make a wrapper test pass — if a test appears to demand that, the test is wrong; say so and fix the test.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
./build/Tests/ProcessorTests.exe
```

Expected: `0 failures`, exit code 0.

- [ ] **Step 6: Confirm the whole suite runs clean**

```bash
./build/Tests/DelayLineTests.exe && ./build/Tests/ProcessorTests.exe
```

Expected: both report `0 failures`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt Tests/ProcessorTest.cpp
git commit -m "Add offline tests for channel handling and state round-trip"
```

---

## Task 6: The editor

**Files:**
- Create: `Source/PluginEditor.h`
- Create: `Source/PluginEditor.cpp`
- Modify: `Source/PluginProcessor.cpp` (`createEditor` and its includes)
- Modify: `CMakeLists.txt` (add the editor source and remove the generic-editor usage)

**Interfaces:**
- Consumes: `apvts` and the parameter IDs from Task 1.

- [ ] **Step 1: Write the editor**

`SimpleDelayAudioProcessorEditor` owns three `juce::Slider`s (rotary, text box below) and three `juce::Label`s. For each slider, keep a `std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>` as a member, constructed as:

```cpp
timeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
    processor.apvts, "time", timeSlider);
```

Use the parameter ID string constants rather than literals. The constructor sets a fixed size of about 420 × 200, calls `addAndMakeVisible` for every child, and positions them in `resized()`.

This task changes no DSP. `DelayLine` and `processBlock` are untouched.

- [ ] **Step 2: Point `createEditor` at it**

```cpp
juce::AudioProcessorEditor* SimpleDelayAudioProcessor::createEditor()
{
    return new SimpleDelayAudioProcessorEditor (*this);
}
```

Add `Source/PluginEditor.cpp` to `target_sources` in `CMakeLists.txt`, and include `PluginEditor.h` from `PluginProcessor.cpp` so the editor type is complete at the point of use. `juce::GenericAudioProcessorEditor` is no longer referenced anywhere and its include can go.

- [ ] **Step 3: Build**

```bash
cmake --build build --config Release --parallel
```

Expected: exit code 0, no warnings from our files.

- [ ] **Step 4: Verify the knobs work**

Run the standalone. Expected: three labelled rotary knobs appear, they match the defaults (Time 375 ms, Feedback 0.40, Mix 0.35), turning each audibly changes the sound, and dragging Time does not click.

- [ ] **Step 5: Commit**

```bash
git add Source Tests CMakeLists.txt
git commit -m "Add custom editor with three parameter-bound sliders"
```

---

## Task 7: In-host verification in FL Studio, and the README

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: the built plugin from every prior task.
- Produces: nothing.

- [ ] **Step 1: Finish `README.md`**

Task 1 created it with the prerequisites, the pinned JUCE clone command, the configure and build commands, the artefact paths, and the two install routes. Add the rest:

- How to run the offline tests, both executables (`build/Tests/DelayLineTests.exe` and `build/Tests/ProcessorTests.exe`).
- The attribution/licensing note from spec §2: JUCE 8 is AGPLv3 or commercial; personal learning use is fine, and distributing this would require a deliberate licensing decision.
- The known limitation: the delay line does not sanitize NaN or infinity arriving from an upstream plugin, so a NaN would circulate in the feedback loop and silence the output. Out of scope for v1, recorded so it is not a surprise.
- The five-line exercise from spec §5.2: write the parameter smoother by hand as a one-pole filter, then compare against `juce::SmoothedValue`.

- [ ] **Step 2: Add the plugin to FL Studio**

Either add the `VST3` artefact folder as a search path and rescan, or copy the bundle into `C:\Program Files\Common Files\VST3`.

Expected: **Simple Delay** appears in FL Studio's plugin list.

- [ ] **Step 3: Run the manual verification checklist**

Put the plugin on an insert with audio playing, then confirm each of these:

- The delay is audible with a repeat behind the source.
- Time, Feedback, and Mix each behave as labelled.
- At the parameter's midpoint, Time reads a few hundred ms (roughly 100–150 ms), not ~1000 ms. If it reads high, the skew is inverted: change `NormalisableRange`'s fourth argument from `0.25f` to `4.0f` in `createParameterLayout()`.
- An automation clip on Feedback moves it smoothly, with no stepping.
- Save the project, close FL Studio, reopen it: all three values are restored.
- Host bypass silences the effect (this is FL Studio's bypass, not a plugin control).
- Dragging Time produces no clicks.
- Stereo output is correct — no stale audio or one-sided noise.
- No clipping or distortion at default settings.

- [ ] **Step 4: Fix anything that failed, then commit**

Any fix gets its own commit with a message saying what the symptom was. If all nine checks pass:

```bash
git add README.md
git commit -m "Document build, install, testing, and FL Studio verification"
```

---

## Verification Summary

| Spec section | Verified by |
|---|---|
| §5.1 delay algorithm, sizing, clamping | Task 2 Steps 5–11, Task 3 |
| §5.1 fractional interpolation | Task 3 Steps 1–4 |
| §5.2 parameters, ranges, defaults | Task 1 Step 7, Task 7 Step 3 |
| §5.2 smoothing | Task 4 Step 6 (by ear), Task 7 Step 3 |
| §5.2 aliased channels | Task 5 Step 2, Task 7 Step 3 |
| §5.3 editor and attachments | Task 6 |
| §6.1 feedback cap | Task 2 Step 8, Task 3 Step 6 |
| §6.2 denormals | Task 4 Step 3 (`ScopedNoDenormals`), Task 3 Step 6 (decays) |
| §6.3 sample-rate changes | Task 3 Step 5, Task 5 Step 2 |
| §6.4 channel configurations | Task 1 Step 7 |
| §6.5 aliased input channels | Task 5 Step 2 |
| §6.6 delay beyond buffer | Task 2 Step 8 |
| §7.1 offline DSP tests | Tasks 2 and 3 (all seven) |
| §7.2 in-host verification | Task 7 Step 3 |
| §7.3 build verification | Task 1 Steps 9–10 |
| Success criterion: no allocation in processBlock | Task 2 Step 10 |
