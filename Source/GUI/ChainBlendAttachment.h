#pragma once
#include "ModulatableSlider.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace fire::ui
{
// Keeps the normal ModulatableSlider interaction model while ensuring host
// callbacks cannot commit a departed scope or tear down a listener mid-broadcast.
class ChainBlendAttachment
{
public:
    ChainBlendAttachment(juce::AudioProcessorValueTreeState& tree,const juce::String& id,ModulatableSlider& slider,std::function<bool()> valid)
    {
        state=std::make_shared<State>();state->parameter=tree.getParameter(id);state->slider=&slider;state->valid=std::move(valid);
        if(!state->parameter)return;
        const auto& range=state->parameter->getNormalisableRange();
        slider.setNormalisableRange(juce::NormalisableRange<double>{range.start,range.end,range.interval,range.skew,range.symmetricSkew});
        const std::weak_ptr<State> weak=state;
        state->attachment=std::make_shared<juce::ParameterAttachment>(*state->parameter,[weak](float value)
        {if(auto s=weak.lock();s && s->slider)s->slider->setValue(value,juce::dontSendNotification);},nullptr);
        slider.onDragStart=[weak]
        {
            auto s=weak.lock();if(!s || !s->current() || s->gesturing)return;
            s->gesturing=true;s->starting=true;auto* parameter=s->parameter;parameter->beginChangeGesture();
            s->starting=false;if(!s->gesturing)parameter->endChangeGesture();
        };
        slider.onDragEnd=[weak]{if(auto s=weak.lock())s->finish();};
        slider.onValueChange=[weak]
        {
            auto s=weak.lock();if(!s || !s->slider || !s->parameter)return;
            auto* parameter=s->parameter;
            if(!s->current()) {s->slider->setValue(parameter->convertFrom0to1(parameter->getValue()),juce::dontSendNotification);return;}
            if(s->starting)return;
            const float value=parameter->convertTo0to1(static_cast<float>(s->slider->getValue()));
            if(juce::approximatelyEqual(value,parameter->getValue()))return;
            if(s->gesturing){parameter->setValueNotifyingHost(value);return;}
            parameter->beginChangeGesture();const juce::ScopeGuard finish{[parameter]{parameter->endChangeGesture();}};
            if(!s->current())return;
            parameter->setValueNotifyingHost(value);
        };
        state->attachment->sendInitialUpdate();
    }
    ~ChainBlendAttachment()
    {
        auto s=std::move(state);if(!s)return;s->attached=false;
        if(s->slider){s->slider->onDragStart={};s->slider->onDragEnd={};s->slider->onValueChange={};}
        s->finish();
    }
private:
    struct State
    {
        juce::RangedAudioParameter* parameter=nullptr;
        juce::Component::SafePointer<ModulatableSlider> slider;
        std::function<bool()> valid;
        std::shared_ptr<juce::ParameterAttachment> attachment;
        bool attached=true,gesturing=false,starting=false;
        bool current()const{return attached && slider && slider->isShowing() && slider->isEnabled() && valid && valid();}
        void finish(){if(!gesturing)return;gesturing=false;if(!starting && parameter)parameter->endChangeGesture();}
    };
    std::shared_ptr<State> state;
};
}
