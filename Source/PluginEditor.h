/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"


class OtherLookAndFeel : public LookAndFeel_V4
{
    public:
    OtherLookAndFeel()
    {
        setColour(Slider::textBoxTextColourId, Colours::red);
        setColour(Slider::textBoxBackgroundColourId, Colours::black);
        setColour(Slider::textBoxOutlineColourId, Colours::red);
    }
    void drawRotarySlider (Graphics &g, int x, int y, int width, int height, float sliderPos, float rotaryStartAngle, float rotaryEndAngle, Slider &slider) override
    {
        float diameter = jmin(width, height)*0.8;
        float radius = diameter / 2;
        float centerX = x+width / 2;
        float centerY = y+height / 2;
        float rx = centerX-radius;
        float ry = centerY-radius;
        float angle = rotaryStartAngle+(sliderPos*(rotaryEndAngle-rotaryStartAngle));
        std::cout<<rotaryStartAngle<<" "<<rotaryEndAngle<<std::endl;
        Rectangle<float> dialArea (rx,ry,diameter,diameter);
        g.setColour(Colours::black);
        //g.drawRect(dialArea);
        g.fillEllipse(dialArea);
        
        g.setColour(Colours::red);
        Path dialTick;
        dialTick.addRectangle(0, -radius, 5.0f, radius*0.3);
        g.fillPath(dialTick, AffineTransform::rotation(angle).translated(centerX, centerY));
        g.setColour(Colours::red);
        g.drawEllipse(rx, ry, diameter, diameter, 2.0f);
    }
    
};



//==============================================================================
/**
*/
class BloodAudioProcessorEditor  : public AudioProcessorEditor //edited 12/28
{
public:
    //typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    BloodAudioProcessorEditor (BloodAudioProcessor&);
    ~BloodAudioProcessorEditor();

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;
    //edited 12/28
    // void sliderValueChanged (Slider *slider) override; //edited 12/28
    
    
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    BloodAudioProcessor& processor;
    
    //create knobs and their labels
    Slider driveKnob;
    Slider rangeKnob;
    Slider mixKnob;
    Slider gainKnob;
    Label driveLabel;
    Label rangeLabel;
    Label mixLabel;
    Label gainLabel;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> rangeAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    OtherLookAndFeel otherLookAndFeel; // create own knob style
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BloodAudioProcessorEditor)
};


