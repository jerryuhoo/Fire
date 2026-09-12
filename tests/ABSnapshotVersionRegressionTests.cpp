#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
void setPlain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float getPlain(FireAudioProcessor& processor, const juce::String& id)
{
    const auto* parameter = processor.treeState.getRawParameterValue(id);
    REQUIRE(parameter != nullptr);
    return parameter->load();
}

std::unique_ptr<juce::XmlElement> hostXml(FireAudioProcessor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(xml != nullptr);
    return xml;
}

void restore(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary(xml, state);
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
}
}

TEST_CASE("Invalid alternate preset versions fall back to the complete restored live state",
          "[state][host][ab][version][corrupt][ott][insertfx][module-order][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto* invalidVersion : { "3", "-1", "1.5", "2oops" })
    {
        CAPTURE(invalidVersion);
        FireAudioProcessor source;
        REQUIRE(source.addInsertEffect(1, fire::effects::Type::delay) == 0);
        source.moveModuleBefore(1, fire::module_order::firstInsert, 0);
        setPlain(source, "drive1", 11.0f);
        setPlain(source, "ottEnabled1", 1.0f);
        setPlain(source, "ottDepth1", 0.25f);
        source.stateAB.copyAB(false);

        // The main state and inactive state contain different complete modern
        // module graphs. Only the inactive state's format marker is damaged.
        source.removeInsertEffect(1, 0);
        REQUIRE(source.addInsertEffect(1, fire::effects::Type::chorus) == 0);
        source.moveModuleBefore(1, 2, 0);
        setPlain(source, "drive1", 64.0f);
        setPlain(source, "ottDepth1", 0.82f);
        const auto savedOrder = source.getModuleOrder(1);
        auto xml = hostXml(source);
        auto* alternate = xml->getChildByName("AB_STATE");
        REQUIRE(alternate != nullptr);
        alternate->setAttribute("presetFormatVersion", invalidVersion);
        alternate->setAttribute("currentSideIsA", false);

        FireAudioProcessor restored;
        restore(restored, *xml);
        CHECK(restored.stateAB.isCurrentA());
        CHECK(getPlain(restored, "drive1") == Catch::Approx(64.0f));
        CHECK(getPlain(restored, "ottDepth1") == Catch::Approx(0.82f));
        CHECK(restored.getInsertEffectType(1, 0) == fire::effects::Type::chorus);
        CHECK(restored.getModuleOrder(1) == savedOrder);

        // Edit the active side before switching: fallback B must retain the
        // restored live sound. An accepted-but-unloadable B would leave these
        // edits in place while flipping the label and discarding the snapshot.
        setPlain(restored, "drive1", 37.0f);
        setPlain(restored, "ottDepth1", 0.15f);
        restored.removeInsertEffect(1, 0);
        restored.moveModuleBefore(1, 3, 0);
        const auto editedOrder = restored.getModuleOrder(1);
        restored.stateAB.toggleAB();
        CHECK_FALSE(restored.stateAB.isCurrentA());
        CHECK(getPlain(restored, "drive1") == Catch::Approx(64.0f));
        CHECK(getPlain(restored, "ottDepth1") == Catch::Approx(0.82f));
        CHECK(restored.getInsertEffectType(1, 0) == fire::effects::Type::chorus);
        CHECK(restored.getModuleOrder(1) == savedOrder);

        restored.stateAB.toggleAB();
        CHECK(restored.stateAB.isCurrentA());
        CHECK(getPlain(restored, "drive1") == Catch::Approx(37.0f));
        CHECK(getPlain(restored, "ottDepth1") == Catch::Approx(0.15f));
        CHECK(restored.getInsertEffectType(1, 0) == fire::effects::Type::none);
        CHECK(restored.getModuleOrder(1) == editedOrder);
    }
}

TEST_CASE("Supported alternate preset versions preserve modern modules across A-B restore",
          "[state][host][ab][version][legacy][ott][insertfx][module-order][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto version : { -1, 0, 1, 2 })
    {
        CAPTURE(version);
        FireAudioProcessor source;
        REQUIRE(source.addInsertEffect(1, fire::effects::Type::delay) == 0);
        source.moveModuleBefore(1, fire::module_order::firstInsert, 0);
        setPlain(source, "ottEnabled1", 1.0f);
        setPlain(source, "ottDepth1", 0.25f);
        const auto alternateOrder = source.getModuleOrder(1);
        source.stateAB.copyAB(false);
        source.removeInsertEffect(1, 0);
        setPlain(source, "ottDepth1", 0.82f);
        auto xml = hostXml(source);
        auto* alternate = xml->getChildByName("AB_STATE");
        REQUIRE(alternate != nullptr);
        if (version < 0)
            alternate->removeAttribute("presetFormatVersion");
        else
            alternate->setAttribute("presetFormatVersion", version);
        alternate->setAttribute("currentSideIsA", false);

        FireAudioProcessor restored;
        restore(restored, *xml);
        CHECK_FALSE(restored.stateAB.isCurrentA());
        CHECK(restored.getInsertEffectType(1, 0) == fire::effects::Type::none);
        CHECK(getPlain(restored, "ottDepth1") == Catch::Approx(0.82f));
        restored.stateAB.toggleAB();
        CHECK(restored.stateAB.isCurrentA());
        CHECK(restored.getInsertEffectType(1, 0) == fire::effects::Type::delay);
        CHECK(getPlain(restored, "ottDepth1") == Catch::Approx(0.25f));
        CHECK(restored.getModuleOrder(1) == alternateOrder);
    }
}
