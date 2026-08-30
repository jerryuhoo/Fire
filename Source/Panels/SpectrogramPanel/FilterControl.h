/*
  ==============================================================================
 
    FilterControl.h
    Created: 18 Sep 2021 9:54:04pm
    Author:  羽翼深蓝Wings
 
  ==============================================================================
*/

#pragma once

#include "../../PluginProcessor.h"
#include "../../GUI/FireTheme.h"
#include "../../Utility/Parameters.h"
#include "DraggableButton.h"
#include <array>
#include <memory>

class GlobalPanel;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct FilterControlTestAccess;
#endif
//==============================================================================
/*
 */
class FilterControl : public juce::Component,
                      public juce::AudioProcessorParameter::Listener,
                      public juce::ChangeListener
{
public:
    FilterControl(FireAudioProcessor&, GlobalPanel&);
    ~FilterControl() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void animationTick();
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void visibilityChanged() override;
    void dismissTransientInteraction();

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct FilterControlTestAccess;
#endif

    FireAudioProcessor& processor;
    juce::Path responseCurve;
    juce::Path responseFillCurve;
    juce::Path lfoResponseCurve;

    using Filter = juce::dsp::IIR::Filter<float>;
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter, Filter, Filter>;
    // lowcut, peak, highcut, lowcut Q, highcut Q
    MonoChain monoChain;
    MonoChain lfoMonoChain;

    void updateResponseCurve();
    void updateChain();
    void setDraggableButtonBounds();
    void updateLfoChain(const ModulatedFilterValues& modulatedValues);
    void updateLfoResponseCurve();
    void checkAnimationStatus();
    void handleFilterDrag(DraggableButton& button,
                          const juce::MouseEvent& event,
                          const juce::String& selectionParameter,
                          const juce::String& frequencyParameter,
                          const juce::String& gainParameter);
    bool setDragParameterValue(juce::RangedAudioParameter& parameter,
                               float normalisedValue);
    void finishDragParameterGestures() noexcept;
    void finishFilterDrag();
    void updateDraggableButtonStates();
    int getCurvePointCount() const;

    bool isAnimationActive = false;
    std::atomic<bool> parameterUpdatePending { false };
    std::atomic<bool> routingStateDirty { true };
    std::vector<juce::AudioProcessorParameter*> observedParameters;
    std::vector<double> responseMagnitudes;
    std::vector<double> lfoMagnitudes;

    bool dragTooltipVisible = false;
    double dragFrequency = 0.0;
    double dragGain = 0.0;
    juce::Point<int> dragTooltipAnchor;
    struct DragGestureSession;
    std::shared_ptr<DragGestureSession> dragGestureSession;

    DraggableButton draggableLowButton, draggablePeakButton, draggableHighButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterControl)
};
