#include <GUI/FireTheme.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace
{
template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;

    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
        if (auto* child = root.getChildComponent(childIndex))
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;

    return nullptr;
}

juce::Button* findButtonWithText(juce::Component& root, const juce::String& text)
{
    if (auto* button = dynamic_cast<juce::Button*>(&root);
        button != nullptr && button->getButtonText() == text)
        return button;

    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
        if (auto* child = root.getChildComponent(childIndex))
            if (auto* match = findButtonWithText(*child, text))
                return match;

    return nullptr;
}
} // namespace

TEST_CASE("Editor-owned tooltips expose help using the Fire theme",
          "[ui][tooltip]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->setVisible(true);

    auto* tooltip = findDescendant<FireTooltipWindow>(*editor);
    auto* hqButton = findButtonWithText(*editor, "HQ");
    REQUIRE(tooltip != nullptr);
    REQUIRE(hqButton != nullptr);

    CHECK(tooltip->getParentComponent() == editor.get());
    CHECK(&tooltip->getLookAndFeel() == &editor->getLookAndFeel());
    CHECK(tooltip->getConfiguredDelayMilliseconds() == 900);
    CHECK_FALSE(tooltip->isOpaque());
    CHECK(hqButton->getTooltip() == "High-quality oversampling");
    const auto& palette = fire::ui::paletteFor(*editor);
    CHECK(tooltip->findColour(juce::TooltipWindow::backgroundColourId)
          == palette.surface1);
    CHECK(tooltip->findColour(juce::TooltipWindow::textColourId)
          == palette.textPrimary);
    CHECK(tooltip->findColour(juce::TooltipWindow::outlineColourId)
          == palette.hairline.withAlpha(0.78f));

    auto* fireLookAndFeel = dynamic_cast<FireLookAndFeel*>(
        &editor->getLookAndFeel());
    REQUIRE(fireLookAndFeel != nullptr);
    const auto tooltipText = hqButton->getTooltip();
    const auto parentArea = editor->getLocalBounds();
    const auto anchor = parentArea.getCentre();
    fireLookAndFeel->scale = 1.0f;
    const auto oneXBounds = fireLookAndFeel->getTooltipBounds(
        tooltipText, anchor, parentArea);
    fireLookAndFeel->scale = 2.0f;
    const auto twoXBounds = fireLookAndFeel->getTooltipBounds(
        tooltipText, anchor, parentArea);
    CHECK(twoXBounds.getWidth() > oneXBounds.getWidth());
    CHECK(twoXBounds.getHeight() > oneXBounds.getHeight());
    CHECK(parentArea.contains(oneXBounds));
    CHECK(parentArea.contains(twoXBounds));
    fireLookAndFeel->scale = 1.0f;

    REQUIRE_FALSE(tooltip->isVisible());
    tooltip->displayTip(editor->localPointToGlobal(hqButton->getBounds().getCentre()),
                        hqButton->getTooltip());
    CHECK(tooltip->isVisible());
    CHECK(editor->getLocalBounds().contains(tooltip->getBounds()));

    juce::Image tooltipImage(juce::Image::ARGB,
                             tooltip->getWidth(),
                             tooltip->getHeight(),
                             true);
    {
        juce::Graphics tooltipGraphics(tooltipImage);
        tooltip->paintEntireComponent(tooltipGraphics, true);
    }

    // The neutral Fire card has truly transparent rounded corners rather
    // than JUCE's opaque square backing or the previous bright orange frame.
    CHECK(tooltipImage.getPixelAt(0, 0).getAlpha() == 0);
    CHECK(tooltipImage.getPixelAt(tooltipImage.getWidth() / 2,
                                  tooltipImage.getHeight() / 2).getAlpha() > 0);
    const auto edgePixel = tooltipImage.getPixelAt(
        0, tooltipImage.getHeight() / 2);
    CHECK(edgePixel.getRed() < fire::ui::colours::flame.getRed() / 2);
    if (fire::ui::isVintage(*editor))
        CHECK(edgePixel.getRed() >= edgePixel.getBlue());
    else
        CHECK(edgePixel.getBlue() >= edgePixel.getRed());

    editor->setVisible(false);
    CHECK_FALSE(tooltip->isVisible());

    editor->setVisible(true);
    CHECK_FALSE(tooltip->isVisible());
}
