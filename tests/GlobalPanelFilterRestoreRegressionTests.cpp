#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>

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
} // namespace

TEST_CASE("Global filter mode UI restores every saved APVTS state without changing parameters",
          "[global-panel][filter][state][regression]")
{
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

            std::array<juce::Button*, 3> modeButtons {};
            for (size_t index = 0; index < componentIDs.size(); ++index)
            {
                modeButtons[index] = findButtonByComponentID(
                    panel, componentIDs[index]);
                REQUIRE(modeButtons[index] != nullptr);
                CHECK(modeButtons[index]->getToggleState()
                      == filterMode.enabled[index]);
                CHECK(getPlainParameter(processor, parameterIDs[index])
                      == Catch::Approx(valuesBeforeConstruction[index]));
            }

            CHECK(panel.getLowcutFreqKnob().isVisible()
                  == filterMode.enabled[0]);
            CHECK(panel.getPeakFreqKnob().isVisible()
                  == filterMode.enabled[1]);
            CHECK(panel.getHighcutFreqKnob().isVisible()
                  == filterMode.enabled[2]);
        }
    }
}
