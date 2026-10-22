#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
void msSet(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void msSetup(FireAudioProcessor& p, int scope, int mode, bool hq, bool reserve, int channels = 2)
{
    p.hasUpdateCheckBeenPerformed = true;
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0, layout.inputBuses[0]);
    if (layout.inputBuses.size() > 1) layout.inputBuses.set(1, juce::AudioChannelSet::disabled());
    REQUIRE(p.setBusesLayout(layout));
    REQUIRE(p.pasteChainSnapshot(0, {}).wasOk());
    REQUIRE(p.pasteChainSnapshot(1, {}).wasOk());
    msSet(p, NUM_BANDS_ID, 1); msSet(p, HQ_ID, hq ? 1 : 0);
    if (reserve)
    {
        REQUIRE(p.addInsertEffect(0, fire::effects::Type::shape) == 0);
        msSet(p, fire::effects::parameterID(0, 0, fire::effects::enabledField), 0);
    }
    msSet(p, fire::channel_mode::parameterID(scope), static_cast<float>(mode));
    p.setRateAndBufferSizeDetails(48000, 128); p.prepareToPlay(48000, 128);
}
void msInput(juce::AudioBuffer<float>& b, int offset)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float mid = .12f * std::sin((offset + i) * .031f);
        const float side = .07f * std::cos((offset + i) * .071f);
        b.setSample(0, i, mid + side);
        if (b.getNumChannels() > 1) b.setSample(1, i, mid - side);
    }
}
}

TEST_CASE("Band and Master Mid Side processing changes only the selected component in base and HQ",
          "[mid-side][dsp][hq][latency]")
{
    for (int scope : {0, 1}) for (int mode : {1, 2}) for (bool hq : {false, true}) for (bool reserve : {false, true})
    {
        CAPTURE(scope, mode, hq, reserve);
        FireAudioProcessor reference, edited;
        msSetup(reference, scope, mode, hq, reserve); msSetup(edited, scope, mode, hq, reserve);
        const auto output = scope == 0 ? juce::String(OUTPUT_ID) : juce::String("output1");
        msSet(edited, output, -6.0f);
        const float gain = juce::Decibels::decibelsToGain(edited.treeState.getRawParameterValue(output)->load());
        CHECK(reference.getLatencySamples() == edited.getLatencySamples());
        juce::AudioBuffer<float> a(2, 128), b(2, 128); juce::MidiBuffer midi;
        float untouchedError = 0, selectedError = 0; double energy = 0;
        for (int block = 0; block < 100; ++block)
        {
            msInput(a, block * 128); b.makeCopyOf(a);
            reference.processBlock(a, midi); edited.processBlock(b, midi);
            if (block < 64) continue;
            for (int i = 0; i < 128; ++i)
            {
                const float am = .5f*a.getSample(0,i)+.5f*a.getSample(1,i), as = .5f*a.getSample(0,i)-.5f*a.getSample(1,i);
                const float bm = .5f*b.getSample(0,i)+.5f*b.getSample(1,i), bs = .5f*b.getSample(0,i)-.5f*b.getSample(1,i);
                untouchedError = std::max(untouchedError, std::abs(mode == 1 ? bs-as : bm-am));
                selectedError = std::max(selectedError, std::abs(mode == 1 ? bm-gain*am : bs-gain*as));
                energy += am*am+as*as;
            }
        }
        CHECK(energy > 1); CHECK(untouchedError < 0.00002f); CHECK(selectedError < 0.00002f);
    }
}

TEST_CASE("Side mode leaves mono input unchanged by selected chain gain",
          "[mid-side][dsp][mono]")
{
    for (int scope : {0,1}) for (bool hq : {false,true})
    {
        FireAudioProcessor reference, edited;
        msSetup(reference, scope, 2, hq, true, 1); msSetup(edited, scope, 2, hq, true, 1);
        msSet(edited, scope == 0 ? juce::String(OUTPUT_ID) : juce::String("output1"), -18);
        juce::AudioBuffer<float> a(1,128), b(1,128); juce::MidiBuffer midi;
        for (int block = 0; block < 80; ++block)
        {
            msInput(a, block*128); b.makeCopyOf(a); reference.processBlock(a,midi); edited.processBlock(b,midi);
            for (int i=0;i<128;++i) CHECK(a.getSample(0,i)==b.getSample(0,i));
        }
    }
}

TEST_CASE("Channel mode automation settles through topology transitions and survives host bypass",
          "[mid-side][dsp][transition][host-bypass]")
{
    FireAudioProcessor processor;
    msSetup(processor, 0, 0, true, true);
    const auto latency = processor.getLatencySamples();
    juce::AudioBuffer<float> buffer(2,128); juce::MidiBuffer midi;
    for (int block=0;block<240;++block)
    {
        if (block < 120 && block % 7 == 0)
            for (int scope : {0,1}) msSet(processor,fire::channel_mode::parameterID(scope), (block/7)%3);
        if (block == 120)
            for (int scope : {0,1}) msSet(processor,fire::channel_mode::parameterID(scope),2);
        msInput(buffer, block*128);
        if (block>=60 && block<100) processor.processBlockBypassed(buffer,midi); else processor.processBlock(buffer,midi);
        CHECK(processor.getLatencySamples()==latency);
        CHECK(std::isfinite(buffer.getMagnitude(0,128))); CHECK(buffer.getMagnitude(0,128)<1);
        if (block>200) CHECK(buffer.getMagnitude(0,128)>.01f);
    }
    // An oversized host block must use the prepared chunks without reallocating the wrapper.
    buffer.setSize(2,17003); msInput(buffer,240*128); processor.processBlock(buffer,midi);
    CHECK(std::isfinite(buffer.getMagnitude(0,17003))); CHECK(buffer.getMagnitude(0,17003)<1);
}

TEST_CASE("Band bypass and zero Mix preserve both components in Mid Side modes",
          "[mid-side][dsp][bypass][mix]")
{
    for (int mode : {1,2}) for (bool hq : {false,true}) for (bool bypass : {false,true})
    {
        CAPTURE(mode,hq,bypass);
        FireAudioProcessor reference, edited;
        msSetup(reference,1,mode,hq,true); msSetup(edited,1,mode,hq,true);
        for (auto* p : {&reference,&edited}) msSet(*p,bypass ? "multibandEnable1" : "mix1",0);
        msSet(edited,"output1",-18);
        juce::AudioBuffer<float> a(2,128), b(2,128); juce::MidiBuffer midi;
        float error=0;
        for(int block=0;block<80;++block)
        {
            msInput(a,block*128); b.makeCopyOf(a); reference.processBlock(a,midi); edited.processBlock(b,midi);
            if(block>64)for(int c=0;c<2;++c)for(int i=0;i<128;++i)
                error=std::max(error,std::abs(a.getSample(c,i)-b.getSample(c,i)));
        }
        CHECK(error<0.000001f);
    }
}

TEST_CASE("Global zero Mix bypasses Mid Side processing with the established host latency",
          "[mid-side][dsp][mix][latency]")
{
    for(bool hq : {false,true})
    {
        FireAudioProcessor reference, edited;
        msSetup(reference,0,0,hq,true); msSetup(edited,0,2,hq,true);
        for(auto* p:{&reference,&edited}) msSet(*p,MIX_ID,0);
        msSet(edited,OUTPUT_ID,-18);
        juce::AudioBuffer<float> a(2,128), b(2,128); juce::MidiBuffer midi;
        float error=0;
        for(int block=0;block<80;++block)
        {
            msInput(a,block*128); b.makeCopyOf(a); reference.processBlock(a,midi); edited.processBlock(b,midi);
            if(block>64)for(int c=0;c<2;++c)for(int i=0;i<128;++i)
                error=std::max(error,std::abs(a.getSample(c,i)-b.getSample(c,i)));
        }
        CHECK(error<0.000001f); CHECK(reference.getLatencySamples()==edited.getLatencySamples());
    }
}
