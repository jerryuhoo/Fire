/*
  ==============================================================================

    Multiband.h
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../PluginProcessor.h"
#include "../../GUI/FireTheme.h"
#include "../../Utility/AudioHelpers.h"
#include "../TopPanel/Preset.h"
#include "CloseButton.h"
#include "EnableButton.h"
#include "FreqDividerGroup.h"
#include "SoloButton.h"
#include "SpectrumComponent.h"
#include <functional>
#include <vector>
//==============================================================================
/*
*/

class Multiband : public juce::Component, juce::Slider::Listener, juce::Button::Listener
{
public:
    Multiband(FireAudioProcessor&, state::StateComponent&);
    ~Multiband() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void animationTick(float deltaSeconds);
    void dismissTransientUi();
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void visibilityChanged() override;

    void dragLines(float xPercent, int index);

    using FocusChangedCallback = std::function<void(int)>;
    void setFocusChangedCallback(FocusChangedCallback callback);
    int getFocusIndex() const noexcept;
    void setFocusIndex(int index);
    void setSoloRelatedBounds();
    EnableButton& getEnableButton(int index);

    void setBandBypassStates(int index, bool state);
    state::StateComponent& getStateComponent();

    int sortLines();
    void setLineRelatedBoundsByX();
    void resortAndRedrawLines();
    void synchroniseBandCountFromParameter();

private:
    struct BandUIs
    {
        std::unique_ptr<SoloButton> soloButton;
        std::unique_ptr<EnableButton> enableButton;
        std::unique_ptr<CloseButton> closeButton;
    };
    std::vector<BandUIs> bandUIs;
    FireAudioProcessor& processor;
    state::StateComponent& stateComponent;
    float margin = 0.0f;
    float size = 15.0f;
    // set vertical lines leftmost and rightmost percentage of the whole width
    const float limitLeft = 0.1f;
    const float limitRight = 1.0f - limitLeft;

    // multi-band
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    int lineNum = 0;
    int focusIndex = 0;
    bool isDragging = false;
    bool isCanonicalisingLines = false;
    bool isPublishingCrossoverCascade = false;
    int hoveredBandIndex = -1;
    FocusChangedCallback focusChangedCallback;

    // Use a 2D vector to store parameter arrays for each band
    std::vector<std::vector<juce::String>> paramsArrays;

    void setParametersToAFromB(int toIndex, int fromIndex);
    void initParameters(int bandindex);
    void setStatesWhenAdd(int changedIndex, bool newBandIsOnLeft);
    void setStatesWhenDelete(int changedIndex);

    void setLineIndex();
    int sortLinesInternal(bool notifyFocusChange);
    void applyAuthoritativeBandCount(int requestedBandCount,
                                     bool forceFocusNotification,
                                     bool publishCanonicalParameters);
    std::array<float, 3> getCanonicalCrossoverFrequencies(int requestedBandCount) const;
    void setDividerState(int dividerIndex,
                         bool enabled,
                         float frequency,
                         juce::NotificationType parameterNotification);
    bool updateFocusIndex(int requestedIndex, bool forceNotification);
    void notifyFocusChanged();

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

    bool shouldSetBlackMask(int index);
    int countLines();
    void paintBandOverlay(juce::Graphics& g,
                          int index,
                          juce::Rectangle<float> area,
                          juce::Point<float> mousePosition);
    float getDividerX(int index) const;
    juce::Rectangle<float> getBandBounds(int index) const;
    int getBandIndexAtX(int x) const;
    int getDividerIndexForEvent(const juce::MouseEvent& event) const;
    bool isEventFromDividerGroup(const juce::MouseEvent& event) const;
    void updateHoveredBand(juce::Point<int> localPosition, bool pointerIsInside);
    void refreshHoveredBandFromMouse();
    void updateCloseButtonVisibility();

    void beginCrossoverGesture();
    void touchCrossoverParameter(int dividerIndex);
    void endCrossoverGesture();
    std::array<juce::RangedAudioParameter*, 3> crossoverParameters {};
    std::array<bool, 3> crossoverParametersTouched {};
    int crossoverGestureDepth = 0;

    std::unique_ptr<FreqDividerGroup> freqDividerGroup[3];

    // Use vectors to manage attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> multiEnableAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> multiSoloAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> freqDividerGroupAttachments;

    struct BandState
    {
        bool isEnabled;
        bool isSoloed;
    };
    BandState getBandState(int bandIndex);
    void setBandState(int bandIndex, BandState state, juce::NotificationType notification = juce::NotificationType::sendNotification);
    void copyBandSettings(int targetIndex, int sourceIndex);
    void resetBandToDefault(int bandIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Multiband)
};
