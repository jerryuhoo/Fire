#include <GUI/ContextAwareComboBox.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <utility>

namespace
{
class RepaintProbe final : public juce::CachedComponentImage
{
public:
    void paint(juce::Graphics&) override {}

    bool invalidateAll() override
    {
        ++invalidations;
        return false;
    }

    bool invalidate(const juce::Rectangle<int>&) override
    {
        ++invalidations;
        return false;
    }

    void releaseResources() override {}

    int takeInvalidations() noexcept
    {
        return std::exchange(invalidations, 0);
    }

private:
    int invalidations = 0;
};

juce::MouseEvent makeMouseEvent(juce::Component& component)
{
    const auto now = juce::Time::getCurrentTime();
    const auto position = component.getLocalBounds().toFloat().getCentre();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             {},
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             now,
             position,
             now,
             1,
             false };
}

void checkPointerMotionRequestsAnimationFrames(juce::ComboBox& comboBox)
{
    auto* probe = new RepaintProbe();
    comboBox.setCachedComponentImage(probe);
    probe->takeInvalidations();

    auto event = makeMouseEvent(comboBox);
    auto& component = static_cast<juce::Component&>(comboBox);

    component.mouseEnter(event);
    CHECK(probe->takeInvalidations() > 0);

    component.mouseMove(event);
    CHECK(probe->takeInvalidations() > 0);

    component.mouseExit(event);
    CHECK(probe->takeInvalidations() > 0);

    comboBox.setCachedComponentImage(nullptr);
}

state::StateComponent* findStateComponent(juce::Component& root)
{
    if (auto* stateComponent = dynamic_cast<state::StateComponent*>(&root))
        return stateComponent;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* child = root.getChildComponent(index))
            if (auto* result = findStateComponent(*child))
                return result;

    return nullptr;
}
} // namespace

TEST_CASE("Production ComboBoxes repaint on hover motion for smooth feedback",
          "[combo-box][ui][animation][hover]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("control panel and LFO menus")
    {
        juce::Component host;
        host.setBounds(0, 0, 480, 240);
        host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        host.setVisible(true);

        ContextAwareComboBox contextAware;
        LfoBrushSelector brushSelector;
        ModulationMatrixRoutingComboBox routingSelector;
        std::array<juce::ComboBox*, 3> comboBoxes {
            &contextAware, &brushSelector, &routingSelector
        };

        int y = 0;
        for (auto* comboBox : comboBoxes)
        {
            host.addAndMakeVisible(*comboBox);
            comboBox->setBounds(0, y, 180, 32);
            y += 40;
            checkPointerMotionRequestsAnimationFrames(*comboBox);
        }
    }

    SECTION("preset menu")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        FireAudioProcessorEditor editor(processor);
        editor.setBounds(0, 0, 1000, 500);
        editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor.setVisible(true);

        auto* stateComponent = findStateComponent(editor);
        REQUIRE(stateComponent != nullptr);
        auto* presetBox = stateComponent->getPresetBox();
        REQUIRE(presetBox != nullptr);
        checkPointerMotionRequestsAnimationFrames(*presetBox);
    }
}
