/*
  ==============================================================================

    LookAndFeel.h
    Created: 12 Jul 2020 8:54:38am
    Author:  羽翼深蓝Wings

    Refactored into a single unified class with scaling support.

  ==============================================================================
*/
#pragma once

#include "InterfaceDefines.h"
#include "ModulatableSlider.h" // Include the new header
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"

#if ! JUCE_MAC
#define M_PI 3.1415926
#endif

inline void drawInnerShadow(juce::Graphics& g, juce::Path target)
{
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

    juce::DropShadow ds(COLOUR7, 5, { 0, 1 });
    ds.drawForPath(g, shadowPath);
}

// A single, unified LookAndFeel class for the entire plugin.
// It uses Component IDs to determine the style of the component to draw.
class FireLookAndFeel : public juce::LookAndFeel_V4
{
public:
    //==============================================================================
    // UI Scaling
    float scale = 1.0f;

    // Public data members for dynamic properties, previously in DriveLookAndFeel
    float reductionPercent = 1.0f;
    float sampleMaxValue = 0.0f;

    FireLookAndFeel()
    {
        // Setup default colours for all components
        setColour(juce::Slider::textBoxTextColourId, KNOB_SUBFONT_COLOUR);
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

    //==============================================================================
    // PopupMenu and ComboBox
    //==============================================================================

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto cornerSize = 10.0f * scale;
        juce::Rectangle<int> boxBounds(0, 0, width, height);

        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f, 0.5f), cornerSize, 1.0f);

        // Draw arrow
        const int arrowZoneWidth = juce::roundToInt(20.0f * scale);
        const int arrowZoneRightMargin = juce::roundToInt(10.0f * scale);
        juce::Rectangle<int> arrowZone(width - arrowZoneWidth - arrowZoneRightMargin, 0, arrowZoneWidth, height);

        juce::Path path;

        const float x_indent = 5.0f * scale;
        const float y_indent_top = 2.0f * scale;
        const float y_indent_bottom = 3.0f * scale;

        path.startNewSubPath((float) arrowZone.getX() + x_indent, (float) arrowZone.getCentreY() - y_indent_top);
        path.lineTo((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + y_indent_bottom);
        path.lineTo((float) arrowZone.getRight() - x_indent, (float) arrowZone.getCentreY() - y_indent_top);

        g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha((box.isEnabled() ? 0.9f : 0.2f)));
        g.fillPath(path);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        // We explicitly use COLOUR6, ignoring the transparent default
        g.setColour(COLOUR6);

#if JUCE_MAC
        g.fillRoundedRectangle(0, 0, width, height, 10.0f * scale);
#else
        g.fillAll();
        g.setColour(findColour(juce::PopupMenu::textColourId).withAlpha(0.6f));
        g.drawRect(0, 0, width, height);
#endif
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, const bool isSeparator, const bool isActive, const bool isHighlighted, const bool isTicked, const bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* const textColourToUse) override
    {
        //g.setColour(COLOUR6); // backgroundColourId : black
        //g.fillAll();

        if (isSeparator)
        {
            auto r = area.reduced(5, 0);
            r.removeFromTop(juce::roundToInt(((float) r.getHeight() * 0.5f) - 0.5f));

            g.setColour(findColour(juce::PopupMenu::textColourId).withAlpha(0.3f)); //findColour(juce::PopupMenu::textColourId)
            g.fillRect(r.removeFromTop(1));
        }
        else
        {
            auto textColour = (textColourToUse == nullptr ? findColour(juce::PopupMenu::textColourId)
                                                          : *textColourToUse); //findColour(juce::PopupMenu::textColourId)

            auto r = area.reduced(1); //auto r = area.reduced(1);

            if (isHighlighted && isActive)
            {
                g.setColour(COLOUR5); //findColour(juce::PopupMenu::highlightedBackgroundColourId) : red
                // g.fillRect(r);
                g.fillRoundedRectangle(r.toFloat(), 10.0f);
                g.setColour(findColour(juce::PopupMenu::highlightedTextColourId)); //findColour(juce::PopupMenu::highlightedTextColourId)
            }
            else
            {
                g.setColour(textColour.withMultipliedAlpha(isActive ? 1.0f : 0.5f));
            }

            r.reduce(juce::jmin(5, area.getWidth() / 20), 0);

            auto font = getPopupMenuFont();

            auto maxFontHeight = (float) r.getHeight() / 1.3f;

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

                auto x = static_cast<float>(r.removeFromRight((int) arrowH).getX());
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

    //==============================================================================
    // Labels (including Slider TextBoxes)
    //==============================================================================
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        float cornerSize = 10.0f * scale;
        g.setColour(label.findColour(juce::Label::backgroundColourId));
        g.fillRoundedRectangle(label.getLocalBounds().toFloat(), cornerSize);

        if (! label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            const juce::Font font(getLabelFont(label));

            g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.setFont(font);

            auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

            g.drawFittedText(label.getText(), textArea, label.getJustificationType(), juce::jmax(1, (int) ((float) textArea.getHeight() / font.getHeight())), label.getMinimumHorizontalScale());

            g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
        }
        else if (label.isEnabled())
        {
            g.setColour(label.findColour(juce::Label::outlineColourId));
        }

        g.drawRoundedRectangle(label.getLocalBounds().toFloat(), cornerSize, 1);

        //        if (label.isEditable())
        //        {
        //            juce::Path pathShadow;
        //            pathShadow.addRoundedRectangle(label.getLocalBounds().toFloat().reduced (0.5f, 0.5f), cornerSize);
        //            g.setColour (juce::Colours::black.withAlpha ((label.isEnabled() ? 0.9f : 0.2f)));
        //            drawInnerShadow(g, pathShadow);
        //        }
    }

    //==============================================================================
    // Sliders
    //==============================================================================
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        // Dispatch to the correct drawing method based on component ID
        if (auto* modSlider = dynamic_cast<ModulatableSlider*>(&slider))
        {
            if (slider.getComponentID() == "drive")
            {
                // If it's the drive knob, call our newly modified function
                drawDriveSlider(g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, *modSlider);
            }
            else
            {
                // Otherwise, call the standard modulatable slider function
                drawModulatableSlider(g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, *modSlider);
            }
            if (modSlider->assignModeGlowAlpha > 0.0f)
            {
                auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10.0f);
                auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;

                // Use the alpha value from the slider itself
                g.setColour(juce::Colours::yellow.withAlpha(modSlider->assignModeGlowAlpha));
                g.fillEllipse(bounds.getCentreX() - radius, bounds.getCentreY() - radius, radius * 2.0f, radius * 2.0f);
            }
        }
        else
        {
            // Fallback for any other standard sliders
            drawDefaultSlider(g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
        }
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
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
                else /* left or right -> centre vertically */
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

    juce::Button* createSliderButton(juce::Slider& slider, bool isIncrement) override
    {
        auto* button = new juce::TextButton();

        if (isIncrement)
            button->setComponentID("slider_up_arrow");
        else
            button->setComponentID("slider_down_arrow");

        button->setColour(juce::TextButton::buttonColourId, slider.findColour(juce::Slider::textBoxBackgroundColourId));
        button->setColour(juce::TextButton::textColourOnId, slider.findColour(juce::Slider::thumbColourId));
        button->setColour(juce::TextButton::textColourOffId, slider.findColour(juce::Slider::thumbColourId));

        return button;
    }

    //==============================================================================
    // Buttons
    //==============================================================================
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
        auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 1.0f)
                              .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);

        g.setColour(baseColour);

        const auto& id = button.getComponentID();

        float cornerSize = 0.0f; // Default to flat corners

        if (id == "rounded" || id == "low_cut" || id == "high_cut" || id == "band_pass")
        {
            cornerSize = 10.0f * scale; // Apply scaling to corner size
        }

        g.fillRoundedRectangle(bounds, cornerSize);
        g.setColour(button.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

        if (id == "zoom")
        {
            g.setColour(button.findColour(juce::TextButton::textColourOnId));
            juce::Path path;
            float width = bounds.getWidth();
            float height = bounds.getHeight();

            float startX = width / 4.0f;
            float startY = height / 4.0f;
            float endX = width * 3.0f / 4.0f;
            float endY = height * 3.0f / 4.0f;

            // Diagonal line
            path.startNewSubPath(startX + 1.0f, startY + 1.0f);
            path.lineTo(endX - 1.0f, endY - 1.0f);
            g.strokePath(path, juce::PathStrokeType(2.0f));

            // Top-left arrow
            juce::Path pathArrow1;
            pathArrow1.startNewSubPath(startX, startY + height / 4.0f);
            pathArrow1.lineTo(startX, startY);
            pathArrow1.lineTo(startX + width / 4.0f, startY);
            pathArrow1.closeSubPath();
            g.fillPath(pathArrow1);

            // Bottom-right arrow
            juce::Path pathArrow2;
            pathArrow2.startNewSubPath(endX, endY - height / 4.0f);
            pathArrow2.lineTo(endX, endY);
            pathArrow2.lineTo(endX - width / 4.0f, endY);
            pathArrow2.closeSubPath();
            g.fillPath(pathArrow2);
        }
        else if (id == "slider_up_arrow" || id == "slider_down_arrow")
        {
            float arrowSize = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.3f;
            auto arrowArea = bounds.withSizeKeepingCentre(arrowSize, arrowSize);
            juce::Path p;

            if (id == "slider_up_arrow")
            {
                p.startNewSubPath(arrowArea.getX(), arrowArea.getBottom());
                p.lineTo(arrowArea.getCentreX(), arrowArea.getY());
                p.lineTo(arrowArea.getRight(), arrowArea.getBottom());
            }
            else // slider_down_arrow
            {
                p.startNewSubPath(arrowArea.getX(), arrowArea.getY());
                p.lineTo(arrowArea.getCentreX(), arrowArea.getBottom());
                p.lineTo(arrowArea.getRight(), arrowArea.getY());
            }
            p.closeSubPath();
            g.setColour(button.findColour(juce::TextButton::textColourOnId));
            g.fillPath(p);
        }
        else if (id == "left_arrow" || id == "right_arrow")
        {
            g.setColour(button.findColour(juce::TextButton::textColourOnId));

            float arrowSize = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.3f;
            auto arrowArea = bounds.withSizeKeepingCentre(arrowSize, arrowSize);

            juce::Path p;

            if (id == "left_arrow")
            {
                p.startNewSubPath(arrowArea.getRight(), arrowArea.getY());
                p.lineTo(arrowArea.getX(), arrowArea.getCentreY());
                p.lineTo(arrowArea.getRight(), arrowArea.getBottom());
            }
            else // right_arrow
            {
                p.startNewSubPath(arrowArea.getX(), arrowArea.getY());
                p.lineTo(arrowArea.getRight(), arrowArea.getCentreY());
                p.lineTo(arrowArea.getX(), arrowArea.getBottom());
            }

            p.closeSubPath();
            g.fillPath(p);
        }
        else
        {
            // Logic for other button types (like filter icons)
            juce::Colour textColour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
            if (! button.isEnabled())
                textColour = textColour.darker(0.5f);

            g.setColour(textColour);

            if (id == "low_cut")
                drawFilterPath(g, bounds, FilterType::LowCut, textColour);
            else if (id == "high_cut")
                drawFilterPath(g, bounds, FilterType::HighCut, textColour);
            else if (id == "band_pass")
                drawFilterPath(g, bounds, FilterType::BandPass, textColour);
        }
    }

    void drawTickBox(juce::Graphics& g, juce::Component& component, float x, float y, float w, float h, const bool ticked, const bool isEnabled, const bool shouldDrawButtonAsHighlighted, const bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(isEnabled, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        const auto& id = component.getComponentID();

        if (id == "flat_toggle")
        {
            juce::Rectangle<float> tickBounds(component.getLocalBounds().toFloat().reduced(2));
            juce::Colour tickColour = component.findColour(ticked ? juce::ToggleButton::tickColourId : juce::ToggleButton::tickDisabledColourId);

            if (component.isMouseButtonDown())
                tickColour = tickColour.contrasting(0.05f);
            else if (component.isMouseOver())
                tickColour = tickColour.contrasting(0.2f);

            g.setColour(tickColour);
            g.fillRect(tickBounds);
        }
        else // This 'else' block handles your bypassButtons
        {
            juce::Rectangle<float> tickBounds(x, y, w, h);

            // 1. Determine the base colour based on the 'ticked' state.
            auto baseColour = ticked ? component.findColour(juce::ToggleButton::tickColourId)
                                     : juce::Colours::grey;

            // 2. ONLY apply hover/down effects if the component is ENABLED.
            if (isEnabled)
            {
                if (component.isMouseButtonDown())
                    baseColour = baseColour.darker();
                else if (component.isMouseOver())
                    baseColour = baseColour.darker(0.3f);
            }

            // 3. Apply a final alpha modification if the component is disabled.
            // This makes it look "greyed out" when the enableButton is off.
            g.setColour(baseColour.withMultipliedAlpha(isEnabled ? 1.0f : 0.5f));

            // 4. Draw the button shape.
            // The logic below is slightly simplified from your original for clarity.
            float radius = juce::jmin(tickBounds.getWidth(), tickBounds.getHeight()) / 2.0f;
            float lineW = radius * 0.2f;
            auto arcRadius = radius - lineW * 2.0f;

            juce::Path powerSymbol;
            powerSymbol.addCentredArc(tickBounds.getCentreX(),
                                      tickBounds.getCentreY(),
                                      arcRadius,
                                      arcRadius,
                                      0.0f,
                                      (float) juce::MathConstants<double>::pi * 0.2f,
                                      (float) juce::MathConstants<double>::pi * 1.8f,
                                      true);

            powerSymbol.startNewSubPath(tickBounds.getCentreX(), tickBounds.getY() + tickBounds.getHeight() * 0.2f);
            powerSymbol.lineTo(tickBounds.getCentreX(), tickBounds.getCentreY());

            // Use the final calculated colour to draw.
            g.strokePath(powerSymbol, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        if (button.getComponentID() == "flat_toggle")
        {
            g.setColour(COLOUR6);
            g.fillRect(button.getLocalBounds());
            drawTickBox(g, button, 0, 0, 0, 0, button.getToggleState(), button.isEnabled(), shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        }
        else
        {
            drawTickBox(g, button, button.getWidth() / 4.0f, button.getHeight() / 4.0f, button.getWidth() / 2.0f, button.getHeight() / 2.0f, button.getToggleState(), button.isEnabled(), shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        }
    }

    //==============================================================================
    // Fonts
    //==============================================================================
    juce::Font getBaseFont(float height = KNOB_FONT_SIZE)
    {
        return juce::Font {
            juce::FontOptions()
                .withName(KNOB_FONT)
                .withStyle("Regular")
                .withHeight(height * scale) // Apply scaling to font height
        };
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override { return getBaseFont(); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return getBaseFont(); }
    juce::Font getPopupMenuFont() override { return getBaseFont(); }
    juce::Font getLabelFont(juce::Label&) override { return getBaseFont(); }

private:
    //==============================================================================
    // Private Drawing Methods
    //==============================================================================
    float x1, y1, x2, y2;
    float changePos = 0;
    void drawDefaultSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider)
    {
        auto outline = COLOUR6;
        auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId).withBrightness(slider.isEnabled() ? 1.0f : 0.5f);
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10 * scale);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto lineW = radius * 0.2f;
        auto arcRadius = radius - lineW * 0.5f;

        juce::Path backgroundArc;
        backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(outline);
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(fill);
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        float diameterInner = juce::jmin(width, height) * 0.4;
        float radiusInner = diameterInner / 2;
        float centerX = x + width / 2;
        float centerY = y + height / 2;
        juce::Rectangle<float> dialArea(centerX - radiusInner, centerY - radiusInner, diameterInner, diameterInner);

        juce::Colour innerColor = KNOB_INNER_COLOUR;

        // --- CORRECTED HIGHLIGHT LOGIC ---
        bool highlight = false;
        if (auto* ms = dynamic_cast<ModulatableSlider*>(&slider))
        {
            // If it's a ModulatableSlider, use our specific hover logic
            highlight = (ms->isMouseOverMainSlider() || ms->isMouseButtonDown()) && ! ms->isModHandleMouseDown;
        }
        else
        {
            // For all other sliders, use the default JUCE logic
            highlight = slider.isMouseOverOrDragging();
        }

        if (highlight && slider.isEnabled())
        {
            innerColor = innerColor.brighter();
        }

        g.setColour(innerColor);
        g.fillEllipse(dialArea);

        juce::Path dialTick;
        dialTick.addRectangle(0, -radiusInner * 0.95f, radiusInner * 0.15f, radiusInner * 0.3f);
        g.setColour(KNOB_TICK_COLOUR);
        g.fillPath(dialTick, juce::AffineTransform::rotation(toAngle).translated(centerX, centerY));
    }

    // Add this new private function to your LookAndFeel class
    void drawModulationVisuals(juce::Graphics& g, int x, int y, int width, int height, float rotaryStartAngle, float rotaryEndAngle, ModulatableSlider& slider)
    {
        // --- Modulation Visualization ---
        if (slider.isModulated && slider.lfoAmount != 0.0 && slider.isEnabled())
        {
            auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10 * scale);
            auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
            auto center = bounds.getCentre();
            auto lineW = radius * 0.2f;
            auto arcRadius = radius - lineW * 0.5f;

            float modulationArcRadius = arcRadius - lineW * 1.2f;
            float modulationArcWidth = lineW * 0.5f;

            // This lambda correctly converts a REAL value to an angle using the slider's non-linear mapping
            auto valueToAngle = [&](double value)
            {
                return rotaryStartAngle + slider.valueToProportionOfLength(value) * (rotaryEndAngle - rotaryStartAngle);
            };

            // 1. Get the slider's knob position as a normalized value [0, 1]
            const double normalizedCurrentValue = slider.valueToProportionOfLength(slider.getValue());

            // 2. Define modulation depth in normalized space. lfoAmount is [-1, 1], representing the depth.
            const double normalizedModulationDepth = std::abs(slider.lfoAmount);

            // 3. Calculate modulation range in NORMALIZED space
            double modStartValueNormalized, modEndValueNormalized;
            if (slider.isBipolar)
            {
                modStartValueNormalized = normalizedCurrentValue - normalizedModulationDepth / 2.0;
                modEndValueNormalized = normalizedCurrentValue + normalizedModulationDepth / 2.0;
            }
            else
            {
                // For unipolar, the range depends on the sign of lfoAmount
                if (slider.lfoAmount > 0)
                {
                    modStartValueNormalized = normalizedCurrentValue;
                    modEndValueNormalized = normalizedCurrentValue + normalizedModulationDepth;
                }
                else // lfoAmount < 0
                {
                    modStartValueNormalized = normalizedCurrentValue - normalizedModulationDepth;
                    modEndValueNormalized = normalizedCurrentValue;
                }
            }

            // Clamp the normalized range
            modStartValueNormalized = juce::jlimit(0.0, 1.0, modStartValueNormalized);
            modEndValueNormalized = juce::jlimit(0.0, 1.0, modEndValueNormalized);

            // 4. Convert the normalized start/end points back to REAL values for the valueToAngle lambda
            const double realStartValue = slider.proportionOfLengthToValue(modStartValueNormalized);
            const double realEndValue = slider.proportionOfLengthToValue(modEndValueNormalized);

            // 5. Convert REAL values to angles
            float modStartAngle = valueToAngle(realStartValue);
            float modEndAngle = valueToAngle(realEndValue);

            // --- Drawing the modulation depth arc ---
            juce::Path modulationDepthArc;
            // Ensure the arc is always drawn clockwise
            if (modEndAngle < modStartAngle)
                std::swap(modStartAngle, modEndAngle);
            modulationDepthArc.addCentredArc(center.x, center.y, modulationArcRadius, modulationArcRadius, 0.0f, modStartAngle, modEndAngle, true);

            juce::Colour arcColour = (slider.lfoAmount > 0) ? juce::Colours::red.withAlpha(0.5f) : juce::Colours::orange.withAlpha(0.5f);
            g.setColour(arcColour);
            g.strokePath(modulationDepthArc, juce::PathStrokeType(modulationArcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // --- Drawing the current modulated value indicator ---

            // 6. Calculate the current LFO-driven value in NORMALIZED space
            // slider.lfoValue is the raw LFO output, typically [0, 1] for unipolar, or transformed to [-1, 1] for bipolar
            double lfoSignal = slider.lfoValue; // Let's assume lfoValue is correctly bipolar/unipolar already
            double currentModulatedValueNormalized;

            if (slider.isBipolar)
            {
                // Assuming slider.lfoValue is [-1, 1] for bipolar
                currentModulatedValueNormalized = normalizedCurrentValue + lfoSignal * (normalizedModulationDepth / 2.0);
            }
            else
            {
                // Assuming slider.lfoValue is [0, 1] for unipolar
                currentModulatedValueNormalized = normalizedCurrentValue + lfoSignal * slider.lfoAmount;
            }

            currentModulatedValueNormalized = juce::jlimit(0.0, 1.0, currentModulatedValueNormalized);

            // 7. Convert back to REAL value and then to angle for drawing
            const double realModulatedValue = slider.proportionOfLengthToValue(currentModulatedValueNormalized);
            float currentModAngle = valueToAngle(realModulatedValue);

            juce::Point<float> modThumbPoint(center.x + modulationArcRadius * std::cos(currentModAngle - juce::MathConstants<float>::halfPi),
                                             center.y + modulationArcRadius * std::sin(currentModAngle - juce::MathConstants<float>::halfPi));

            g.setColour(juce::Colours::red);
            g.fillEllipse(juce::Rectangle<float>(modulationArcWidth * 1.5f, modulationArcWidth * 1.5f).withCentre(modThumbPoint));
        }

        if (slider.isModulated)
        {
            auto handleBounds = slider.getModulationHandleBounds();
            juce::Colour handleColour = COLOUR5;

            if (! slider.isEnabled())
            {
                // If slider is disabled, use a darker, less saturated color
                handleColour = handleColour.darker(0.6f);
            }
            else if (slider.isModHandleMouseOver || slider.isModHandleMouseDown)
            {
                // Only show hover/click effect if enabled
                handleColour = handleColour.brighter(0.2f);
            }

            g.setColour(handleColour);
            g.fillEllipse(handleBounds);
            g.setColour(juce::Colours::white);
            g.setFont(getBaseFont(handleBounds.getHeight() * 0.6f));
            g.drawText(juce::String(slider.lfoSource), handleBounds, juce::Justification::centred);
        }
    }

    void drawModulatableSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, ModulatableSlider& slider)
    {
        // 1. Draw the default slider visuals
        drawDefaultSlider(g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);

        // 2. Draw the modulation visuals on top
        drawModulationVisuals(g, x, y, width, height, rotaryStartAngle, rotaryEndAngle, slider);
    }

    void drawDriveSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, ModulatableSlider& slider)
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
        //        DBG(reductionPrecent);
        float reductAngle = toAngle - (1.0f - reductionPercent) * 2 * M_PI;

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
            juce::ColourGradient grad(juce::Colours::red, x1, y1, COLOUR1, x2, y2, true);
            g.setGradientFill(grad);

            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (reductionPercent != 1)
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

                juce::ColourGradient grad(juce::Colours::red.withAlpha(0.5f), x1, y1, COLOUR1.withAlpha(0.5f), x2, y2, true);
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
        juce::ColourGradient grad(juce::Colours::black, centerX, centerY, juce::Colours::white.withBrightness(slider.isEnabled() ? 0.9f : 0.5f), radiusInner, radiusInner, true);
        g.setGradientFill(grad);
        g.fillEllipse(dialArea);

        // draw small circle
        juce::Rectangle<float> smallDialArea(rx + radiusInner / 10.0f * 3, ry + radiusInner / 10.0f * 3, diameterInner / 10.0f * 7, diameterInner / 10.0f * 7);

        juce::Colour innerColor = KNOB_INNER_COLOUR;

        if (slider.isMouseOverOrDragging() && slider.isEnabled())
        {
            innerColor = innerColor.brighter();
        }

        g.setColour(innerColor);
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

            g.setColour(juce::Colour(255, juce::jmax(255 - sampleMaxValue * 2000, 0.0f), 0));
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
        drawModulationVisuals(g, x, y, width, height, rotaryStartAngle, rotaryEndAngle, slider);
    }

    enum class FilterType
    {
        LowCut,
        HighCut,
        BandPass
    };
    void drawFilterPath(juce::Graphics& g, const juce::Rectangle<float>& bounds, FilterType type, const juce::Colour& colour)
    {
        float w = bounds.getWidth();
        float h = bounds.getHeight();
        float cornerSize = 10.0f * scale;
        juce::Path p, p2;

        // 1. Draw the main filter curve (the line itself)
        switch (type)
        {
            case FilterType::LowCut:
                p.startNewSubPath(w * 0.25f, h);
                p.lineTo(w / 2.0f, h / 2.0f);
                p.lineTo(w - 1.0f, h / 2.0f);
                break;
            case FilterType::HighCut:
                p.startNewSubPath(1.0f, h / 2.0f);
                p.lineTo(w / 2.0f, h / 2.0f);
                p.lineTo(w * 0.75f, h);
                break;
            case FilterType::BandPass:
                p.startNewSubPath(w * 0.25f, h);
                p.lineTo(w / 2.0f, h * 0.4f);
                p.lineTo(w * 0.75f, h);
                break;
        }

        juce::Path roundedPath = p.createPathWithRoundedCorners(5.0f * scale);
        g.strokePath(roundedPath, juce::PathStrokeType(1.0f));

        // 2. Fill the area enclosed by the filter curve and the button edges
        g.setColour(colour.withAlpha(0.5f));

        // Close the stroked path to fill the "upper" part
        roundedPath.closeSubPath();
        g.fillPath(roundedPath);

        // Create a second path to fill the area "below" the curve
        auto csx = juce::jmin(cornerSize, w * 0.5f);
        auto csy = juce::jmin(cornerSize, h * 0.5f);
        auto cs45x = csx * 0.45f;
        auto cs45y = csy * 0.45f;

        switch (type)
        {
            case FilterType::LowCut:
                p2.startNewSubPath(w * 0.25f, h);
                p2.lineTo(w - 1.0f, h / 2.0f);
                p2.lineTo(w, h - csy);
                p2.cubicTo(w, h - cs45y, w - cs45x, h, w - csx, h);
                p2.closeSubPath();
                break;
            case FilterType::HighCut:
                p2.startNewSubPath(1.0f, h / 2.0f);
                p2.lineTo(w * 0.75f, h);
                p2.lineTo(csx, h);
                p2.cubicTo(cs45x, h, 0, h - cs45y, 0, h - csy);
                p2.closeSubPath();
                break;
            case FilterType::BandPass:
                // For BandPass, the shape is already enclosed, so we just fill the main path.
                // No need for p2.
                break;
        }

        if (! p2.isEmpty())
        {
            g.fillPath(p2);
        }
    }
};
