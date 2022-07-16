# Fire (Version 0.9.9) [![](https://travis-ci.com/jerryuhoo/Fire.svg?branch=master)](https://travis-ci.com/jerryuhoo/Fire) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/8c68fa4c8da04cb8abca88e2dfceb280)](https://www.codacy.com/gh/jerryuhoo/Fire/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=jerryuhoo/Fire&amp;utm_campaign=Badge_Grade)[![CMake Build Matrix](https://github.com/jerryuhoo/Fire/actions/workflows/fire.yml/badge.svg)](https://github.com/jerryuhoo/Fire/actions/workflows/fire.yml)

![Alt text](Fire1.png?raw=true "Title")

![Alt text](Fire2.png?raw=true "Title")

## 1. Introduce

This is a multi-band distortion plugin 『Fire』. It can be used in DAWs which supports AU and Vst3 plugins such as Ableton Live, Fl Studio, etc.

Demo video:

(YouTube) <https://youtu.be/sccW_8W_0o4>

(Bilibili) <https://www.bilibili.com/video/BV17f4y1M7Hm>

:musical_note: Home Page for Wings Music: [Wings Music](https://jerryuhoo.github.io/)

Hope you like it!

## 2. How to install

### Download from Release Page

[Download here](https://github.com/jerryuhoo/Fire/releases/latest)

### Build by JUCE

Open .jucer file by Projucer then select your IDE to build 『Fire』. If you don't have projucer, you can go to JUCE/extras/Projucer/Builds/... and build it, or download from the [latest JUCE release](https://www.juce.com)

Note: You should choose Release mode rather than Debug mode(or disable Debug mode) in .fire Exporters page. Build with debug mode will cause high CPU usage.

## 3. User Manual

『Fire』has a top panel, a spectrogram, four graph visualizers, band effect, and global effect.

### 3.1. Top panel

* HQ: 4x oversampling for high quality audio.
* A/B: switch between A/B to compare.
* Copy: copy current preset parameters to another(A/B) panel.
* Preset bar: choose your current preset.
* Save: save your preset to user folder.
* Menu: other settings including init, open preset folder, rescan preset folder, open GitHub page, check for new version.

### 3.2. Spectrogram

* You can split up frequency to four bands for multiband distortion.

### 3.3. Graph Visualizer

* There are four graph visualizers. You can click on each graph to zoom in or zoom out.
* Top left is oscilloscope.
* Top right is distortion graph.
* Buttom Left is VU meters.
* Buttom right is Width graph.

### 3.4. Band Effect

* You can click four switches on the right side of graphs.

* Drive:『Fire』 has several distortion functions. There are two buttons "L" and "S" related to drive knob, and each means "Link" and "Safe". "Link" means when your turn up drive knob, the output knob value will automatically reduce. "Safe" means if your drive knob pushes your volume too loud, it will automatically reduce your drive value. It also shows reduced value on drive knob.

* Retification and bias: Change your distortion shape.

* Compressor: you can change threshold and ratio for each band.

* Width: change audio width.

* Output and mix: for band control.

### 3.5. Global Effect

* Filter: you can set lowcut, highcut, and peak. Lowcut and highcut each has four slopes (12, 24, 36, 48) you can choose.

* Downsample: downsample your audio.

* Output and mix: for global control.

## 4. Support the author

我也是一个音乐制作人，欢迎关注网易云音乐：[羽翼深蓝Wings](https://music.163.com/#/artist?id=12118139)

I am also a music producer.
Netease channel: [羽翼深蓝Wings](https://music.163.com/#/artist?id=12118139)
SoundCloud: [Wings](https://soundcloud.com/jerry-876742699)

<div align=center>
    <img src="support.png" width = "50%"/>
</div>

## 5. References

5.1. A great example of using XML by [johnflynnjohnflynn](https://github.com/johnflynnjohnflynn/TestParameters02) to save presets to file example. I changed the code to save presets as mutiple files.

5.2. [Diode Clipping algorithm](https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227) from JUCE forum

5.3. [Spectrum Analyser](https://github.com/adriannaziel/SpectrumAnalyser_et) by adriannaziel

5.4. [SimpleEQ](https://github.com/matkatmusic/SimpleEQ) by matkatmusic

## 6. Update Notes

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

[v0.9.3 - v0.9.7 has those bugs]

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

* Currently, up to version 0.9.4 is free for music producers, audio programmers who start to learn JUCE. You can fork, modify my code, but projects that used my code must be open-source.

### Things you can't do with this source

* Do not create an app and distribute it on the iOS app store.

* Do not use the name "Fire", "Wings", or "羽翼深蓝Wings" for marketing or to name any distribution of binaries built with this source. This source code does not give you rights to infringe on trademarks. If you wanna use it for commercial, please send me an email. Otherwise, your software has to be open-source.
