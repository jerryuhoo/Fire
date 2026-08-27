#include <GUI/ModulatableSlider.h>
#include <GUI/ValuePopup.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <array>

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
    editor.showValuePopupForSlider(&slider);

    ValuePopup* popup = nullptr;
    for (auto* child : editor.getChildren())
        if (auto* candidate = dynamic_cast<ValuePopup*>(child))
            popup = candidate;

    REQUIRE(popup != nullptr);
    REQUIRE(popup->getNumChildComponents() == 1);
    auto* label = dynamic_cast<juce::Label*>(popup->getChildComponent(0));
    REQUIRE(label != nullptr);
    return label->getText();
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
