/*
  ==============================================================================

    ValueEntryPopup.h
    Created: 8 Oct 2025 8:17:48pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>

struct ValueEntryPopupTestAccess;

//==============================================================================
class ValueEntryPopup : public juce::Component,
                        private juce::TextEditor::Listener
{
public:
    ValueEntryPopup();
    ~ValueEntryPopup() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorTextChanged(juce::TextEditor&) override;

    std::function<void(double)> onOk;
    std::function<void()> onCancel;

private:
    friend struct ValueEntryPopupTestAccess;

    bool submitEditorText();
    void setInputError(bool shouldShowError);

    juce::TextEditor editor;
    juce::TextButton okButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValueEntryPopup)
};
