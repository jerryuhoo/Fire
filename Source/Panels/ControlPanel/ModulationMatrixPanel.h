/*
  ==============================================================================

    ModulationMatrixPanel.h
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "../../PluginProcessor.h"
#include "../../Utility/Parameters.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <cstdint>
#include <memory>

class ModulationMatrixPrimaryButton final : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void visibilityChanged() override;
    void enablementChanged() override;

private:
    enum class PointerGesture
    {
        none,
        rejected,
        primary
    };

    void cancelPointerGesture() noexcept;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
};

//
//  A header component to display titles for the matrix columns.
//
class ModulationMatrixHeader : public juce::Component
{
public:
    ModulationMatrixHeader();
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label sourceLabel;
    juce::Label amountLabel;
    juce::Label polarityLabel;
    juce::Label bypassLabel;
    juce::Label destinationLabel;
};

struct ModulationRoutingEditSession
{
    std::uint64_t revision = 0;
};

//
//  A single row in our modulation matrix UI.
//
class ModulationMatrixRow : public juce::Component,
                            public juce::Button::Listener,
                            public juce::Slider::Listener,
                            public juce::ComboBox::Listener
{
public:
    // The constructor now accepts a callback function to handle its deletion.
    ModulationMatrixRow(FireAudioProcessor& p,
                        int routingIndex,
                        const ModulationRouting& routing,
                        std::shared_ptr<ModulationRoutingEditSession> editSession,
                        std::function<void(std::uint64_t,
                                           ModulationRouting)> onDelete);
    ~ModulationMatrixRow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class PrimaryButtonSlider final : public juce::Slider
    {
    public:
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        bool primaryGestureInProgress = false;
    };

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    bool isParentRebuildPending();
    void requestParentRebuild();

    FireAudioProcessor& processor;
    FireLookAndFeel fireLookAndFeel;
    int index; // The index of the routing this row represents in the processor's array
    ModulationRouting expectedRouting;
    std::shared_ptr<ModulationRoutingEditSession> routingEditSession;
    std::function<void(std::uint64_t, ModulationRouting)>
        onDeleteCallback;

    juce::ComboBox sourceMenu;
    PrimaryButtonSlider amountSlider;
    ModulationMatrixPrimaryButton bipolarButton;
    ModulationMatrixPrimaryButton bypassButton;
    juce::ComboBox destinationMenu;
    ModulationMatrixPrimaryButton removeButton;

    std::vector<ModulationTarget> allPossibleTargets;
};

//
//  The main panel that holds all the modulation routing rows.
//
class ModulationMatrixPanel : public juce::Component,
                              public juce::Button::Listener,
                              private juce::ChangeListener,
                              private juce::AsyncUpdater
{
public:
    ModulationMatrixPanel(FireAudioProcessor& p);
    ~ModulationMatrixPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Rebuilds the UI from the processor's data model
    void buildUiFromProcessorState();
    void requestUiRebuild();
    bool isUiRebuildPending() const noexcept;

private:
    void buttonClicked(juce::Button* button) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void handleAsyncUpdate() override;
    FireAudioProcessor& processor;
    FireLookAndFeel fireLookAndFeel;

    ModulationMatrixHeader header;
    std::vector<std::unique_ptr<ModulationMatrixRow>> rows;
    ModulationMatrixPrimaryButton addButton { "+" };
    ModulationMatrixPrimaryButton closeButton { "Close" };

    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::Rectangle<int> titleArea;
    std::shared_ptr<ModulationRoutingEditSession> routingEditSession;
};
