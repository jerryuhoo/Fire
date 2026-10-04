#include <PluginProcessor.h>
#include <Utility/FactoryPresets.h>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <map>

namespace
{
struct Folder
{
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile("TestTemp")
        .getChildFile("FactoryPresets-" + juce::Uuid().toString());
    Folder() { REQUIRE(file.createDirectory().wasOk()); }
    ~Folder() { file.deleteRecursively(); }
};
}

TEST_CASE("Factory library offers 236 complete scenes in eleven collections without changing the live sound", "[factory-presets][preset][state]")
{
    FireAudioProcessor processor;
    CHECK(fire::factory::presetCount == 236);
    REQUIRE(processor.statePresets.getNumFactoryPresets() == fire::factory::presetCount);
    juce::MemoryBlock before, after;
    processor.getStateInformation(before);
    juce::StringArray keys, categories;
    std::map<juce::String, int> categoryCounts;
    juce::StringArray parameterSignatures;
    for (int index = 0; index < fire::factory::presetCount; ++index)
    {
        auto presets = fire::factory::create(processor, index); REQUIRE(presets.size() == 1);
        const auto& preset = *presets.front();
        CHECK(state::canLoadStateFromXml(preset, processor));
        CHECK(preset.getStringAttribute("presetDescription").isNotEmpty());
        CHECK(preset.getStringAttribute("presetKey").startsWith("@factory/"));
        keys.add(preset.getStringAttribute("presetKey"));
        const auto category = preset.getStringAttribute("presetCategory");
        categories.addIfNotAlreadyThere(category); ++categoryCounts[category];
        auto parametersOnly = preset;
        for (const auto* attribute : {"presetName", "presetDescription", "presetKey", "presetCategory"})
            parametersOnly.removeAttribute(attribute);
        parametersOnly.setTagName("WINGSFIRE");
        parameterSignatures.add(juce::String(parametersOnly.toString().hashCode64()));
        if (index >= 12 && index < fire::factory::basePresetCount)
        {
            const auto& definition = fire::factory::definitions[static_cast<size_t>(index)];
            juce::StringArray macroSources;
            for (auto* routing : preset.getChildByName("MODULATION_STATE")->getChildIterator())
                if (routing->getIntAttribute("source") >= fire::mod_sources::firstMacro)
                    macroSources.addIfNotAlreadyThere(juce::String(routing->getIntAttribute("source")));
            CHECK(macroSources.size() == 4);
            const int band = definition.categoryIndex == 1 || definition.categoryIndex == 3 ? 1 : 0;
            const auto id = fire::analog_params::bandID(band);
            REQUIRE(processor.treeState.getParameter(id));
            CHECK(processor.treeState.getParameter(id)->convertFrom0to1(static_cast<float>(preset.getDoubleAttribute(id))) == definition.colourModel + 1);
            for (int slot = 0; slot < 2; ++slot)
                if (preset.getDoubleAttribute(fire::effects::parameterID(0, slot, fire::effects::typeField))
                    == processor.treeState.getParameter(fire::effects::parameterID(0, slot, fire::effects::typeField))->convertTo0to1(static_cast<float>(fire::effects::Type::reverb)))
                {
                    const auto model = fire::reverb_params::parameterID(0, slot);
                    CHECK(processor.treeState.getParameter(model)->convertFrom0to1(static_cast<float>(preset.getDoubleAttribute(model))) == definition.spaceModel);
                }
        }
    }
    processor.getStateInformation(after); CHECK(before == after);
    keys.removeDuplicates(false); CHECK(keys.size() == fire::factory::presetCount); CHECK(categories.size() == 11);
    parameterSignatures.removeDuplicates(false); CHECK(parameterSignatures.size() == fire::factory::presetCount);
    for (const auto& [category, count] : categoryCounts) {CAPTURE(category); CHECK(count == (category == "Analog Drive" ? 36 : 20));}

}

TEST_CASE("Analog Drive scenes cover all twelve colours with valid macros and one active colour stage",
          "[factory-presets][analog-presets][preset][state][routing]")
{
    FireAudioProcessor processor;
    std::array<int, 12> models{};
    int masterScenes = 0, splitScenes = 0, envelopeScenes = 0;
    const auto value = [&](const juce::String& id)
    {
        auto* parameter = processor.treeState.getParameter(id);
        REQUIRE(parameter);
        return parameter->convertFrom0to1(parameter->getValue());
    };
    for (int index = fire::factory::basePresetCount; index < fire::factory::presetCount; ++index)
    {
        auto presets = fire::factory::create(processor, index);
        REQUIRE(presets.size() == 1);
        const auto& preset = *presets.front();
        CAPTURE(preset.getStringAttribute("presetName"));
        REQUIRE(state::loadStateFromXml(preset, processor));
        CHECK(preset.getStringAttribute("presetCategory") == "Analog Drive");
        CHECK(preset.getStringAttribute("presetKey").startsWith("@factory/analog-"));
        CHECK(processor.isCurrentStateEquivalentToPreset(preset));
        CHECK(value(HQ_ID) == 1);
        int colourStages = 0, model = -1;
        for (int band = 0; band < juce::roundToInt(value(NUM_BANDS_ID)); ++band)
            if (value(ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, band)) > .5f)
            {
                model = processor.getShapeMode(band + 1, -1) - fire::analog::legacyCount;
                ++colourStages;
                CHECK(value(ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, band)) == 1);
                CHECK(value(ParameterIDAndName::getIDString(SAFE_ID, band)) == 0);
                CHECK(value(ParameterIDAndName::getIDString(LINKED_ID, band)) == 0);
                CHECK(value(ParameterIDAndName::getIDString(SHAPE_MIX_ID, band)) == 1);
            }
        for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
            for (int slot = 0; slot < fire::effects::slotCount; ++slot)
                if (processor.getInsertEffectType(scope, slot) == fire::effects::Type::shape)
                {
                    model = processor.getShapeMode(scope, slot) - fire::analog::legacyCount;
                    ++colourStages;
                    if (scope == 0) ++masterScenes;
                }
        REQUIRE(colourStages == 1);
        REQUIRE(model >= 0); REQUIRE(model < 12);
        ++models[static_cast<size_t>(model)];
        if (value(NUM_BANDS_ID) == 2)
        {
            ++splitScenes;
            CHECK(value("driveBypass1") == 0); CHECK(value("shapeBypass1") == 0);
            CHECK(value("drive1") == 0);
        }
        std::array<int, 4> macroRoutes{};
        juce::StringArray targets;
        for (auto* route : preset.getChildByName("MODULATION_STATE")->getChildIterator())
        {
            const auto target = route->getStringAttribute("target");
            REQUIRE(processor.treeState.getParameter(target));
            CHECK_FALSE(targets.contains(target)); targets.add(target);
            CHECK_FALSE(route->getBoolAttribute("bipolar"));
            CHECK_FALSE(route->getBoolAttribute("bypassed"));
            const auto source = route->getIntAttribute("source");
            if (source == fire::mod_sources::envelope) ++envelopeScenes;
            else
            {
                REQUIRE(source >= fire::mod_sources::firstMacro);
                REQUIRE(source < fire::mod_sources::sourceCount);
                ++macroRoutes[static_cast<size_t>(source - fire::mod_sources::firstMacro)];
            }
        }
        for (const auto count : macroRoutes) CHECK(count == 1);
    }
    for (const auto count : models) CHECK(count == 3);
    CHECK(masterScenes == 5); CHECK(splitScenes == 8); CHECK(envelopeScenes == 7);
}

TEST_CASE("Every Analog Drive macro changes real HQ audio and full macro settings remain bounded",
          "[factory-presets][analog-presets][dsp][hq][routing]")
{
    FireAudioProcessor processor;
    constexpr int frames = 4096, blockSize = 128;
    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;
    for (const double sampleRate : {44100.0, 48000.0, 96000.0})
    for (int index = fire::factory::basePresetCount; index < fire::factory::presetCount; ++index)
    {
        CAPTURE(sampleRate);
        auto preset = fire::factory::create(processor, index);
        CAPTURE(preset.front()->getStringAttribute("presetName"));
        auto withoutEnvelope = std::make_unique<juce::XmlElement>(*preset.front());
        bool hasEnvelope = false;
        for (auto* route : withoutEnvelope->getChildByName("MODULATION_STATE")->getChildIterator())
            if (route->getIntAttribute("source") == fire::mod_sources::envelope)
            {route->setAttribute("bypassed", true); hasEnvelope = true;}
        std::vector<float> reference;
        for (int setting = -1; setting < (hasEnvelope ? 6 : 5); ++setting)
        {
            CAPTURE(setting);
            REQUIRE(state::loadStateFromXml(setting == 5 ? *withoutEnvelope : *preset.front(), processor));
            for (int macro = 0; macro < 4; ++macro)
                processor.treeState.getParameter("macro" + juce::String(macro + 1))->setValueNotifyingHost(
                    setting == macro || setting == 4 ? 1.0f : 0.0f);
            processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
            processor.prepareToPlay(sampleRate, blockSize);
            for (int silent = 0; silent < 4; ++silent)
            {
                block.clear(); processor.processBlock(block, midi);
                // Float processing may leave sub-ulp residuals; require the
                // complete chain's silent output below -120 dBFS.
                CHECK(block.getMagnitude(0, blockSize) < 1e-6f);
            }
            double error = 0, energy = 0, peak = 0;
            bool finite = true;
            for (int offset = 0; offset < frames; offset += blockSize)
            {
                for (int sample = 0; sample < blockSize; ++sample)
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        const auto t = (offset + sample) / sampleRate;
                        const auto input = .12 * std::sin(6.283185307179586 * 165 * t + channel * .2)
                            + .07 * std::sin(6.283185307179586 * 1500 * t)
                            + .04 * std::sin(6.283185307179586 * 5000 * t);
                        block.setSample(channel, sample, static_cast<float>(input));
                    }
                processor.processBlock(block, midi);
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    const auto output = block.getSample(0, sample);
                    finite = finite && std::isfinite(output);
                    peak = std::max(peak, static_cast<double>(std::abs(output)));
                    energy += output * output;
                    if (setting == -1) reference.push_back(output);
                    else error += std::pow(output - reference[static_cast<size_t>(offset + sample)], 2);
                }
            }
            CHECK(finite); CHECK(peak < 2); CHECK(energy > .01);
            if (setting >= 0) CHECK(error > 1e-5);
            processor.releaseResources();
        }
    }
}

TEST_CASE("Exported factory scenes are editable user presets and cannot become virtual recipes", "[factory-presets][preset][filesystem]")
{
    FireAudioProcessor processor;
    Folder folder;
    auto preset = fire::factory::create(processor, 12);
    preset.front()->setTagName("WINGSFIRE");
    const auto driveID = juce::String("drive1");
    preset.front()->setAttribute(driveID, .23f);
    preset.front()->setAttribute("factoryVirtual", true);
    preset.front()->setAttribute("factoryRecipeIndex", 199);
    auto file = folder.file.getChildFile("My Scene.fire");
    REQUIRE(preset.front()->writeTo(file));
    auto& library = processor.statePresets;
    library.setPresetDirectoryForTesting(folder.file); library.enableFactoryPresets();
    juce::ComboBox menu; library.setPresetAndFolderNames(menu);
    REQUIRE(library.getNumPresets() == fire::factory::presetCount + 1);
    const auto entries = library.getBrowserEntries();
    REQUIRE_FALSE(entries.front().factory);
    REQUIRE(library.loadPreset(entries.front().tag));
    CHECK(processor.treeState.getParameter(driveID)->getValue() == .23f);
    CHECK(library.getCurrentPresetKey() == "My Scene.fire");
    library.setCurrentPresetId(1); library.deletePreset(); CHECK(library.getNumPresets() == fire::factory::presetCount);
}

TEST_CASE("Factory keys survive user-library rescans and read-only scenes cannot be deleted", "[factory-presets][preset][filesystem]")
{
    FireAudioProcessor processor;
    Folder folder;
    auto& library = processor.statePresets;
    library.setPresetDirectoryForTesting(folder.file);
    library.enableFactoryPresets();
    CHECK(folder.file.findChildFiles(juce::File::findFiles, true, "*.fire").isEmpty());
    juce::ComboBox menu; library.setPresetAndFolderNames(menu);
    REQUIRE(menu.getNumItems() == fire::factory::presetCount);
    REQUIRE(library.loadPreset(library.comboBoxIdToTagNameMap[1]));
    const auto key = library.getCurrentPresetKey();
    CHECK(library.getCurrentPresetDescription().contains("drum"));
    const auto userFile = folder.file.getChildFile("User.fire");
    REQUIRE(library.savePreset(userFile).isNotEmpty());
    library.setCurrentPresetKey(key);
    menu.clear(); library.setPresetAndFolderNames(menu);
    REQUIRE(library.getCurrentPresetId() > 1);
    CHECK(library.getNumPresets() == fire::factory::presetCount + 1);
    library.deletePreset();
    CHECK(library.getNumPresets() == fire::factory::presetCount + 1);
    CHECK(library.getCurrentPresetKey() == key);
    library.scanAllPresets();
    menu.clear(); library.setPresetAndFolderNames(menu);
    CHECK(library.getCurrentPresetKey() == key);
    CHECK(library.getNumPresets() == fire::factory::presetCount + 1);
    juce::MemoryBlock host; processor.getStateInformation(host);
    FireAudioProcessor restored;
    restored.setStateInformation(host.getData(), static_cast<int>(host.getSize()));
    juce::ComboBox restoredMenu; restored.statePresets.setPresetAndFolderNames(restoredMenu);
    CHECK(restored.statePresets.getCurrentPresetKey() == key);
}

TEST_CASE("Every factory scene renders finite non-silent audio with a bounded output", "[factory-presets][dsp]")
{
    FireAudioProcessor processor;

    juce::AudioBuffer<float> block(2, 128);
    juce::MidiBuffer midi;
    for (int index = 0; index < fire::factory::presetCount; ++index)
    {
        auto presets = fire::factory::create(processor, index); auto& preset = presets.front();
        CAPTURE(preset->getStringAttribute("presetName"));
        REQUIRE(state::loadStateFromXml(*preset, processor));
        CHECK(processor.isCurrentStateEquivalentToPreset(*preset));
        processor.setRateAndBufferSizeDetails(48000, 128);
        processor.prepareToPlay(48000, 128);
        double energy = 0, peak = 0;
        bool finite = true;
        for (int offset = 0; offset < 12288; offset += 128)
        {
            for (int frame = 0; frame < 128; ++frame)
                for (int channel = 0; channel < 2; ++channel)
                {
                    const double time = static_cast<double>(offset + frame) / 48000;
                    const auto value = 0.12 * std::sin(6.283185307179586 * 165 * time + channel * 0.2)
                        + 0.04 * std::sin(6.283185307179586 * 1500 * time)
                        + 0.025 * std::sin(6.283185307179586 * 5000 * time);
                    block.setSample(channel, frame, static_cast<float>(value));
                }
            processor.processBlock(block, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int frame = 0; frame < 128; ++frame)
                {
                    const auto value = block.getSample(channel, frame);
                    finite = finite && std::isfinite(value);
                    peak = std::max(peak, static_cast<double>(std::abs(value)));
                    energy += value * value;
                }
        }
        CHECK(finite); CHECK(energy > 0.1); CHECK(peak < 2.0);
    }
}
