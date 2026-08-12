#include "../Source/PluginProcessor.h"
#include "../Source/Panels/TopPanel/Preset.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace
{
class ScopedTemporaryDirectory
{
public:
    ScopedTemporaryDirectory()
        : directory(juce::File::getCurrentWorkingDirectory()
                        .getChildFile("Builds")
                        .getChildFile("TestTemp")
                        .getChildFile("FirePresetStateTests-" + juce::Uuid().toString()))
    {
        directoryWasCreated = directory.createDirectory().wasOk();
    }

    ~ScopedTemporaryDirectory()
    {
        if (directoryWasCreated)
            directory.deleteRecursively(false);
    }

    bool wasCreated() const noexcept { return directoryWasCreated; }

    juce::File directory;

private:
    bool directoryWasCreated = false;
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(
        parameter->getNormalisableRange().convertTo0to1(plainValue));
}

float getPlainParameter(const FireAudioProcessor& processor,
                        const juce::String& parameterID)
{
    const auto* value = processor.treeState.getRawParameterValue(parameterID);
    REQUIRE(value != nullptr);
    return value->load(std::memory_order_relaxed);
}

LfoData makeLfoShape(float middleX, float middleY, float smoothness)
{
    LfoData shape;
    shape.points = { { 0.0f, 0.15f }, { middleX, middleY }, { 1.0f, 0.35f } };
    shape.curvatures = { 0.4f, -0.6f };
    shape.smoothness = smoothness;
    shape.sanitise();
    return shape;
}

const ModulationRouting* findRouting(const juce::Array<ModulationRouting>& routings,
                                     const juce::String& target)
{
    return std::find_if(routings.begin(), routings.end(), [&](const auto& routing)
                        { return routing.targetParameterID == target; });
}

int countRoutingsForTarget(const juce::Array<ModulationRouting>& routings,
                           const juce::String& target)
{
    return static_cast<int>(std::count_if(routings.begin(), routings.end(), [&](const auto& routing)
                                          { return routing.targetParameterID == target; }));
}

void appendRouting(juce::XmlElement& routingParent,
                   int source,
                   const juce::String& target,
                   float depth)
{
    auto* routing = routingParent.createNewChildElement("ROUTING");
    routing->setAttribute("source", source);
    routing->setAttribute("target", target);
    routing->setAttribute("depth", depth);
    routing->setAttribute("bipolar", true);
    routing->setAttribute("bypassed", false);
}

void writePresetFile(FireAudioProcessor& processor,
                     const juce::File& file,
                     const juce::String& name)
{
    juce::XmlElement xml { "WINGSFIRE" };
    xml.setAttribute("presetName", name);
    state::saveStateToXml(processor, xml);
    REQUIRE(xml.writeTo(file));
}

juce::XmlElement* findHostParameter(juce::XmlElement& stateXml,
                                    FireAudioProcessor& processor,
                                    const juce::String& parameterID)
{
    auto* parameterState = stateXml.getChildByName(
        processor.treeState.state.getType().toString());
    REQUIRE(parameterState != nullptr);

    for (auto* child : parameterState->getChildIterator())
        if (child->getStringAttribute("id") == parameterID)
            return child;

    return nullptr;
}

int countValueTreeChildrenWithID(const juce::ValueTree& tree,
                                 const juce::String& parameterID)
{
    int count = 0;
    for (const auto& child : tree)
        if (child.getProperty("id").toString() == parameterID)
            ++count;

    return count;
}

juce::File findProjectRoot()
{
    auto candidate = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                         .getParentDirectory();
    while (candidate != candidate.getParentDirectory())
    {
        if (candidate.getChildFile("CMakeLists.txt").existsAsFile()
            && candidate.getChildFile("tests/Presets").isDirectory())
            return candidate;

        candidate = candidate.getParentDirectory();
    }

    return {};
}
} // namespace

TEST_CASE("Preset files round-trip parameters, multiband state, LFOs and routings headlessly",
          "[preset][state][roundtrip][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());
    FireAudioProcessor processor;
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto frequencyID = ParameterIDAndName::getIDString(FREQ_ID, 0);
    setPlainParameter(processor, NUM_BANDS_ID, 2.0f);
    setPlainParameter(processor, frequencyID, 1375.0f);
    setPlainParameter(processor, driveID, 24.0f);

    const auto savedShape = makeLfoShape(0.42f, 0.91f, 0.67f);
    processor.getLfoManager().setLfoData(2, savedShape);
    processor.assignLfoToTarget(2, driveID);
    processor.setModulationDepth(driveID, -0.37f);

    const auto savePath = temporaryDirectory.directory.getChildFile("Roundtrip.fire");
    REQUIRE(presets.savePreset(savePath) == "Roundtrip");

    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, frequencyID, 8000.0f);
    setPlainParameter(processor, driveID, 6.0f);
    processor.getLfoManager().setLfoData(2, LfoData {});
    processor.clearModulationForParameter(driveID);

    juce::ComboBox menu;
    presets.setPresetAndFolderNames(menu);
    const int presetID = presets.getCurrentPresetId();
    REQUIRE(presetID > 0);
    const auto presetTag = presets.comboBoxIdToTagNameMap[presetID];
    REQUIRE(presetTag.isNotEmpty());
    presets.loadPreset(presetTag);

    CHECK(getPlainParameter(processor, NUM_BANDS_ID) == Catch::Approx(2.0f));
    CHECK(getPlainParameter(processor, frequencyID) == Catch::Approx(1375.0f));
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(24.0f));

    const auto restoredShapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredShapes.size() == 4);
    REQUIRE(restoredShapes[2].points.size() == 3);
    CHECK(restoredShapes[2].points[1].x == Catch::Approx(0.42f));
    CHECK(restoredShapes[2].points[1].y == Catch::Approx(0.91f));
    CHECK(restoredShapes[2].smoothness == Catch::Approx(0.67f));

    const auto restoredRoutings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* restoredRouting = findRouting(restoredRoutings, driveID);
    REQUIRE(restoredRouting != nullptr);
    CHECK(restoredRouting->sourceLfoIndex == 2);
    CHECK(restoredRouting->depth == Catch::Approx(-0.37f));
}

TEST_CASE("Preset save failure is atomic and dotted filenames retain their identity",
          "[preset][filesystem][identity]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());
    FireAudioProcessor processor;
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };

    const auto dottedPath = temporaryDirectory.directory.getChildFile("Lead.v2.fire");
    CHECK(presets.savePreset(dottedPath) == "Lead.v2");
    CHECK(dottedPath.existsAsFile());
    CHECK(presets.getNumPresets() == 1);
    CHECK(presets.getPresetName() == "Lead.v2");

    juce::ComboBox menu;
    presets.setPresetAndFolderNames(menu);
    CHECK(menu.getNumItems() == 1);
    CHECK(menu.getItemText(0) == "Lead.v2");

    const juce::String nameBeforeFailure { presets.getPresetName() };
    const int countBeforeFailure = presets.getNumPresets();
    const auto parentBlocker = temporaryDirectory.directory.getChildFile("not-a-folder");
    REQUIRE(parentBlocker.replaceWithText("This regular file cannot contain a preset."));
    const auto unwritablePath = parentBlocker.getChildFile("CannotSave.fire");
    REQUIRE(parentBlocker.existsAsFile());
    CHECK(presets.savePreset(unwritablePath).isEmpty());
    CHECK_FALSE(unwritablePath.existsAsFile());
    CHECK(presets.getPresetName() == nameBeforeFailure);
    CHECK(presets.getNumPresets() == countBeforeFailure);
}

TEST_CASE("Duplicate preset display names keep their relative-path identity",
          "[preset][filesystem][identity][duplicates]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    const auto folderA = temporaryDirectory.directory.getChildFile("A");
    const auto folderB = temporaryDirectory.directory.getChildFile("B");
    REQUIRE(folderA.createDirectory().wasOk());
    REQUIRE(folderB.createDirectory().wasOk());

    FireAudioProcessor presetA;
    FireAudioProcessor presetB;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(presetA, driveID, 17.0f);
    setPlainParameter(presetB, driveID, 63.0f);
    writePresetFile(presetA, folderA.getChildFile("Twin.fire"), "Twin");
    writePresetFile(presetB, folderB.getChildFile("Twin.fire"), "Twin");

    FireAudioProcessor processor;
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };
    juce::ComboBox initialMenu;
    presets.setPresetAndFolderNames(initialMenu);
    REQUIRE(presets.getNumPresets() == 2);

    int folderAPresetID = 0;
    for (int id = 1; id <= presets.getNumPresets(); ++id)
    {
        presets.loadPreset(presets.comboBoxIdToTagNameMap[id]);
        if (presets.getCurrentPresetKey() == "A/Twin.fire")
        {
            folderAPresetID = id;
            break;
        }
    }

    REQUIRE(folderAPresetID > 0);
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(17.0f));
    presets.setCurrentPresetId(folderAPresetID);

    presets.scanAllPresets();
    juce::ComboBox rescannedMenu;
    presets.setPresetAndFolderNames(rescannedMenu);

    CHECK(presets.getCurrentPresetKey() == "A/Twin.fire");
    CHECK(presets.getCurrentPresetId() == folderAPresetID);
}

TEST_CASE("Preset UI synchronisation reflects restored identity without reloading live state",
          "[preset][state][ui][identity][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 34.0f);
    writePresetFile(processor,
                    temporaryDirectory.directory.getChildFile("Sync.fire"),
                    "Sync");

    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };
    presets.setCurrentPresetKey("Sync.fire");
    state::StateComponent component { processor.stateAB, presets, processor.treeState };
    component.synchronisePresetSelectionFromManager();

    auto* presetBox = component.getPresetBox();
    REQUIRE(presetBox != nullptr);
    REQUIRE(presets.getCurrentPresetId() == 1);
    CHECK(presetBox->getSelectedId() == 1);
    CHECK(presetBox->getText() == "Sync");
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(34.0f));

    // Reconciliation is display-only. A differing live state must be marked
    // dirty rather than silently reloading the on-disk preset.
    setPlainParameter(processor, driveID, 61.0f);
    component.synchronisePresetSelectionFromManager();
    CHECK(presetBox->getSelectedId() == 0);
    CHECK(presetBox->getText() == "Sync*");
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(61.0f));
}

TEST_CASE("Preset scan rejects malformed and foreign XML without exposing reset traps",
          "[preset][scan][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());
    FireAudioProcessor validPresetProcessor;
    setPlainParameter(validPresetProcessor,
                      ParameterIDAndName::getIDString(DRIVE_ID, 0),
                      19.0f);
    writePresetFile(validPresetProcessor,
                    temporaryDirectory.directory.getChildFile("Valid.fire"),
                    "Valid");

    juce::XmlElement foreignXml { "SETTINGS" };
    foreignXml.setAttribute("unrelated", "data");
    REQUIRE(foreignXml.writeTo(temporaryDirectory.directory.getChildFile("Foreign.fire")));
    juce::XmlElement emptyFirePreset { "WINGSFIRE" };
    REQUIRE(emptyFirePreset.writeTo(
        temporaryDirectory.directory.getChildFile("EmptyFire.fire")));
    REQUIRE(temporaryDirectory.directory.getChildFile("Malformed.fire")
                .replaceWithText("<WINGSFIRE><broken></WINGSFIRE>"));

    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 27.0f);
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };

    juce::ComboBox menu;
    presets.setPresetAndFolderNames(menu);
    REQUIRE(presets.getNumPresets() == 1);
    REQUIRE(menu.getNumItems() == 1);
    CHECK(menu.getItemText(0) == "Valid");

    // A foreign XML file must never become a selectable action that silently
    // restores every missing parameter to its default value.
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(27.0f));
}

TEST_CASE("Invalid numeric preset attributes fall back safely",
          "[preset][state][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto* mix = processor.treeState.getParameter(MIX_ID);
    REQUIRE(mix != nullptr);
    const float expectedDefault = mix->getDefaultValue();
    mix->setValueNotifyingHost(0.31f);

    juce::XmlElement malformedPreset { "WINGSFIRE" };
    malformedPreset.setAttribute(MIX_ID, "not-a-number");
    state::loadStateFromXml(malformedPreset, processor);

    CHECK(mix->getValue() == Catch::Approx(expectedDefault));
}

TEST_CASE("Corrupt host state without a valid APVTS tree is rejected atomically",
          "[state][host][corrupt][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 22.0f);
    processor.setSavedWidth(1432);
    processor.setSavedHeight(811);
    processor.statePresets.setCurrentPresetId(7);

    const auto shape = makeLfoShape(0.38f, 0.88f, 0.57f);
    processor.getLfoManager().setLfoData(1, shape);
    processor.assignLfoToTarget(1, driveID);
    processor.setModulationDepth(driveID, 0.42f);

    // Keep a known-good inactive B snapshot, then make active A observably
    // different. A rejected host transaction must preserve both sides.
    processor.stateAB.copyAB();
    setPlainParameter(processor, driveID, 31.0f);

    juce::XmlElement corruptState { "state" };
    corruptState.createNewChildElement("BROKEN_PARAMETER_STATE");
    auto* otherState = corruptState.createNewChildElement("otherState");
    otherState->setAttribute("currentPresetID", 1);
    otherState->setAttribute("editorWidth", 1);
    otherState->setAttribute("editorHeight", 1);

    FireAudioProcessor hostileABPayload;
    setPlainParameter(hostileABPayload, driveID, 88.0f);
    auto* hostileABState = corruptState.createNewChildElement("AB_STATE");
    state::saveStateToXml(hostileABPayload, *hostileABState);
    hostileABState->setAttribute("currentSideIsA", false);

    juce::MemoryBlock corruptBinary;
    juce::AudioProcessor::copyXmlToBinary(corruptState, corruptBinary);
    processor.setStateInformation(corruptBinary.getData(),
                                  static_cast<int>(corruptBinary.getSize()));

    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(31.0f));
    CHECK(processor.getSavedWidth() == 1432);
    CHECK(processor.getSavedHeight() == 811);
    CHECK(processor.statePresets.getCurrentPresetId() == 7);
    CHECK(processor.stateAB.isCurrentA());

    const auto shapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes.size() == 4);
    REQUIRE(shapes[1].points.size() == 3);
    CHECK(shapes[1].points[1].x == Catch::Approx(0.38f));
    CHECK(shapes[1].points[1].y == Catch::Approx(0.88f));
    CHECK(shapes[1].smoothness == Catch::Approx(0.57f));

    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* routing = findRouting(routings, driveID);
    REQUIRE(routing != nullptr);
    CHECK(routing->sourceLfoIndex == 1);
    CHECK(routing->depth == Catch::Approx(0.42f));

    processor.stateAB.toggleAB();
    CHECK_FALSE(processor.stateAB.isCurrentA());
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(22.0f));
}

TEST_CASE("Invalid known host parameters reject the complete state transaction",
          "[state][host][corrupt][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 26.0f);
    processor.setSavedWidth(1410);
    processor.setSavedHeight(805);
    processor.statePresets.setCurrentPresetId(4);

    const auto shape = makeLfoShape(0.36f, 0.86f, 0.54f);
    processor.getLfoManager().setLfoData(1, shape);
    processor.assignLfoToTarget(1, driveID);
    processor.setModulationDepth(driveID, 0.41f);

    juce::MemoryBlock validState;
    processor.getStateInformation(validState);
    auto baselineXml = juce::AudioProcessor::getXmlFromBinary(
        validState.getData(), static_cast<int>(validState.getSize()));
    REQUIRE(baselineXml != nullptr);

    for (const juce::String invalidValue : { "nan", "oops", "1e4294967296" })
    {
        CAPTURE(invalidValue);
        auto invalidXml = std::make_unique<juce::XmlElement>(*baselineXml);
        auto* drive = findHostParameter(*invalidXml, processor, driveID);
        REQUIRE(drive != nullptr);
        drive->setAttribute("value", invalidValue);

        auto* otherState = invalidXml->getChildByName("otherState");
        REQUIRE(otherState != nullptr);
        otherState->setAttribute("currentPresetID", 0);
        otherState->setAttribute("editorWidth", 999);
        otherState->setAttribute("editorHeight", 600);

        if (auto* lfoState = invalidXml->getChildByName("LFO_STATE"))
            invalidXml->removeChildElement(lfoState, true);
        if (auto* routingState = invalidXml->getChildByName("MODULATION_STATE"))
            routingState->deleteAllChildElements();

        juce::MemoryBlock invalidState;
        juce::AudioProcessor::copyXmlToBinary(*invalidXml, invalidState);
        processor.setStateInformation(invalidState.getData(),
                                      static_cast<int>(invalidState.getSize()));

        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(26.0f));
        CHECK(processor.getSavedWidth() == 1410);
        CHECK(processor.getSavedHeight() == 805);
        CHECK(processor.statePresets.getCurrentPresetId() == 4);

        const auto shapes = processor.getLfoManager().getLfoDataCopy();
        REQUIRE(shapes.size() == 4);
        REQUIRE(shapes[1].points.size() == 3);
        CHECK(shapes[1].points[1].x == Catch::Approx(0.36f));
        CHECK(shapes[1].points[1].y == Catch::Approx(0.86f));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* routing = findRouting(routings, driveID);
        REQUIRE(routing != nullptr);
        CHECK(routing->sourceLfoIndex == 1);
        CHECK(routing->depth == Catch::Approx(0.41f));
    }
}

TEST_CASE("Legacy preset equivalence compares effective LFO and routing defaults",
          "[preset][state][equivalence][legacy][lfo]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    setPlainParameter(processor, driveID, 34.0f);
    juce::XmlElement legacyPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, legacyPreset);
    if (auto* lfoState = legacyPreset.getChildByName("LFO_STATE"))
        legacyPreset.removeChildElement(lfoState, true);
    if (auto* routingState = legacyPreset.getChildByName("MODULATION_STATE"))
        legacyPreset.removeChildElement(routingState, true);

    setPlainParameter(processor, driveID, 61.0f);
    processor.getLfoManager().setLfoData(0, makeLfoShape(0.4f, 0.9f, 0.3f));
    processor.assignLfoToTarget(0, driveID);
    state::loadStateFromXml(legacyPreset, processor);
    CHECK(processor.isCurrentStateEquivalentToPreset(legacyPreset));

    processor.getLfoManager().setLfoData(0, makeLfoShape(0.62f, 0.18f, 0.3f));
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(legacyPreset));

    state::loadStateFromXml(legacyPreset, processor);
    REQUIRE(processor.isCurrentStateEquivalentToPreset(legacyPreset));
    processor.assignLfoToTarget(2, driveID);
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(legacyPreset));

    state::loadStateFromXml(legacyPreset, processor);
    auto malformedFallbackPreset = std::make_unique<juce::XmlElement>(legacyPreset);
    malformedFallbackPreset->setAttribute(driveID, "0.5oops");
    state::loadStateFromXml(*malformedFallbackPreset, processor);
    CHECK(processor.isCurrentStateEquivalentToPreset(*malformedFallbackPreset));

    // Smoothness originally lived only in LFO XML. The loader promotes it to
    // APVTS, so a missing lfoSmooth parameter attribute must still compare
    // clean against the promoted value.
    juce::XmlElement legacySmoothnessPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, legacySmoothnessPreset);
    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0);
    legacySmoothnessPreset.removeAttribute(smoothnessID);
    auto* legacyLfoState = legacySmoothnessPreset.getChildByName("LFO_STATE");
    REQUIRE(legacyLfoState != nullptr);
    auto* legacyLfo = legacyLfoState->getChildByAttribute("index", "0");
    REQUIRE(legacyLfo != nullptr);
    legacyLfo->setAttribute("smoothness", "0.37");
    state::loadStateFromXml(legacySmoothnessPreset, processor);
    CHECK(getPlainParameter(processor, smoothnessID) == Catch::Approx(0.37f));
    CHECK(processor.isCurrentStateEquivalentToPreset(legacySmoothnessPreset));
}

TEST_CASE("Bundled legacy LFO preset is clean after semantic model loading",
          "[preset][state][equivalence][legacy][lfo][fixture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto fixture = findProjectRoot().getChildFile("tests/Presets/lfo_sawup.fire");
    REQUIRE(fixture.existsAsFile());
    auto preset = juce::XmlDocument::parse(fixture);
    REQUIRE(preset != nullptr);

    FireAudioProcessor processor;
    state::loadStateFromXml(*preset, processor);

    // This fixture predates LFO smoothness and routing bypass persistence. Its
    // short decimal strings and omitted default attributes must compare as the
    // exact model produced by the legacy loader.
    CHECK(processor.isCurrentStateEquivalentToPreset(*preset));

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    processor.getLfoManager().toggleBypassForRouting(driveID);
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(*preset));

    state::loadStateFromXml(*preset, processor);
    auto changedShape = processor.getLfoManager().getLfoDataCopy()[0];
    REQUIRE(changedShape.points.size() > 1);
    changedShape.points[1].y = juce::jlimit(0.0f, 1.0f, changedShape.points[1].y + 0.01f);
    processor.getLfoManager().setLfoData(0, changedShape);
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(*preset));
}

TEST_CASE("Host parameter migration clamps ranges and canonicalises duplicate or unknown IDs",
          "[state][host][migration][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("out-of-range denormalised values are clamped")
    {
        FireAudioProcessor source;
        juce::MemoryBlock stateBlock;
        source.getStateInformation(stateBlock);
        auto xml = juce::AudioProcessor::getXmlFromBinary(
            stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        REQUIRE(xml != nullptr);

        const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        auto* drive = findHostParameter(*xml, source, driveID);
        auto* numBands = findHostParameter(*xml, source, NUM_BANDS_ID);
        REQUIRE(drive != nullptr);
        REQUIRE(numBands != nullptr);
        drive->setAttribute("value", "100000");
        numBands->setAttribute("value", "-999");
        juce::AudioProcessor::copyXmlToBinary(*xml, stateBlock);

        FireAudioProcessor restored;
        restored.setStateInformation(stateBlock.getData(),
                                     static_cast<int>(stateBlock.getSize()));
        const auto* driveParameter = restored.treeState.getParameter(driveID);
        const auto* numBandsParameter = restored.treeState.getParameter(NUM_BANDS_ID);
        REQUIRE(driveParameter != nullptr);
        REQUIRE(numBandsParameter != nullptr);
        CHECK(getPlainParameter(restored, driveID)
              == Catch::Approx(driveParameter->getNormalisableRange().end));
        CHECK(getPlainParameter(restored, NUM_BANDS_ID)
              == Catch::Approx(numBandsParameter->getNormalisableRange().start));
    }

    SECTION("first known ID wins and foreign IDs are discarded")
    {
        FireAudioProcessor source;
        const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        setPlainParameter(source, driveID, 28.0f);
        juce::MemoryBlock stateBlock;
        source.getStateInformation(stateBlock);
        auto xml = juce::AudioProcessor::getXmlFromBinary(
            stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        REQUIRE(xml != nullptr);
        auto* parameters = xml->getChildByName(source.treeState.state.getType().toString());
        auto* originalDrive = findHostParameter(*xml, source, driveID);
        REQUIRE(parameters != nullptr);
        REQUIRE(originalDrive != nullptr);

        auto duplicateDrive = std::make_unique<juce::XmlElement>(*originalDrive);
        duplicateDrive->setAttribute("value", 73.0);
        parameters->addChildElement(duplicateDrive.release());
        auto* unknown = parameters->createNewChildElement(originalDrive->getTagName());
        unknown->setAttribute("id", "futureUnknownParameter");
        unknown->setAttribute("value", 9.0);
        juce::AudioProcessor::copyXmlToBinary(*xml, stateBlock);

        FireAudioProcessor restored;
        restored.setStateInformation(stateBlock.getData(),
                                     static_cast<int>(stateBlock.getSize()));
        CHECK(getPlainParameter(restored, driveID) == Catch::Approx(28.0f));

        const auto canonicalState = restored.treeState.copyState();
        CHECK(countValueTreeChildrenWithID(canonicalState, driveID) == 1);
        CHECK(countValueTreeChildrenWithID(canonicalState, "futureUnknownParameter") == 0);
    }
}

TEST_CASE("Host state round-trip preserves stable preset path identity",
          "[state][host][preset][identity][roundtrip]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    source.statePresets.setCurrentPresetKey("Factory/Lead.v2.fire");

    juce::MemoryBlock stateBlock;
    source.getStateInformation(stateBlock);
    FireAudioProcessor restored;
    restored.setStateInformation(stateBlock.getData(),
                                 static_cast<int>(stateBlock.getSize()));

    CHECK(restored.statePresets.getCurrentPresetKey() == "Factory/Lead.v2.fire");
}

TEST_CASE("State loaders enforce one modulation routing per target",
          "[state][preset][host][lfo][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto targetID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    SECTION("preset XML")
    {
        FireAudioProcessor processor;
        juce::XmlElement preset { "WINGSFIRE" };
        state::saveStateToXml(processor, preset);
        auto* routingState = preset.getChildByName("MODULATION_STATE");
        REQUIRE(routingState != nullptr);
        appendRouting(*routingState, 1, targetID, 0.25f);
        appendRouting(*routingState, 3, targetID, -0.75f);

        state::loadStateFromXml(preset, processor);
        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        CHECK(countRoutingsForTarget(routings, targetID) == 1);
    }

    SECTION("host state")
    {
        FireAudioProcessor source;
        juce::MemoryBlock stateBlock;
        source.getStateInformation(stateBlock);
        auto xml = juce::AudioProcessor::getXmlFromBinary(
            stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        REQUIRE(xml != nullptr);
        auto* routingState = xml->getChildByName("MODULATION_STATE");
        REQUIRE(routingState != nullptr);
        appendRouting(*routingState, 0, targetID, 0.2f);
        appendRouting(*routingState, 2, targetID, -0.4f);
        juce::AudioProcessor::copyXmlToBinary(*xml, stateBlock);

        FireAudioProcessor restored;
        restored.setStateInformation(stateBlock.getData(),
                                     static_cast<int>(stateBlock.getSize()));
        const auto routings = restored.getLfoManager().getModulationRoutingsCopy();
        CHECK(countRoutingsForTarget(routings, targetID) == 1);
    }
}

TEST_CASE("A-B swapping preserves multiband, LFO and modulation state",
          "[state][ab][roundtrip][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto frequencyID = ParameterIDAndName::getIDString(FREQ_ID, 0);

    const auto shapeA = makeLfoShape(0.31f, 0.83f, 0.22f);
    setPlainParameter(processor, NUM_BANDS_ID, 2.0f);
    setPlainParameter(processor, frequencyID, 1180.0f);
    setPlainParameter(processor, driveID, 21.0f);
    processor.getLfoManager().setLfoData(0, shapeA);
    processor.assignLfoToTarget(0, driveID);
    processor.setModulationDepth(driveID, 0.3f);
    processor.stateAB.copyAB();

    const auto shapeB = makeLfoShape(0.73f, 0.24f, 0.79f);
    setPlainParameter(processor, NUM_BANDS_ID, 3.0f);
    setPlainParameter(processor, frequencyID, 4200.0f);
    setPlainParameter(processor, driveID, 3.0f);
    processor.getLfoManager().setLfoData(0, shapeB);
    processor.clearModulationForParameter(driveID);
    processor.assignLfoToTarget(3, driveID);
    processor.setModulationDepth(driveID, -0.65f);

    processor.stateAB.toggleAB();
    CHECK(getPlainParameter(processor, NUM_BANDS_ID) == Catch::Approx(2.0f));
    CHECK(getPlainParameter(processor, frequencyID) == Catch::Approx(1180.0f));
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(21.0f));
    auto shapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes[0].points.size() == 3);
    CHECK(shapes[0].points[1].x == Catch::Approx(0.31f));
    auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    auto* routing = findRouting(routings, driveID);
    REQUIRE(routing != nullptr);
    CHECK(routing->sourceLfoIndex == 0);
    CHECK(routing->depth == Catch::Approx(0.3f));

    processor.stateAB.toggleAB();
    CHECK(getPlainParameter(processor, NUM_BANDS_ID) == Catch::Approx(3.0f));
    CHECK(getPlainParameter(processor, frequencyID) == Catch::Approx(4200.0f));
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(3.0f));
    shapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes[0].points.size() == 3);
    CHECK(shapes[0].points[1].x == Catch::Approx(0.73f));
    routings = processor.getLfoManager().getModulationRoutingsCopy();
    routing = findRouting(routings, driveID);
    REQUIRE(routing != nullptr);
    CHECK(routing->sourceLfoIndex == 3);
    CHECK(routing->depth == Catch::Approx(-0.65f));
}

TEST_CASE("Host sessions preserve the inactive A-B snapshot and active side",
          "[state][host][ab][roundtrip][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    const auto shapeB = makeLfoShape(0.24f, 0.81f, 0.18f);
    setPlainParameter(source, driveID, 14.0f);
    source.getLfoManager().setLfoData(0, shapeB);
    source.assignLfoToTarget(0, driveID);
    source.setModulationDepth(driveID, 0.23f);
    source.stateAB.copyAB();

    const auto shapeA = makeLfoShape(0.68f, 0.29f, 0.76f);
    setPlainParameter(source, driveID, 72.0f);
    source.getLfoManager().setLfoData(0, shapeA);
    source.clearModulationForParameter(driveID);
    source.assignLfoToTarget(2, driveID);
    source.setModulationDepth(driveID, -0.61f);
    REQUIRE(source.stateAB.isCurrentA());

    const auto checkLiveState = [&](FireAudioProcessor& processor,
                                    float expectedDrive,
                                    float expectedMiddleX,
                                    int expectedSource,
                                    float expectedDepth)
    {
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(expectedDrive));
        const auto shapes = processor.getLfoManager().getLfoDataCopy();
        REQUIRE(shapes.size() == 4);
        REQUIRE(shapes[0].points.size() == 3);
        CHECK(shapes[0].points[1].x == Catch::Approx(expectedMiddleX));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* routing = findRouting(routings, driveID);
        REQUIRE(routing != nullptr);
        CHECK(routing->sourceLfoIndex == expectedSource);
        CHECK(routing->depth == Catch::Approx(expectedDepth));
    };

    SECTION("session saved while A is active")
    {
        juce::MemoryBlock hostState;
        source.getStateInformation(hostState);
        FireAudioProcessor restored;
        restored.setStateInformation(hostState.getData(),
                                     static_cast<int>(hostState.getSize()));

        REQUIRE(restored.stateAB.isCurrentA());
        checkLiveState(restored, 72.0f, 0.68f, 2, -0.61f);

        restored.stateAB.toggleAB();
        CHECK_FALSE(restored.stateAB.isCurrentA());
        checkLiveState(restored, 14.0f, 0.24f, 0, 0.23f);
    }

    SECTION("session saved while B is active")
    {
        source.stateAB.toggleAB();
        REQUIRE_FALSE(source.stateAB.isCurrentA());
        checkLiveState(source, 14.0f, 0.24f, 0, 0.23f);

        juce::MemoryBlock hostState;
        source.getStateInformation(hostState);
        FireAudioProcessor restored;
        restored.setStateInformation(hostState.getData(),
                                     static_cast<int>(hostState.getSize()));

        REQUIRE_FALSE(restored.stateAB.isCurrentA());
        checkLiveState(restored, 14.0f, 0.24f, 0, 0.23f);

        restored.stateAB.toggleAB();
        CHECK(restored.stateAB.isCurrentA());
        checkLiveState(restored, 72.0f, 0.68f, 2, -0.61f);
    }
}

TEST_CASE("Truncated A-B snapshots fall back to the restored live state",
          "[state][host][ab][corrupt][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(source, driveID, 64.0f);

    juce::MemoryBlock hostState;
    source.getStateInformation(hostState);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        hostState.getData(), static_cast<int>(hostState.getSize()));
    REQUIRE(xml != nullptr);

    auto* abState = xml->getChildByName("AB_STATE");
    REQUIRE(abState != nullptr);
    abState->removeAllAttributes();
    abState->deleteAllChildElements();
    abState->setAttribute("currentSideIsA", false);
    abState->setAttribute(driveID, 0.12);

    juce::MemoryBlock truncatedState;
    juce::AudioProcessor::copyXmlToBinary(*xml, truncatedState);

    FireAudioProcessor restored;
    restored.setStateInformation(truncatedState.getData(),
                                 static_cast<int>(truncatedState.getSize()));

    CHECK(restored.stateAB.isCurrentA());
    CHECK(getPlainParameter(restored, driveID) == Catch::Approx(64.0f));

    restored.stateAB.toggleAB();
    CHECK_FALSE(restored.stateAB.isCurrentA());
    CHECK(getPlainParameter(restored, driveID) == Catch::Approx(64.0f));
}
