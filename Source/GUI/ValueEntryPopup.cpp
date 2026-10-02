#include "ValueEntryPopup.h"
#include "InterfaceDefines.h"
#include "Skin.h"
#include "../Utility/StrictNumberParser.h"
#include <cmath>

namespace
{
bool parseStrictFiniteDouble(const juce::String& text, double& value) noexcept
{
    double parsed = 0.0;
    if (! fire::utility::parseStrictFiniteDouble(text, parsed)
        || ! std::isfinite(static_cast<float>(parsed)))
        return false;

    value = parsed;
    return true;
}
} // namespace

ValueEntryPopup::ValueEntryPopup()
{
    juce::Desktop::getInstance().addGlobalMouseListener(this);

    // ==================================================================
    // 1. Configure TextEditor colors
    // ==================================================================
    addAndMakeVisible(editor);
    editor.setTextToShowWhenEmpty("Enter value...", fire::ui::colours::textMuted);
    editor.setJustification(juce::Justification::centred);
    editor.addListener(this);
    editor.onReturnPressedSynchronously = [this] { submitEditorText(); };
    editor.onEscapePressedSynchronously = [this] { cancelSession(); };

    // Set colors for the text editor
    editor.setColour(juce::TextEditor::backgroundColourId, fire::ui::colours::surface0);
    editor.setColour(juce::TextEditor::textColourId, fire::ui::colours::textPrimary);
    editor.setColour(juce::TextEditor::outlineColourId, fire::ui::colours::hairline);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, fire::ui::colours::ember);

    // ==================================================================
    // 2. Configure "OK" button colors
    // ==================================================================
    addAndMakeVisible(okButton);
    okButton.setButtonText("OK");
    okButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface1);
    okButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::surface2);
    okButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::positive);
    okButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::whiteHot);
    okButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    okButton.onClick = [this]
    {
        submitEditorText();
    };

    // ==================================================================
    // 3. Configure "Cancel" button colors (using red as a secondary/alert color)
    // ==================================================================
    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText("Cancel");
    cancelButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface1);
    cancelButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::surface2);
    cancelButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    cancelButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::danger);
    cancelButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    cancelButton.onClick = [this]
    {
        cancelSession();
    };

    setSize(160, 60);
    setVisible(false);
}

ValueEntryPopup::~ValueEntryPopup()
{
    juce::Desktop::getInstance().removeGlobalMouseListener(this);
    editor.removeListener(this);
}

void ValueEntryPopup::dismissSession()
{
    cancelSession();
}

void ValueEntryPopup::resized()
{
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::column;
    fb.items.add(juce::FlexItem(editor).withFlex(1.0f));

    juce::FlexBox buttonBox;
    buttonBox.flexDirection = juce::FlexBox::Direction::row;
    buttonBox.items.add(juce::FlexItem(okButton).withFlex(1.0f));
    buttonBox.items.add(juce::FlexItem().withWidth(5.0f)); // Spacer between buttons
    buttonBox.items.add(juce::FlexItem(cancelButton).withFlex(1.0f));

    fb.items.add(juce::FlexItem(buttonBox).withFlex(1.0f).withMargin(juce::FlexItem::Margin(5.0f, 0, 0, 0)));

    fb.performLayout(getLocalBounds().reduced(8));
}

// ==================================================================
// 4. Redesigned paint method
// ==================================================================
void ValueEntryPopup::paint(juce::Graphics& g)
{
    fire::ui::drawPanel(g, getLocalBounds().toFloat(), fire::ui::colours::ember, true,
                        fire::ui::skinFor(*this));
}

bool ValueEntryPopup::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        cancelSession();
        return true;
    }

    return juce::Component::keyPressed(key);
}

void ValueEntryPopup::focusLost(FocusChangeType)
{
    cancelIfFocusLeftPopup();
}

void ValueEntryPopup::focusOfChildComponentChanged(FocusChangeType)
{
    cancelIfFocusLeftPopup();
}

void ValueEntryPopup::visibilityChanged()
{
    const juce::Component::SafePointer<ValueEntryPopup> safeThis(this);

    if (isVisible())
    {
        if (! completingSession)
        {
            resetTransientState();

            if (safeThis == nullptr || ! isVisible())
                return;

            sessionActive = true;
        }

        return;
    }

    if (sessionActive && ! completingSession)
        cancelSession();
}

void ValueEntryPopup::mouseDown(const juce::MouseEvent& event)
{
    if (! sessionActive || ! isVisible())
        return;

    auto* clickedComponent = event.originalComponent;
    if (clickedComponent != this
        && (clickedComponent == nullptr || ! isParentOf(clickedComponent)))
        cancelSession();
}

void ValueEntryPopup::textEditorTextChanged(juce::TextEditor&)
{
    setInputError(false);
}

bool ValueEntryPopup::submitEditorText()
{
    if (! sessionActive || completingSession)
        return false;

    const juce::Component::SafePointer<ValueEntryPopup> safeThis(this);

    double value = 0.0;
    if (! parseStrictFiniteDouble(editor.getText(), value))
    {
        setInputError(true);

        if (safeThis == nullptr)
            return false;

        editor.selectAll();

        if (safeThis == nullptr)
            return false;

        if (editor.isShowing())
            editor.grabKeyboardFocus();
        return false;
    }

    setInputError(false);

    if (safeThis == nullptr)
        return false;

    auto callback = onOk;

    completingSession = true;
    sessionActive = false;
    setVisible(false);

    if (safeThis == nullptr)
        return true;

    resetTransientState();

    if (safeThis == nullptr)
        return true;

    completingSession = false;

    if (callback)
        callback(value);

    return true;
}

void ValueEntryPopup::cancelSession()
{
    if (! sessionActive || completingSession)
        return;

    const juce::Component::SafePointer<ValueEntryPopup> safeThis(this);
    auto callback = onCancel;

    completingSession = true;
    sessionActive = false;
    setVisible(false);

    if (safeThis == nullptr)
        return;

    resetTransientState();

    if (safeThis == nullptr)
        return;

    completingSession = false;

    if (callback)
        callback();
}

void ValueEntryPopup::resetTransientState()
{
    const juce::Component::SafePointer<ValueEntryPopup> safeThis(this);

    okButton.dismissPointerGesture();

    if (safeThis == nullptr)
        return;

    cancelButton.dismissPointerGesture();

    if (safeThis == nullptr)
        return;

    editor.setText({}, juce::dontSendNotification);

    if (safeThis == nullptr)
        return;

    setInputError(false);
}

void ValueEntryPopup::cancelIfFocusLeftPopup()
{
    if (! hasKeyboardFocus(true))
        cancelSession();
}

void ValueEntryPopup::setInputError(bool shouldShowError)
{
    const juce::Component::SafePointer<ValueEntryPopup> safeThis(this);

    editor.setColour(juce::TextEditor::outlineColourId,
                     shouldShowError ? fire::ui::colours::danger
                                     : fire::ui::colours::hairline);

    if (safeThis == nullptr)
        return;

    editor.setColour(juce::TextEditor::focusedOutlineColourId,
                     shouldShowError ? fire::ui::colours::danger
                                     : fire::ui::colours::ember);

    if (safeThis == nullptr)
        return;

    editor.repaint();
}

void ValueEntryPopup::SessionTextButton::triggerClick()
{
    if (! isEnabled())
        return;

    // Accessibility presses use Button::triggerClick(), whose default command
    // is asynchronous. This popup is rebound between sessions, so invoke the
    // callback now and make it the final operation: it may delete the popup.
    internalClickCallback(juce::ModifierKeys::currentModifiers);
}

bool ValueEntryPopup::SessionTextEditor::keyPressed(
    const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        auto callback = onReturnPressedSynchronously;
        if (callback)
            callback();
        return true;
    }

    if (key.isKeyCode(juce::KeyPress::escapeKey))
    {
        auto callback = onEscapePressedSynchronously;
        if (callback)
            callback();
        return true;
    }

    return juce::TextEditor::keyPressed(key);
}
