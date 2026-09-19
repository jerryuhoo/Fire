#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <GUI/FireTheme.h>
#include <GUI/SettingsComponent.h>
#include <Panels/ControlPanel/Graph Components/GraphTemplate.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <vector>

struct LfoPanelDialogTestAccess final
{
    static void setDialog(LfoPanel& panel, juce::DialogWindow* dialog)
    {
        panel.modulationMatrixDialog = dialog;
    }

    static void showDialog(LfoPanel& panel)
    {
        panel.showModulationMatrixDialog();
    }

    static juce::DialogWindow* getDialog(LfoPanel& panel)
    {
        return panel.modulationMatrixDialog.getComponent();
    }

    static void setDialogFactory(LfoPanel& panel,
                                 std::function<juce::DialogWindow*()> factory)
    {
        panel.modulationMatrixDialogFactoryForTesting = std::move(factory);
    }

    static void configureDialog(
        LfoPanel& panel,
        juce::DialogWindow::LaunchOptions& launchOptions)
    {
        panel.configureModulationMatrixDialog(launchOptions);
    }

    static void configureDialogResizeLimits(juce::DialogWindow& dialog)
    {
        LfoPanel::configureModulationMatrixDialogResizeLimits(dialog);
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

    static void configureSettingsResizeLimits(juce::DialogWindow& dialog)
    {
        state::StateComponent::configureSettingsDialogResizeLimits(dialog);
    }
};

struct EditorHiddenSessionTestAccess final
{
    static std::uint64_t cleanupCount(
        const FireAudioProcessorEditor& editor) noexcept
    {
        return editor.hiddenUiCleanupCountForTesting;
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

void collectGraphTemplates(juce::Component& root,
                           std::vector<GraphTemplate*>& graphs)
{
    if (auto* graph = dynamic_cast<GraphTemplate*>(&root))
        graphs.push_back(graph);

    for (auto* child : root.getChildren())
        if (child != nullptr)
            collectGraphTemplates(*child, graphs);
}

juce::DialogWindow* createModulationMatrixDialog(
    FireAudioProcessor& processor,
    juce::Component* parent = nullptr)
{
    auto* dialog = new juce::DialogWindow(
        "Modulation Matrix", fire::ui::colours::canvas, true, false);
    dialog->setContentOwned(new ModulationMatrixPanel(processor), false);
    dialog->setBounds(0, 0, 800, 400);
    if (parent != nullptr)
        parent->addAndMakeVisible(dialog);
    dialog->enterModalState(false, nullptr, true);
    return dialog;
}

juce::DialogWindow* installModulationMatrixDialog(
    LfoPanel& panel,
    FireAudioProcessor& processor,
    juce::Component* parent = nullptr)
{
    auto* dialog = createModulationMatrixDialog(processor, parent);
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

struct HistorySourceAtShow final : juce::ComponentListener
{
    explicit HistorySourceAtShow(FireAudioProcessor& processorToObserve)
        : processor(processorToObserve)
    {
    }

    void componentVisibilityChanged(juce::Component& component) override
    {
        if (component.isVisible())
            sourceWhenShown = processor.getHistorySourceToken() & 0x7u;
    }

    FireAudioProcessor& processor;
    std::uint64_t sourceWhenShown = 99u;
};

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

TEST_CASE("Fire rotary value arcs preserve their configured solid colour",
          "[ui][theme][rotary][render][colour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    const juce::Colour accent { 0xff26d5cf };

    auto renderDial = [&](bool drive)
    {
        juce::Slider slider;
        slider.setLookAndFeel(&lookAndFeel);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRange(0.0, 1.0);
        slider.setValue(0.82, juce::dontSendNotification);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setComponentID(drive ? "drive" : "ordinary");
        slider.setBounds(0, 0, 260, 260);

        lookAndFeel.reductionPercent = drive ? 0.52f : 1.0f;
        juce::Image image(juce::Image::ARGB, 260, 260, true);
        juce::Graphics graphics(image);
        slider.paintEntireComponent(graphics, true);
        slider.setLookAndFeel(nullptr);
        return image;
    };

    for (const bool drive : { false, true })
    {
        const auto image = renderDial(drive);
        int accentPixels = 0;
        int warmGradientPixels = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto pixel = image.getPixelAt(x, y);
                if (pixel.getAlpha() < 96)
                    continue;

                if (pixel.getGreen() > pixel.getRed() + 55
                    && pixel.getBlue() > pixel.getRed() + 55)
                    ++accentPixels;

                if (pixel.getRed() > pixel.getGreen() + 45
                    && pixel.getGreen() > pixel.getBlue() + 28)
                    ++warmGradientPixels;
            }

        CAPTURE(drive, accentPixels, warmGradientPixels);
        CHECK(accentPixels > 80);
        CHECK(warmGradientPixels == 0);
    }
}

TEST_CASE("LFO bank palette preserves its index contracts",
          "[ui][theme][lfo][colour]")
{
    std::set<juce::uint32> distinctColours;
    for (int index = 0; index < fire::ui::lfoBankCount; ++index)
    {
        const auto colour = fire::ui::lfoBankColour(index);
        CHECK(colour == fire::ui::lfoBankColours[static_cast<size_t>(index)]);
        CHECK(fire::ui::lfoBankColourForSource(index + 1) == colour);
        distinctColours.insert(colour.getARGB());
    }

    CHECK(distinctColours.size()
          == static_cast<size_t>(fire::ui::lfoBankCount));
    CHECK(fire::ui::lfoBankColour(-1) == fire::ui::colours::disabled);
    CHECK(fire::ui::lfoBankColour(fire::ui::lfoBankCount)
          == fire::ui::colours::disabled);
    CHECK(fire::ui::lfoBankColourForSource(0)
          == fire::ui::colours::disabled);
    CHECK(fire::ui::lfoBankColourForSource(
              fire::ui::lfoBankCount + 1)
          == fire::ui::colours::disabled);
    CHECK(fire::ui::lfoBankColourForSource(std::numeric_limits<int>::min())
          == fire::ui::colours::disabled);
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

TEST_CASE("Editor publishes the selected history source before its graphs appear",
          "[ui][history][workspace][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    setParameterValue(processor, NUM_BANDS_ID, 3.0f);
    for (int divider = 0; divider < 2; ++divider)
        setParameterValue(processor,
                          ParameterIDAndName::getIDString(LINE_STATE_ID, divider),
                          1.0f);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    auto* multiband = findComponentOfType<Multiband>(*editor);
    auto* bandPanel = findComponentOfType<BandPanel>(*editor);
    auto* globalPanel = findComponentOfType<GlobalPanel>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(bandPanel != nullptr);
    REQUIRE(globalPanel != nullptr);

    constexpr std::uint64_t sourceMask = 0x7u;
    CHECK((processor.getHistorySourceToken() & sourceMask) == 0u);

    multiband->setFocusIndex(2);
    REQUIRE(multiband->getFocusIndex() == 2);
    CHECK((processor.getHistorySourceToken() & sourceMask) == 2u);

    const auto bandTwoToken = processor.getHistorySourceToken();
    selectWorkspace(*editor, "MOD FORGE");
    CHECK(processor.getHistorySourceToken() == bandTwoToken);

    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    HistorySourceAtShow globalShow(processor);
    globalPanel->addComponentListener(&globalShow);
    const juce::ScopeGuard removeGlobalShowListener {
        [&] { globalPanel->removeComponentListener(&globalShow); }
    };
    selectWorkspace(*editor, "MASTER LAB");
    CHECK(globalShow.sourceWhenShown == 4u);
    CHECK((processor.getHistorySourceToken() & sourceMask) == 4u);

    // A host topology reduction can clamp focus while the band view is
    // hidden.  It must not steal the global graph source, but the clamped
    // band must be ready before BAND LAB becomes visible again.
    setParameterValue(processor, NUM_BANDS_ID, 1.0f);
    editor->timerCallback();
    REQUIRE(multiband->getFocusIndex() == 0);
    CHECK((processor.getHistorySourceToken() & sourceMask) == 4u);

    HistorySourceAtShow bandShow(processor);
    bandPanel->addComponentListener(&bandShow);
    const juce::ScopeGuard removeBandShowListener {
        [&] { bandPanel->removeComponentListener(&bandShow); }
    };
    selectWorkspace(*editor, "BAND LAB");
    CHECK(bandShow.sourceWhenShown == 0u);
    CHECK((processor.getHistorySourceToken() & sourceMask) == 0u);

    editor->setVisible(false);
    processor.setHistoryArray(FireAudioProcessor::globalHistorySourceIndex);
    REQUIRE((processor.getHistorySourceToken() & sourceMask) == 4u);
    editor->setVisible(true);
    CHECK((processor.getHistorySourceToken() & sourceMask) == 0u);
}

TEST_CASE("Preset focus reset is consumed by state synchronisation, not a later header click",
          "[ui][preset][focus][workspace][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    setParameterValue(processor, NUM_BANDS_ID, 3.0f);
    for (int divider = 0; divider < 2; ++divider)
        setParameterValue(processor,
                          ParameterIDAndName::getIDString(LINE_STATE_ID,
                                                          divider),
                          1.0f);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    auto* multiband = findComponentOfType<Multiband>(*editor);
    auto* presetState =
        findComponentOfType<state::StateComponent>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(presetState != nullptr);

    multiband->setFocusIndex(2);
    REQUIRE(multiband->getFocusIndex() == 2);
    presetState->requestFocusResetAfterStateLoad();

    // A workspace click can arrive before the asynchronous processor change
    // notification. It must not consume the preset-load coordination state.
    selectWorkspace(*editor, "MOD FORGE");
    CHECK(multiband->getFocusIndex() == 2);

    editor->changeListenerCallback(&processor);
    CHECK(multiband->getFocusIndex() == 0);

    // Once the matching state synchronisation has consumed the request,
    // unrelated header commands preserve the user's current band.
    multiband->setFocusIndex(2);
    REQUIRE(multiband->getFocusIndex() == 2);
    selectWorkspace(*editor, "MASTER LAB");
    CHECK(multiband->getFocusIndex() == 2);
}

TEST_CASE("Editor scale reaches every embedded control-panel graph",
          "[ui][graph][scale][layout][regression]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);

    struct ScaleCase
    {
        int width;
        int height;
        float expectedScale;
    };
    constexpr std::array scaleCases {
        ScaleCase { 1000, 500, 1.0f },
        ScaleCase { 1500, 750, 1.5f },
        ScaleCase { 2000, 1000, 2.0f }
    };

    for (const auto& scaleCase : scaleCases)
    {
        DYNAMIC_SECTION(scaleCase.width << "x" << scaleCase.height)
        {
            editor->setBounds(0, 0, scaleCase.width, scaleCase.height);

            std::vector<GraphTemplate*> graphs;
            collectGraphTemplates(*editor, graphs);
            REQUIRE(graphs.size() >= 7);
            for (const auto* graph : graphs)
            {
                REQUIRE(graph != nullptr);
                CHECK(graph->getScale()
                      == Catch::Approx(scaleCase.expectedScale));
            }
        }
    }
}

TEST_CASE("Fire multiband selection renders after adding dividers", "[ui][smoke]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);

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
    setParameterValue(processor, NUM_BANDS_ID, 4.0f);

    editor->timerCallback();

    auto* multiband = findComponentOfType<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    int visibleDividerCount = 0;
    for (auto* child : multiband->getChildren())
        if (dynamic_cast<FreqDividerGroup*>(child) != nullptr
            && child->isVisible())
            ++visibleDividerCount;
    REQUIRE(visibleDividerCount == 3);

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
    SECTION("production launch options adapt without crossing the layout floor")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        LfoPanel panel(processor);
        panel.setBounds(0, 0, 700, 360);

        juce::DialogWindow::LaunchOptions launchOptions;
        LfoPanelDialogTestAccess::configureDialog(panel, launchOptions);
        auto* content = dynamic_cast<ModulationMatrixPanel*>(
            launchOptions.content.get());
        REQUIRE(content != nullptr);

        CHECK(launchOptions.dialogTitle == "Modulation Matrix");
        CHECK(launchOptions.dialogBackgroundColour == fire::ui::colours::canvas);
        CHECK(launchOptions.useNativeTitleBar);
        CHECK(launchOptions.escapeKeyTriggersCloseButton);
        CHECK(launchOptions.componentToCentreAround == &panel);
        CHECK(launchOptions.resizable);
        CHECK(content->getWidth()
              >= ModulationMatrixPanel::minimumContentWidth);
        CHECK(content->getHeight()
              >= ModulationMatrixPanel::minimumContentHeight);
        CHECK(content->getWidth()
              <= ModulationMatrixPanel::preferredContentWidth);
        CHECK(content->getHeight()
              <= ModulationMatrixPanel::preferredContentHeight);
        CHECK(content->getWidth()
              < ModulationMatrixPanel::preferredContentWidth);
        CHECK(content->getWidth() <= panel.getWidth());
    }

    SECTION("shared dialog resize floor preserves the complete matrix layout")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        while (processor.getLfoManager()
                   .getModulationRoutingStateSnapshot()
                   .routings.size() < 5)
        {
            const auto routingState = processor.getLfoManager()
                                          .getModulationRoutingStateSnapshot();
            const auto result = processor.getLfoManager()
                                    .addEmptyModulationRoutingIfRevisionMatches(
                                        routingState.revision);
            REQUIRE(result.accepted);
            REQUIRE(result.changed);
        }

        juce::Component host;
        host.setBounds(0, 0, 1000, 500);
        host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        REQUIRE(host.getPeer() != nullptr);
        host.setVisible(true);
        REQUIRE(host.isShowing());

        LfoPanel panel(processor);
        REQUIRE(panel.getNumChildComponents() > 0);
        host.addAndMakeVisible(panel);
        panel.setBounds(host.getLocalBounds());
        REQUIRE(panel.getPeer() != nullptr);
        REQUIRE(panel.isShowing());

        auto* dialog = createModulationMatrixDialog(processor, &host);
        REQUIRE(dialog != nullptr);
        LfoPanelDialogTestAccess::configureDialogResizeLimits(*dialog);
        LfoPanelDialogTestAccess::setDialog(panel, dialog);
        REQUIRE(dialog->isResizable());
        REQUIRE(dialog->getConstrainer() != nullptr);
        auto* content = dynamic_cast<ModulationMatrixPanel*>(
            dialog->getContentComponent());
        REQUIRE(content != nullptr);

        dialog->setBoundsConstrained({ dialog->getX(),
                                       dialog->getY(),
                                       40,
                                       40 });
        CHECK(content->getWidth()
              >= ModulationMatrixPanel::minimumContentWidth);
        CHECK(content->getHeight()
              >= ModulationMatrixPanel::minimumContentHeight);

        auto* header = findComponentOfType<ModulationMatrixHeader>(*content);
        auto* viewport = findComponentOfType<juce::Viewport>(*content);
        auto* row = findComponentOfType<ModulationMatrixRow>(*content);
        auto* addButton = findButtonWithText(*content, "Add routing");
        auto* closeButton = findButtonWithText(*content, "Close");
        REQUIRE(header != nullptr);
        REQUIRE(viewport != nullptr);
        REQUIRE(row != nullptr);
        REQUIRE(addButton != nullptr);
        CHECK(closeButton == nullptr);
        CHECK_FALSE(header->getBounds().isEmpty());
        CHECK(viewport->getHeight() > 0);
        CHECK(viewport->getVerticalScrollBar().isVisible());
        CHECK_FALSE(viewport->getHorizontalScrollBar().isVisible());
        CHECK_FALSE(row->getBounds().isEmpty());
        CHECK_FALSE(addButton->getBounds().isEmpty());
        std::vector<juce::Rectangle<int>> controlBounds;
        for (auto* child : row->getChildren())
        {
            REQUIRE(child != nullptr);
            CHECK_FALSE(child->getBounds().isEmpty());
            CHECK(row->getLocalBounds().contains(child->getBounds()));
            for (const auto& previous : controlBounds)
                CHECK_FALSE(previous.intersects(child->getBounds()));
            controlBounds.push_back(child->getBounds());
        }

        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        panel.dismissModulationMatrixDialog();
        CHECK(safeDialog == nullptr);
        host.removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

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

    SECTION("direct panel disable")
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

        panel.setEnabled(false);

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()
                  ->getNumModalComponents()
              == 0);
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

    SECTION("host editor disable")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        selectWorkspace(*editor, "MOD FORGE");

        auto* lfoPanel = findComponentOfType<LfoPanel>(*editor);
        REQUIRE(lfoPanel != nullptr);
        auto* dialog = installModulationMatrixDialog(
            *lfoPanel, processor, editor.get());
        REQUIRE(dialog != nullptr);
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        juce::Component::SafePointer<juce::Component> safeContent(
            dialog->getContentComponent());

        editor->setEnabled(false);

        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()
                  ->getNumModalComponents()
              == 0);
        editor->removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("title-bar close can reopen before deferred deletion")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        selectWorkspace(*editor, "MOD FORGE");

        auto* lfoPanel = findComponentOfType<LfoPanel>(*editor);
        REQUIRE(lfoPanel != nullptr);
        auto* oldDialog = installModulationMatrixDialog(
            *lfoPanel, processor, editor.get());
        REQUIRE(oldDialog != nullptr);
        REQUIRE(oldDialog->isShowing());
        REQUIRE(oldDialog->isCurrentlyModal(false));
        juce::Component::SafePointer<juce::DialogWindow> safeOldDialog(
            oldDialog);
        juce::Component::SafePointer<juce::Component> safeOldContent(
            oldDialog->getContentComponent());
        int factoryCalls = 0;
        LfoPanelDialogTestAccess::setDialogFactory(
            *lfoPanel,
            [&processor, &factoryCalls, parent = editor.get()]
            {
                ++factoryCalls;
                return createModulationMatrixDialog(processor, parent);
            });

        LfoPanelDialogTestAccess::showDialog(*lfoPanel);
        CHECK(LfoPanelDialogTestAccess::getDialog(*lfoPanel) == oldDialog);
        CHECK(factoryCalls == 0);

        // JUCE's LaunchOptions default close button hides the modal window;
        // ModalComponentManager deletes it on a later async update.
        oldDialog->setVisible(false);
        REQUIRE(safeOldDialog != nullptr);
        REQUIRE_FALSE(oldDialog->isCurrentlyModal(false));
        LfoPanelDialogTestAccess::showDialog(*lfoPanel);

        CHECK(safeOldDialog == nullptr);
        CHECK(safeOldContent == nullptr);
        auto* newDialog = LfoPanelDialogTestAccess::getDialog(*lfoPanel);
        REQUIRE(newDialog != nullptr);
        CHECK(factoryCalls == 1);
        REQUIRE(newDialog->isShowing());
        REQUIRE(newDialog->isCurrentlyModal(false));
        REQUIRE(newDialog->isResizable());
        REQUIRE(newDialog->getConstrainer() != nullptr);
        auto* newContent = dynamic_cast<ModulationMatrixPanel*>(
            newDialog->getContentComponent());
        REQUIRE(newContent != nullptr);
        newDialog->setBoundsConstrained({ newDialog->getX(),
                                          newDialog->getY(),
                                          40,
                                          40 });
        CHECK(newContent->getWidth()
              >= ModulationMatrixPanel::minimumContentWidth);
        CHECK(newContent->getHeight()
              >= ModulationMatrixPanel::minimumContentHeight);
        juce::Component::SafePointer<juce::DialogWindow> safeNewDialog(
            newDialog);
        juce::Component::SafePointer<juce::Component> safeNewContent(
            newDialog->getContentComponent());

        lfoPanel->dismissModulationMatrixDialog();
        CHECK(safeNewDialog == nullptr);
        CHECK(safeNewContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()
                  ->getNumModalComponents()
              == 0);
        editor->removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    SECTION("dialog factory may synchronously destroy its owner")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto panel = std::make_unique<LfoPanel>(processor);
        panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel->setVisible(true);

        int factoryCalls = 0;
        juce::Component::SafePointer<juce::DialogWindow> safeReturnedDialog;
        juce::Component::SafePointer<juce::Component> safeReturnedContent;
        LfoPanelDialogTestAccess::setDialogFactory(
            *panel,
            [&]
            {
                ++factoryCalls;
                auto* dialog = createModulationMatrixDialog(processor);
                safeReturnedDialog = dialog;
                safeReturnedContent = dialog->getContentComponent();
                panel.reset();
                return dialog;
            });

        auto* panelAtLaunch = panel.get();
        REQUIRE(panelAtLaunch != nullptr);
        LfoPanelDialogTestAccess::showDialog(*panelAtLaunch);

        CHECK(factoryCalls == 1);
        CHECK(panel == nullptr);
        CHECK(safeReturnedDialog == nullptr);
        CHECK(safeReturnedContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()
                  ->getNumModalComponents()
              == 0);
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

    SECTION("editor destruction leaves no queued content callbacks")
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

        // Settings commands now complete inside their visible dialog session,
        // so teardown cannot inherit a posted command from that session.
        autoUpdateToggle->triggerClick();
        CHECK(queuedClicks == 1);
        editor.reset();
        CHECK(safeDialog == nullptr);
        CHECK(safeContent == nullptr);
        CHECK(safeToggle == nullptr);

        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(queuedClicks == 1);
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
        REQUIRE(newDialog->getConstrainer() != nullptr);
        CHECK(newDialog->getConstrainer()->getMinimumWidth()
              == SettingsComponent::minimumDialogWidth);
        CHECK(newDialog->getConstrainer()->getMinimumHeight()
              == SettingsComponent::minimumDialogHeight);
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

    SECTION("dialog factory may synchronously destroy its owner")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        auto component = std::make_unique<state::StateComponent>(
            processor.stateAB, processor.statePresets, processor.treeState);
        component->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        component->setVisible(true);

        int factoryCalls = 0;
        juce::Component::SafePointer<juce::DialogWindow> safeReturnedDialog;
        juce::Component::SafePointer<juce::Component> safeReturnedContent;
        StateComponentDialogTestAccess::setDialogFactory(
            *component,
            [&]
            {
                ++factoryCalls;
                auto* dialog = createSettingsDialog(processor);
                safeReturnedDialog = dialog;
                safeReturnedContent = dialog->getContentComponent();
                component.reset();
                return dialog;
            });

        auto* componentAtLaunch = component.get();
        REQUIRE(componentAtLaunch != nullptr);
        StateComponentDialogTestAccess::showDialog(*componentAtLaunch);

        CHECK(factoryCalls == 1);
        CHECK(component == nullptr);
        CHECK(safeReturnedDialog == nullptr);
        CHECK(safeReturnedContent == nullptr);
        CHECK(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0);
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

TEST_CASE("Hidden editor transient cleanup runs once per peer session",
          "[ui][editor][hidden][lifecycle][performance][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);

    REQUIRE_FALSE(editor->isShowing());
    const auto initialCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);

    editor->timerCallback();
    const auto firstHiddenCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);
    CHECK(firstHiddenCleanupCount == initialCleanupCount + 1);

    editor->timerCallback();
    CHECK(EditorHiddenSessionTestAccess::cleanupCount(*editor)
          == firstHiddenCleanupCount);

    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    REQUIRE(editor->isShowing());
    editor->timerCallback();
    const auto visibleCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);
    CHECK(visibleCleanupCount == firstHiddenCleanupCount);

    editor->removeFromDesktop();
    REQUIRE_FALSE(editor->isShowing());
    editor->timerCallback();
    const auto detachedCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);
    CHECK(detachedCleanupCount == visibleCleanupCount + 1);

    editor->timerCallback();
    CHECK(EditorHiddenSessionTestAccess::cleanupCount(*editor)
          == detachedCleanupCount);
}

TEST_CASE("A provisional no-peer cleanup cannot consume a later explicit hide",
          "[ui][editor][hidden][lifecycle][gesture][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);

    editor->setVisible(true);
    REQUIRE_FALSE(editor->isShowing());
    editor->timerCallback();
    const auto provisionalCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);

    editor->timerCallback();
    CHECK(EditorHiddenSessionTestAccess::cleanupCount(*editor)
          == provisionalCleanupCount);

    // This is a real lifecycle boundary even without a desktop peer.  Tests
    // and hosts can begin parameter gestures on the visible component tree,
    // so setVisible(false) must not inherit the provisional one-shot guard.
    editor->setVisible(false);
    const auto hiddenCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);
    CHECK(hiddenCleanupCount == provisionalCleanupCount + 1);

    editor->setVisible(false);
    editor->timerCallback();
    CHECK(EditorHiddenSessionTestAccess::cleanupCount(*editor)
          == hiddenCleanupCount);
}

TEST_CASE("A detached editor rearms no-peer visibility sessions",
          "[ui][editor][hidden][lifecycle][reattach][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    REQUIRE(editor->isShowing());
    editor->timerCallback();
    const auto visibleCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);

    editor->removeFromDesktop();
    REQUIRE_FALSE(editor->isShowing());
    editor->timerCallback();
    const auto detachedCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);
    CHECK(detachedCleanupCount == visibleCleanupCount + 1);

    editor->setVisible(false);
    CHECK(EditorHiddenSessionTestAccess::cleanupCount(*editor)
          == detachedCleanupCount);

    // The update-check state remains hidden after peer removal.  Component
    // visibility still starts its own provisional session and must rearm the
    // following explicit hide independently of that unrelated cached state.
    editor->setVisible(true);
    REQUIRE_FALSE(editor->isShowing());
    editor->timerCallback();
    const auto noPeerVisibleCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);
    CHECK(noPeerVisibleCleanupCount == detachedCleanupCount + 1);

    editor->setVisible(false);
    const auto finalHiddenCleanupCount =
        EditorHiddenSessionTestAccess::cleanupCount(*editor);
    CHECK(finalHiddenCleanupCount == noPeerVisibleCleanupCount + 1);

    editor->timerCallback();
    CHECK(EditorHiddenSessionTestAccess::cleanupCount(*editor)
          == finalHiddenCleanupCount);
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

TEST_CASE("Settings dialog enforces its usable resize floor",
          "[ui][settings][dialog][resize][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    juce::DialogWindow dialog(
        "Settings", fire::ui::colours::canvas, true, false);
    dialog.setContentOwned(
        new SettingsComponent(processor.getAppSettings()), false);
    dialog.setBounds(0, 0, 400, 300);
    StateComponentDialogTestAccess::configureSettingsResizeLimits(dialog);

    REQUIRE(dialog.getConstrainer() != nullptr);
    CHECK(dialog.getConstrainer()->getMinimumWidth()
          == SettingsComponent::minimumDialogWidth);
    CHECK(dialog.getConstrainer()->getMinimumHeight()
          == SettingsComponent::minimumDialogHeight);

    dialog.setBoundsConstrained({ 0, 0, 40, 40 });
    CHECK(dialog.getWidth() >= SettingsComponent::minimumDialogWidth);
    CHECK(dialog.getHeight() >= SettingsComponent::minimumDialogHeight);
    auto* content = dynamic_cast<SettingsComponent*>(dialog.getContentComponent());
    REQUIRE(content != nullptr);
    CHECK(content->getWidth() >= SettingsComponent::minimumContentWidth);
    CHECK(content->getHeight() >= SettingsComponent::minimumContentHeight);
}
