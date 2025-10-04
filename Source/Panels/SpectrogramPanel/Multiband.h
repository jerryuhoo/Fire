/*
  ==============================================================================

    Multiband.h
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../PluginProcessor.h"
#include "../../Utility/AudioHelpers.h"
#include "../TopPanel/Preset.h"
#include "CloseButton.h"
#include "EnableButton.h"
#include "FreqDividerGroup.h"
#include "SoloButton.h"
#include "SpectrumComponent.h"
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
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setCloseButtonState();
    void dragLines(float xPercent, int index);

    int getFocusIndex();
    void setFocusIndex(int index);
    void setSoloRelatedBounds();
    EnableButton& getEnableButton(int index);

    void setBandBypassStates(int index, bool state);
    state::StateComponent& getStateComponent();

    int sortLines();
    void setLineRelatedBoundsByX();
    void resortAndRedrawLines();

private:
    struct BandUIs
    {
        std::unique_ptr<SoloButton> soloButton;
        std::unique_ptr<EnableButton> enableButton;
        std::unique_ptr<CloseButton> closeButton;
        int id = 0;
    };
    std::vector<BandUIs> bandUIs;
    FireAudioProcessor& processor;
    state::StateComponent& stateComponent;
    float margin;
    float size = 15.0f;
    float width = 5.0f;
    // set vertical lines leftmost and rightmost percentage of the whole width
    const float limitLeft = 0.1f;
    const float limitRight = 1.0f - limitLeft;
    bool isMoving = false;

    // multi-band
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    int lineNum = 0;
    int focusIndex = 0;
    bool isDragging = false;

    // Use a 2D vector to store parameter arrays for each band
    std::vector<std::vector<juce::String>> paramsArrays;

    bool isParamInArray(juce::String paramName, const std::vector<juce::String>& paramArray);
    void setParametersToAFromB(int toIndex, int fromIndex);
    void initParameters(int bandindex);
    void setStatesWhenAdd(int changedIndex);
    void setStatesWhenDelete(int changedIndex);

    void setLineIndex();
    void updateLineNumAndSortedIndex(int option);

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

    bool shouldSetBlackMask(int index);
    int countLines();
    void setMasks(juce::Graphics& g, int index, int lineNumLimit, int x, int y, int width, int height, int mouseX, int mouseY);

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