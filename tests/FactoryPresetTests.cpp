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

TEST_CASE("Factory library offers two hundred complete scenes in ten collections without changing the live sound", "[factory-presets][preset][state]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.statePresets.getNumFactoryPresets() == 200);
    juce::MemoryBlock before, after;
    processor.getStateInformation(before);
    juce::StringArray keys, categories;
    std::map<juce::String, int> categoryCounts;
    juce::StringArray parameterSignatures;
    for (int index = 0; index < 200; ++index)
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
        if (index >= 12)
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
    keys.removeDuplicates(false); CHECK(keys.size() == 200); CHECK(categories.size() == 10);
    parameterSignatures.removeDuplicates(false); CHECK(parameterSignatures.size() == 200);
    for (const auto& [category, count] : categoryCounts) {CAPTURE(category); CHECK(count == 20);}

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
    REQUIRE(library.getNumPresets() == 201);
    const auto entries = library.getBrowserEntries();
    REQUIRE_FALSE(entries.front().factory);
    REQUIRE(library.loadPreset(entries.front().tag));
    CHECK(processor.treeState.getParameter(driveID)->getValue() == .23f);
    CHECK(library.getCurrentPresetKey() == "My Scene.fire");
    library.setCurrentPresetId(1); library.deletePreset(); CHECK(library.getNumPresets() == 200);
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
    REQUIRE(menu.getNumItems() == 200);
    REQUIRE(library.loadPreset(library.comboBoxIdToTagNameMap[1]));
    const auto key = library.getCurrentPresetKey();
    CHECK(library.getCurrentPresetDescription().contains("drum"));
    const auto userFile = folder.file.getChildFile("User.fire");
    REQUIRE(library.savePreset(userFile).isNotEmpty());
    library.setCurrentPresetKey(key);
    menu.clear(); library.setPresetAndFolderNames(menu);
    REQUIRE(library.getCurrentPresetId() > 1);
    CHECK(library.getNumPresets() == 201);
    library.deletePreset();
    CHECK(library.getNumPresets() == 201);
    CHECK(library.getCurrentPresetKey() == key);
    library.scanAllPresets();
    menu.clear(); library.setPresetAndFolderNames(menu);
    CHECK(library.getCurrentPresetKey() == key);
    CHECK(library.getNumPresets() == 201);
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
    for (int index = 0; index < 200; ++index)
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
