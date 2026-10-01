#include <PluginEditor.h>
#include <Utility/DriveCompensationParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
namespace comp = fire::drive_comp;
constexpr const char* marker = "driveCompSchemaVersion";
juce::String bandID(const char* base, int band) { return ParameterIDAndName::getIDString(base, band); }

void setPlain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float getPlain(FireAudioProcessor& processor, const juce::String& id)
{
    auto* parameter = processor.treeState.getRawParameterValue(id);
    REQUIRE(parameter != nullptr);
    return parameter->load();
}
juce::XmlElement preset(FireAudioProcessor& processor)
{
    juce::XmlElement xml("WINGSFIRE");
    state::saveStateToXml(processor, xml);
    return xml;
}
juce::XmlElement host(FireAudioProcessor& processor)
{
    juce::MemoryBlock bytes;
    processor.getStateInformation(bytes);
    auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(xml != nullptr);
    return *xml;
}
void restoreHost(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock bytes;
    juce::AudioProcessor::copyXmlToBinary(xml, bytes);
    processor.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
}
juce::XmlElement& hostParameter(juce::XmlElement& xml, const juce::String& id)
{
    auto* parameters = xml.getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
    auto* value = parameters->getChildByAttribute("id", id); REQUIRE(value != nullptr);
    return *value;
}
void removeHostParameter(juce::XmlElement& xml, const juce::String& id)
{
    auto* parameters = xml.getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
    parameters->removeChildElement(&hostParameter(xml, id), true);
    xml.setAttribute("savedParameterCount", parameters->getNumChildElements());
}
void stripFamily(juce::XmlElement& xml)
{
    xml.removeAttribute(marker);
    for (const auto& id : comp::parameterIDs()) xml.removeAttribute(id);
}
void setModes(FireAudioProcessor& processor, float value)
{
    for (int band = 0; band < 4; ++band) setPlain(processor, comp::parameterID(band), value);
}
void checkModes(FireAudioProcessor& processor, float value)
{
    for (int band = 0; band < 4; ++band)
        CHECK(getPlain(processor, comp::parameterID(band)) == value);
}
void configure(FireAudioProcessor& processor, int side)
{
    for (int band = 0; band < 4; ++band)
    {
        setPlain(processor, comp::parameterID(band), (band + side) % 2 == 0 ? 0.0f : 1.0f);
        setPlain(processor, bandID(LINKED_ID, band), (band + side) % 2 == 0 ? 1.0f : 0.0f);
        setPlain(processor, bandID(DRIVE_ID, band), static_cast<float>(20 + band * 15 + side));
        setPlain(processor, bandID(OUTPUT_ID, band), static_cast<float>(-7 + band * 2 - side));
    }
}
void checkSoundSettings(FireAudioProcessor& processor, int side, bool legacy = false)
{
    for (int band = 0; band < 4; ++band)
    {
        CAPTURE(side, band, legacy);
        CHECK(getPlain(processor, comp::parameterID(band))
              == (legacy ? 0.0f : ((band + side) % 2 == 0 ? 0.0f : 1.0f)));
        CHECK(getPlain(processor, bandID(LINKED_ID, band)) == ((band + side) % 2 == 0 ? 1.0f : 0.0f));
        CHECK(getPlain(processor, bandID(DRIVE_ID, band)) == Catch::Approx(20 + band * 15 + side));
        CHECK(getPlain(processor, bandID(OUTPUT_ID, band)) == Catch::Approx(-7 + band * 2 - side));
    }
}
ModulatableSlider* findOutput(juce::Component& root)
{
    if (auto* slider = dynamic_cast<ModulatableSlider*>(&root);
        slider && slider->parameterID == "output1") return slider;
    for (auto* child : root.getChildren()) if (auto* result = findOutput(*child)) return result;
    return nullptr;
}
}

TEST_CASE("Modern Drive compensation is appended while Link keeps its historical parameter contract",
          "[drive-comp][state][parameters][compatibility]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.getParameters().size() == 1047 + fire::core_modules::parameterCount + fire::analog_params::parameterCount);
    auto* previousLast = processor.treeState.getParameter("bandFx8Resonator4");
    REQUIRE(previousLast != nullptr);
    CHECK(previousLast->getParameterIndex() == 1035);
    for (int band = 0; band < 4; ++band)
    {
        const auto id = "driveCompModern" + juce::String(band + 1);
        auto* parameter = processor.treeState.getParameter(id);
        auto* linked = processor.treeState.getParameter("linked" + juce::String(band + 1));
        REQUIRE(parameter != nullptr); REQUIRE(linked != nullptr);
        CHECK(comp::parameterID(band) == id);
        CHECK(comp::isParameterID(id));
        CHECK(parameter->getParameterIndex() == 1036 + band);
        CHECK(parameter->getVersionHint() == 10);
        CHECK(parameter->getDefaultValue() == 1.0f);
        CHECK(getPlain(processor, id) == 1.0f);
        CHECK(linked->getVersionHint() == 1);
        CHECK(linked->getDefaultValue() == 1.0f);
        CHECK(linked->convertFrom0to1(0.0f) == 0.0f);
        CHECK(linked->convertFrom0to1(1.0f) == 1.0f);
        CHECK_FALSE(fire::effects::isParameterID(id));
        CHECK_FALSE(fire::resonator_params::isParameterID(id));
    }
    CHECK_FALSE(comp::isParameterID("driveCompModern0"));
    CHECK_FALSE(comp::isParameterID("driveCompModern5"));
    CHECK_FALSE(comp::isParameterID("driveCompModern1junk"));
}

TEST_CASE("New Drive compensation modes and independent Output values survive preset host and AB copies",
          "[drive-comp][state][preset][host][ab]")
{
    FireAudioProcessor source;
    configure(source, 0);
    const auto first = preset(source);
    CHECK(first.getIntAttribute(marker) == 1);
    source.stateAB.copyAB(false); source.stateAB.toggleAB();
    configure(source, 1);
    const auto second = preset(source);
    const auto savedHost = host(source);
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(first, restored)); checkSoundSettings(restored, 0);
    CHECK(restored.isCurrentStateEquivalentToPreset(first));
    REQUIRE(state::loadStateFromXml(second, restored)); checkSoundSettings(restored, 1);
    restoreHost(restored, savedHost);
    CHECK_FALSE(restored.stateAB.isCurrentA()); checkSoundSettings(restored, 1);
    restored.stateAB.toggleAB(); checkSoundSettings(restored, 0);
    restored.stateAB.copyAB(false); restored.stateAB.toggleAB(); checkSoundSettings(restored, 0);
}

TEST_CASE("Absent Drive compensation families retain legacy Link on active and alternate sounds",
          "[drive-comp][state][legacy][preset][host][ab]")
{
    FireAudioProcessor source;
    configure(source, 0);
    auto legacyPreset = preset(source); stripFamily(legacyPreset);
    source.stateAB.copyAB(false); source.stateAB.toggleAB();
    configure(source, 1);
    auto legacyHost = host(source);
    legacyHost.removeAttribute(marker);
    for (const auto& id : comp::parameterIDs()) removeHostParameter(legacyHost, id);
    auto* ab = legacyHost.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
    stripFamily(*ab);
    FireAudioProcessor restored;
    for (int version : {-1, 0, 1, 2})
    {
        CAPTURE(version);
        auto candidate = legacyPreset;
        if (version < 0) candidate.removeAttribute("presetFormatVersion");
        else candidate.setAttribute("presetFormatVersion", version);
        setModes(restored, 1.0f);
        REQUIRE(state::loadStateFromXml(candidate, restored));
        checkSoundSettings(restored, 0, true);
        CHECK(restored.isCurrentStateEquivalentToPreset(candidate));
        CHECK(preset(restored).getIntAttribute(marker) == 1);
    }
    setModes(restored, 1.0f);
    restoreHost(restored, legacyHost);
    CHECK_FALSE(restored.stateAB.isCurrentA());
    checkSoundSettings(restored, 1, true);
    auto resaved = host(restored);
    auto* savedAlternate = resaved.getChildByName("AB_STATE"); REQUIRE(savedAlternate != nullptr);
    CHECK(savedAlternate->getIntAttribute(marker) == 1);
    for (const auto& id : comp::parameterIDs())
    {
        REQUIRE(savedAlternate->hasAttribute(id));
        CHECK(savedAlternate->getDoubleAttribute(id) == 0.0);
    }
    restored.stateAB.toggleAB(); checkSoundSettings(restored, 0, true);
    restored.stateAB.copyAB(false); restored.stateAB.toggleAB(); checkSoundSettings(restored, 0, true);
}

TEST_CASE("Drive compensation schema and canonical booleans reject partial or malformed replacements",
          "[drive-comp][state][validation][canonical]")
{
    FireAudioProcessor processor;
    configure(processor, 0);
    const auto baseline = preset(processor);
    const auto savedHost = host(processor);
    const auto id = comp::parameterID(3);
    const auto rejectHost = [&](juce::XmlElement candidate)
    {
        hostParameter(candidate, OUTPUT_ID).setAttribute("value", -9.0f);
        restoreHost(processor, candidate);
        CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
    };
    for (const auto* version : {"0", "2", "-1", "1.5", "junk"})
    {
        auto candidate = baseline; candidate.setAttribute(marker, version);
        CHECK_FALSE(state::loadStateFromXml(candidate, processor));
        CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(candidate));
        auto incoming = savedHost; incoming.setAttribute(marker, version); rejectHost(incoming);
    }
    for (bool keepMarker : {false, true})
    {
        auto candidate = baseline; candidate.removeAttribute(id);
        if (! keepMarker) candidate.removeAttribute(marker);
        CHECK_FALSE(state::loadStateFromXml(candidate, processor));
        auto incoming = savedHost; removeHostParameter(incoming, id);
        if (! keepMarker) incoming.removeAttribute(marker);
        rejectHost(incoming);
    }
    for (const auto* value : {"-1", "2", "0.5", "NaN"})
    {
        auto incoming = savedHost; hostParameter(incoming, id).setAttribute("value", value); rejectHost(incoming);
    }
    for (const auto* value : {"-0.1", "1.1", "NaN"})
    {
        auto candidate = baseline; candidate.setAttribute(id, value);
        CHECK_FALSE(state::loadStateFromXml(candidate, processor));
    }
    for (float fraction : {0.25f, 0.75f})
    {
        const auto expected = fraction > 0.5f ? 1.0f : 0.0f;
        auto candidate = baseline; candidate.setAttribute(id, fraction);
        REQUIRE(state::loadStateFromXml(candidate, processor));
        CHECK(getPlain(processor, id) == expected);
        CHECK(processor.treeState.getParameter(id)->getValue() == expected);
        CHECK(processor.isCurrentStateEquivalentToPreset(candidate));
        processor.stateAB.copyAB(false);
        auto incoming = host(processor);
        auto* ab = incoming.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
        ab->setAttribute(id, fraction);
        restoreHost(processor, incoming);
        processor.stateAB.toggleAB();
        CHECK(processor.treeState.getParameter(id)->getValue() == expected);
    }
}

TEST_CASE("A damaged Drive compensation alternate mirrors the restored main mode instead of upgrading it",
          "[drive-comp][state][ab][validation]")
{
    FireAudioProcessor source;
    configure(source, 1);
    source.stateAB.copyAB(false); source.stateAB.toggleAB();
    configure(source, 0);
    auto incoming = host(source);
    auto* alternate = incoming.getChildByName("AB_STATE"); REQUIRE(alternate != nullptr);
    SECTION("future marker") { alternate->setAttribute(marker, 2); }
    SECTION("truncated flags") { alternate->removeAttribute(comp::parameterID(0)); }
    FireAudioProcessor restored;
    restoreHost(restored, incoming); checkSoundSettings(restored, 0);
    restored.stateAB.toggleAB(); checkSoundSettings(restored, 0);
}

TEST_CASE("Band insertion preserves migrated Drive modes while newly reset bands use modern compensation",
          "[drive-comp][state][topology]")
{
    FireAudioProcessor processor;
    configure(processor, 0);
    setPlain(processor, NUM_BANDS_ID, 2);
    setPlain(processor, "lineState1", 1); setPlain(processor, "freq1", 1500);
    const auto target = bandID(OUTPUT_ID, 0);
    REQUIRE(processor.assignLfoToTarget(1, target) == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.addMultibandBand(0, 2, true, 300));
    CHECK(getPlain(processor, comp::parameterID(0)) == 1.0f);
    CHECK(getPlain(processor, comp::parameterID(1)) == 0.0f);
    CHECK(getPlain(processor, comp::parameterID(2)) == 1.0f);
    CHECK(getPlain(processor, bandID(OUTPUT_ID, 1)) == Catch::Approx(-7.0f));
    CHECK(processor.getModulationInfoForParameter(bandID(OUTPUT_ID, 1)).sourceLfoIndex == 2);
    REQUIRE(processor.deleteMultibandBand(0, 3));
    CHECK(getPlain(processor, comp::parameterID(0)) == 0.0f);
    CHECK(getPlain(processor, comp::parameterID(1)) == 1.0f);
    CHECK(getPlain(processor, comp::parameterID(2)) == 1.0f);
    CHECK(getPlain(processor, target) == Catch::Approx(-7.0f));
    CHECK(processor.getModulationInfoForParameter(target).sourceLfoIndex == 2);
}

TEST_CASE("Opening an old sound does not upgrade Link and explicit upgrade preserves editable Output",
          "[drive-comp][state][ui][upgrade]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    configure(processor, 0);
    setPlain(processor, "linked1", 0.0f);
    auto old = preset(processor); stripFamily(old);
    REQUIRE(state::loadStateFromXml(old, processor));
    checkModes(processor, 0.0f);
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    editor.timerCallback();
    CHECK(processor.isCurrentStateEquivalentToPreset(old));
    checkModes(processor, 0.0f);
    auto* output = findOutput(editor); REQUIRE(output != nullptr);
    const auto stored = getPlain(processor, "output1");
    processor.upgradeBandDriveCompensation(0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    editor.timerCallback();
    CHECK(getPlain(processor, comp::parameterID(0)) == 1.0f);
    CHECK(getPlain(processor, "linked1") == 1.0f);
    CHECK(getPlain(processor, "output1") == stored);
    for (int band = 1; band < 4; ++band) CHECK(getPlain(processor, comp::parameterID(band)) == 0.0f);
    CHECK(output->isEnabled());
    output->setValue(-3.4, juce::sendNotificationSync);
    CHECK(getPlain(processor, "output1") == Catch::Approx(-3.4f));
    setPlain(processor, "drive1", 83.0f);
    editor.timerCallback();
    CHECK(getPlain(processor, "output1") == Catch::Approx(-3.4f));
}
