# Blood
![Alt text](Blood.png?raw=true "Title")

 This is a distortion plugin.
 This plugin has 6 different distortion modes:
 - Arctan Soft Clipping
 - Exp Soft Clipping
 - Tanh Soft Clipping
 - Cubic Soft Clipping
 - Hard Clipping
 - Square Wave Clipping
 And four knobs:
 - Input knob
 - Drive knob
 - Mix knob
 - Output knob
 
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
