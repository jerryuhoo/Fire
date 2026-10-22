#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/EnvelopeStateValidation.h>
#include <Utility/DynamicEqParameters.h>
#include <catch2/catch_test_macros.hpp>
#include <limits>

TEST_CASE("Host states restore a legitimate minimum Dynamic EQ attack after decimal serialisation",
          "[state-numeric-boundary][state][dynamic-eq-state][host]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source,restored;
    const auto id=fire::dynamic_eq::parameterID(0,-1,0,fire::dynamic_eq::attack);
    auto* attack=source.treeState.getParameter(id);REQUIRE(attack);
    attack->setValueNotifyingHost(0);
    source.treeState.getParameter("drive1")->setValueNotifyingHost(.37f);
    restored.treeState.getParameter("drive1")->setValueNotifyingHost(.81f);
    juce::MemoryBlock bytes;source.getStateInformation(bytes);
    auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize()));REQUIRE(xml);
    const auto* parameter=xml->getChildByName("PARAMETERS")->getChildByAttribute("id",id);REQUIRE(parameter);
    const auto text=parameter->getStringAttribute("value");CAPTURE(text);
    CHECK(static_cast<float>(text.getDoubleValue())==attack->getNormalisableRange().start);
    CHECK(fire::envelope_state::isValidValue(*attack,text,false));
    restored.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));
    juce::XmlElement expected("WINGSFIRE");state::saveStateToXml(source,expected);
    CHECK(restored.isCurrentStateEquivalentToPreset(expected));
    CHECK(restored.treeState.getRawParameterValue(id)->load()==attack->getNormalisableRange().start);
}

TEST_CASE("Continuous host boundary validation accepts exact float endpoints and rejects adjacent out of range floats",
          "[state-numeric-boundary][state][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor processor;
    for(const auto* parameter:processor.getParameters())
        if(const auto* ranged=dynamic_cast<const juce::AudioParameterFloat*>(parameter))
        {
            CAPTURE(ranged->paramID);
            const auto& range=ranged->getNormalisableRange();
            for(float value:{range.start,range.end})
            {
                const auto text=juce::var{static_cast<double>(value)}.toString();
                CHECK(fire::envelope_state::isValidValue(*ranged,text,false));
            }
            const float below=std::nextafter(range.start,-std::numeric_limits<float>::infinity());
            const float above=std::nextafter(range.end,std::numeric_limits<float>::infinity());
            CHECK_FALSE(fire::envelope_state::isValidValue(*ranged,juce::var{static_cast<double>(below)}.toString(),false));
            CHECK_FALSE(fire::envelope_state::isValidValue(*ranged,juce::var{static_cast<double>(above)}.toString(),false));
        }
    auto* boolean=processor.treeState.getParameter(fire::dynamic_eq::parameterID(0,-1,0,fire::dynamic_eq::enabled));REQUIRE(boolean);
    CHECK_FALSE(fire::envelope_state::isValidValue(*boolean,"0.99999999999",false));
    CHECK_FALSE(fire::envelope_state::isValidValue(*boolean,"1.00000000001",false));
    auto* number=processor.treeState.getParameter("midiCC1Number");REQUIRE(number);
    CHECK_FALSE(fire::envelope_state::isValidValue(*number,"126.99999999999",false));
    const auto* continuous=processor.treeState.getParameter(fire::dynamic_eq::parameterID(0,-1,0,fire::dynamic_eq::attack));REQUIRE(continuous);
    CHECK_FALSE(fire::envelope_state::isValidValue(*continuous,"1.00000000001",true));
    CHECK_FALSE(fire::envelope_state::isValidValue(*continuous,"-0.00000000001",true));
    CHECK_FALSE(fire::envelope_state::isValidValue(*continuous,"NaN",false));
    CHECK_FALSE(fire::envelope_state::isValidValue(*continuous,"1e300",false));
}
