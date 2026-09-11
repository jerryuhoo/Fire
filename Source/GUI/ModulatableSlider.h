/*
  ==============================================================================

    ModulatableSlider.h
    Created: 2 Sep 2025 1:42:12am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "GuardedSliderAccessibility.h"
#include "InterfaceDefines.h"
#include "PrimaryButton.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <cstdint>
#include <optional>

struct ModulatableSliderTestAccess;
struct ModulatableSliderInteractionTestAccess;

//==============================================================================
/**
    A custom slider that can be modulated by an LFO.
*/
class ModulatableSlider : public juce::Slider,
                          public juce::Timer
{
public:
    enum class ModulationMenuCommand
    {
        setValue = 1,
        clearModulation,
        invertDepth,
        togglePolarity,
        toggleBypass
    };

    ModulatableSlider();
    ~ModulatableSlider() override;

    void paintOverChildren(juce::Graphics& g) override;
    bool hitTest(int x, int y) override;

    /** The idle painted badge geometry. Interaction feedback may enlarge this
        visually, but must never change the pointer target. */
    juce::Rectangle<float> getModulationHandleVisualBounds() const;
    /** The pointer target for every modulation-handle input path. */
    juce::Rectangle<float> getModulationHandleHitBounds() const;
    /** Stable value row below the dial, also used by the inline editor. */
    juce::Rectangle<int> getValueDisplayBounds() const;
    /** Kept for source compatibility; painting code should request the visual
        bounds and input code should request the hit bounds explicitly. */
    juce::Rectangle<float> getModulationHandleBounds() const
    {
        return getModulationHandleVisualBounds();
    }

    // LFO properties
    int lfoSource;
    double lfoAmount;
    double lfoValue; // Current LFO output (-1 to 1)

    // Mouse interaction states for the handle
    bool isModHandleMouseOver;
    bool isModHandleMouseDown;

    // Is the mouse over the main body of the slider, and not the handle?
    bool isMouseOverMainSlider() const;
    bool hasActiveInteraction() const noexcept
    {
        return activePointerGesture == PointerGesture::mainSlider
            || activePointerGesture == PointerGesture::modulationHandle;
    }
    void dismissTransientInteraction();
    /** Closes pointer, editor and hover state while retaining an asynchronous
        context-menu result for the parameter that originally opened it. This
        is only for a shared control's deliberate parameter rebind. */
    void dismissTransientInteractionPreservingContextMenu();
    bool advanceAnimation(float deltaSeconds) noexcept;
    float getHoverAnimation() const noexcept { return hoverAnimation; }
    float getPressAnimation() const noexcept { return pressAnimation; }
    float getFocusAnimation() const noexcept { return focusAnimation; }
    float getModulationHandleHoverAnimation() const noexcept
    {
        return modulationHandleHoverAnimation;
    }
    float getModulationHandlePressAnimation() const noexcept
    {
        return modulationHandlePressAnimation;
    }
    bool isModulated = false;
    bool isBipolar = true;
    bool isBypassed = false;
    juce::String parameterID;

    float assignModeGlowAlpha = 0.0f;

    // Callback to notify when the modulation amount changes via UI drag
    std::function<void(double)> onModAmountSetValue;
    std::function<void(ModulatableSlider*, const juce::String&)> onSetValueRequested;
    std::function<void(double)> onModAmountChanged;
    std::function<void(const juce::String&)> onBipolarModeToggled;
    std::function<void()> onModulationReset;
    std::function<void(const juce::String&)> onClickInAssignMode;
    std::function<void(int, const juce::String&)> onLfoAssignmentRequested;
    std::function<void(const juce::String&)> onModulationCleared;
    std::function<void(const juce::String&)> onModulationInverted;
    std::function<void(const juce::String&)> onBypassToggled;

    // Override mouse events to update handle states and control dragging
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType cause) override;
    void focusLost(FocusChangeType cause) override;

    std::function<void(ModulatableSlider*)> onModDragStart;
    std::function<void(ModulatableSlider*)> onModDragMove;
    std::function<void(ModulatableSlider*)> onModDragEnd;

    std::function<void(ModulatableSlider*)> onMainDragStart;
    std::function<void(ModulatableSlider*)> onMainDragMove;
    std::function<void(ModulatableSlider*)> onMainDragEnd;
    std::function<void(ModulatableSlider*)> onHoverStart;
    std::function<void(ModulatableSlider*)> onHoverEnd;

    // Used by a shared panel to apply a parameter rebind only after Slider's
    // own drag listeners have closed their current host gesture.
    std::function<void()> onInteractionEnded;
    // A pure value constraint for controls whose legal limits depend on a peer.
    std::function<double(double)> valueConstraint;

    const juce::String& getParamID() const { return parameterID; }
    double getLfoValue() const { return lfoValue; }

    // Add a public method to set up the label.
    void setLabel(const juce::String& text, juce::Colour colour);

    // Override component methods for layout and mouse events.
    void resized() override;
    void timerCallback() override;
    void visibilityChanged() override;
    void enablementChanged() override;

private:
    void valueChanged() override;
    friend struct ModulatableSliderTestAccess;
    friend struct ModulatableSliderInteractionTestAccess;

    enum class PointerGesture
    {
        none,
        rejected,
        popupMenu,
        assignment,
        mainSlider,
        modulationHandle
    };

    enum class PopupMenuTarget
    {
        none,
        mainSlider,
        modulationHandle
    };

    std::function<void(int)> createModulationMenuResultHandler(
        juce::String targetParameterIDAtOpen = {});
    std::function<void(int)> createLfoAssignmentMenuResultHandler(
        juce::String targetParameterIDAtOpen = {});
    void executeModulationMenuCommand(ModulationMenuCommand command,
                                      const juce::String& targetParameterID);

    float getUiScale() const noexcept;
    juce::Rectangle<float> getRotarySliderBounds() const;
    juce::Rectangle<int> getHeaderBounds() const;
    bool isCompletePrimaryDown(const juce::MouseEvent& event) const noexcept;
    static bool isStandalonePopupDown(
        const juce::MouseEvent& event) noexcept;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    bool recoverMissingPointerUp(const juce::MouseEvent& event);
    bool shouldShowKeyboardFocus() const noexcept;
    bool shouldSuppressAssignmentDoubleClick(
        const juce::MouseEvent& event) const noexcept;
    void clearAssignmentDoubleClickSuppression() noexcept;
    bool finishActivePointerGesture(const juce::MouseEvent& releaseEvent);
    void dismissTransientInteractionImpl(bool invalidateContextMenu);
    void beginPointerGesture(PointerGesture gesture,
                             const juce::MouseEvent& event);
    bool resetTransientPresentation();
    void lookAndFeelChanged() override;
    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override;
    void attachValueLabelPopupForwarder();
    void detachValueLabelPopupForwarder();

    class ValueLabelPopupForwarder final : public juce::MouseListener
    {
    public:
        explicit ValueLabelPopupForwarder(ModulatableSlider& ownerToUse)
            : owner(ownerToUse)
        {
        }

        void mouseDown(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void cancelGesture();

    private:
        ModulatableSlider& owner;
        bool ownsPopupGesture = false;
    };

    juce::Label label;
    ValueLabelPopupForwarder valueLabelPopupForwarder { *this };
    juce::Component::SafePointer<juce::Label> forwardedValueLabel;
    bool isDraggingMainSlider;
    PointerGesture activePointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    std::optional<juce::MouseEvent> lastAcceptedPointerEvent;
    PopupMenuTarget popupMenuTarget = PopupMenuTarget::none;
    juce::String popupTargetParameterID;
    bool suppressAssignmentDoubleClick = false;
    juce::MouseInputSource::InputSourceType assignmentSourceType =
        juce::MouseInputSource::mouse;
    int assignmentSourceIndex = -1;
    juce::int64 assignmentDoubleClickDeadlineMs = 0;
    std::uint64_t contextMenuRevision = 0;
    float hoverAnimation = 0.0f;
    float pressAnimation = 0.0f;
    float focusAnimation = 0.0f;
    float modulationHandleHoverAnimation = 0.0f;
    float modulationHandlePressAnimation = 0.0f;
    fire::ui::KeyboardFocusModalityState focusModality;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    std::uint64_t accessibilityHandlerCreationCountForTesting = 0;
#endif

    // Store the initial LFO amount when a drag starts for smoother interaction
    double initialLfoAmount = 0.0;
};
