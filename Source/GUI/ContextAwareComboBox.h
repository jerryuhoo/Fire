/*
  ==============================================================================

    ContextAwareComboBox.h

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdint>
#include <functional>

struct ContextAwareComboBoxTestAccess;

//==============================================================================
class ContextAwareComboBox final : public juce::ComboBox
{
public:
    using GenerationProvider = std::function<std::uint64_t()>;
    using ContextValidator = std::function<bool()>;

    ContextAwareComboBox() = default;

    void configurePopupSession(GenerationProvider generationProvider,
                               ContextValidator contextValidator,
                               juce::RangedAudioParameter* parameter);
    void dismissTransientInteraction() noexcept;
    void showPopup() override;

private:
    friend struct ContextAwareComboBoxTestAccess;

    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    std::function<void(int)> createPopupResultHandler();
    std::function<void(int)> createPopupResultHandler(
        std::uint64_t contextGeneration);
    void capturePopupRequest() noexcept;
    bool isContextCurrent(std::uint64_t contextGeneration) const;
    void closePopupWindow() noexcept;

    GenerationProvider getCurrentGeneration;
    ContextValidator isPopupContextValid;
    juce::RangedAudioParameter* boundParameter = nullptr;
    std::uint64_t popupRequestGeneration = 0;
    std::uint64_t popupSessionRevision = 0;
    std::uint64_t pointerInteractionGeneration = 0;
    bool popupRequestArmed = false;
    bool popupSessionActive = false;
    bool pointerInteractionActive = false;
    bool cancelPendingPointerRelease = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContextAwareComboBox)
};
