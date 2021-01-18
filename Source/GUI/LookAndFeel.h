/*
  ==============================================================================

    LookAndFeel.h
    Created: 12 Jul 2020 8:54:38am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/
#pragma once

#include "InterfaceDefines.h"

class OtherLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // resize scale
    float scale = 1.f;

    OtherLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, KNOB_FONT_COLOUR);
        setColour(juce::Slider::textBoxBackgroundColourId, COLOUR1.withAlpha(0.0f));
        setColour(juce::Slider::textBoxOutlineColourId, COLOUR1.withAlpha(0.0f));
        setColour(juce::Slider::trackColourId, COLOUR1);
        setColour(juce::Slider::thumbColourId, COLOUR5);
        setColour(juce::Slider::backgroundColourId, COLOUR6);
    }

    // customize knobs
    void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
                          const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider &slider) override
    {
        // draw outline
        auto outline = COLOUR6;
        auto fill = COLOUR1;

        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10);

        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        // auto lineW = jmin(8.0f, radius * 0.2f);
        auto lineW = radius * 0.6f;
        auto arcRadius = radius - lineW * 0.5f;

        juce::Path backgroundArc;
        backgroundArc.addCentredArc(bounds.getCentreX(),
                                    bounds.getCentreY(),
                                    arcRadius,
                                    arcRadius,
                                    0.0f,
                                    rotaryStartAngle,
                                    rotaryEndAngle,
                                    true);

        g.setColour(outline);
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc(bounds.getCentreX(),
                                   bounds.getCentreY(),
                                   arcRadius,
                                   arcRadius,
                                   0.0f,
                                   rotaryStartAngle,
                                   toAngle,
                                   true);

            g.setColour(fill);
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        //auto thumbWidth = lineW * 1.0f;
        //Point<float> thumbPoint(bounds.getCentreX() + arcRadius * std::cos(toAngle - MathConstants<float>::halfPi),
        //    bounds.getCentreY() + arcRadius * std::sin(toAngle - MathConstants<float>::halfPi));

        //g.setColour(COLOUR5);
        //g.fillEllipse(Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));

        // draw inner circle
        float diameterInner = juce::jmin(width, height) * 0.4;
        float radiusInner = diameterInner / 2;
        float centerX = x + width / 2;
        float centerY = y + height / 2;
        float rx = centerX - radiusInner;
        float ry = centerY - radiusInner;
        float angle = rotaryStartAngle + (sliderPos * (rotaryEndAngle - rotaryStartAngle));

        juce::Rectangle<float> dialArea(rx, ry, diameterInner, diameterInner);

        g.setColour(COLOUR5.withBrightness(slider.isEnabled() ? 1.0f : 0.5f));
        g.fillEllipse(dialArea);

        // draw tick
        g.setColour(COLOUR1.withBrightness(slider.isEnabled() ? 1.0f : 0.5f));
        juce::Path dialTick;
        dialTick.addRectangle(0, -radiusInner, radiusInner * 0.1f, radiusInner * 0.3);
        g.fillPath(dialTick, juce::AffineTransform::rotation(angle).translated(centerX, centerY));
        //g.setColour(COLOUR5);
        //g.drawEllipse(rx, ry, diameter, diameter, 1.0f);
    }

    // resize slider and textbox size
    juce::Slider::SliderLayout getSliderLayout(juce::Slider &slider) override
    {
        // 1. compute the actually visible textBox size from the slider textBox size and some additional constraints

        int minXSpace = 0;
        int minYSpace = 0;

        auto textBoxPos = slider.getTextBoxPosition();

        if (textBoxPos == juce::Slider::TextBoxLeft || textBoxPos == juce::Slider::TextBoxRight)
            minXSpace = 30;
        else
            minYSpace = 15;

        auto localBounds = slider.getLocalBounds();

        auto textBoxWidth = juce::jmax(0, juce::jmin(static_cast<int>(slider.getTextBoxWidth() * scale), localBounds.getWidth() - minXSpace));
        auto textBoxHeight = juce::jmax(0, juce::jmin(static_cast<int>(slider.getTextBoxHeight() * scale), localBounds.getHeight() - minYSpace));

        juce::Slider::SliderLayout layout;

        // 2. set the textBox bounds

        if (textBoxPos != juce::Slider::NoTextBox)
        {
            if (slider.isBar())
            {
                layout.textBoxBounds = localBounds;
            }
            else
            {
                layout.textBoxBounds.setWidth(textBoxWidth);
                layout.textBoxBounds.setHeight(textBoxHeight);

                if (textBoxPos == juce::Slider::TextBoxLeft)
                    layout.textBoxBounds.setX(0);
                else if (textBoxPos == juce::Slider::TextBoxRight)
                    layout.textBoxBounds.setX(localBounds.getWidth() - textBoxWidth);
                else /* above or below -> centre horizontally */
                    layout.textBoxBounds.setX((localBounds.getWidth() - textBoxWidth) / 2);

                if (textBoxPos == juce::Slider::TextBoxAbove)
                    layout.textBoxBounds.setY(0);
                else if (textBoxPos == juce::Slider::TextBoxBelow)
                    layout.textBoxBounds.setY(localBounds.getHeight() - textBoxHeight); // changed here!
                else                                                                         /* left or right -> centre vertically */
                    layout.textBoxBounds.setY((localBounds.getHeight() - textBoxHeight) / 2);
            }
        }

        // 3. set the slider bounds

        layout.sliderBounds = localBounds;

        if (slider.isBar())
        {
            layout.sliderBounds.reduce(1, 1); // bar border
        }
        else
        {
            if (textBoxPos == juce::Slider::TextBoxLeft)
                layout.sliderBounds.removeFromLeft(textBoxWidth);
            else if (textBoxPos == juce::Slider::TextBoxRight)
                layout.sliderBounds.removeFromRight(textBoxWidth);
            else if (textBoxPos == juce::Slider::TextBoxAbove)
                layout.sliderBounds.removeFromTop(textBoxHeight);
            else if (textBoxPos == juce::Slider::TextBoxBelow)
                layout.sliderBounds.removeFromBottom(textBoxHeight);

            const int thumbIndent = getSliderThumbRadius(slider);

            if (slider.isHorizontal())
                layout.sliderBounds.reduce(thumbIndent, 0);
            else if (slider.isVertical())
                layout.sliderBounds.reduce(0, thumbIndent);
        }

        return layout;
    }

    // draw rounded tickbox (toggle button)
    void drawTickBox(juce::Graphics &g, juce::Component &component,
                     float x, float y, float w, float h,
                     const bool ticked,
                     const bool isEnabled,
                     const bool shouldDrawButtonAsHighlighted,
                     const bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(isEnabled, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        juce::Rectangle<float> tickBounds(x, y, w, h);

        g.setColour(COLOUR2);
        g.fillEllipse(tickBounds);

        if (ticked)
        {
            g.setColour(component.findColour(juce::ToggleButton::tickColourId));
        }
        else
        {
            g.setColour(COLOUR2);
        }
        juce::Rectangle<float> tickInnerBounds(x + 3, y + 3, w - 6, h - 6);
        g.fillEllipse(tickInnerBounds);
    }
    
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(COLOUR6); // findColour (PopupMenu::backgroundColourId)
        juce::ignoreUnused(width, height);

       #if ! JUCE_MAC
        g.setColour(COLOUR1.withAlpha(0.6f)); //juce::LookAndFeel::findColour(juce::PopupMenu::textColourId)
        g.drawRect(0, 0, width, height);
       #endif
    }

    void drawPopupMenuItem(juce::Graphics &g, const juce::Rectangle<int> &area,
                           const bool isSeparator, const bool isActive,
                           const bool isHighlighted, const bool isTicked,
                           const bool hasSubMenu, const juce::String &text,
                           const juce::String &shortcutKeyText,
                           const juce::Drawable *icon, const juce::Colour *const textColourToUse) override
    {
        //g.setColour(COLOUR6); // backgroundColourId : black
        //g.fillAll();
        
        if (isSeparator)
        {
            auto r = area.reduced(5, 0);
            r.removeFromTop(juce::roundToInt(((float)r.getHeight() * 0.5f) - 0.5f));

            g.setColour(COLOUR1.withAlpha(0.3f)); //findColour(juce::PopupMenu::textColourId)
            g.fillRect(r.removeFromTop(1));
        }
        else
        {
            auto textColour = (textColourToUse == nullptr ? COLOUR1
                                                          : *textColourToUse); //findColour(juce::PopupMenu::textColourId)

            auto r = area.reduced(1);

            if (isHighlighted && isActive)
            {
                g.setColour(COLOUR5); //findColour(juce::PopupMenu::highlightedBackgroundColourId) : red
                g.fillRect(r);

                g.setColour(COLOUR1); //findColour(juce::PopupMenu::highlightedTextColourId)
            }
            else
            {
                g.setColour(textColour.withMultipliedAlpha(isActive ? 1.0f : 0.5f));
            }

            r.reduce(juce::jmin(5, area.getWidth() / 20), 0);

            auto font = getPopupMenuFont();

            auto maxFontHeight = (float)r.getHeight() / 1.3f;

            if (font.getHeight() > maxFontHeight)
                font.setHeight(maxFontHeight);

            g.setFont(font);

            auto iconArea = r.removeFromLeft(juce::roundToInt(maxFontHeight)).toFloat();

            if (icon != nullptr)
            {
                icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
                r.removeFromLeft(juce::roundToInt(maxFontHeight * 0.5f));
            }
            else if (isTicked)
            {
                //auto tick = getTickShape (1.0f);
                //g.fillPath (tick, tick.getTransformToScaleToFit (iconArea.reduced (iconArea.getWidth() / 5, 0).toFloat(), true));
                
                float radius = scale * 10;
                float xPos = iconArea.getX() + iconArea.getWidth() / 2 - radius / 2;
                float yPos = iconArea.getY() + iconArea.getHeight() / 2 - radius / 2;
                
                g.fillEllipse(xPos, yPos, radius, radius);
            }

            if (hasSubMenu)
            {
                auto arrowH = 0.6f * getPopupMenuFont().getAscent();

                auto x = static_cast<float>(r.removeFromRight((int)arrowH).getX());
                auto halfH = static_cast<float>(r.getCentreY());

                juce::Path path;
                path.startNewSubPath(x, halfH - arrowH * 0.5f);
                path.lineTo(x + arrowH * 0.6f, halfH);
                path.lineTo(x, halfH + arrowH * 0.5f);

                g.strokePath(path, juce::PathStrokeType(2.0f));
            }

            r.removeFromRight(3);
            g.drawFittedText(text, r, juce::Justification::centredLeft, 1);

            if (shortcutKeyText.isNotEmpty())
            {
                auto f2 = font;
                f2.setHeight(f2.getHeight() * 0.75f);
                f2.setHorizontalScale(0.95f);
                g.setFont(f2);

                g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
            }
        }
    }

    void drawComboBoxTextWhenNothingSelected(juce::Graphics &g, juce::ComboBox &box, juce::Label &label) override
    {
        //g.setColour (findColour (ComboBox::textColourId).withMultipliedAlpha (0.5f));
        g.setColour(COLOUR1.withMultipliedAlpha(0.5f));
        auto font = label.getLookAndFeel().getLabelFont(label);

        g.setFont(font);

        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

        g.drawFittedText(box.getTextWhenNothingSelected(), textArea, label.getJustificationType(),
                         juce::jmax(1, (int)((float)textArea.getHeight() / font.getHeight())),
                         label.getMinimumHorizontalScale());
    }

    // customize button
    void drawButtonBackground(juce::Graphics &g,
                              juce::Button &button,
                              const juce::Colour &backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 0.0f;
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);

        auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 1.0f)
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
            juce::Path path;
            path.addRoundedRectangle(bounds.getX(), bounds.getY(),
                                     bounds.getWidth(), bounds.getHeight(),
                                     cornerSize, cornerSize,
                                     !(flatOnLeft || flatOnTop),
                                     !(flatOnRight || flatOnTop),
                                     !(flatOnLeft || flatOnBottom),
                                     !(flatOnRight || flatOnBottom));

            g.fillPath(path);

            g.setColour(button.findColour(juce::ComboBox::outlineColourId));
            g.strokePath(path, juce::PathStrokeType(1.0f));
        }
        else
        {
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(button.findColour(juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
    }

    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
    }

    // combobox customize font
    juce::Font getComboBoxFont(juce::ComboBox & /*box*/) override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
    }

    // label customize font
    juce::Font getLabelFont(juce::Label &label) override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
        ;
    }

    void drawAlertBox(juce::Graphics &g, juce::AlertWindow &alert,
                      const juce::Rectangle<int> &textArea, juce::TextLayout &textLayout) override
    {
        auto cornerSize = 1.0f;

        g.setColour(alert.findColour(juce::AlertWindow::outlineColourId));
        g.drawRoundedRectangle(alert.getLocalBounds().toFloat(), cornerSize, 2.0f);

        auto bounds = alert.getLocalBounds().reduced(1);
        g.reduceClipRegion(bounds);

        g.setColour(alert.findColour(juce::AlertWindow::backgroundColourId));
        g.fillRoundedRectangle(bounds.toFloat(), cornerSize);

        auto iconSpaceUsed = 0;

        auto iconWidth = 80;
        auto iconSize = juce::jmin(iconWidth + 50, bounds.getHeight() + 20);

        if (alert.containsAnyExtraComponents() || alert.getNumButtons() > 2)
            iconSize = juce::jmin(iconSize, textArea.getHeight() + 50);

        juce::Rectangle<int> iconRect(iconSize / -10, iconSize / -10,
                                      iconSize, iconSize);

        if (alert.getAlertType() != juce::AlertWindow::NoIcon)
        {
            juce::Path icon;
            char character;
            juce::uint32 colour;

            if (alert.getAlertType() == juce::AlertWindow::WarningIcon)
            {
                character = '!';

                icon.addTriangle((float)iconRect.getX() + (float)iconRect.getWidth() * 0.5f, (float)iconRect.getY(),
                                 static_cast<float>(iconRect.getRight()), static_cast<float>(iconRect.getBottom()),
                                 static_cast<float>(iconRect.getX()), static_cast<float>(iconRect.getBottom()));

                icon = icon.createPathWithRoundedCorners(5.0f);
                colour = 0x66ff2a00;
            }
            else
            {
                colour = juce::Colour(0xff00b0b9).withAlpha(0.4f).getARGB();
                character = alert.getAlertType() == juce::AlertWindow::InfoIcon ? 'i' : '?';

                icon.addEllipse(iconRect.toFloat());
            }

            juce::GlyphArrangement ga;
            ga.addFittedText({(float)iconRect.getHeight() * 0.9f, juce::Font::bold},
                             juce::String::charToString((juce::juce_wchar)(juce::uint8)character),
                             static_cast<float>(iconRect.getX()), static_cast<float>(iconRect.getY()),
                             static_cast<float>(iconRect.getWidth()), static_cast<float>(iconRect.getHeight()),
                             juce::Justification::centred, false);
            ga.createPath(icon);

            icon.setUsingNonZeroWinding(false);
            g.setColour(juce::Colour(colour));
            g.fillPath(icon);

            iconSpaceUsed = iconWidth;
        }

        g.setColour(alert.findColour(juce::AlertWindow::textColourId));

        juce::Rectangle<int> alertBounds(bounds.getX() + iconSpaceUsed, 30,
                                         bounds.getWidth(), bounds.getHeight() - getAlertWindowButtonHeight() - 20);

        textLayout.draw(g, alertBounds.toFloat());
    }
};

//class OtherLookAndFeelRed : public LookAndFeel_V4
//{
//public:
//
//    Colour mainColour = Colour(255, 0, 0);
//    Colour secondColour = Colours::darkred;
//    Colour backgroundColour = Colour(77, 4, 4);
//
//
//    OtherLookAndFeelRed()
//    {
//        setColour(Slider::textBoxTextColourId, mainColour);
//        setColour(Slider::textBoxBackgroundColourId, backgroundColour);
//        setColour(Slider::textBoxOutlineColourId, backgroundColour); // old is secondColour
//        setColour(Slider::thumbColourId, Colours::red);
//        setColour(Slider::rotarySliderFillColourId, mainColour);
//        setColour(Slider::rotarySliderOutlineColourId, secondColour);
//    }
//
//    void drawTickBox(Graphics& g, Component& component,
//        float x, float y, float w, float h,
//        const bool ticked,
//        const bool isEnabled,
//        const bool shouldDrawButtonAsHighlighted,
//        const bool shouldDrawButtonAsDown) override
//    {
//        ignoreUnused(isEnabled, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
//
//        Rectangle<float> tickBounds(x, y, w, h);
//
//        g.setColour(component.findColour(ToggleButton::tickDisabledColourId));
//        g.drawRect(tickBounds, 1.0f);
//
//        if (ticked)
//        {
//            g.setColour(component.findColour(ToggleButton::tickColourId));
//            // auto tick = getTickShape(0.75f);
//            // g.fillPath(tick, tick.getTransformToScaleToFit(tickBounds.reduced(4, 5).toFloat(), false));
//            Rectangle<float> tickInnerBounds(x + 1, y + 1, w - 2, h - 2);
//            g.fillRect(tickInnerBounds);
//        }
//    }
//};

class RoundedButtonLnf : public juce::LookAndFeel_V4
{
public:
    float scale = 1.f;
    void drawButtonBackground(juce::Graphics &g,
                              juce::Button &button,
                              const juce::Colour &backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 15.0f * scale;
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);

        auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 1.0f)
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
            juce::Path path;
            path.addRoundedRectangle(bounds.getX(), bounds.getY(),
                                     bounds.getWidth(), bounds.getHeight(),
                                     cornerSize, cornerSize,
                                     !(flatOnLeft || flatOnTop),
                                     !(flatOnRight || flatOnTop),
                                     !(flatOnLeft || flatOnBottom),
                                     !(flatOnRight || flatOnBottom));

            g.fillPath(path);

            g.setColour(button.findColour(juce::ComboBox::outlineColourId));
            g.strokePath(path, juce::PathStrokeType(1.0f));
        }
        else
        {
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(button.findColour(juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
    }

    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
    }
};


//class FlatButtonLnf : public juce::LookAndFeel_V4
//{
//public:
//    float scale = 1.f;
//    void drawButtonBackground(juce::Graphics &g,
//                              juce::Button &button,
//                              const juce::Colour &backgroundColour,
//                              bool shouldDrawButtonAsHighlighted,
//                              bool shouldDrawButtonAsDown) override
//    {
//        auto cornerSize = 0.0f;
//        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
//
//        auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 1.0f)
//                              .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);
//
//        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
//            baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);
//
//        g.setColour(baseColour);
//
//        auto flatOnLeft = button.isConnectedOnLeft();
//        auto flatOnRight = button.isConnectedOnRight();
//        auto flatOnTop = button.isConnectedOnTop();
//        auto flatOnBottom = button.isConnectedOnBottom();
//
//        if (flatOnLeft || flatOnRight || flatOnTop || flatOnBottom)
//        {
//            juce::Path path;
//            path.addRoundedRectangle(bounds.getX(), bounds.getY(),
//                                     bounds.getWidth(), bounds.getHeight(),
//                                     cornerSize, cornerSize,
//                                     !(flatOnLeft || flatOnTop),
//                                     !(flatOnRight || flatOnTop),
//                                     !(flatOnLeft || flatOnBottom),
//                                     !(flatOnRight || flatOnBottom));
//
//            g.fillPath(path);
//
//            g.setColour(button.findColour(juce::ComboBox::outlineColourId));
//            g.strokePath(path, juce::PathStrokeType(1.0f));
//        }
//        else
//        {
//            g.fillRoundedRectangle(bounds, cornerSize);
//            g.setColour(button.findColour(juce::ComboBox::outlineColourId));
//            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
//        }
//    }
//
//    
//    
//    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override
//    {
//        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
//    }
//};
