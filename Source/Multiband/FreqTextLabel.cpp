/*
  ==============================================================================

    FreqTextLabel.cpp
    Created: 2 Dec 2020 7:53:08pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FreqTextLabel.h"
#include "../LookAndFeel.h"

//==============================================================================
FreqTextLabel::FreqTextLabel(VerticalLine &v) : verticalLine(v)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
    mFrequency = -1;
    mState = false;
}

FreqTextLabel::~FreqTextLabel()
{
}

void FreqTextLabel::paint (juce::Graphics& g)
{
    /* This demo code just fills the component's background and
       draws some placeholder text to get you started.

       You should replace everything in this method with your own
       drawing code..
    */
//    if (verticalLine.isMoving() || verticalLine.isMouseOver())
//    {
//        setVisible(true);
        g.setColour (COLOUR1.withAlpha(0.2f));
        g.fillAll();
        g.setColour (COLOUR1);
        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
        
    //    g.drawRoundedRectangle(<#Rectangle<float> rectangle#>, <#float cornerSize#>, <#float lineThickness#>)
        g.setFont (14.0f);
        g.drawText (static_cast<juce::String>(mFrequency) + " Hz", getLocalBounds(),
                    juce::Justification::centred, true);   // draw some placeholder text
//    }
//    else
//    {
//        setVisible(false);
//    }
}

void FreqTextLabel::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}

void FreqTextLabel::setFreq(int freq)
{
    mFrequency = freq;
}
