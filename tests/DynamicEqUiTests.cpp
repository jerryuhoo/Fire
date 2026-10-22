#include <GUI/DynamicEqControls.h>
#include <GUI/EqControlsPanel.h>
#include <Panels/ControlPanel/GlobalPanel.h>
#include <Panels/SpectrogramPanel/FilterControl.h>
#include <PluginEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>

struct DynamicEqUiTestAccess
{
    static auto& slider(fire::ui::DynamicEqControls& c,int field){return c.sliders[static_cast<size_t>(field-1)];}
    static auto& enabled(fire::ui::DynamicEqControls& c){return c.enabled;}
    static void tick(fire::ui::DynamicEqControls& c){c.timerCallback();}
    static auto& button(EqControlsPanel& c){return c.dynamicButton;}
    static void open(EqControlsPanel& c){c.showDynamics();}
    static auto popover(EqControlsPanel& c){return c.dynamicPopover;}
    static auto* content(EqControlsPanel& c){return c.dynamicSession ? c.dynamicSession->content.get() : nullptr;}
    static auto handle(const FilterControl& c){return c.draggablePeakButton.getBounds();}
    static bool liveCurve(const FilterControl& c){return !c.lfoResponseCurve.isEmpty();}
    static double peakMagnitude(const FilterControl& c){return c.modulatedEqResponse[1].magnitudeAt(1000,48000);}
    static void expire(FilterControl& c){c.lastTelemetryTimeMs=juce::Time::getMillisecondCounterHiRes()-2000;}
    static void previewValue(fire::ui::DynamicEqControls& c)
    {c.presentation.update({-4.2f,1.8f,1,true},6,1.f/30.f);}
};
namespace
{
namespace dyn=fire::dynamic_eq;
using Access=DynamicEqUiTestAccess;
void show(juce::Component& c){c.addToDesktop(juce::ComponentPeer::windowIsTemporary);c.setVisible(true);}
template<class T,class Predicate> T* find(juce::Component& root,Predicate predicate)
{
    if(auto* item=dynamic_cast<T*>(&root);item && predicate(*item))return item;
    for(auto* child:root.getChildren())if(auto* item=find<T>(*child,predicate))return item;
    return nullptr;
}
void set(FireAudioProcessor& p,const juce::String& id,float value)
{auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
float get(FireAudioProcessor& p,const juce::String& id)
{auto* parameter=p.treeState.getRawParameterValue(id);REQUIRE(parameter);return parameter->load();}
struct GestureObserver : juce::AudioProcessorParameter::Listener
{
    std::function<void()> onBegin;int starts=0,ends=0;
    void parameterValueChanged(int,float) override{}
    void parameterGestureChanged(int,bool begins) override
    {if(begins){++starts;if(onBegin)onBegin();}else ++ends;}
};
void preview(juce::Component& c,const juce::String& name)
{
    if(const auto* directory=std::getenv("FIRE_DYNAMIC_EQ_PREVIEW_DIR"))
    {
        auto file=juce::File(directory).getChildFile(name+".png");REQUIRE(file.getParentDirectory().createDirectory().wasOk());
        auto stream=file.createOutputStream();REQUIRE(stream);stream->setPosition(0);stream->truncate();
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*stream));
    }
}
}

TEST_CASE("Dynamic EQ controls bind five continuous settings and one toggle to the selected node", "[dynamic-eq-ui][ui][dynamic-eq]")
{
    FireAudioProcessor processor;
    fire::ui::DynamicEqControls control(processor,1,-1,-1,[]{return true;});show(control);
    Access::enabled(control).triggerClick();
    CHECK(get(processor,dyn::parameterID(0,-1,1,dyn::enabled))==1);
    for(const auto [field,value]:std::array<std::pair<int,float>,5>{{{dyn::threshold,-35},{dyn::ratio,6.5f},{dyn::range,-10.5f},{dyn::attack,5.5f},{dyn::release,350}}})
    {
        Access::slider(control,field).setValue(value,juce::sendNotificationSync);
        CHECK(get(processor,dyn::parameterID(0,-1,1,field))==Catch::Approx(value));
        CHECK(get(processor,dyn::parameterID(0,-1,0,field))==Catch::Approx(dyn::defaults[static_cast<size_t>(field)]));
        CHECK(dynamic_cast<ModulatableSlider*>(&Access::slider(control,field))==nullptr);
    }
    REQUIRE(processor.undoEdit());CHECK(get(processor,dyn::parameterID(0,-1,1,dyn::release))==100);
    set(processor,dyn::parameterID(0,-1,1,dyn::threshold),-12);
    CHECK(Access::slider(control,dyn::threshold).getValue()==Catch::Approx(-12));
}

TEST_CASE("Dynamic EQ writes stop if the host closes or rebinds the panel at begin gesture", "[dynamic-eq-ui][ui][lifecycle][host][self-delete]")
{
    for(bool toggle:{false,true}) for(int boundary:{0,1,2})
    {
        CAPTURE(toggle,boundary);
        FireAudioProcessor processor;bool current=true;
        auto control=std::make_unique<fire::ui::DynamicEqControls>(processor,1,-1,-1,[&]{return current;});show(*control);
        const int field=toggle?dyn::enabled:dyn::threshold;
        const auto id=dyn::parameterID(0,-1,1,field);const auto original=get(processor,id);
        auto* parameter=processor.treeState.getParameter(id);GestureObserver observer;
        observer.onBegin=[&]{if(boundary==0)control->setVisible(false);else if(boundary==1)current=false;else control.reset();};
        parameter->addListener(&observer);
        if(toggle)
        {Access::enabled(*control).setToggleState(true,juce::dontSendNotification);auto callback=Access::enabled(*control).onClick;callback();}
        else Access::slider(*control,field).setValue(-42,juce::sendNotificationSync);
        CHECK(get(processor,id)==original);CHECK(observer.starts==1);CHECK(observer.ends==1);
        parameter->removeListener(&observer);
    }
}

TEST_CASE("Dynamic EQ popovers keep the existing point layout and expire on selection or type changes", "[dynamic-eq-ui][ui][layout][lifecycle]")
{
    FireAudioProcessor processor;EqControlsPanel panel(processor);panel.setSize(600,230);show(panel);
    panel.selectNode(0);CHECK_FALSE(Access::button(panel).isEnabled());
    panel.selectNode(1);CHECK(Access::button(panel).isEnabled());
    const auto buttons=panel.getLocalBounds();CHECK(buttons.contains(Access::button(panel).getBounds()));
    Access::open(panel);auto popover=Access::popover(panel);REQUIRE(popover);
    panel.selectNode(0);CHECK(Access::popover(panel)==nullptr);CHECK(popover==nullptr);
    panel.selectNode(1);Access::open(panel);REQUIRE(Access::popover(panel));
    set(processor,fire::eq::parameterID(1,fire::eq::Field::type),float(fire::eq::Type::notch));panel.refresh();
    CHECK_FALSE(Access::button(panel).isEnabled());CHECK(Access::popover(panel)==nullptr);
}

TEST_CASE("Dynamic EQ gestures safely close the entire editor and its other parameter displays", "[dynamic-eq-ui][ui][lifecycle][host][self-delete]")
{
    for(bool toggle:{false,true})
    {
        CAPTURE(toggle);
        FireAudioProcessor processor;processor.hasUpdateCheckBeenPerformed=true;
        auto editor=std::make_unique<FireAudioProcessorEditor>(processor);editor->stopTimer();show(*editor);
        auto* master=find<juce::Button>(*editor,[](auto& b){return b.getButtonText()=="MASTER LAB";});REQUIRE(master);master->triggerClick();
        auto* global=find<GlobalPanel>(*editor,[](auto&){return true;});REQUIRE(global);global->selectEqNode(1);
        auto* panel=find<EqControlsPanel>(*global,[](auto& p){return p.isShowing();});REQUIRE(panel);panel->selectNode(1);
        Access::open(*panel);auto* control=Access::content(*panel);REQUIRE(control);
        const int field=toggle?dyn::enabled:dyn::threshold;
        const auto id=dyn::parameterID(0,-1,1,field);const auto original=get(processor,id);
        auto* parameter=processor.treeState.getParameter(id);GestureObserver observer;
        observer.onBegin=[&]{editor.reset();};parameter->addListener(&observer);
        if(toggle)
        {Access::enabled(*control).setToggleState(true,juce::dontSendNotification);auto callback=Access::enabled(*control).onClick;callback();}
        else Access::slider(*control,field).setValue(-43,juce::sendNotificationSync);
        CHECK(editor==nullptr);CHECK(get(processor,id)==original);
        CHECK(observer.starts==1);CHECK(observer.ends==1);parameter->removeListener(&observer);
    }
}

TEST_CASE("Dynamic EQ spectrum follows effective gain while its draggable point stays at base gain", "[dynamic-eq-ui][filter-control][ui][telemetry]")
{
    FireAudioProcessor processor;
    set(processor,FILTER_BYPASS_ID,1);set(processor,PEAK_GAIN_ID,6);set(processor,PEAK_FREQ_ID,1000);
    for(const auto [field,value]:std::array<std::pair<int,float>,5>{{{dyn::enabled,1},{dyn::threshold,-50},{dyn::ratio,10},{dyn::range,-12},{dyn::attack,.1f}}})
        set(processor,dyn::parameterID(0,-1,1,field),value);
    processor.setRateAndBufferSizeDetails(48000,64);processor.prepareToPlay(48000,64);
    const auto storedGain=processor.getEqNodeState(1).gainDb;
    GlobalPanel global(processor,{},{},{},{},{});FilterControl curve(processor,global);curve.setSize(900,300);show(curve);curve.animationTick();
    const auto baseHandle=Access::handle(curve);
    juce::AudioBuffer<float> audio(2,64);juce::MidiBuffer midi;
    int position=0;
    for(int block=0;block<160;++block)
    {
        for(int sample=0;sample<64;++sample,++position)
        {const float value=.7f*std::sin(juce::MathConstants<float>::twoPi*1000.f*float(position)/48000.f);audio.setSample(0,sample,value);audio.setSample(1,sample,value);}
        processor.processBlock(audio,midi);curve.animationTick();
    }
    const auto state=processor.getDynamicEqVisualState(1);
    CHECK(state.active);CHECK(state.gainChangeDb < -1);CHECK(state.effectiveGainDb < 5);
    CHECK(Access::liveCurve(curve));CHECK(Access::peakMagnitude(curve)<.9);
    CHECK(Access::handle(curve)==baseHandle);CHECK(processor.getEqNodeState(1).gainDb==storedGain);
    Access::expire(curve);curve.animationTick();CHECK_FALSE(Access::liveCurve(curve));
    processor.releaseResources();
}

TEST_CASE("Dynamic EQ visual motion fades when audio stops and controls fit every skin", "[dynamic-eq-ui][ui][skin][layout]")
{
    fire::ui::DynamicEqPresentation value;const dyn::VisualState signal{-6,2,100,true};
    value.update(signal,8,.03f);CHECK(value.gainChangeDb==-6);CHECK(value.effectiveGainDb==2);
    for(int i=0;i<30;++i)value.update(signal,8,.02f);
    CHECK(value.gainChangeDb==0);CHECK(value.effectiveGainDb==8);CHECK(value.opacity==0);
    FireAudioProcessor processor;fire::ui::DynamicEqControls control(processor,1,-1,-1,[]{return true;});
    set(processor,dyn::parameterID(0,-1,1,dyn::enabled),1);Access::previewValue(control);
    for(auto skin:fire::ui::skins)
    {
        fire::ui::setSkin(control,skin);control.sendLookAndFeelChange();
        for(auto* child:control.getChildren())CHECK(control.getLocalBounds().contains(child->getBounds()));
        preview(control,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-dynamic-eq");
    }
}
