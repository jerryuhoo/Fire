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

    void dismissSession();

    void resized() override;
    void paint(juce::Graphics& g) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusLost(FocusChangeType cause) override;
    void focusOfChildComponentChanged(FocusChangeType cause) override;
    void visibilityChanged() override;
    void mouseDown(const juce::MouseEvent& event) override;

    void textEditorTextChanged(juce::TextEditor&) override;

    std::function<void(double)> onOk;
    std::function<void()> onCancel;

private:
    friend struct ValueEntryPopupTestAccess;

    class SessionTextEditor final : public juce::TextEditor
    {
    public:
        std::function<void()> onReturnPressedSynchronously;
        std::function<void()> onEscapePressedSynchronously;

    private:
        bool keyPressed(const juce::KeyPress& key) override;
    };

    class PrimaryTextButton final : public juce::TextButton
    {
    public:
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        bool keyPressed(const juce::KeyPress& key) override;
        void dismissPointerGesture() noexcept;

    private:
        void visibilityChanged() override;
        void enablementChanged() override;
        bool isPointerSource(const juce::MouseEvent& event) const noexcept;

        bool primaryPointerDown = false;
        juce::MouseInputSource::InputSourceType pointerSourceType =
            juce::MouseInputSource::mouse;
        int pointerSourceIndex = -1;
    };

    bool submitEditorText();
    void cancelSession();
    void resetTransientState();
    void cancelIfFocusLeftPopup();
    void setInputError(bool shouldShowError);

    SessionTextEditor editor;
    PrimaryTextButton okButton;
    PrimaryTextButton cancelButton;
    bool sessionActive = false;
    bool completingSession = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValueEntryPopup)
};
