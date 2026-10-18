# Simple Delay — VST3 Effects Plugin (Design Spec)

Date: 2026-10-19
Status: Approved design, pending spec review

## 1. Intent

Build a simple stereo delay as a VST3 plugin for Windows, as a first plugin project.
The goal is **learning how plugins and audio DSP work**, not shipping a product. Every
design decision below favors clarity and directness over feature count or optimization.

The project succeeds if the author can explain, from memory:

- what the host calls and when (`prepareToPlay`, `processBlock`, state save/load)
- why `processBlock` must not allocate or block
- how a circular buffer plus feedback produces an infinite repeat
- why fractional delay needs interpolation
- why feedback must be capped below 1.0

and can hear the result of their own changes in FL Studio.

### Success criteria

1. The plugin builds from a clean checkout with two commands.
2. It loads in FL Studio 2025 and in the standalone app, and passes audio.
3. Time, Feedback, and Mix each audibly and correctly affect the output.
4. Dragging Time produces no clicks (smoothing + interpolation work).
5. Saving and reloading the FL Studio project restores all knob values.
6. Host bypass works.
7. The offline DSP test suite passes.
8. No allocation occurs inside the audio-processing path.

### Explicitly out of scope for v1

Tempo sync, filters or saturation in the feedback path, LFO modulation, ping-pong,
custom-drawn UI, preset browser, installers, code signing, VST2, AU/AAX,
macOS/Linux builds, CI, telemetry. These are not deferred silently; they are
excluded so the project stays finishable.

## 2. Environment and constraints

Verified present on the target machine:

- Windows, 64-bit.
- Visual Studio 2022 **Build Tools** 17.14 (MSVC 14.44). No IDE.
- CMake and Ninja bundled with Build Tools; neither on `PATH`.
- `git` and `winget` available.
- FL Studio 2025 installed (also 21 and 2024).
- VST3 plugin directory: `C:\Program Files\Common Files\VST3`.
- Working directory `C:\dev\vst\vst-test` is empty and **not yet a git repository**.

Author background: experienced programmer, new to C++ and new to audio DSP.
No prior experience with plugin APIs, signal processing, or audio hardware APIs.

Constraints that follow:

- The build must be driven from the command line; no IDE is available.
- C++ constructs used should be explained where they differ from a language the
  author already knows, or avoided in favor of simpler equivalents.
- No dependency beyond JUCE and the standard library.

### Licensing

JUCE 8 is AGPLv3 or commercial. Personal learning use with no distribution is
permitted under AGPLv3. If this project is ever distributed, a licensing decision
must be made deliberately. Recorded here so it is not discovered late.

## 3. Technology choice

**JUCE 8 (pinned to an exact release tag verified against the JUCE releases page at
implementation time) + CMake + MSVC.**

Rationale:
- One source tree produces both the VST3 and a **Standalone** application. The
  standalone allows testing without launching a DAW, which makes the edit/hear
  loop fast.
- JUCE supplies parameter handling, host automation, and state serialization, so
  the author's attention goes to the DSP rather than to COM-style boilerplate.
- JUCE bundles the Steinberg VST3 SDK; no additional download is required.

Rejected alternatives:
- **Raw Steinberg VST3 SDK** — requires implementing the processor, edit
  controller, and factory, roughly 400 lines of boilerplate before any audio.
  Valuable later as a deliberate "what does JUCE hide" exercise; poor first project.
- **iPlug2** — smaller and MIT-licensed, but thinner documentation and rougher
  CMake support on Windows, which would mean debugging the build system instead of
  the DSP.

## 4. Architecture

### 4.1 Repository layout

```
vst-test/
  CMakeLists.txt            top-level build: fetches JUCE, defines targets
  .gitignore                build/, libs/, .vs/, *.vst3
  README.md                 build instructions and plugin install location
  Source/
    PluginProcessor.h/.cpp  AudioProcessor: parameters, smoothing, processBlock
    PluginEditor.h/.cpp     three sliders bound to parameters
    DelayLine.h             hand-written delay DSP; pure C++, no JUCE
  Tests/
    DelayLineTest.cpp       offline test harness; no JUCE, no audio device
  docs/superpowers/specs/   this document and later plans
```

### 4.2 The central boundary

`DelayLine` has **no dependency on JUCE, on the plugin API, or on any audio device
API**. It accepts samples and returns samples. Consequences:

- It can be fully tested offline in under a second.
- The drop in complexity between "understand the DSP" and "understand the plugin"
  is confined to one file each.
- It could be reused in a non-plugin context unchanged.

Division of responsibility:

| Unit | Knows about | Must not know about |
|---|---|---|
| `DelayLine` | buffers, echoes, feedback, interpolation | JUCE, parameters, hosts |
| `SimpleDelayAudioProcessor` | parameters, smoothing, mixing, host callbacks | buffer indexing details |
| `SimpleDelayAudioProcessorEditor` | sliders and parameter attachments | DSP |

### 4.3 Signal flow

```
in ──┬─────────────────────────────────────────► ×(1−mix) ──┐
     │                                                        ├──► out
     └──► DelayLine.processSample ──► wet ──────────► ×mix ───┘
                  │
                  └── feedback × ──┐
                                   └── (added back into the delay buffer's write)
```

`DelayLine.processSample(input)` returns the delayed (wet) sample and internally
writes `input + delayed × feedback` into the buffer. Dry/wet mixing happens in the
processor, because it is a plugin-level decision rather than a property of a delay.

### 4.4 Targets and build configuration

- Generator: **Visual Studio 17 2022** (msbuild), not Ninja. Ninja requires the
  MSVC toolchain on `PATH`, which requires wrapping every build in a `vcvars`
  shell. The VS generator locates the toolchain itself. Slower builds are
  acceptable at this project's size.
- CMake installed via `winget install Kitware.CMake` so it is on `PATH`.
- JUCE fetched **once** into `libs/JUCE`, pinned to an exact release tag, cloned
  with its submodules (the VST3 SDK is a submodule). Pinning prevents upstream
  changes from breaking the build.
- Targets: `SimpleDelay_VST3` and `SimpleDelay_Standalone`, both from one
  `juce_add_plugin` definition.
- Build type: Release for normal use; Debug available for stepping through DSP.
- Install: `COPY_PLUGIN_AFTER_BUILD` copies the bundle into
  `C:\Program Files\Common Files\VST3`, which requires an elevated shell. Fallback
  if permissions or elevation are a problem: point FL Studio's
  **Plugin Manager → VST3 search paths** at the build output directory, requiring
  no administrator rights. Both options documented in `README.md`.

## 5. Components

### 5.1 `DelayLine` (Source/DelayLine.h)

A circular buffer whose read head lags its write head. Header-only, one class,
standard library only.

Interface:

```cpp
class DelayLine
{
public:
    void  prepare(double sampleRate, float maxDelayMs);  // allocates; never called during processing
    void  reset();                                       // clears buffer and indices
    void  setDelaySamples(float samples);                // fractional allowed
    void  setFeedback(float feedback);                   // clamps to [0, 0.95]
    float processSample(float input);                    // returns delayed sample

    // read-only accessors, used by the offline tests
    int          getBufferSize() const noexcept;
    const float* bufferData()   const noexcept;
private:
    std::vector<float> buffer;
    int   bufferSize   = 0;
    int   writeIndex   = 0;
    float delaySamples = 0.0f;
    float feedback     = 0.0f;
};
```

Per-sample algorithm, in this exact order:

```
1. readIndexF = wrap(writeIndex - delaySamples)        // fractional
2. delayed    = interpolate(buffer, readIndexF)        // linear
3. buffer[writeIndex] = input + delayed × feedback
4. writeIndex = (writeIndex + 1) % bufferSize
5. return delayed
```

Step 3 is the mechanism that produces an infinite repeat: the buffer holds the
input signal plus its own last echo, so one multiply per sample yields unbounded
repetition.

**Fractional delay and interpolation.** Linear interpolation between the two
samples adjacent to `readIndexF`:

```
delayed = buffer[i0] + frac × (buffer[i1] − buffer[i0])
```

Required because an integer-only read head jumps position when the delay time
changes, producing audible clicks, and because it is the same operation underlying
sample-rate conversion and modulation effects.

**Buffer sizing.** `ceil(sampleRate × maxDelayMs / 1000) + 4` samples. The
additional samples provide headroom for the interpolation lookahead
(`i0 + 1`) and for the maximum delay value to be reachable.

**Clamping.** `setDelaySamples` clamps into `[1, bufferSize − 4]`;
`setFeedback` clamps into `[0, 0.95]`.

### 5.2 `SimpleDelayAudioProcessor`

`juce::AudioProcessor` subclass. Owns one `DelayLine` per channel (two for stereo;
channels delay independently — this is not ping-pong).

Parameters, exposed via `juce::AudioProcessorValueTreeState` so they are
automatable and saved with the project:

| ID | Range | Default | Notes |
|---|---|---|---|
| `time` | 1 – 2000 ms | 375 ms | logarithmic skew for even knob travel |
| `feedback` | 0 – 0.95 | 0.40 | capped deliberately, see §6.1 |
| `mix` | 0 – 100 % | 35 % | dry/wet |

Host-provided bypass is used; no bypass parameter is created.

`prepareToPlay(sampleRate, maxBlockSize)`:
- stores the sample rate
- calls `prepare` on each `DelayLine` with `maxDelayMs = 2000`
- resets the `SmoothedValue` instances with the sample rate and a ~20 ms ramp

`processBlock(buffer, midi)`:
- `juce::ScopedNoDenormals` at the top of the function
- clears output channels beyond the number of input channels (JUCE aliases
  absent input channels to the output buffer; without this, stereo-out/mono-in
  hosts hear stale data in one channel)
- reads the three parameter values **once per block** from raw atomics
- sets smoother targets
- for each sample, for each channel: advance the smoothers, apply
  `setDelaySamples` / `setFeedback`, call `processSample`, then compute
  `out = in × (1 − mix) + wet × mix`
- performs no allocation, no locking, no string or file work

`getStateInformation` / `setStateInformation` delegate to the APVTS's
`copyState` / `replaceState`.

**Parameter smoothing.** Time and Mix are smoothed so a stepped per-block parameter
value does not produce zipper noise or clicking. `juce::SmoothedValue` is used in
the processor, not in `DelayLine`, to keep `DelayLine` free of JUCE. A hand-written
one-pole smoother is noted in the README as a five-line exercise for understanding
what the class does internally.

### 5.3 `SimpleDelayAudioProcessorEditor`

Three `juce::Slider`s with labels, each bound to its parameter through
`juce::AudioProcessorValueTreeState::SliderAttachment`. Approximately 40 lines.

`juce::GenericAudioProcessorEditor` is a one-line alternative that creates the
same controls automatically; it is rejected for v1 only because the attachment
mechanism is the piece worth seeing once, since it is how widgets and parameters
stay in sync in every plugin.

## 6. Error handling and edge cases

### 6.1 Unbounded feedback

Each pass through the loop multiplies the signal by `feedback` with no attenuation.
At `feedback ≥ 1.0`, output grows without bound and can reach levels that damage
speakers and hearing. The parameter range is capped at 0.95 and `DelayLine`
independently clamps its input to the same range, so no host automation curve can
drive it above the cap.

### 6.2 Denormals

As a feedback tail decays toward silence it produces very small floating-point
values (on the order of `1e-40`) that some CPUs process on a path hundreds of times
slower, stalling a tail into audible crackling. `juce::ScopedNoDenormals` at the
top of `processBlock` sets the CPU into flush-to-zero mode for the duration.

### 6.3 Sample rate changes

A host may call `prepareToPlay` more than once, including mid-session when the
audio device changes, and `maxBlockSize` may change. Buffers are sized from
`sampleRate` at that point and the smoothers are re-initialized. Because
`DelayLine::prepare` allocates, it must only ever be called from `prepareToPlay`,
never from `processBlock`.

### 6.4 Channel configurations

`isBusesLayoutSupported` accepts mono or stereo input and output, and rejects
anything else, so unsupported configurations fail loudly at load time rather than
misbehaving during playback. Output-only (no input) configurations are not
supported.

### 6.5 Aliased input channels

Covered in §5.2: output channels with no corresponding input channel must be
explicitly cleared.

### 6.6 Delay time beyond the buffer

`DelayLine` clamps any requested delay to the buffer's usable range, so an
out-of-range value from automation cannot read outside the buffer.

### 6.7 State restore across versions

Not a concern in v1; parameter IDs are chosen to be stable strings so saved
projects keep working in later versions.

## 7. Testing

### 7.1 Offline DSP tests (Tests/DelayLineTest.cpp)

A plain `main()` with a small `CHECK` macro. No test framework, no JUCE, no audio
device. Runs in under a second, and is the definition of "correct" for the DSP.

| Test | Asserts |
|---|---|
| Impulse with a 100-sample delay | Non-zero output first appears at sample 100 |
| Impulse, delay 100, feedback 0.5 | Echo amplitudes at 100/200/300 are 0.5/0.25/0.125 |
| Impulse, delay 100.5 | Energy splits between samples 100 and 101 (interpolation) |
| Feedback 0.95, 10 seconds of samples | Output stays finite and bounded, and decays |
| `bufferData()` and `getBufferSize()` captured before and after 1000 `processSample` calls | Both unchanged — nothing allocates in the audio path |
| `reset()` followed by a new impulse | No echoes remain from before the reset |
| Sweeping delay time across a run | Output contains no NaN or infinity |

### 7.2 In-host verification (manual)

In FL Studio 2025 with the plugin on an insert:
- the delay is audible and the three knobs do what they say
- a parameter automated from a FL Studio automation clip moves smoothly
- saving, closing, and reopening the project restores all three values
- host bypass silences the effect
- dragging Time produces no clicks
- audio passes through with no added noise or distortion

### 7.3 Build verification

A clean checkout plus documented commands produces both targets, and the VST3
appears where FL Studio's Plugin Manager finds it.

## 8. Implementation milestones

Ordered to close the risky pipeline before writing any DSP, so that a failure at
any step identifies itself unambiguously.

1. **Scaffold and hear something.** CMake configures and builds; the VST3 installs
   or is visible to FL Studio's search path; the standalone app opens; audio passes
   through; and the Time parameter's value is temporarily interpreted as an output
   gain, so turning the knob audibly changes the level. This proves build,
   install, load, audio, and parameter-to-audio-thread plumbing using no DSP at
   all. Any problem here is definitely a build or host-integration problem.
2. **`DelayLine` plus offline tests.** No plugin involved. Tests green before the
   code goes near audio hardware.
3. **Wire the DSP into `processBlock`** with the three parameters and smoothing.
4. **The editor.** Three sliders and their attachments.
5. **Verify in FL Studio 2025** per §7.2.

The step-1 gain control is temporary scaffolding and is removed in step 3.

## 9. Naming and identity

Placeholders to be confirmed during implementation, each a one-line change:

- Company name: `jirwo`
- Plugin name: `Simple Delay` (the name FL Studio displays), passed to
  `juce_add_plugin` as `SimpleDelay`, from which JUCE derives the
  `SimpleDelay_VST3` and `SimpleDelay_Standalone` targets named in §4.4

## 10. Open questions

None. All design decisions above are settled; anything not listed is out of scope
per §1.
