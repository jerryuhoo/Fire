# Fire (Version 0.9.2) [![](https://travis-ci.com/jerryuhoo/Fire.svg?branch=master)](https://travis-ci.com/jerryuhoo/Fire) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/8c68fa4c8da04cb8abca88e2dfceb280)](https://www.codacy.com/gh/jerryuhoo/Fire/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=jerryuhoo/Fire&amp;utm_campaign=Badge_Grade)

![Alt text](Fire1.png?raw=true "Title")

![Alt text](Fire2.png?raw=true "Title")

## 1. Introduce

 This is a multi-band distortion plugin. 『Fire』 supports DAWs such as Ableton Live, Fl Studio...

Hope you like it!

## 2. How to install

### Download from Release Page

<a href="https://github.com/jerryuhoo/Fire/releases/latest">Download here</a>

### Build by JUCE

Open .jucer file by Projucer then select your IDE to build 『Fire』. If you don't have projucer, you can go to JUCE/extras/Projucer/Builds/... and build it, or download from the <a href="https://www.juce.com" rel="nofollow">latest JUCE release</a>.</li>

Note: You should choose Release mode rather than Debug mode(or disable Debug mode) in .fire Exporters page. Build with debug mode will cause high CPU usage.

## 3. Support the author

欢迎关注网易云音乐：<a href="https://music.163.com/#/artist?id=12118139">羽翼深蓝Wings</a>

follow my Neteast channel: 羽翼深蓝Wings

<div align=center>
    <img src="support.png" width = "50%"/>
</div>

## 4. References

1. A great example of using XML by <a href="https://github.com/johnflynnjohnflynn/TestParameters02">johnflynnjohnflynn</a> to save presets to file example. I changed the code to save presets as mutiple files.

2. <a href="https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227">Diode Clipping algorithm</a> from JUCE forum

3. <a href="https://github.com/adriannaziel/SpectrumAnalyser_et">Spectrum Analyser</a> by adriannaziel

## 5. Update Notes

2020-9-7 (version 0.9.2)

1. GUI improvement.
2. Added shape, width, and compressor groups.
3. Spectrogram display bug fixed.
4. Added VU meter and width graph.
5. Added frequency label, and frequency automation bug fixed.

2020-1-24 (version 0.9.1)

1. Drive knob reduction and GUI improvement
Fixed a bug that in global mode menu won't change when selecting different bands.

2020-1-23 (version 0.9.0)

1. First Beta release.
