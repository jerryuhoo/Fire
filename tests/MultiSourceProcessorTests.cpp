#include <PluginProcessor.h>
#include <Utility/ModulationStateValidation.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
void setMultiParameter(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
juce::MemoryBlock multiHostState(FireAudioProcessor& processor)
{
    juce::MemoryBlock result; processor.getStateInformation(result); return result;
}
void loadMultiHost(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock bytes; juce::AudioProcessor::copyXmlToBinary(xml, bytes);
    processor.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
}
void addMultiRoutes(FireAudioProcessor& processor, const juce::String& target)
{
    auto& manager = processor.getLfoManager();
    REQUIRE(processor.assignLfoToTarget(fire::mod_sources::firstMacro, target) == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.assignLfoToTarget(fire::mod_sources::firstMacro + 1, target) == LfoManager::AssignmentResult::changed);
    const auto snapshot = manager.getModulationRoutingStateSnapshot();
    for (int index = 0; index < snapshot.routings.size(); ++index)
    {
        auto current = manager.getModulationRoutingStateSnapshot();
        auto route = current.routings[index];
        if (route.targetParameterID != target) continue;
        route.depth = .2f; route.isBipolar = false;
        REQUIRE(manager.updateModulationRoutingIfRevisionMatches(index, current.revision, current.routings[index], route).accepted);
    }
}
}

TEST_CASE("Multi-source host state round trips mappings and rejects damaged new schemas atomically",
          "[multi-source][state][host][mapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source; addMultiRoutes(source, "drive1");
    auto snapshot = source.getLfoManager().getModulationRoutingStateSnapshot();
    const auto found = std::find_if(snapshot.routings.begin(), snapshot.routings.end(), [](const auto& r) {return r.targetParameterID == "drive1";});
    REQUIRE(found != snapshot.routings.end());
    const auto index = static_cast<int>(found - snapshot.routings.begin());
    auto mapping = *found; mapping.customRange = true; mapping.rangeStart = -.2f; mapping.rangeEnd = .6f;
    mapping.curve = 3; mapping.depthMacro = 2;
    REQUIRE(source.getLfoManager().updateModulationRoutingIfRevisionMatches(index, snapshot.revision, *found, mapping).accepted);
    const auto saved = multiHostState(source);
    auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize())); REQUIRE(xml);
    REQUIRE(xml->getIntAttribute("stateFormatVersion") == 5);
    FireAudioProcessor restored; loadMultiHost(restored, *xml);
    CHECK(restored.getModulationInfoForParameter("drive1").routeCount == 2);
    const auto restoredRoutes = restored.getLfoManager().getModulationRoutingsCopy();
    CHECK(std::any_of(restoredRoutes.begin(), restoredRoutes.end(), [&](const auto& r) {return r.hasSameState(mapping);}));
    const auto baseline = multiHostState(restored);
    for (const auto& pair : std::array<std::pair<const char*, const char*>, 5>{{
        {"rangeStart", "nan"}, {"rangeEnd", "2"}, {"curve", "1.5"}, {"depthMacro", "8"}, {"source", "17e0"}}})
    {
        CAPTURE(pair.first, pair.second);
        auto bad = std::make_unique<juce::XmlElement>(*xml);
        bad->getChildByName("MODULATION_STATE")->getChildElement(index)->setAttribute(pair.first, pair.second);
        loadMultiHost(restored, *bad);
        CHECK(multiHostState(restored) == baseline);
    }
}

TEST_CASE("Summed macros reach real Band Shape controls in Base and HQ",
          "[multi-source][hq][processor][dsp]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto* target : {"drive1", "bias1", "rec1", "shapeMix1"})
        for (bool hq : {false, true})
        {
            CAPTURE(target, hq);
            auto actual = std::make_unique<FireAudioProcessor>(), reference = std::make_unique<FireAudioProcessor>();
            for (auto* processor : {actual.get(), reference.get()})
            {
                processor->setRateAndBufferSizeDetails(48000, 128);
                setMultiParameter(*processor, NUM_BANDS_ID, 1);
                setMultiParameter(*processor, "hq", hq ? 1.0f : 0.0f);
                setMultiParameter(*processor, MIX_ID, 1);
                setMultiParameter(*processor, OUTPUT_ID, 0);
                setMultiParameter(*processor, FILTER_BYPASS_ID, 0);
                setMultiParameter(*processor, DOWNSAMPLE_BYPASS_ID, 0);
                for (const auto* id : {BAND_ENABLE_ID, DRIVE_BYPASS_ID, SHAPE_BYPASS_ID})
                    setMultiParameter(*processor, ParameterIDAndName::getIDString(id,0), 1);
                for (const auto* id : {EXTREME_ID, COMP_BYPASS_ID, WIDTH_BYPASS_ID, DC_FILTER_ID})
                    setMultiParameter(*processor, ParameterIDAndName::getIDString(id,0), 0);
                setMultiParameter(*processor, "safe1", 0);
                setMultiParameter(*processor, "linked1", 0);
                setMultiParameter(*processor, "driveCompModern1", 0);
                setMultiParameter(*processor, "mode1", 2);
                setMultiParameter(*processor, "mix1", 1);
                setMultiParameter(*processor, "output1", 0);
                setMultiParameter(*processor, "drive1", 8);
                setMultiParameter(*processor, "bias1", -.1f);
                setMultiParameter(*processor, "rec1", .1f);
                setMultiParameter(*processor, "shapeMix1", .25f);
            }
            setMultiParameter(*actual, "macro1", .25f); setMultiParameter(*actual, "macro2", .75f);
            addMultiRoutes(*actual, target);
            auto* targetParameter = reference->treeState.getParameter(target); REQUIRE(targetParameter);
            targetParameter->setValueNotifyingHost(targetParameter->getValue() + .2f);
            actual->prepareToPlay(48000, 128); reference->prepareToPlay(48000, 128);
            juce::MidiBuffer midi;
            float error = 0;
            for (int block = 0; block < 80; ++block)
            {
                juce::AudioBuffer<float> a(2,128), b(2,128);
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < 128; ++sample)
                        a.setSample(channel, sample, .12f * std::sin(float(block * 128 + sample) * .071f + channel * .2f));
                b.makeCopyOf(a); actual->processBlock(a, midi); reference->processBlock(b, midi);
                if (block < 64) continue;
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < 128; ++sample)
                    {
                        REQUIRE(std::isfinite(a.getSample(channel,sample)));
                        error = juce::jmax(error, std::abs(a.getSample(channel,sample) - b.getSample(channel,sample)));
                    }
            }
            CHECK(error < 3.0e-4f);
        }
}
