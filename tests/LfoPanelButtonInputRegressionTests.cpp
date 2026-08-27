#include <GUI/PrimaryButton.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>

namespace
{
struct ParameterGestureRecorder final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged(int, float) override {}

    void parameterGestureChanged(int, bool gestureIsStarting) override
    {
        gestures.push_back(gestureIsStarting);
    }

    std::vector<bool> gestures;
};

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::ModifierKeys modifiers)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             modifiers,
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

void beginPrimaryClick(juce::Button& button)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(
        component,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
}

void endPrimaryClick(juce::Button& button)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseUp(makeMouseEvent(component, {}));
}

void performPrimaryClick(juce::Button& button)
{
    beginPrimaryClick(button);
    endPrimaryClick(button);
}

PrimaryTextButton* findDirectButton(LfoPanel& panel,
                                    const juce::String& text)
{
    for (auto* child : panel.getChildren())
        if (auto* button = dynamic_cast<PrimaryTextButton*>(child);
            button != nullptr && button->getButtonText() == text)
            return button;

    return nullptr;
}

std::vector<PrimaryTextButton*> collectDirectButtons(LfoPanel& panel)
{
    std::vector<PrimaryTextButton*> buttons;
    for (auto* child : panel.getChildren())
        if (auto* button = dynamic_cast<PrimaryTextButton*>(child))
            buttons.push_back(button);

    return buttons;
}

void setParameterValue(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}
} // namespace

TEST_CASE("LFO panel actions use primary-only pointer buttons",
          "[lfo-button][lfo][ui][input][primary-button]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);

    auto buttons = collectDirectButtons(panel);
    REQUIRE(buttons.size() == 9);

    std::map<juce::String, int> counts;
    for (auto* button : buttons)
        ++counts[button->getButtonText()];

    for (int lfoIndex = 1; lfoIndex <= 4; ++lfoIndex)
        CHECK(counts["LFO " + juce::String(lfoIndex)] == 1);
    CHECK(counts["Edit Mode"] == 1);
    CHECK(counts["Brush Mode"] == 1);
    CHECK(counts["Assign"] == 1);
    CHECK(counts["Matrix"] == 1);
    CHECK(counts["BPM"] == 1);

    for (auto* button : buttons)
    {
        CAPTURE(button->getButtonText());
        int clicks = 0;
        button->onClick = [&clicks] { ++clicks; };

        beginPrimaryClick(*button);
        REQUIRE(button->isDown());
        panel.dismissTransientInteraction();
        CHECK_FALSE(button->isDown());
        endPrimaryClick(*button);
        CHECK(clicks == 0);

        button->onClick = nullptr;
    }

    panel.removeFromDesktop();
}

TEST_CASE("LFO selection cancels a Sync click before rebinding its attachment",
          "[lfo-button][lfo][ui][input][sync][attachment][lifecycle]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    const auto oldSyncID =
        ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0);
    const auto newSyncID =
        ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 1);
    setParameterValue(processor, oldSyncID, 0.0f);
    setParameterValue(processor, newSyncID, 1.0f);

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    auto* syncButton = findDirectButton(panel, "BPM");
    auto* lfoTwoButton = findDirectButton(panel, "LFO 2");
    auto* oldParameter = processor.treeState.getParameter(oldSyncID);
    auto* newParameter = processor.treeState.getParameter(newSyncID);
    REQUIRE(syncButton != nullptr);
    REQUIRE(lfoTwoButton != nullptr);
    REQUIRE(oldParameter != nullptr);
    REQUIRE(newParameter != nullptr);

    ParameterGestureRecorder oldRecorder;
    ParameterGestureRecorder newRecorder;
    oldParameter->addListener(&oldRecorder);
    newParameter->addListener(&newRecorder);
    const juce::ScopeGuard removeListeners {
        [&]
        {
            oldParameter->removeListener(&oldRecorder);
            newParameter->removeListener(&newRecorder);
        }
    };

    beginPrimaryClick(*syncButton);
    REQUIRE(syncButton->isDown());
    lfoTwoButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    CHECK_FALSE(syncButton->isDown());
    CHECK(syncButton->getToggleState());
    CHECK(processor.treeState.getRawParameterValue(oldSyncID)->load() == 0.0f);
    CHECK(processor.treeState.getRawParameterValue(newSyncID)->load() == 1.0f);
    CHECK(oldRecorder.gestures.empty());
    CHECK(newRecorder.gestures.empty());

    endPrimaryClick(*syncButton);
    CHECK(processor.treeState.getRawParameterValue(oldSyncID)->load() == 0.0f);
    CHECK(processor.treeState.getRawParameterValue(newSyncID)->load() == 1.0f);
    CHECK(oldRecorder.gestures.empty());
    CHECK(newRecorder.gestures.empty());

    performPrimaryClick(*syncButton);
    CHECK(processor.treeState.getRawParameterValue(oldSyncID)->load() == 0.0f);
    CHECK(processor.treeState.getRawParameterValue(newSyncID)->load() == 0.0f);
    CHECK(oldRecorder.gestures.empty());
    CHECK(newRecorder.gestures == std::vector<bool> { true, false });

    panel.removeFromDesktop();
}
