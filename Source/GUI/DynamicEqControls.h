#pragma once
#include "PerformanceControlHelpers.h"
#include "../Utility/DynamicEqParameters.h"

struct DynamicEqUiTestAccess;
namespace fire::ui
{
struct DynamicEqPresentation
{
    std::uint64_t sequence = 0;
    float idleSeconds = 1, gainChangeDb = 0, effectiveGainDb = 0, opacity = 0;
    void update(const dynamic_eq::VisualState& value, float baseGain, float seconds)
    {
        const bool fresh = value.renderSequence != 0 && value.renderSequence != sequence;
        sequence = value.renderSequence;
        idleSeconds = fresh ? 0.f : idleSeconds + juce::jlimit(0.f, .1f, seconds);
        opacity = value.active ? juce::jlimit(0.f,1.f,1.f-(idleSeconds-.15f)/.2f) : 0.f;
        gainChangeDb = std::isfinite(value.gainChangeDb) ? value.gainChangeDb * opacity : 0.f;
        effectiveGainDb = baseGain + (std::isfinite(value.effectiveGainDb) ? (value.effectiveGainDb-baseGain)*opacity : 0.f);
    }
    void reset() { sequence=0;idleSeconds=1;gainChangeDb=0;effectiveGainDb=0;opacity=0; }
};
inline juce::String dynamicGainText(float value)
{
    if (std::abs(value)<.05f) value=0;
    return (value>0?juce::String("+"):juce::String())+juce::String(value,1)+" dB";
}

class DynamicEqControls final : public juce::Component, private juce::Timer
{
public:
    DynamicEqControls(FireAudioProcessor& owner,int selectedNode,int scope,int slot,std::function<bool()> validator)
        : processor(owner),node(selectedNode),fxScope(scope),fxSlot(slot),valid(std::move(validator))
    {
        setLookAndFeel(&look);
        const juce::Component::SafePointer<DynamicEqControls> safe(this);
        enabled.setButtonText("Enabled");enabled.setTitle("Dynamic EQ enabled");enabled.setComponentID(parameterID(dynamic_eq::enabled));
        enabled.setTooltip("Apply level-dependent gain to this Bell or Shelf point. The stored Gain remains the base value.");
        auto* enableParameter=processor.treeState.getParameter(parameterID(dynamic_eq::enabled));
        enableAttachment=std::make_shared<juce::ParameterAttachment>(*enableParameter,[safe](float value)
        {if(safe)safe->enabled.setToggleState(value>.5f,juce::dontSendNotification);},nullptr);
        enabled.onClick=[safe,enableParameter]
        {
            auto owner=safe;auto* target=enableParameter;
            if(!owner || !owner->isShowing() || !owner->contextCurrent())return;
            const auto keepListenerAlive=owner->enableAttachment;
            const auto value=owner->enabled.getToggleState()?1.f:0.f;
            target->beginChangeGesture();const juce::ScopeGuard finish{[target]{target->endChangeGesture();}};
            if(!owner || !owner->isShowing() || !owner->contextCurrent())return;
            target->setValueNotifyingHost(value);
        };
        enableAttachment->sendInitialUpdate();addAndMakeVisible(enabled);
        const std::array<const char*,5> help {
            "Threshold in dB for the local frequency-filtered, stereo-linked peak detector.",
            "How strongly a signal above Threshold changes the gain. 1:1 produces no dynamic change.",
            "Maximum dynamic gain change. Negative Range cuts above Threshold; positive Range boosts above Threshold.",
            "Time taken for dynamic gain to react when the filtered signal rises above Threshold.",
            "Time taken for dynamic gain to recover as the filtered signal falls."};
        for(int i=0;i<5;++i)
        {
            const auto index=static_cast<size_t>(i);const int field=i+1;
            auto& slider=sliders[index];const auto id=parameterID(field);
            slider.setComponentID(id);slider.setTitle("Dynamic EQ "+juce::String(dynamic_eq::suffixes[static_cast<size_t>(field)]));
            slider.setTooltip(help[index]);slider.setHelpText(help[index]);
            slider.bind(*processor.treeState.getParameter(id),[safe]{return safe && safe->contextCurrent();});
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setSkewFactor(dynamic_eq::rangeFor(field).skew);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,84,24);
            slider.setDoubleClickReturnValue(true,dynamic_eq::defaults[static_cast<size_t>(field)]);
            slider.textFromValueFunction=[field](double value)
            {return field==dynamic_eq::ratio?juce::String(value,2)+":1":juce::String(value,field==dynamic_eq::release?0:1)+(field==dynamic_eq::attack || field==dynamic_eq::release?" ms":" dB");};
            slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue();};
            slider.updateText();addAndMakeVisible(slider);
            labels[index].setText(dynamic_eq::suffixes[static_cast<size_t>(field)],juce::dontSendNotification);
            labels[index].setFont(labelFont(11));labels[index].setJustificationType(juce::Justification::centred);addAndMakeVisible(labels[index]);
        }
        setSize(540,270);lookAndFeelChanged();
    }
    ~DynamicEqControls() override {stopTimer();dismiss();setLookAndFeel(nullptr);}
    void styleCallout(juce::CallOutBox& callout) {setSkin(callout,skinFor(*this));callout.setLookAndFeel(&calloutLook);}
    void dismiss()
    {
        const juce::Component::SafePointer<DynamicEqControls> safe(this);
        enabled.dismissPointerGesture();if(!safe)return;
        for(auto& slider:sliders){slider.dismissTransientInteraction();if(!safe)return;}
    }
    void visibilityChanged() override {if(isShowing())startTimerHz(30);else{stopTimer();dismiss();presentation.reset();}}
    void enablementChanged() override {if(!isEnabled())dismiss();}
    void lookAndFeelChanged() override
    {
        look.setSkin(skinFor(*this));const auto& palette=paletteFor(*this);
        for(auto& label:labels)label.setColour(juce::Label::textColourId,palette.textSecondary);
        for(auto& slider:sliders)
        {
            slider.setColour(juce::Slider::rotarySliderFillColourId,isLineSkin(*this)?palette.accent:colours::filter);
            slider.setColour(juce::Slider::textBoxTextColourId,palette.textPrimary);
            slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        }
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {return performanceUndo(processor,key);}
    void resized() override
    {
        auto area=getLocalBounds().reduced(18);auto heading=area.removeFromTop(46);
        enabled.setBounds(heading.removeFromRight(90).withHeight(28));
        auto knobs=area.removeFromTop(126);const int gap=8,width=(knobs.getWidth()-gap*4)/5;
        for(int i=0;i<5;++i)
        {auto cell=knobs.removeFromLeft(width);labels[static_cast<size_t>(i)].setBounds(cell.removeFromTop(20));sliders[static_cast<size_t>(i)].setBounds(cell);knobs.removeFromLeft(gap);}
        area.removeFromTop(10);meter=area.removeFromTop(12);readout=area;
    }
    void paint(juce::Graphics& g) override
    {
        drawCanvas(g,getLocalBounds().toFloat(),skinFor(*this));const auto& palette=paletteFor(*this);
        auto title=getLocalBounds().reduced(18).withTrimmedRight(100);
        g.setFont(displayFont(17));g.setColour(palette.textPrimary);
        g.drawText("Dynamic EQ / Point "+juce::String(node+1),title.removeFromTop(24),juce::Justification::centredLeft);
        g.setFont(bodyFont(10.5f));g.setColour(palette.textMuted);
        g.drawText("Local filtered detector / stereo-linked peak",title.removeFromTop(20),juce::Justification::centredLeft);
        auto bar=meter.toFloat().reduced(0,4);g.setColour(palette.hairline);g.fillRoundedRectangle(bar,2);
        const float start=bar.getCentreX(),end=start+juce::jlimit(-24.f,24.f,presentation.gainChangeDb)*bar.getWidth()/48.f;
        g.setColour(isLineSkin(*this)?palette.accent:colours::filter);
        g.fillRoundedRectangle(juce::Rectangle<float>::leftTopRightBottom(juce::jmin(start,end),bar.getY(),juce::jmax(start,end),bar.getBottom()),2);
        g.setColour(palette.textSecondary);g.drawVerticalLine(juce::roundToInt(start),float(meter.getY()),float(meter.getBottom()));
        auto text=readout;g.setFont(labelFont(11));
        g.drawText("Gain change  "+dynamicGainText(presentation.gainChangeDb),text.removeFromLeft(text.getWidth()/2),juce::Justification::centredLeft);
        g.setFont(bodyFont(11));g.drawText("Effective Gain  "+dynamicGainText(presentation.effectiveGainDb),text,juce::Justification::centredRight);
    }
private:
    friend struct ::DynamicEqUiTestAccess;
    class CalloutLook final : public juce::LookAndFeel_V4
    {
        void drawCallOutBoxBackground(juce::CallOutBox& box,juce::Graphics& g,const juce::Path& path,juce::Image&) override
        {const auto& p=paletteFor(box);juce::DropShadow(juce::Colours::black.withAlpha(.18f),7,{0,2}).drawForPath(g,path);g.setColour(p.canvas);g.fillPath(path);g.setColour(p.hairline);g.strokePath(path,juce::PathStrokeType(1));}
    };
    juce::String parameterID(int field)const {return dynamic_eq::parameterID(fxSlot<0?0:fxScope,fxSlot,node,field);}
    bool contextCurrent()const
    {
        if(!valid || !valid())return false;
        const auto state=processor.getEqNodeState(node,fxScope,fxSlot);
        return state.present && dynamic_eq::supports(state.type);
    }
    void timerCallback() override
    {
        if(!contextCurrent())
        {
            stopTimer();const juce::Component::SafePointer<DynamicEqControls> safe(this);dismiss();if(!safe)return;
            enabled.setEnabled(false);if(!safe)return;
            for(auto& slider:sliders){slider.setEnabled(false);if(!safe)return;}
            presentation.reset();repaint();return;
        }
        const auto state=processor.getEqNodeState(node,fxScope,fxSlot);
        presentation.update(processor.getDynamicEqVisualState(node,fxScope,fxSlot),state.gainDb,1.f/30.f);repaint();
    }
    FireAudioProcessor& processor;
    int node,fxScope,fxSlot;
    std::function<bool()> valid;
    FireLookAndFeel look;CalloutLook calloutLook;
    PrimaryToggleButton enabled;
    std::array<PerformanceIntegerControl,5> sliders;
    std::array<juce::Label,5> labels;
    std::shared_ptr<juce::ParameterAttachment> enableAttachment;
    DynamicEqPresentation presentation;
    juce::Rectangle<int> meter,readout;
};
}
