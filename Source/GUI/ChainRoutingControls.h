#pragma once
#include "LookAndFeel.h"
#include "ContextAwareComboBox.h"
#include "ModulatableSlider.h"
#include "ValueEntryPopup.h"
#include "../PluginProcessor.h"
#include "../Utility/ParallelChainParameters.h"
#include "../Utility/ChannelModeParameters.h"

struct ChainRoutingUiTestAccess;
namespace fire::ui
{
class RoutingSummaryButton final : public PrimaryTextButton
{
public:
    bool parallel=false;
    void paintButton(juce::Graphics& g,bool over,bool down) override
    {
        const auto& p=paletteFor(*this);auto area=getLocalBounds().toFloat().reduced(.5f);
        const auto unit=juce::jmax(.5f,getHeight()/21.f);
        g.setColour(p.surface0.interpolatedWith(p.accent,(over || hasKeyboardFocus(false))?.10f:0.f));
        g.fillRoundedRectangle(area,3*unit);g.setColour(p.hairline);g.drawRoundedRectangle(area,3*unit,.8f*unit);
        auto icon=area.removeFromRight(18*unit).reduced(3*unit);
        g.setColour(p.textPrimary.withAlpha(isEnabled()?1.f:.45f));g.setFont(isLineSkin(*this)?portfolioFont(10*unit):bodyFont(10*unit));
        g.drawText(getButtonText(),area.reduced(5*unit,0),juce::Justification::centredLeft);
        const auto c=icon.getCentre();juce::Path path;
        if(parallel)
        {path.startNewSubPath(icon.getX(),c.y);path.lineTo(c.x-2*unit,c.y);path.lineTo(c.x-2*unit,c.y-3*unit);path.lineTo(icon.getRight(),c.y-3*unit);
         path.startNewSubPath(c.x-2*unit,c.y);path.lineTo(c.x-2*unit,c.y+3*unit);path.lineTo(icon.getRight(),c.y+3*unit);}
        else{path.startNewSubPath(icon.getX(),c.y);path.lineTo(icon.getRight(),c.y);}
        g.setColour(parallel?p.accent:p.textSecondary);g.strokePath(path,juce::PathStrokeType((down?1.5f:1.1f)*unit,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
};

// The rack owns channel and Blend for its entire lifetime. This temporary view
// borrows them, preserving the editor's registered modulation-slider pointers.
class ChainRoutingControls final : public juce::Component, private juce::Timer
{
public:
    ChainRoutingControls(FireAudioProcessor& p,int targetScope,ContextAwareComboBox& channelControl,
                         ModulatableSlider& blendControl,std::function<bool()> ownerValid)
        :processor(p),scope(targetScope),channel(channelControl),blend(blendControl),validOwner(std::move(ownerValid)),
         expectedRevision(p.getChainEditRevision()),channelAlive(&channelControl),blendAlive(&blendControl)
    {
        setLookAndFeel(&look);setTitle((scope==0?juce::String("Master"):"Band "+juce::String(scope))+" channel and chain routing");
        addAndMakeVisible(channel);addAndMakeVisible(blend);
        mode.addItem("Serial",1);mode.addItem("Parallel A/B",2);mode.setTitle("Effect chain routing mode");
        mode.setComponentID(parallel_chain::modeID(scope));mode.setTooltip("Serial processes modules in order. Parallel sends the same input through A and B, then blends their outputs.");
        addAndMakeVisible(mode);
        const juce::Component::SafePointer<ChainRoutingControls> safe(this);
        mode.configurePopupSession([safe]{return safe?safe->viewGeneration+safe->processor.getChainEditRevision():0;},
            [safe]{return safe && safe->contextCurrent();},[safe](int id){if(safe)safe->changeMode(id-1);});
        for(size_t i=0;i<2;++i)
        {viewports[i].setViewedComponent(&contents[i],false);viewports[i].setScrollBarsShown(true,false);viewports[i].setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);addAndMakeVisible(viewports[i]);}
        originalValueEntry=blend.onSetValueRequested;
        blend.onSetValueRequested=[safe](ModulatableSlider*,const juce::String& id)
        {
            if(!safe || !safe->contextCurrent() || id!=parallel_chain::blendID(safe->scope))return;
            safe->depthEntry.dismissSession();safe->depthEntry.setBounds(safe->getLocalBounds().withSizeKeepingCentre(180,80));
            safe->depthEntry.setVisible(true);safe->depthEntry.toFront(true);safe->depthEntry.grabKeyboardFocus();
        };
        depthEntry.onOk=[safe](double value)
        {
            if(!safe || !safe->contextCurrent())return;
            auto owner=safe;const auto id=parallel_chain::blendID(owner->scope);owner->depthEntry.setVisible(false);
            owner->processor.setModulationValue(id,static_cast<float>(value));
        };
        depthEntry.onCancel=[safe]{if(safe)safe->depthEntry.setVisible(false);};addChildComponent(depthEntry);
        setSize(620,510);refresh();lookAndFeelChanged();
    }
    ~ChainRoutingControls() override
    {
        tearingDown=true;
        stopTimer();depthEntry.onCancel={};depthEntry.onOk={};depthEntry.dismissSession();
        if(blendAlive)blendAlive->onSetValueRequested=std::move(originalValueEntry);
        for(auto& view:viewports)view.setViewedComponent(nullptr,false);
        if(blendAlive)removeChildComponent(blendAlive.getComponent());
        if(channelAlive)removeChildComponent(channelAlive.getComponent());setLookAndFeel(nullptr);
    }
    bool contextCurrent()const
    {return validOwner && validOwner() && !invalidated && processor.getChainEditRevision()==expectedRevision;}
    void styleCallout(juce::CallOutBox& box){setSkin(box,skinFor(*this));box.setLookAndFeel(&calloutLook);}
    void dismiss()
    {
        const juce::Component::SafePointer<ChainRoutingControls> safe(this);++viewGeneration;
        mode.dismissTransientInteraction();if(!safe)return;
        if(channelAlive)channelAlive->dismissTransientInteraction();if(!safe)return;
        depthEntry.dismissSession();if(!safe)return;
        if(blendAlive)blendAlive->dismissTransientInteraction();if(!safe)return;
        for(auto& row:rows)for(auto* button:{&row->move,&row->up,&row->down}){button->dismissPointerGesture();if(!safe)return;}
    }
    void visibilityChanged() override{if(isShowing())startTimerHz(30);else{stopTimer();dismiss();}}
    void enablementChanged() override{if(!isEnabled())dismiss();}
    void lookAndFeelChanged() override
    {
        if(tearingDown)return;
        look.setSkin(skinFor(*this));const auto& p=paletteFor(*this);
        for(auto* menu:std::array<ContextAwareComboBox*,2>{&mode,channelAlive.getComponent()})
        if(menu)
        {menu->setColour(juce::ComboBox::backgroundColourId,p.surface0);menu->setColour(juce::ComboBox::textColourId,p.textPrimary);menu->setColour(juce::ComboBox::arrowColourId,p.textSecondary);menu->setColour(juce::ComboBox::outlineColourId,p.hairline);}
        for(auto& view:viewports)view.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,p.textMuted.withAlpha(.3f));
        repaint();
    }
    void resized() override
    {
        if(!channelAlive || !blendAlive)return;
        auto area=getLocalBounds().reduced(18);area.removeFromTop(28);
        auto selectors=area.removeFromTop(48);auto left=selectors.removeFromLeft((selectors.getWidth()-12)/2);selectors.removeFromLeft(12);
        channelCaption=left.removeFromTop(18);channel.setBounds(left);modeCaption=selectors.removeFromTop(18);mode.setBounds(selectors);
        area.removeFromTop(16);footer=area.removeFromBottom(116);area.removeFromBottom(8);
        const bool parallel=processor.getChainRoutingMode(scope)==parallel_chain::parallel;
        auto leftLane=parallel?area.removeFromLeft((area.getWidth()-12)/2):area;
        if(parallel)area.removeFromLeft(12);
        laneTitles[0]=leftLane.removeFromTop(24);viewports[0].setBounds(leftLane);
        laneTitles[1]=area.removeFromTop(24);viewports[1].setBounds(area);viewports[1].setVisible(parallel);
        const int knobWidth=88;blend.setBounds(footer.withSizeKeepingCentre(knobWidth,108));
        for(size_t i=0;i<2;++i)
        {const int width=juce::jmax(1,viewports[i].getWidth()-6);contents[i].setSize(width,juce::jmax(viewports[i].getHeight(),counts[i]*34));}
        std::array<int,2> positions{};
        for(auto& row:rows){const int lane=parallel?row->branch:0;row->setBounds(0,positions[static_cast<size_t>(lane)]++*34,contents[static_cast<size_t>(lane)].getWidth(),32);}
    }
    void paint(juce::Graphics& g) override
    {
        drawCanvas(g,getLocalBounds().toFloat(),skinFor(*this));const auto& p=paletteFor(*this);g.setColour(p.textPrimary);g.setFont(displayFont(18));
        g.drawText(scope==0?"Master / Routing":"Band "+juce::String(scope)+" / Routing",getLocalBounds().reduced(18).withHeight(26),juce::Justification::centredLeft);
        g.setFont(labelFont(10));g.setColour(p.textMuted);g.drawText("PROCESSING CHANNEL",channelCaption,juce::Justification::centredLeft);g.drawText("SIGNAL PATH",modeCaption,juce::Justification::centredLeft);
        const bool parallel=processor.getChainRoutingMode(scope)==parallel_chain::parallel;
        for(size_t lane=0;lane<(parallel?2u:1u);++lane)
        {
            g.setColour(p.textSecondary);g.setFont(labelFont(11));g.drawText(parallel?(lane==0?"BRANCH A":"BRANCH B"):"SERIAL ORDER",laneTitles[lane],juce::Justification::centredLeft);
            if(counts[lane]==0){g.setFont(bodyFont(12));g.setColour(p.textMuted);g.drawText("Dry input",viewports[lane].getBounds().withHeight(50),juce::Justification::centred);}
        }
        g.setFont(bodyFont(11));g.setColour(p.textSecondary);
        auto left=footer.withTrimmedRight(footer.getWidth()/2+55),right=footer.withTrimmedLeft(footer.getWidth()/2+55);
        g.drawFittedText(invalidated?"The chain changed. Reopen Routing to continue.":parallel?"A only at 0% B\nB only at 100% B":"Blend and branch assignments are used in Parallel mode.",left,juce::Justification::centredLeft,3);
        g.drawFittedText("Linear blend.\nAn empty branch passes dry input.",right,juce::Justification::centredLeft,3);
    }
private:
    friend struct ::ChainRoutingUiTestAccess;
    struct Row final:juce::Component
    {
        int node=-1,branch=0;juce::Label name;PrimaryTextButton move,up{"Up"},down{"Down"};
        Row(){name.setFont(bodyFont(11));addAndMakeVisible(name);addAndMakeVisible(move);addAndMakeVisible(up);addAndMakeVisible(down);}
        void resized()override{auto a=getLocalBounds();down.setBounds(a.removeFromRight(38));a.removeFromRight(3);up.setBounds(a.removeFromRight(28));a.removeFromRight(3);move.setBounds(a.removeFromRight(38));name.setBounds(a);}
    };
    class CalloutLook final:public juce::LookAndFeel_V4
    {void drawCallOutBoxBackground(juce::CallOutBox& box,juce::Graphics& g,const juce::Path& path,juce::Image&)override
     {const auto& p=paletteFor(box);juce::DropShadow(juce::Colours::black.withAlpha(.18f),7,{0,2}).drawForPath(g,path);g.setColour(p.canvas);g.fillPath(path);g.setColour(p.hairline);g.strokePath(path,juce::PathStrokeType(1));}};
    void invalidate()
    {
        invalidated=true;const juce::Component::SafePointer<ChainRoutingControls> safe(this);dismiss();if(!safe)return;
        mode.setEnabled(false);if(!safe)return;
        if(channelAlive)channelAlive->setEnabled(false);if(!safe)return;
        if(blendAlive)blendAlive->setEnabled(false);if(!safe)return;
        for(auto& row:rows)for(auto* button:{&row->move,&row->up,&row->down}){button->setEnabled(false);if(!safe)return;}
        repaint();stopTimer();
    }
    template<class Operation> void perform(Operation operation)
    {
        if(!contextCurrent() || !isShowing())return;
        const juce::Component::SafePointer<ChainRoutingControls> safe(this);
        const auto before=expectedRevision;std::uint64_t committed=0;
        const bool changed=operation(committed);
        if(!safe)return;
        if(!safe->validOwner || !safe->validOwner()){safe->invalidate();return;}
        const auto after=safe->processor.getChainEditRevision();
        if((changed && committed!=0 && after==committed) || (!changed && after==before))
        {safe->expectedRevision=after;safe->refresh();}
        else safe->invalidate();
    }
    void changeMode(int value)
    {auto* p=&processor;const int s=scope;perform([p,s,value](auto& revision){return p->setChainRoutingMode(s,value,&revision);});}
    void changeBranch(int node,int value)
    {auto* p=&processor;const int s=scope;perform([p,s,node,value](auto& revision){return p->setModuleBranch(s,node,value,&revision);});}
    void moveNode(int node,int direction)
    {
        if(!contextCurrent())return;
        const auto lane=processor.getModuleBranch(scope,node);std::vector<int> ordered;
        const bool parallel=processor.getChainRoutingMode(scope)==parallel_chain::parallel;
        const auto globalOrder=processor.getModuleOrder(scope);
        for(int candidate:globalOrder)if(parallel_chain::validNode(scope,candidate) && processor.isModulePresent(scope,candidate)
            && (!parallel || processor.getModuleBranch(scope,candidate)==lane))ordered.push_back(candidate);
        const auto it=std::find(ordered.begin(),ordered.end(),node);if(it==ordered.end())return;
        const auto index=static_cast<int>(std::distance(ordered.begin(),it));const int next=index+direction;
        if(!juce::isPositiveAndBelow(next,static_cast<int>(ordered.size())))return;
        int anchor=ordered[static_cast<size_t>(next)];
        if(direction>0)
        {
            const auto at=std::find(globalOrder.begin(),globalOrder.end(),anchor);
            anchor=-1;
            // Permanent slots remain in globalOrder after deletion, whereas
            // moveModuleBefore accepts only a currently visible anchor.
            for(auto nextNode=at+1;nextNode!=globalOrder.end();++nextNode)
                if(*nextNode>=0 && processor.isModulePresent(scope,*nextNode)){anchor=*nextNode;break;}
        }
        auto* p=&processor;const int s=scope;const auto before=expectedRevision;
        perform([p,s,node,anchor,before](auto& revision){p->moveModuleBefore(s,node,anchor);revision=before+2;return true;});
    }
    void refresh()
    {
        if(!contextCurrent()){invalidate();return;}
        const juce::Component::SafePointer<ChainRoutingControls> safe(this);++viewGeneration;
        for(auto& row:rows)for(auto* button:{&row->move,&row->up,&row->down}){button->dismissPointerGesture();if(!safe)return;}
        rows.clear();counts={};
        const bool parallel=processor.getChainRoutingMode(scope)==parallel_chain::parallel;
        mode.setSelectedId(parallel?2:1,juce::dontSendNotification);blend.setEnabled(parallel);
        if(!safe)return;
        const auto epoch=viewGeneration;
        for(int node:processor.getModuleOrder(scope))
        {
            if(!parallel_chain::validNode(scope,node) || !processor.isModulePresent(scope,node))continue;
            auto row=std::make_unique<Row>();row->node=node;row->branch=processor.getModuleBranch(scope,node);
            const auto type=node<module_order::firstInsert?core_modules::legacyType(scope,node):processor.getInsertEffectType(scope,node-module_order::firstInsert);
            row->name.setText(effects::name(type),juce::dontSendNotification);row->name.setColour(juce::Label::textColourId,paletteFor(*this).textPrimary);
            const auto id="routingModule"+juce::String(node);row->setComponentID(id);
            row->move.setButtonText(row->branch==0?"To B":"To A");row->move.setTitle("Move "+juce::String(effects::name(type))+" to Branch "+(row->branch==0?"B":"A"));
            row->move.setTooltip(parallel?"Move this module to the other parallel branch":"Set the branch remembered for Parallel mode");
            row->move.onClick=[safe,node,branch=1-row->branch,epoch]{auto owner=safe;if(owner && owner->viewGeneration==epoch)owner->changeBranch(node,branch);};
            row->up.onClick=[safe,node,epoch]{auto owner=safe;if(owner && owner->viewGeneration==epoch)owner->moveNode(node,-1);};
            row->down.onClick=[safe,node,epoch]{auto owner=safe;if(owner && owner->viewGeneration==epoch)owner->moveNode(node,1);};
            const int lane=parallel?row->branch:0;contents[static_cast<size_t>(lane)].addAndMakeVisible(*row);++counts[static_cast<size_t>(lane)];rows.push_back(std::move(row));
        }
        resized();repaint();
    }
    void timerCallback()override
    {
        if(!contextCurrent()){invalidate();return;}
        const auto modeNow=processor.getChainRoutingMode(scope);
        bool changed=mode.getSelectedId()!=modeNow+1;
        for(const auto& row:rows)changed=changed || processor.getModuleBranch(scope,row->node)!=row->branch;
        if(changed)refresh();
    }
    FireAudioProcessor& processor;int scope;ContextAwareComboBox& channel;ModulatableSlider& blend;
    std::function<bool()> validOwner;std::uint64_t expectedRevision=0,viewGeneration=0;bool invalidated=false,tearingDown=false;
    juce::Component::SafePointer<ContextAwareComboBox> channelAlive;
    juce::Component::SafePointer<ModulatableSlider> blendAlive;
    FireLookAndFeel look;CalloutLook calloutLook;ContextAwareComboBox mode;
    std::array<juce::Viewport,2> viewports;std::array<juce::Component,2> contents;
    std::vector<std::unique_ptr<Row>> rows;std::array<int,2> counts{};
    ValueEntryPopup depthEntry;std::function<void(ModulatableSlider*,const juce::String&)> originalValueEntry;
    juce::Rectangle<int> channelCaption,modeCaption,footer;std::array<juce::Rectangle<int>,2> laneTitles;
};
}
