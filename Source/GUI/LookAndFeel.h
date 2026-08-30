/*
  ==============================================================================

    LookAndFeel.h
    Fire's CPU-efficient Obsidian Reactor look and feel.

  ==============================================================================
*/

#pragma once

#include "FireTheme.h"
#include "InterfaceDefines.h"
#include "ModulatableSlider.h"
#include "PrimaryButton.h"
#include "PrimarySlider.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

class FireLookAndFeel final : public juce::LookAndFeel_V4,
                              private juce::Timer
{
public:
    float scale = 1.0f;
    float reductionPercent = 1.0f;
    float sampleMaxValue = 0.0f;

    FireLookAndFeel()
    {
        using namespace fire::ui;

        setColour(juce::Slider::textBoxTextColourId, colours::textPrimary);
        setColour(juce::Slider::textBoxBackgroundColourId, colours::surface0.withAlpha(0.94f));
        setColour(juce::Slider::textBoxOutlineColourId, colours::hairline);
        setColour(juce::Slider::textBoxHighlightColourId, colours::ember.withAlpha(0.35f));
        setColour(juce::Slider::trackColourId, colours::ember);
        setColour(juce::Slider::thumbColourId, colours::whiteHot);
        setColour(juce::Slider::backgroundColourId, colours::surface2);

        setColour(juce::Label::textColourId, colours::textSecondary);
        setColour(juce::Label::textWhenEditingColourId, colours::textPrimary);
        setColour(juce::Label::backgroundWhenEditingColourId, colours::surface0);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::outlineWhenEditingColourId, colours::ember.withAlpha(0.65f));

        setColour(juce::ComboBox::backgroundColourId, colours::surface1);
        setColour(juce::ComboBox::outlineColourId, colours::hairline);
        setColour(juce::ComboBox::focusedOutlineColourId, colours::ember);
        setColour(juce::ComboBox::textColourId, colours::textPrimary);
        setColour(juce::ComboBox::arrowColourId, colours::flame);

        // A non-opaque menu peer lets the rounded Fire surface keep genuinely
        // transparent corners instead of JUCE's opaque white fallback fill.
        setColour(juce::PopupMenu::backgroundColourId,
                  colours::surface0.withAlpha(0.98f));
        setColour(juce::PopupMenu::textColourId, colours::textSecondary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::raised);
        setColour(juce::PopupMenu::highlightedTextColourId, colours::whiteHot);
        setColour(juce::PopupMenu::headerTextColourId, colours::flame);

        setColour(juce::TooltipWindow::backgroundColourId, colours::surface1);
        setColour(juce::TooltipWindow::textColourId, colours::textPrimary);
        setColour(juce::TooltipWindow::outlineColourId, colours::flame.withAlpha(0.62f));

        setColour(juce::TextEditor::backgroundColourId, colours::surface0);
        setColour(juce::TextEditor::textColourId, colours::textPrimary);
        setColour(juce::TextEditor::outlineColourId, colours::hairline);
        setColour(juce::TextEditor::focusedOutlineColourId, colours::ember);

        setColour(juce::ToggleButton::textColourId, colours::textSecondary);
        setColour(juce::ToggleButton::tickColourId, colours::flame);
        setColour(juce::ToggleButton::tickDisabledColourId, colours::disabled);
    }

    juce::Font getBaseFont(float height = KNOB_FONT_SIZE) const
    {
        return fire::ui::bodyFont(height * scale);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return fire::ui::labelFont(juce::jlimit(9.0f, 15.0f * scale,
                                               static_cast<float>(buttonHeight) * 0.38f));
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return fire::ui::bodyFont(juce::jlimit(10.0f, 14.0f * scale,
                                              static_cast<float>(box.getHeight()) * 0.42f));
    }

    juce::Font getPopupMenuFont() override { return fire::ui::bodyFont(13.0f * scale); }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (label.getFont().getHeight() > 0.0f)
            return label.getFont();
        return fire::ui::bodyFont(13.0f * scale);
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int,
                      int,
                      int,
                      int,
                      juce::ComboBox& box) override
    {
        using namespace fire::ui;
        auto& animation = getComboBoxAnimation(box, isButtonDown);
        const auto hover = animation.hover.current;
        const auto press = animation.press.current;
        const auto focus = animation.focus.current;
        const auto disabled = animation.disabled.current;
        auto bounds = juce::Rectangle<float>(0.5f, 0.5f,
                                              static_cast<float>(width) - 1.0f,
                                              static_cast<float>(height) - 1.0f);
        const auto isHeaderPreset = box.getComponentID() == "header_preset";

        if (isHeaderPreset)
        {
            // The preset selector lives in a visually dense toolbar.  A flat
            // hover/focus wash keeps it discoverable without putting another
            // framed card inside the header.
            const auto washAmount = juce::jmax(focus, hover * 0.64f) * (1.0f - disabled);
            if (washAmount > 0.001f)
            {
                auto wash = colours::surface2.interpolatedWith(colours::raised, focus)
                                .darker(0.10f * press);
                g.setColour(wash
                                .withAlpha(0.72f * washAmount));
                g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
            }

            const auto emphasis = juce::jmax(focus, hover * 0.55f);
            g.setColour(colours::hairline.interpolatedWith(colours::flame, emphasis)
                            .withAlpha((0.42f + 0.30f * emphasis) * (1.0f - 0.65f * disabled)));
            const auto lineWidth = bounds.getWidth() * (0.18f + 0.14f * emphasis);
            g.fillRoundedRectangle(bounds.getCentreX() - lineWidth * 0.5f,
                                   bounds.getBottom() - 1.0f,
                                   lineWidth,
                                   1.0f,
                                   0.5f);
        }
        else
        {
            const auto radius = juce::jmin(bounds.getHeight() * 0.5f, Metrics::radius);
            auto base = colours::surface1.interpolatedWith(colours::raised, focus * 0.72f);
            base = base.brighter(0.06f * hover).darker(0.10f * press);
            base = base.interpolatedWith(colours::surface0, disabled * 0.48f);
            juce::ColourGradient fill(base.brighter(0.05f), bounds.getX(), bounds.getY(),
                                      base.darker(0.12f), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(fill);
            g.fillRoundedRectangle(bounds, radius);

            const auto emphasis = juce::jmax(focus, hover * 0.42f);
            auto edge = box.findColour(juce::ComboBox::outlineColourId)
                            .interpolatedWith(box.findColour(juce::ComboBox::focusedOutlineColourId),
                                              emphasis);
            g.setColour(edge.withMultipliedAlpha(1.0f - 0.68f * disabled));
            g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f + 0.35f * focus);

            if (focus > 0.001f)
            {
                g.setColour(box.findColour(juce::ComboBox::focusedOutlineColourId)
                                .withAlpha(0.92f * focus * (1.0f - disabled)));
                g.fillRoundedRectangle(bounds.getX() + 1.0f,
                                       bounds.getY() + bounds.getHeight() * 0.24f,
                                       2.0f,
                                       bounds.getHeight() * 0.52f,
                                       1.0f);
            }
        }

        auto arrowArea = bounds.removeFromRight(juce::jmax(18.0f * scale, bounds.getHeight() * 0.82f));
        const auto centre = arrowArea.getCentre();
        const auto halfWidth = juce::jmin(4.0f * scale, arrowArea.getWidth() * 0.18f);
        juce::Path arrow;
        arrow.startNewSubPath(centre.x - halfWidth, centre.y - halfWidth * 0.35f);
        arrow.lineTo(centre.x, centre.y + halfWidth * 0.55f);
        arrow.lineTo(centre.x + halfWidth, centre.y - halfWidth * 0.35f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId)
                        .withMultipliedAlpha(0.9f - 0.6f * disabled));
        g.strokePath(arrow, juce::PathStrokeType(1.5f * scale,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        using namespace fire::ui;
        auto bounds = juce::Rectangle<float>(0.5f, 0.5f,
                                              static_cast<float>(width) - 1.0f,
                                              static_cast<float>(height) - 1.0f);
        juce::ColourGradient fill(colours::surface2, 0.0f, 0.0f,
                                  colours::surface0, 0.0f, static_cast<float>(height), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, Metrics::radius);
        g.setColour(colours::hairline.withAlpha(0.9f));
        g.drawRoundedRectangle(bounds, Metrics::radius, 1.0f);
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override
    {
        using namespace fire::ui;
        if (isSeparator)
        {
            g.setColour(colours::hairline.withAlpha(0.75f));
            g.fillRect(area.reduced(9, 0).withHeight(1).withCentre(area.getCentre()));
            return;
        }

        auto row = area.reduced(4, 2);
        if (isHighlighted && isActive)
        {
            drawGlassPill(g, row.toFloat(), colours::ember, true, true, false);
            g.setColour(colours::whiteHot);
        }
        else
        {
            g.setColour((textColourToUse != nullptr ? *textColourToUse : colours::textSecondary)
                            .withMultipliedAlpha(isActive ? 1.0f : 0.38f));
        }

        auto content = row.reduced(8, 0);
        auto iconArea = content.removeFromLeft(18).toFloat();
        if (icon != nullptr)
            icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
        else if (isTicked)
        {
            g.setColour(colours::flame);
            g.fillEllipse(iconArea.withSizeKeepingCentre(5.0f * scale, 5.0f * scale));
        }

        if (hasSubMenu)
        {
            auto arrowArea = content.removeFromRight(14).toFloat();
            juce::Path arrow;
            arrow.startNewSubPath(arrowArea.getX(), arrowArea.getY() + arrowArea.getHeight() * 0.28f);
            arrow.lineTo(arrowArea.getCentreX(), arrowArea.getCentreY());
            arrow.lineTo(arrowArea.getX(), arrowArea.getBottom() - arrowArea.getHeight() * 0.28f);
            g.strokePath(arrow, juce::PathStrokeType(1.4f * scale));
        }

        g.setFont(getPopupMenuFont());
        g.drawFittedText(text, content, juce::Justification::centredLeft, 1);
        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour(colours::textMuted);
            g.drawText(shortcutKeyText, content, juce::Justification::centredRight);
        }
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        using namespace fire::ui;
        auto bounds = label.getLocalBounds().toFloat();
        const auto background = label.findColour(juce::Label::backgroundColourId);
        if (! background.isTransparent())
        {
            g.setColour(background);
            g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
        }

        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        const auto enabledAlpha = label.isEnabled() ? 1.0f : 0.38f;
        g.setColour(label.findColour(label.isBeingEdited() ? juce::Label::textWhenEditingColourId
                                                          : juce::Label::textColourId)
                        .withMultipliedAlpha(enabledAlpha));
        g.setFont(getLabelFont(label));
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, juce::roundToInt(textArea.getHeight() / juce::jmax(1.0f, getLabelFont(label).getHeight()))),
                         label.getMinimumHorizontalScale());

        const auto outline = label.findColour(label.isBeingEdited() ? juce::Label::outlineWhenEditingColourId
                                                                    : juce::Label::outlineColourId);
        if (! outline.isTransparent())
        {
            g.setColour(outline.withMultipliedAlpha(enabledAlpha));
            g.drawRoundedRectangle(bounds.reduced(0.5f), Metrics::radiusSmall * scale, 1.0f);
        }
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        if (dynamic_cast<ModulatableSlider*>(&slider) == nullptr)
            return juce::LookAndFeel_V4::getSliderLayout(slider);

        juce::Slider::SliderLayout layout;
        auto bounds = slider.getLocalBounds();
        const int headerHeight = juce::jmin(bounds.getHeight() / 3,
                                            juce::roundToInt(TEXTBOX_HEIGHT * scale));
        auto header = bounds.removeFromTop(headerHeight);
        if (slider.getTextBoxPosition() != juce::Slider::NoTextBox)
        {
            const auto textWidth = juce::jmin(juce::roundToInt(TEXTBOX_WIDTH * scale), header.getWidth());
            layout.textBoxBounds = header.withSizeKeepingCentre(textWidth, headerHeight);
        }
        layout.sliderBounds = bounds;
        return layout;
    }

    juce::Button* createSliderButton(juce::Slider& slider, bool isIncrement) override
    {
        auto* button = new PrimaryTextButton();
        button->setComponentID(isIncrement ? "slider_up_arrow" : "slider_down_arrow");
        button->setColour(juce::TextButton::buttonColourId,
                          slider.findColour(juce::Slider::textBoxBackgroundColourId));
        button->setColour(juce::TextButton::textColourOnId,
                          slider.findColour(juce::Slider::thumbColourId));
        button->setColour(juce::TextButton::textColourOffId,
                          slider.findColour(juce::Slider::thumbColourId));
        return button;
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                              static_cast<float>(width), static_cast<float>(height));
        drawDial(g, bounds, sliderPos, rotaryStartAngle, rotaryEndAngle, slider,
                 slider.getComponentID() == "drive");

        if (auto* modSlider = dynamic_cast<ModulatableSlider*>(&slider))
            drawModulation(g, bounds, rotaryStartAngle, rotaryEndAngle, *modSlider);
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        using namespace fire::ui;
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                                   minSliderPos, maxSliderPos, style, slider);
            return;
        }

        auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(width), static_cast<float>(height));
        const auto animation = getSliderAnimation(slider);
        const auto centreY = area.getCentreY();
        const auto track = area.reduced(6.0f * scale, area.getHeight() * 0.42f);
        g.setColour(colours::surface2.interpolatedWith(colours::raised,
                                                       animation.focus * 0.52f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
        g.setColour(colours::hairline.interpolatedWith(colours::ember,
                                                       juce::jmax(animation.focus,
                                                                  animation.hover * 0.36f))
                        .withMultipliedAlpha(1.0f - 0.66f * animation.disabled));
        g.drawRoundedRectangle(track, track.getHeight() * 0.5f,
                               1.0f + animation.focus * 0.35f);

        const auto normalisedZero = slider.valueToProportionOfLength(0.0);
        const auto zeroX = area.getX() + static_cast<float>(normalisedZero) * area.getWidth();
        const auto startX = slider.getMinimum() < 0.0 ? zeroX : track.getX();
        auto valueBounds = juce::Rectangle<float>::leftTopRightBottom(juce::jmin(startX, sliderPos),
                                                                      track.getY(),
                                                                      juce::jmax(startX, sliderPos),
                                                                      track.getBottom());
        const auto valueColour = sliderPos < startX ? colours::signalCool : colours::ember;
        g.setColour(valueColour.withAlpha(0.9f - 0.6f * animation.disabled));
        g.fillRoundedRectangle(valueBounds, track.getHeight() * 0.5f);

        const auto thumbScale = 1.0f + animation.hover * 0.10f - animation.press * 0.08f;
        g.setColour(colours::whiteHot.withMultipliedAlpha(1.0f - 0.65f * animation.disabled));
        g.fillEllipse(juce::Rectangle<float>(8.0f * scale * thumbScale,
                                             8.0f * scale * thumbScale)
                          .withCentre({ sliderPos, centreY }));
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool highlighted,
                              bool down) override
    {
        using namespace fire::ui;
        const auto animation = getPrimaryButtonAnimation(button, highlighted, down);
        const auto hoverAmount = animation.hover;
        const auto pressAmount = animation.press;
        const auto focusAmount = animation.focus;
        const auto disabledAmount = animation.disabled;
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto id = button.getComponentID();
        const auto isHeaderControl = id.startsWith("header_") || id == "workspace_tab";
        const auto animatedSelection = static_cast<bool>(
            button.getProperties().getWithDefault("fireAnimatedSelection", false));
        const auto visuallySelected = button.getToggleState() && ! animatedSelection;

        // Transparent tab buttons keep their module colour in textColourOffId
        // while using white text for the selected state.  Using the current text
        // colour as the outline therefore turned every live module tab white.
        auto accent = button.findColour((visuallySelected && ! backgroundColour.isTransparent())
                                            ? juce::TextButton::textColourOnId
                                            : juce::TextButton::textColourOffId);
        if (accent.isTransparent() || accent == juce::Colours::black)
            accent = colours::flame;

        if (isHeaderControl)
        {
            const auto headerAccent = button.findColour(juce::TextButton::textColourOnId);
            if (! headerAccent.isTransparent() && headerAccent != juce::Colours::black)
                accent = headerAccent;

            // Header controls are intentionally borderless.  State is conveyed
            // by a quiet surface wash and a short accent rail instead of by a
            // stack of outlines around every control.
            if (disabledAmount < 1.0f)
            {
                if (visuallySelected)
                {
                    g.setColour(accent.withAlpha(id == "workspace_tab" ? 0.075f : 0.11f));
                    g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
                }
                else if (hoverAmount > 0.001f || pressAmount > 0.001f || focusAmount > 0.001f)
                {
                    auto wash = colours::surface2.interpolatedWith(colours::raised, pressAmount);
                    g.setColour(wash.withAlpha((0.56f * hoverAmount
                                                + 0.72f * pressAmount
                                                + 0.34f * focusAmount)
                                               * (1.0f - disabledAmount)));
                    g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
                }
            }

            if (visuallySelected)
            {
                const auto indicatorWidth = juce::jmin(bounds.getWidth() * 0.46f,
                                                       30.0f * scale);
                g.setColour(accent.withAlpha(0.92f));
                g.fillRoundedRectangle(bounds.getCentreX() - indicatorWidth * 0.5f,
                                       bounds.getBottom() - juce::jmax(1.0f, 1.5f * scale),
                                       indicatorWidth,
                                       juce::jmax(1.0f, 1.5f * scale),
                                       0.75f * scale);
            }

            if (disabledAmount > 0.001f)
            {
                g.setColour(colours::canvas.withAlpha(0.42f * disabledAmount));
                g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
            }

            if (id == "header_previous" || id == "header_next")
                drawArrowIcon(g, bounds, id, accent);
            else if (id == "header_menu")
                drawMenuIcon(g, bounds, accent);

            return;
        }

        auto base = backgroundColour;
        if (! base.isTransparent())
            base = base.brighter(0.06f * hoverAmount).darker(0.10f * pressAmount);

        const auto radius = juce::jmin(bounds.getHeight() * 0.5f, Metrics::radius);
        if (! base.isTransparent())
        {
            g.setColour(base);
            g.fillRoundedRectangle(bounds, radius);
        }
        else if (visuallySelected || hoverAmount > 0.001f || focusAmount > 0.001f)
        {
            // Preserve an explicitly transparent button background while still
            // giving selected/hovered tabs a restrained energy wash.
            const auto alpha = visuallySelected ? 0.11f
                                                 : 0.055f * hoverAmount + 0.045f * focusAmount;
            g.setColour(accent.withAlpha(alpha));
            g.fillRoundedRectangle(bounds, radius);
        }

        if (visuallySelected || hoverAmount > 0.001f || focusAmount > 0.001f)
        {
            auto outline = visuallySelected
                               ? accent.withAlpha(0.54f)
                               : colours::hairline.withAlpha(0.48f * juce::jmax(hoverAmount, focusAmount));
            g.setColour(outline);
            g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
        }

        if (disabledAmount > 0.001f)
        {
            g.setColour(colours::canvas.withAlpha(0.45f * disabledAmount));
            g.fillRoundedRectangle(bounds, juce::jmin(bounds.getHeight() * 0.5f, Metrics::radius));
        }

        if (id == "zoom")
            drawZoomIcon(g, bounds, accent);
        else if (id == "slider_up_arrow" || id == "slider_down_arrow"
                 || id == "left_arrow" || id == "right_arrow")
            drawArrowIcon(g, bounds, id, accent);
        else if (id == "low_cut" || id == "high_cut" || id == "band_pass")
            drawFilterIcon(g, bounds.reduced(5.0f * scale), id, accent);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool highlighted,
                        bool down) override
    {
        using namespace fire::ui;
        const auto animation = getPrimaryButtonAnimation(button, highlighted, down);
        highlighted = animation.hover > 0.001f;
        down = animation.press > 0.001f;
        const auto id = button.getComponentID();
        if (id == "zoom" || id == "slider_up_arrow" || id == "slider_down_arrow"
            || id == "left_arrow" || id == "right_arrow"
            || id == "header_previous" || id == "header_next" || id == "header_menu"
            || id == "low_cut" || id == "high_cut" || id == "band_pass")
            return;

        if (id == "remove_button")
        {
            auto area = button.getLocalBounds().toFloat().withSizeKeepingCentre(
                juce::jmin(button.getWidth(), button.getHeight()) * 0.28f,
                juce::jmin(button.getWidth(), button.getHeight()) * 0.28f);
            juce::Path cross;
            cross.startNewSubPath(area.getTopLeft());
            cross.lineTo(area.getBottomRight());
            cross.startNewSubPath(area.getTopRight());
            cross.lineTo(area.getBottomLeft());
            g.setColour(colours::danger.withAlpha(0.68f + 0.32f * animation.hover));
            g.strokePath(cross, juce::PathStrokeType(1.8f * scale,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
            return;
        }

        auto colour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                : juce::TextButton::textColourOffId);
        colour = colour.brighter(0.14f * animation.hover)
                       .darker(0.08f * animation.press)
                       .interpolatedWith(colours::textMuted.withAlpha(0.45f), animation.disabled);

        g.setColour(colour);
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(7, 1),
                         juce::Justification::centred, 1);
    }

    void drawTickBox(juce::Graphics& g,
                     juce::Component& component,
                     float x,
                     float y,
                     float w,
                     float h,
                     bool ticked,
                     bool isEnabled,
                     bool highlighted,
                     bool down) override
    {
        using namespace fire::ui;
        if (component.getComponentID() == "flat_toggle")
        {
            auto bounds = component.getLocalBounds().toFloat().reduced(1.0f);
            auto accent = component.findColour(juce::ToggleButton::tickColourId);
            drawGlassPill(g, bounds, accent, ticked, highlighted, down);
            return;
        }

        auto bounds = juce::Rectangle<float>(x, y, w, h);
        auto colour = ticked ? component.findColour(juce::ToggleButton::tickColourId)
                             : colours::textMuted;
        if (highlighted && isEnabled)
            colour = colour.brighter(0.18f);
        if (down && isEnabled)
            colour = colour.darker(0.14f);
        colour = colour.withMultipliedAlpha(isEnabled ? (ticked ? 0.95f : 0.55f) : 0.25f);

        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.31f;
        const auto centre = bounds.getCentre();
        juce::Path power;
        power.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                            juce::MathConstants<float>::pi * 0.22f,
                            juce::MathConstants<float>::pi * 1.78f, true);
        power.startNewSubPath(centre.x, centre.y - radius * 1.18f);
        power.lineTo(centre.x, centre.y - radius * 0.10f);
        g.setColour(colour);
        g.strokePath(power, juce::PathStrokeType(juce::jmax(1.2f, radius * 0.18f),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool highlighted,
                          bool down) override
    {
        using namespace fire::ui;
        const auto animation = getPrimaryButtonAnimation(button, highlighted, down);
        highlighted = animation.hover > 0.001f;
        down = animation.press > 0.001f;
        const auto text = button.getButtonText();

        if (animation.hover > 0.001f || animation.press > 0.001f
            || animation.focus > 0.001f)
        {
            auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
            auto accent = button.findColour(juce::ToggleButton::tickColourId);
            g.setColour(accent.withAlpha((0.035f * animation.hover
                                          + 0.065f * animation.press
                                          + 0.025f * animation.focus)
                                         * (1.0f - animation.disabled)));
            g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
            g.setColour(accent.withAlpha(0.42f * animation.focus
                                         * (1.0f - animation.disabled)));
            g.drawRoundedRectangle(bounds, Metrics::radiusSmall * scale, 1.0f);
        }

        if (button.getComponentID() == "flat_toggle")
        {
            drawTickBox(g, button, 0.0f, 0.0f,
                        static_cast<float>(button.getWidth()), static_cast<float>(button.getHeight()),
                        button.getToggleState(), button.isEnabled(), highlighted, down);
        }
        else
        {
            auto content = button.getLocalBounds().toFloat().reduced(1.0f);
            auto tickArea = content.withSizeKeepingCentre(content.getWidth() * 0.60f,
                                                          content.getHeight() * 0.60f);
            if (text.isNotEmpty())
            {
                const auto tickExtent = juce::jmin(content.getHeight(), 20.0f * scale);
                tickArea = content.removeFromLeft(tickExtent).withSizeKeepingCentre(tickExtent, tickExtent);
                content.removeFromLeft(juce::jmax(3.0f, 4.0f * scale));
            }

            drawTickBox(g, button,
                        tickArea.getX(), tickArea.getY(), tickArea.getWidth(), tickArea.getHeight(),
                        button.getToggleState(), button.isEnabled(), highlighted, down);
        }

        if (text.isNotEmpty())
        {
            auto textArea = button.getLocalBounds().reduced(2, 0);
            if (button.getComponentID() != "flat_toggle")
            {
                const auto tickExtent = juce::jmin(static_cast<float>(button.getHeight() - 2),
                                                   20.0f * scale);
                textArea.removeFromLeft(juce::roundToInt(tickExtent + juce::jmax(3.0f, 4.0f * scale)));
            }

            auto textColour = button.findColour(juce::ToggleButton::textColourId)
                                  .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.35f);
            textColour = textColour.brighter(0.12f * animation.hover)
                                   .interpolatedWith(colours::textMuted.withAlpha(0.35f),
                                                     animation.disabled);

            g.setColour(textColour);
            g.setFont(bodyFont(juce::jlimit(9.0f, juce::jmax(9.0f, 14.0f * scale),
                                            static_cast<float>(button.getHeight()) * 0.58f)));
            g.drawFittedText(text, textArea,
                             button.getComponentID() == "flat_toggle"
                                 ? juce::Justification::centred
                                 : juce::Justification::centredLeft,
                             1);
        }
    }

private:
    struct ComboBoxAnimation
    {
        juce::Component::SafePointer<juce::ComboBox> box;
        fire::ui::DampedValue hover;
        fire::ui::DampedValue press;
        fire::ui::DampedValue focus;
        fire::ui::DampedValue disabled;
        bool pressTarget = false;
    };

    ComboBoxAnimation& getComboBoxAnimation(juce::ComboBox& box,
                                             bool isButtonDown)
    {
        auto found = std::find_if(comboBoxAnimations.begin(), comboBoxAnimations.end(),
                                  [&box](const auto& state)
                                  {
                                      return state.box.getComponent() == &box;
                                  });
        if (found == comboBoxAnimations.end())
        {
            ComboBoxAnimation state;
            state.box = &box;
            const auto interactive = box.isEnabled() && box.isShowing();
            state.hover.snapTo(interactive && box.isMouseOver(true) ? 1.0f : 0.0f);
            state.press.snapTo(interactive && isButtonDown ? 1.0f : 0.0f);
            state.focus.snapTo(interactive
                                   && (box.hasKeyboardFocus(true) || box.isPopupActive())
                               ? 1.0f : 0.0f);
            state.disabled.snapTo(box.isEnabled() ? 0.0f : 1.0f);
            state.pressTarget = isButtonDown;
            comboBoxAnimations.push_back(std::move(state));
            return comboBoxAnimations.back();
        }

        found->pressTarget = isButtonDown;
        updateComboBoxTargets(*found);
        startComboBoxTimerIfNeeded();
        return *found;
    }

    static void updateComboBoxTargets(ComboBoxAnimation& state) noexcept
    {
        auto* box = state.box.getComponent();
        if (box == nullptr)
            return;

        const auto interactive = box->isEnabled() && box->isShowing();
        state.hover.setTarget(interactive && box->isMouseOver(true) ? 1.0f : 0.0f);
        state.press.setTarget(interactive && state.pressTarget ? 1.0f : 0.0f);
        state.focus.setTarget(interactive
                                  && (box->hasKeyboardFocus(true) || box->isPopupActive())
                              ? 1.0f : 0.0f);
        state.disabled.setTarget(box->isEnabled() ? 0.0f : 1.0f);
    }

    static bool isComboBoxAnimationSettled(const ComboBoxAnimation& state) noexcept
    {
        return state.hover.isSettled() && state.press.isSettled()
            && state.focus.isSettled() && state.disabled.isSettled();
    }

    void startComboBoxTimerIfNeeded()
    {
        if (! isTimerRunning()
            && std::any_of(comboBoxAnimations.begin(), comboBoxAnimations.end(),
                           [](const auto& state)
                           {
                               return state.box != nullptr
                                   && state.box->isShowing()
                                   && ! isComboBoxAnimationSettled(state);
                           }))
            startTimerHz(60);
    }

    void timerCallback() override
    {
        auto anyAnimating = false;
        for (auto iterator = comboBoxAnimations.begin(); iterator != comboBoxAnimations.end();)
        {
            auto* box = iterator->box.getComponent();
            if (box == nullptr || ! box->isShowing())
            {
                iterator = comboBoxAnimations.erase(iterator);
                continue;
            }

            updateComboBoxTargets(*iterator);
            auto changed = iterator->hover.advance(1.0f / 60.0f, 0.10f);
            changed = iterator->press.advance(1.0f / 60.0f, 0.065f) || changed;
            changed = iterator->focus.advance(1.0f / 60.0f, 0.11f) || changed;
            changed = iterator->disabled.advance(1.0f / 60.0f, 0.13f) || changed;
            if (changed)
                box->repaint();
            anyAnimating = anyAnimating || ! isComboBoxAnimationSettled(*iterator);
            ++iterator;
        }

        if (! anyAnimating)
            stopTimer();
    }

    std::vector<ComboBoxAnimation> comboBoxAnimations;

    struct ButtonAnimation
    {
        float hover = 0.0f;
        float press = 0.0f;
        float focus = 0.0f;
        float disabled = 0.0f;
    };

    struct SliderAnimation
    {
        float hover = 0.0f;
        float press = 0.0f;
        float focus = 0.0f;
        float disabled = 0.0f;
    };

    static SliderAnimation getSliderAnimation(const juce::Slider& slider) noexcept
    {
        // ModulatableSlider owns its existing main-body/handle animation.
        // Check it first so this refactor never layers PrimarySlider state on
        // top of its specialised interaction model.
        if (const auto* modSlider = dynamic_cast<const ModulatableSlider*>(&slider))
            return { modSlider->getHoverAnimation(), modSlider->getPressAnimation(),
                     slider.hasKeyboardFocus(true) ? 1.0f : 0.0f,
                     slider.isEnabled() ? 0.0f : 1.0f };

        if (const auto* primary = dynamic_cast<const PrimarySliderAnimationState*>(&slider))
            return { primary->getHoverAnimation(), primary->getPressAnimation(),
                     primary->getFocusAnimation(), primary->getDisabledAnimation() };

        return { slider.isMouseOverOrDragging() ? 1.0f : 0.0f,
                 0.0f,
                 slider.hasKeyboardFocus(true) ? 1.0f : 0.0f,
                 slider.isEnabled() ? 0.0f : 1.0f };
    }

    static ButtonAnimation getPrimaryButtonAnimation(const juce::Button& button,
                                                       bool highlighted,
                                                       bool down) noexcept
    {
        if (const auto* animated = dynamic_cast<const PrimaryButtonAnimationState*>(&button))
            return { animated->getHoverAnimation(), animated->getPressAnimation(),
                     animated->getFocusAnimation(), animated->getDisabledAnimation() };
        return { highlighted ? 1.0f : 0.0f, down ? 1.0f : 0.0f,
                 button.hasKeyboardFocus(true) ? 1.0f : 0.0f,
                 button.isEnabled() ? 0.0f : 1.0f };
    }

    void drawDial(juce::Graphics& g,
                  juce::Rectangle<float> bounds,
                  float sliderPos,
                  float startAngle,
                  float endAngle,
                  juce::Slider& slider,
                  bool isDrive)
    {
        using namespace fire::ui;
        const auto margin = juce::jmax(5.0f, 7.0f * scale);
        const auto animation = getSliderAnimation(slider);
        const auto hoverAmount = animation.hover;
        const auto pressAmount = animation.press;
        const auto focusAmount = animation.focus;
        const auto disabledAmount = animation.disabled;
        bounds = bounds.reduced(margin + pressAmount * 0.8f * scale);
        const auto radius = juce::jmax(2.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f);
        bounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(bounds.getCentre());
        const auto centre = bounds.getCentre();
        const auto stroke = dialArcStroke(radius, scale, isDrive);
        const auto trackRadius = juce::jmax(
            2.0f,
            radius - (stroke + 2.0f * scale) * 0.5f - 1.0f * scale);
        const auto arcState = calculateDialArcState(sliderPos, reductionPercent, isDrive);
        const auto valueAngle = startAngle
                                + arcState.requestedProportion * (endAngle - startAngle);
        auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (accent.isTransparent())
            accent = colours::flame;

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                            startAngle, endAngle, true);
        g.setColour(colours::canvas.withAlpha(0.85f));
        g.strokePath(track, juce::PathStrokeType(stroke + 2.0f * scale,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(colours::hairline.interpolatedWith(accent, focusAmount * 0.52f)
                        .withAlpha(0.9f - 0.55f * disabledAmount));
        g.strokePath(track, juce::PathStrokeType(stroke,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        if ((hoverAmount > 0.01f || focusAmount > 0.01f)
            && disabledAmount < 0.999f)
        {
            g.setColour(accent.withAlpha((0.06f + hoverAmount * 0.08f
                                          + focusAmount * 0.06f)
                                         * (1.0f - disabledAmount)));
            g.strokePath(track, juce::PathStrokeType(stroke + 4.0f * scale,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        if (arcState.requestedProportion > 0.0001f)
        {
            const auto safeEnd = startAngle
                                 + arcState.effectiveProportion * (endAngle - startAngle);
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                   startAngle, safeEnd, true);
            juce::ColourGradient heat(accent, bounds.getX(), bounds.getBottom(),
                                      colours::gold, bounds.getRight(), bounds.getY(), false);
            heat.addColour(0.55, colours::flame);
            g.setGradientFill(heat);
            g.strokePath(valueArc, juce::PathStrokeType(stroke,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

            if (isDrive && arcState.hasReduction())
            {
                juce::Path reducedArc;
                reducedArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                         safeEnd, valueAngle, true);
                juce::ColourGradient reducedHeat(accent.withMultipliedAlpha(0.32f),
                                                  bounds.getX(), bounds.getBottom(),
                                                  colours::gold.withAlpha(0.32f),
                                                  bounds.getRight(), bounds.getY(), false);
                reducedHeat.addColour(0.55, colours::flame.withAlpha(0.32f));
                g.setGradientFill(reducedHeat);
                g.strokePath(reducedArc, juce::PathStrokeType(stroke,
                                                             juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
            }
        }

        auto disc = bounds.reduced(radius * 0.27f);
        juce::ColourGradient metal(colours::raised.brighter(hoverAmount * 0.09f),
                                   centre.x, disc.getY(), colours::surface0,
                                   centre.x, disc.getBottom(), false);
        metal.addColour(0.42, colours::surface2);
        g.setGradientFill(metal);
        g.fillEllipse(disc);
        g.setColour(colours::textPrimary.withAlpha(0.08f));
        g.drawEllipse(disc.reduced(0.5f), 1.0f);

        const auto indicatorLength = disc.getHeight() * 0.31f;
        const auto indicatorStart = disc.getHeight() * 0.08f;
        juce::Path indicator;
        indicator.startNewSubPath(0.0f, -indicatorStart);
        indicator.lineTo(0.0f, -indicatorLength);
        auto tickColour = isDrive && sampleMaxValue > 0.0001f
                              ? colours::whiteHot.interpolatedWith(colours::danger,
                                                                   juce::jlimit(0.0f, 1.0f, sampleMaxValue * 2.0f))
                              : colours::whiteHot;
        g.setColour(tickColour.withMultipliedAlpha(slider.isEnabled() ? 0.96f : 0.3f));
        g.strokePath(indicator,
                     juce::PathStrokeType(juce::jmax(1.2f, 1.8f * scale),
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded),
                     juce::AffineTransform::rotation(valueAngle).translated(centre.x, centre.y));
    }

    void drawModulation(juce::Graphics& g,
                        juce::Rectangle<float> bounds,
                        float startAngle,
                        float endAngle,
                        ModulatableSlider& slider)
    {
        using namespace fire::ui;
        const auto margin = juce::jmax(5.0f, 7.0f * scale);
        bounds = bounds.reduced(margin);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();

        if (slider.assignModeGlowAlpha > 0.0f && slider.isEnabled())
        {
            g.setColour(colours::flame.withAlpha(juce::jlimit(0.0f, 0.8f, slider.assignModeGlowAlpha)));
            g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre).reduced(1.0f),
                          juce::jmax(1.0f, 1.5f * scale));
        }

        if (slider.isModulated && std::abs(slider.lfoAmount) > 0.0001 && slider.isEnabled())
        {
            const auto base = slider.valueToProportionOfLength(slider.getValue());
            const auto depth = std::abs(slider.lfoAmount);
            auto low = slider.isBipolar ? base - depth * 0.5 : base + juce::jmin(0.0, slider.lfoAmount);
            auto high = slider.isBipolar ? base + depth * 0.5 : base + juce::jmax(0.0, slider.lfoAmount);
            low = juce::jlimit(0.0, 1.0, low);
            high = juce::jlimit(0.0, 1.0, high);
            const auto modRadius = juce::jmax(2.0f, radius - 8.0f * scale);
            juce::Path range;
            range.addCentredArc(centre.x, centre.y, modRadius, modRadius, 0.0f,
                                startAngle + static_cast<float>(low) * (endAngle - startAngle),
                                startAngle + static_cast<float>(high) * (endAngle - startAngle), true);
            g.setColour((slider.isBypassed ? colours::disabled : colours::modulation)
                            .withAlpha(slider.isBypassed ? 0.36f : 0.82f));
            g.strokePath(range, juce::PathStrokeType(juce::jmax(1.0f, 1.7f * scale),
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

            if (! slider.isBypassed)
            {
                const auto signal = slider.isBipolar ? slider.lfoValue * 0.5 : slider.lfoValue;
                const auto current = juce::jlimit(0.0, 1.0, base + signal * slider.lfoAmount);
                const auto angle = startAngle + static_cast<float>(current) * (endAngle - startAngle);
                const juce::Point<float> point {
                    centre.x + modRadius * std::sin(angle),
                    centre.y - modRadius * std::cos(angle)
                };
                g.setColour(colours::whiteHot);
                g.fillEllipse(juce::Rectangle<float>(3.8f * scale, 3.8f * scale).withCentre(point));
            }
        }

        if (slider.isModulated)
        {
            const auto hover = juce::jlimit(
                0.0f, 1.0f, slider.getModulationHandleHoverAnimation());
            const auto press = juce::jlimit(
                0.0f, 1.0f, slider.getModulationHandlePressAnimation());
            auto handle = slider.getModulationHandleBounds()
                              .reduced(press * 0.65f * scale);
            const auto accent = slider.isBypassed ? colours::disabled : colours::modulation;

            if (slider.isEnabled() && (hover > 0.001f || press > 0.001f))
            {
                const auto haloExpansion = (1.5f + hover * 2.5f
                                             + press * 1.0f) * scale;
                g.setColour(accent.withAlpha(0.10f + hover * 0.20f
                                              + press * 0.12f));
                g.fillEllipse(handle.expanded(haloExpansion));
            }

            g.setColour(colours::canvas.withAlpha(0.92f));
            g.fillEllipse(handle.expanded((1.0f + hover * 0.65f) * scale));
            g.setColour(accent.brighter(hover * 0.12f)
                            .interpolatedWith(colours::whiteHot,
                                              press * 0.16f)
                            .withAlpha(slider.isEnabled()
                                           ? 0.90f + hover * 0.08f
                                           : 0.35f));
            g.fillEllipse(handle);
            if (slider.isEnabled() && hover > 0.001f)
            {
                g.setColour(colours::whiteHot.withAlpha(hover * 0.42f));
                g.drawEllipse(handle.reduced(0.4f * scale),
                              juce::jmax(0.8f, 1.0f * scale));
            }
            g.setColour(colours::textPrimary.withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.4f));
            g.setFont(fire::ui::labelFont(juce::jmax(7.0f, handle.getHeight() * 0.48f)));
            g.drawText(juce::String(slider.lfoSource), handle, juce::Justification::centred);
        }
    }

    void drawArrowIcon(juce::Graphics& g,
                       juce::Rectangle<float> bounds,
                       const juce::String& id,
                       juce::Colour colour) const
    {
        const auto extent = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.34f;
        const auto stemThickness = juce::jmax(1.0f, 1.35f * scale);
        auto area = juce::Rectangle<float>(extent, extent).withCentre(bounds.getCentre());
        juce::Path arrow;
        if (id == "left_arrow" || id == "header_previous")
        {
            arrow.addRectangle(area.getX() + area.getWidth() * 0.34f,
                               area.getCentreY() - stemThickness * 0.5f,
                               area.getWidth() * 0.66f,
                               stemThickness);
            arrow.startNewSubPath(area.getX(), area.getCentreY());
            arrow.lineTo(area.getX() + area.getWidth() * 0.48f, area.getY());
            arrow.lineTo(area.getX() + area.getWidth() * 0.48f, area.getBottom());
            arrow.closeSubPath();
        }
        else if (id == "right_arrow" || id == "header_next")
        {
            arrow.addRectangle(area.getX(),
                               area.getCentreY() - stemThickness * 0.5f,
                               area.getWidth() * 0.66f,
                               stemThickness);
            arrow.startNewSubPath(area.getRight(), area.getCentreY());
            arrow.lineTo(area.getRight() - area.getWidth() * 0.48f, area.getY());
            arrow.lineTo(area.getRight() - area.getWidth() * 0.48f, area.getBottom());
            arrow.closeSubPath();
        }
        else if (id == "slider_up_arrow")
        {
            arrow.addRectangle(area.getCentreX() - stemThickness * 0.5f,
                               area.getY() + area.getHeight() * 0.34f,
                               stemThickness,
                               area.getHeight() * 0.66f);
            arrow.startNewSubPath(area.getCentreX(), area.getY());
            arrow.lineTo(area.getRight(), area.getY() + area.getHeight() * 0.48f);
            arrow.lineTo(area.getX(), area.getY() + area.getHeight() * 0.48f);
            arrow.closeSubPath();
        }
        else
        {
            arrow.addRectangle(area.getCentreX() - stemThickness * 0.5f,
                               area.getY(),
                               stemThickness,
                               area.getHeight() * 0.66f);
            arrow.startNewSubPath(area.getCentreX(), area.getBottom());
            arrow.lineTo(area.getRight(), area.getBottom() - area.getHeight() * 0.48f);
            arrow.lineTo(area.getX(), area.getBottom() - area.getHeight() * 0.48f);
            arrow.closeSubPath();
        }
        g.setColour(colour);
        g.fillPath(arrow);
    }

    void drawMenuIcon(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      juce::Colour colour) const
    {
        const auto extent = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.34f;
        auto area = juce::Rectangle<float>(extent, extent * 0.72f)
                        .withCentre(bounds.getCentre());
        juce::Path menu;
        for (int row = 0; row < 3; ++row)
        {
            const auto y = area.getY() + area.getHeight() * static_cast<float>(row) * 0.5f;
            menu.startNewSubPath(area.getX(), y);
            menu.lineTo(area.getRight(), y);
        }
        g.setColour(colour);
        g.strokePath(menu, juce::PathStrokeType(1.45f * scale,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    void drawZoomIcon(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      juce::Colour colour) const
    {
        auto area = bounds.reduced(bounds.getWidth() * 0.30f, bounds.getHeight() * 0.30f);
        juce::Path icon;
        icon.startNewSubPath(area.getX(), area.getCentreY());
        icon.lineTo(area.getX(), area.getY());
        icon.lineTo(area.getCentreX(), area.getY());
        icon.startNewSubPath(area.getCentreX(), area.getBottom());
        icon.lineTo(area.getRight(), area.getBottom());
        icon.lineTo(area.getRight(), area.getCentreY());
        g.setColour(colour);
        g.strokePath(icon, juce::PathStrokeType(1.5f * scale,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    void drawFilterIcon(juce::Graphics& g,
                        juce::Rectangle<float> bounds,
                        const juce::String& id,
                        juce::Colour colour) const
    {
        auto area = bounds.reduced(bounds.getWidth() * 0.12f, bounds.getHeight() * 0.22f);
        juce::Path curve;
        if (id == "low_cut")
        {
            curve.startNewSubPath(area.getX(), area.getBottom());
            curve.cubicTo(area.getX() + area.getWidth() * 0.18f, area.getBottom(),
                          area.getX() + area.getWidth() * 0.34f, area.getY(),
                          area.getX() + area.getWidth() * 0.55f, area.getY());
            curve.lineTo(area.getRight(), area.getY());
        }
        else if (id == "high_cut")
        {
            curve.startNewSubPath(area.getX(), area.getY());
            curve.lineTo(area.getX() + area.getWidth() * 0.45f, area.getY());
            curve.cubicTo(area.getX() + area.getWidth() * 0.66f, area.getY(),
                          area.getX() + area.getWidth() * 0.82f, area.getBottom(),
                          area.getRight(), area.getBottom());
        }
        else
        {
            curve.startNewSubPath(area.getX(), area.getBottom());
            curve.cubicTo(area.getX() + area.getWidth() * 0.26f, area.getBottom(),
                          area.getX() + area.getWidth() * 0.28f, area.getY(),
                          area.getCentreX(), area.getY());
            curve.cubicTo(area.getX() + area.getWidth() * 0.72f, area.getY(),
                          area.getX() + area.getWidth() * 0.74f, area.getBottom(),
                          area.getRight(), area.getBottom());
        }
        g.setColour(colour.withAlpha(0.92f));
        g.strokePath(curve, juce::PathStrokeType(1.8f * scale,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
};
