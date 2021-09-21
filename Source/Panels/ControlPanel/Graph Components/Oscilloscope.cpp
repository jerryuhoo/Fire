/*
  ==============================================================================

    Oscilloscope.cpp
    Created: 25 Oct 2020 7:26:35pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Oscilloscope.h"


//==============================================================================
Oscilloscope::Oscilloscope(FireAudioProcessor &p) : processor(p)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
}

Oscilloscope::~Oscilloscope()
{
}

void Oscilloscope::paint (juce::Graphics& g)
{
    // draw outline
    g.setColour (COLOUR6);
    g.drawRect (getLocalBounds(), 1);
    
    // fill rounded rectangle
//    float cornerSize = 10 * scale;
//    g.setColour(COLOUR6);
//    juce::Rectangle<float> rectFloat(0, 0, getWidth(), getHeight());
//    g.fillRoundedRectangle(rectFloat, cornerSize);
    
    // change color according to the drive
//    float drive = processor.getNewDrive(1);
//    DBG(drive);
//    int colour_r = 244;
//    int colour_g = (208 - drive * 2 < 0) ? 0 : (208 - drive * 2);
//    int colour_b = 63;
    
//    juce::ColourGradient grad(juce::Colour(colour_r, colour_g, colour_b), getWidth() / 2, getHeight() / 2,
//                              juce::Colour(colour_r, colour_g, colour_b).withAlpha(0.f), 0, getHeight() / 2, true);
//    g.setGradientFill(grad);
    
    juce::ColourGradient grad(juce::Colours::red.withBrightness(0.9f), getWidth() / 2, getHeight() / 2,
                              juce::Colours::red.withAlpha(0.0f), 0, getHeight() / 2, true);
    g.setGradientFill(grad);
    
    
    historyL = processor.getHistoryArrayL();
    historyR = processor.getHistoryArrayR();
    
    juce::Path pathL;
    juce::Path pathR;
    
    float amp = getHeight() / 4;
    
    // get max
    float maxValue = 0.0f;
    
    for (int i = 0; i < historyL.size(); i++)
    {
        if (historyL[i] > maxValue || historyR[i] > maxValue)
        {
            maxValue = historyL[i] > historyR[i] ? historyL[i] : historyR[i];
        }
    }
    
    //TODO: this may cause high CPU usage! maybe use i += 2?
    for (int i = 0; i < getWidth(); i++) { 
        int scaledIndex = static_cast<int>(i * (float)historyL.size() / (float)getWidth());

        float valL = historyL[scaledIndex];
        float valR = historyR[scaledIndex];
        
        // normalize
        if (maxValue > 0.00001f)
        {
            valL = valL / maxValue * 0.6f;
            valR = valR / maxValue * 0.6f;
        }
        
        valL = juce::jlimit<float>(-1, 1, valL);
        valR = juce::jlimit<float>(-1, 1, valR);

        if (i == 0)
        {
            pathL.startNewSubPath(0, amp);
            pathR.startNewSubPath(0, amp * 3);
        }
        else
        {
            pathL.lineTo(i, amp - valL * amp);
            pathR.lineTo(i, amp * 3 - valR * amp);
        }
    }

    g.strokePath(pathL, juce::PathStrokeType(2.0));
    g.strokePath(pathR, juce::PathStrokeType(2.0));
    
    // shadow
//    juce::Path pathShadow;
//    pathShadow.addRoundedRectangle(rectFloat.reduced (0.5f, 0.5f), cornerSize);
//    drawInnerShadow(g, pathShadow);
}

void Oscilloscope::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}

void Oscilloscope::setScale(float scale)
{
    this->scale = scale;
}

bool Oscilloscope::getZoomState()
{
    return mZoomState;
}

void Oscilloscope::setZoomState(bool zoomState)
{
    mZoomState = zoomState;
}

void Oscilloscope::mouseDown(const juce::MouseEvent &e)
{
    if (mZoomState)
    {
        mZoomState = false;
    }
    else
    {
        mZoomState = true;
    }
}
