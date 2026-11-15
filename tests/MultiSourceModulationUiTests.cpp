#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <GUI/ModulatableSlider.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>

struct MultiSourceModulationUiTestAccess
{
    static auto popup(ModulationMatrixRoutingComboBox& box) {return box.createPopupResultHandler();}
    static auto menu(ModulatableSlider& slider) {return slider.createModulationMenuResultHandler();}
};

namespace
{
template<class T> T* mappingControl(juce::Component& root,const juce::String& id)
{
    if(root.getComponentID()==id) if(auto* control=dynamic_cast<T*>(&root)) return control;
    for(auto* child:root.getChildren()) if(auto* control=mappingControl<T>(*child,id)) return control;
    return nullptr;
}
std::vector<ModulationMatrixRow*> mappingRows(juce::Component& root)
{
    std::vector<ModulationMatrixRow*> result;
    const auto collect=[&](auto&& self,juce::Component& parent)->void
    {
        if(auto* row=dynamic_cast<ModulationMatrixRow*>(&parent)) result.push_back(row);
        for(auto* child:parent.getChildren()) self(self,*child);
    };
    collect(collect,root);return result;
}
void seedMappingRoutes(FireAudioProcessor& processor)
{
    auto& manager=processor.getLfoManager();
    const juce::ScopedLock lock(manager.getLfoDataLock());
    auto& routes=manager.getModulationRoutings();routes.clear();
    routes.add({0,"drive1",.5f,true,false});routes.add({1,"drive1",-.25f,false,false});
    manager.advanceModulationRoutingRevisionLocked();
}
juce::MouseEvent mappingPointer(juce::Component& owner,juce::Point<float> position,
                                juce::Point<float> start,juce::ModifierKeys modifiers,bool dragged=false)
{
    const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),position,modifiers,0,0,0,0,0,&owner,&owner,now,start,now,1,dragged};
}
void mappingPreview(juce::Component& panel,const juce::String& name)
{
    if(const auto* directory=std::getenv("FIRE_MODULATION_MAPPING_PREVIEW_DIR"))
    {
        const auto file=juce::File(directory).getChildFile(name+".png");REQUIRE(file.getParentDirectory().createDirectory().wasOk());
        auto stream=file.createOutputStream();REQUIRE(stream);stream->setPosition(0);stream->truncate();
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()),*stream));
    }
}
}

TEST_CASE("Two-line modulation routes fit every skin and keep duplicate targets independently editable",
          "[ui][modulation-matrix][multi-source][mapping][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;seedMappingRoutes(processor);
    ModulationMatrixPanel panel(processor);panel.setSize(800,480);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);panel.setVisible(true);
    const auto rows=mappingRows(panel);REQUIRE(rows.size()==2);
    CHECK(rows[0]->getTargetParameterID()==rows[1]->getTargetParameterID());
    for(auto skin:fire::ui::skins)
        for(int width:{ModulationMatrixPanel::minimumContentWidth,800,1100})
        {
            CAPTURE(static_cast<int>(skin),width);
            fire::ui::setSkin(panel,skin);panel.sendLookAndFeelChange();panel.setSize(width,480);
            for(auto* row:rows)
                for(auto* child:row->getChildren()) if(child->isVisible())
                {
                    CAPTURE(child->getComponentID(),child->getBounds().toString());
                    CHECK(row->getLocalBounds().contains(child->getBounds()));
                    for(auto* other:row->getChildren()) if(other!=child && other->isVisible())
                        CHECK_FALSE(child->getBounds().intersects(other->getBounds()));
                }
            if(width==800) mappingPreview(panel,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-mapping");
        }
    panel.focusTarget("drive1");CHECK(mappingRows(panel).size()==2);
    auto* from=mappingControl<juce::Slider>(*rows[0],"matrix_range_start");
    auto* to=mappingControl<juce::Slider>(*rows[0],"matrix_range_end");
    auto* curve=mappingControl<juce::ComboBox>(*rows[0],"matrix_curve");
    auto* macro=mappingControl<juce::ComboBox>(*rows[0],"matrix_depth_macro");
    REQUIRE(from);REQUIRE(to);REQUIRE(curve);REQUIRE(macro);
    CHECK(from->getValue()==Catch::Approx(-.5));CHECK(to->getValue()==Catch::Approx(.5));
    REQUIRE(from->getAccessibilityHandler());
    REQUIRE(static_cast<juce::Component&>(*from).keyPressed(juce::KeyPress(juce::KeyPress::rightKey)));
    CHECK(from->getValue()>-.5);
    from->setValue(-.25,juce::sendNotificationSync);
    auto current=processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(current.size()==2);CHECK(current[0].customRange);CHECK(current[0].rangeEnd==Catch::Approx(.5));
    to->setValue(.75,juce::sendNotificationSync);curve->setSelectedId(3,juce::sendNotificationSync);macro->setSelectedId(3,juce::sendNotificationSync);
    current=processor.getLfoManager().getModulationRoutingsCopy();
    CHECK(current[0].rangeStart==Catch::Approx(-.25));CHECK(current[0].rangeEnd==Catch::Approx(.75));
    CHECK(current[0].curve==2);CHECK(current[0].depthMacro==1);
    CHECK(current[1].sourceLfoIndex==1);CHECK(current[1].depth==Catch::Approx(-.25));CHECK_FALSE(current[1].customRange);
    juce::Button* polarity=nullptr;
    for(auto* child:rows[0]->getChildren()) if(auto* button=dynamic_cast<juce::Button*>(child);button && button->getTitle().endsWith("polarity")) polarity=button;
    REQUIRE(polarity);polarity->setToggleState(false,juce::sendNotificationSync);
    current=processor.getLfoManager().getModulationRoutingsCopy();
    CHECK_FALSE(current[0].customRange);CHECK_FALSE(current[0].isBipolar);
    CHECK(current[0].minimumOffset()==0.0f);CHECK(current[0].maximumOffset()==1.0f);
    CHECK(from->getValue()==Catch::Approx(0.0).margin(1.0e-12));
    CHECK(to->getValue()==Catch::Approx(1.0).margin(1.0e-12));
    CHECK(current[0].curve==2);CHECK(current[0].depthMacro==1);
}

TEST_CASE("Mapping popups cannot replay a result over a later custom-range edit",
          "[ui][modulation-matrix][multi-source][mapping][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;seedMappingRoutes(processor);
    ModulationMatrixPanel panel(processor);panel.setSize(800,480);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);panel.setVisible(true);
    const auto rows=mappingRows(panel);REQUIRE(rows.size()==2);
    auto* curve=mappingControl<ModulationMatrixRoutingComboBox>(*rows[0],"matrix_curve");
    auto* from=mappingControl<juce::Slider>(*rows[0],"matrix_range_start");REQUIRE(curve);REQUIRE(from);
    auto result=MultiSourceModulationUiTestAccess::popup(*curve);
    from->setValue(-.1,juce::sendNotificationSync);result(4);
    const auto state=processor.getLfoManager().getModulationRoutingsCopy();
    CHECK(state[0].customRange);CHECK(state[0].rangeStart==Catch::Approx(-.1));CHECK(state[0].curve==0);
}

TEST_CASE("Mapped modulation badges open the matrix without editing an arbitrary route",
          "[ui][modulatable-slider][multi-source][mapping][interaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel look;
    juce::Component host;ModulatableSlider slider;
    host.addAndMakeVisible(slider);host.setSize(160,150);slider.setBounds(10,10,100,120);slider.setLookAndFeel(&look);
    const juce::ScopeGuard resetLook{[&] {slider.setLookAndFeel(nullptr);}};
    host.addToDesktop(juce::ComponentPeer::windowIsTemporary);host.setVisible(true);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setRange(0,1);slider.setValue(.5,juce::dontSendNotification);
    slider.parameterID="drive1";slider.isModulated=true;slider.lfoSource=1;
    int edited=0,depthEdits=0,inverted=0,cleared=0;
    slider.onEditModulationRequested=[&](const juce::String& target) {CHECK(target=="drive1");++edited;};
    slider.onModAmountChanged=[&](double) {++depthEdits;};slider.onModulationReset=[&] {++depthEdits;};
    slider.onModulationInverted=[&](const auto&) {++inverted;};slider.onModulationCleared=[&](const auto&) {++cleared;};
    for(int count:{2,1})
    {
        slider.modulationRouteCount=count;slider.usesModulationSummary=true;
        slider.modulationOffsetMinimum=-.2;slider.modulationOffsetMaximum=.4;slider.modulationOffsetCurrent=.15;
        const auto point=slider.getModulationHandleHitBounds().getCentre();
        const auto before=edited;
        slider.mouseDown(mappingPointer(slider,point,point,juce::ModifierKeys::leftButtonModifier));
        slider.mouseDrag(mappingPointer(slider,point.translated(0,-30),point,juce::ModifierKeys::leftButtonModifier,true));
        slider.mouseUp(mappingPointer(slider,point,point,{}));
        CHECK(edited==before+1);CHECK(depthEdits==0);CHECK(slider.getValue()==.5);
        auto menu=MultiSourceModulationUiTestAccess::menu(slider);menu(static_cast<int>(ModulatableSlider::ModulationMenuCommand::invertDepth));
        CHECK(edited==before+2);CHECK(inverted==0);
    }
    std::uint64_t revision=9;
    slider.getLfoSourceMenuState=[&] {ModulatableSlider::LfoSourceMenuState state;state.revision=revision;return state;};
    auto stale=MultiSourceModulationUiTestAccess::menu(slider);++revision;
    stale(static_cast<int>(ModulatableSlider::ModulationMenuCommand::clearModulation));CHECK(cleared==0);
    auto live=MultiSourceModulationUiTestAccess::menu(slider);live(static_cast<int>(ModulatableSlider::ModulationMenuCommand::clearModulation));CHECK(cleared==1);
    live(static_cast<int>(ModulatableSlider::ModulationMenuCommand::clearModulation));CHECK(cleared==1);
    slider.modulationRouteCount=2;
    for(auto skin:fire::ui::skins)
    {
        look.setSkin(skin);fire::ui::setSkin(host,skin);
        slider.usesModulationSummary=true;
        const auto mapped=slider.createComponentSnapshot(slider.getLocalBounds());
        slider.usesModulationSummary=false;slider.lfoAmount=0;
        const auto plain=slider.createComponentSnapshot(slider.getLocalBounds());
        int changed=0;
        for(int y=0;y<mapped.getHeight();++y) for(int x=0;x<mapped.getWidth();++x)
            changed+=mapped.getPixelAt(x,y)!=plain.getPixelAt(x,y);
        CHECK(changed>15);
    }
}


TEST_CASE("Zero-valued route controls initialise their visible percentage text in every skin",
          "[ui][modulation-matrix][multi-source][mapping][format]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const ModulationRouting route {0,"drive1",0.0f,false,false};
    auto session=std::make_shared<ModulationRoutingEditSession>();
    session->revision=processor.getLfoManager().getModulationRoutingStateSnapshot().revision;
    ModulationMatrixRow row(processor,0,route,session,[](std::uint64_t,ModulationRouting) {});
    row.setSize(800,ModulationMatrixPanel::rowHeight);
    const auto visibleText=[](juce::Slider& slider)
    {
        juce::Label* text=nullptr;
        for(auto* child:slider.getChildren()) if(auto* label=dynamic_cast<juce::Label*>(child)) text=label;
        REQUIRE(text);return text->getText();
    };
    auto* from=mappingControl<juce::Slider>(row,"matrix_range_start");
    auto* depth=mappingControl<juce::Slider>(row,"matrix_amount");REQUIRE(from);REQUIRE(depth);
    for(auto skin:fire::ui::skins)
    {
        CAPTURE(static_cast<int>(skin));
        fire::ui::setSkin(row,skin);row.sendLookAndFeelChange();
        CHECK(visibleText(*from)=="0.0%");
        CHECK(visibleText(*depth)=="0%");
    }
}
