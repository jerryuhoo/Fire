#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/LfoPanel.h>

#include <catch2/catch_test_macros.hpp>

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

juce::MouseEvent makeLeftMouseDown(juce::Component& component)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             time,
             position,
             time,
             1,
             false };
}

int countActiveRoutings(const juce::Array<ModulationRouting>& routings)
{
    int activeCount = 0;
    for (const auto& routing : routings)
        if (routing.targetParameterID.isNotEmpty())
            ++activeCount;

    return activeCount;
}
} // namespace

TEST_CASE("Changing LFO selection updates the pending Assign source",
          "[ui][lfo][assign]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);

    auto* lfoPanel = findDescendant<LfoPanel>(*editor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    REQUIRE(lfoPanel != nullptr);
    REQUIRE(bandPanel != nullptr);

    auto* assignButton = findButtonWithText(*lfoPanel, "Assign");
    auto* lfoFourButton = findButtonWithText(*lfoPanel, "LFO 4");
    REQUIRE(assignButton != nullptr);
    REQUIRE(lfoFourButton != nullptr);

    assignButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    REQUIRE(assignButton->getToggleState());
    CHECK(countActiveRoutings(processor.getLfoManager().getModulationRoutingsCopy()) == 0);

    lfoFourButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    // Selecting an LFO only retargets the pending one-shot Assign session.
    // It must neither finish the interaction nor create a routing by itself.
    REQUIRE(lfoFourButton->getToggleState());
    REQUIRE(assignButton->getToggleState());
    CHECK(countActiveRoutings(processor.getLfoManager().getModulationRoutingsCopy()) == 0);

    auto* target = bandPanel->getDriveKnob();
    REQUIRE(target != nullptr);
    const auto targetParameterID = target->getParamID();
    REQUIRE(targetParameterID.isNotEmpty());

    target->mouseDown(makeLeftMouseDown(*target));

    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    CHECK(countActiveRoutings(routings) == 1);
    REQUIRE_FALSE(assignButton->getToggleState());

    const ModulationRouting* assignedRouting = nullptr;
    for (const auto& routing : routings)
        if (routing.targetParameterID == targetParameterID)
            assignedRouting = &routing;

    REQUIRE(assignedRouting != nullptr);
    CHECK(assignedRouting->sourceLfoIndex == 3);

    for (const auto& routing : routings)
        if (routing.targetParameterID.isNotEmpty())
            CHECK(routing.sourceLfoIndex != 0);
}
