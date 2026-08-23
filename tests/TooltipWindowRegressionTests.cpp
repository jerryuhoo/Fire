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

    auto* tooltip = findDescendant<juce::TooltipWindow>(*editor);
    auto* hqButton = findButtonWithText(*editor, "HQ");
    REQUIRE(tooltip != nullptr);
    REQUIRE(hqButton != nullptr);

    CHECK(tooltip->getParentComponent() == editor.get());
    CHECK(&tooltip->getLookAndFeel() == &editor->getLookAndFeel());
    CHECK(hqButton->getTooltip() == "High-quality oversampling");
    CHECK(tooltip->findColour(juce::TooltipWindow::backgroundColourId)
          == fire::ui::colours::surface1);
    CHECK(tooltip->findColour(juce::TooltipWindow::textColourId)
          == fire::ui::colours::textPrimary);
    CHECK(tooltip->findColour(juce::TooltipWindow::outlineColourId)
          == fire::ui::colours::flame.withAlpha(0.62f));

    REQUIRE_FALSE(tooltip->isVisible());
    tooltip->displayTip(editor->localPointToGlobal(hqButton->getBounds().getCentre()),
                        hqButton->getTooltip());
    CHECK(tooltip->isVisible());
    CHECK(editor->getLocalBounds().contains(tooltip->getBounds()));
    tooltip->hideTip();
    CHECK_FALSE(tooltip->isVisible());
}
