# Clouds normal-mode DSP adapter

`CloudsEngine` is an adapted normal granular-mode implementation from the original
Clouds firmware, with the original grain scheduler, variable window, density
mapping, stereo spread, diffusion, feedback and reverb algorithms. It is not a
complete firmware emulator or a claim of bit-identical hardware output.

## Provenance and build

- `pichenettes/eurorack`: `08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4`.
- Its pinned `pichenettes/stmlib`: `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.
- Source: <https://github.com/pichenettes/eurorack/tree/08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4/clouds>.
- The source file headers and `LICENCE` retain the upstream MIT notices.
- Compile **only `CloudsEngine.cpp`**. It includes `vendor/clouds/resources.inc`
  and `vendor/stmlib/dsp/units.inc`; do not compile those tables separately.
- All vendor includes are relative. No global include paths or `TEST` macro are
  required. The public header exposes only the standard-library pimpl.
- Vendor symbols use `fire_clouds_vendor` / `fire_clouds_stmlib`, avoiding a
  process-wide `stmlib::Random` or shared mutable DSP state.

## Processing contract

Call `prepare(sampleRate)` off the audio callback. Supported host rates are
8–384 kHz; invalid or unsupported rates fall back to 48 kHz. `process()` consumes
one stereo host frame and returns fully wet audio. For mono, the caller can pass
identical inputs and fold the stereo wet result according to its own mix policy.
Dry mixing, enable ramps, host bypass, parameter persistence and latency reporting
belong to the caller.

The DSP runs at **32 kHz in fixed 32-frame blocks**, with 32 active-grain slots
(the firmware's normal stereo quality). Its recording allocation is 32,704
16-bit samples per channel, with an effective circular length of 32,696 samples.
The normal stereo 16-bit path is the only firmware mode in this adapter. There is
no spectral/stretch/looping mode, low-fidelity selector, hardware control scanner,
trigger/gate input, or firmware sample-memory persistence.

The host adapters use streaming Blackman-windowed sinc conversion in both
directions. Decimation increases the FIR length, rather than reusing an
insufficient fixed-length kernel at high host rates. Tables have 256 fractional
intervals and an endpoint row, with interpolation and DC normalisation. Immutable
coefficient tables are shared between instances; a mutex/weak cache is accessed
**only in prepare**. Ring buffers and a one-core-block output prefill make the wet
latency deterministic for a given rate, independent of the host callback partition.

Controls are clamped, non-finite samples are replaced with silence, and input is
limited to the hardware codec range. Continuous controls have 2 ms smoothing at
the fixed internal rate, followed by the original 1 kHz block control updates.
The output preserves the core's 1.2 post gain and soft 16-bit conversion. The
hardware's dry/wet crossfade is omitted; the caller owns that mix and its dry path.

## Intentional portability and safety changes

- Every used DSP state is explicitly initialised. The firmware's reliance on a
  global, zero-initialised processor is not suitable for multiple heap instances.
- Each granular player owns its LCG seed. Resetting or rendering another instance
  cannot alter this instance's scheduling, and no process-wide RNG lock is used.
- Recording storage uses a logical valid prefix; FX delay memories use 64-bit
  generation tags. `reset()` clears counters, small feedback/filter state and
  grain activity without clearing the large recording/reverb allocations.
- `AudioBuffer::WriteFade` never reads `tail[256]`. Short freezes use only captured
  tail entries, holding the last available sample during the remainder of the
  256-sample resume crossfade.
- The first 160 recorded internal samples have a raised-cosine fade. This affects
  the recording-start boundary after reset/activation, not each new note or grain.
  It prevents a nonzero first input sample appearing midway through a grain's
  envelope as an artificial click.
- Freeze requested before the first audio has been recorded captures a full recording
  window after signal is present, then engages. A Freeze edge with existing
  non-silent recording, including a partial window, freezes at the next internal
  block boundary. Unfreezing retains the original
  recording-tail transition and freeze-dependent feedback/reverb behaviour.
- Lookup interpolation returns an exact endpoint without reading the nonexistent
  following entry. This covers Size/Texture endpoints used by grain/window LUTs.
- Host DSP helpers use portable saturation/square-root code, and the reciprocal
  square-root bit conversion uses `memcpy` instead of inactive-union type punning.
- Read interpolation and the reverb's 12-bit storage are retained from the
  firmware. Host SRC prevents sample-rate-conversion folding; it does **not** turn
  the upstream grain pitch reader into an ideal band-limited pitch shifter.

## Verification

`tests/CloudsEngineTests.cpp` covers rate conversion, callback partitioning,
finite/bounded extrema, LUT endpoints, first-history onset, reset versus a fresh
instance, instance RNG isolation, short freezes, freeze requested in silence, and
actual input/output SRC alias rejection. The same tests have been exercised in a
standalone AddressSanitizer/UndefinedBehaviorSanitizer build, including float-to-
integer overflow checks. They do not establish equality with physical hardware.
