#include <GUI/PrimaryButton.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <vector>

struct LfoPanelBrushTestAccess final
{
    static LfoBrushSelector& getSelector(LfoPanel& panel)
    {
        return panel.brushSelector;
    }

    static std::function<void(int)> createPopupResultHandler(
        LfoPanel& panel)
    {
        return panel.brushSelector.createPopupResultHandler();
    }

    static void setEditMode(LfoPanel& panel, LfoEditMode mode)
    {
        panel.setEditMode(mode);
    }

    static void setLfo(LfoPanel& panel, int lfoIndex)
    {
        panel.setLfo(lfoIndex);
    }

    static LfoPresetShape getCurrentBrush(const LfoPanel& panel)
    {
        return panel.lfoEditor.currentBrush;
    }

    static bool keyPressed(LfoPanel& panel, const juce::KeyPress& key)
    {
        return panel.brushSelector.keyPressed(key);
    }

    static bool hasPointerInteraction(const LfoPanel& panel)
    {
        return panel.brushSelector.pointerInteractionActive;
    }

    static PrimaryTextButton& getLfoSelectButton(LfoPanel& panel,
                                                  int index)
    {
        return *panel.lfoSelectButtons[static_cast<size_t>(index)];
    }

    static LfoEditor& getEditor(LfoPanel& panel)
    {
        return panel.lfoEditor;
    }

    static std::array<PrimarySlider*, 3> getMotionSliders(LfoPanel& panel)
    {
        return { &panel.rateSlider,
                 &panel.lfoSmoothSlider,
                 &panel.lfoPhaseSlider };
    }

    static std::array<PrimarySlider*, 5> getAllSliders(LfoPanel& panel)
    {
        return { &panel.rateSlider,
                 &panel.gridXSlider,
                 &panel.gridYSlider,
                 &panel.lfoSmoothSlider,
                 &panel.lfoPhaseSlider };
    }

    static void restoreDefaultSelectionCallback(LfoPanel& panel)
    {
        panel.brushSelector.setSelectionCallback(
            [&panel](LfoPresetShape brush)
            {
                panel.lfoEditor.setCurrentBrush(brush);
            });
    }
};

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

struct DeletePanelOnButtonStateChange final : juce::Button::Listener
{
    explicit DeletePanelOnButtonStateChange(
        std::unique_ptr<LfoPanel>& panelToDelete)
        : panel(panelToDelete)
    {
    }

    void buttonClicked(juce::Button*) override {}

    void buttonStateChanged(juce::Button*) override
    {
        if (armed)
            panel.reset();
    }

    std::unique_ptr<LfoPanel>& panel;
    bool armed = false;
};

struct DeletePanelOnVisibilityChange final : juce::ComponentListener
{
    explicit DeletePanelOnVisibilityChange(
        std::unique_ptr<LfoPanel>& panelToDelete)
        : panel(panelToDelete)
    {
    }

    void componentVisibilityChanged(juce::Component&) override
    {
        if (armed)
            panel.reset();
    }

    std::unique_ptr<LfoPanel>& panel;
    bool armed = false;
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

void prepareBrushPanel(LfoPanel& panel)
{
    panel.setBounds(0, 0, 1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    LfoPanelBrushTestAccess::setEditMode(panel, LfoEditMode::BrushPaint);
}

std::vector<juce::Component*> collectVisibleInteractiveChildren(
    LfoPanel& panel)
{
    std::vector<juce::Component*> controls;
    auto* editor = &LfoPanelBrushTestAccess::getEditor(panel);

    for (auto* child : panel.getChildren())
        if (child->isVisible()
            && (child == editor
                || dynamic_cast<juce::Button*>(child) != nullptr
                || dynamic_cast<juce::Slider*>(child) != nullptr
                || dynamic_cast<juce::ComboBox*>(child) != nullptr))
            controls.push_back(child);

    return controls;
}

juce::Label* findSliderTextLabel(juce::Slider& slider)
{
    for (auto* child : slider.getChildren())
        if (auto* label = dynamic_cast<juce::Label*>(child))
            return label;

    return nullptr;
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

TEST_CASE("LFO brush selector has one primary pointer owner",
          "[lfo][brush-selector][combo][input][primary][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    LfoPanel panel(processor);
    prepareBrushPanel(panel);
    auto& selector = LfoPanelBrushTestAccess::getSelector(panel);
    auto& component = static_cast<juce::Component&>(selector);

    const auto exerciseRejectedDown = [&](int modifierFlags)
    {
        component.mouseDown(makeMouseEvent(
            selector, juce::ModifierKeys { modifierFlags }));
        CHECK_FALSE(selector.isPopupActive());
        CHECK_FALSE(LfoPanelBrushTestAccess::hasPointerInteraction(panel));
        component.mouseUp(makeMouseEvent(selector, {}));
    };

    exerciseRejectedDown(juce::ModifierKeys::rightButtonModifier);
    exerciseRejectedDown(juce::ModifierKeys::middleButtonModifier);
    exerciseRejectedDown(juce::ModifierKeys::leftButtonModifier
                         | juce::ModifierKeys::rightButtonModifier);
    exerciseRejectedDown(juce::ModifierKeys::leftButtonModifier
                         | juce::ModifierKeys::middleButtonModifier);

    component.mouseDown(makeMouseEvent(
        selector,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    REQUIRE(selector.isPopupActive());
    REQUIRE(LfoPanelBrushTestAccess::hasPointerInteraction(panel));

    panel.dismissTransientInteraction();
    CHECK_FALSE(selector.isPopupActive());

    // The physical release belongs to the dismissed session. It may only
    // clear JUCE's private pressed bit, never reopen the popup.
    component.mouseUp(makeMouseEvent(selector, {}));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    CHECK_FALSE(selector.isPopupActive());
    CHECK_FALSE(LfoPanelBrushTestAccess::hasPointerInteraction(panel));

    panel.removeFromDesktop();
}

TEST_CASE("LFO brush selector rejects results from replaced editing sessions",
          "[lfo][brush-selector][combo][popup][session][stale][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    LfoPanel panel(processor);
    prepareBrushPanel(panel);
    auto& selector = LfoPanelBrushTestAccess::getSelector(panel);
    REQUIRE(selector.getSelectedId()
            == static_cast<int>(LfoPresetShape::SawUp));
    REQUIRE(LfoPanelBrushTestAccess::getCurrentBrush(panel)
            == LfoPresetShape::SawUp);

    auto staleResult =
        LfoPanelBrushTestAccess::createPopupResultHandler(panel);

    SECTION("replacement popup")
    {
        // Creating the current result below is the entire boundary.
    }

    SECTION("Point Edit to Brush Paint ABA")
    {
        LfoPanelBrushTestAccess::setEditMode(panel,
                                             LfoEditMode::PointEdit);
        LfoPanelBrushTestAccess::setEditMode(panel,
                                             LfoEditMode::BrushPaint);
    }

    SECTION("LFO authority switch")
    {
        LfoPanelBrushTestAccess::setLfo(panel, 1);
    }

    SECTION("data snapshot replacement")
    {
        panel.refreshLfoDisplay();
    }

    SECTION("visibility ABA")
    {
        panel.setVisible(false);
        panel.setVisible(true);
    }

    SECTION("enablement ABA")
    {
        panel.setEnabled(false);
        panel.setEnabled(true);
    }

    SECTION("selection callback replacement")
    {
        int replacementCallbackCount = 0;
        selector.setSelectionCallback(
            [&replacementCallbackCount](LfoPresetShape)
            {
                ++replacementCallbackCount;
            });

        staleResult(static_cast<int>(LfoPresetShape::SquareLow));
        CHECK(replacementCallbackCount == 0);
        LfoPanelBrushTestAccess::restoreDefaultSelectionCallback(panel);
    }

    // The stale result must neither commit nor consume this replacement
    // session. A subsequent current result must still work synchronously.
    auto currentResult =
        LfoPanelBrushTestAccess::createPopupResultHandler(panel);
    staleResult(static_cast<int>(LfoPresetShape::SquareLow));

    CHECK(selector.getSelectedId()
          == static_cast<int>(LfoPresetShape::SawUp));
    CHECK(LfoPanelBrushTestAccess::getCurrentBrush(panel)
          == LfoPresetShape::SawUp);

    currentResult(static_cast<int>(LfoPresetShape::SineConcave));
    CHECK(selector.getSelectedId()
          == static_cast<int>(LfoPresetShape::SineConcave));
    CHECK(LfoPanelBrushTestAccess::getCurrentBrush(panel)
          == LfoPresetShape::SineConcave);

    panel.removeFromDesktop();
}

TEST_CASE("Hidden and disabled LFO brush commands cannot cross sessions",
          "[lfo][brush-selector][combo][keyboard][accessibility][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    LfoPanel panel(processor);
    prepareBrushPanel(panel);
    auto& selector = LfoPanelBrushTestAccess::getSelector(panel);
    auto* accessibility = selector.getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);

    REQUIRE(LfoPanelBrushTestAccess::keyPressed(
        panel, juce::KeyPress { juce::KeyPress::downKey }));
    CHECK(selector.getSelectedId()
          == static_cast<int>(LfoPresetShape::SawDown));
    CHECK(LfoPanelBrushTestAccess::getCurrentBrush(panel)
          == LfoPresetShape::SawDown);

    const auto exerciseRejectedCommands = [&]
    {
        REQUIRE(LfoPanelBrushTestAccess::keyPressed(
            panel, juce::KeyPress { juce::KeyPress::downKey }));
        REQUIRE(LfoPanelBrushTestAccess::keyPressed(
            panel, juce::KeyPress { juce::KeyPress::returnKey }));
        REQUIRE(accessibility->getActions().invoke(
            juce::AccessibilityActionType::press));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK_FALSE(selector.isPopupActive());
        CHECK(selector.getSelectedId()
              == static_cast<int>(LfoPresetShape::SawDown));
        CHECK(LfoPanelBrushTestAccess::getCurrentBrush(panel)
              == LfoPresetShape::SawDown);
    };

    SECTION("Point Edit")
    {
        LfoPanelBrushTestAccess::setEditMode(panel,
                                             LfoEditMode::PointEdit);
        exerciseRejectedCommands();
        LfoPanelBrushTestAccess::setEditMode(panel,
                                             LfoEditMode::BrushPaint);
    }

    SECTION("hidden panel")
    {
        panel.setVisible(false);
        exerciseRejectedCommands();
        panel.setVisible(true);
    }

    SECTION("disabled panel")
    {
        panel.setEnabled(false);
        exerciseRejectedCommands();
        panel.setEnabled(true);
    }

    REQUIRE(LfoPanelBrushTestAccess::keyPressed(
        panel, juce::KeyPress { juce::KeyPress::downKey }));
    CHECK(selector.getSelectedId()
          == static_cast<int>(LfoPresetShape::SineConvex));
    CHECK(LfoPanelBrushTestAccess::getCurrentBrush(panel)
          == LfoPresetShape::SineConvex);

    panel.removeFromDesktop();
}

TEST_CASE("LFO brush completion survives synchronous owner deletion",
          "[lfo][brush-selector][combo][popup][reentrancy][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto panel = std::make_unique<LfoPanel>(processor);
    prepareBrushPanel(*panel);

    auto* selector = &LfoPanelBrushTestAccess::getSelector(*panel);
    LfoPresetShape deliveredBrush = LfoPresetShape::SawUp;
    selector->setSelectionCallback(
        [&panel, &deliveredBrush](LfoPresetShape brush)
        {
            deliveredBrush = brush;
            panel.reset();
        });
    auto result =
        LfoPanelBrushTestAccess::createPopupResultHandler(*panel);

    result(static_cast<int>(LfoPresetShape::SquareHigh));
    CHECK(panel == nullptr);
    CHECK(deliveredBrush == LfoPresetShape::SquareHigh);
}

TEST_CASE("LFO interaction cleanup stops after synchronous owner deletion",
          "[lfo-button][lfo][ui][lifecycle][reentrancy][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto panel = std::make_unique<LfoPanel>(processor);
    panel->setBounds(0, 0, 1000, 500);
    panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel->setVisible(true);

    auto& firstButton =
        LfoPanelBrushTestAccess::getLfoSelectButton(*panel, 0);
    beginPrimaryClick(firstButton);
    REQUIRE(firstButton.isDown());

    DeletePanelOnButtonStateChange deleteListener(panel);
    firstButton.addListener(&deleteListener);
    deleteListener.armed = true;

    panel->dismissTransientInteraction();
    CHECK(panel == nullptr);
}

TEST_CASE("LFO mode switch stops after synchronous owner deletion",
          "[lfo][brush-selector][ui][lifecycle][reentrancy][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto panel = std::make_unique<LfoPanel>(processor);
    prepareBrushPanel(*panel);

    auto& selector = LfoPanelBrushTestAccess::getSelector(*panel);
    REQUIRE(selector.isVisible());

    DeletePanelOnVisibilityChange deleteListener(panel);
    selector.addComponentListener(&deleteListener);
    deleteListener.armed = true;

    LfoPanelBrushTestAccess::setEditMode(*panel,
                                         LfoEditMode::PointEdit);
    CHECK(panel == nullptr);
}

TEST_CASE("LFO panel preserves interactive layout at narrow and scaled sizes",
          "[lfo][ui][layout][resize][scale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    LfoPanel panel(processor);

    const auto checkLayout = [&](float panelScale,
                                 juce::Rectangle<int> bounds)
    {
        CAPTURE(panelScale, bounds.toString());
        panel.setScale(panelScale);
        panel.setBounds(bounds);
        LfoPanelBrushTestAccess::setEditMode(panel,
                                             LfoEditMode::BrushPaint);

        auto controls = collectVisibleInteractiveChildren(panel);
        REQUIRE(controls.size() == 16);
        for (size_t first = 0; first < controls.size(); ++first)
        {
            CAPTURE(first, controls[first]->getBounds().toString());
            REQUIRE_FALSE(controls[first]->getBounds().isEmpty());
            CHECK(panel.getLocalBounds().contains(controls[first]->getBounds()));

            for (size_t second = first + 1;
                 second < controls.size(); ++second)
            {
                CAPTURE(second, controls[second]->getBounds().toString());
                CHECK_FALSE(controls[first]->getBounds().intersects(
                    controls[second]->getBounds()));
            }
        }
    };

    checkLayout(1.0f, { 0, 0, 984, 224 });
    const auto normalEditorBounds =
        LfoPanelBrushTestAccess::getEditor(panel).getBounds();
    const auto normalTextBoxWidth =
        LfoPanelBrushTestAccess::getMotionSliders(panel)[0]->getTextBoxWidth();
    const auto normalTextBoxHeight =
        LfoPanelBrushTestAccess::getMotionSliders(panel)[0]->getTextBoxHeight();

    // This is the same logical workspace at a 50% host scale. Fixed pixel
    // minima used to consume most of the editor, while unscaled Slider text
    // boxes left almost no rotary hit area.
    checkLayout(0.5f, { 0, 0, 492, 112 });
    const auto scaledEditorBounds =
        LfoPanelBrushTestAccess::getEditor(panel).getBounds();
    const auto scaledSliders =
        LfoPanelBrushTestAccess::getMotionSliders(panel);
    CHECK(scaledEditorBounds.getWidth()
          >= juce::roundToInt(normalEditorBounds.getWidth() * 0.45f));
    CHECK(scaledEditorBounds.getHeight()
          >= juce::roundToInt(normalEditorBounds.getHeight() * 0.45f));
    for (auto* slider : scaledSliders)
    {
        CHECK(slider->getTextBoxWidth()
              <= juce::roundToInt(normalTextBoxWidth * 0.55f));
        CHECK(slider->getTextBoxHeight()
              <= juce::roundToInt(normalTextBoxHeight * 0.55f));
    }

    checkLayout(1.5f, { 0, 0, 1476, 336 });

    // Hosts can also transiently report a narrow logical width before their
    // scale callback. The centre controls wrap rather than collapsing.
    checkLayout(1.0f, { 0, 0, 640, 224 });
}

TEST_CASE("LFO resize preserves an active numeric value edit",
          "[lfo][ui][layout][resize][slider][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    setParameterValue(
        processor,
        ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0),
        0.0f);
    LfoPanel panel(processor);
    panel.setBounds(0, 0, 984, 224);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);

    auto* rateSlider =
        LfoPanelBrushTestAccess::getMotionSliders(panel)[0];
    REQUIRE(rateSlider != nullptr);
    REQUIRE(rateSlider->isTextBoxEditable());
    auto* valueLabel = findSliderTextLabel(*rateSlider);
    REQUIRE(valueLabel != nullptr);
    const auto originalTextBoxWidth = rateSlider->getTextBoxWidth();

    rateSlider->showTextBox();
    REQUIRE(valueLabel->isBeingEdited());

    panel.setScale(1.25f);
    CHECK(valueLabel->isBeingEdited());
    CHECK(findSliderTextLabel(*rateSlider) == valueLabel);
    CHECK(rateSlider->getTextBoxWidth() == originalTextBoxWidth);

    rateSlider->hideTextBox(true);
    REQUIRE_FALSE(valueLabel->isBeingEdited());
    panel.setScale(1.25f);
    CHECK(rateSlider->getTextBoxWidth() > originalTextBoxWidth);

    panel.removeFromDesktop();
}

TEST_CASE("LFO controls expose meaningful accessibility titles and help",
          "[lfo][ui][accessibility][slider][brush-selector]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    LfoPanel panel(processor);
    panel.setBounds(0, 0, 984, 224);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);

    const std::array<juce::String, 5> sliderTitles {
        "LFO rate",
        "Horizontal grid divisions",
        "Vertical grid divisions",
        "LFO smoothness",
        "LFO phase"
    };
    const auto sliders = LfoPanelBrushTestAccess::getAllSliders(panel);
    for (size_t index = 0; index < sliders.size(); ++index)
    {
        CAPTURE(index);
        auto* slider = sliders[index];
        REQUIRE(slider != nullptr);
        CHECK(slider->getTitle() == sliderTitles[index]);
        REQUIRE_FALSE(slider->getTooltip().isEmpty());

        auto* accessibility = slider->getAccessibilityHandler();
        REQUIRE(accessibility != nullptr);
        CHECK(accessibility->getTitle() == sliderTitles[index]);
        CHECK(accessibility->getHelp() == slider->getTooltip());
        CHECK(accessibility->getRole() == juce::AccessibilityRole::slider);
    }

    auto& selector = LfoPanelBrushTestAccess::getSelector(panel);
    CHECK(selector.getTitle() == "LFO brush shape");
    REQUIRE_FALSE(selector.getTooltip().isEmpty());
    auto* selectorAccessibility = selector.getAccessibilityHandler();
    REQUIRE(selectorAccessibility != nullptr);
    CHECK(selectorAccessibility->getTitle() == "LFO brush shape");
    CHECK(selectorAccessibility->getHelp() == selector.getTooltip());
    CHECK(selectorAccessibility->getRole()
          == juce::AccessibilityRole::comboBox);

    panel.removeFromDesktop();
}
