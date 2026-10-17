#include <PluginProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace
{
void envSet(FireAudioProcessor& p,const juce::String& id,float value)
{
    auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void enableSidechain(FireAudioProcessor& p,int channels=2)
{
    auto layout=p.getBusesLayout();REQUIRE(layout.inputBuses.size()==2);
    layout.inputBuses.getReference(1)=channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo();
    REQUIRE(p.setBusesLayout(layout));
}
void prepareEnvelope(FireAudioProcessor& p)
{
    p.setRateAndBufferSizeDetails(48000,64);p.prepareToPlay(48000,64);
}
}

TEST_CASE("Sidechain is isolated from main output and bypass for oversized callbacks",
          "[envelope-bank][sidechain][processor][host-bypass][oversized]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for(bool bypass:{false,true}) for(int sideChannels:{1,2})
    {
        CAPTURE(bypass,sideChannels);
        auto p=std::make_unique<FireAudioProcessor>();enableSidechain(*p,sideChannels);
        envSet(*p,fire::envelope_params::parameterID(1,fire::envelope_params::source),1);
        envSet(*p,fire::envelope_params::parameterID(1,fire::envelope_params::attack),.1f);
        prepareEnvelope(*p);
        juce::AudioBuffer<float> audio(2+sideChannels,17003);audio.clear();
        for(int ch=2;ch<audio.getNumChannels();++ch)
            juce::FloatVectorOperations::fill(audio.getWritePointer(ch),ch==2?.6f:-.6f,audio.getNumSamples());
        audio.setSample(2,41,std::numeric_limits<float>::quiet_NaN());
        juce::MidiBuffer midi;
        if(bypass)p->processBlockBypassed(audio,midi);else p->processBlock(audio,midi);
        CHECK(audio.getMagnitude(0,0,audio.getNumSamples())<1e-7f);
        CHECK(audio.getMagnitude(1,0,audio.getNumSamples())<1e-7f);
        CHECK(std::isnan(audio.getSample(2,41)));
        CHECK(audio.getSample(2,17002)==.6f);
        if(sideChannels==2) CHECK(audio.getSample(3,17002)==-.6f);
        CHECK(p->getLfoManager().getLfoOutput(fire::mod_sources::envelopeSource(1))==Catch::Approx(.6f).margin(.001));
        CHECK(p->getLfoManager().getLfoOutput(fire::mod_sources::envelope)==0);
    }
}

TEST_CASE("Disconnected external detectors stay silent and default envelope keeps main energy",
          "[envelope-bank][sidechain][processor][legacy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor p;
    envSet(p,fire::envelope_params::parameterID(2,fire::envelope_params::source),1);
    envSet(p,"envAttack",.1f);prepareEnvelope(p);
    juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,128);
    for(int block=0;block<16;++block)
    {
        juce::FloatVectorOperations::fill(audio.getWritePointer(0),.25f,128);
        juce::FloatVectorOperations::fill(audio.getWritePointer(1),-.25f,128);
        p.processBlock(audio,midi);
    }
    CHECK(p.getLfoManager().getLfoOutput(fire::mod_sources::envelope)==Catch::Approx(.25f).margin(.0001));
    CHECK(p.getLfoManager().getLfoOutput(fire::mod_sources::envelopeSource(2))==0);
}

TEST_CASE("External envelope reaches actual distortion with HQ enabled",
          "[envelope-bank][sidechain][processor][hq]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for(bool hq:{false,true})
    {
        auto actual=std::make_unique<FireAudioProcessor>(),reference=std::make_unique<FireAudioProcessor>();
        enableSidechain(*actual);
        for(auto* p:{actual.get(),reference.get()})
        {
            envSet(*p,HQ_ID,hq?1:0);envSet(*p,"mode1",2);envSet(*p,"safe1",0);
            envSet(*p,"linked1",0);envSet(*p,"driveCompModern1",0);envSet(*p,"drive1",5);
            envSet(*p,"driveBypass1",1);envSet(*p,"shapeBypass1",1);envSet(*p,"mix1",1);
        }
        envSet(*actual,fire::envelope_params::parameterID(1,fire::envelope_params::source),1);
        envSet(*actual,fire::envelope_params::parameterID(1,fire::envelope_params::attack),.1f);
        REQUIRE(actual->assignLfoToTarget(fire::mod_sources::envelopeSource(1),"drive1")==LfoManager::AssignmentResult::changed);
        actual->setModulationDepth("drive1",.2f);envSet(*reference,"drive1",17);
        prepareEnvelope(*actual);prepareEnvelope(*reference);
        juce::MidiBuffer midi;float error=0;
        for(int block=0;block<100;++block)
        {
            juce::AudioBuffer<float> a(4,128),b(2,128);
            for(int sample=0;sample<128;++sample) for(int ch=0;ch<2;++ch)
            {
                const auto input=.08f*std::sin(float(block*128+sample)*.065f+ch*.13f);
                a.setSample(ch,sample,input);b.setSample(ch,sample,input);
                a.setSample(ch+2,sample,.6f);
            }
            actual->processBlock(a,midi);reference->processBlock(b,midi);
            if(block>80) for(int ch=0;ch<2;++ch) for(int sample=0;sample<128;++sample)
                error=juce::jmax(error,std::abs(a.getSample(ch,sample)-b.getSample(ch,sample)));
        }
        CHECK(error<3e-4f);
    }
}

TEST_CASE("Audition ownership is transient and listens through latency without altering sound state",
          "[envelope-bank][audition][processor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor p;enableSidechain(p);
    envSet(p,fire::envelope_params::parameterID(1,fire::envelope_params::source),1);prepareEnvelope(p);
    const auto before=p.treeState.copyState().toXmlString();
    const auto old=p.beginEnvelopeAudition(0),current=p.beginEnvelopeAudition(1);
    p.endEnvelopeAudition(old);CHECK(p.getEnvelopeAuditionIndex()==1);
    CHECK(p.treeState.copyState().toXmlString()==before);
    juce::MidiBuffer midi;juce::AudioBuffer<float> audio(4,128);
    for(int block=0;block<100;++block)
    {
        audio.clear();for(int ch=2;ch<4;++ch) juce::FloatVectorOperations::fill(audio.getWritePointer(ch),.2f,128);
        p.processBlock(audio,midi);
    }
    CHECK(audio.getSample(0,127)==Catch::Approx(.2f).margin(.0001));
    p.endEnvelopeAudition(current);CHECK(p.getEnvelopeAuditionIndex()==-1);
    for(int block=0;block<20;++block)
    {
        audio.clear();for(int ch=2;ch<4;++ch) juce::FloatVectorOperations::fill(audio.getWritePointer(ch),.2f,128);
        p.processBlock(audio,midi);
    }
    CHECK(audio.getMagnitude(0,0,128)<1e-6f);
    CHECK(p.treeState.copyState().toXmlString()==before);
}

TEST_CASE("Mono audition remains finite with maximum finite stereo sidechain input",
          "[envelope-bank][audition][non-finite][mono]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor p;
    auto layout=p.getBusesLayout();
    layout.inputBuses.getReference(0)=layout.outputBuses.getReference(0)=juce::AudioChannelSet::mono();
    layout.inputBuses.getReference(1)=juce::AudioChannelSet::stereo();REQUIRE(p.setBusesLayout(layout));
    envSet(p,fire::envelope_params::parameterID(1,fire::envelope_params::source),1);prepareEnvelope(p);
    const auto token=p.beginEnvelopeAudition(1);
    juce::MidiBuffer midi;juce::AudioBuffer<float> audio(3,128);
    for(int block=0;block<32;++block)
    {
        audio.clear();
        for(int ch=1;ch<3;++ch) juce::FloatVectorOperations::fill(audio.getWritePointer(ch),std::numeric_limits<float>::max(),128);
        p.processBlock(audio,midi);
        for(int sample=0;sample<128;++sample)
        {
            REQUIRE(std::isfinite(audio.getSample(0,sample)));
            CHECK(std::abs(audio.getSample(0,sample))<=32.001f);
        }
    }
    p.endEnvelopeAudition(token);
}

TEST_CASE("Audition aligns with the latency matched raw bypass in Base and HQ",
          "[envelope-bank][audition][latency][hq]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for(bool hq:{false,true})
    {
        CAPTURE(hq);
        auto actual=std::make_unique<FireAudioProcessor>(),reference=std::make_unique<FireAudioProcessor>();
        enableSidechain(*actual);
        envSet(*actual,fire::envelope_params::parameterID(1,fire::envelope_params::source),1);
        for(auto* p:{actual.get(),reference.get()}){envSet(*p,HQ_ID,hq?1:0);prepareEnvelope(*p);}
        juce::MidiBuffer midi;juce::AudioBuffer<float> a(4,128),b(2,128);
        for(int block=0;block<64;++block)
        {a.clear();b.clear();actual->processBlock(a,midi);reference->processBlock(b,midi);}
        actual->beginEnvelopeAudition(1);float error=0;
        for(int block=0;block<64;++block)
        {
            a.clear();
            for(int ch=0;ch<2;++ch)for(int sample=0;sample<128;++sample)
            {
                const auto input=.15f*std::sin((block*128+sample)*.09f+ch*.21f);
                a.setSample(ch+2,sample,input);b.setSample(ch,sample,input);
            }
            actual->processBlock(a,midi);reference->processBlockBypassed(b,midi);
            if(block>48)for(int ch=0;ch<2;++ch)for(int sample=0;sample<128;++sample)
                error=juce::jmax(error,std::abs(a.getSample(ch,sample)-b.getSample(ch,sample)));
        }
        CHECK(error<2e-5f);
    }
}
