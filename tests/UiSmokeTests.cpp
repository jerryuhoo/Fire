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

namespace
{
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

void setParameterValue(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
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

TEST_CASE("Fire settings dialog uses the shared visual language", "[ui][smoke]")
{
    FireAudioProcessor processor;
    SettingsComponent settings(processor.getAppSettings());
    settings.setBounds(0, 0, 420, 300);

    const auto image = settings.createComponentSnapshot(settings.getLocalBounds(), true, 1.0f);
    checkRenderedEditor(image, 420, 300);
    writeSnapshotIfRequested(image, "fire-settings.png");
}
