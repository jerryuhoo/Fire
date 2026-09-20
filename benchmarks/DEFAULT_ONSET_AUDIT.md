# Default sine-onset investigation (2026-09-19)

The reported host is JUCE AudioPluginHost. Inspection of its running process
identified the loaded plug-in as the user-installed AU. Its executable hash
matched `Builds/MacOSX/build/Debug/Fire.component`, not the Release build.
The host itself was also running from its Debug directory. The saved device
configuration used 44.1 kHz; the currently active buffer size was not established.

## DSP isolation

`DefaultOnsetRegressionTests.cpp` feeds repeated sine notes with varied phases,
silence gaps, and either an immediate first input or a short attack. It exercises
44.1/48/96 kHz and 37/128/512-sample callbacks without changing any plug-in
parameters. After compensating the fixed three-sample latency, each output is
checked against the independent default cubic transfer curve:

`y = 1.5*x - 0.5*x*x*x`, for the probe's input range.

The default curve intentionally adds harmonics and gain; it is not a unity
wire. The checks look for extra state-dependent discontinuities, not for
removing transients already present in the input. The initial repeated-note
probe measured a maximum residual of approximately `1.2e-7`.

## Installed AU versus Release AU

A temporary Core Audio harness loaded the installed AU and the compiled Release
AU, using the same defaults, callback sizes, silence and 440 Hz / 0.12-amplitude
sine input. Each measurement covered 800 callbacks. Both builds produced the
same measured steady peak, approximately 0.17914.

| Frames at 44.1 kHz | Debug tone median | Release tone median | Callback budget |
| --- | ---: | ---: | ---: |
| 64 | 474 us | 60 us | 1451 us |
| 128 | 595 us | 86 us | 2902 us |
| 512 | 1313 us | 266 us | 11610 us |

Some Debug wall-time spikes exceeded the small-buffer budgets. This harness
was not the host's real-time callback thread, so those spikes alone do not
prove that a host underrun caused every reported click. They establish a
substantial loss of processing headroom when auditioning the Debug build.

## Fix and live verification

- Projucer Debug configurations no longer automatically overwrite installed
  plug-ins. Debug binaries remain available in their build folders.
- Fresh CMake Debug and multi-configuration builds default installation to off;
  an explicit or previously cached `FIRE_INSTALL_PLUGINS` choice still takes
  precedence. Existing build directories need the option explicitly set to off.
- The loaded Debug AU was preserved in a hidden backup directory, and the
  installed AU was atomically replaced with the signed Release bundle.
- A running host retains its loaded Debug code until it is restarted. Save
  its graph first, reopen it, and compare the same notes with Fire enabled
  and bypassed. That listening comparison is required before concluding that
  the original symptom is fully resolved.

## Live follow-up

The user still heard the symptom after restarting with the Release AU. Direct
inspection confirmed the loaded AU executable was the 7,493,136-byte Release
binary. The host executable itself contained `JUCE v8.0.9` and was still the old
Debug AudioPluginHost; updating Projucer had not rebuilt it.

The current Audio Settings dialog showed ZOOM L-8 Driver input/output,
44.1 kHz and 512 samples. A 45-second Core Audio overload-notification probe
reported no events. This short notification observation is not proof that the
device never drops out.

The live Fire state was Drive 100, Cubic, Safe and Link enabled, HQ disabled,
with the other effects disabled. The graph was saved to a separate debug copy.
A second copy connected Sine Wave Synth directly to Audio Output and disconnected
both Fire outputs. The user **still heard the noise in this verified bypass
graph**. Fire processing is therefore not required for the reported symptom.

An offline probe loaded the complete saved JUCE plug-in state into the Release
processor. Repeated 55/440 Hz notes, two start phases and 0/2/10 ms attacks
matched the sample-wise Safe+Cubic calculation within `8.94e-8`. Safe's recovery
to high gain during silence sharpens the first cycle at Drive 100; these probes
did not find an additional random discontinuity.

Both JUCE 8.0.9 and the pinned JUCE 9.0.2 internal Sine Wave Synth use immediate
full-level starts, with no attack envelope. Phase starts at zero, so the ordinary
note-on has a slope discontinuity rather than a nonzero first sample. Release
multiplies the tail by 0.99 per sample and cuts it at 0.005 (about 12 ms at
44.1 kHz). Voice stealing can also stop an old voice immediately. These are
source-transient candidates; upgrading the host alone does not change that
envelope. Compare with a source using a short smooth attack/release before
attributing remaining clicks to Fire.

The default processor regression passed all 18 combinations. A JUCE 9.0.2
Release AudioPluginHost was built from the pinned project submodule, signed,
and installed at `/Applications/JUCE/AudioPluginHost.app`. The running process
was verified to use that executable and the Release Fire AU. The bypass graph
and ZOOM / 44.1 kHz / 512-sample settings were retained for the next live
comparison. No production DSP curve or input gate was changed.

The user confirmed that the noise disappeared in the JUCE 9.0.2 Release
host with the bypass graph. This narrows the observed issue to the previous
host/runtime setup; it does not isolate framework version from build mode.
Reconnecting Fire for the final listening comparison was deferred when work
switched to the requested UI/EQ changes.

The preset-pack task remains paused.
