/*
  ==============================================================================

    LookAndFeel.h
    Fire's CPU-efficient Obsidian Reactor look and feel.

  ==============================================================================
*/

#pragma once

#include "FireTheme.h"
#include "FocusAwareComboBox.h"
#include "InterfaceDefines.h"
#include "ModulatableSlider.h"
#include "PrimaryButton.h"
#include "PrimarySlider.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace fire::ui
{
inline float headerInteractionWashAlpha(float hover,
                                        float press,
                                        float focus,
                                        float disabled) noexcept
{
    hover = juce::jlimit(0.0f, 1.0f, hover);
    press = juce::jlimit(0.0f, 1.0f, press);
    focus = juce::jlimit(0.0f, 1.0f, focus);
    disabled = juce::jlimit(0.0f, 1.0f, disabled);
    return juce::jlimit(0.0f,
                        1.0f,
                        (0.56f * hover + 0.72f * press + 0.34f * focus)
                            * (1.0f - disabled));
}

inline juce::TextLayout createTooltipTextLayout(const juce::String& text,
                                                float scale,
                                                juce::Colour colour)
{
    scale = std::isfinite(scale) ? juce::jlimit(0.75f, 2.5f, scale) : 1.0f;

    juce::AttributedString attributedText;
    attributedText.setJustification(juce::Justification::centred);
    attributedText.setWordWrap(juce::AttributedString::byWord);
    attributedText.append(text, bodyFont(12.5f * scale), colour);

    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(attributedText, 340.0f * scale);
    return layout;
}
} // namespace fire::ui

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
        setColour(juce::TooltipWindow::outlineColourId,
                  colours::hairline.withAlpha(0.78f));

        setColour(juce::TextEditor::backgroundColourId, colours::surface0);
        setColour(juce::TextEditor::textColourId, colours::textPrimary);
        setColour(juce::TextEditor::outlineColourId, colours::hairline);
        setColour(juce::TextEditor::focusedOutlineColourId, colours::ember);

        setColour(juce::ToggleButton::textColourId, colours::textSecondary);
        setColour(juce::ToggleButton::tickColourId, colours::flame);
        setColour(juce::ToggleButton::tickDisabledColourId, colours::disabled);
    }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    size_t getTrackedComboBoxAnimationCountForTesting() const noexcept
    {
        return comboBoxAnimations.size();
    }
#endif

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
        if (box.getComponentID() == "header_preset")
            return fire::ui::bodyFont(13.0f * scale);
        return fire::ui::bodyFont(juce::jlimit(10.0f, 14.0f * scale,
                                              static_cast<float>(box.getHeight()) * 0.42f));
    }

    juce::Font getPopupMenuFont() override { return fire::ui::bodyFont(13.0f * scale); }

    int getPopupMenuBorderSize() override
    {
        // drawPopupMenuBackground already owns the rounded hairline. A
        // parented JUCE MenuWindow otherwise draws an additional square
        // resizable frame around that surface.
        return 0;
    }

    void preparePopupMenuWindow(juce::Component& menuWindow) override
    {
        auto* parent = menuWindow.getParentComponent();
        if (parent != nullptr && &parent->getLookAndFeel() == this)
        {
            // The explicit theme was needed while JUCE constructed the menu.
            // Once it is parented, inherit the same theme instead so a host
            // closing the editor cannot leave a live weak reference that
            // trips LookAndFeel's destruction assertion.
            menuWindow.setLookAndFeel(nullptr);
        }
    }

    juce::Rectangle<int> getTooltipBounds(const juce::String& text,
                                          juce::Point<int> screenPosition,
                                          juce::Rectangle<int> parentArea) override
    {
        const auto safeScale = std::isfinite(scale)
                                   ? juce::jlimit(0.75f, 2.5f, scale)
                                   : 1.0f;
        const auto layout = fire::ui::createTooltipTextLayout(
            text,
            safeScale,
            findColour(juce::TooltipWindow::textColourId));
        const auto horizontalPadding = 11.0f * safeScale;
        const auto verticalPadding = 6.5f * safeScale;
        const auto width = juce::roundToInt(std::ceil(
            layout.getWidth() + horizontalPadding * 2.0f));
        const auto height = juce::roundToInt(std::ceil(
            layout.getHeight() + verticalPadding * 2.0f));
        const auto horizontalGap = juce::roundToInt(16.0f * safeScale);
        const auto verticalGap = juce::roundToInt(8.0f * safeScale);

        return juce::Rectangle<int>(
                   screenPosition.x > parentArea.getCentreX()
                       ? screenPosition.x - width - horizontalGap
                       : screenPosition.x + horizontalGap,
                   screenPosition.y > parentArea.getCentreY()
                       ? screenPosition.y - height - verticalGap
                       : screenPosition.y + verticalGap,
                   width,
                   height)
            .constrainedWithin(parentArea);
    }

    void drawTooltip(juce::Graphics& g,
                     const juce::String& text,
                     int width,
                     int height) override
    {
        using namespace fire::ui;
        if (width <= 0 || height <= 0)
            return;

        const auto safeScale = std::isfinite(scale)
                                   ? juce::jlimit(0.75f, 2.5f, scale)
                                   : 1.0f;
        auto bounds = juce::Rectangle<float>(0.5f,
                                              0.5f,
                                              static_cast<float>(width) - 1.0f,
                                              static_cast<float>(height) - 1.0f);
        const auto radius = juce::jmin(Metrics::radiusSmall * safeScale + 1.0f,
                                       bounds.getHeight() * 0.5f);
        const auto background = findColour(
            juce::TooltipWindow::backgroundColourId);
        juce::ColourGradient fill(background.brighter(0.025f),
                                  bounds.getX(),
                                  bounds.getY(),
                                  background.darker(0.18f),
                                  bounds.getX(),
                                  bounds.getBottom(),
                                  false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(findColour(juce::TooltipWindow::outlineColourId));
        g.drawRoundedRectangle(bounds, radius, 1.0f);

        const auto layout = createTooltipTextLayout(
            text,
            safeScale,
            findColour(juce::TooltipWindow::textColourId));
        layout.draw(g,
                    juce::Rectangle<float>(0.0f,
                                            0.0f,
                                            static_cast<float>(width),
                                            static_cast<float>(height))
                        .reduced(11.0f * safeScale,
                                 6.5f * safeScale));
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr
            && label.getComponentID() != "parameter_title")
            return fire::ui::valueFont(12.0f * scale);
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
        auto base = box.findColour(juce::ComboBox::backgroundColourId)
                        .interpolatedWith(colours::raised, juce::jmax(focus * 0.85f, hover * 0.38f));
        base = base.darker(press * 0.10f).interpolatedWith(colours::surface0, disabled * 0.48f);
        g.setColour(base);
        g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);

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
        g.setColour(colours::surface1);
        g.fillRoundedRectangle(bounds, Metrics::radius);

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

        if (label.isBeingEdited()
            && ! label.findColour(juce::Label::outlineWhenEditingColourId).isTransparent())
        {
            // Editing keeps a clear focus cue through a solid surface, with
            // the TextEditor remaining the sole owner of the visible glyphs.
            g.setColour(colours::raised);
            g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
        }

        const auto enabledAlpha = label.isEnabled() ? 1.0f : 0.68f;
        // The TextEditor created by Label owns the text while editing. Drawing
        // the backing Label text as well leaves both glyph runs visible when
        // the editor has a transparent background (as the crossover frequency
        // editor does), which looks like a persistent text shadow/ghost.
        if (! label.isBeingEdited())
        {
            auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
            g.setColour(label.findColour(juce::Label::textColourId)
                            .withMultipliedAlpha(enabledAlpha));
            g.setFont(getLabelFont(label));
            g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                             juce::jmax(1, juce::roundToInt(textArea.getHeight() / juce::jmax(1.0f, getLabelFont(label).getHeight()))),
                             label.getMinimumHorizontalScale());
        }


    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        if (static_cast<bool>(slider.getProperties().getWithDefault("ottThreshold", false)))
        {
            juce::Slider::SliderLayout layout;
            const auto height = static_cast<float>(slider.getHeight());
            const auto top = juce::roundToInt(-slider.getMaximum() * height / 100.0);
            const auto bottom = juce::roundToInt(-slider.getMinimum() * height / 100.0);
            layout.sliderBounds = { 0, top, slider.getWidth(), juce::jmax(1, bottom - top) };
            return layout;
        }
        if (dynamic_cast<ModulatableSlider*>(&slider) == nullptr
            && ! static_cast<bool>(slider.getProperties().getWithDefault("fireOrdinaryKnob", false)))
            return juce::LookAndFeel_V4::getSliderLayout(slider);

        juce::Slider::SliderLayout layout;
        auto bounds = slider.getLocalBounds();
        const int headerHeight = juce::jmin(bounds.getHeight() / 4,
            juce::roundToInt(fire::ui::Metrics::knobTitleHeight * scale));
        bounds.removeFromTop(headerHeight);
        auto footer = bounds.removeFromBottom(juce::jmin(bounds.getHeight() / 3,
            juce::roundToInt(fire::ui::Metrics::knobValueHeight * scale)));
        if (slider.getTextBoxPosition() != juce::Slider::NoTextBox)
        {
            const auto textWidth = juce::jmin(juce::roundToInt(84.0f * scale), footer.getWidth());
            layout.textBoxBounds = footer.withSizeKeepingCentre(textWidth, footer.getHeight());
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
        const auto configuredTrackColour =
            slider.findColour(juce::Slider::trackColourId);
        const auto trackAccent = configuredTrackColour.isTransparent()
                                   ? colours::ember
                                   : configuredTrackColour;
        g.setColour(colours::surface2.interpolatedWith(colours::raised,
                                                       animation.focus * 0.52f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
        g.setColour(colours::hairline.interpolatedWith(trackAccent,
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
        g.setColour(trackAccent.withMultipliedAlpha(0.9f - 0.6f * animation.disabled));
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
                    g.setColour(colours::raised);
                    g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
                }
                else if (hoverAmount > 0.001f || pressAmount > 0.001f || focusAmount > 0.001f)
                {
                    auto wash = colours::surface2.interpolatedWith(colours::raised, pressAmount);
                    // Hover, press, and keyboard focus overlap during a normal
                    // mouse click. Their visual weights are additive, so the
                    // transient total can exceed Colour::withAlpha's [0, 1]
                    // contract even though every animation is individually
                    // bounded. Saturate only the composed wash opacity.
                    const auto washAlpha = headerInteractionWashAlpha(
                        hoverAmount, pressAmount, focusAmount, disabledAmount);
                    g.setColour(wash.withAlpha(washAlpha));
                    g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
                }
            }

            if (disabledAmount > 0.001f)
            {
                g.setColour(colours::canvas.withAlpha(0.42f * disabledAmount));
                g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
            }

            if (id == "header_previous" || id == "header_next")
                drawArrowIcon(g, bounds, id, accent.withMultipliedAlpha(1.0f - 0.60f * disabledAmount));
            else if (id == "header_menu")
                drawMenuIcon(g, bounds, accent.withMultipliedAlpha(1.0f - 0.60f * disabledAmount));

            return;
        }

        auto base = visuallySelected ? colours::raised : backgroundColour;
        const auto emphasis = juce::jmax(hoverAmount * 0.45f, focusAmount * 0.80f);
        if (base.isTransparent())
            base = colours::raised.withAlpha(emphasis);
        else
            base = base.interpolatedWith(colours::raised.brighter(0.08f), emphasis)
                       .darker(0.10f * pressAmount);
        const auto radius = juce::jmin(bounds.getHeight() * 0.5f, Metrics::radius);
        if (! base.isTransparent())
        {
            g.setColour(base);
            g.fillRoundedRectangle(bounds, radius);
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
            const auto emphasis = juce::jmax(animation.hover, animation.focus);
            g.setColour(colours::textMuted.interpolatedWith(colours::danger, emphasis)
                            .withMultipliedAlpha(1.0f - 0.60f * animation.disabled));
            g.strokePath(cross, juce::PathStrokeType(1.4f * scale,
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
        auto textBounds = button.getLocalBounds().reduced(juce::roundToInt(7.0f * scale), 1);
        const bool moduleRail = static_cast<bool>(
            button.getProperties().getWithDefault("fireModuleRail", false));
        if (moduleRail)
        {
            textBounds.removeFromLeft(juce::roundToInt(25.0f * scale));
            textBounds.removeFromRight(juce::roundToInt(static_cast<float>(
                button.getProperties().getWithDefault("fireModuleTrailingSpace", 0.0f))));
        }
        g.drawFittedText(button.getButtonText(), textBounds,
                         moduleRail ? juce::Justification::centredLeft
                                    : juce::Justification::centred, 1);
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
            g.setColour(colours::raised.withAlpha(
                juce::jmax(animation.hover * 0.45f, animation.focus * 0.80f)
                * (1.0f - animation.disabled)));
            g.fillRoundedRectangle(bounds, Metrics::radiusSmall * scale);
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
        // A ComboBox may be painted once in an idle state and then destroyed
        // without ever starting the animation timer. Prune those expired (or
        // hidden) entries whenever the cache is next used so its size follows
        // the number of live controls, rather than the number of UI sessions.
        comboBoxAnimations.erase(
            std::remove_if(comboBoxAnimations.begin(),
                           comboBoxAnimations.end(),
                           [](const auto& state)
                           {
                               return state.box == nullptr
                                   || ! state.box->isShowing();
                           }),
            comboBoxAnimations.end());

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
            const auto focusVisible =
                [&box]
                {
                    if (auto* focusAware =
                            dynamic_cast<FocusAwareComboBox*>(&box))
                        return focusAware->shouldShowInteractionFocus();

                    return box.hasKeyboardFocus(true) || box.isPopupActive();
                }();
            state.focus.snapTo(interactive && focusVisible ? 1.0f : 0.0f);
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
        const auto focusVisible =
            [box]
            {
                if (auto* focusAware =
                        dynamic_cast<FocusAwareComboBox*>(box))
                    return focusAware->shouldShowInteractionFocus();

                return box->hasKeyboardFocus(true) || box->isPopupActive();
            }();
        state.focus.setTarget(interactive && focusVisible ? 1.0f : 0.0f);
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
            auto changed = iterator->hover.advance(1.0f / 60.0f, fire::ui::Motion::hover * 0.5f);
            changed = iterator->press.advance(1.0f / 60.0f, fire::ui::Motion::press * 0.5f) || changed;
            changed = iterator->focus.advance(1.0f / 60.0f, fire::ui::Motion::focus * 0.5f) || changed;
            changed = iterator->disabled.advance(1.0f / 60.0f, fire::ui::Motion::disabled * 0.5f) || changed;
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
                     modSlider->getFocusAnimation(),
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

    static constexpr float dialDiscInsetProportion = 0.27f;

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
            const auto enabledOpacity = 1.0f - 0.65f * disabledAmount;
            g.setColour(accent.withMultipliedAlpha(enabledOpacity));
            g.strokePath(valueArc, juce::PathStrokeType(stroke,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

            if (isDrive && arcState.hasReduction())
            {
                juce::Path reducedArc;
                reducedArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                         safeEnd, valueAngle, true);
                g.setColour(accent.withMultipliedAlpha(0.32f
                                                        * enabledOpacity));
                g.strokePath(reducedArc, juce::PathStrokeType(stroke,
                                                             juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
            }
        }

        auto disc = bounds.reduced(radius * dialDiscInsetProportion);
        juce::ColourGradient metal(colours::surface2.brighter(hoverAmount * 0.06f),
                                   centre.x, disc.getY(), colours::surface0,
                                   centre.x, disc.getBottom(), false);
        metal.addColour(0.42, colours::surface1);
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
        const auto hasValidSource = isValidLfoSourceNumber(slider.lfoSource);
        const auto bankAccent = lfoBankColourForSource(slider.lfoSource);
        const auto modulationAccent = slider.isBypassed
                                          ? bankAccent.interpolatedWith(
                                                colours::disabled, 0.68f)
                                          : bankAccent;

        if (slider.assignModeGlowAlpha > 0.0f && slider.isEnabled())
        {
            g.setColour(colours::flame.withAlpha(juce::jlimit(0.0f, 0.8f, slider.assignModeGlowAlpha)));
            g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre).reduced(1.0f),
                          juce::jmax(1.0f, 1.5f * scale));
        }

        if (slider.isModulated && std::abs(slider.lfoAmount) > 0.0001
            && slider.isEnabled() && radius > 1.0f)
        {
            const auto base = slider.valueToProportionOfLength(slider.getValue());
            const auto depth = std::abs(slider.lfoAmount);
            auto low = slider.isBipolar ? base - depth * 0.5 : base + juce::jmin(0.0, slider.lfoAmount);
            auto high = slider.isBipolar ? base + depth * 0.5 : base + juce::jmax(0.0, slider.lfoAmount);
            low = juce::jlimit(0.0, 1.0, low);
            high = juce::jlimit(0.0, 1.0, high);
            // Follow the pressed dial's inner disc, keeping both the range
            // stroke and the live marker clear of its rim and the value arc.
            const auto dialRadius = juce::jmax(
                2.0f, radius - slider.getPressAnimation() * 0.8f * scale);
            const auto discRadius = dialRadius * (1.0f - dialDiscInsetProportion);
            const auto modStroke = juce::jmin(3.0f * scale, discRadius * 0.14f);
            const auto pointDiameter = juce::jmin(4.0f * scale, discRadius * 0.22f);
            const auto pointBorder = juce::jmin(0.8f * scale, discRadius * 0.05f);
            const auto rimInset = juce::jmax(modStroke * 0.5f,
                                             pointDiameter * 0.5f + pointBorder)
                                  + juce::jmin(0.8f * scale, discRadius * 0.05f);
            const auto modRadius = discRadius - rimInset;
            juce::Path range;
            range.addCentredArc(centre.x, centre.y, modRadius, modRadius, 0.0f,
                                startAngle + static_cast<float>(low) * (endAngle - startAngle),
                                startAngle + static_cast<float>(high) * (endAngle - startAngle), true);
            // The band describes the available range, so keep it behind the
            // neutral base pointer and the brighter, moving position marker.
            g.setColour(modulationAccent.withAlpha(slider.isBypassed ? 0.18f
                                                                     : 0.30f));
            g.strokePath(range, juce::PathStrokeType(modStroke,
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
                const auto pointBounds = juce::Rectangle<float>(
                                             pointDiameter, pointDiameter)
                                             .withCentre(point);
                g.setColour(colours::canvas.withAlpha(0.96f));
                g.fillEllipse(pointBounds.expanded(pointBorder));
                g.setColour(bankAccent.interpolatedWith(colours::whiteHot, 0.60f));
                g.fillEllipse(pointBounds);
            }

            // Extend the base pointer with an inner origin notch. It sits
            // inward of the live dot, including when both values coincide.
            const auto baseAngle = startAngle
                                   + static_cast<float>(base) * (endAngle - startAngle);
            const juce::Point<float> baseDirection {
                std::sin(baseAngle), -std::cos(baseAngle)
            };
            const auto originOuter = modRadius - pointDiameter * 0.5f - pointBorder
                                     - juce::jmin(0.7f * scale, discRadius * 0.045f);
            const auto originInner = originOuter
                                     - juce::jmin(3.0f * scale, discRadius * 0.20f);
            const juce::Line<float> origin {
                centre + baseDirection * originInner,
                centre + baseDirection * originOuter
            };
            const auto originWidth = juce::jmin(1.3f * scale, discRadius * 0.09f);
            g.setColour(colours::canvas.withAlpha(0.92f));
            g.drawLine(origin, originWidth + pointBorder * 2.0f);
            g.setColour(colours::whiteHot.withAlpha(slider.isBypassed ? 0.48f : 0.96f));
            g.drawLine(origin, originWidth);
        }

        if (slider.isModulated)
        {
            const auto hover = juce::jlimit(
                0.0f, 1.0f, slider.getModulationHandleHoverAnimation());
            const auto press = juce::jlimit(
                0.0f, 1.0f, slider.getModulationHandlePressAnimation());
            auto handle = slider.getModulationHandleVisualBounds();
            handle = handle.expanded(handle.getWidth() * 0.08f * hover);
            const auto accent = modulationAccent;
            const auto useDarkLabel = slider.isEnabled() && ! slider.isBypassed
                                      && hasValidSource;
            const auto labelColour = useDarkLabel ? colours::canvas
                                                   : colours::textPrimary;

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
            g.setColour(accent.brighter(hover * 0.12f + press * 0.06f)
                            .withAlpha(slider.isEnabled()
                                           ? 0.90f + hover * 0.08f
                                           : 0.35f));
            g.fillEllipse(handle);
            if (slider.isEnabled() && hover > 0.001f)
            {
                g.setColour(labelColour.withAlpha(hover * 0.42f));
                g.drawEllipse(handle.reduced(0.4f * scale),
                              juce::jmax(0.8f, 1.0f * scale));
            }
            g.setColour(labelColour.withMultipliedAlpha(slider.isEnabled()
                                                            ? 0.96f
                                                            : 0.72f));
            g.setFont(fire::ui::labelFont(juce::jmax(7.0f, handle.getHeight() * 0.48f)));
            g.drawText(hasValidSource ? juce::String(slider.lfoSource) : "?",
                       handle,
                       juce::Justification::centred);
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
        // Match the menu/zoom icon's light, rounded strokes. An open arrow
        // keeps the same connected shaft without a heavy filled triangle.
        const auto shoulder = area.getX() + area.getWidth() * 0.48f;
        juce::Path arrow;
        arrow.startNewSubPath(area.getRight(), area.getCentreY());
        arrow.lineTo(area.getX(), area.getCentreY());
        arrow.startNewSubPath(shoulder, area.getBottom());
        arrow.lineTo(area.getX(), area.getCentreY());
        arrow.lineTo(shoulder, area.getY());

        float rotation = 0.0f;
        if (id == "right_arrow" || id == "header_next")
            rotation = juce::MathConstants<float>::pi;
        else if (id == "slider_up_arrow")
            rotation = juce::MathConstants<float>::halfPi;
        else if (id != "left_arrow" && id != "header_previous")
            rotation = -juce::MathConstants<float>::halfPi;
        arrow.applyTransform(juce::AffineTransform::rotation(rotation, area.getCentreX(), area.getCentreY()));
        g.setColour(colour);
        g.strokePath(arrow, juce::PathStrokeType(stemThickness,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
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
