# Fire (Version 0.9.3) [![](https://travis-ci.com/jerryuhoo/Fire.svg?branch=master)](https://travis-ci.com/jerryuhoo/Fire) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/8c68fa4c8da04cb8abca88e2dfceb280)](https://www.codacy.com/gh/jerryuhoo/Fire/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=jerryuhoo/Fire&amp;utm_campaign=Badge_Grade)

![Alt text](Fire1.png?raw=true "Title")

![Alt text](Fire2.png?raw=true "Title")

## 1. Introduce

 This is a multi-band distortion plugin 『Fire』. It can be used in DAWs which supports AU and Vst3 plugins such as Ableton Live, Fl Studio, etc.

Hope you like it!

## 2. How to install

### Download from Release Page

<a href="https://github.com/jerryuhoo/Fire/releases/latest">Download here</a>

### Build by JUCE

Open .jucer file by Projucer then select your IDE to build 『Fire』. If you don't have projucer, you can go to JUCE/extras/Projucer/Builds/... and build it, or download from the <a href="https://www.juce.com" rel="nofollow">latest JUCE release</a>.</li>

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

欢迎关注网易云音乐：<a href="https://music.163.com/#/artist?id=12118139">羽翼深蓝Wings</a>

My Neteast channel: <a href="https://music.163.com/#/artist?id=12118139">羽翼深蓝Wings

<div align=center>
    <img src="support.png" width = "50%"/>
</div>

## 5. References

5.1. A great example of using XML by <a href="https://github.com/johnflynnjohnflynn/TestParameters02">johnflynnjohnflynn</a> to save presets to file example. I changed the code to save presets as mutiple files.

5.2. <a href="https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227">Diode Clipping algorithm</a> from JUCE forum

5.3. <a href="https://github.com/adriannaziel/SpectrumAnalyser_et">Spectrum Analyser</a> by adriannaziel

5.4. <a href="a href=https://github.com/matkatmusic/SimpleEQ">SimpleEQ</a> by matkatmusic

## 6. Update Notes

### 2020-10-2 (version 0.9.3)

Warning: Previous presets may not work, you have to resave your preset!

1. Redesign GUI.
2. Fixed multiple bugs of multiband control(enable buttons, focus band, vertical lines, etc.), redesigned the code structure.
3. Added new filter control and filter graph.
4. Fixed other bugs such as preset box selection, A/B mode, bypass repaint issue, etc.
5. Now you can click graph to zoom in and out.

### 2020-9-7 (version 0.9.2)

1. GUI improvement.
2. Added shape, width, and compressor groups.
3. Spectrogram display bug fixed.
4. Added VU meter and width graph.
5. Added frequency label, and frequency automation bug fixed.

### 2020-1-24 (version 0.9.1)

1. Drive knob reduction and GUI improvement
Fixed a bug that in global mode menu won't change when selecting different bands.

### 2020-1-23 (version 0.9.0)

1. First Beta release.
