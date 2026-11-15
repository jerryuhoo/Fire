#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

namespace
{
void midiSet(FireAudioProcessor& p,const juce::String& id,float value)
{auto* v=p.treeState.getParameter(id);REQUIRE(v);v->setValueNotifyingHost(v->convertTo0to1(value));}
void midiPrepare(FireAudioProcessor& p,bool hq=false)
{
    p.hasUpdateCheckBeenPerformed=true;
    REQUIRE(p.pasteChainSnapshot(0,{}).wasOk()); REQUIRE(p.pasteChainSnapshot(1,{}).wasOk());
    midiSet(p,HQ_ID,hq?1.f:0.f); midiSet(p,OUTPUT_ID,-18);
    midiSet(p,fire::midi_mod::parameterID(0,fire::midi_mod::number),74);
    midiSet(p,fire::midi_mod::parameterID(0,fire::midi_mod::channel),2);
    REQUIRE(p.assignLfoToTarget(fire::mod_sources::firstMidiCC,OUTPUT_ID)==LfoManager::AssignmentResult::changed);
    p.setModulationDepth(OUTPUT_ID,.1f);
    p.setRateAndBufferSizeDetails(48000,128);p.prepareToPlay(48000,128);
}
juce::AudioBuffer<float> midiRender(int size,bool hq)
{
    FireAudioProcessor p;midiPrepare(p,hq);
    constexpr int total=32768;
    juce::AudioBuffer<float> rendered(2,total);
    constexpr int positions[]{17,127,128,16384,17002,17003};
    constexpr int values[]{127,64,0,127,32,100};
    for(int offset=0;offset<total;offset+=size)
    {
        const int count=std::min(size,total-offset);
        juce::AudioBuffer<float> buffer(2,count);juce::MidiBuffer midi;
        for(int c=0;c<2;++c)for(int i=0;i<count;++i)buffer.setSample(c,i,.1f*std::sin((offset+i)*.031f+c*.2f));
        for(size_t i=0;i<std::size(positions);++i)if(positions[i]>=offset && positions[i]<offset+count)
            midi.addEvent(juce::MidiMessage::controllerEvent(2,74,values[i]),positions[i]-offset);
        const int events=midi.getNumEvents();p.processBlock(buffer,midi);CHECK(midi.getNumEvents()==events);
        for(int c=0;c<2;++c)rendered.copyFrom(c,offset,buffer,c,0,count);
    }
    CHECK(p.getMidiCCValue(0)==Catch::Approx(100.f/127).margin(.0001));
    return rendered;
}
}

TEST_CASE("MIDI CC audio modulation is invariant across host blocks oversized chunks and HQ", "[midi-processor][dsp][hq][block-size]")
{
    for(bool hq:{false,true})
    {
        auto reference=midiRender(128,hq);
        for(int size:{31,17003})
        {
            auto test=midiRender(size,hq);float error=0;
            for(int c=0;c<2;++c)for(int i=0;i<test.getNumSamples();++i)
                error=std::max(error,std::abs(test.getSample(c,i)-reference.getSample(c,i)));
            CAPTURE(hq,size,error);CHECK(error<.0001f);
        }
    }
}

TEST_CASE("Host bypass continues receiving CC while wrong controller and channel are ignored", "[midi-processor][host-bypass]")
{
    FireAudioProcessor p;midiPrepare(p,true);
    juce::AudioBuffer<float> block(2,1024);block.clear();juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1,74,127),0);
    midi.addEvent(juce::MidiMessage::controllerEvent(2,75,127),1);
    p.processBlock(block,midi);CHECK(p.getMidiCCValue(0)==0);
    midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(2,74,127),17);
    block.clear();p.processBlockBypassed(block,midi);CHECK(p.getMidiCCValue(0)==Catch::Approx(1));
    midi.clear();block.clear();p.processBlock(block,midi);
    CHECK(p.getMidiCCValue(0)==Catch::Approx(1));CHECK(std::isfinite(block.getMagnitude(0,1024)));
}

TEST_CASE("MIDI Learn applies one undoable mapping and its captured value without stale ownership", "[midi-processor][learn][edit-history]")
{
    FireAudioProcessor p;p.hasUpdateCheckBeenPerformed=true;
    p.setRateAndBufferSizeDetails(48000,128);p.prepareToPlay(48000,128);
    const auto old=p.beginMidiLearn(0),token=p.beginMidiLearn(1);
    REQUIRE(token!=0);p.endMidiLearn(old);CHECK(p.isMidiLearnActive(token));CHECK_FALSE(p.isMidiLearnActive(old));
    juce::AudioBuffer<float> audio(2,512);audio.clear();juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(3,11,100),31);p.processBlock(audio,midi);
    REQUIRE(p.applyMidiLearn(token));CHECK_FALSE(p.isMidiLearnActive(token));CHECK_FALSE(p.applyMidiLearn(token));
    CHECK(p.treeState.getRawParameterValue("midiCC2Number")->load()==11);
    CHECK(p.treeState.getRawParameterValue("midiCC2Channel")->load()==3);
    midi.clear();audio.clear();p.processBlock(audio,midi);
    CHECK(p.getMidiCCValue(1)==Catch::Approx(100.f/127).margin(.0001));
    REQUIRE(p.undoEdit());CHECK(p.treeState.getRawParameterValue("midiCC2Number")->load()==-1);
    CHECK(p.treeState.getRawParameterValue("midiCC2Channel")->load()==0);
    const auto stale=p.beginMidiLearn(0);REQUIRE(p.addLfo()>=0);
    CHECK_FALSE(p.isMidiLearnActive(stale));CHECK_FALSE(p.applyMidiLearn(stale));
}

TEST_CASE("Processor note and manual triggers reach One shot clock at the event offset", "[midi-processor][trigger]")
{
    FireAudioProcessor p;p.hasUpdateCheckBeenPerformed=true;
    midiSet(p,fire::lfo_playback::parameterID(0,fire::lfo_playback::playMode),2);
    midiSet(p,fire::lfo_playback::parameterID(0,fire::lfo_playback::triggerChannel),2);
    midiSet(p,fire::lfo_playback::parameterID(0,fire::lfo_playback::triggerNote),60);
    midiSet(p,fire::lfo_bank::parameterID(0,fire::lfo_bank::Field::syncMode),0);
    midiSet(p,fire::lfo_bank::parameterID(0,fire::lfo_bank::Field::rateHz),20);
    p.setRateAndBufferSizeDetails(48000,128);p.prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);audio.clear();juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);p.processBlock(audio,midi);
    CHECK(p.getLfoPhase(0)==0);
    midi.clear();midi.addEvent(juce::MidiMessage::noteOn(2,60,(juce::uint8)100),64);audio.clear();p.processBlock(audio,midi);
    CHECK(p.getLfoPhase(0)==Catch::Approx(64.f*20/48000).margin(.001));
    midi.clear();audio.setSize(2,4096);audio.clear();p.processBlock(audio,midi);CHECK(p.getLfoPhase(0)==1);
    const auto generation=p.getLfoSlotGeneration(0);REQUIRE(p.triggerLfo(0,generation));
    audio.setSize(2,128);audio.clear();p.processBlock(audio,midi);
    CHECK(p.getLfoPhase(0)==Catch::Approx(128.f*20/48000).margin(.001));
    REQUIRE(p.removeLfo(0));REQUIRE(p.addLfo()==0);CHECK_FALSE(p.triggerLfo(0,generation));
}
