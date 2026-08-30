/*
  ==============================================================================

    ModulationMatrixPanel.h
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "../../GUI/PrimaryButton.h"
#include "../../PluginProcessor.h"
#include "../../Utility/Parameters.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

struct ModulationMatrixRoutingComboBoxTestAccess;
struct ModulationMatrixRowTestAccess;

struct ModulationRoutingEditSession
{
    std::uint64_t revision = 0;
};

class ModulationMatrixRoutingComboBox final : public juce::ComboBox
{
public:
    ModulationMatrixRoutingComboBox() = default;

    struct EditContext
    {
        std::shared_ptr<ModulationRoutingEditSession> editSession;
        std::uint64_t revision = 0;
        ModulationRouting expectedRouting;
    };

    using EditContextProvider = std::function<EditContext()>;
    using EditContextValidator = std::function<bool(const EditContext&)>;
    using SelectionCommitter =
        std::function<void(ModulationMatrixRoutingComboBox&,
                           int,
                           const EditContext&)>;

    void configurePopupSession(EditContextProvider contextProvider,
                               EditContextValidator contextValidator,
                               SelectionCommitter selectionCommitter);
    void dismissTransientInteraction() noexcept;
    void showPopup() override;

private:
    friend struct ModulationMatrixRoutingComboBoxTestAccess;

    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;
    void visibilityChanged() override;
    void enablementChanged() override;
    void parentHierarchyChanged() override;

    std::function<void(int)> createPopupResultHandler();
    std::function<void(int)> createPopupResultHandler(
        EditContext context,
        std::uint64_t interactionGeneration);
    bool capturePopupRequest();
    bool isContextCurrent(const EditContext& context) const;
    bool isPopupContextCurrent(
        const EditContext& context,
        std::uint64_t interactionGeneration) const;
    bool commitKeyboardSelection(int itemId,
                                 const EditContext& context);
    bool isCompletePrimaryDown(const juce::MouseEvent& event) const noexcept;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    void recoverMissingPointerUp(const juce::MouseEvent& event);
    void releasePointerInteractionWithoutSelection(
        const juce::MouseEvent& event);
    void clearPointerInteraction() noexcept;
    void closePopupWindow() noexcept;

    EditContextProvider getCurrentEditContext;
    EditContextValidator isEditContextValid;
    SelectionCommitter commitSelection;
    EditContext popupRequestContext;
    std::uint64_t interactionGeneration = 0;
    std::uint64_t popupRequestInteractionGeneration = 0;
    std::uint64_t popupSessionRevision = 0;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    bool popupRequestArmed = false;
    bool popupSessionActive = false;
    bool pointerInteractionActive = false;
    bool cancelPendingPointerRelease = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        ModulationMatrixRoutingComboBox)
};

class ModulationMatrixPrimaryButton final : public PrimaryTextButton
{
public:
    using PrimaryTextButton::PrimaryTextButton;
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
    void visibilityChanged() override;
    void enablementChanged() override;
    void parentHierarchyChanged() override;
    void dismissTransientInteractions() noexcept;

private:
    friend struct ModulationMatrixRowTestAccess;

    class PrimaryButtonSlider final : public juce::Slider
    {
    public:
        ~PrimaryButtonSlider() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void visibilityChanged() override;
        void enablementChanged() override;
        void parentHierarchyChanged() override;
        void dismissTransientInteraction();

    private:
        friend struct ModulationMatrixRowTestAccess;

        bool isPointerSource(const juce::MouseEvent& event) const noexcept;
        void finishActivePointerGesture();

        juce::MouseInputSource::InputSourceType pointerSourceType =
            juce::MouseInputSource::mouse;
        int pointerSourceIndex = -1;
        std::optional<juce::MouseEvent> lastAcceptedPointerEvent;
        bool primaryGestureInProgress = false;
    };

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    ModulationMatrixRoutingComboBox::EditContext
        captureComboBoxEditContext() const;
    bool isComboBoxEditContextCurrent(
        const ModulationMatrixRoutingComboBox::EditContext& context);
    void commitComboBoxSelection(
        ModulationMatrixRoutingComboBox& comboBox,
        int selectedId,
        const ModulationMatrixRoutingComboBox::EditContext& context);
    bool isParentRebuildPending();
    void requestParentRebuild();

    FireAudioProcessor& processor;
    FireLookAndFeel fireLookAndFeel;
    int index; // The index of the routing this row represents in the processor's array
    ModulationRouting expectedRouting;
    std::shared_ptr<ModulationRoutingEditSession> routingEditSession;
    std::function<void(std::uint64_t, ModulationRouting)>
        onDeleteCallback;

    ModulationMatrixRoutingComboBox sourceMenu;
    PrimaryButtonSlider amountSlider;
    ModulationMatrixPrimaryButton bipolarButton;
    ModulationMatrixPrimaryButton bypassButton;
    ModulationMatrixRoutingComboBox destinationMenu;
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
    void visibilityChanged() override;
    void enablementChanged() override;

    // Rebuilds the UI from the processor's data model
    void buildUiFromProcessorState();
    void requestUiRebuild();
    bool isUiRebuildPending() const noexcept;

private:
    void dismissTransientInteractions() noexcept;
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
