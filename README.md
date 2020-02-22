# Blood
![Alt text](Blood.png?raw=true "Title")

 This is a distortion plugin.
 This plugin has 8 different distortion modes:
 - Arctan Soft Clipping
 - Exp Soft Clipping
 - Tanh Soft Clipping
 - Cubic Soft Clipping
 - Hard Clipping
 - Sin Fold
 - Linear Fold
 - Downsample
 
 
 And 6 knobs:
 - Input knob
 - Drive knob
 - Mix knob
 - Output knob
 - Cutoff knob
 - Resonance knob
 
 
 And 6 options for the filter
 - Off
 - Pre
 - Post
 - Low-pass
 - Band-pass
 - High-pass
 
 
Hope you like it!

# Release Notes:

2019-12-30 (version 0.255)
1. add one more screen to display distortion modes.
2. add global color (not in the user interface)
3. add more distortion modes.

2020-1-1 (version 0.26)
1. remove drive knob, use input volume knob as the new drive knob, which ranges in (-36, 36) db.
2. Fix some display problems. Now you can see the change of graph when you modify drive knob and mix knob.
3. Make the windows resizable.

2020-2-12 (version 0.275)
1. add input knob, change the location of mix and output knob. Now the output knob is before the mix knob so that it doesn't affect mix knob when mix = 0.
2. fit JUCE v5.7.4
3. seperate distortion module into Distortion.cpp and Distortion.h
4. Change "mode" into interger

2020-2-14 (version 0.28)
1. Finally, I use smoothedValue to eliminate zipper noise when twist knobs quickly.

2020-2-20 (version 0.3)
1. add lin fold and downsample mode
2. redesign the GUI

2020-2-20 (version 0.32)
1. add 3 filter modes (low-pass, band-pass, high-pass)
2. add 3 filter states (off, pre, post)
