# Fire (Version 0.722)
![Alt text](Fire.png?raw=true "Title")

 This is a distortion plugin.
 This plugin has 9 different distortion modes:
 - Arctan Soft Clipping
 - Exp Soft Clipping
 - Tanh Soft Clipping
 - Cubic Soft Clipping
 - Hard Clipping
 - Sausage Fattener
 - Sin Fold
 - Linear Fold
 - Diode Clipping
 
 
9 knobs:
 - Drive knob
 - Downsample knob
 - Rectification knob
 - Color knob
 - Bias knob
 - Output knob
 - Cutoff knob
 - Q (Resonance) knob
 - Mix knob
 
 
6 options for the filter
 - Off
 - Pre
 - Post
 - Low-pass
 - Band-pass
 - High-pass
 
 And other features such as HQ, link mode, safe mode
 
Hope you like it!

# Thanks for your support
欢迎关注网易云音乐：羽翼深蓝Wings！（这次一定！）

follow my Neteast channel: 羽翼深蓝Wings

![image](neteast.png)

支付宝赞助！

![image](alipay.png)

# Reference
1. The level meter used ff_meters by Daniel Walz.
Find the repository here: https://github.com/ffAudio/ff_meters
2. A great example of using XML to save presets to file example. I changed the code to save presets as mutiple files.
https://github.com/johnflynnjohnflynn/TestParameters02
3. Diode Clipping 1 is an algorithm from https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227

# Release Notes:

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
