# Fire (Version 1.6.0) [![](https://travis-ci.com/jerryuhoo/Fire.svg?branch=master)](https://travis-ci.com/jerryuhoo/Fire) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/8c68fa4c8da04cb8abca88e2dfceb280)](https://app.codacy.com/gh/jerryuhoo/Fire/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)[![CMake Build Matrix](https://github.com/jerryuhoo/Fire/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/jerryuhoo/Fire/actions/workflows/build_and_test.yml)

![Alt text](Fire1.png?raw=true "Title")

![Alt text](Fire2.png?raw=true "Title")

![Alt text](Fire3.png?raw=true "Title")

## 1. Introduce

This is a multi-band distortion plugin [『Fire』](https://www.bluewingsmusic.com/fire.html). It can be used in DAWs which supports AU and Vst3 plugins such as Ableton Live, Fl Studio, etc.

Demo video:

(YouTube) <https://youtu.be/U5UTz6kWVE4>

(Bilibili) <https://www.bilibili.com/video/BV11MWpzUEKA/>

:musical_note: Home Page for Wings Music: [Blue Wings Music](https://www.bluewingsmusic.com/)

Hope you like it!

## 2. How to install

### OPTION 1 - Download from Release Page

[Download here](https://github.com/jerryuhoo/Fire/releases/latest)

### macOS Installation Guide for Fire Plugin

Thank you for downloading the **Fire** plugin!

#### ❗️Why macOS Blocks This Plugin

When you try to load the plugin in your DAW on macOS, you may see an error like:

> "Fire.vst3" cannot be opened because the developer cannot be verified.

This **does not mean** the plugin contains any virus or malicious code.

Instead, this is due to **Apple’s security policy**, which requires developers to:

- Enroll in the Apple Developer Program
- Pay **$99/year**
- Notarize and sign each build with an Apple-issued certificate

As an independent developer releasing **free** software, I currently do not have the budget to enroll in the paid program. Therefore, macOS treats this unsigned plugin as “unverified.”

---

#### ✅ How to Install and Use the Plugin

To use the Fire plugin on macOS, follow these steps to manually allow it.

##### 🔧 Step 1: Move the Plugin to the Correct Location

Copy the plugin files to the standard plugin folders:

```bash
# VST3
~/Library/Audio/Plug-Ins/VST3/

# Audio Unit (.component)
~/Library/Audio/Plug-Ins/Components/

# CLAP (if applicable)
~/Library/Audio/Plug-Ins/CLAP/
````

##### 🛡 Step 2: Remove Quarantine Attribute

macOS adds a quarantine flag to files downloaded from the internet. Remove it using Terminal:

```bash
xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Fire.vst3
xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Fire.component
```

If you're using the CLAP version:

```bash
xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/CLAP/Fire.clap
```

##### 📝 Step 3: Ad-Hoc Code Sign (Optional but Recommended)

Some DAWs still require the plugin to be signed, even if it’s local. You can apply an ad-hoc (self) signature:

```bash
codesign --deep --force --sign - ~/Library/Audio/Plug-Ins/VST3/Fire.vst3
codesign --deep --force --sign - ~/Library/Audio/Plug-Ins/Components/Fire.component
```

##### 🚀 Step 4: Launch Your DAW

Now open your DAW. The plugin should scan and load without issues.

---

#### ❤️ Support Independent Developers

If you appreciate this plugin and would like to support development, you can consider donating via PayPal or sharing the plugin with others.

Thank you for your understanding!

[![PayPal](https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif)](https://www.paypal.com/donate/?business=9NTZ9PADW6LYN&no_recurring=0&item_name=Thank+you+for+supporting+this+open-source+plugin%21+With+your+support%2C+I%E2%80%99ll+continue+improving+and+updating+it%21&currency_code=USD)

---

### ✅ OPTION 2 – Build with JUCE or CMake

The project pins **JUCE 9.0.2** in its `JUCE` submodule. Initialise submodules
before building; CMake and every Projucer exporter use this local copy.
To build plug-in bundles without installing them into the user's plug-in folders:

```sh
cmake -S . -B Builds/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DFIRE_INSTALL_PLUGINS=OFF
cmake --build Builds/Release --target Fire_AU Fire_VST3 Fire_CLAP -j6
```

The AU target is available on macOS. Built bundles are under
`Builds/Release/Fire_artefacts/Release/`. Set `FIRE_INSTALL_PLUGINS=ON` to retain
the project's usual automatic copy behaviour.

Use **Release** builds when auditioning in a host. Projucer Debug configurations
do not automatically replace the installed plug-in. Fresh CMake Debug and
multi-configuration builds default to `FIRE_INSTALL_PLUGINS=OFF`; an existing
cache or explicit option is preserved. Set `-DFIRE_INSTALL_PLUGINS=OFF` when
reusing an older build directory. Debug output remains available in the build
directory for debugger sessions.

To build AudioPluginHost with the same pinned JUCE version, build it separately:

```bash
cmake -S JUCE -B Builds/JUCE9-Host -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_BUILD_EXTRAS=ON
cmake --build Builds/JUCE9-Host --target AudioPluginHost --parallel
```

The macOS app is in
`Builds/JUCE9-Host/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/`.
Updating Projucer or rebuilding Fire does not update an existing AudioPluginHost.

#### 🔧 Using Projucer (JUCE GUI)

1. Open the `.jucer` file using **Projucer**.
2. Select your preferred IDE (Xcode, Visual Studio, etc.) in the *Exporters* tab.
3. Open the generated project and build the target named **Fire**.

> **Note:** You should choose **Release mode** rather than **Debug mode**.
> Debug builds may cause significantly higher CPU usage during audio processing.

If you don't have Projucer:

- You can build it from source at:
  `JUCE/extras/Projucer/Builds/...`
- Or download it from the [JUCE latest release](https://www.juce.com)

---

#### ⚙️ Using CMake (Recommended for CI and Advanced Users)

If you have CMake and Ninja installed:

```bash
cmake -S . -B Builds -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build Builds --config Release
```

This will generate the builds in the `Builds/` folder. You can then find the built plugin under:

```
Builds/Fire_artefacts/Release/
```

> **Tip:** Use `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` on macOS if you want to build for universal binaries.

## 3. User Manual

『Fire』has a top panel, a spectrogram, four graph visualizers, band effect, and global effect.

### 3.1. Top panel

- **HQ**: 4x oversampling for high quality audio.
- **A/B**: Switch between A/B to compare.
- **Copy**: Copy current preset parameters to another (A/B) panel.
- **Match**: Learn and hold separate loudness compensation for A and B.
- **Preset bar**: Choose your current preset.
- **Save**: Save your preset to user folder.
- **Menu**: Other settings including init, open preset folder, rescan preset folder, open GitHub page, check for new version.

#### Equal-loudness A/B listening

Loop a representative passage, then turn **Match** on and play for about three seconds.
The current side learns its processed level relative to the plug-in input,
then displays a fixed compensation value in dB. Switch **A/B** and replay the
same passage to learn the other side. Previously learned sides retain their
own compensation, and **Copy** copies the current side's compensation too.

Click the value/Learn button to measure again; click it during measurement to
cancel. The compensation stays fixed when you subsequently adjust controls.
Turn Match off to hear the original output level; changes use a short gain
fade. The Output knob, its automation, and the plug-in's reported latency stay
unchanged. Host bypass passes the original bypass signal and pauses learning.

Measurement uses K-weighted energy over a short window, accepting only frames
where both input and output contain usable audio. **Play audio** means there
was insufficient signal; a previous valid compensation is retained. Correction
is limited to **±18 dB**, with **LIMIT** shown when full matching is outside
that range. This is an audition aid, rather than an integrated LUFS meter.

Match settings and both learned gains are saved in the DAW project, separately
from sound-preset files. Loading a sound preset clears the current side's
measurement and learns again if Match is enabled. Older projects open with
Match off. Closing the editor retains the comparison state in the processor.

### 3.2. Spectrogram

- You can split up frequency to four bands for multiband distortion.

### 3.3. Band Effect

- You can click four switches on the right side of graphs.
- **Drive**:『Fire』 has several distortion functions, with **Gain Comp**, **Safe** and **Extreme** controls.
  - **Gain Comp**: Estimates a level reduction from the audible Drive gain while keeping Output independent. The amount appears beside the Drive control. Older projects retain **Legacy Link** until explicitly upgraded.
  - **Safe**: If your drive knob pushes your volume too loud, it will automatically reduce your drive value. It also shows reduced value on drive knob.
  - **Extreme**: It expands the range of the the drive knob (from around +40db to around +60db) when it is enabled to give more distortion.
- **Retification and bias**: Change your distortion shape.
- **Compressor**: You can change threshold, ratio, attack, and release for each band.
- **Stereo**: Change audio width and pan.
- **Output and mix**: For each bandm you can change the output and the mix.

### 3.4. LFO / Modulation

『Fire』 starts with four fully customizable LFOs and supports up to 16 sources.

- **LFO Selection**: Select a source in the scrollable left rail. Use **+** to add a source, or its remove button to delete it.
- **Rate & Sync**: The **Rate** knob controls the LFO speed. Click the **BPM** button to toggle synchronization with your DAW's tempo.
- **Smooth**: Adjusts the smoothness of the LFO shape.
- **Phase**: Adjusts the starting point (phase) of the LFO shape.
- **Grid**: The X and Y controls adjust the grid divisions in the editor for visual guidance and snapping.

#### LFO Editor

The main LFO display allows you to create complex modulation shapes. It has two primary modes: **Edit Mode** and **Brush Mode**.

**Edit Mode Controls:**

- **Add Point**: **Double-click** on an empty space in the editor to add a new point.
- **Delete Point**: **Double-click** on an existing point to delete it. Note: The first and last points cannot be deleted.
- **Move Point(s)**: **Click and drag** a point to move it.
- **Adjust Curve**: **Click and drag** the line segment between two points to adjust its curvature.
- **Select Multiple Points**:
  - Hold **Shift** and **drag** to draw a selection box (marquee) around multiple points.
  - Clicking on an already selected point allows you to drag the entire selection.
- **Snap to Grid**: Hold **Ctrl** (Windows) or **Cmd** (Mac) while dragging a point to snap it to the nearest grid line.
- **Context Menu**: **Right-click** anywhere in the editor to open a context menu with the following options:
  - **Select All**: Selects all points.
  - **Clear**: Resets the LFO to a default straight line.
  - **Copy / Paste**: Copies the current LFO shape and allows you to paste it into another LFO editor.
  - **Invert**: Flips the shape horizontally or vertically.

**Brush Mode Controls:**

- Activate **Brush Mode** to paint pre-defined shapes onto the grid.
- Use the dropdown menu next to the mode buttons to select a brush shape (e.g., Saw, Sine, Square).
- **Click and drag** within the editor grid to paint the selected shape. The brush will only respond to the **left mouse button**.

#### Modulation Matrix

- **Matrix Button**: Opens the Modulation Matrix window, where you can assign LFOs to control plugin parameters.
- **Assign Button**: Engages "Assign Mode." While active, the next parameter you click in the plugin will be automatically assigned to the currently selected LFO.

The matrix presents each routing from **Source → Destination → Depth**, with
signed percentage depth, bipolar/unipolar selection, an **Active** power button
and a remove icon. Use **Add routing** to create a connection; close the window
with its native close button or Escape. The column layout stays aligned while
scrolling and resizing.

Assigned knobs use a translucent source-coloured range band. A bright outlined
dot shows the current modulated value, while the neutral inner notch marks the
base value. Bypassed modulation is dimmed and does not show a moving dot.

### 3.5. Global Effect

- **Filter**: you can set lowcut, highcut, and peak. Lowcut and highcut each has four slopes (12, 24, 36, 48) you can choose.
- **Downsample**: downsample your audio.
- **Output and mix**: for global control.

## 4. Support the author

我也是一个音乐制作人，欢迎关注网易云音乐：[羽翼深蓝](https://music.163.com/#/artist?id=12118139)

[我的个人主页](https://www.bluewingsmusic.com/)（之后会推出其他的音频软件）

I am also a music producer (Artist name: 羽翼深蓝 - BlueWings). Check out my music here:

[Netease Music](https://music.163.com/#/artist?id=12118139)

[Apple Music](https://music.apple.com/us/artist/%E7%BE%BD%E7%BF%BC%E6%B7%B1%E8%93%9D/1696577755)

[Spotify](https://open.spotify.com/artist/0xi1eMyrrSXdZMX8n7Ilmt)

[YouTube](https://www.youtube.com/@bluewings-music)

[Other Softwares by Blue Wings Music](https://www.bluewingsmusic.com/)

## 5. References

5.1. A great example of using XML by [johnflynnjohnflynn](https://github.com/johnflynnjohnflynn/TestParameters02) to save presets to file example. I changed the code to save presets as mutiple files.

5.2. [Diode Clipping algorithm](https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227) from JUCE forum

5.3. [Spectrum Analyser](https://github.com/adriannaziel/SpectrumAnalyser_et) by adriannaziel

5.4. [SimpleEQ](https://github.com/matkatmusic/SimpleEQ) by matkatmusic

## 6. Update Notes

### 2025-10-18 (version 1.5.0)

This is a major feature update that introduces a comprehensive LFO modulation system and involves a significant refactoring of the plugin's core architecture.

1. **New Feature: LFO Modulation System** 🚀
    - **Four Independent LFOs**: Added a new LFO panel with four fully customizable LFOs that can modulate most parameters in the plugin.
    - **Customizable LFO Shapes**: Includes a powerful graphical editor to create custom LFO shapes. You can add, remove, and drag points, as well as adjust the curvature of segments between points.
    - **Brush & Edit Modes**: Two distinct editing modes for flexible workflow. Use "Edit Mode" for precise point-based editing and "Brush Mode" to quickly paint preset shapes (Saw, Sine, Square) onto the grid.
    - **Modulation Matrix**: A dedicated matrix panel to manage all modulation routings in one place.
    - **BPM Sync & Free (Hz) Modes**: Each LFO can be synchronized to the host's BPM or run freely in Hz.
    - **Quick Modulation Assignment**: A new "Assign" mode allows you to click any knob to instantly map it as a modulation target.
    - **Other New Features**: Features like attack / release for compressor, new pan for stereo, and more downsample controls!

2. **Major Code Refactoring**:
    - I refactored both UI and DSP code of this plugin so it is easier to modify and understand now.

3. **Bug Fixes**:
    - Fixed a critical bug where the plugin might not load presets correctly.
    - Fixed a bug to correct parameter and LFO state when adding/deleting bands.

4. **Performance Improvement**
    - Highly improved performance compared to v1.5.0b.
      - Refactored signal processing chain for significantly lower CPU usage.
      - Optimized Waterfall Spectrogram with smooth animation and minimal CPU load.
    - UI Improvements
      - Mouse Wheel Q-Control: Adjust filter Q values by hovering over nodes and scrolling.
      - Redesigned UI: A more logical layout with intuitive controls for a seamless workflow.
      - Unified Color Scheme: New professional look with improved visual comfort during long sessions.

### 2025-7-3 (version 1.0.2)

1. Add real-time value display on the global filter.
2. Refactor internal code structure to improve GUI rendering performance.
3. Fix a bug where VU meters were not visible in multiband (Band) mode.
4. Add a new setting to disable auto-update.
5. Fix phase issues in multiband mode:

   - Frequency response is now flatter and more phase-coherent across bands.
   - Fix phase shift issues when the Mix value is less than 1.

   > ⚠️ **Note:** These improvements may slightly alter the sound of older sessions. Please **freeze your audio** if you are using a previous version of the plugin.

   > ⚠️ **Note:** In the default mode, the algorithm is set to "Cubic." This is a nonlinear distortion algorithm, so even when the drive is set to 0, the algorithm still affects the sound. If you want to output a completely dry signal that is not affected by the algorithm, you can set the mix (in band mode) to 0. In that case, it will output a flat response.

6. For long-term product consistency, the manufacturer name has been changed to **Blue Wings Music**.

   - The new plugin will **not** appear under the old "Wings" manufacturer in your DAW.
   - You may need to **manually remove** the old version.
   - Whether the new version automatically replaces the old one **has not been fully tested**.
   - If it fails to replace the old plugin, please **save your presets** and re-import them in the new version.

7. Global Filter EQ gain range expanded from ±15 dB to ±24 dB. Add smoother to prevent noise when moving the global filter.
   > ⚠️ **Note:** Important: This is a breaking change.
Due to how DAWs store normalized parameter values, old presets and projects will sound different after the update if you used the global filter. I'm sorry about that 😭.

### 2024-5-22 (version 1.0.1)

1. Add a button for more Drive gain (E-Extreme).
2. Fix a bug of compression settings and width settings can not be set for each band independently.
3. Add original spectrum (white).
4. Improve GUI of VU meters.

### 2022-7-29 (version 1.0.0)

1. Refactor the code of frequency lines and close buttons.
2. Fix a crash in Fl Studio of v0.9.9.
3. Fix audiobuffer pointer bug when setting history array.
4. Fix the bug of changing presets that might cause wrong frequency line positions.
5. New close buttons for each band.
6. Other GUI improvements.
7. Add Extreme button for more distortion.
8. Check update when opening the plugin.
9. Support JUCE 7.
10. Fix a bug that changing presets doesn't reset new params.
11. Add a Limiter in the global mode.

### 2022-7-13 (version 0.9.9)

1. Fix the bug of freezing distortion graph before playing the audio. (in Logic Pro)
2. Fix the bug of global mix button phasing issue.
3. New function! When you move your mouse to the spectrogram, the peak frequency and decibel will be calculated.
4. New separate bypass buttons for each band.
5. Improve width graph, reduce CPU usage.
6. Fix the bug that close button sometimes doesn't work.

### 2022-6-29 (version 0.9.8)

1. Fix phasing issue in HQ mode.
2. Fix low frequency drop in HQ mode.

- v0.9.3 - v0.9.7 has those bugs

### 2022-6-22 (version 0.9.7)

1. Fix HQ not working bug in v0.9.6.
2. Add fade in/out of frequency text labels.

### 2022-5-28 (version 0.9.6)

1. Fix a crash when turn on HQ mode in mono channel mode in Logic Pro.
2. Improve graphs in mono channel mode.
3. Fix an issue where graphs are not refreshed before playing in Logic Pro.

### 2022-2-5 (version 0.9.5)

1. Smoothed the graph of spectrogram.
2. Fix Logic Pro crash.
3. Add GitHub CI.
4. Fix vst3 bug.
5. Fix A/B mode bug.

### 2021-11-18 (version 0.9.4)

Warning: Previous presets won't work, you have to resave your preset!

1. For VU meters and width graph, each represents to each band instead of global.
2. Add more details to VU meters.
3. Add "solo" buttons, change "enable" buttons to "bypass" buttons.
4. Remove "None" option in the distortion functions, because it causes bug(in version 0.9.3) and you can bypass the band now.
5. Add bypass buttons for compressor, width, filter, downsample.
6. Add 4 new distortion functions.

### 2021-10-2 (version 0.9.3)

Warning: Previous presets won't work, you have to resave your preset!

1. Redesign GUI.
2. Fix multiple bugs of multiband control(enable buttons, focus band, vertical lines, etc.), redesigned the code structure.
3. Add new filter control and filter graph.
4. Fix other bugs such as preset box selection, A/B mode, bypass repaint issue, etc.
5. Now you can click graph to zoom in and out.

### 2021-9-7 (version 0.9.2)

1. GUI improvement.
2. Add shape, width, and compressor groups.
3. Spectrogram display bug fixed.
4. Add VU meter and width graph.
5. Add frequency label, and frequency automation bug fixed.

### 2021-1-24 (version 0.9.1)

1. Drive knob reduction and GUI improvement
Fix a bug that in global mode menu won't change when selecting different bands.

### 2021-1-23 (version 0.9.0)

1. First Beta release.

## 7. License

From v.0.9.4, I changed the license to AGPL-3.0.

### What can you do with the source

- Currently, up to version 0.9.4 is free for music producers, audio programmers who start to learn JUCE. You can fork, modify my code, but projects that used my code must be open-source.

### Things you can't do with this source

- Do not create an app and distribute it on the iOS app store.

- Do not use the name "Fire", "Wings", or "羽翼深蓝Wings" for marketing or to name any distribution of binaries built with this source. This source code does not give you rights to infringe on trademarks. If you wanna use it for commercial, please send me an email. Otherwise, your software has to be open-source.

## 8. Acknowledgement

@[IcyLeaves](https://github.com/IcyLeaves)

### Per-band OTT

OTT is an independent upward/downward dynamics module in **Band Lab**. It uses Fire's existing one-to-four frequency bands and runs after Compressor and before Stereo, with no additional latency. The dynamics concept is described in the [Ableton Multiband Dynamics reference](https://www.ableton.com/en/manual/live-audio-effect-reference/#multiband-dynamics).

1. Add or move crossover points in the spectrum using the existing band controls.
2. Select **OTT** in Band Lab, choose a band, and enable that band's OTT power button.
3. Drag the spectrum's lower (upward) and upper (downward) threshold lines vertically. Dragging another band's line selects that band. A fading **Up** or **Down** readout identifies the threshold during editing. The lines share the spectrum's dB scale; double-click resets a threshold, and arrow keys allow precise changes.

| Control | Function |
| --- | --- |
| Depth | Strength of both upward and downward compression, from 0 to 1. |
| Time | Attack/release scale, 10–400%. At 100%, the linked peak detector uses 5 ms attack and 100 ms release. |
| Up Thresh | Raises detail below this level. Remains at least 6 dB below Down Thresh when edited. |
| Down Thresh | Compresses signals above this level. |
| Gain | OTT output trim, −24 to +24 dB. |
| Mix | Blends this band's OTT signal with its input; zero is transparent. |

The stage uses 4:1 upward and 8:1 downward ratios with soft knees. Upward boost is limited to 24 dB and tapers off near the noise floor. Both channels share the detector and gain, preserving the stereo image. Parameters can be configured while OTT is bypassed, and all six continuous controls support LFO routing and host automation.

OTT uses a blue-violet palette distinct from Compressor. Threshold lines have no circular handles or permanent numeric labels. OTT knob values and the band's Output/Mix values fade in during a drag, keyboard focus or text editing, then fade out when the interaction ends. The dynamics panel retains slim input/gain meters and reveals exact readings during OTT edits.

Ribbons rise for upward compression and sink for downward compression. Their shape, local opacity and saturation follow the live logarithmic spectrum; motion strength follows wet-weighted dynamics activity, excluding output trim. Higher-frequency energy produces faster ripples. Dragging a threshold previews its direction even without audio, and the preview fades away on release. Stale audio telemetry settles back to idle.

OTT settings participate in presets, A/B comparisons and band copying. Older projects load with OTT disabled, preserving their existing processing. Existing parameter IDs and automation indices are retained.

### Master and Band insert effects

Use **CHAIN → +** in either **Master Lab** or **Band Lab** to select an effect. Built-in Drive, Shape, Compressor, OTT, Stereo, Master EQ and Master Lo-Fi entries enable and select their existing module. Additional effects use eight independent insert slots per chain and can have multiple instances. The module rail keeps five fixed-height rows visible and scrolls as more effects are added. The selected module scrolls into view automatically.

| Effect | Controls |
| --- | --- |
| Chorus | Rate, Depth, Delay, Feedback, Width, Mix. |
| Delay | Time (10–2000 ms), Feedback, Tone, Ping-Pong, tempo Sync, Mix. Sync offers 1/16 through one bar, including dotted eighth/quarter notes, within the two-second delay capacity. |
| Reverb | Size, Damping, Pre-delay, Width, Low Cut, Mix. |
| Granular | Clouds: Position, Size, Pitch (±24 semitones), Density, Texture, Mix, Spread, Feedback, Reverb and Freeze. Clouds is the sole granular engine. |
| Lo-Fi insert | Rate, Bits, Tape, Wow, Flutter, Mix. |

All Granular inserts use a port of Mutable Instruments' **Clouds normal granular
mode**, with its grain scheduler, window shapes, diffusion, feedback and reverb.
Density selects regular grains to the left, random grains to the right, and no
new grains in the centre. Texture moves from sharp to smooth grain envelopes,
then adds diffusion. Position selects progressively older audio. The expanded
Clouds control page uses the space normally occupied by the waveform display.

Freeze holds the recording while grains continue to play. Its switch is saved,
but recorded audio is not stored in presets, A/B snapshots or host projects.
After an empty reset, Freeze captures a fresh buffer of audio before holding it.
Spread, Feedback and Reverb support host automation and LFO modulation.

The Clouds core runs at the original 32 kHz, stereo/16-bit quality, with
band-limited conversion to and from the host sample rate. Its wet bandwidth and
intentional granular texture follow that design. Fire retains its transparent
dry path and linear Mix control. The alternate stretch, looping-delay and
spectral modes, hardware trigger input and lower-fidelity quality modes are not
included. This is a desktop adaptation, not a claim of bit-identical hardware
emulation. Upstream source revisions, licensing and portability fixes are
documented in [the Clouds port notes](Source/DSP/Clouds/README.md).
Regression coverage and reproducible CPU measurements are in
[the Clouds validation report](benchmarks/CLOUDS_PORT.md).

Granular now uses Clouds exclusively; the engine selector and the original Fire
granular DSP have been removed. Older Fire granular states are converted once to
Clouds controls on load. Pitch and Mix are preserved; Size, Density, Position and
Spray are mapped to the closest supported Clouds controls. This migration changes
the sound of old granular presets. Existing Clouds presets keep their values.
The old engine parameter IDs remain inert reserved slots so later automation
indices do not move; they cannot select another engine.

The **Master Lo-Fi** menu entry opens the same built-in page, with Rate, Bits, Jitter, Mix, Tape, Wow and Flutter; it does not create a second Master Lo-Fi variant. Historical Lo-Fi inserts remain editable with their original parameter ranges and use the same two-row control grouping. Tape adds saturation and a softer high-frequency response; Wow introduces slow pitch drift and Flutter adds faster pitch variation. Zero Tape/Wow/Flutter preserves the previous Lo-Fi processing.

Built-in modules and insert effects run in the same freely reorderable list, before the chain's Output/Mix controls. Drag any module, including Drive, Shape, Compressor, Stereo, OTT, Master EQ and Lo-Fi, to change its processing position. Analysis is a movable display page and does not process audio. Older presets retain their original audio order (OTT before Stereo, and Master Lo-Fi before EQ); the list now displays that order. Drive and Shape retain their legacy combined processing when adjacent in that order, and run as separate stages when moved apart. Hover an inserted effect to reveal its remove button, or drag its name/body to reorder it. A floating preview and insertion line indicate the destination; holding near the list edges scrolls to offscreen effects. The new order is committed once on release. Escape, dropping outside the list or changing workspace/band cancels the drag. Removing the selected effect selects its nearest remaining neighbour. The power button bypasses each effect, and the context menu also provides move/remove actions. Reordering retains that slot's parameters and LFO assignments. New effect values fade in while editing; the waveform shows the selected band's output or the master output.

Every insert retains its six normalized host controls with effect-specific labels and units in Fire's UI; Clouds adds a separately versioned set of controls. LFO modulation, presets, host state, A/B comparisons and band copying include the new controls. New host parameters are appended with a newer AU version hint, and older projects load empty insert racks with Tape/Wow/Flutter at zero. The insert stages add no reported processing latency; delay, pitch modulation, granular playback and reverb create their intended time offsets and tails in the wet signal. Tail reporting reserves conservative bounds so hosts do not cut long echoes prematurely; a frozen Clouds buffer or maximum feedback reports an infinite tail.


### Graph views and interactive EQ

The Band Lab **View** dropdown switches between Waveform, Transfer, Meters and
Stereo. **Auto** follows the selected module. A manual choice remains selected
when changing modules or bands; dragging Drive temporarily previews Transfer
and restores the previous view afterward. Hidden graphs stop their display timers.

Master **EQ** supports up to 12 points. Double-click an empty area of the spectrum
to add a bell at that frequency and gain, or use the **+** in the EQ panel. Drag a
point to change frequency/gain; scroll over it for Q. Select a point to show its
Frequency, Gain, Q, Filter Type and Slope controls. The numbered selected dot in
the bottom navigation grows smoothly; click another dot, or use left/right arrows
while a dot has keyboard focus, to change points. **−** removes the selected point;
Delete/Backspace also removes it when the spectrum has focus.

Types are Bell, Low Cut, High Cut, Low Shelf, High Shelf, Notch and Band Pass.
Slope is available for cut filters (12/24/36/48 dB per octave); Gain is unavailable
for Notch and Band Pass. Points can be bypassed individually. The EQ remains
editable while its global power is off; adding a point enables it for audition.

The original three points keep their parameter IDs, ranges and filter response,
including the cut filters' gain/Q shaping. New points occupy fixed parameter
slots: deleting a neighbour does not move automation or LFO assignments. EQ points
and routing are saved in presets, A/B states and host projects. Adding/removing
an EQ point leaves other effects' delay tails and Clouds Freeze recordings intact.

### LFO bank and control sizing

The **LFO BANK** rail uses the same width and row spacing as the Band Lab chain.
Use **+** to add a source (up to 16), and hover or focus a row to reveal its remove
button. The rail scrolls without shrinking its rows and brings the selected
source into view. All sources may be removed; the empty bank offers **+** to
start again.

Slots keep their identities: deleting LFO 2 does not renumber LFO 3 or move its
routes. Removing a source clears its routes; other sources and audio-effect tails
continue. Reusing a slot starts with a fresh shape and timing settings. Only
present sources appear in Assign and Matrix menus. Old projects retain their
original four sources; presets, A/B and host state save the expanded bank.

EQ, LFO, insert, band and master ordinary rotaries share the same control and dial
sizes at each UI scale. The main Drive control retains its larger emphasis size.

The large Drive dial uses a single proportional pointer; its modulation range
and source badge stay visible without extra pointer marks. In Shape Forge, a
short gradient highlight follows the LFO curve with a soft trailing glow. It uses
the phase actually read by the audio engine, including phase offsets and timing
corrections. The glow wraps across the cycle boundary, stays below edit handles,
and follows free-running audio even when transport is stopped. When audio
callbacks stop, it holds the last real position and fades out. Waveform and glow
masks are cached; movement redraws only the old and new highlight regions.

### Light, motion and analysis displays

Shape Forge uses a compact icon toolbar: a grid opens **Matrix**, connected
points select **Edit Mode**, and a brush selects **Brush Mode**. **Assign** keeps
its routing icon and a text confirmation so the armed, assigned and full states
remain explicit. Hover tooltips, keyboard focus and accessible names explain
each control. Active tools pick up the selected LFO colour; switching edit modes
keeps the toolbar and curve in place.

Waveforms use warm left and cool right traces, a restrained glow and a bright
live endpoint. Pixel buckets retain the extrema in the available display
history, preserving brief transients that point sampling could miss. Both
channels share a bounded display gain, so quiet signals stay quiet and the
stereo balance remains visible. Silent sections return to faint reference lines.

The spectrum uses a fine bright edge with a subtle fill and a slower release.
Peak-preserving pixel reduction keeps narrow peaks aligned with their dB
readouts. Traces settle below the display floor, and bypass clears held peaks as
well as live data. Graph cards use a soft hover edge and an expand/restore icon.
All motion follows signal data or interaction; static geometry is cached and
hidden displays stop their animation timers.

### Flanger and Phaser

Both **Band Lab** and **Master Lab** offer **Flanger** and **Phaser** in the
CHAIN **+** menu. Each uses a normal insert slot, so multiple instances can be
combined, reordered, bypassed and removed independently. Their six controls
support host automation and LFO assignment; presets, A/B and band copies retain
the complete setup.

- **Flanger** mixes a swept short delay with the dry signal for a comb-like,
  metallic sweep. Controls: **Rate**, **Depth**, **Delay**, **Feedback**,
  **Width**, **Mix**.
- **Phaser** mixes a six-stage all-pass chain with the dry signal for moving
  notches and a rounded, swirling sweep. Controls: **Rate**, **Depth**,
  **Center**, **Feedback**, **Width**, **Mix**.

Start with Mix at 50% to hear the cancellations clearly. Width offsets the
internal sweep between left and right; set it to zero for a centred sweep.
Negative feedback gives an alternative resonance character. Mix at zero and
bypass retain the dry signal; bypass and ordinary parameter changes fade
smoothly. These effects add no reported processing latency.
When Flanger starts or resumes from a fully cleared bypass, it first fills its
short delay under dry audio, then fades the wet signal in. This avoids exposing
the edge of an empty delay buffer while inserting an effect on sustained audio.

Existing effect Type automation retains its six original choices and values.
The new algorithms use an appended **Modulation Type** host selector
(Standard / Flanger / Phaser); the plugin's CHAIN menu handles it automatically.
Old presets and projects load with Standard selected.

### Chord Resonator

Add **Chord Resonator** from the CHAIN **+** menu in **Band Lab** or **Master
Lab** to excite a tuned harmonic bank with the incoming audio. Multiple
instances can be reordered, bypassed and saved independently, just like other
inserts. Band instances listen to their selected band's signal; Master listens
to the complete signal at its position in the chain.

Choose **Root** (C2–C5; middle C is C4) and **Chord** from the two selectors.
The eight voicings are Major, Minor, Major 7, Minor 7, Sus 2, Sus 4, Fifth and
Octave. The note readout shows the chosen voicing. Four ordinary rotaries shape
the result:

- **Color** sets the balance of the fundamental and upper harmonics.
- **Decay** sets the nominal time for the resonances to decay by 60 dB
  (0.05–3 seconds).
- **Width** spreads the resonant voices in stereo; zero gives each voice equal
  left/right gain while retaining the input's stereo channels.
- **Mix** blends the dry attack with the resonant sound.

Start with a harmonically rich bass or a rhythmic noise/percussion source and
the default C3 Minor 7 voicing. To keep the sub intact, place the effect on a
mid/high band. It colours frequencies excited by the input; it does not
automatically detect a key or retune arbitrary audio. No MIDI or external
carrier is needed. Root/chord changes crossfade, while continuous parameters
support LFO modulation and host automation. The effect adds no fixed processing
latency, and silence produces only the existing decaying tail.
The wet path removes DC bias, and controlled gain transitions prevent stored
harmonics from causing a sudden level jump when Color or Width is increased.

The previous Type and Modulation Type host selectors retain their ranges and
automation positions. A new per-slot **Chord Resonator** flag selects this
algorithm; the CHAIN menu manages it automatically. Older presets and projects
load with these flags off.

### Drive level compensation

**Gain Comp** lives on the Drive page and estimates a level reduction from the
Drive gain actually being applied, including its LFO modulation, Safe limiting,
Extreme mode and bypass transition. At ordinary, unrestricted settings it
retains the approximate **Drive 60 → −6 dB** relationship. This is an estimate
for convenient sound design, rather than a loudness measurement.

The reduction is applied at the end of the Drive stage (or the adjacent joined
Drive/Shape stage). Output stays independently adjustable, and later effects
receive the compensated signal. Changing Gain Comp no longer directly turns
down an already-recorded tail in a downstream delay or reverb. Bypassing Drive
returns its compensation smoothly to unity; a joined Shape Mix at zero also
removes compensation for the inaudible Drive contribution. Reordering Drive
moves its compensation with it. Compressor remains a separate, reorderable
effect.

The readout shows the applied gain while audio is running. **≈** marks a base
Drive/Extreme estimate when current audio telemetry is unavailable; it does not
pretend to track an unheard LFO or Safe limiting. **Off** and **Bypassed** make
inactive states explicit. Use **Match / Learn** afterwards for an actual
whole-chain listening-level comparison; its learned adjustment remains fixed
until learned again.

New instances and newly created bands use Gain Comp. Existing presets and
projects retain **Legacy Link**, including its old override of the stored
Output setting. Their Drive page offers **Use Drive Comp** to explicitly switch
that band to the new behavior and enable compensation. This restores the stored
manual Output value and changes the compensation's position, so the sound can
change. Merely opening the editor never upgrades an old project. Both modes
are saved independently per band and on both A/B sides; existing parameter IDs
and automation positions remain unchanged.

### Vector branding and audio-driven fire

The header and Settings page use a vector **火** mark based on the original
Fire logo. The header's Wings signature uses the original angular wing motif.
Editable transparent SVG versions live in `assets/images/firelogo.svg` and
`assets/images/firewingslogo.svg`; the UI draws cached paths at the current scale.

The Fire mark's warm edge, small flame tongues and sparks follow the global
output RMS and peaks. Loud sounds and attacks produce stronger motion, while
quiet sounds stay restrained. It works with live input even when the transport
is stopped and is independent of the selected band. Silence or stopped audio
callbacks settle to a still gold mark. Hidden editors discard old fire energy
and require fresh audio after reattachment. This shares the existing UI clock,
stops header animation repaints once settled, and adds no audio-thread work.
