#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <algorithm>

namespace
{
struct FilterModeState
{
    const char* name;
    std::array<bool, 3> enabled;
};

constexpr std::array filterModes {
    FilterModeState { "Low Cut", { true, false, false } },
    FilterModeState { "Peak", { false, true, false } },
    FilterModeState { "High Cut", { false, false, true } }
};

constexpr std::array<const char*, 3> parameterIDs { LOW_ID, BAND_ID, HIGH_ID };
constexpr std::array<const char*, 3> componentIDs { "low_cut", "band_pass", "high_cut" };

void setBooleanParameter(FireAudioProcessor& processor,
                         const juce::String& parameterID,
                         bool enabled)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(
        parameter->convertTo0to1(enabled ? 1.0f : 0.0f));
}

float getPlainParameter(const FireAudioProcessor& processor,
                        const juce::String& parameterID)
{
    const auto* value = processor.treeState.getRawParameterValue(parameterID);
    REQUIRE(value != nullptr);
    return value->load();
}

juce::Button* findButtonByComponentID(juce::Component& root,
                                      const juce::String& componentID)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (child == nullptr)
            continue;

        if (child->getComponentID() == componentID)
            return dynamic_cast<juce::Button*>(child);

        if (auto* nested = findButtonByComponentID(*child, componentID))
            return nested;
    }

    return nullptr;
}

juce::Button* findButtonByText(juce::Component& root,
                               const juce::String& buttonText)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (child == nullptr)
            continue;

        if (auto* button = dynamic_cast<juce::Button*>(child);
            button != nullptr && button->getButtonText() == buttonText)
            return button;

        if (auto* nested = findButtonByText(*child, buttonText))
            return nested;
    }

    return nullptr;
}
} // namespace

TEST_CASE("Global filter mode UI restores every saved APVTS state without changing parameters",
          "[global-panel][filter][state][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto& filterMode : filterModes)
    {
        DYNAMIC_SECTION(filterMode.name)
        {
            FireAudioProcessor processor;

            for (size_t index = 0; index < parameterIDs.size(); ++index)
                setBooleanParameter(processor,
                                    parameterIDs[index],
                                    filterMode.enabled[index]);

            std::array<float, 3> valuesBeforeConstruction {};
            for (size_t index = 0; index < parameterIDs.size(); ++index)
                valuesBeforeConstruction[index] = getPlainParameter(
                    processor, parameterIDs[index]);

            GlobalPanel panel(processor, {}, {}, {}, {}, {});
            panel.setBounds(0, 0, 1000, 500);
            panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            panel.setVisible(true);

            std::array<juce::Button*, 3> modeButtons {};
            for (size_t index = 0; index < componentIDs.size(); ++index)
            {
                modeButtons[index] = findButtonByComponentID(
                    panel, componentIDs[index]);
                REQUIRE(modeButtons[index] != nullptr);
                CHECK(modeButtons[index]->getToggleState()
                      == filterMode.enabled[index]);
                CHECK_FALSE(modeButtons[index]->isVisible());
                CHECK(getPlainParameter(processor, parameterIDs[index])
                      == Catch::Approx(valuesBeforeConstruction[index]));
            }

            CHECK(panel.getEqControls().isShowing());
            CHECK(panel.getSelectedEqNode()
                  == static_cast<int>(std::distance(filterMode.enabled.begin(),
                      std::find(filterMode.enabled.begin(), filterMode.enabled.end(), true))));
            CHECK(panel.getLowcutFreqKnob().isShowing()
                  == filterMode.enabled[0]);
            CHECK(panel.getPeakFreqKnob().isShowing()
                  == filterMode.enabled[1]);
            CHECK(panel.getHighcutFreqKnob().isShowing()
                  == filterMode.enabled[2]);
        }
    }
}

TEST_CASE("Global filter automation cannot reveal controls outside the EQ module",
          "[global-panel][filter][visibility][automation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr std::array<const char*, 2> nonFilterModules { "Lo-Fi", "Analysis" };

    for (const auto* moduleName : nonFilterModules)
    {
        for (size_t selectedMode = 0; selectedMode < filterModes.size(); ++selectedMode)
        {
            DYNAMIC_SECTION(moduleName << " / " << filterModes[selectedMode].name)
            {
                FireAudioProcessor processor;
                GlobalPanel panel(processor, {}, {}, {}, {}, {});
                panel.setBounds(0, 0, 1000, 500);
                panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
                panel.setVisible(true);

                auto* moduleButton = findButtonByText(panel, moduleName);
                auto* filterButton = findButtonByText(panel, "EQ");
                REQUIRE(moduleButton != nullptr);
                REQUIRE(filterButton != nullptr);

                moduleButton->setToggleState(true,
                                             juce::sendNotificationSync);

                // Force an actual host-driven type transition even when the
                // requested type equals the processor's default.
                for (const auto* parameterID : parameterIDs)
                    setBooleanParameter(processor, parameterID, false);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

                setBooleanParameter(processor,
                                    parameterIDs[selectedMode],
                                    true);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

                CHECK_FALSE(panel.getEqControls().isShowing());
                CHECK_FALSE(panel.getLowcutFreqKnob().isShowing());
                CHECK_FALSE(panel.getLowcutGainKnob().isShowing());
                CHECK_FALSE(panel.getPeakFreqKnob().isShowing());
                CHECK_FALSE(panel.getPeakGainKnob().isShowing());
                CHECK_FALSE(panel.getHighcutFreqKnob().isShowing());
                CHECK_FALSE(panel.getHighcutGainKnob().isShowing());

                filterButton->setToggleState(true,
                                             juce::sendNotificationSync);

                CHECK(panel.getEqControls().isShowing());
                CHECK(panel.getSelectedEqNode() == static_cast<int>(selectedMode));
                CHECK(panel.getLowcutFreqKnob().isShowing()
                      == (selectedMode == 0));
                CHECK(panel.getPeakFreqKnob().isShowing()
                      == (selectedMode == 1));
                CHECK(panel.getHighcutFreqKnob().isShowing()
                      == (selectedMode == 2));
            }
        }
    }
}
