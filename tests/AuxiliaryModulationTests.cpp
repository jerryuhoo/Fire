#include <PluginProcessor.h>
#include <GUI/ModulationSourceControls.h>
#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
namespace sources = fire::mod_sources;
void plain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void render(FireAudioProcessor& processor, float value, int blocks = 30)
{
    processor.setRateAndBufferSizeDetails(48000, 128); processor.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> block(2, 128); juce::MidiBuffer midi;
    for (int i = 0; i < blocks; ++i)
    {
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(block.getWritePointer(channel), channel == 0 ? value : -value, 128);
        processor.processBlock(block, midi);
    }
}
template<class T> T* find(juce::Component& root, const juce::String& id)
{
    if (auto* result = dynamic_cast<T*>(&root); result && root.getComponentID() == id) return result;
    for (auto* child : root.getChildren()) if (auto* result = find<T>(*child, id)) return result;
    return nullptr;
}
}

TEST_CASE("Input envelope uses stereo energy and retains its attack and release time constants", "[aux-mod][dsp]")
{
    fire::dsp::AuxiliaryModulation same, anti;
    same.prepare(48000); anti.prepare(48000);
    auto parameters = sources::defaults; parameters[0] = 0.1f; parameters[1] = 50;
    same.setParameters(parameters); anti.setParameters(parameters);
    float level = 0;
    for (int i = 0; i < 4800; ++i)
    {
        auto a = same.next(0.5f, 0.5f, true); auto b = anti.next(0.5f, -0.5f, true);
        REQUIRE(a[0] == b[0]); level = a[0];
    }
    CHECK(level == Catch::Approx(0.5f).margin(0.001f));
    for (int i = 0; i < 4800; ++i) level = anti.next(0, 0, true)[0];
    CHECK(level == Catch::Approx(0.5 * std::exp(-2.0)).margin(0.001));
}

TEST_CASE("Macro smoothing advances in samples and reaches its target without a step", "[aux-mod][dsp]")
{
    fire::dsp::AuxiliaryModulation processor; processor.prepare(48000);
    auto parameters = sources::defaults;
    processor.setParameters(parameters); CHECK(processor.next(0, 0, false)[1] == 0);
    parameters[3] = 1; processor.setParameters(parameters);
    float previous = 0, value = 0;
    for (int i = 0; i < 960; ++i)
    {
        value = processor.next(0, 0, false)[1];
        CHECK(value >= previous); CHECK(value - previous < 0.002f); previous = value;
    }
    CHECK(value == Catch::Approx(1));
}

TEST_CASE("New source parameters append after historical indices and macros drive several targets", "[aux-mod][state][parameters]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.getParameters().size() == 1047);
    CHECK(processor.treeState.getParameter("driveCompModern4")->getParameterIndex() == 1039);
    for (size_t index = 0; index < sources::ids.size(); ++index)
    {
        auto* parameter = processor.treeState.getParameter(sources::ids[index]); REQUIRE(parameter != nullptr);
        CHECK(parameter->getParameterIndex() == 1040 + static_cast<int>(index));
        CHECK(parameter->getVersionHint() == 11);
    }
    plain(processor, "macro1", 0.75f);
    REQUIRE(processor.assignLfoToTarget(sources::firstMacro, "drive1") == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.assignLfoToTarget(sources::firstMacro, "width1") == LfoManager::AssignmentResult::changed);
    CHECK_FALSE(processor.getModulationInfoForParameter("drive1").isBipolar);
    render(processor, 0.1f);
    CHECK(processor.getLfoManager().getModulatedValue("drive1") == Catch::Approx(37.5f).margin(0.05));
    CHECK(processor.getLfoManager().getModulatedValue("width1") == Catch::Approx(0.875f).margin(0.01));
    CHECK(processor.getModulationInfoForParameter("drive1").sourceLfoIndex == 18);
    CHECK(processor.assignLfoToTarget(sources::firstMacro, "macro1") == LfoManager::AssignmentResult::invalidRequest);
    CHECK(processor.assignLfoToTarget(sources::envelope, "envAttack") == LfoManager::AssignmentResult::invalidRequest);
}

TEST_CASE("Input envelope reaches band and master DSP and survives project and preset round trips", "[aux-mod][state][dsp]")
{
    FireAudioProcessor processor;
    plain(processor, "envAttack", 0.1f); plain(processor, "envSensitivity", 12);
    plain(processor, "macro3", 0.65f);
    REQUIRE(processor.assignLfoToTarget(sources::envelope, "drive1") == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.assignLfoToTarget(sources::envelope, "peakGain") == LfoManager::AssignmentResult::changed);
    render(processor, 0.2f);
    auto info = processor.getModulationInfoForParameter("drive1");
    CHECK(info.isModulated); CHECK(info.sourceLfoIndex == 17);
    CHECK(info.currentValue > 0.75f); CHECK(info.currentValue < 0.85f);
    juce::MemoryBlock state; processor.getStateInformation(state);
    FireAudioProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(restored.getModulationInfoForParameter("drive1").sourceLfoIndex == 17);
    CHECK(restored.treeState.getRawParameterValue("macro3")->load() == Catch::Approx(0.65f));
    juce::XmlElement preset("WINGSFIRE"); ::state::saveStateToXml(processor, preset);
    REQUIRE(::state::loadStateFromXml(preset, restored));
    CHECK(restored.isCurrentStateEquivalentToPreset(preset));
    preset.removeAttribute("envAttack");
    CHECK_FALSE(::state::loadStateFromXml(preset, restored));
}

TEST_CASE("A project predating auxiliary sources opens with neutral macros", "[aux-mod][state][legacy]")
{
    FireAudioProcessor processor;
    juce::MemoryBlock state; processor.getStateInformation(state);
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize())); REQUIRE(xml != nullptr);
    auto* parameters = xml->getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
    for (auto* id : sources::ids)
    {
        auto* child = parameters->getChildByAttribute("id", id); REQUIRE(child != nullptr);
        parameters->removeChildElement(child, true);
        xml->getChildByName("AB_STATE")->removeAttribute(id);
    }
    xml->removeAttribute("modulationSourcesSchemaVersion");
    xml->getChildByName("AB_STATE")->removeAttribute("modulationSourcesSchemaVersion");
    xml->setAttribute("savedParameterCount", parameters->getNumChildElements());
    juce::AudioProcessor::copyXmlToBinary(*xml, state);
    plain(processor, "macro1", 0.9f);
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(processor.treeState.getRawParameterValue("macro1")->load() == 0);
}

TEST_CASE("Envelope and macro controls have editable values and matrix source names", "[aux-mod][ui]")
{
    FireAudioProcessor processor;
    fire::ui::ModulationSourceControls controls(processor); controls.setSize(620, 340);
    for (auto* id : sources::ids)
    {
        auto* slider = find<PrimarySlider>(controls, id); REQUIRE(slider != nullptr);
        CHECK(controls.getLocalBounds().contains(controls.getLocalArea(slider, slider->getLocalBounds())));
    }
    auto* macro = find<PrimarySlider>(controls, "macro1"); REQUIRE(macro != nullptr);
    CHECK(macro->getValueFromText("75 %") == Catch::Approx(0.75));
    macro->setValue(0.5, juce::sendNotificationSync);
    CHECK(processor.treeState.getRawParameterValue("macro1")->load() == Catch::Approx(0.5f));
    REQUIRE(processor.undoEdit());
    CHECK(processor.treeState.getRawParameterValue("macro1")->load() == 0);
    REQUIRE(processor.assignLfoToTarget(sources::envelope, "drive1") == LfoManager::AssignmentResult::changed);
    ModulationMatrixPanel matrix(processor); matrix.setSize(800, 420);
    auto* menu = find<juce::ComboBox>(matrix, "matrix_source"); REQUIRE(menu != nullptr);
    CHECK(menu->getItemText(menu->indexOfItemId(17)) == "Envelope");
    CHECK(menu->getItemText(menu->indexOfItemId(21)) == "Macro 4");
    const auto folder = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
    if (folder.isNotEmpty())
    {
        auto file = juce::File(folder).getChildFile("envelope-macros.png");
        auto stream = file.createOutputStream(); REQUIRE(stream != nullptr);
        REQUIRE(juce::PNGImageFormat().writeImageToStream(controls.createComponentSnapshot(controls.getLocalBounds()), *stream));
    }
}
