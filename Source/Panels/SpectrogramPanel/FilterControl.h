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
#include "../../DSP/EqCoefficients.h"
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
    void suspendTelemetryPresentation();
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void visibilityChanged() override;
    void dismissTransientInteraction();
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct FilterControlTestAccess;
#endif

    FireAudioProcessor& processor;
    juce::Component::SafePointer<GlobalPanel> globalPanel;
    juce::Path responseCurve;
    juce::Path responseFillCurve;
    juce::Path lfoResponseCurve;

    std::array<fire::eq::Coefficients, fire::eq::maxNodes> eqResponse, modulatedEqResponse;
    int lastSelectedNode = -1;

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
    DraggableButton& nodeButton(int slot);

    bool isAnimationActive = false;
    bool telemetryPresentationActive = false;
    bool hasFreshTelemetryForPresentation = false;
    std::uint64_t requiredTelemetryCaptureEpoch = 0;
    std::atomic<bool> parameterUpdatePending { false };
    std::atomic<bool> routingStateDirty { true };
    std::vector<juce::AudioProcessorParameter*> observedParameters;
    std::vector<double> responseMagnitudes;
    std::vector<double> lfoMagnitudes;
    double responseSampleRate = 0.0;

    bool dragTooltipVisible = false;
    double dragFrequency = 0.0;
    double dragGain = 0.0;
    juce::Point<int> dragTooltipAnchor;
    struct DragGestureSession;
    std::shared_ptr<DragGestureSession> dragGestureSession;

    DraggableButton draggableLowButton, draggablePeakButton, draggableHighButton;
    std::array<DraggableButton, fire::eq::maxNodes - 3> extraNodes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterControl)
};
