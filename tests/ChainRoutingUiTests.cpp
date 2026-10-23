#include <PluginEditor.h>
#include <GUI/EffectRackNavigation.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>

struct ChainRoutingUiTestAccess
{
    static auto* body(fire::ui::EffectRackNavigation& r){return r.routingSession?r.routingSession->content.get():nullptr;}
    static auto popup(fire::ui::EffectRackNavigation& r){return r.routingPopover;}
    static void close(fire::ui::EffectRackNavigation& r){r.closeRouting();}
    static void mode(fire::ui::ChainRoutingControls& c,int value){c.changeMode(value);}
    static void branch(fire::ui::ChainRoutingControls& c,int node,int value){c.changeBranch(node,value);}
    static void move(fire::ui::ChainRoutingControls& c,int node,int direction){c.moveNode(node,direction);}
    static void tick(fire::ui::ChainRoutingControls& c){c.timerCallback();}
    static int count(fire::ui::ChainRoutingControls& c,int lane){return c.counts[static_cast<size_t>(lane)];}
    static auto& modeMenu(fire::ui::ChainRoutingControls& c){return c.mode;}
    static auto channelResult(fire::ui::ChainRoutingControls& c){return c.channel.createPopupResultHandler();}
    static bool channelEnabled(const fire::ui::ChainRoutingControls& c){return c.channel.isEnabled();}
    static bool key(ContextAwareComboBox& c,int key){return c.keyPressed(juce::KeyPress(key));}
    static auto assignment(ModulatableSlider& s){return s.createLfoAssignmentMenuResultHandler();}
    static auto& badge(fire::ui::EffectRackNavigation& r,int node){return r.branchBadges[static_cast<size_t>(node)];}
    static auto& summary(fire::ui::EffectRackNavigation& r){return r.routingButton;}
    static void hover(fire::ui::EffectRackNavigation& r,int node){r.hoveredNode=node;}
    static auto targets(const ModulationMatrixRow& row){return row.buildTargetChoices();}
};
namespace
{
using Rack=fire::ui::EffectRackNavigation;using Body=fire::ui::ChainRoutingControls;using Access=ChainRoutingUiTestAccess;
namespace route=fire::parallel_chain;
void show(juce::Component& c){c.addToDesktop(juce::ComponentPeer::windowIsTemporary);c.setVisible(true);}
struct Fixture
{
    FireAudioProcessor processor;FireLookAndFeel look;juce::Component host;Rack rack{processor,1};
    Fixture(){host.setLookAndFeel(&look);host.setSize(800,620);host.addAndMakeVisible(rack);rack.setBounds(20,20,210,390);show(host);}
    ~Fixture(){rack.dismiss();host.setLookAndFeel(nullptr);}
    Body& open(){rack.showRouting();auto* body=Access::body(rack);REQUIRE(body);return *body;}
};
template<class T,class Pred>T* find(juce::Component& root,Pred pred)
{if(auto* c=dynamic_cast<T*>(&root);c && pred(*c))return c;for(auto* child:root.getChildren())if(auto* c=find<T>(*child,pred))return c;return nullptr;}
struct Gesture : juce::AudioProcessorParameter::Listener
{int begins=0,ends=0;std::function<void()> onBegin;void parameterValueChanged(int,float)override{}
 void parameterGestureChanged(int,bool beginning)override{if(beginning){++begins;if(onBegin)onBegin();}else ++ends;}};
void preview(juce::Component& c,const juce::String& name)
{if(const auto* root=std::getenv("FIRE_CHAIN_ROUTING_PREVIEW_DIR")){auto file=juce::File(root).getChildFile(name+".png");REQUIRE(file.getParentDirectory().createDirectory().wasOk());auto stream=file.createOutputStream();REQUIRE(stream);stream->setPosition(0);stream->truncate();REQUIRE(juce::PNGImageFormat{}.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*stream));}}
}

TEST_CASE("Routing popover continues after its own topology edits and filters stable module order into A and B", "[chain-routing-ui][ui][parallel-chain]")
{
    Fixture f;const auto pitch=f.rack.getRowPitch();auto& body=f.open();
    CHECK(f.processor.getChainRoutingMode(1)==route::serial);CHECK_FALSE(f.rack.getParallelBlendSlider().isEnabled());
    CHECK(Access::key(Access::modeMenu(body),juce::KeyPress::downKey));
    CHECK(f.processor.getChainRoutingMode(1)==route::parallel);CHECK(body.contextCurrent());
    CHECK(f.rack.getParallelBlendSlider().isEnabled());
    Access::branch(body,1,route::b);CHECK(body.contextCurrent());CHECK(f.processor.getModuleBranch(1,1)==route::b);
    CHECK(Access::count(body,1)==1);CHECK(Access::count(body,0)==4);
    Access::move(body,4,-1);CHECK(body.contextCurrent());
    const auto order=f.processor.getModuleOrder(1);CHECK(std::find(order.begin(),order.end(),4)<std::find(order.begin(),order.end(),3));
    CHECK(f.rack.getRowPitch()==pitch);
    REQUIRE(f.processor.undoEdit());Access::tick(body);CHECK_FALSE(body.contextCurrent());
    Access::branch(body,0,route::b);CHECK(f.processor.getModuleBranch(1,0)==route::a);
}

TEST_CASE("Parallel Blend keeps one registered slider across popup closure and scope rebinding", "[chain-routing-ui][ui][binding][lifecycle]")
{
    Fixture f;auto* slider=&f.rack.getParallelBlendSlider();auto& body=f.open();Access::mode(body,route::parallel);
    slider->setValue(.73,juce::sendNotificationSync);CHECK(f.processor.getParallelBlend(1)==Catch::Approx(.73));
    Access::close(f.rack);CHECK(&f.rack.getParallelBlendSlider()==slider);CHECK(slider->getParentComponent()==&f.rack);CHECK_FALSE(slider->isVisible());
    f.rack.setScope(2);auto& second=f.open();Access::mode(second,route::parallel);
    CHECK(slider->getParamID()==route::blendID(2));CHECK(slider->getValue()==Catch::Approx(.5));
    slider->setValue(.25,juce::sendNotificationSync);CHECK(f.processor.getParallelBlend(2)==Catch::Approx(.25));CHECK(f.processor.getParallelBlend(1)==Catch::Approx(.73));
    f.rack.setScope(1);CHECK(Access::popup(f.rack)==nullptr);CHECK(slider->getValue()==Catch::Approx(.73));
}

TEST_CASE("Moving a routing row down skips removed permanent slots when finding its anchor", "[chain-routing-ui][ui][parallel-chain][reorder]")
{
    Fixture f;
    f.processor.removeModule(1,3); // Stereo's permanent slot remains in the full order.
    REQUIRE_FALSE(f.processor.isModulePresent(1,3));
    auto& body=f.open();Access::mode(body,route::parallel);
    Access::move(body,1,1); // Shape must move after Compressor, skipping deleted Stereo.
    CHECK(body.contextCurrent());
    const auto order=f.processor.getModuleOrder(1);
    const auto shape=std::find(order.begin(),order.end(),1),compressor=std::find(order.begin(),order.end(),2);
    REQUIRE(shape!=order.end());REQUIRE(compressor!=order.end());CHECK(compressor<shape);
    CHECK(shape<std::find(order.begin(),order.end(),4));
    CHECK_FALSE(f.processor.isModulePresent(1,3));
    Access::branch(body,1,route::b);CHECK(body.contextCurrent());CHECK(f.processor.getModuleBranch(1,1)==route::b);
}

TEST_CASE("Parallel Blend routes every source family and exposes the normal lock action", "[chain-routing-ui][ui][modulation][preset-locks]")
{
    FireAudioProcessor p;p.hasUpdateCheckBeenPerformed=true;FireAudioProcessorEditor editor(p);editor.stopTimer();editor.setSize(1000,500);show(editor);
    auto* rack=find<Rack>(editor,[](auto& r){return r.getScope()==1;});REQUIRE(rack);rack->showRouting();auto* body=Access::body(*rack);REQUIRE(body);Access::mode(*body,route::parallel);
    auto& slider=rack->getParallelBlendSlider();
    REQUIRE(Access::popup(*rack));CHECK(editor.getLocalBounds().contains(Access::popup(*rack)->getBounds()));
    CHECK(body->getLocalBounds().contains(slider.getBounds()));CHECK(body->getHeight()<=440);
    CHECK(slider.isShowing());CHECK(slider.isEnabled());slider.grabKeyboardFocus();CHECK(slider.hasKeyboardFocus(true));
    const auto beforeKey=p.getParallelBlend(1);
    CHECK(slider.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)));CHECK(p.getParallelBlend(1)>beforeKey);
    for(int source:{0,fire::mod_sources::envelope,fire::mod_sources::firstMacro,fire::mod_sources::firstMidiCC})
    {auto choose=Access::assignment(slider);choose(source+1);}
    const auto info=p.getModulationInfoForParameter(route::blendID(1));CHECK(info.routeCount==4);CHECK(slider.usesModulationSummary);
    auto lock=Access::assignment(slider);lock(1003);CHECK(p.isPresetParameterLocked(route::blendID(1)));CHECK(slider.isPresetLocked);
    Access::close(*rack);
    auto* lfo=find<LfoPanel>(editor,[](auto&){return true;});REQUIRE(lfo);lfo->onRevealModulationTarget(route::blendID(1));
    REQUIRE(Access::body(*rack));CHECK(&rack->getParallelBlendSlider()==&slider);CHECK(slider.isShowing());
}

TEST_CASE("An external preset during a routing notification is not accepted as the popup's own revision", "[chain-routing-ui][ui][lifecycle][host]")
{
    Fixture f;juce::MemoryBlock original;f.processor.getStateInformation(original);auto& body=f.open();
    struct RestoreOnChange:juce::AudioProcessorListener
    {
        FireAudioProcessor& p;const juce::MemoryBlock& state;bool armed=true;
        RestoreOnChange(FireAudioProcessor& owner,const juce::MemoryBlock& original):p(owner),state(original){}
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float)override{}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details)override
        {if(armed && details.nonParameterStateChanged){armed=false;p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));}}
    } observer(f.processor,original);
    f.processor.addListener(&observer);Access::mode(body,route::parallel);f.processor.removeListener(&observer);
    CHECK_FALSE(observer.armed);CHECK(f.processor.getChainRoutingMode(1)==route::serial);CHECK_FALSE(body.contextCurrent());
    Access::branch(body,0,route::b);CHECK(f.processor.getModuleBranch(1,0)==route::a);
}

TEST_CASE("A stale routing popup cannot open a new M-S edit before its timer observes an external restore", "[chain-routing-ui][ui][lifecycle][midside]")
{
    Fixture f;juce::MemoryBlock saved;f.processor.getStateInformation(saved);auto& body=f.open();
    auto pending=Access::channelResult(body);
    f.processor.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
    REQUIRE_FALSE(body.contextCurrent());
    REQUIRE(Access::channelEnabled(body)); // No UI timer has yet disabled the stale surface.
    pending(3);
    auto reopened=Access::channelResult(body); // This callback captures the new processor revision.
    reopened(2);
    CHECK(f.processor.treeState.getRawParameterValue(fire::channel_mode::parameterID(1))->load()==fire::channel_mode::stereo);
}

TEST_CASE("Routing commands and Blend gestures cannot write after a host deletes or rebinds their owner", "[chain-routing-ui][ui][lifecycle][host]")
{
    for(bool destroy:{false,true})
    {
        FireAudioProcessor p;juce::Component host;host.setSize(800,620);show(host);
        auto rack=std::make_unique<Rack>(p,1);host.addAndMakeVisible(*rack);rack->setBounds(20,20,210,390);rack->showRouting();auto* body=Access::body(*rack);REQUIRE(body);Access::mode(*body,route::parallel);
        auto* slider=&rack->getParallelBlendSlider();auto* parameter=p.treeState.getParameter(route::blendID(1));Gesture gesture;
        gesture.onBegin=[&]{if(destroy)rack.reset();else rack->setScope(2);};parameter->addListener(&gesture);
        slider->setValue(.81,juce::sendNotificationSync);
        CHECK(p.getParallelBlend(1)==Catch::Approx(.5));CHECK(gesture.begins==1);CHECK(gesture.ends==1);parameter->removeListener(&gesture);
    }
}

TEST_CASE("Band and Master panels retire borrowed Blend controls before base slider teardown", "[chain-routing-ui][ui][lifecycle][ownership]")
{
    FireAudioProcessor p;
    for(bool master:{false,true})
    {
        CAPTURE(master);
        std::unique_ptr<juce::Component> panel;
        if(master)panel=std::make_unique<GlobalPanel>(p,std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{});
        else panel=std::make_unique<BandPanel>(p,std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{},std::function<void(ModulatableSlider*)>{});
        const auto id=route::blendID(master?0:1);
        auto* controls=dynamic_cast<PanelBase*>(panel.get());
        REQUIRE(controls!=nullptr);
        const auto& sliders=controls->getModulatableSliders();
        const auto found=std::find_if(sliders.begin(),sliders.end(),[&](const auto* slider){return slider && slider->getParamID()==id;});
        REQUIRE(found!=sliders.end());
        CHECK(std::count(sliders.begin(),sliders.end(),*found)==1);
        juce::Component::SafePointer<ModulatableSlider> blend(*found);
        panel.reset();
        CHECK(blend==nullptr);
    }
}

TEST_CASE("Parallel branches use existing trailing space and the routing header preserves all four skin layouts", "[chain-routing-ui][ui][skin][layout]")
{
    Fixture f;const auto viewport=f.rack.getViewport().getBounds();const int slot=f.processor.addInsertEffect(1,fire::effects::Type::compressor);REQUIRE(slot>=0);f.rack.refresh();
    auto& body=f.open();Access::mode(body,route::parallel);Access::branch(body,fire::module_order::firstInsert+slot,route::b);f.rack.animationTick(.02f);
    Access::hover(f.rack,fire::module_order::firstInsert+slot);
    for(int i=0;i<40;++i)f.rack.animationTick(.03f);
    CHECK(Access::badge(f.rack,fire::module_order::firstInsert+slot).getAlpha()<.1f);
    Access::hover(f.rack,-1);
    for(auto skin:fire::ui::skins)
    {
        fire::ui::setSkin(f.host,skin);f.look.setSkin(skin);f.host.sendLookAndFeelChange();
        CHECK(f.rack.getViewport().getBounds()==viewport);CHECK(Access::summary(f.rack).getWidth()==70);
        auto& badge=Access::badge(f.rack,fire::module_order::firstInsert+slot);CHECK(badge.isVisible());
        bool accepts=false,children=false;badge.getInterceptsMouseClicks(accepts,children);CHECK_FALSE(accepts);CHECK_FALSE(children);
        for(auto* child:body.getChildren())if(child->isVisible())CHECK(body.getLocalBounds().contains(child->getBounds()));
        preview(body,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-parallel-routing");
        Access::mode(body,route::serial);preview(body,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-serial-routing");Access::mode(body,route::parallel);
    }
}

TEST_CASE("Module branch context commands preserve Serial mode and reject recycled identities", "[chain-routing-ui][ui][lifecycle][rack-interaction]")
{
    Fixture f;auto assign=f.rack.createModuleMenuResultHandler(0);assign(Rack::moveToB);
    CHECK(f.processor.getModuleBranch(1,0)==route::b);CHECK(f.processor.getChainRoutingMode(1)==route::serial);
    assign=f.rack.createModuleMenuResultHandler(0);f.processor.removeModule(1,0);REQUIRE(f.processor.restoreLegacyModule(1,0));
    CHECK(f.processor.getModuleBranch(1,0)==route::a);assign(Rack::moveToB);CHECK(f.processor.getModuleBranch(1,0)==route::a);
}

TEST_CASE("Matrix classifies parallel Blend as scope routing without requiring an EQ module", "[chain-routing-ui][ui][modulation-matrix]")
{
    FireAudioProcessor p;p.removeModule(0,0);REQUIRE(p.setChainRoutingMode(0,route::parallel));
    auto session=std::make_shared<ModulationRoutingEditSession>();session->revision=p.getLfoManager().getModulationRoutingStateSnapshot().revision;
    ModulationMatrixRow row(p,0,ModulationRouting{0,route::blendID(0),.5f,false,false},session,[](std::uint64_t,ModulationRouting){});
    auto choices=Access::targets(row);
    const auto choice=std::find_if(choices.begin(),choices.end(),[](const auto& c){return c.parameterID==route::blendID(0);});
    REQUIRE(choice!=choices.end());CHECK(choice->scope=="Master");CHECK(choice->module=="Routing");CHECK(choice->label=="Blend");CHECK(choice->active);
    REQUIRE(p.setChainRoutingMode(0,route::serial));choices=Access::targets(row);
    const auto serial=std::find_if(choices.begin(),choices.end(),[](const auto& c){return c.parameterID==route::blendID(0);});
    REQUIRE(serial!=choices.end());CHECK_FALSE(serial->active);
}
