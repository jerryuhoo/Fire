/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"



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
        g.setColour(Colour (50, 0, 0));
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

//class Visualiser : public AudioVisualiserComponent
//{
//public:
//    Visualiser() : AudioVisualiserComponent (2)
//    {
//        setBufferSize(512);
//        setSamplesPerBlock(256);
//        setColours(Colour (50, 0, 0), Colours::red);
//    }
//};

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
    
    // combobox

    
    
//    Visualiser visualiser;
    
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    BloodAudioProcessor& processor;
    
    // ff meter
    std::unique_ptr<FFAU::LevelMeter> inputMeter;
    std::unique_ptr<FFAU::LevelMeter> outputMeter;
    std::unique_ptr<FFAU::LevelMeterLookAndFeel> lnf;
    
    // create knobs and their labels
    Slider driveKnob;
    Slider inputKnob;
    Slider mixKnob;
    Slider gainKnob;
    Label driveLabel;
    Label inputLabel;
    Label mixLabel;
    Label gainLabel;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    
    // mode menu
    ComboBox distortionMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeSelection;
    // create own knob style
    OtherLookAndFeel otherLookAndFeel;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BloodAudioProcessorEditor)
};


