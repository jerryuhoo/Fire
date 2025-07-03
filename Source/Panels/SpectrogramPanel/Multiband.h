/*
  ==============================================================================

    Multiband.h
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../PluginProcessor.h"
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
    void setSoloRelatedBounds();
    EnableButton& getEnableButton(int index);

    void setScale(float scale);
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

    std::vector<juce::String> paramsArray1 = { MODE_NAME1, LINKED_NAME1, SAFE_NAME1, DRIVE_NAME1, COMP_RATIO_NAME1, COMP_THRESH_NAME1, WIDTH_NAME1, OUTPUT_NAME1, MIX_NAME1, BIAS_NAME1, REC_NAME1, COMP_BYPASS_NAME1, WIDTH_BYPASS_NAME1 };
    std::vector<juce::String> paramsArray2 = { MODE_NAME2, LINKED_NAME2, SAFE_NAME2, DRIVE_NAME2, COMP_RATIO_NAME2, COMP_THRESH_NAME2, WIDTH_NAME2, OUTPUT_NAME2, MIX_NAME2, BIAS_NAME2, REC_NAME2, COMP_BYPASS_NAME2, WIDTH_BYPASS_NAME2 };
    std::vector<juce::String> paramsArray3 = { MODE_NAME3, LINKED_NAME3, SAFE_NAME3, DRIVE_NAME3, COMP_RATIO_NAME3, COMP_THRESH_NAME3, WIDTH_NAME3, OUTPUT_NAME3, MIX_NAME3, BIAS_NAME3, REC_NAME3, COMP_BYPASS_NAME3, WIDTH_BYPASS_NAME3 };
    std::vector<juce::String> paramsArray4 = { MODE_NAME4, LINKED_NAME4, SAFE_NAME4, DRIVE_NAME4, COMP_RATIO_NAME4, COMP_THRESH_NAME4, WIDTH_NAME4, OUTPUT_NAME4, MIX_NAME4, BIAS_NAME4, REC_NAME4, COMP_BYPASS_NAME4, WIDTH_BYPASS_NAME4 };
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
