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
- **Drive**:『Fire』 has several distortion functions. There are three buttons "L", "S", "E", related to drive knob, and each means "Link", "Safe", and "Extreme".
  - **Link**: When your turn up drive knob, the output knob value will automatically reduce.
  - **Safe**: If your drive knob pushes your volume too loud, it will automatically reduce your drive value. It also shows reduced value on drive knob.
  - **Extreme**: It expands the range of the the drive knob (from around +40db to around +60db) when it is enabled to give more distortion.
- **Retification and bias**: Change your distortion shape.
- **Compressor**: You can change threshold, ratio, attack, and release for each band.
- **Stereo**: Change audio width and pan.
- **Output and mix**: For each bandm you can change the output and the mix.

### 3.4. LFO / Modulation

『Fire』 includes four fully customizable LFOs that can be used to modulate various parameters.

- **LFO Selection**: On the left, click the "LFO 1" through "LFO 4" buttons to select which LFO you are editing.
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

Use **MODE → +** in either **Master Lab** or **Band Lab** to add an effect. Each master/band chain has eight independent insert slots and can contain multiple instances of the same effect. The module rail keeps five fixed-height rows visible and scrolls as more effects are added. The selected module scrolls into view automatically.

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

The existing **Master Lo-Fi** page retains Rate, Bits, Jitter and Mix, and adds **Tape**, **Wow** and **Flutter**. Tape adds saturation and a softer high-frequency response; Wow introduces slow pitch drift and Flutter adds faster pitch variation. Zero Tape/Wow/Flutter preserves the previous Lo-Fi processing.

Built-in modules and insert effects run in the same freely reorderable list, before the chain's Output/Mix controls. Drag any module, including Drive, Shape, Compressor, Stereo, OTT, Master Filter and Lo-Fi, to change its processing position. Analysis is a movable display page and does not process audio. Older presets retain their original audio order (OTT before Stereo, and Master Lo-Fi before Filter); the list now displays that order. Drive and Shape retain their legacy combined processing when adjacent in that order, and run as separate stages when moved apart. Hover an inserted effect to reveal its remove button, or drag its name/body to reorder it. A floating preview and insertion line indicate the destination; holding near the list edges scrolls to offscreen effects. The new order is committed once on release. Escape, dropping outside the list or changing workspace/band cancels the drag. Removing the selected effect selects its nearest remaining neighbour. The power button bypasses each effect, and the context menu also provides move/remove actions. Reordering retains that slot's parameters and LFO assignments. New effect values fade in while editing; the waveform shows the selected band's output or the master output.

Every insert retains its six normalized host controls with effect-specific labels and units in Fire's UI; Clouds adds a separately versioned set of controls. LFO modulation, presets, host state, A/B comparisons and band copying include the new controls. New host parameters are appended with a newer AU version hint, and older projects load empty insert racks with Tape/Wow/Flutter at zero. The insert stages add no reported processing latency; delay, pitch modulation, granular playback and reverb create their intended time offsets and tails in the wet signal. Tail reporting reserves conservative bounds so hosts do not cut long echoes prematurely; a frozen Clouds buffer or maximum feedback reports an infinite tail.
