/*
  ==============================================================================

    LookAndFeel.h
    Created: 12 Jul 2020 8:54:38am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/
#pragma once

#include "InterfaceDefines.h"

inline void drawInnerShadow(juce::Graphics &g, juce::Path target) {
    // resets the Clip Region when the function returns
    juce::Graphics::ScopedSaveState saveState(g);

    // invert the path's fill shape and enlarge it,
    // so it casts a shadow
    juce::Path shadowPath(target);
    shadowPath.addRectangle(target.getBounds().expanded(10));
    shadowPath.setUsingNonZeroWinding(false);

    // reduce clip region to avoid the shadow
    // being drawn outside of the shape to cast the shadow on
    g.reduceClipRegion(target);

    juce::DropShadow ds(COLOUR7, 5, {0, 1});
    ds.drawForPath(g, shadowPath);
}

class OtherLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // resize scale
    float scale = 1.0f;

    OtherLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, KNOB_FONT_COLOUR);
        setColour(juce::Slider::textBoxBackgroundColourId, COLOUR6);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
        setColour(juce::Slider::textBoxHighlightColourId, COLOUR7);
        setColour(juce::Label::textColourId, COLOUR1);
        setColour(juce::Label::textWhenEditingColourId, COLOUR1);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentWhite);
        setColour(juce::Label::backgroundWhenEditingColourId, COLOUR6);
        setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::transparentWhite);
        setColour(juce::Slider::trackColourId, COLOUR1);
        setColour(juce::Slider::thumbColourId, COLOUR5);
        setColour(juce::Slider::backgroundColourId, COLOUR6);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colours::transparentWhite);
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
        auto lineW = radius * 0.1f;
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
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

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
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
//
//        g.setColour(COLOUR5.withBrightness(slider.isEnabled() ? 1.0f : 0.5f));
//        g.fillEllipse(dialArea);

        // draw tick
        //g.setColour(COLOUR1.withBrightness(slider.isEnabled() ? 1.0f : 0.5f));
        //juce::Path dialTick;
        //dialTick.addRectangle(0, -radiusInner, radiusInner * 0.1f, radiusInner * 0.3);
        //dialTick.addEllipse(0, -radiusInner, radiusInner * 0.3f, radiusInner * 0.3);
        //g.fillPath(dialTick, juce::AffineTransform::rotation(angle).translated(centerX, centerY));
        //g.setColour(COLOUR5);
        //g.drawEllipse(rx, ry, diameter, diameter, 1.0f);
        
        // draw big circle
        juce::Path dialTick;
        dialTick.addRectangle(0, -radiusInner, radiusInner * 0.1f, radiusInner * 0.3);
        juce::ColourGradient grad(juce::Colours::black, centerX, centerY,
                                  juce::Colours::black.withBrightness(slider.isEnabled() ? 1.0f : 0.5f), radiusInner, radiusInner, true);
        g.setGradientFill(grad);
        g.fillEllipse(dialArea);
        
        // draw small circle
        juce::Rectangle<float> smallDialArea(rx + radiusInner / 10.0f * 3, ry + radiusInner / 10.0f * 3, diameterInner / 10.0f * 7, diameterInner / 10.0f * 7);
        g.setColour(COLOUR5.withBrightness(slider.isEnabled() ? 1.0f : 0.5f));
        g.fillEllipse(smallDialArea);
        
        // draw tick
        g.setColour(COLOUR3.withBrightness(slider.isEnabled() ? 1.0f : 0.5f));
        g.fillPath(dialTick, juce::AffineTransform::rotation(angle).translated(centerX, centerY));
        
        // when mouse is over or dragging, change the label name to the slider value
//        auto isMouseOver = slider.isMouseOverOrDragging () && slider.isEnabled ();
//
//        if (!isMouseOver)
//        {
//            slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
//        }
//        else
//        {
//            slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
//        }
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

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        float cornerSize = 10.0f * scale;
        if (dynamic_cast<juce::AlertWindow*> (textEditor.getParentComponent()) != nullptr)
        {
            g.setColour (textEditor.findColour (juce::TextEditor::backgroundColourId));
            g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), cornerSize);
            g.setColour (textEditor.findColour (juce::TextEditor::outlineColourId));
            g.drawHorizontalLine (height - 1, 0.0f, static_cast<float> (width));
        }
        else
        {
            g.setColour (textEditor.findColour (juce::TextEditor::backgroundColourId));
            g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), cornerSize);
        }
//        juce::Path pathShadow;
//        pathShadow.addRoundedRectangle(textEditor.getLocalBounds().toFloat().reduced (0.5f, 0.5f), cornerSize);
//        g.setColour (juce::Colours::black.withAlpha ((textEditor.isEnabled() ? 0.9f : 0.2f)));
//        drawInnerShadow(g, pathShadow);
    }
    
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        float cornerSize = 10.0f * scale;
        g.setColour(label.findColour (juce::Label::backgroundColourId));
        g.fillRoundedRectangle(label.getLocalBounds().toFloat(), cornerSize);
        
        if (! label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            const juce::Font font (getLabelFont (label));

            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (font);

            auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());

            g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                              label.getMinimumHorizontalScale());

            g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
        }
        else if (label.isEnabled())
        {
            g.setColour (label.findColour (juce::Label::outlineColourId));
        }

        g.drawRoundedRectangle (label.getLocalBounds().toFloat(), cornerSize, 1);
        
//        if (label.isEditable())
//        {
//            juce::Path pathShadow;
//            pathShadow.addRoundedRectangle(label.getLocalBounds().toFloat().reduced (0.5f, 0.5f), cornerSize);
//            g.setColour (juce::Colours::black.withAlpha ((label.isEnabled() ? 0.9f : 0.2f)));
//            drawInnerShadow(g, pathShadow);
//        }
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
    
    // rounded menu box
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto cornerSize = 10.0f * scale;
        juce::Rectangle<int> boxBounds (0, 0, width, height);

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);
        
        // draw shadow
        if (box.getY() > 100) // top preset box don't need shadow
        {
            juce::Path pathShadow;
            pathShadow.addRoundedRectangle(boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize);
            g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha ((box.isEnabled() ? 0.9f : 0.2f)));
            drawInnerShadow(g, pathShadow);
        }
        
        juce::Rectangle<int> arrowZone (width - 30, 0, 20, height);
        juce::Path path;
        path.startNewSubPath ((float) arrowZone.getX() + 3.0f, (float) arrowZone.getCentreY() - 2.0f);
        path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 3.0f);
        path.lineTo ((float) arrowZone.getRight() - 3.0f, (float) arrowZone.getCentreY() - 2.0f);

        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha ((box.isEnabled() ? 0.9f : 0.2f)));
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }
    
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        
        // g.fillAll(COLOUR6); // findColour (PopupMenu::backgroundColourId)
        g.setColour(COLOUR6);
#if JUCE_MAC
        g.fillRoundedRectangle(0, 0, width, height, 10);
#endif
        juce::ignoreUnused(width, height);
       
#if ! JUCE_MAC
        g.fillAll(COLOUR6);
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

            auto r = area.reduced(1); //auto r = area.reduced(1);

            if (isHighlighted && isActive)
            {
                g.setColour(COLOUR5); //findColour(juce::PopupMenu::highlightedBackgroundColourId) : red
                // g.fillRect(r);
                g.fillRoundedRectangle(r.toFloat(), 10.0f);
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

class DriveLookAndFeel : public juce::LookAndFeel_V4
{
private:
    float x1, y1, x2, y2;
    float changePos = 0; // use this to change drive knob gradient
public:
    // resize scale
    float scale = 1.0f;
    inline static float reductionPrecent = 1.0f;
    inline static float sampleMaxValue = 0.0f;
    
    DriveLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, KNOB_FONT_COLOUR);
        setColour(juce::Slider::textBoxBackgroundColourId, COLOUR6);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
        setColour(juce::Slider::textBoxHighlightColourId, COLOUR7);
        setColour(juce::Label::textColourId, COLOUR_FONT);
        setColour(juce::Label::textWhenEditingColourId, COLOUR1);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentWhite);
        setColour(juce::Label::backgroundWhenEditingColourId, COLOUR6);
        setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::transparentWhite);
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
        auto lineW = radius * 0.2f;
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
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // draw shadow
        juce::Path backgroundShadowArc;
        backgroundShadowArc.addCentredArc(bounds.getCentreX(),
                                    bounds.getCentreY(),
                                    arcRadius + lineW / 2.0f,
                                    arcRadius + lineW / 2.0f,
                                    0.0f,
                                    0,
                                    2 * M_PI,
                                    true);
        //backgroundShadowArc.lineTo(<#Point<float> end#>)
        backgroundShadowArc.addCentredArc(bounds.getCentreX(),
                                    bounds.getCentreY(),
                                    arcRadius - lineW / 2.0f,
                                    arcRadius - lineW / 2.0f,
                                    0.0f,
                                    0,
                                    2 * M_PI,
                                    true);
        drawInnerShadow(g, backgroundShadowArc);
        //DBG(reductionPrecent);
        float reductAngle = toAngle - (1.0f - reductionPrecent) * 2 * M_PI;
        
        // safe reduce paint
        if (reductAngle < rotaryStartAngle)
        {
            reductAngle = rotaryStartAngle;
        }
        
        if (slider.isEnabled())
        {
            // real drive path
            juce::Path valueArc;
            valueArc.addCentredArc(bounds.getCentreX(),
                                   bounds.getCentreY(),
                                   arcRadius,
                                   arcRadius,
                                   0.0f,
                                   rotaryStartAngle,
                                   reductAngle,
                                   true);
            
            // circle path, is used to draw dynamic color loop
            juce::Path circlePath;
            circlePath.addCentredArc(bounds.getCentreX(),
                                   bounds.getCentreY(),
                                   arcRadius,
                                   arcRadius,
                                   0.0f,
                                   0,
                                   2 * M_PI,
                                   true);
            
            // make the color loop move
            if (sampleMaxValue > 0.00001f)
            {
                if (changePos < circlePath.getLength())
                {
                    changePos += 10;
                }
                else
                {
                    changePos = 0;
                }
//                juce::Point<float> p1 = circlePath.getPointAlongPath(changePos);
//                x1 = p1.x;
//                y1 = p1.y;
//                x2 = p1.x + 100 * scale;
//                y2 = p1.y + 100 * scale;
            }
            else // normal paint static color
            {
//                juce::Point<float> p1 = valueArc.getPointAlongPath(valueArc.getLength());
//                x1 = p1.x;
//                y1 = p1.y;
//                x2 = p1.x + 100 * scale;
//                y2 = p1.y + 100 * scale;
            }
            // 2021.6.3
            juce::Point<float> p1 = circlePath.getPointAlongPath(changePos);
            x1 = p1.x;
            y1 = p1.y;
            x2 = p1.x + 100 * scale;
            y2 = p1.y + 100 * scale;
            
            // draw real drive path
            juce::ColourGradient grad(juce::Colours::red, x1, y1,
                                      COLOUR1, x2, y2, true);
            g.setGradientFill(grad);
            
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            if (reductionPrecent != 1)
            {
                // draw reducted path
                juce::Path valueArcReduce;
                valueArcReduce.addCentredArc(bounds.getCentreX(),
                                             bounds.getCentreY(),
                                             arcRadius,
                                             arcRadius,
                                             0.0f,
                                             reductAngle - 0.02f,
                                             toAngle,
                                             true);
                
                juce::ColourGradient grad(juce::Colours::red.withAlpha(0.5f), x1, y1,
                                          COLOUR1.withAlpha(0.5f), x2, y2, true);
                g.setGradientFill(grad);
                
                g.strokePath(valueArcReduce, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        //auto thumbWidth = lineW * 1.0f;
        //Point<float> thumbPoint(bounds.getCentreX() + arcRadius * std::cos(toAngle - MathConstants<float>::halfPi),
        //    bounds.getCentreY() + arcRadius * std::sin(toAngle - MathConstants<float>::halfPi));

        //g.setColour(COLOUR5);
        //g.fillEllipse(Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));

        // draw inner circle
        float diameterInner = juce::jmin(width, height) * 0.4f;
        float radiusInner = diameterInner / 2;
        float centerX = x + width / 2;
        float centerY = y + height / 2;
        float rx = centerX - radiusInner;
        float ry = centerY - radiusInner;
        float angle = rotaryStartAngle + (sliderPos * (rotaryEndAngle - rotaryStartAngle));

        juce::Rectangle<float> dialArea(rx, ry, diameterInner, diameterInner);
        
        // draw big circle
        juce::Path dialTick;
        dialTick.addRectangle(0, -radiusInner, radiusInner * 0.1f, radiusInner * 0.3);
        juce::ColourGradient grad(juce::Colours::black, centerX, centerY,
                                  juce::Colours::white.withBrightness(slider.isEnabled() ? 0.5f : 0.2f), radiusInner, radiusInner, true);
        g.setGradientFill(grad);
        g.fillEllipse(dialArea);
        
        // draw small circle
        juce::Rectangle<float> smallDialArea(rx + radiusInner / 10.0f * 3, ry + radiusInner / 10.0f * 3, diameterInner / 10.0f * 7, diameterInner / 10.0f * 7);
        g.setColour(juce::Colours::black.withBrightness(slider.isEnabled() ? 0.3f : 0.2f));
        g.fillEllipse(smallDialArea);
        
        // draw colorful inner circle
        if (sampleMaxValue > 0.00001f && slider.isEnabled())
        {
//            if (rotaryStartAngle == toAngle)
//            {
//                reductAngle = rotaryStartAngle + 0.1f;
//            }
//            juce::Path valueArc;
//            valueArc.addCentredArc(bounds.getCentreX(),
//                                   bounds.getCentreY(),
//                                   arcRadius,
//                                   arcRadius,
//                                   0.0f,
//                                   rotaryStartAngle,
//                                   reductAngle,
//                                   true);
//            juce::ColourGradient grad(juce::Colours::red, valueArc.getPointAlongPath(valueArc.getLength()).x, valueArc.getPointAlongPath(valueArc.getLength()).y,
//                                      COLOUR1, rx + diameterInner * sampleMaxValue, ry + diameterInner * sampleMaxValue, true);
//            g.setGradientFill(grad);
//            g.fillEllipse(dialArea);
            
            
            // draw colorful tick
            g.setColour(juce::Colours::orange.withBrightness(slider.isEnabled() ? 1.0f : 0.2f));
            g.fillPath(dialTick, juce::AffineTransform::rotation(angle).translated(centerX, centerY));
        }
        else // draw grey 2 layers circle
        {
            // draw grey tick
            g.setColour(juce::Colours::lightgrey.withBrightness(slider.isEnabled() ? 0.5f : 0.2f));
            g.fillPath(dialTick, juce::AffineTransform::rotation(angle).translated(centerX, centerY));
        }

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
    
    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        float cornerSize = 10.0f * scale;
        if (dynamic_cast<juce::AlertWindow*> (textEditor.getParentComponent()) != nullptr)
        {
            g.setColour (textEditor.findColour (juce::TextEditor::backgroundColourId));
            g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), cornerSize);
            g.setColour (textEditor.findColour (juce::TextEditor::outlineColourId));
            g.drawHorizontalLine (height - 1, 0.0f, static_cast<float> (width));
        }
        else
        {
            g.setColour (textEditor.findColour (juce::TextEditor::backgroundColourId));
            g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), cornerSize);
        }
//        juce::Path pathShadow;
//        pathShadow.addRoundedRectangle(textEditor.getLocalBounds().toFloat().reduced (0.5f, 0.5f), cornerSize);
//        g.setColour (juce::Colours::black.withAlpha ((textEditor.isEnabled() ? 0.9f : 0.2f)));
//        drawInnerShadow(g, pathShadow);
    }
    
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        float cornerSize = 10.0f * scale;
        g.setColour(label.findColour (juce::Label::backgroundColourId));
        g.fillRoundedRectangle(label.getLocalBounds().toFloat(), cornerSize);
        
        if (! label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            const juce::Font font (getLabelFont (label));

            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (font);

            auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());

            g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                              label.getMinimumHorizontalScale());

            g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
        }
        else if (label.isEnabled())
        {
            g.setColour (label.findColour (juce::Label::outlineColourId));
        }

        g.drawRoundedRectangle (label.getLocalBounds().toFloat(), cornerSize, 1);
        
//        if (label.isEditable())
//        {
//            juce::Path pathShadow;
//            pathShadow.addRoundedRectangle(label.getLocalBounds().toFloat().reduced (0.5f, 0.5f), cornerSize);
//            g.setColour (juce::Colours::black.withAlpha ((label.isEnabled() ? 0.9f : 0.2f)));
//            drawInnerShadow(g, pathShadow);
//        }
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
};

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
        auto cornerSize = 10.0f * scale;
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

class HighPassButtonLnf : public juce::LookAndFeel_V4
{
public:
    float scale = 1.f;
    void drawButtonBackground(juce::Graphics &g,
                              juce::Button &button,
                              const juce::Colour &backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 10.0f * scale;
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

        // draw high pass filter
        if (button.isEnabled()) g.setColour(COLOUR1);
        else g.setColour(COLOUR1.darker(0.5f));
        
        juce::Path p;
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        p.startNewSubPath(width * 0.25f, height);
        p.lineTo(width / 2.0f, height / 2.0f);
        p.lineTo(width - 1, height / 2.0f);
        juce::Path roundedPath = p.createPathWithRoundedCorners(5.0f);
        g.strokePath(roundedPath, juce::PathStrokeType(1.0f));
        
        // fill above part
        if (button.isEnabled()) g.setColour(COLOUR1.withAlpha(0.5f));
        else g.setColour(COLOUR1.withAlpha(0.5f).darker(0.5f));
        roundedPath.closeSubPath();
        g.fillPath(roundedPath);
        
        // fill below part
        juce::Path p2;
        p2.startNewSubPath(width * 0.25f, height);
        p2.lineTo(width - 1, height / 2.0f);

        auto csx = juce::jmin (cornerSize, width * 0.5f);
        auto csy = juce::jmin (cornerSize, height * 0.5f);
        auto cs45x = csx * 0.45f;
        auto cs45y = csy * 0.45f;
        auto x2 = width;
        auto y2 = height;

        p2.lineTo (x2, y2 - csy);
        p2.cubicTo (x2, y2 - cs45y, x2 - cs45x, y2, x2 - csx, y2);
        p2.closeSubPath();
        
        g.fillPath(p2);
    }

    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
    }
};

class LowPassButtonLnf : public juce::LookAndFeel_V4
{
public:
    float scale = 1.f;
    void drawButtonBackground(juce::Graphics &g,
                              juce::Button &button,
                              const juce::Colour &backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 10.0f * scale;
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
        
        // draw low filter path
        if (button.isEnabled()) g.setColour(COLOUR1);
        else g.setColour(COLOUR1.darker(0.5f));
        juce::Path p;
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        p.startNewSubPath(1.0f, height / 2.0f);
        p.lineTo(width / 2.0f, height / 2.0f);
        p.lineTo(width * 0.75f, height);
        juce::Path roundedPath = p.createPathWithRoundedCorners(5.0f);
        g.strokePath(roundedPath, juce::PathStrokeType(1.0f));
        
        // fill above part
        if (button.isEnabled()) g.setColour(COLOUR1.withAlpha(0.5f));
        else g.setColour(COLOUR1.withAlpha(0.5f).darker(0.5f));
        roundedPath.closeSubPath();
        g.fillPath(roundedPath);
        
        // fill below part
        juce::Path p2;
        p2.startNewSubPath(1.0f, height / 2.0f);
        p2.lineTo(width * 0.75f, height);

        auto csx = juce::jmin (cornerSize, width * 0.5f);
        auto csy = juce::jmin (cornerSize, height * 0.5f);
        auto cs45x = csx * 0.45f;
        auto cs45y = csy * 0.45f;

        auto y2 = height;

        p2.lineTo (csx, y2);
        p2.cubicTo (cs45x, y2, 0, y2 - cs45y, 0, y2 - csy);
        p2.closeSubPath();
        
        g.fillPath(p2);
    }

    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
    }
};

class BandPassButtonLnf : public juce::LookAndFeel_V4
{
public:
    float scale = 1.f;
    void drawButtonBackground(juce::Graphics &g,
                              juce::Button &button,
                              const juce::Colour &backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 10.0f * scale;
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
        
        // draw band filter path
        if (button.isEnabled()) g.setColour(COLOUR1);
        else g.setColour(COLOUR1.darker(0.5f));
        juce::Path p;
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        p.startNewSubPath(width * 0.25f, height);
        p.lineTo(width / 2.0f, height * 0.4f);
        p.lineTo(width * 0.75f, height);
        juce::Path roundedPath = p.createPathWithRoundedCorners(20.0f);
        g.strokePath(roundedPath, juce::PathStrokeType(1.0f));
        
        if (button.isEnabled()) g.setColour(COLOUR1.withAlpha(0.5f));
        else g.setColour(COLOUR1.withAlpha(0.5f).darker(0.5f));
        g.fillPath(roundedPath);
    }

    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override
    {
        return juce::Font(KNOB_FONT, "Regular", KNOB_FONT_SIZE * scale);
    }
};


class FlatButtonLnf : public juce::LookAndFeel_V4
{
public:
    float scale = 1.f;
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
};
