/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Distortion.h"

#define COLOUR0 Colour(244, 244, 210)
#define COLOUR1 Colour(244, 208, 63)
#define COLOUR2 Colour(243, 156, 18)
#define COLOUR3 Colour(230, 126, 34)
#define COLOUR4 Colour(211, 84, 0)
#define COLOUR5 Colour(192, 57, 43)

#define KNOB_FONT "Futura"

class OtherLookAndFeel : public LookAndFeel_V4
{
public:

    OtherLookAndFeel()
    {
        setColour(Slider::textBoxTextColourId, COLOUR5);
        setColour(Slider::textBoxBackgroundColourId, COLOUR1.withAlpha(0.0f));
        setColour(Slider::textBoxOutlineColourId, COLOUR1.withAlpha(0.0f));
    }

    void drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, Slider& slider) override
    {
        auto outline = COLOUR2;
        auto fill = COLOUR5;

        auto bounds = Rectangle<int>(x, y, width, height).toFloat().reduced(10);

        auto radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto lineW = jmin(8.0f, radius * 0.2f);
        auto arcRadius = radius - lineW * 0.5f;

        Path backgroundArc;
        backgroundArc.addCentredArc(bounds.getCentreX(),
            bounds.getCentreY(),
            arcRadius,
            arcRadius,
            0.0f,
            rotaryStartAngle,
            rotaryEndAngle,
            true);

        g.setColour(outline);
        g.strokePath(backgroundArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));

        if (slider.isEnabled())
        {
            Path valueArc;
            valueArc.addCentredArc(bounds.getCentreX(),
                bounds.getCentreY(),
                arcRadius,
                arcRadius,
                0.0f,
                rotaryStartAngle,
                toAngle,
                true);

            g.setColour(fill);
            g.strokePath(valueArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));
        }

        //auto thumbWidth = lineW * 1.0f;
        //Point<float> thumbPoint(bounds.getCentreX() + arcRadius * std::cos(toAngle - MathConstants<float>::halfPi),
        //    bounds.getCentreY() + arcRadius * std::sin(toAngle - MathConstants<float>::halfPi));

        //g.setColour(COLOUR5);
        //g.fillEllipse(Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));

        // draw inner circle
        float diameterInner = jmin(width, height) * 0.4;
        float radiusInner = diameterInner / 2;
        float centerX = x + width / 2;
        float centerY = y + height / 2;
        float rx = centerX - radiusInner;
        float ry = centerY - radiusInner;
        float angle = rotaryStartAngle + (sliderPos * (rotaryEndAngle - rotaryStartAngle));

        Rectangle<float> dialArea(rx, ry, diameterInner, diameterInner);
        g.setColour(COLOUR5);
        g.fillEllipse(dialArea);

        g.setColour(COLOUR2);
        Path dialTick;
        dialTick.addRectangle(0, -radiusInner, 2.0f, radiusInner * 0.3);
        g.fillPath(dialTick, AffineTransform::rotation(angle).translated(centerX, centerY));
        //g.setColour(COLOUR5);
        //g.drawEllipse(rx, ry, diameter, diameter, 1.0f);
    }

    Slider::SliderLayout getSliderLayout(Slider& slider) override
    {
        // 1. compute the actually visible textBox size from the slider textBox size and some additional constraints

        int minXSpace = 0;
        int minYSpace = 0;

        auto textBoxPos = slider.getTextBoxPosition();

        if (textBoxPos == Slider::TextBoxLeft || textBoxPos == Slider::TextBoxRight)
            minXSpace = 30;
        else
            minYSpace = 15;

        auto localBounds = slider.getLocalBounds();

        auto textBoxWidth = jmax(0, jmin(slider.getTextBoxWidth(), localBounds.getWidth() - minXSpace));
        auto textBoxHeight = jmax(0, jmin(25, localBounds.getHeight() - minYSpace));

        Slider::SliderLayout layout;

        // 2. set the textBox bounds

        if (textBoxPos != Slider::NoTextBox)
        {
            if (slider.isBar())
            {
                layout.textBoxBounds = localBounds;
            }
            else
            {
                layout.textBoxBounds.setWidth(textBoxWidth);
                layout.textBoxBounds.setHeight(textBoxHeight);

                if (textBoxPos == Slider::TextBoxLeft)           layout.textBoxBounds.setX(0);
                else if (textBoxPos == Slider::TextBoxRight)     layout.textBoxBounds.setX(localBounds.getWidth() - textBoxWidth);
                else /* above or below -> centre horizontally */ layout.textBoxBounds.setX((localBounds.getWidth() - textBoxWidth) / 2);

                if (textBoxPos == Slider::TextBoxAbove)          layout.textBoxBounds.setY(0);
                else if (textBoxPos == Slider::TextBoxBelow)     layout.textBoxBounds.setY(localBounds.getHeight() - textBoxHeight - 10); // changed here!
                else /* left or right -> centre vertically */    layout.textBoxBounds.setY((localBounds.getHeight() - textBoxHeight) / 2);
            }
        }

        // 3. set the slider bounds

        layout.sliderBounds = localBounds;

        if (slider.isBar())
        {
            layout.sliderBounds.reduce(1, 1);   // bar border
        }
        else
        {
            if (textBoxPos == Slider::TextBoxLeft)       layout.sliderBounds.removeFromLeft(textBoxWidth);
            else if (textBoxPos == Slider::TextBoxRight) layout.sliderBounds.removeFromRight(textBoxWidth);
            else if (textBoxPos == Slider::TextBoxAbove) layout.sliderBounds.removeFromTop(textBoxHeight);
            else if (textBoxPos == Slider::TextBoxBelow) layout.sliderBounds.removeFromBottom(textBoxHeight);

            const int thumbIndent = getSliderThumbRadius(slider);

            if (slider.isHorizontal())    layout.sliderBounds.reduce(thumbIndent, 0);
            else if (slider.isVertical()) layout.sliderBounds.reduce(0, thumbIndent);
        }

        return layout;

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
        
        g.setColour(COLOUR2);
        g.fillEllipse(tickBounds);

        if (ticked)
        {
            g.setColour(component.findColour(ToggleButton::tickColourId));
        }
        else
        {
            g.setColour(COLOUR2);
        }
        Rectangle<float> tickInnerBounds(x + 3, y + 3, w - 6, h - 6);
        g.fillEllipse(tickInnerBounds);
    }

    void drawPopupMenuItem (Graphics& g, const Rectangle<int>& area,
                                            const bool isSeparator, const bool isActive,
                                            const bool isHighlighted, const bool isTicked,
                                            const bool hasSubMenu, const String& text,
                                            const String& shortcutKeyText,
                                            const Drawable* icon, const Colour* const textColourToUse) override
    {
        if (isSeparator)
        {
            auto r  = area.reduced (5, 0);
            r.removeFromTop (roundToInt (((float) r.getHeight() * 0.5f) - 0.5f));
    
            g.setColour (findColour (PopupMenu::textColourId).withAlpha (0.3f));
            g.fillRect (r.removeFromTop (1));
        }
        else
        {
            auto textColour = (textColourToUse == nullptr ? findColour (PopupMenu::textColourId)
                                                          : *textColourToUse);
    
            auto r  = area.reduced (1);
    
            if (isHighlighted && isActive)
            {
                g.setColour (findColour (PopupMenu::highlightedBackgroundColourId));
                g.fillRect (r);
    
                g.setColour (findColour (PopupMenu::highlightedTextColourId));
            }
            else
            {
                g.setColour (textColour.withMultipliedAlpha (isActive ? 1.0f : 0.5f));
            }
    
            r.reduce (jmin (5, area.getWidth() / 20), 0);
    
            auto font = getPopupMenuFont();
    
            auto maxFontHeight = (float) r.getHeight() / 1.3f;
    
            if (font.getHeight() > maxFontHeight)
                font.setHeight (maxFontHeight);
    
            g.setFont (font);
    
            auto iconArea = r.removeFromLeft (roundToInt (maxFontHeight)).toFloat();
    
            if (icon != nullptr)
            {
                icon->drawWithin (g, iconArea, RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize, 1.0f);
                r.removeFromLeft (roundToInt (maxFontHeight * 0.5f));
            }
            else if (isTicked)
            {
                //auto tick = getTickShape (1.0f);
                //g.fillPath (tick, tick.getTransformToScaleToFit (iconArea.reduced (iconArea.getWidth() / 5, 0).toFloat(), true));
                g.fillEllipse(iconArea.getX() + 15, iconArea.getY() + 20, iconArea.getWidth() - 30, iconArea.getWidth() - 30);
            }
    
            if (hasSubMenu)
            {
                auto arrowH = 0.6f * getPopupMenuFont().getAscent();
    
                auto x = static_cast<float> (r.removeFromRight ((int) arrowH).getX());
                auto halfH = static_cast<float> (r.getCentreY());
    
                Path path;
                path.startNewSubPath (x, halfH - arrowH * 0.5f);
                path.lineTo (x + arrowH * 0.6f, halfH);
                path.lineTo (x, halfH + arrowH * 0.5f);
    
                g.strokePath (path, PathStrokeType (2.0f));
            }
    
            r.removeFromRight (3);
            g.drawFittedText (text, r, Justification::centredLeft, 1);
    
            if (shortcutKeyText.isNotEmpty())
            {
                auto f2 = font;
                f2.setHeight (f2.getHeight() * 0.75f);
                f2.setHorizontalScale (0.95f);
                g.setFont (f2);
    
                g.drawText (shortcutKeyText, r, Justification::centredRight, true);
            }
        }
    }

    void drawButtonBackground(Graphics& g,
        Button& button,
        const Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 0.0f;
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);

        auto baseColour = COLOUR5.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 0.9f)
            .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);

        g.setColour(baseColour);

        auto flatOnLeft = button.isConnectedOnLeft();
        auto flatOnRight = button.isConnectedOnRight();
        auto flatOnTop = button.isConnectedOnTop();
        auto flatOnBottom = button.isConnectedOnBottom();

       if (flatOnLeft || flatOnRight || flatOnTop || flatOnBottom)
       {
            Path path;
            path.addRoundedRectangle(bounds.getX(), bounds.getY(),
                bounds.getWidth(), bounds.getHeight(),
                cornerSize, cornerSize,
                !(flatOnLeft || flatOnTop),
                !(flatOnRight || flatOnTop),
                !(flatOnLeft || flatOnBottom),
                !(flatOnRight || flatOnBottom));

            g.fillPath(path);

            g.setColour(COLOUR5);
            g.strokePath(path, PathStrokeType(1.0f));
        }
        else
        {
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(COLOUR5);
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
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
class FireAudioProcessorEditor : public AudioProcessorEditor,//edited 12/28
                                  public Slider::Listener,
                                  public Timer //edited 2020/07/03 fps
{
public:
    //typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    FireAudioProcessorEditor(FireAudioProcessor &);
    ~FireAudioProcessorEditor();

    //==============================================================================
    void paint(Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    // linked
    void sliderValueChanged (Slider* slider) override
    {
        float thresh = 20.f;
        float changeThresh = 3.f;
        if (linkedButton.getToggleState() == true)
        {
            if (slider == &driveKnob)
            {
                if (driveKnob.getValue()>2)
                    outputKnob.setValue(-changeThresh - (driveKnob.getValue()-1)/31*(thresh-changeThresh));
                else
                    outputKnob.setValue((driveKnob.getValue()-1)*(-3));
            }
            else if (slider == &outputKnob)
            {
                if (outputKnob.getValue() < -changeThresh && outputKnob.getValue() > -thresh)
                    driveKnob.setValue(1-(outputKnob.getValue()+changeThresh)*31/(thresh-changeThresh));
                else if (outputKnob.getValue() >=-changeThresh && outputKnob.getValue() <0)
                    driveKnob.setValue(1 + outputKnob.getValue()/(-changeThresh));
                else if (outputKnob.getValue() >= 0)
                    driveKnob.setValue(1);
                else if (outputKnob.getValue() <= -10)
                    driveKnob.setValue(32);
            }
        }
    }
    
    //    Visualiser visualiser;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor &processor;

    // ff meter
    foleys::LevelMeterLookAndFeel lnf;
    foleys::LevelMeter inputMeter{ foleys::LevelMeter::Minimal };
    foleys::LevelMeter outputMeter{ foleys::LevelMeter::Minimal };

    // sliders
    Slider
        inputKnob,
        driveKnob,
        downSampleKnob,
        outputKnob,
        recKnob,
        mixKnob,
        cutoffKnob,
        resKnob;
    int knobSize = 100;

    // labels
    Label
        hqLabel,
        inputLabel,
        driveLabel,
        downSampleLabel,
        outputLabel,
        recLabel,
        mixLabel,
        cutoffLabel,
        resLabel,
        linkedLabel,
        recOffLabel,
        recHalfLabel,
        recFullLabel,
        filterOffLabel,
        filterLowLabel;


    // toggle buttons
    ToggleButton
        hqButton{ "HQ" }, // high quality (oversampling)
        linkedButton{ "Link" },
        //recOffButton{ "Off" },
        //recHalfButton{ "Half" },
        //recFullButton{ "Full" },
        filterOffButton{ "Off" },
        filterPreButton{ "Pre" },
        filterPostButton{ "Post" },
        filterLowButton{ "Low Pass" },
        filterBandButton{ "Band Pass" },
        filterHighButton{ "High Pass" };
    
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
        //recStateButtons = 1003
    };
    
    void updateToggleState (Button* button, String name);
    
    // Slider attachment
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>
        inputAttachment,
        driveAttachment,
        downSampleAttachment,
        recAttachment,
        outputAttachment,
        mixAttachment,
        cutoffAttachment,
        resAttachment;
    
    // Toggle Button attachment
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment,
        linkedAttachment,
        //recOffAttachment,
        //recHalfAttachment,
        //recFullAttachment,
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
    //float getFunctionValue(FireAudioProcessor& processor, float value);
    //int modeChoice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
