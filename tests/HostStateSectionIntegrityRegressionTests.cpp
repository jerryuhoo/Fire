#include "../Source/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace
{
juce::String driveParameterID()
{
    return ParameterIDAndName::getIDString(DRIVE_ID, 0);
}

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float value)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float getPlainParameter(const FireAudioProcessor& processor,
                        const juce::String& parameterID)
{
    const auto* value = processor.treeState.getRawParameterValue(parameterID);
    REQUIRE(value != nullptr);
    return value->load(std::memory_order_relaxed);
}

LfoData makeShape(float x, float y, float smoothness)
{
    LfoData shape;
    shape.points = { { 0.0f, 0.0f }, { x, y }, { 1.0f, 0.0f } };
    shape.curvatures = { 0.17f, -0.29f };
    shape.smoothness = smoothness;
    return shape;
}

void setShape(FireAudioProcessor& processor,
              int index,
              const LfoData& shape)
{
    setPlainParameter(
        processor,
        ParameterIDAndName::getIDString(LFO_SMOOTH_ID, index),
        shape.smoothness);
    processor.getLfoManager().setLfoData(index, shape);
}

juce::MemoryBlock serialiseHostState(FireAudioProcessor& processor)
{
    juce::MemoryBlock result;
    processor.getStateInformation(result);
    REQUIRE(result.getSize() > 0);
    return result;
}

std::unique_ptr<juce::XmlElement> parseHostState(
    const juce::MemoryBlock& state)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(xml != nullptr);
    REQUIRE(xml->hasAttribute("stateFormatVersion"));
    return xml;
}

void loadHostState(FireAudioProcessor& processor,
                   const juce::XmlElement& xml)
{
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary(xml, state);
    processor.setStateInformation(state.getData(),
                                  static_cast<int>(state.getSize()));
}

void seedBaselineState(FireAudioProcessor& processor)
{
    const auto driveID = driveParameterID();

    setPlainParameter(processor, driveID, 17.0f);
    setShape(processor, 1, makeShape(0.23f, 0.76f, 0.21f));
    processor.assignLfoToTarget(1, driveID);
    processor.setModulationDepth(driveID, 0.22f);
    processor.stateAB.copyAB(false);

    setPlainParameter(processor, driveID, 31.0f);
    setShape(processor, 2, makeShape(0.41f, 0.87f, 0.58f));
    processor.assignLfoToTarget(2, driveID);
    processor.setModulationDepth(driveID, -0.36f);
    processor.setSavedWidth(1410);
    processor.setSavedHeight(805);
    processor.statePresets.setCurrentPresetKey("Factory/Baseline.fire");
}

void seedIncomingState(FireAudioProcessor& processor)
{
    const auto driveID = driveParameterID();
    setPlainParameter(processor, driveID, 73.0f);
    setShape(processor, 0, makeShape(0.67f, 0.19f, 0.44f));
    processor.assignLfoToTarget(0, driveID);
    processor.setModulationDepth(driveID, 0.72f);
    processor.setSavedWidth(999);
    processor.setSavedHeight(600);
    processor.statePresets.setCurrentPresetKey("Factory/Incoming.fire");
}

juce::XmlElement* findRoutingForTarget(juce::XmlElement& state,
                                       const juce::String& target)
{
    auto* routingState = state.getChildByName("MODULATION_STATE");
    REQUIRE(routingState != nullptr);
    for (auto* routing : routingState->getChildIterator())
        if (routing->hasTagName("ROUTING")
            && routing->getStringAttribute("target") == target)
            return routing;
    return nullptr;
}
} // namespace

TEST_CASE("Versioned host states reject malformed model sections atomically",
          "[state][host][corrupt][versioned][transaction][section-integrity]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    seedBaselineState(subject);
    const auto baselineState = serialiseHostState(subject);

    FireAudioProcessor incoming;
    seedIncomingState(incoming);
    auto incomingXml = parseHostState(serialiseHostState(incoming));

    SECTION("truncated LFO state")
    {
        auto* lfoState = incomingXml->getChildByName("LFO_STATE");
        REQUIRE(lfoState != nullptr);
        REQUIRE(lfoState->getNumChildElements() == 4);
        lfoState->removeChildElement(lfoState->getChildElement(3), true);
    }

    SECTION("duplicate LFO index")
    {
        auto* lfoState = incomingXml->getChildByName("LFO_STATE");
        REQUIRE(lfoState != nullptr);
        auto* first = lfoState->getChildElement(0);
        auto* last = lfoState->getChildElement(3);
        REQUIRE(first != nullptr);
        REQUIRE(last != nullptr);
        last->setAttribute("index", first->getIntAttribute("index"));
    }

    SECTION("duplicate LFO section")
    {
        auto* lfoState = incomingXml->getChildByName("LFO_STATE");
        REQUIRE(lfoState != nullptr);
        incomingXml->addChildElement(new juce::XmlElement(*lfoState));
    }

    SECTION("malformed routing")
    {
        auto* routing = findRoutingForTarget(*incomingXml, driveParameterID());
        REQUIRE(routing != nullptr);
        routing->setAttribute("depth", "0.5oops");
    }

    SECTION("duplicate populated routing")
    {
        auto* routingState = incomingXml->getChildByName("MODULATION_STATE");
        auto* routing = findRoutingForTarget(*incomingXml, driveParameterID());
        REQUIRE(routingState != nullptr);
        REQUIRE(routing != nullptr);
        routingState->addChildElement(new juce::XmlElement(*routing));
    }

    SECTION("duplicate modulation section")
    {
        auto* routingState = incomingXml->getChildByName("MODULATION_STATE");
        REQUIRE(routingState != nullptr);
        incomingXml->addChildElement(new juce::XmlElement(*routingState));
    }

    loadHostState(subject, *incomingXml);
    CHECK(serialiseHostState(subject) == baselineState);
}

TEST_CASE("An empty versioned modulation section explicitly clears routings",
          "[state][host][versioned][section-integrity][empty-routing]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    seedBaselineState(subject);
    REQUIRE_FALSE(subject.getLfoManager().getModulationRoutingsCopy().isEmpty());

    FireAudioProcessor incoming;
    seedIncomingState(incoming);
    auto incomingXml = parseHostState(serialiseHostState(incoming));
    auto* routingState = incomingXml->getChildByName("MODULATION_STATE");
    REQUIRE(routingState != nullptr);
    routingState->deleteAllChildElements();

    loadHostState(subject, *incomingXml);

    CHECK(getPlainParameter(subject, driveParameterID())
          == Catch::Approx(73.0f));
    CHECK(subject.getLfoManager().getModulationRoutingsCopy().isEmpty());
    const auto shapes = subject.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes.size() == 4);
    REQUIRE(shapes[0].points.size() == 3);
    CHECK(shapes[0].points[1].x == Catch::Approx(0.67f));
    CHECK(shapes[0].points[1].y == Catch::Approx(0.19f));
}

TEST_CASE("Unversioned host states retain legacy missing-section migration",
          "[state][host][legacy][compatibility][section-integrity]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor incoming;
    seedIncomingState(incoming);
    auto legacyXml = parseHostState(serialiseHostState(incoming));
    legacyXml->removeAttribute("stateFormatVersion");
    legacyXml->removeAttribute("savedParameterCount");

    for (const auto* sectionName : {
             "otherState", "LFO_STATE", "MODULATION_STATE", "AB_STATE" })
        if (auto* section = legacyXml->getChildByName(sectionName))
            legacyXml->removeChildElement(section, true);

    FireAudioProcessor subject;
    seedBaselineState(subject);
    loadHostState(subject, *legacyXml);

    CHECK(getPlainParameter(subject, driveParameterID())
          == Catch::Approx(73.0f));
    CHECK(subject.getLfoManager().getModulationRoutingsCopy().isEmpty());
    const auto shapes = subject.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes.size() == 4);
    for (const auto& shape : shapes)
    {
        REQUIRE(shape.points.size() == 2);
        REQUIRE(shape.curvatures.size() == 1);
        CHECK(shape.points[0] == (juce::Point<float> { 0.0f, 0.0f }));
        CHECK(shape.points[1] == (juce::Point<float> { 1.0f, 0.0f }));
    }
}

TEST_CASE("Concurrent editor resizes serialize coherent size pairs",
          "[state][host][editor-size][concurrency]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    constexpr FireAudioProcessor::SavedEditorSize compact { 1200, 600 };
    constexpr FireAudioProcessor::SavedEditorSize expanded { 1800, 900 };

    processor.setSavedEditorSize(compact.width, compact.height);
    processor.setSavedWidth(1300);
    CHECK(processor.getSavedEditorSize().width == 1300);
    CHECK(processor.getSavedEditorSize().height == compact.height);
    processor.setSavedHeight(650);
    CHECK(processor.getSavedEditorSize().width == 1300);
    CHECK(processor.getSavedEditorSize().height == 650);
    processor.setSavedEditorSize(compact.width, compact.height);

    std::atomic<bool> startWriter { false };
    std::atomic<bool> stopWriter { false };
    std::thread writer([&]
    {
        while (! startWriter.load(std::memory_order_acquire))
            std::this_thread::yield();

        while (! stopWriter.load(std::memory_order_acquire))
        {
            processor.setSavedEditorSize(compact.width, compact.height);
            processor.setSavedEditorSize(expanded.width, expanded.height);
        }
    });

    std::vector<FireAudioProcessor::SavedEditorSize> savedSizes;
    savedSizes.reserve(256);
    {
        const juce::ScopeGuard stopAndJoinWriter { [&]
        {
            stopWriter.store(true, std::memory_order_release);
            writer.join();
        } };

        startWriter.store(true, std::memory_order_release);
        for (int iteration = 0; iteration < 256; ++iteration)
        {
            const auto state = serialiseHostState(processor);
            const auto xml = parseHostState(state);
            const auto* otherState = xml->getChildByName("otherState");
            REQUIRE(otherState != nullptr);
            savedSizes.push_back({
                otherState->getIntAttribute("editorWidth"),
                otherState->getIntAttribute("editorHeight")
            });
        }
    }

    REQUIRE(savedSizes.size() == 256);
    for (const auto& savedSize : savedSizes)
    {
        CAPTURE(savedSize.width, savedSize.height);
        const bool isCompact = savedSize.width == compact.width
                               && savedSize.height == compact.height;
        const bool isExpanded = savedSize.width == expanded.width
                                && savedSize.height == expanded.height;
        CHECK((isCompact || isExpanded));
    }
}

TEST_CASE("Legacy mismatched editor dimensions restore to a valid aspect ratio",
          "[state][host][editor-size][legacy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    auto legacyState = parseHostState(serialiseHostState(source));
    legacyState->removeAttribute("stateFormatVersion");
    legacyState->removeAttribute("savedParameterCount");

    auto* otherState = legacyState->getChildByName("otherState");
    REQUIRE(otherState != nullptr);
    otherState->setAttribute("editorWidth", 1800);
    otherState->setAttribute("editorHeight", 600);

    FireAudioProcessor restored;
    loadHostState(restored, *legacyState);

    const auto restoredSize = restored.getSavedEditorSize();
    CHECK(restoredSize.width == 1200);
    CHECK(restoredSize.height == 600);
    CHECK(restoredSize.width == restoredSize.height * 2);

    const auto roundTrippedState = parseHostState(serialiseHostState(restored));
    const auto* roundTrippedOtherState =
        roundTrippedState->getChildByName("otherState");
    REQUIRE(roundTrippedOtherState != nullptr);
    CHECK(roundTrippedOtherState->getIntAttribute("editorWidth") == 1200);
    CHECK(roundTrippedOtherState->getIntAttribute("editorHeight") == 600);
}
