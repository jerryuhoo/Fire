#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <GUI/FireTheme.h>
#include <GUI/SettingsComponent.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

struct LfoPanelDialogTestAccess final
{
    static void setDialog(LfoPanel& panel, juce::DialogWindow* dialog)
    {
        panel.modulationMatrixDialog = dialog;
    }
};

struct StateComponentDialogTestAccess final
{
    static void setDialog(state::StateComponent& component,
                          juce::DialogWindow* dialog)
    {
        component.settingsDialog = dialog;
    }

    static void showDialog(state::StateComponent& component)
    {
        component.showSettingsDialog();
    }

    static bool hasDialog(const state::StateComponent& component)
    {
        return component.settingsDialog != nullptr;
    }

    static juce::DialogWindow* getDialog(state::StateComponent& component)
    {
        return component.settingsDialog.getComponent();
    }

    static void setDialogFactory(state::StateComponent& component,
                                 std::function<juce::DialogWindow*()> factory)
    {
        component.settingsDialogFactoryForTesting = std::move(factory);
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

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers,
                                juce::Point<float> mouseDownPosition,
                                bool wasDragged)
{
    const auto now = juce::Time::getCurrentTime();
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
             mouseDownPosition,
             now,
             1,
             wasDragged };
}

juce::Image renderEditorAtSize(FireAudioProcessorEditor& editor, int width, int height)
{
    editor.setBounds(0, 0, width, height);
    return editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
}

juce::Button* findButtonWithText(juce::Component& root, const juce::String& text)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (auto* button = dynamic_cast<juce::Button*>(child);
            button != nullptr && button->getButtonText() == text)
            return button;

        if (child != nullptr)
            if (auto* nested = findButtonWithText(*child, text))
                return nested;
    }

    return nullptr;
}

juce::Slider* findSliderAttachedToLabel(juce::Component& root, const juce::String& labelText)
{
    if (auto* label = dynamic_cast<juce::Label*>(&root);
        label != nullptr && label->getText() == labelText)
        return dynamic_cast<juce::Slider*>(label->getAttachedComponent());

    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
        if (auto* child = root.getChildComponent(childIndex))
            if (auto* slider = findSliderAttachedToLabel(*child, labelText))
                return slider;

    return nullptr;
}

template <typename ComponentType>
ComponentType* findComponentOfType(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;

    for (auto* child : root.getChildren())
        if (child != nullptr)
            if (auto* match = findComponentOfType<ComponentType>(*child))
                return match;

    return nullptr;
}

juce::DialogWindow* installModulationMatrixDialog(LfoPanel& panel,
                                                   FireAudioProcessor& processor)
{
    auto* dialog = new juce::DialogWindow(
        "Modulation Matrix", fire::ui::colours::canvas, true, false);
    dialog->setContentOwned(new ModulationMatrixPanel(processor), false);
    dialog->enterModalState(false, nullptr, true);
    LfoPanelDialogTestAccess::setDialog(panel, dialog);
    return dialog;
}

juce::DialogWindow* createSettingsDialog(FireAudioProcessor& processor,
                                          juce::Component* parent = nullptr)
{
    auto* dialog = new juce::DialogWindow(
        "Settings", fire::ui::colours::canvas, true, false);
    dialog->setContentOwned(
        new SettingsComponent(processor.getAppSettings()), false);
    dialog->setBounds(0, 0, 400, 300);
    if (parent != nullptr)
        parent->addAndMakeVisible(dialog);
    dialog->enterModalState(false, nullptr, true);
    return dialog;
}

juce::DialogWindow* installSettingsDialog(state::StateComponent& component,
                                           FireAudioProcessor& processor,
                                           juce::Component* parent = nullptr)
{
    auto* dialog = createSettingsDialog(processor, parent);
    StateComponentDialogTestAccess::setDialog(component, dialog);
    return dialog;
}

void setParameterValue(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void checkLfoSelectionClosesSliderGesture(const juce::String& labelText,
                                          const juce::String& parameterBase)
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    for (int lfoIndex = 0; lfoIndex < 2; ++lfoIndex)
        setParameterValue(
            processor,
            ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, lfoIndex),
            0.0f);

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    panel.setVisible(true);
    auto* slider = dynamic_cast<PrimarySlider*>(
        findSliderAttachedToLabel(panel, labelText));
    auto* lfoTwoButton = findButtonWithText(panel, "LFO 2");
    const auto oldParameterID =
        ParameterIDAndName::getIDString(parameterBase, 0);
    const auto newParameterID =
        ParameterIDAndName::getIDString(parameterBase, 1);
    auto* oldParameter = processor.treeState.getParameter(oldParameterID);
    auto* newParameter = processor.treeState.getParameter(newParameterID);
    REQUIRE(slider != nullptr);
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

    const auto position = slider->getLocalBounds().toFloat().getCentre();
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    slider->mouseDown(makeMouseEvent(
        *slider, position, primary, position, false));
    REQUIRE(oldRecorder.gestures == std::vector<bool> { true });
    REQUIRE(newRecorder.gestures.empty());

    lfoTwoButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    CHECK_FALSE(slider->hasActivePointerGesture());
    CHECK(oldRecorder.gestures == std::vector<bool> { true, false });
    CHECK(newRecorder.gestures.empty());

    // The physical release from the old interaction must not be delivered to
    // the newly attached LFO parameter.
    slider->mouseUp(makeMouseEvent(
        *slider, position, {}, position, false));
    CHECK(oldRecorder.gestures == std::vector<bool> { true, false });
    CHECK(newRecorder.gestures.empty());

    slider->mouseDown(makeMouseEvent(
        *slider, position, primary, position, false));
    slider->mouseUp(makeMouseEvent(
        *slider, position, {}, position, false));
    CHECK(oldRecorder.gestures == std::vector<bool> { true, false });
    CHECK(newRecorder.gestures == std::vector<bool> { true, false });
}

void selectWorkspace(juce::Component& editor, const juce::String& buttonText)
{
    auto* button = findButtonWithText(editor, buttonText);
    REQUIRE(button != nullptr);
    button->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    REQUIRE(button->getToggleState());

    const std::array<juce::String, 3> workspaceNames {
        "BAND LAB", "MOD FORGE", "MASTER LAB"
    };
    for (const auto& otherText : workspaceNames)
        if (otherText != buttonText)
        {
            auto* otherButton = findButtonWithText(editor, otherText);
            REQUIRE(otherButton != nullptr);
            CHECK_FALSE(otherButton->getToggleState());
        }
}

std::uint64_t contentFingerprint(const juce::Image& image)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (int y = image.getHeight() / 2; y < image.getHeight(); y += 3)
        for (int x = 0; x < image.getWidth(); x += 3)
        {
            hash ^= image.getPixelAt(x, y).getARGB();
            hash *= 1099511628211ull;
        }
    return hash;
}

void checkRenderedEditor(const juce::Image& image, int expectedWidth, int expectedHeight)
{
    REQUIRE_FALSE(image.isNull());
    CHECK(image.getWidth() == expectedWidth);
    CHECK(image.getHeight() == expectedHeight);

    std::set<juce::uint32> sampledColours;
    for (int y = 0; y < image.getHeight(); y += juce::jmax(1, image.getHeight() / 25))
        for (int x = 0; x < image.getWidth(); x += juce::jmax(1, image.getWidth() / 40))
            sampledColours.insert(image.getPixelAt(x, y).getARGB());

    // A blank/transparent editor produces one colour. The themed editor has
    // several cached surfaces even before audio starts flowing.
    CHECK(sampledColours.size() >= 8);
}

void writeSnapshotIfRequested(const juce::Image& image, const juce::String& fileName)
{
    const auto outputDirectory = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
    if (outputDirectory.isEmpty())
        return;

    const auto outputFile = juce::File(outputDirectory).getChildFile(fileName);
    if (outputFile.existsAsFile())
        REQUIRE(outputFile.deleteFile());

    auto stream = outputFile.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->openedOk());

    juce::PNGImageFormat png;
    CHECK(png.writeImageToStream(image, *stream));
}
} // namespace

TEST_CASE("Safe Drive dial maps the DSP reduction ratio to the requested value",
          "[ui][theme]")
{
    const auto reduced = fire::ui::calculateDialArcState(0.8f, 0.5f, true);
    CHECK(reduced.requestedProportion == Catch::Approx(0.8f));
    CHECK(reduced.effectiveProportion == Catch::Approx(0.4f));
    CHECK(reduced.hasReduction());

    const auto unrestricted = fire::ui::calculateDialArcState(0.8f, 1.0f, true);
    CHECK(unrestricted.effectiveProportion == Catch::Approx(0.8f));
    CHECK_FALSE(unrestricted.hasReduction());

    const auto ordinaryKnob = fire::ui::calculateDialArcState(0.8f, 0.5f, false);
    CHECK(ordinaryKnob.effectiveProportion == Catch::Approx(0.8f));
    CHECK_FALSE(ordinaryKnob.hasReduction());

    const auto ordinaryStroke = fire::ui::dialArcStroke(60.0f, 1.0f, false);
    const auto driveStroke = fire::ui::dialArcStroke(60.0f, 1.0f, true);
    CHECK(driveStroke > ordinaryStroke * 2.0f);
}

TEST_CASE("Fire editor renders at supported scale extremes", "[ui][smoke]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);

    SECTION("minimum editor size")
    {
        const auto image = renderEditorAtSize(*editor, 1000, 500);
        checkRenderedEditor(image, 1000, 500);
        writeSnapshotIfRequested(image, "fire-editor-1000x500.png");
    }

    SECTION("maximum editor size")
    {
        const auto image = renderEditorAtSize(*editor, 2000, 1000);
        checkRenderedEditor(image, 2000, 1000);
        writeSnapshotIfRequested(image, "fire-editor-2000x1000.png");
    }

    SECTION("modulation and master workspaces")
    {
        editor->setBounds(0, 0, 1000, 500);
        const auto bandImage = renderEditorAtSize(*editor, 1000, 500);

        selectWorkspace(*editor, "MOD FORGE");
        const auto modulationImage = renderEditorAtSize(*editor, 1000, 500);
        checkRenderedEditor(modulationImage, 1000, 500);
        writeSnapshotIfRequested(modulationImage, "fire-editor-mod-forge.png");

        selectWorkspace(*editor, "MASTER LAB");
        const auto masterImage = renderEditorAtSize(*editor, 1000, 500);
        checkRenderedEditor(masterImage, 1000, 500);
        writeSnapshotIfRequested(masterImage, "fire-editor-master-lab.png");

        CHECK(contentFingerprint(bandImage) != contentFingerprint(modulationImage));
        CHECK(contentFingerprint(bandImage) != contentFingerprint(masterImage));
        CHECK(contentFingerprint(modulationImage) != contentFingerprint(masterImage));
    }
}

TEST_CASE("Fire multiband selection renders after adding dividers", "[ui][smoke]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    constexpr std::array<float, 3> crossoverFrequencies { 200.0f, 1200.0f, 6000.0f };
    for (int index = 0; index < static_cast<int>(crossoverFrequencies.size()); ++index)
    {
        setParameterValue(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, index),
                          crossoverFrequencies[static_cast<size_t>(index)]);
        setParameterValue(processor,
                          ParameterIDAndName::getIDString(LINE_STATE_ID, index),
                          1.0f);
    }

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    const auto image = renderEditorAtSize(*editor, 1000, 500);
    checkRenderedEditor(image, 1000, 500);
    writeSnapshotIfRequested(image, "fire-editor-multiband.png");
}

TEST_CASE("LFO Rate text entry is available only in Free Hz mode",
          "[ui][lfo][rate][sync]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    const auto syncModeID = ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0);
    setParameterValue(processor, syncModeID, 1.0f);

    LfoPanel lfoPanel(processor);
    lfoPanel.setBounds(0, 0, 1000, 500);

    auto* rateSlider = findSliderAttachedToLabel(lfoPanel, "Rate");
    REQUIRE(rateSlider != nullptr);
    REQUIRE(rateSlider->getTextBoxPosition() == juce::Slider::TextBoxBelow);
    CHECK_FALSE(rateSlider->isTextBoxEditable());
    CHECK(rateSlider->getValue() == Catch::Approx(8.0));
    CHECK(rateSlider->getTextFromValue(8.0) == "1/4");

    const auto syncRateID = ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, 0);
    const auto* syncRate = processor.treeState.getRawParameterValue(syncRateID);
    REQUIRE(syncRate != nullptr);
    rateSlider->setValue(6.0, juce::sendNotificationSync);
    CHECK(syncRate->load() == Catch::Approx(6.0f));
    CHECK(rateSlider->getTextFromValue(rateSlider->getValue()) == "1/8");

    setParameterValue(processor, syncModeID, 0.0f);
    lfoPanel.animationTick();

    CHECK(rateSlider->isTextBoxEditable());

    const auto freeRateID = ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0);
    const auto* freeRate = processor.treeState.getRawParameterValue(freeRateID);
    REQUIRE(freeRate != nullptr);
    rateSlider->setValue(2.75, juce::sendNotificationSync);
    CHECK(freeRate->load() == Catch::Approx(2.75f));

    setParameterValue(processor, syncModeID, 1.0f);
    lfoPanel.animationTick();

    CHECK_FALSE(rateSlider->isTextBoxEditable());
    CHECK(rateSlider->getValue() == Catch::Approx(6.0));
    CHECK(rateSlider->getTextFromValue(rateSlider->getValue()) == "1/8");
}

TEST_CASE("LFO Rate defers Sync attachment changes until the active host gesture ends",
          "[ui][lfo][rate][sync][gesture]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    const auto syncModeID = ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0);
    const auto syncRateID = ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, 0);
    const auto freeRateID = ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0);
    setParameterValue(processor, syncModeID, 0.0f);

    LfoPanel lfoPanel(processor);
    lfoPanel.setBounds(0, 0, 1000, 500);

    auto* rateSlider = findSliderAttachedToLabel(lfoPanel, "Rate");
    auto* freeRate = processor.treeState.getParameter(freeRateID);
    auto* syncRate = processor.treeState.getParameter(syncRateID);
    REQUIRE(rateSlider != nullptr);
    REQUIRE(freeRate != nullptr);
    REQUIRE(syncRate != nullptr);

    ParameterGestureRecorder freeRecorder;
    ParameterGestureRecorder syncRecorder;
    freeRate->addListener(&freeRecorder);
    syncRate->addListener(&syncRecorder);

    const auto downPosition = rateSlider->getLocalBounds().toFloat().getCentre();
    rateSlider->mouseDown(makeMouseEvent(
        *rateSlider,
        downPosition,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
        downPosition,
        false));
    REQUIRE(freeRecorder.gestures == std::vector<bool> { true });

    // Model host automation changing Sync while the user is still dragging.
    // The visible knob must finish its old Free-Hz gesture before rebinding.
    setParameterValue(processor, syncModeID, 1.0f);
    lfoPanel.animationTick();
    CHECK(rateSlider->isTextBoxEditable());
    CHECK(syncRecorder.gestures.empty());

    rateSlider->setValue(2.75, juce::sendNotificationSync);
    CHECK(processor.treeState.getRawParameterValue(freeRateID)->load()
          == Catch::Approx(2.75f));

    rateSlider->mouseUp(makeMouseEvent(
        *rateSlider,
        downPosition,
        juce::ModifierKeys {},
        downPosition,
        false));

    REQUIRE(freeRecorder.gestures == std::vector<bool> { true, false });
    CHECK(syncRecorder.gestures.empty());

    lfoPanel.animationTick();
    CHECK_FALSE(rateSlider->isTextBoxEditable());
    CHECK(freeRecorder.gestures == std::vector<bool> { true, false });
    CHECK(syncRecorder.gestures.empty());

    freeRate->removeListener(&freeRecorder);
    syncRate->removeListener(&syncRecorder);
}

TEST_CASE("LFO selection closes old Slider gestures before rebinding attachments",
          "[ui][lfo][gesture][attachment][lifecycle]")
{
    checkLfoSelectionClosesSliderGesture("Rate", LFO_RATE_HZ_ID);
    checkLfoSelectionClosesSliderGesture("Smooth", LFO_SMOOTH_ID);
    checkLfoSelectionClosesSliderGesture("Phase", LFO_PHASE_ID);
}

TEST_CASE("LFO selection discards text that belongs to the old attachment",
          "[ui][lfo][text-entry][attachment][lifecycle]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    for (int lfoIndex = 0; lfoIndex < 2; ++lfoIndex)
        setParameterValue(
            processor,
            ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, lfoIndex),
            0.0f);

    const auto oldRateID =
        ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0);
    const auto newRateID =
        ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 1);
    setParameterValue(processor, oldRateID, 2.0f);
    setParameterValue(processor, newRateID, 7.0f);

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    const juce::ScopeGuard removePanelPeer {
        [&] { panel.removeFromDesktop(); }
    };
    auto* rateSlider = dynamic_cast<PrimarySlider*>(
        findSliderAttachedToLabel(panel, "Rate"));
    auto* lfoTwoButton = findButtonWithText(panel, "LFO 2");
    REQUIRE(rateSlider != nullptr);
    REQUIRE(lfoTwoButton != nullptr);
    REQUIRE(rateSlider->isTextBoxEditable());

    rateSlider->showTextBox();
    juce::Label* valueLabel = nullptr;
    for (auto* child : rateSlider->getChildren())
        if (auto* candidate = dynamic_cast<juce::Label*>(child);
            candidate != nullptr && candidate->getCurrentTextEditor() != nullptr)
        {
            valueLabel = candidate;
            break;
        }

    REQUIRE(valueLabel != nullptr);
    REQUIRE(valueLabel->isBeingEdited());
    valueLabel->getCurrentTextEditor()->setText("19.0", false);

    lfoTwoButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    CHECK_FALSE(valueLabel->isBeingEdited());
    CHECK(processor.treeState.getRawParameterValue(oldRateID)->load()
          == Catch::Approx(2.0f));
    CHECK(processor.treeState.getRawParameterValue(newRateID)->load()
          == Catch::Approx(7.0f));
    CHECK(rateSlider->getValue() == Catch::Approx(7.0));
}

TEST_CASE("LFO Slider gestures close at panel and editor lifecycle boundaries",
          "[ui][lfo][gesture][attachment][lifecycle]")
{
    SECTION("direct panel hide")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto panel = std::make_unique<LfoPanel>(processor);
        panel->setBounds(0, 0, 1000, 500);
        panel->setVisible(true);
        auto* slider = dynamic_cast<PrimarySlider*>(
            findSliderAttachedToLabel(*panel, "Smooth"));
        auto* parameter = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0));
        REQUIRE(slider != nullptr);
        REQUIRE(parameter != nullptr);
        ParameterGestureRecorder recorder;
        parameter->addListener(&recorder);
        const juce::ScopeGuard removeListener {
            [&] { parameter->removeListener(&recorder); }
        };

        const auto position = slider->getLocalBounds().toFloat().getCentre();
        slider->mouseDown(makeMouseEvent(
            *slider,
            position,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
            position,
            false));
        REQUIRE(recorder.gestures == std::vector<bool> { true });

        panel->setVisible(false);
        CHECK_FALSE(slider->hasActivePointerGesture());
        CHECK(recorder.gestures == std::vector<bool> { true, false });
    }

    SECTION("panel destruction")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto panel = std::make_unique<LfoPanel>(processor);
        panel->setBounds(0, 0, 1000, 500);
        auto* slider = dynamic_cast<PrimarySlider*>(
            findSliderAttachedToLabel(*panel, "Phase"));
        auto* parameter = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(LFO_PHASE_ID, 0));
        REQUIRE(slider != nullptr);
        REQUIRE(parameter != nullptr);
        ParameterGestureRecorder recorder;
        parameter->addListener(&recorder);
        const juce::ScopeGuard removeListener {
            [&] { parameter->removeListener(&recorder); }
        };

        const auto position = slider->getLocalBounds().toFloat().getCentre();
        slider->mouseDown(makeMouseEvent(
            *slider,
            position,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
            position,
            false));
        REQUIRE(recorder.gestures == std::vector<bool> { true });

        panel.reset();
        CHECK(recorder.gestures == std::vector<bool> { true, false });
    }

    SECTION("host editor hide")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        setParameterValue(
            processor,
            ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0),
            0.0f);
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        selectWorkspace(*editor, "MOD FORGE");

        auto* slider = dynamic_cast<PrimarySlider*>(
            findSliderAttachedToLabel(*editor, "Rate"));
        auto* parameter = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0));
        REQUIRE(slider != nullptr);
        REQUIRE(parameter != nullptr);
        ParameterGestureRecorder recorder;
        parameter->addListener(&recorder);
        const juce::ScopeGuard removeListener {
            [&] { parameter->removeListener(&recorder); }
        };

        const auto position = slider->getLocalBounds().toFloat().getCentre();
        slider->mouseDown(makeMouseEvent(
            *slider,
            position,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
            position,
            false));
        REQUIRE(recorder.gestures == std::vector<bool> { true });

        editor->setVisible(false);
        CHECK_FALSE(slider->hasActivePointerGesture());
        CHECK(recorder.gestures == std::vector<bool> { true, false });
        editor->removeFromDesktop();
    }
}

TEST_CASE("Modulation Matrix dialog closes synchronously with its owning UI",
          "[ui][lfo][matrix][dialog][lifecycle]")
{
    SECTION("direct panel hide")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        LfoPanel panel(processor);
        panel.setVisible(true);

        auto* dialog = installModulationMatrixDialog(panel, processor);
        REQUIRE(dialog != nullptr);
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        panel.setVisible(false);

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("panel destruction")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto panel = std::make_unique<LfoPanel>(processor);

        auto* dialog = installModulationMatrixDialog(*panel, processor);
        REQUIRE(dialog != nullptr);
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        panel.reset();

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("host editor hide")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        selectWorkspace(*editor, "MOD FORGE");

        auto* lfoPanel = findComponentOfType<LfoPanel>(*editor);
        REQUIRE(lfoPanel != nullptr);
        auto* dialog = installModulationMatrixDialog(*lfoPanel, processor);
        REQUIRE(dialog != nullptr);
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        editor->setVisible(false);

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        editor->removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }
}

TEST_CASE("Settings dialog closes synchronously with its owning UI",
          "[ui][settings][dialog][lifecycle]")
{
    SECTION("direct StateComponent hide")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        state::StateComponent component(
            processor.stateAB, processor.statePresets, processor.treeState);
        component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        component.setVisible(true);

        auto* dialog = installSettingsDialog(component, processor, &component);
        REQUIRE(dialog != nullptr);
        REQUIRE(dialog->isCurrentlyModal(false));
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        component.setVisible(false);

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        component.removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("StateComponent destruction")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto component = std::make_unique<state::StateComponent>(
            processor.stateAB, processor.statePresets, processor.treeState);
        component->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        component->setVisible(true);

        auto* dialog = installSettingsDialog(*component, processor, component.get());
        REQUIRE(dialog != nullptr);
        REQUIRE(dialog->isCurrentlyModal(false));
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        component.reset();

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("editor destruction cancels queued content callbacks")
    {
        auto processor = std::make_unique<FireAudioProcessor>();
        processor->hasUpdateCheckBeenPerformed = true;
        auto editor = std::make_unique<FireAudioProcessorEditor>(*processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        auto* stateComponent = findComponentOfType<state::StateComponent>(*editor);
        REQUIRE(stateComponent != nullptr);

        auto* dialog = installSettingsDialog(
            *stateComponent, *processor, editor.get());
        REQUIRE(dialog != nullptr);
        REQUIRE(dialog->isCurrentlyModal(false));
        auto* autoUpdateToggle = findButtonWithText(
            *dialog->getContentComponent(), "Auto-check for updates on startup");
        REQUIRE(autoUpdateToggle != nullptr);
        int queuedClicks = 0;
        autoUpdateToggle->onClick = [&queuedClicks] { ++queuedClicks; };
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());
        juce::Component::SafePointer<juce::Button> safeToggle(autoUpdateToggle);

        // The posted Button command must become a no-op when synchronous
        // teardown destroys the dialog's controls.
        autoUpdateToggle->triggerClick();
        CHECK(queuedClicks == 0);
        editor.reset();
        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(safeToggle == nullptr);

        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(queuedClicks == 0);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        processor.reset();
    }

    SECTION("host editor hide")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);

        auto* stateComponent = findComponentOfType<state::StateComponent>(*editor);
        REQUIRE(stateComponent != nullptr);
        auto* dialog = installSettingsDialog(
            *stateComponent, processor, editor.get());
        REQUIRE(dialog != nullptr);
        REQUIRE(dialog->isCurrentlyModal(false));
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        editor->setVisible(false);

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        editor->removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("hidden editor timer fallback")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setVisible(false);
        auto* stateComponent = findComponentOfType<state::StateComponent>(*editor);
        REQUIRE(stateComponent != nullptr);

        auto* dialog = installSettingsDialog(*stateComponent, processor);
        REQUIRE(dialog != nullptr);
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        editor->timerCallback();

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("title-bar close can reopen before deferred deletion")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        auto* stateComponent = findComponentOfType<state::StateComponent>(*editor);
        REQUIRE(stateComponent != nullptr);

        auto* oldDialog = installSettingsDialog(
            *stateComponent, processor, editor.get());
        REQUIRE(oldDialog != nullptr);
        REQUIRE(oldDialog->isCurrentlyModal(false));
        juce::Component::SafePointer<juce::DialogWindow> safeOldDialog(oldDialog);
        juce::Component::SafePointer<juce::Component> safeOldContent(
            oldDialog->getContentComponent());
        StateComponentDialogTestAccess::setDialogFactory(
            *stateComponent,
            [&processor, parent = editor.get()]
            {
                return createSettingsDialog(processor, parent);
            });

        oldDialog->setVisible(false);
        REQUIRE(safeOldDialog != nullptr);
        StateComponentDialogTestAccess::showDialog(*stateComponent);

        CHECK(safeOldDialog == nullptr);
        CHECK(safeOldContent == nullptr);
        auto* newDialog = StateComponentDialogTestAccess::getDialog(*stateComponent);
        REQUIRE(newDialog != nullptr);
        REQUIRE(newDialog->isCurrentlyModal(false));
        juce::Component::SafePointer<juce::DialogWindow> safeNewDialog(newDialog);
        juce::Component::SafePointer<juce::Component> safeNewContent(
            newDialog->getContentComponent());

        stateComponent->dismissSettingsDialog();
        CHECK(safeNewDialog == nullptr);
        CHECK(safeNewContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
        editor->removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("hidden delayed menu result cannot create a dialog")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        int factoryCalls = 0;
        state::StateComponent component(
            processor.stateAB, processor.statePresets, processor.treeState);
        StateComponentDialogTestAccess::setDialogFactory(
            component,
            [&factoryCalls]
            {
                ++factoryCalls;
                return static_cast<juce::DialogWindow*>(nullptr);
            });

        REQUIRE_FALSE(component.isShowing());
        StateComponentDialogTestAccess::showDialog(component);
        CHECK_FALSE(StateComponentDialogTestAccess::hasDialog(component));
        CHECK(factoryCalls == 0);
    }
}

TEST_CASE("Fire settings dialog uses the shared visual language", "[ui][smoke]")
{
    FireAudioProcessor processor;
    SettingsComponent settings(processor.getAppSettings());
    settings.setBounds(0, 0, 420, 300);

    const auto image = settings.createComponentSnapshot(settings.getLocalBounds(), true, 1.0f);
    checkRenderedEditor(image, 420, 300);
    writeSnapshotIfRequested(image, "fire-settings.png");
}
