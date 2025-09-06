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

    std::vector<juce::String> paramsArray1 = { ParameterIDAndName::getName(MODE_NAME, 0), ParameterIDAndName::getName(LINKED_NAME, 0), ParameterIDAndName::getName(SAFE_NAME, 0), ParameterIDAndName::getName(DRIVE_NAME, 0), ParameterIDAndName::getName(COMP_RATIO_NAME, 0), ParameterIDAndName::getName(COMP_THRESH_NAME, 0), ParameterIDAndName::getName(WIDTH_NAME, 0), ParameterIDAndName::getName(OUTPUT_NAME, 0), ParameterIDAndName::getName(MIX_NAME, 0), ParameterIDAndName::getName(BIAS_NAME, 0), ParameterIDAndName::getName(REC_NAME, 0), ParameterIDAndName::getName(COMP_BYPASS_NAME, 0), ParameterIDAndName::getName(WIDTH_BYPASS_NAME, 0) };
    std::vector<juce::String> paramsArray2 = { ParameterIDAndName::getName(MODE_NAME, 1), ParameterIDAndName::getName(LINKED_NAME, 1), ParameterIDAndName::getName(SAFE_NAME, 1), ParameterIDAndName::getName(DRIVE_NAME, 1), ParameterIDAndName::getName(COMP_RATIO_NAME, 1), ParameterIDAndName::getName(COMP_THRESH_NAME, 1), ParameterIDAndName::getName(WIDTH_NAME, 1), ParameterIDAndName::getName(OUTPUT_NAME, 1), ParameterIDAndName::getName(MIX_NAME, 1), ParameterIDAndName::getName(BIAS_NAME, 1), ParameterIDAndName::getName(REC_NAME, 1), ParameterIDAndName::getName(COMP_BYPASS_NAME, 1), ParameterIDAndName::getName(WIDTH_BYPASS_NAME, 1) };
    std::vector<juce::String> paramsArray3 = { ParameterIDAndName::getName(MODE_NAME, 2), ParameterIDAndName::getName(LINKED_NAME, 2), ParameterIDAndName::getName(SAFE_NAME, 2), ParameterIDAndName::getName(DRIVE_NAME, 2), ParameterIDAndName::getName(COMP_RATIO_NAME, 2), ParameterIDAndName::getName(COMP_THRESH_NAME, 2), ParameterIDAndName::getName(WIDTH_NAME, 2), ParameterIDAndName::getName(OUTPUT_NAME, 2), ParameterIDAndName::getName(MIX_NAME, 2), ParameterIDAndName::getName(BIAS_NAME, 2), ParameterIDAndName::getName(REC_NAME, 2), ParameterIDAndName::getName(COMP_BYPASS_NAME, 2), ParameterIDAndName::getName(WIDTH_BYPASS_NAME, 2) };
    std::vector<juce::String> paramsArray4 = { ParameterIDAndName::getName(MODE_NAME, 3), ParameterIDAndName::getName(LINKED_NAME, 3), ParameterIDAndName::getName(SAFE_NAME, 3), ParameterIDAndName::getName(DRIVE_NAME, 3), ParameterIDAndName::getName(COMP_RATIO_NAME, 3), ParameterIDAndName::getName(COMP_THRESH_NAME, 3), ParameterIDAndName::getName(WIDTH_NAME, 3), ParameterIDAndName::getName(OUTPUT_NAME, 3), ParameterIDAndName::getName(MIX_NAME, 3), ParameterIDAndName::getName(BIAS_NAME, 3), ParameterIDAndName::getName(REC_NAME, 3), ParameterIDAndName::getName(COMP_BYPASS_NAME, 3), ParameterIDAndName::getName(WIDTH_BYPASS_NAME, 3) };
    bool isParamInArray(juce::String paramName, std::vector<juce::String> paramArray);
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

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> multiEnableAttachment1, multiEnableAttachment2, multiEnableAttachment3, multiEnableAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> multiSoloAttachment1, multiSoloAttachment2, multiSoloAttachment3, multiSoloAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freqDividerGroupAttachment1, freqDividerGroupAttachment2, freqDividerGroupAttachment3;

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
