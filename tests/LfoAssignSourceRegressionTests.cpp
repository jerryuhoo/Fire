#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/LfoPanel.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <utility>

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

juce::ComboBox* selectCleanPreset(FireAudioProcessorEditor& editor)
{
    auto* stateComponent = findDescendant<state::StateComponent>(editor);
    if (stateComponent == nullptr)
        return nullptr;

    auto* presetBox = stateComponent->getPresetBox();
    if (presetBox == nullptr)
        return nullptr;

    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    presetBox->clear(juce::dontSendNotification);
    presetBox->addItem("Clean Preset", 1);
    presetBox->setSelectedId(1, juce::dontSendNotification);
    return presetBox;
}

void fillModulationRoutingCapacity(FireAudioProcessor& processor)
{
    juce::Array<ModulationRouting> fullRoutings;
    fullRoutings.ensureStorageAllocated(
        LfoManager::maximumModulationRoutings);
    for (int index = 0; index < LfoManager::maximumModulationRoutings;
         ++index)
    {
        ModulationRouting routing;
        routing.sourceLfoIndex = index % 4;
        routing.targetParameterID =
            "full_assignment_target_" + juce::String(index);
        fullRoutings.add(std::move(routing));
    }

    REQUIRE(processor.getLfoManager().replaceLfoDataAndRoutings(
        std::array<LfoData, 4> {}, fullRoutings));
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
    CHECK(assignButton->getButtonText() == "Assign LFO 1");
    CHECK(countActiveRoutings(processor.getLfoManager().getModulationRoutingsCopy()) == 0);

    lfoFourButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    // Selecting an LFO only retargets the pending one-shot Assign session.
    // It must neither finish the interaction nor create a routing by itself.
    REQUIRE(lfoFourButton->getToggleState());
    REQUIRE(assignButton->getToggleState());
    CHECK(assignButton->getButtonText() == "Assign LFO 4");
    CHECK(countActiveRoutings(processor.getLfoManager().getModulationRoutingsCopy()) == 0);

    auto* target = bandPanel->getDriveKnob();
    REQUIRE(target != nullptr);
    const auto targetParameterID = target->getParamID();
    REQUIRE(targetParameterID.isNotEmpty());

    target->mouseDown(makeLeftMouseDown(*target));

    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    CHECK(countActiveRoutings(routings) == 1);
    REQUIRE_FALSE(assignButton->getToggleState());
    CHECK(assignButton->getButtonText() == "LFO 4 Assigned");

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

TEST_CASE("Editor modulation snapshots reject invalid LFO source indices",
          "[ui][lfo][snapshot][invalid-source][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto targetParameterID =
        ParameterIDAndName::getIDString(DRIVE_ID, 0);

    SECTION("An invalid source is not presented as LFO 1 or LFO 4")
    {
        for (const int invalidSource : { -1, 4 })
        {
            DYNAMIC_SECTION("source index " << invalidSource)
            {
                FireAudioProcessor processor;
                processor.hasUpdateCheckBeenPerformed = true;

                ModulationRouting invalidRouting;
                invalidRouting.sourceLfoIndex = invalidSource;
                invalidRouting.targetParameterID = targetParameterID;
                invalidRouting.depth = 0.75f;
                invalidRouting.isBipolar = false;
                invalidRouting.isBypassed = true;

                juce::Array<ModulationRouting> routings;
                routings.add(invalidRouting);
                REQUIRE(processor.getLfoManager().replaceLfoDataAndRoutings(
                    std::array<LfoData, 4> {}, routings));

                const auto processorInfo =
                    processor.getModulationInfoForParameter(targetParameterID);
                REQUIRE_FALSE(processorInfo.isModulated);

                FireAudioProcessorEditor editor(processor);
                auto* bandPanel = findDescendant<BandPanel>(editor);
                REQUIRE(bandPanel != nullptr);
                auto* drive = bandPanel->getDriveKnob();
                REQUIRE(drive != nullptr);
                REQUIRE(drive->getParamID() == targetParameterID);

                CHECK_FALSE(drive->isModulated);
                CHECK(drive->lfoSource == 0);
                CHECK(drive->lfoAmount == Catch::Approx(0.0));
                CHECK(drive->lfoValue == Catch::Approx(0.0));
                CHECK(drive->isBipolar);
                CHECK_FALSE(drive->isBypassed);
            }
        }
    }

    SECTION("An invalid first route does not hide a later valid route")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;

        ModulationRouting invalidRouting;
        invalidRouting.sourceLfoIndex = -1;
        invalidRouting.targetParameterID = targetParameterID;
        invalidRouting.depth = -0.8f;
        invalidRouting.isBypassed = true;

        ModulationRouting validRouting;
        validRouting.sourceLfoIndex = 2;
        validRouting.targetParameterID = targetParameterID;
        validRouting.depth = 0.37f;
        validRouting.isBipolar = false;

        juce::Array<ModulationRouting> routings;
        routings.add(invalidRouting);
        routings.add(validRouting);
        REQUIRE(processor.getLfoManager().replaceLfoDataAndRoutings(
            std::array<LfoData, 4> {}, routings));

        const auto processorInfo =
            processor.getModulationInfoForParameter(targetParameterID);
        REQUIRE(processorInfo.isModulated);
        REQUIRE(processorInfo.sourceLfoIndex == 3);

        FireAudioProcessorEditor editor(processor);
        auto* bandPanel = findDescendant<BandPanel>(editor);
        REQUIRE(bandPanel != nullptr);
        auto* drive = bandPanel->getDriveKnob();
        REQUIRE(drive != nullptr);
        REQUIRE(drive->getParamID() == targetParameterID);

        CHECK(drive->isModulated);
        CHECK(drive->lfoSource == 3);
        CHECK(drive->lfoAmount == Catch::Approx(0.37f));
        CHECK_FALSE(drive->isBipolar);
        CHECK_FALSE(drive->isBypassed);
    }
}

TEST_CASE("LFO assignment feedback reflects unchanged and full results",
          "[ui][lfo][assign][feedback][capacity][dirty][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("Assign mode reports an existing route without dirtying preset")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        const auto targetParameterID =
            ParameterIDAndName::getIDString(DRIVE_ID, 0);
        REQUIRE(processor.assignLfoToTarget(0, targetParameterID)
                == LfoManager::AssignmentResult::changed);

        FireAudioProcessorEditor editor(processor);
        editor.setBounds(0, 0, 1000, 500);
        auto* lfoPanel = findDescendant<LfoPanel>(editor);
        auto* bandPanel = findDescendant<BandPanel>(editor);
        REQUIRE(lfoPanel != nullptr);
        REQUIRE(bandPanel != nullptr);
        auto* assignButton = findButtonWithText(*lfoPanel, "Assign");
        auto* target = bandPanel->getDriveKnob();
        auto* presetBox = selectCleanPreset(editor);
        REQUIRE(assignButton != nullptr);
        REQUIRE(target != nullptr);
        REQUIRE(presetBox != nullptr);
        REQUIRE(target->getParamID() == targetParameterID);
        REQUIRE(presetBox->getSelectedId() == 1);

        const auto revisionBefore =
            processor.getLfoManager().getModulationRoutingRevision();
        assignButton->triggerClick();
        REQUIRE(assignButton->getToggleState());
        target->mouseDown(makeLeftMouseDown(*target));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);

        CHECK_FALSE(assignButton->getToggleState());
        CHECK(assignButton->getButtonText()
              == "LFO 1 Already Assigned");
        CHECK(processor.getLfoManager().getModulationRoutingRevision()
              == revisionBefore);
        CHECK(presetBox->getSelectedId() == 1);
        CHECK(presetBox->getText() == "Clean Preset");
    }

    SECTION("right-click assignment reports capacity without dirtying preset")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        fillModulationRoutingCapacity(processor);

        FireAudioProcessorEditor editor(processor);
        editor.setBounds(0, 0, 1000, 500);
        auto* lfoPanel = findDescendant<LfoPanel>(editor);
        auto* bandPanel = findDescendant<BandPanel>(editor);
        REQUIRE(lfoPanel != nullptr);
        REQUIRE(bandPanel != nullptr);
        auto* assignButton = findButtonWithText(*lfoPanel, "Assign");
        auto* target = bandPanel->getDriveKnob();
        auto* presetBox = selectCleanPreset(editor);
        REQUIRE(assignButton != nullptr);
        REQUIRE(target != nullptr);
        REQUIRE(presetBox != nullptr);
        REQUIRE(target->onLfoAssignmentRequested != nullptr);

        const auto revisionBefore =
            processor.getLfoManager().getModulationRoutingRevision();
        target->onLfoAssignmentRequested(2, target->getParamID());
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);

        CHECK(assignButton->getButtonText() == "Mod Matrix Full");
        CHECK(processor.getLfoManager().getModulationRoutingRevision()
              == revisionBefore);
        CHECK(countActiveRoutings(
                  processor.getLfoManager().getModulationRoutingsCopy())
              == LfoManager::maximumModulationRoutings);
        CHECK(presetBox->getSelectedId() == 1);
        CHECK(presetBox->getText() == "Clean Preset");
    }
}

TEST_CASE("Assign mode reports cancellation and cannot survive editor hiding",
          "[ui][lfo][assign][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->setVisible(true);
    auto* lfoPanel = findDescendant<LfoPanel>(*editor);
    REQUIRE(lfoPanel != nullptr);
    auto* assignButton = findButtonWithText(*lfoPanel, "Assign");
    REQUIRE(assignButton != nullptr);

    assignButton->triggerClick();
    REQUIRE(assignButton->getToggleState());
    assignButton->triggerClick();
    CHECK_FALSE(assignButton->getToggleState());
    CHECK(assignButton->getButtonText() == "Assign Cancelled");

    assignButton->triggerClick();
    REQUIRE(assignButton->getToggleState());
    editor->setVisible(false);
    CHECK_FALSE(assignButton->getToggleState());
    CHECK(assignButton->getButtonText() == "Assign");

    editor->setVisible(true);
    assignButton->triggerClick();
    REQUIRE(assignButton->getToggleState());
    editor->setEnabled(false);
    CHECK_FALSE(assignButton->getToggleState());
    CHECK(assignButton->getButtonText() == "Assign");
}

TEST_CASE("Assign result feedback pauses while the LFO workspace is hidden",
          "[ui][lfo][assign][feedback][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    juce::Component host;
    host.setBounds(0, 0, 1000, 500);
    host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    host.setVisible(true);

    LfoPanel lfoPanel(processor);
    lfoPanel.setBounds(host.getLocalBounds());
    host.addAndMakeVisible(lfoPanel);
    REQUIRE(lfoPanel.isShowing());

    lfoPanel.showAssignCompleted(2);
    CHECK(lfoPanel.assignButton.getButtonText() == "LFO 3 Assigned");

    // Switching to another workspace must pause the amount of visible time
    // remaining instead of consuming or clearing it off-screen.
    lfoPanel.setVisible(false);
    for (int frame = 0; frame < 20; ++frame)
        lfoPanel.animationTick(0.1f);
    CHECK(lfoPanel.assignButton.getButtonText() == "LFO 3 Assigned");

    lfoPanel.setVisible(true);
    REQUIRE(lfoPanel.isShowing());
    for (int frame = 0; frame < 5; ++frame)
        lfoPanel.animationTick(0.1f);
    CHECK(lfoPanel.assignButton.getButtonText() == "LFO 3 Assigned");

    for (int frame = 0; frame < 7; ++frame)
        lfoPanel.animationTick(0.1f);
    CHECK(lfoPanel.assignButton.getButtonText() == "Assign");
}

TEST_CASE("Assign feedback cannot survive a detached editor peer",
          "[ui][lfo][assign][feedback][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    REQUIRE(editor->isShowing());
    editor->timerCallback();

    auto* lfoPanel = findDescendant<LfoPanel>(*editor);
    REQUIRE(lfoPanel != nullptr);
    lfoPanel->showAssignCompleted(1);
    CHECK(lfoPanel->assignButton.getButtonText() == "LFO 2 Assigned");

    // Some hosts detach the peer while retaining a visible Editor component,
    // so there is no Component::visibilityChanged() notification to perform
    // session cleanup. The shared UI timer observes this boundary instead.
    editor->removeFromDesktop();
    REQUIRE_FALSE(editor->isShowing());
    editor->timerCallback();
    CHECK(lfoPanel->assignButton.getButtonText() == "Assign");
}
