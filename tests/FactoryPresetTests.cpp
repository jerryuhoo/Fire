#include <PluginProcessor.h>
#include <Utility/FactoryPresets.h>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
void gather(const juce::XmlElement& node, juce::StringArray& tags)
{
    for (auto* child : node.getChildIterator())
        if (child->getBoolAttribute("factoryPreset")) tags.add(child->getTagName());
        else if (child->hasTagName("FOLDER")) gather(*child, tags);
}
struct Folder
{
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile("TestTemp")
        .getChildFile("FactoryPresets-" + juce::Uuid().toString());
    Folder() { REQUIRE(file.createDirectory().wasOk()); }
    ~Folder() { file.deleteRecursively(); }
};
}

TEST_CASE("Factory library offers twelve complete categorized scenes without changing the live sound", "[factory-presets][preset][state]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.statePresets.getNumFactoryPresets() == 12);
    juce::MemoryBlock before, after;
    processor.getStateInformation(before);
    auto presets = fire::factory::create(processor);
    processor.getStateInformation(after);
    CHECK(before == after);
    REQUIRE(presets.size() == 12);
    juce::StringArray keys, categories;
    for (auto& preset : presets)
    {
        CHECK(state::canLoadStateFromXml(*preset, processor));
        CHECK(preset->getStringAttribute("presetDescription").isNotEmpty());
        CHECK(preset->getStringAttribute("presetKey").startsWith("@factory/"));
        keys.add(preset->getStringAttribute("presetKey"));
        categories.addIfNotAlreadyThere(preset->getStringAttribute("presetCategory"));
    }
    keys.removeDuplicates(false);
    CHECK(keys.size() == 12); CHECK(categories.size() == 6);
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
    REQUIRE(menu.getNumItems() == 12);
    REQUIRE(library.loadPreset(library.comboBoxIdToTagNameMap[1]));
    const auto key = library.getCurrentPresetKey();
    CHECK(library.getCurrentPresetDescription().contains("drum"));
    const auto userFile = folder.file.getChildFile("User.fire");
    REQUIRE(library.savePreset(userFile).isNotEmpty());
    library.setCurrentPresetKey(key);
    menu.clear(); library.setPresetAndFolderNames(menu);
    REQUIRE(library.getCurrentPresetId() > 1);
    CHECK(library.getNumPresets() == 13);
    library.deletePreset();
    CHECK(library.getNumPresets() == 13);
    CHECK(library.getCurrentPresetKey() == key);
    library.scanAllPresets();
    menu.clear(); library.setPresetAndFolderNames(menu);
    CHECK(library.getCurrentPresetKey() == key);
    CHECK(library.getNumPresets() == 13);
    juce::MemoryBlock host; processor.getStateInformation(host);
    FireAudioProcessor restored;
    restored.setStateInformation(host.getData(), static_cast<int>(host.getSize()));
    juce::ComboBox restoredMenu; restored.statePresets.setPresetAndFolderNames(restoredMenu);
    CHECK(restored.statePresets.getCurrentPresetKey() == key);
}

TEST_CASE("Every factory scene renders finite non-silent audio with a bounded output", "[factory-presets][dsp]")
{
    FireAudioProcessor processor;
    auto presets = fire::factory::create(processor);
    juce::AudioBuffer<float> block(2, 128);
    juce::MidiBuffer midi;
    for (auto& preset : presets)
    {
        CAPTURE(preset->getStringAttribute("presetName"));
        REQUIRE(state::loadStateFromXml(*preset, processor));
        CHECK(processor.isCurrentStateEquivalentToPreset(*preset));
        processor.setRateAndBufferSizeDetails(48000, 128);
        processor.prepareToPlay(48000, 128);
        double energy = 0, peak = 0;
        bool finite = true;
        for (int offset = 0; offset < 96000; offset += 128)
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
