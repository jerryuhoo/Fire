/*
  ==============================================================================

    FocusAwareComboBox.h

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Keeps a ComboBox's keyboard focus indication separate from focus acquired
    by a pointer press. The last explicit input modality survives temporary
    popup focus transfers, while JUCE's menu-active state remains the single
    source of truth for popup highlighting.
*/
class FocusAwareComboBox : public juce::ComboBox
{
public:
    FocusAwareComboBox() = default;

    bool shouldShowInteractionFocus() const noexcept
    {
        return isPopupActive()
            || (lastInputWasKeyboard && hasKeyboardFocus(true));
    }

protected:
    void notePointerInteraction() noexcept
    {
        if (! lastInputWasKeyboard)
            return;

        lastInputWasKeyboard = false;
        repaint();
    }

    void noteKeyboardInteraction() noexcept
    {
        if (lastInputWasKeyboard)
            return;

        lastInputWasKeyboard = true;
        repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        noteKeyboardInteraction();

        // ComboBox keyboard input may synchronously notify code that deletes
        // the control, so the JUCE call remains the final operation.
        return juce::ComboBox::keyPressed(key);
    }

    void focusGained(FocusChangeType cause) override
    {
        const juce::Component::SafePointer<FocusAwareComboBox> safeThis(this);
        juce::ComboBox::focusGained(cause);
        if (safeThis == nullptr)
            return;

        if (cause == focusChangedByMouseClick)
            notePointerInteraction();
        else if (cause == focusChangedByTabKey)
            noteKeyboardInteraction();
    }

    void focusLost(FocusChangeType cause) override
    {
        // Popup menus may temporarily transfer focus and restore it with
        // focusChangedDirectly. Preserve the last explicit input modality so
        // that restoration cannot turn a pointer-opened menu keyboard-visible.
        juce::ComboBox::focusLost(cause);
    }

private:
    // A first programmatic/direct focus is keyboard-visible unless a real
    // pointer event has established pointer modality first.
    bool lastInputWasKeyboard = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FocusAwareComboBox)
};
