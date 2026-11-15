#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <chrono>
#include <iostream>

namespace
{
void setDynamicTest(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
int dynamicSetup(FireAudioProcessor& p, int scope, bool native, bool enabled, bool hq = false, int channels = 2)
{
    p.hasUpdateCheckBeenPerformed = true;
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0, layout.inputBuses[0]);
    if (layout.inputBuses.size() > 1) layout.inputBuses.set(1, juce::AudioChannelSet::disabled());
    REQUIRE(p.setBusesLayout(layout));
    REQUIRE(p.pasteChainSnapshot(0, {}).wasOk()); REQUIRE(p.pasteChainSnapshot(1, {}).wasOk());
    setDynamicTest(p, NUM_BANDS_ID, 1); setDynamicTest(p, HQ_ID, hq ? 1 : 0);
    int slot = -1;
    if (native) { REQUIRE(p.restoreLegacyModule(0, 0)); setDynamicTest(p, FILTER_BYPASS_ID, 1); }
    else { slot = p.addInsertEffect(scope, fire::effects::Type::eq); REQUIRE(slot >= 0); }
    const auto eqID = [&](int node, fire::eq::Field field)
    { return native ? fire::eq::parameterID(node, field) : fire::core_modules::eqParameterID(scope, slot, node, field); };
    for (int node = 0; node < fire::eq::maxNodes; ++node)
        setDynamicTest(p, eqID(node, fire::eq::Field::present), node == 1 ? 1 : 0);
    setDynamicTest(p, eqID(1, fire::eq::Field::frequency), 1000);
    setDynamicTest(p, eqID(1, fire::eq::Field::gain), 0);
    setDynamicTest(p, eqID(1, fire::eq::Field::q), 1);
    setDynamicTest(p, eqID(1, fire::eq::Field::type), static_cast<float>(fire::eq::Type::bell));
    using namespace fire::dynamic_eq;
    setDynamicTest(p, parameterID(scope, slot, 1, Field::enabled), enabled ? 1 : 0);
    setDynamicTest(p, parameterID(scope, slot, 1, threshold), -30);
    setDynamicTest(p, parameterID(scope, slot, 1, ratio), 10);
    setDynamicTest(p, parameterID(scope, slot, 1, range), -12);
    setDynamicTest(p, parameterID(scope, slot, 1, attack), 1);
    p.setRateAndBufferSizeDetails(48000, 128); p.prepareToPlay(48000, 128);
    return slot;
}
void tone(juce::AudioBuffer<float>& buffer, int offset, float amplitude = .4f)
{
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            buffer.setSample(c, s, amplitude * std::sin(float((offset+s)*2.0*juce::MathConstants<double>::pi*1000/48000)) * (c == 0 ? 1.f : -1.f));
}
juce::AudioBuffer<float> render(FireAudioProcessor& p, int size, int total = 16384, int channels = 2, float amplitude = .4f)
{
    juce::AudioBuffer<float> output(channels, total); juce::MidiBuffer midi;
    for (int offset = 0; offset < total; offset += size)
    {
        const int count = std::min(size, total-offset);
        juce::AudioBuffer<float> block(channels, count); tone(block, offset, amplitude);
        p.processBlock(block, midi);
        REQUIRE(std::isfinite(block.getMagnitude(0, count)));
        for (int c = 0; c < channels; ++c) output.copyFrom(c, offset, block, c, 0, count);
    }
    return output;
}
}

TEST_CASE("Dynamic EQ reaches the default Master Bell and insert EQ with linked antiphase input", "[dynamic-eq-processor][dynamic-eq][dsp]")
{
    for (int location : {-1,0,1}) for (bool hq : {false,true})
    {
        CAPTURE(location,hq);
        FireAudioProcessor plain, dynamic;
        dynamicSetup(plain, std::max(0,location), location<0, false, hq);
        const auto slot = dynamicSetup(dynamic, std::max(0,location), location<0, true, hq);
        const auto a=render(plain,128), b=render(dynamic,128);
        const auto rms=a.getRMSLevel(0,12000,4000);
        REQUIRE(rms>.05f); CHECK(b.getRMSLevel(0,12000,4000)<rms*.35f);
        const auto meter=dynamic.getDynamicEqVisualState(1,std::max(0,location),slot);
        CHECK(meter.active); CHECK(meter.gainChangeDb < -11.5f); CHECK(meter.renderSequence>0);
        // A polarity inversion cannot disappear from a linked detector.
        CHECK(b.getRMSLevel(1,12000,4000)==Catch::Approx(b.getRMSLevel(0,12000,4000)).margin(.00001));
    }
}

TEST_CASE("Dynamic EQ rendering is invariant to host blocks including oversized chunks", "[dynamic-eq-processor][dynamic-eq][block-size]")
{
    for (bool native : {true,false})
    {
        FireAudioProcessor reference; dynamicSetup(reference,0,native,true);
        const auto expected=render(reference,128,32768);
        for (int size : {31,17003})
        {
            FireAudioProcessor p; dynamicSetup(p,0,native,true);
            const auto actual=render(p,size,32768); float error=0;
            for (int c=0;c<2;++c)for(int i=0;i<actual.getNumSamples();++i)
                error=std::max(error,std::abs(actual.getSample(c,i)-expected.getSample(c,i)));
            CAPTURE(native,size,error); CHECK(error<.00002f);
        }
    }
}

TEST_CASE("Master dynamic detector precedes other EQ nodes and quiet input stays static", "[dynamic-eq-processor][dynamic-eq][detector]")
{
    FireAudioProcessor reference, cut;
    dynamicSetup(reference,0,true,true); dynamicSetup(cut,0,true,true);
    setDynamicTest(cut,fire::eq::parameterID(0,fire::eq::Field::present),1);
    setDynamicTest(cut,LOWCUT_FREQ_ID,5000);
    render(reference,128); render(cut,128);
    CHECK(cut.getDynamicEqVisualState(1).gainChangeDb==Catch::Approx(reference.getDynamicEqVisualState(1).gainChangeDb).margin(.0001));
    FireAudioProcessor plain, quiet; dynamicSetup(plain,0,true,false,false,1); dynamicSetup(quiet,0,true,true,false,1);
    const auto a=render(plain,128,16384,1,.0001f),b=render(quiet,128,16384,1,.0001f);
    CHECK(a.getRMSLevel(0,12000,4000)==Catch::Approx(b.getRMSLevel(0,12000,4000)).margin(.0000001));
    CHECK(quiet.getDynamicEqVisualState(1).gainChangeDb==0);
}

TEST_CASE("Dynamic EQ coexists with Macro Gain modulation and clamps its displayed change", "[dynamic-eq-processor][dynamic-eq][modulation]")
{
    FireAudioProcessor p; dynamicSetup(p,0,true,true);
    setDynamicTest(p,fire::dynamic_eq::parameterID(0,-1,1,fire::dynamic_eq::range),6);
    setDynamicTest(p,PEAK_GAIN_ID,24);
    render(p,128);
    auto meter=p.getDynamicEqVisualState(1); CHECK(meter.active);
    CHECK(meter.effectiveGainDb==Catch::Approx(24).margin(.001)); CHECK(meter.gainChangeDb==Catch::Approx(0).margin(.001));
    setDynamicTest(p,PEAK_GAIN_ID,0);
    REQUIRE(p.assignLfoToTarget(fire::mod_sources::firstMacro,PEAK_GAIN_ID)==LfoManager::AssignmentResult::changed);
    p.setModulationDepth(PEAK_GAIN_ID,.125f); setDynamicTest(p,"macro1",1);
    render(p,128); meter=p.getDynamicEqVisualState(1);
    CHECK(meter.effectiveGainDb>6.5f); CHECK(meter.gainChangeDb>5.9f);
    const auto id=fire::dynamic_eq::parameterID(0,-1,1,fire::dynamic_eq::threshold);
    CHECK(p.assignLfoToTarget(0,id)!=LfoManager::AssignmentResult::changed);
}

TEST_CASE("Dynamic EQ disabled controls have no effect and removal clears configuration", "[dynamic-eq-processor][dynamic-eq][lifecycle]")
{
    FireAudioProcessor a,b; dynamicSetup(a,0,true,false); dynamicSetup(b,0,true,false);
    setDynamicTest(b,fire::dynamic_eq::parameterID(0,-1,1,fire::dynamic_eq::range),24);
    setDynamicTest(b,fire::dynamic_eq::parameterID(0,-1,1,fire::dynamic_eq::threshold),-60);
    const auto x=render(a,128), y=render(b,128);
    for(int c=0;c<2;++c)for(int i=0;i<x.getNumSamples();++i) CHECK(x.getSample(c,i)==y.getSample(c,i));
    setDynamicTest(b,fire::dynamic_eq::parameterID(0,-1,1,fire::dynamic_eq::enabled),1);
    REQUIRE(b.removeEqNode(1)); CHECK_FALSE(b.getDynamicEqVisualState(1).active);
    const int node=b.addEqNode(1000,0); REQUIRE(node>=0);
    CHECK(b.treeState.getRawParameterValue(fire::dynamic_eq::parameterID(0,-1,node,fire::dynamic_eq::enabled))->load()==0);
}

TEST_CASE("Reusing an inserted EQ point between callbacks cannot inherit its dynamic envelope", "[dynamic-eq-processor][dynamic-eq][lifecycle]")
{
    FireAudioProcessor p;
    const int slot=dynamicSetup(p,0,false,true);
    // Make point 1 the first free slot after removal, without an intermediate
    // audio callback exposing the absent state to the DSP.
    setDynamicTest(p,fire::core_modules::eqParameterID(0,slot,0,fire::eq::Field::present),1);
    setDynamicTest(p,fire::core_modules::eqParameterID(0,slot,0,fire::eq::Field::bypassed),1);
    render(p,128); REQUIRE(p.getDynamicEqVisualState(1,0,slot).gainChangeDb < -11);
    REQUIRE(p.removeEqNode(1,0,slot)); REQUIRE(p.addEqNode(1000,0,fire::eq::Type::bell,0,slot)==1);
    render(p,1,1);
    const auto meter=p.getDynamicEqVisualState(1,0,slot);
    CHECK_FALSE(meter.active); CHECK(meter.gainChangeDb==0);
}

TEST_CASE("Dynamic EQ in Mid Side scopes preserves the unselected component in HQ", "[dynamic-eq-processor][dynamic-eq][mid-side][hq]")
{
    for(bool native:{true,false}) for(int mode:{1,2})
    {
        const int scope=native?0:1;
        FireAudioProcessor reference,dynamic;
        dynamicSetup(reference,scope,native,false,true);dynamicSetup(dynamic,scope,native,true,true);
        for(auto* processor:{&reference,&dynamic})setDynamicTest(*processor,fire::channel_mode::parameterID(scope),float(mode));
        juce::AudioBuffer<float> a(2,128),b(2,128);juce::MidiBuffer midi;
        float untouchedError=0;double referenceEnergy=0,dynamicEnergy=0;
        for(int block=0;block<160;++block)
        {
            for(int i=0;i<128;++i)
            {
                const double phase=(block*128+i)*2.0*juce::MathConstants<double>::pi*1000/48000;
                const float mid=.15f*float(std::sin(phase)),side=.1f*float(std::cos(phase+.3));
                a.setSample(0,i,mid+side);a.setSample(1,i,mid-side);
            }
            b.makeCopyOf(a);reference.processBlock(a,midi);dynamic.processBlock(b,midi);
            if(block<100)continue;
            for(int i=0;i<128;++i)
            {
                const float am=.5f*(a.getSample(0,i)+a.getSample(1,i)),as=.5f*(a.getSample(0,i)-a.getSample(1,i));
                const float bm=.5f*(b.getSample(0,i)+b.getSample(1,i)),bs=.5f*(b.getSample(0,i)-b.getSample(1,i));
                untouchedError=std::max(untouchedError,std::abs(mode==1?as-bs:am-bm));
                const auto original=mode==1?am:as,changed=mode==1?bm:bs;
                referenceEnergy+=original*original;dynamicEnergy+=changed*changed;
            }
        }
        CAPTURE(native,mode,untouchedError,referenceEnergy,dynamicEnergy);
        CHECK(untouchedError<.00002f);REQUIRE(referenceEnergy>1);CHECK(dynamicEnergy<referenceEnergy*.4);
        CHECK(reference.getLatencySamples()==dynamic.getLatencySamples());
    }
}

TEST_CASE("Dynamic EQ CPU sample reports the full processor audio cost", "[.][dynamic-eq-benchmark]")
{
    for (bool enabled : {false,true})
    {
        FireAudioProcessor p; dynamicSetup(p,0,true,enabled); render(p,128);
        juce::AudioBuffer<float> block(2,128); juce::MidiBuffer midi;
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<4000;++i)
        {
            const float amplitude=.015f+.065f*(.5f+.5f*std::sin(i*128.f/48000.f*juce::MathConstants<float>::twoPi*3.f));
            tone(block,i*128,amplitude);p.processBlock(block,midi);
        }
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"dynamic EQ "<<(enabled?"on":"off")<<": "<<seconds<<" seconds / 10.667 seconds audio, "<<seconds/(512000.0/48000)*100<<"% of one core\n";
        CHECK(std::isfinite(seconds));
    }
}
