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
#include <array>
#include <functional>
#include <memory>
#include <vector>

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct MultibandPointerTestAccess;
struct MultibandTopologyAnimationTestAccess;
#endif
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
    bool keyPressed(const juce::KeyPress& key) override;

    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override;

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
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct MultibandPointerTestAccess;
    friend struct MultibandTopologyAnimationTestAccess;
#endif

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
    bool primaryDragActive = false;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    int activePointerDividerIndex = -1;
    bool isCanonicalisingLines = false;
    bool isPublishingCrossoverCascade = false;
    int hoveredBandIndex = -1;
    FocusChangedCallback focusChangedCallback;

    struct DividerVisualState
    {
        float xPercent = 0.0f;
        float opacity = 1.0f;
    };

    struct DividerVisualSnapshot
    {
        int lineCount = 0;
        std::array<DividerVisualState, 3> dividers {};
    };

    struct RetiringDividerVisual
    {
        float xPercent = 0.0f;
        fire::ui::DampedValue opacity;
    };

    void setLineIndex();
    int sortLinesInternal(bool notifyFocusChange);
    void applyAuthoritativeBandCount(int requestedBandCount,
                                     bool forceFocusNotification,
                                     bool publishCanonicalParameters,
                                     const DividerVisualSnapshot* previousVisuals = nullptr);
    std::array<float, 3> getCanonicalCrossoverFrequencies(int requestedBandCount) const;
    void setDividerState(int dividerIndex,
                         bool enabled,
                         float frequency,
                         juce::NotificationType parameterNotification);
    bool addBandAtX(float localX);
    bool deleteBandAtIndex(int bandIndex);
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
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    bool admitDividerPointerGesture(
        int dividerIndex,
        juce::MouseInputSource::InputSourceType sourceType,
        int sourceIndex);
    void clearPrimaryPointerState() noexcept;
    void dismissTrackedDividerGesture(int dividerIndex);
    void recoverMissingPointerUp(const juce::MouseEvent& event);
    void handleDividerHidden(int dividerIndex);
    void updateHoveredBand(juce::Point<int> localPosition, bool pointerIsInside);
    void refreshHoveredBandFromMouse();
    void updateCloseButtonVisibility();
    DividerVisualSnapshot captureDividerVisuals() const;
    void reconcileDividerTopologyVisuals(const DividerVisualSnapshot& previous,
                                         int newLineCount,
                                         bool animate);
    float takeRetiringDividerOpacity(float xPercent) noexcept;
    void addRetiringDividerVisual(float xPercent, float opacity);
    void clearRetiringDividerVisuals();
    void paintRetiringDividerVisuals(juce::Graphics& g) const;

    struct CrossoverGestureSession;
    void beginCrossoverGesture();
    VerticalLine::ParameterGestureToken touchCrossoverParameter(int dividerIndex);
    void endCrossoverGesture();
    std::array<juce::RangedAudioParameter*, 3> crossoverParameters {};
    std::shared_ptr<CrossoverGestureSession> crossoverGestureSession;
    int crossoverGestureDepth = 0;

    std::unique_ptr<FreqDividerGroup> freqDividerGroup[3];
    std::vector<RetiringDividerVisual> retiringDividerVisuals;

    // Use vectors to manage attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> multiEnableAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> multiSoloAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> freqDividerGroupAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Multiband)
};
