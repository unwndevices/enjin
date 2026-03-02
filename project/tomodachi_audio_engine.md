# Tomodachi Audio Engine — Design Document (v0.2)

Companion to the main Tomodachi design doc. Covers the on-device (and WASM-identical) audio synthesis engine, its block graph, the Lua control API, and the rhythm/melody/logic toolkit.

---

## 1. Design philosophy

The Tomodachi audio engine is **not** a general-purpose DSP environment. It is a fixed-vocabulary synth engine where Lua scripts wire together pre-built C++ blocks, set their parameters, and drive them with rhythm/melody/logic tools — all without writing any sample-level code.

Think of it as four layers:

```
┌─────────────────────────────────────┐
│  Lua scripts                        │  ← musical logic, UI, sensor mapping
├─────────────────────────────────────┤
│  Control plane  (120 Hz tick)       │  ← clocks, sequencers, scales, logic
├─────────────────────────────────────┤
│  Audio graph    (sample rate)       │  ← node graph, command queue, routing
├─────────────────────────────────────┤
│  DaisySP        (sample rate)       │  ← DSP primitives (oscillators, filters, etc.)
└─────────────────────────────────────┘
```

Lua never touches samples. It sends **commands** (create, connect, set param, free) that the audio thread processes at block boundaries. Inspired by Norns' SuperCollider engine model, but with a fixed block vocabulary instead of an open synthesis language.

### 1.1 DSP dependency: DaisySP

The sample-level DSP is built on **DaisySP** (MIT license, https://github.com/electro-smith/DaisySP), an open-source C++ library created by Electrosmith with contributions from Émilie Gillet (Mutable Instruments). DaisySP provides portable, float-based implementations of oscillators, filters, envelopes, effects, physical models, and drum synthesis — covering ~90% of the block catalog defined in this document.

**Why DaisySP:**

- MIT licensed — compatible with any distribution model, including commercial hardware.
- Platform-independent pure C++ with no hardware dependencies; compiles cleanly for ARM targets (ESP32-S3, RP2350, STM32H7) and to WASM via Emscripten.
- Single-precision float throughout, optimized for MCUs with a single-precision FPU.
- Per-sample `Process()` interface on every module — straightforward to wrap inside our block-based graph (call `Process()` in a loop of `block_size` iterations).
- Proven in production on Daisy hardware (STM32H750 @ 480 MHz) and ported to other platforms.
- Includes Mutable Instruments–derived algorithms (soft clipping, waveshaping) via pichenettes/stmlib.

**What DaisySP provides (mapped to our catalog):**

| Tomodachi block | DaisySP class | Notes |
|---|---|---|
| `osc.sine/saw/square/tri` | `Oscillator` | Supports sine, saw, square, tri, polyBLEP anti-aliasing |
| `osc.noise` | `WhiteNoise` | |
| `filt.svf` | `Svf` | LP, HP, BP, Notch modes |
| `filt.onepole` | `OnePole` | |
| `filt.comb` | `CombFilter` | Feeds into Karplus-Strong |
| `env.adsr` | `Adsr` | |
| `env.ad` | `AdEnv` | |
| `mod.lfo` | `Oscillator` (low freq) | Same class, driven below audio rate |
| `fx.delay` | `DelayLine` | Template-based, configurable max length |
| `fx.crush` | `Decimator` | Bitcrush + sample-rate reduction |
| `fx.overdrive` | `Overdrive` | Soft-clip waveshaper |
| `voice.pluck` | `KarplusString` / `Pluck` | |
| `voice.fm2` | `Fm2` | 2-operator FM with ratios and index |

**What we build ourselves (not in DaisySP):**

- The **node graph** — topology, connections, port routing, topological sort.
- The **command queue** — lock-free ring buffer between Lua/control thread and audio thread.
- The **node wrapper** — `TomoNode` base class that owns a DaisySP object, manages parameter mapping, and implements the `process(block_size)` interface.
- **Complex voice wrappers** (`voice.fm2`, `voice.pluck`) that compose multiple DaisySP objects into a single node with `NOTE_ON`/`NOTE_OFF` semantics.
- **Utility nodes** (`util.out`, `util.vca`, `util.mix`, `util.pan`) — thin; mostly multiply/sum operations.
- The **Lua binding layer** — `audio.new()`, `audio.connect()`, `audio.set()`, etc.
- The entire **control-plane toolkit** (clock, sequencer, scale, pattern, probability) — pure Lua, no DaisySP involvement.

**Integration approach:**

DaisySP is vendored into `packages/core/vendor/DaisySP/` (or added as a git submodule). Only the `Source/` directory is needed — no Daisy hardware dependencies are pulled in. Each Tomodachi node type includes the relevant DaisySP header(s) and instantiates the DSP object internally:

```cpp
// Example: osc.sine node wrapping DaisySP's Oscillator
#include "daisysp.h"

class OscSineNode : public TomoNode {
    daisysp::Oscillator osc_;
public:
    void init(float sample_rate) override {
        osc_.Init(sample_rate);
        osc_.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    }
    void set_param(ParamId id, float value) override {
        switch (id) {
            case PARAM_FREQ: osc_.SetFreq(value); break;
            case PARAM_AMP:  osc_.SetAmp(value);  break;
        }
    }
    void process(float* out, const float* fm_in, size_t size) override {
        for (size_t i = 0; i < size; i++) {
            if (fm_in) osc_.SetFreq(params_.freq + fm_in[i]);
            out[i] = osc_.Process();
        }
    }
};
```

This keeps DaisySP as a leaf dependency — it never leaks into the Lua API or the graph layer. If a DaisySP module doesn't fit (or we need a custom variant), we can swap in our own implementation behind the same `TomoNode` interface without changing anything upstream.

---

## 2. Hardware / resource budget

| Resource | Budget |
|---|---|
| Sample rate | 32 kHz mono |
| Block size | 64 samples (~2 ms @ 32 kHz) |
| Audio thread budget | ~1.5 ms per block on one core |
| Max simultaneous nodes | 32 |
| Max connections | 64 |
| Param smoothing | per-block linear ramp (no per-sample unless critical) |
| RAM for audio buffers | ~32 KB (32 nodes × 64 samples × 32-bit float) |
| Output | mono mix bus → DAC / I2S; stereo pan only at final stage |

All DSP uses **single-precision float (f32)** throughout. DaisySP is float-native and all target MCUs (ESP32-S3, RP2350, STM32H7) have a hardware single-precision FPU. This eliminates fixed-point conversion overhead and ensures the DaisySP modules run at full speed with no adaptation layer. The WASM build uses float identically.

---

## 3. Audio graph architecture

### 3.1 Nodes and ports

Every DSP block is a **Node**. Each node has:

- 0–N **input ports** (audio-rate or control-rate)
- 1 **output port** (audio-rate, mono)
- 0–N **parameters** (named, float-range, settable from Lua)

Connections are **one-to-many**: one output can feed multiple inputs. Many-to-one on an input sums the signals.

### 3.2 Processing model

The graph is evaluated in **topological order** once per block. A simple static sort runs whenever the graph topology changes (connect/disconnect). No per-sample graph walking — just iterate a flat sorted list.

```
for each node in sorted_order:
    node.process(block_size)
```

### 3.3 Command queue

Lua (control thread, 120 Hz) pushes commands into a **lock-free ring buffer**. The audio thread drains commands at block boundaries.

Commands:
- `CREATE(node_id, type)` — instantiate a block
- `DESTROY(node_id)` — remove a block, disconnect all
- `CONNECT(src_node, dst_node, dst_port)` — wire output→input
- `DISCONNECT(src_node, dst_node, dst_port)` — unwire
- `SET_PARAM(node_id, param_id, value, ramp_ms)` — set with optional smoothing
- `NOTE_ON(node_id, freq, vel)` — for voice-type nodes
- `NOTE_OFF(node_id)` — release
- `TRIGGER(node_id)` — single-shot trigger (envelopes, drums)

---

## 4. Block catalog

Each block wraps one or more DaisySP classes inside a `TomoNode`. The DaisySP class column below shows the upstream implementation used. Blocks marked "custom" are thin enough to implement without DaisySP.

### 4.1 Oscillators (5)

| Block | DaisySP class | Params |
|---|---|---|
| `osc.sine` | `Oscillator` (WAVE_SIN) | `freq`, `amp`, `detune` |
| `osc.saw` | `Oscillator` (WAVE_POLYBLEP_SAW) | `freq`, `amp`, `detune`, `pw` (morphs saw→pulse) |
| `osc.square` | `Oscillator` (WAVE_POLYBLEP_SQUARE) | `freq`, `amp`, `pw` |
| `osc.tri` | `Oscillator` (WAVE_POLYBLEP_TRI) | `freq`, `amp` |
| `osc.noise` | `WhiteNoise` | `amp` |

All oscillators accept an audio-rate **FM input** (port 0) that adds to frequency, enabling linear FM synthesis without a dedicated FM voice. `freq` can be set via `NOTE_ON` or `SET_PARAM`.

### 4.2 Filters (3)

| Block | DaisySP class | Params |
|---|---|---|
| `filt.svf` | `Svf` | `cutoff`, `res`, `mode` (0=LP,1=HP,2=BP,3=Notch) |
| `filt.onepole` | `OnePole` | `cutoff` |
| `filt.comb` | `CombFilter` | `delay_samples`, `feedback`, `damping` |

`filt.svf` and `filt.comb` accept audio-rate modulation on port 1 → cutoff/delay.

### 4.3 Envelopes & modulators (4)

| Block | DaisySP class | Params |
|---|---|---|
| `env.adsr` | `Adsr` | `attack`, `decay`, `sustain`, `release` |
| `env.ad` | `AdEnv` | `attack`, `decay`, `curve` |
| `mod.lfo` | `Oscillator` (sub-audio) | `freq`, `amp`, `shape` (0=sin,1=tri,2=saw,3=sq,4=s&h) |
| `mod.random` | custom (slewed `WhiteNoise`) | `freq`, `amp`, `slew` |

Envelopes output a **control signal** (0–1). They can be connected to any input port or used as an audio-rate AM source.

### 4.4 Effects (3)

| Block | DaisySP class | Params |
|---|---|---|
| `fx.delay` | `DelayLine<MAX_SIZE>` | `time`, `feedback`, `mix` |
| `fx.crush` | `Decimator` | `bits`, `rate_div` |
| `fx.overdrive` | `Overdrive` | `drive`, `tone`, `mix` |

### 4.5 Utility (4)

| Block | DaisySP class | Params |
|---|---|---|
| `util.mix` | custom (sum + crossfade) | `balance` (0=A, 1=B), `amp` |
| `util.vca` | custom (multiply) | `amp` (base amplitude) |
| `util.pan` | custom (constant-power pan law) | `pan` |
| `util.out` | custom (summing bus) | `amp` |

`util.out` is a singleton created by the engine at init. Everything that should be heard connects to it.

### 4.6 Complex voices (2)

Pre-wired multi-block nodes for common synthesis patterns, to save node slots and make the API friendlier. These compose multiple DaisySP objects internally:

| Block | DaisySP classes used | Params |
|---|---|---|
| `voice.fm2` | `Fm2` + `Adsr` × 2 | `freq`, `ratio`, `index`, `attack`, `decay`, `sustain`, `release`, `mod_attack`, `mod_decay`, `mod_sustain`, `mod_release`, `amp` |
| `voice.pluck` | `Pluck` (or `KarplusString`) | `freq`, `decay`, `brightness`, `amp` |

Complex voices respond to `NOTE_ON` / `NOTE_OFF` and manage their internal envelopes. They count as 1 node but internally use the equivalent of ~4–5 basic blocks.

### Block count summary

| Category | Count |
|---|---|
| Oscillators | 5 |
| Filters | 3 |
| Envelopes & modulators | 4 |
| Effects | 3 |
| Utility | 4 |
| Complex voices | 2 |
| **Total** | **21** |

---

## 5. The control toolkit (Lua-side)

This is where Tomodachi differentiates itself. While the DSP blocks are intentionally basic, the **control-plane toolkit** running at 120 Hz in Lua is rich and expressive. These are Lua modules shipped with the system (not DSP blocks).

### 5.1 Clocks & transport

```lua
clock.bpm              -- get/set global BPM (20–300)
clock.swing            -- swing amount (0–1, 0.5 = straight)
clock.run(fn)          -- start a coroutine synced to clock
clock.sleep(beats)     -- yield for N beats (fractional OK)
clock.sync(div)        -- yield until next division boundary (1=quarter, 0.25=16th)
clock.on_beat(div, fn) -- register a callback every div beats
clock.stop_all()       -- kill all clock coroutines

-- external sync (post-MVP): MIDI clock in, analog clock in
```

Coroutine-based like Norns. `clock.sleep` and `clock.sync` are the heartbeat of every rhythmic app.

### 5.2 Sequencers

```lua
-- Step sequencer: stores a table of values, advances on trigger
seq = sequencer.new({60, 62, 64, 65, 67, 69, 71, 72})
seq:set_mode("forward")     -- "forward" | "reverse" | "pingpong" | "random" | "walk"
seq:set_length(8)           -- active length (can be < data length)
val = seq:next()            -- advance and return current value
val = seq:peek()            -- current value without advancing
seq:reset()                 -- back to step 1
seq:set(step, value)        -- edit a step
seq:set_all({...})          -- replace all data

-- Euclidean rhythm generator
euc = sequencer.euclid(hits, length, rotation?)
-- returns a table of booleans, e.g. euclid(3,8) → {t,f,f,t,f,f,t,f}
-- use with clock:
clock.run(function()
    local pattern = sequencer.euclid(5, 16)
    local step = 1
    while true do
        if pattern[step] then
            audio.trigger(kick_node)
        end
        step = (step % #pattern) + 1
        clock.sync(1/4)  -- 16th notes
    end
end)
```

### 5.3 Scales & harmony

```lua
-- Scale quantizer
scale.set("dorian")          -- set current scale
scale.set_root(60)           -- C4
scale.note(degree)           -- degree (1-indexed) → MIDI note
scale.quantize(note)         -- snap any MIDI note to nearest scale tone
scale.chord(degree, size?)   -- returns table of notes for chord on degree (triad default)

-- Available scales (MVP set):
-- "major", "minor", "dorian", "phrygian", "lydian", "mixolydian",
-- "aeolian", "locrian", "pentatonic", "minor_pentatonic",
-- "blues", "harmonic_minor", "melodic_minor", "whole_tone", "chromatic"

-- Chord helpers
chord.from_name("Cm7")       -- → {60, 63, 67, 70}
chord.invert(notes, inv)     -- inversion (1 = first, 2 = second, ...)
chord.spread(notes, octaves) -- spread across octave range
```

### 5.4 Pattern recorder

```lua
-- Record and loop sequences of events in real time
pat = pattern.new()
pat:watch(some_table)        -- observe changes to a table (notes, params)
pat:rec()                    -- start recording (on next beat if clock running)
pat:stop()                   -- stop recording
pat:play()                   -- loop playback
pat:overdub()                -- layer on top
pat.on_step = function(data) -- callback on each replayed event
    -- data is whatever was recorded
end
pat:set_quantize(1/4)        -- quantize recorded events to 16ths
pat:clear()
```

### 5.5 Logic & mapping utilities

```lua
-- Value mapping
util.linlin(x, in_lo, in_hi, out_lo, out_hi)   -- linear→linear
util.linexp(x, in_lo, in_hi, out_lo, out_hi)   -- linear→exponential
util.explin(x, in_lo, in_hi, out_lo, out_hi)   -- exponential→linear
util.clamp(x, lo, hi)
util.wrap(x, lo, hi)                             -- wrap around (for sequences)
util.mtof(note)                                   -- MIDI note → frequency
util.ftom(freq)                                   -- frequency → MIDI note

-- Slew / lag (control rate, runs at 120 Hz)
s = slew.new(rate_up, rate_down)
smoothed = s:process(raw_value)

-- Trigger utilities
trig.metro(interval_beats, fn)       -- repeating trigger
trig.once(delay_beats, fn)           -- one-shot delayed trigger
trig.chance(probability, fn)         -- probabilistic trigger (0–1)
trig.burst(count, interval, fn)      -- N triggers spaced by interval

-- Logic / probability
prob.weighted({60, 62, 64}, {0.5, 0.3, 0.2})  -- weighted random pick
prob.drunk(current, step, lo, hi)               -- random walk
prob.markov(state, transition_table)            -- simple Markov chain
```

---

## 6. Revised Lua audio API

The v0.1 design doc had a minimal `audio.*` namespace. Here's the expanded version that wraps the graph engine:

### 6.1 Node lifecycle

```lua
-- Create a node, returns integer node ID
local osc = audio.new("osc.sine")
local filt = audio.new("filt.svf")
local env = audio.new("env.adsr")
local vca = audio.new("util.vca")

-- Wire: src:output → dst:input_port
audio.connect(osc, filt)          -- default: port 0 (signal in)
audio.connect(filt, vca)
audio.connect(env, vca, 1)        -- port 1 = modulation in on VCA
audio.connect(vca, audio.out)     -- audio.out is the global sink

-- Disconnect
audio.disconnect(osc, filt)

-- Destroy
audio.free(osc)
audio.free_all()                   -- teardown on app exit (shell calls this)
```

### 6.2 Parameters

```lua
audio.set(osc, "freq", 440)
audio.set(osc, "freq", 440, 10)       -- ramp to 440 over 10ms
audio.set(filt, "cutoff", 2000)
audio.set(filt, "res", 0.7)
audio.set(filt, "mode", 0)            -- 0=LP

audio.get(osc, "freq")                -- read current value (last commanded)
```

### 6.3 Note & trigger interface

```lua
-- For oscillators and complex voices
audio.note_on(osc, 60, 0.8)     -- note 60, velocity 0.8 (sets freq via mtof)
audio.note_off(osc)

-- For envelopes and one-shot blocks
audio.trigger(env)

-- Convenience: note_on on a voice.fm2 triggers its internal envelopes
local fm = audio.new("voice.fm2")
audio.connect(fm, audio.out)
audio.note_on(fm, 48, 1.0)      -- sets freq, triggers both carrier and mod ADSRs
```

### 6.4 Engine info

```lua
audio.node_count()      -- current active nodes
audio.cpu()             -- audio thread load (0–1), useful for on-screen meters
audio.sample_rate()     -- 32000 (or whatever the platform runs)
```

---

## 7. Putting it all together — example scripts

### 7.1 Euclidean drum machine

```lua
-- Minimal euclidean beat generator with 3 voices

local kick = audio.new("voice.pluck")
audio.set(kick, "freq", 55)
audio.set(kick, "decay", 0.3)
audio.set(kick, "brightness", 0.1)
audio.connect(kick, audio.out)

local hat = audio.new("osc.noise")
local hat_env = audio.new("env.ad")
local hat_vca = audio.new("util.vca")
audio.set(hat_env, "attack", 0.001)
audio.set(hat_env, "decay", 0.05)
audio.connect(hat, hat_vca)
audio.connect(hat_env, hat_vca, 1)
audio.connect(hat_vca, audio.out)

local bass = audio.new("voice.fm2")
audio.set(bass, "ratio", 1.0)
audio.set(bass, "index", 2.0)
audio.set(bass, "decay", 0.2)
audio.connect(bass, audio.out)

local kick_pat = sequencer.euclid(4, 16)
local hat_pat  = sequencer.euclid(7, 16)
local bass_seq = sequencer.new({36, 36, 39, 36, 41, 36, 43, 36})
bass_seq:set_mode("forward")

clock.bpm = 120

clock.run(function()
    local step = 1
    while true do
        if kick_pat[step] then audio.trigger(kick) end
        if hat_pat[step] then audio.trigger(hat_env) end
        if step % 4 == 1 then
            local note = bass_seq:next()
            audio.note_on(bass, note, 0.7)
        end
        step = (step % 16) + 1
        clock.sync(1/4)
    end
end)
```

### 7.2 Tilt-controlled FM drone

```lua
local drone = audio.new("voice.fm2")
audio.set(drone, "ratio", 3.01)
audio.set(drone, "index", 0.5)
audio.set(drone, "attack", 2.0)
audio.set(drone, "sustain", 0.8)
audio.set(drone, "release", 3.0)
audio.connect(drone, audio.out)

local delay = audio.new("fx.delay")
audio.set(delay, "time", 0.375)
audio.set(delay, "feedback", 0.6)
audio.set(delay, "mix", 0.4)
audio.connect(drone, delay)
audio.connect(delay, audio.out)

scale.set("dorian")
scale.set_root(48)

local tilt_slew = slew.new(0.1, 0.1)
local playing = false

function update(dt)
    local tilt = sensors.imu.tiltX()
    local degree = math.floor(util.linlin(tilt, -45, 45, 1, 15))
    local note = scale.note(degree)
    local smoothed_note = tilt_slew:process(note)

    audio.set(drone, "freq", util.mtof(smoothed_note), 20)

    local index = util.linexp(math.abs(sensors.imu.tiltY()), 0, 45, 0.3, 8.0)
    audio.set(drone, "index", index, 10)
end

function on_button(name, down)
    if name == "A" and down then
        if not playing then
            audio.note_on(drone, 60, 0.6)
            playing = true
        else
            audio.note_off(drone)
            playing = false
        end
    end
end
```

---

## 8. Implementation priorities

### Phase 1 — Skeleton (get sound out)

1. Vendor DaisySP into `packages/core/vendor/DaisySP/` and verify it compiles for target MCU and Emscripten
2. Implement `TomoNode` base class and node registry
3. Audio thread with block-based processing loop (ISR or FreeRTOS task)
4. Lock-free command queue (control thread → audio thread)
5. Wrap DaisySP `Oscillator` (sine) → `osc.sine`, `WhiteNoise` → `osc.noise`
6. Implement `util.vca` (multiply) and `util.out` (summing bus) — no DaisySP needed
7. Wrap DaisySP `AdEnv` → `env.ad`
8. Basic Lua API: `audio.new`, `audio.free`, `audio.connect`, `audio.set`, `audio.trigger`
9. WASM adapter using WebAudio AudioWorklet

### Phase 2 — Playable

10. Wrap remaining DaisySP `Oscillator` waveforms → `osc.saw`, `osc.square`, `osc.tri`
11. Wrap DaisySP `Svf` → `filt.svf`, `Adsr` → `env.adsr`
12. `clock.*` coroutine system (pure Lua, original implementation)
13. `sequencer.new`, `sequencer.euclid` (pure Lua)
14. `scale.*` module (pure Lua)
15. `util.linlin`, `util.mtof`, etc. (pure Lua)
16. Wrap DaisySP `Pluck` / `KarplusString` → `voice.pluck`

### Phase 3 — Expressive

17. Compose DaisySP `Fm2` + `Adsr` × 2 → `voice.fm2` wrapper
18. Wrap DaisySP `DelayLine` → `fx.delay`, `Decimator` → `fx.crush`, `Overdrive` → `fx.overdrive`
19. `mod.lfo` (DaisySP `Oscillator` at sub-audio rate), `mod.random` (custom or DaisySP `Dust`-style)
20. `pattern.*` recorder (pure Lua)
21. `prob.*`, `trig.*` utilities (pure Lua)
22. Wrap DaisySP `CombFilter` → `filt.comb`, `OnePole` → `filt.onepole`
23. `util.mix`, `util.pan` (no DaisySP needed)
24. `slew.new` (pure Lua, control-rate)

### Phase 4 — Polish

25. CPU load metering
26. Param smoothing audit (click-free transitions on all DaisySP-backed nodes)
27. Tuning: profile block processing per node type, optimize hot paths
28. External MIDI clock sync (if hardware ready)
29. Evaluate DaisySP modules not yet used: `Chorus`, `Flanger`, `Phaser`, `ReverbSc` — add as bonus blocks if CPU allows

---

## 9. Open questions

1. ~~**Sample rate**~~: **Resolved — 32 kHz.** Lo-fi character fits the device. DaisySP is sample-rate agnostic (set at `Init()` time).

2. ~~**Fixed vs floating point**~~: **Resolved — float32 everywhere.** DaisySP is float-native. All target MCUs have a hardware single-precision FPU.

3. **Polyphony model**: The current design is "allocate nodes manually." Should we add a `voice.poly(type, max_voices)` helper that auto-allocates and steals voices? DaisySP does not include a voice allocator, so this would be custom. Probably yes, post-Phase 2.

4. **Audio input**: The hardware has a mic and line in. Should there be an `input.mic` node and `input.line` node that inject external audio into the graph? Very powerful (live processing, vocoder, etc.) but adds latency considerations. Likely Phase 3/4.

5. **Wavetable / sample playback**: DaisySP does not include a wavetable oscillator, but LEAF (another MIT library, https://github.com/spiricom/LEAF) does — we could cherry-pick that implementation or write our own. Could limit to tiny wavetables (256 samples × 8 tables) loaded from `.tomo` assets.

6. ~~**Stereo**~~: **Resolved — mono with pan.** Mono throughout, stereo only at the DAC stage via `util.pan`. True stereo processing doubles CPU and buffer cost.

7. **DaisySP modules beyond the catalog**: DaisySP ships additional modules we haven't mapped yet — `ReverbSc` (Costello reverb), `Chorus`, `Flanger`, `Phaser`, modal synthesis (`Resonator`), granular (`GranularPlayer`), and drum models (`AnalogBassDrum`, `AnalogSnareDrum`, `HiHat`). These are strong candidates for post-MVP expansion. The `TomoNode` wrapper pattern makes adding new blocks trivial once the graph engine is in place.

8. **DaisySP version pinning**: DaisySP does not follow semantic versioning rigorously. We should vendor a specific commit hash rather than tracking `master`, and document which version we've validated against.
