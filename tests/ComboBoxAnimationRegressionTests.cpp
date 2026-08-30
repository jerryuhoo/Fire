#include <GUI/ContextAwareComboBox.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <type_traits>
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

class ComboBoxFocusProbe final : public FocusAwareComboBox
{
public:
    void beginPointerInteractionForTesting() noexcept
    {
        notePointerInteraction();
    }

    void activateJucePopupForTesting()
    {
        juce::ComboBox::keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey });
    }

    void showPopup() override
    {
        // JUCE's queued opener has already set menuActive synchronously. The
        // focus-state test needs that real flag but no native popup window.
    }

    bool sendKeyForTesting(const juce::KeyPress& key)
    {
        return FocusAwareComboBox::keyPressed(key);
    }

    void gainFocusForTesting(FocusChangeType cause)
    {
        FocusAwareComboBox::focusGained(cause);
    }

    void loseFocusForTesting(FocusChangeType cause)
    {
        FocusAwareComboBox::focusLost(cause);
    }
};

std::uint64_t renderFingerprint(FocusAwareComboBox& comboBox)
{
    FireLookAndFeel lookAndFeel;
    comboBox.setLookAndFeel(&lookAndFeel);

    juce::Image image(juce::Image::ARGB,
                      comboBox.getWidth(),
                      comboBox.getHeight(),
                      true);
    juce::Graphics graphics(image);
    comboBox.paintEntireComponent(graphics, true);
    comboBox.setLookAndFeel(nullptr);

    std::uint64_t fingerprint = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            fingerprint ^= image.getPixelAt(x, y).getARGB();
            fingerprint *= 1099511628211ull;
        }

    return fingerprint;
}

juce::MouseEvent makeMouseEvent(
    juce::Component& component,
    juce::ModifierKeys modifiers = {})
{
    const auto now = juce::Time::getCurrentTime();
    const auto position = component.getLocalBounds().toFloat().getCentre();
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
            CHECK(dynamic_cast<FocusAwareComboBox*>(comboBox) != nullptr);
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
        CHECK(dynamic_cast<FocusAwareComboBox*>(presetBox) != nullptr);
        checkPointerMotionRequestsAnimationFrames(*presetBox);
    }
}

TEST_CASE("ComboBox focus styling follows input modality and popup lifetime",
          "[combo-box][ui][animation][focus]")
{
    static_assert(std::is_base_of_v<FocusAwareComboBox,
                                    ContextAwareComboBox>);
    static_assert(std::is_base_of_v<FocusAwareComboBox,
                                    LfoBrushSelector>);
    static_assert(std::is_base_of_v<FocusAwareComboBox,
                                    ModulationMatrixRoutingComboBox>);

    juce::ScopedJuceInitialiser_GUI gui;

    juce::Component host;
    host.setBounds(0, 0, 420, 120);
    host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    host.setVisible(true);

    juce::Component focusSink;
    focusSink.setWantsKeyboardFocus(true);
    host.addAndMakeVisible(focusSink);
    focusSink.setBounds(220, 16, 80, 32);

    ComboBoxFocusProbe comboBox;
    comboBox.addItem("First", 1);
    comboBox.addItem("Second", 2);
    comboBox.setSelectedId(1, juce::dontSendNotification);
    comboBox.setWantsKeyboardFocus(true);
    host.addAndMakeVisible(comboBox);
    comboBox.setBounds(16, 16, 180, 32);

    focusSink.grabKeyboardFocus();
    comboBox.grabKeyboardFocus();
    REQUIRE(comboBox.hasKeyboardFocus(true));
    CHECK(comboBox.shouldShowInteractionFocus());
    const auto directFocusFingerprint = renderFingerprint(comboBox);

    // A click on a ComboBox which already owns keyboard focus must still
    // switch it back to pointer modality.
    comboBox.beginPointerInteractionForTesting();
    CHECK_FALSE(comboBox.shouldShowInteractionFocus());
    const auto pointerFocusFingerprint = renderFingerprint(comboBox);
    CHECK(pointerFocusFingerprint != directFocusFingerprint);

    // The popup is a live interaction even though pointer-acquired focus is
    // intentionally not focus-visible.
    comboBox.activateJucePopupForTesting();
    CHECK(comboBox.shouldShowInteractionFocus());
    const auto popupFingerprint = renderFingerprint(comboBox);
    CHECK(popupFingerprint == directFocusFingerprint);

    comboBox.loseFocusForTesting(
        juce::Component::focusChangedDirectly);
    comboBox.gainFocusForTesting(
        juce::Component::focusChangedDirectly);
    CHECK(comboBox.shouldShowInteractionFocus());

    // Calling JUCE's base hidePopup must be sufficient: there is no parallel
    // popup-focus bit which can leak when a production subclass bypasses it.
    comboBox.hidePopup();
    static_cast<juce::Component&>(comboBox).mouseExit(
        makeMouseEvent(comboBox));
    CHECK_FALSE(comboBox.shouldShowInteractionFocus());
    CHECK(renderFingerprint(comboBox) == pointerFocusFingerprint);

    // The first real key after a mouse click restores focus-visible without
    // requiring focus to leave and re-enter the control.
    comboBox.sendKeyForTesting(juce::KeyPress { 'x' });
    CHECK(comboBox.shouldShowInteractionFocus());
    CHECK(renderFingerprint(comboBox) == directFocusFingerprint);

    comboBox.sendKeyForTesting(
        juce::KeyPress { juce::KeyPress::returnKey });
    REQUIRE(comboBox.isPopupActive());
    comboBox.loseFocusForTesting(
        juce::Component::focusChangedDirectly);
    comboBox.gainFocusForTesting(
        juce::Component::focusChangedDirectly);
    comboBox.hidePopup();
    CHECK(comboBox.shouldShowInteractionFocus());

    comboBox.beginPointerInteractionForTesting();
    comboBox.gainFocusForTesting(juce::Component::focusChangedByTabKey);
    CHECK(comboBox.shouldShowInteractionFocus());

    ContextAwareComboBox productionComboBox;
    productionComboBox.addItem("First", 1);
    productionComboBox.addItem("Second", 2);
    productionComboBox.setSelectedId(1, juce::dontSendNotification);
    productionComboBox.configurePopupSession(
        [] { return std::uint64_t { 1 }; },
        [] { return true; },
        nullptr);
    host.addAndMakeVisible(productionComboBox);
    productionComboBox.setBounds(16, 64, 180, 32);

    focusSink.grabKeyboardFocus();
    productionComboBox.grabKeyboardFocus();
    REQUIRE(productionComboBox.shouldShowInteractionFocus());
    const auto productionKeyboardFingerprint =
        renderFingerprint(productionComboBox);

    static_cast<juce::Component&>(productionComboBox).mouseDown(
        makeMouseEvent(
            productionComboBox,
            juce::ModifierKeys {
                juce::ModifierKeys::leftButtonModifier }));
    REQUIRE(productionComboBox.isPopupActive());
    CHECK(productionComboBox.shouldShowInteractionFocus());

    productionComboBox.dismissTransientInteraction();
    static_cast<juce::Component&>(productionComboBox).mouseExit(
        makeMouseEvent(productionComboBox));
    CHECK_FALSE(productionComboBox.isPopupActive());
    CHECK_FALSE(productionComboBox.shouldShowInteractionFocus());
    CHECK(renderFingerprint(productionComboBox)
          != productionKeyboardFingerprint);
}
