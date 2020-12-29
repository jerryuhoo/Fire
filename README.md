# Fire (Version 0.80) [![](https://travis-ci.com/jerryuhoo/Fire.svg?branch=master)](https://travis-ci.com/jerryuhoo/Fire) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/8c68fa4c8da04cb8abca88e2dfceb280)](https://www.codacy.com/gh/jerryuhoo/Fire/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=jerryuhoo/Fire&amp;utm_campaign=Badge_Grade)

![Alt text](Fire1.png?raw=true "Title")

## 1. Introduce

 This is a multi-band distortion plugin. Fire supports DAWs such as Ableton Live, Fl Studio...

Hope you like it!

## 2. How to install

### Download from Release Page

currently not avaliable

### Build by JUCE

Open .jucer file by Projucer then select your IDE to build Fire. If you don't have projucer, you can go to JUCE/extras/Projucer/Builds/... and build it, or download from the <a href="https://www.juce.com" rel="nofollow">latest JUCE release</a>.</li>

Note: You should choose Release mode rather than Debug mode(or disable Debug mode) in .fire Exporters page. Build with debug mode will cause high CPU usage.

## 3. Support the author

欢迎关注网易云音乐：<a href="https://music.163.com/#/artist?id=12118139">羽翼深蓝Wings</a>（这次一定！）

follow my Neteast channel: 羽翼深蓝Wings

<div align=center>
    <img src="support.png" width = "50%"/>
</div>

## 4. References

1. The level meter used <a href="https://github.com/ffAudio/ff_meters">ff_meters</a> by Daniel Walz.

2. A great example of using XML by <a href="https://github.com/johnflynnjohnflynn/TestParameters02">johnflynnjohnflynn</a> to save presets to file example. I changed the code to save presets as mutiple files.

3. <a href="https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227">Diode Clipping algorithm</a> from JUCE forum

4. <a href="https://github.com/adriannaziel/SpectrumAnalyser_et">Spectrum Analyser</a> by adriannaziel

## 5. Update Notes

2020-7-15 (version 0.8)

1. multiband distortion
2. spectrum graph
3. more knobs

2020-7-15 (version 0.7)

1. Added more knobs and functions
2. Added title bar that contains HQ button, A/B modes, and presets menu.
3. Redesigned the GUI

2020-6-16 (version 0.5)

1. Added a linked bottom between drive and output knob.

2020-2-20 (version 0.32)

1. Added 3 filter modes (low-pass, band-pass, high-pass)
2. Added 3 filter states (off, pre, post)

2020-2-20 (version 0.3)

1. Used smoothedValue to eliminate zipper noise when twist knobs quickly.
2. Added lin fold and downsample mode
3. Redesigned the GUI

2020-1-1 (version 0.28)

1. removed drive knob, use input volume knob as the new drive knob, which ranges in (-36, 36) db.
2. Fixed some display problems. Now you can see the change of graph when you modify drive knob and mix knob.
3. Made the windows resizable.
4. Added input knob, change the location of mix and output knob. Now the output knob is before the mix knob so that it doesn't affect mix knob when mix = 0.
5. Fit JUCE v5.7.4
6. Seperated distortion module into Distortion.cpp and Distortion.h
7. Changed "mode" into interger

2019-12-30 (version 0.255)

1. Added one more screen to display distortion modes.
2. Added global color (not in the user interface)
3. Added more distortion modes.
