#include <GUI/ModulatableSlider.h>
#include <GUI/ValuePopup.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>

namespace
{
struct EndpointCase
{
    juce::String parameterID;
    float baseValue = 0.0f;
    float depth = 0.0f;
    bool bipolar = true;
    bool bypassed = false;
};

template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* child = root.getChildComponent(index))
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;

    return nullptr;
}

ValuePopup* findValuePopup(FireAudioProcessorEditor& editor)
{
    for (auto* child : editor.getChildren())
        if (auto* popup = dynamic_cast<ValuePopup*>(child))
            return popup;

    return nullptr;
}

ModulatableSlider* findSlider(BandPanel& panel,
                              const juce::String& parameterID)
{
    for (auto* slider : panel.getModulatableSliders())
        if (slider != nullptr && slider->getParamID() == parameterID)
            return slider;

    return nullptr;
}

juce::Rectangle<int> valueDisplayBoundsInEditor(
    FireAudioProcessorEditor& editor,
    ModulatableSlider& slider)
{
    return editor.getLocalArea(
        &slider,
        slider.getValueDisplayBounds());
}

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String expectedEndpointText(FireAudioProcessor& processor,
                                  const EndpointCase& testCase)
{
    auto* parameter = processor.treeState.getParameter(testCase.parameterID);
    REQUIRE(parameter != nullptr);

    const auto baseNormalised = juce::jlimit(
        0.0f,
        1.0f,
        parameter->convertTo0to1(testCase.baseValue));
    const auto endpointNormalised = juce::jlimit(
        0.0f,
        1.0f,
        baseNormalised
            + testCase.depth * (testCase.bipolar ? 0.5f : 1.0f));
    return parameter->getText(endpointNormalised, 0);
}

juce::String displayedEndpointText(FireAudioProcessorEditor& editor,
                                   const juce::String& parameterID)
{
    ModulatableSlider slider;
    slider.parameterID = parameterID;
    slider.setBounds(0, 0, 80, 80);
    editor.addAndMakeVisible(slider);
    editor.showValuePopupForSlider(&slider);

    auto* popup = findValuePopup(editor);

    REQUIRE(popup != nullptr);
    REQUIRE(popup->getNumChildComponents() == 1);
    auto* label = dynamic_cast<juce::Label*>(popup->getChildComponent(0));
    REQUIRE(label != nullptr);
    const auto text = label->getText();
    editor.hideValuePopup();
    return text;
}

void configureRouting(FireAudioProcessor& processor,
                      const EndpointCase& testCase)
{
    processor.assignLfoToTarget(0, testCase.parameterID);
    processor.setModulationDepth(testCase.parameterID, testCase.depth);

    auto info = processor.getModulationInfoForParameter(testCase.parameterID);
    REQUIRE(info.isModulated);
    if (info.isBipolar != testCase.bipolar)
        processor.toggleBipolarMode(testCase.parameterID);
    if (testCase.bypassed)
        processor.getLfoManager().toggleBypassForRouting(testCase.parameterID);

    info = processor.getModulationInfoForParameter(testCase.parameterID);
    REQUIRE(info.isModulated);
    REQUIRE(info.isBipolar == testCase.bipolar);
    REQUIRE(info.isBypassed == testCase.bypassed);
}
} // namespace

TEST_CASE("Modulation value popup follows normalised configured endpoint math",
          "[ui][modulation][value-popup][normalised-domain]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    editor.setBounds(0, 0, 1000, 500);

    const std::array cases {
        // Frequency and compressor-time ranges are skewed, so physical-domain
        // offsets cannot reproduce the DSP endpoint.
        EndpointCase { LOWCUT_FREQ_ID, 1000.0f, 0.50f, false, false },
        EndpointCase { LOWCUT_FREQ_ID, 3200.0f, -0.40f, true, false },
        EndpointCase {
            ParameterIDAndName::getIDString(COMP_ATTACK_ID, 0),
            10.0f,
            0.60f,
            true,
            false
        },
        // Also lock negative unipolar depth on linear and lower-clamped skew
        // ranges.
        EndpointCase { OUTPUT_ID, -12.0f, -0.30f, false, false },
        EndpointCase { PEAK_FREQ_ID, 1000.0f, -0.75f, false, false },
    };

    auto* lowCutParameter =
        processor.treeState.getParameter(LOWCUT_FREQ_ID);
    REQUIRE(lowCutParameter != nullptr);
    CHECK(expectedEndpointText(processor, cases.front())
          == lowCutParameter->getText(1.0f, 0));

    for (const auto& testCase : cases)
    {
        INFO("parameter=" << testCase.parameterID
                          << " depth=" << testCase.depth
                          << " bipolar=" << testCase.bipolar
                          << " bypassed=" << testCase.bypassed);
        setPlainParameter(processor, testCase.parameterID, testCase.baseValue);
        configureRouting(processor, testCase);

        CHECK(displayedEndpointText(editor, testCase.parameterID)
              == expectedEndpointText(processor, testCase));

        processor.clearModulationForParameter(testCase.parameterID);
    }

    const auto unmodulatedParameterID =
        ParameterIDAndName::getIDString(COMP_RELEASE_ID, 0);
    constexpr float unmodulatedBase = 375.0f;
    setPlainParameter(processor, unmodulatedParameterID, unmodulatedBase);
    auto* unmodulatedParameter =
        processor.treeState.getParameter(unmodulatedParameterID);
    REQUIRE(unmodulatedParameter != nullptr);
    CHECK(displayedEndpointText(editor, unmodulatedParameterID)
          == unmodulatedParameter->getText(
              unmodulatedParameter->convertTo0to1(unmodulatedBase), 0));
}

TEST_CASE("Bypassed modulation popup preserves its editable configured endpoint",
          "[ui][modulation][value-popup][normalised-domain][bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    editor.setBounds(0, 0, 1000, 500);

    const EndpointCase testCase {
        PEAK_FREQ_ID, 1500.0f, 0.75f, false, true
    };
    setPlainParameter(processor, testCase.parameterID, testCase.baseValue);
    configureRouting(processor, testCase);

    CHECK(displayedEndpointText(editor, testCase.parameterID)
          == expectedEndpointText(processor, testCase));
}

TEST_CASE("Modulation value popup stays above the knob value display",
          "[ui][modulation][value-popup][layout][scale][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto outputID = ParameterIDAndName::getIDString(OUTPUT_ID, 0);
    REQUIRE(processor.assignLfoToTarget(0, driveID)
            == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.assignLfoToTarget(1, outputID)
            == LfoManager::AssignmentResult::changed);

    FireAudioProcessorEditor editor(processor);
    editor.setBounds(0, 0, 1000, 500);
    auto* bandPanel = findDescendant<BandPanel>(editor);
    auto* popup = findValuePopup(editor);
    REQUIRE(bandPanel != nullptr);
    REQUIRE(popup != nullptr);

    auto* drive = findSlider(*bandPanel, driveID);
    auto* output = findSlider(*bandPanel, outputID);
    REQUIRE(drive != nullptr);
    REQUIRE(output != nullptr);

    for (auto* slider : { drive, output })
    {
        editor.showValuePopupForSlider(slider);
        REQUIRE(popup->isVisible());
        const auto valueDisplay = valueDisplayBoundsInEditor(editor, *slider);
        CHECK(std::abs(popup->getBounds().getCentreX()
                       - valueDisplay.getCentreX())
              <= 1);
        CHECK(popup->getBottom() < valueDisplay.getY());
        const auto rotaryBounds = editor.getLocalArea(
            slider,
            slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds);
        CHECK_FALSE(popup->getBounds().intersects(rotaryBounds));
        CHECK(popup->getWidth() == 80);
        CHECK(popup->getHeight() == 20);
    }

    editor.showValuePopupForSlider(drive);
    editor.setBounds(0, 0, 2000, 1000);
    REQUIRE(popup->isVisible());
    const auto resizedDriveValueDisplay =
        valueDisplayBoundsInEditor(editor, *drive);
    CHECK(std::abs(popup->getBounds().getCentreX()
                   - resizedDriveValueDisplay.getCentreX())
          <= 1);
    CHECK(popup->getBottom() < resizedDriveValueDisplay.getY());
    CHECK(popup->getWidth() == 160);
    CHECK(popup->getHeight() == 40);
    REQUIRE(popup->getNumChildComponents() == 1);
    auto* popupLabel = dynamic_cast<juce::Label*>(popup->getChildComponent(0));
    REQUIRE(popupLabel != nullptr);
    CHECK(popupLabel->getFont().getHeight() == Catch::Approx(24.0f));

    editor.setBounds(0, 0, 1000, 500);
    ModulatableSlider boundarySlider;
    boundarySlider.parameterID = PEAK_FREQ_ID;
    boundarySlider.setBounds(editor.getWidth() - 40, 120, 80, 80);
    editor.addAndMakeVisible(boundarySlider);
    editor.showValuePopupForSlider(&boundarySlider);
    REQUIRE(popup->isVisible());
    const auto boundaryValueDisplay =
        valueDisplayBoundsInEditor(editor, boundarySlider);
    CHECK(popup->getBottom() < boundaryValueDisplay.getY());
    CHECK(editor.getLocalBounds().contains(popup->getBounds()));

    ModulatableSlider topBoundarySlider;
    topBoundarySlider.parameterID = PEAK_FREQ_ID;
    topBoundarySlider.setBounds(100, 0, 80, 80);
    editor.addAndMakeVisible(topBoundarySlider);
    editor.showValuePopupForSlider(&topBoundarySlider);
    CHECK_FALSE(popup->isVisible());

    ModulatableSlider invalidSlider;
    invalidSlider.parameterID = "missing_parameter";
    invalidSlider.setBounds(100, 100, 80, 80);
    editor.addAndMakeVisible(invalidSlider);
    editor.showValuePopupForSlider(&invalidSlider);
    CHECK_FALSE(popup->isVisible());

    auto transientOwner = std::make_unique<ModulatableSlider>();
    transientOwner->parameterID = PEAK_FREQ_ID;
    transientOwner->setBounds(100, 100, 80, 80);
    editor.addAndMakeVisible(*transientOwner);
    editor.showValuePopupForSlider(transientOwner.get());
    REQUIRE(popup->isVisible());
    transientOwner.reset();
    editor.resized();
    CHECK_FALSE(popup->isVisible());
}
