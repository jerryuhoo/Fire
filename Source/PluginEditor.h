/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Distortion.h"

class OtherLookAndFeel : public LookAndFeel_V4
{
public:
    Colour mainColour = Colours::red;
    Colour backgroundColour = Colours::black;
    Colour secondColour = Colour(50, 0, 0);

    OtherLookAndFeel()
    {
        setColour(Slider::textBoxTextColourId, mainColour);
        setColour(Slider::textBoxBackgroundColourId, backgroundColour);
        setColour(Slider::textBoxOutlineColourId, mainColour);
    }

    void drawRotarySlider(Graphics &g, int x, int y, int width, int height, float sliderPos, float rotaryStartAngle, float rotaryEndAngle, Slider &slider) override
    {
        float diameter = jmin(width, height) * 0.8;
        float radius = diameter / 2;
        float centerX = x + width / 2;
        float centerY = y + height / 2;
        float rx = centerX - radius;
        float ry = centerY - radius;
        float angle = rotaryStartAngle + (sliderPos * (rotaryEndAngle - rotaryStartAngle));
        std::cout << rotaryStartAngle << " " << rotaryEndAngle << std::endl;
        Rectangle<float> dialArea(rx, ry, diameter, diameter);
        g.setColour(secondColour);
        //g.drawRect(dialArea);
        g.fillEllipse(dialArea);

        g.setColour(mainColour);
        Path dialTick;
        dialTick.addRectangle(0, -radius, 5.0f, radius * 0.3);
        g.fillPath(dialTick, AffineTransform::rotation(angle).translated(centerX, centerY));
        g.setColour(mainColour);
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
class BloodAudioProcessorEditor : public AudioProcessorEditor //edited 12/28
{
public:
    //typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    BloodAudioProcessorEditor(BloodAudioProcessor &);
    ~BloodAudioProcessorEditor();

    //==============================================================================
    void paint(Graphics &) override;
    void resized() override;
    // color setting
    Colour mainColour = Colours::red;
    Colour backgroundColour = Colour::fromFloatRGBA (0.0f, 0.0f, 0.0f, 0.0f); //Colours::black;
    Colour secondColour = Colour(50, 0, 0);

    //    Visualiser visualiser;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    BloodAudioProcessor &processor;

    // ff meter
    std::unique_ptr<FFAU::LevelMeter> inputMeter;
    std::unique_ptr<FFAU::LevelMeter> outputMeter;
    std::unique_ptr<FFAU::LevelMeterLookAndFeel> lnf;

    // create knobs and their labels
    Slider driveKnob; // (deleted drive)
    Slider inputKnob;
    Slider outputKnob;
    Slider mixKnob;
    
    Label driveLabel; // (deleted drive)
    Label inputLabel;
    Label outputLabel;
    Label mixLabel;
    
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    // mode menu
    ComboBox distortionMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeSelection;
    // create own knob style
    OtherLookAndFeel otherLookAndFeel;

    Distortion distortionProcessor;

    //return function value by different modes
    //float getFunctionValue(BloodAudioProcessor& processor, float value);
    //int modeChoice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BloodAudioProcessorEditor)
};
