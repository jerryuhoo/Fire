/*
  ==============================================================================

    ContextAwareComboBox.h

  ==============================================================================
*/

#pragma once

#include "FocusAwareComboBox.h"
#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdint>
#include <functional>

struct ContextAwareComboBoxTestAccess;
struct GraphViewSelectorTestAccess;

//==============================================================================
class ContextAwareComboBox final : public FocusAwareComboBox
{
public:
    using GenerationProvider = std::function<std::uint64_t()>;
    using ContextValidator = std::function<bool()>;
    using SelectionCallback = std::function<void(int)>;

    ContextAwareComboBox() = default;

    void configurePopupSession(GenerationProvider generationProvider,
                               ContextValidator contextValidator,
                               juce::RangedAudioParameter* parameter);
    // UI-only choices use the same lifecycle protection without creating a
    // dummy audio parameter or notifying the host of a presentation change.
    void configurePopupSession(GenerationProvider generationProvider,
                               ContextValidator contextValidator,
                               SelectionCallback selectionCallback);
    void dismissTransientInteraction() noexcept;
    void showPopup() override;

private:
    friend struct ContextAwareComboBoxTestAccess;
    friend struct GraphViewSelectorTestAccess;
    friend struct ChordResonatorUiTestAccess;

    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;
    std::function<void(int)> createPopupResultHandler();
    std::function<void(int)> createPopupResultHandler(
        std::uint64_t contextGeneration);
    void capturePopupRequest() noexcept;
    bool isContextCurrent(std::uint64_t contextGeneration) const;
    bool isCompletePrimaryDown(const juce::MouseEvent& event) const noexcept;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    void recoverMissingPointerUp(const juce::MouseEvent& event);
    void releasePointerInteractionWithoutSelection(
        const juce::MouseEvent& event);
    void clearPointerInteraction() noexcept;
    void closePopupWindow() noexcept;

    GenerationProvider getCurrentGeneration;
    ContextValidator isPopupContextValid;
    juce::RangedAudioParameter* boundParameter = nullptr;
    SelectionCallback commitSelection;
    std::uint64_t popupRequestGeneration = 0;
    std::uint64_t popupSessionRevision = 0;
    std::uint64_t pointerInteractionGeneration = 0;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    bool popupRequestArmed = false;
    bool popupSessionActive = false;
    bool pointerInteractionActive = false;
    bool cancelPendingPointerRelease = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContextAwareComboBox)
};
