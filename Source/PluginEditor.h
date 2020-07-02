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
    Colour secondColour = Colour(77, 4, 4);

    OtherLookAndFeel()
    {
        setColour(Slider::textBoxTextColourId, mainColour);
        setColour(Slider::textBoxBackgroundColourId, backgroundColour);
        setColour(Slider::textBoxOutlineColourId, mainColour);
    }

    void drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos, float rotaryStartAngle, float rotaryEndAngle, Slider& slider) override
    {
        float diameter = jmin(width, height) * 0.8;
        float radius = diameter / 2;
        float centerX = x + width / 2;
        float centerY = y + height / 2;
        float rx = centerX - radius;
        float ry = centerY - radius;
        float angle = rotaryStartAngle + (sliderPos * (rotaryEndAngle - rotaryStartAngle));

        Rectangle<float> dialArea(rx, ry, diameter, diameter);
        g.setColour(secondColour);
        g.fillEllipse(dialArea);

        g.setColour(mainColour);
        Path dialTick;
        dialTick.addRectangle(0, -radius, 5.0f, radius * 0.3);
        g.fillPath(dialTick, AffineTransform::rotation(angle).translated(centerX, centerY));
        g.setColour(mainColour);
        g.drawEllipse(rx, ry, diameter, diameter, 2.0f);
    }
};

class OtherLookAndFeelRed : public LookAndFeel_V4
{
public:
    Colour mainColour = Colour(255, 0, 0);
    Colour secondColour = Colours::darkred;
    Colour backgroundColour = Colour(77, 4, 4);
    

    OtherLookAndFeelRed()
    {
        setColour(Slider::textBoxTextColourId, mainColour);
        setColour(Slider::textBoxBackgroundColourId, backgroundColour);
        setColour(Slider::textBoxOutlineColourId, backgroundColour); // old is secondColour
        setColour(Slider::thumbColourId, Colours::red);
        setColour(Slider::rotarySliderFillColourId, mainColour);
        setColour(Slider::rotarySliderOutlineColourId, secondColour);
    }

    void drawTickBox(Graphics& g, Component& component,
        float x, float y, float w, float h,
        const bool ticked,
        const bool isEnabled,
        const bool shouldDrawButtonAsHighlighted,
        const bool shouldDrawButtonAsDown) override
    {
        ignoreUnused(isEnabled, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        Rectangle<float> tickBounds(x, y, w, h);

        g.setColour(component.findColour(ToggleButton::tickDisabledColourId));
        g.drawRect(tickBounds, 1.0f);
        
        if (ticked)
        {
            g.setColour(component.findColour(ToggleButton::tickColourId));
            // auto tick = getTickShape(0.75f);
            // g.fillPath(tick, tick.getTransformToScaleToFit(tickBounds.reduced(4, 5).toFloat(), false));
            Rectangle<float> tickInnerBounds(x + 1, y + 1, w - 2, h - 2);
            g.fillRect(tickInnerBounds);
        }
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
class BloodAudioProcessorEditor : public AudioProcessorEditor,//edited 12/28
                                  public Slider::Listener
{
public:
    //typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    BloodAudioProcessorEditor(BloodAudioProcessor &);
    ~BloodAudioProcessorEditor();

    //==============================================================================
    void paint(Graphics &) override;
    void resized() override;
    // color setting
    Colour mainColour = Colour(255, 0, 0);
    Colour backgroundColour = Colour::fromFloatRGBA (0.0f, 0.0f, 0.0f, 0.0f); //Colours::black;
    Colour secondColour = Colour(77, 4, 4);
    
    // linked
    void sliderValueChanged (Slider* slider) override
    {
        if (linkedButton.getToggleState() == true)
        {
            if (slider == &driveKnob)
            {
                if (driveKnob.getValue()>2)
                    outputKnob.setValue(-3 - (driveKnob.getValue()-1)/63*7);
                else
                    outputKnob.setValue((driveKnob.getValue()-1)*(-3));
            }
            else if (slider == &outputKnob)
            {
                if (outputKnob.getValue() < -3 && outputKnob.getValue() > -10)
                    driveKnob.setValue(1-(outputKnob.getValue()+3)*63/7);
                else if (outputKnob.getValue() >=-3 && outputKnob.getValue() <0)
                    driveKnob.setValue(1 + outputKnob.getValue()/(-3));
                else if (outputKnob.getValue() >= 0)
                    driveKnob.setValue(1);
                else if (outputKnob.getValue() <= -10)
                    driveKnob.setValue(64);
            }
        }
    }
    
    //    Visualiser visualiser;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    BloodAudioProcessor &processor;

    // ff meter
    foleys::LevelMeterLookAndFeel lnf;
    foleys::LevelMeter inputMeter{ foleys::LevelMeter::Minimal };
    foleys::LevelMeter outputMeter{ foleys::LevelMeter::Minimal };

    // sliders
    Slider
        driveKnob,
        inputKnob,
        outputKnob,
        mixKnob,
        cutoffKnob,
        resKnob;
    
    // labels
    Label
        driveLabel,
        inputLabel,
        outputLabel,
        mixLabel,
        cutoffLabel,
        resLabel,
        debugLabel;
    
    // toggle buttons
    ToggleButton
        hqButton{ "" }, // high quality (oversampling)
        linkedButton {""}, //{"Linked"},
        recOffButton{ "" }, 
        recHalfButton{ "" }, 
        recFullButton{ "" }, 
        filterOffButton {""}, //{"Off"},
        filterPreButton {""}, //{"Pre"},
        filterPostButton {""}, //{"Post"},
        filterLowButton {""}, //{"Low Pass"},
        filterBandButton {""}, //{"Band Pass"},
        filterHighButton {""}; //{"High Pass"};
    
    // about
    TextButton aboutButton {"about"};
//    AlertWindow aboutDialog;
    // group toggle buttons
    enum RadioButtonIds
    {
        // filter state: off, pre, post
        filterStateButtons = 1001,
        // filter mode: low, band, high
        filterModeButtons = 1002,
        // rec state: off, half, full
        recStateButtons = 1003
    };
    
    void updateToggleState (Button* button, String name);
    
    // Slider attachment
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>
        driveAttachment,
        inputAttachment,
        outputAttachment,
        mixAttachment,
        cutoffAttachment,
        resAttachment;
    
    // Toggle Button attachment
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment,
        linkedAttachment,
        recOffAttachment,
        recHalfAttachment,
        recFullAttachment,
        filterOffAttachment,
        filterPreAttachment,
        filterPostAttachment,
        filterLowAttachment,
        filterBandAttachment,
        filterHighAttachment;
    
    // ComboBox attachment
    ComboBox distortionMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    
    //ComboBoxParameterAttachment
    
    // create own knob style
    OtherLookAndFeel otherLookAndFeel;
    OtherLookAndFeelRed otherLookAndFeelRed;

    Distortion distortionProcessor;

    //return function value by different modes
    //float getFunctionValue(BloodAudioProcessor& processor, float value);
    //int modeChoice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BloodAudioProcessorEditor)
};
