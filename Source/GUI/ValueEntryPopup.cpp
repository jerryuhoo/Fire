#include "ValueEntryPopup.h"
#include "InterfaceDefines.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace
{
bool parseStrictFiniteDouble(const juce::String& text, double& value) noexcept
{
    const auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return false;

    const auto first = trimmed.toRawUTF8();
    char* last = nullptr;
    errno = 0;
    const auto parsed = std::strtod(first, &last);
    if (last == first || last == nullptr || *last != '\0'
        || errno == ERANGE || ! std::isfinite(parsed))
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
                        fire::ui::Metrics::radius);
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
    if (isVisible())
    {
        if (! completingSession)
        {
            resetTransientState();
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

    double value = 0.0;
    if (! parseStrictFiniteDouble(editor.getText(), value))
    {
        setInputError(true);
        editor.selectAll();
        if (editor.isShowing())
            editor.grabKeyboardFocus();
        return false;
    }

    setInputError(false);
    auto callback = onOk;

    completingSession = true;
    sessionActive = false;
    setVisible(false);
    resetTransientState();
    completingSession = false;

    if (callback)
        callback(value);

    return true;
}

void ValueEntryPopup::cancelSession()
{
    if (! sessionActive || completingSession)
        return;

    auto callback = onCancel;

    completingSession = true;
    sessionActive = false;
    setVisible(false);
    resetTransientState();
    completingSession = false;

    if (callback)
        callback();
}

void ValueEntryPopup::resetTransientState()
{
    okButton.dismissPointerGesture();
    cancelButton.dismissPointerGesture();
    editor.setText({}, juce::dontSendNotification);
    setInputError(false);
}

void ValueEntryPopup::cancelIfFocusLeftPopup()
{
    if (! hasKeyboardFocus(true))
        cancelSession();
}

void ValueEntryPopup::setInputError(bool shouldShowError)
{
    editor.setColour(juce::TextEditor::outlineColourId,
                     shouldShowError ? fire::ui::colours::danger
                                     : fire::ui::colours::hairline);
    editor.setColour(juce::TextEditor::focusedOutlineColourId,
                     shouldShowError ? fire::ui::colours::danger
                                     : fire::ui::colours::ember);
    editor.repaint();
}

void ValueEntryPopup::PrimaryTextButton::mouseDown(const juce::MouseEvent& event)
{
    if (primaryPointerDown && ! isPointerSource(event))
        return;

    dismissPointerGesture();
    primaryPointerDown = event.mods.isLeftButtonDown()
                         && ! event.mods.isRightButtonDown()
                         && ! event.mods.isMiddleButtonDown()
                         && ! event.mods.isPopupMenu();

    if (! primaryPointerDown)
        return;

    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();
    juce::TextButton::mouseDown(event);
}

void ValueEntryPopup::PrimaryTextButton::mouseDrag(const juce::MouseEvent& event)
{
    if (primaryPointerDown && isPointerSource(event))
        juce::TextButton::mouseDrag(event);
}

void ValueEntryPopup::PrimaryTextButton::mouseUp(const juce::MouseEvent& event)
{
    if (! primaryPointerDown)
    {
        dismissPointerGesture();
        return;
    }

    if (! isPointerSource(event))
        return;

    primaryPointerDown = false;
    pointerSourceIndex = -1;
    juce::TextButton::mouseUp(event);
}

bool ValueEntryPopup::PrimaryTextButton::keyPressed(
    const juce::KeyPress& key)
{
    if (isEnabled() && key.isKeyCode(juce::KeyPress::returnKey))
    {
        // Button::keyPressed queues triggerClick(). Invoke the normal callback
        // synchronously so a key from session A cannot commit session B.
        internalClickCallback(key.getModifiers());
        return true;
    }

    return juce::TextButton::keyPressed(key);
}

void ValueEntryPopup::PrimaryTextButton::visibilityChanged()
{
    juce::TextButton::visibilityChanged();

    if (! isVisible())
        dismissPointerGesture();
}

void ValueEntryPopup::PrimaryTextButton::enablementChanged()
{
    juce::TextButton::enablementChanged();

    if (! isEnabled())
        dismissPointerGesture();
}

void ValueEntryPopup::PrimaryTextButton::dismissPointerGesture() noexcept
{
    primaryPointerDown = false;
    pointerSourceIndex = -1;

    if (isDown())
        setState(juce::Button::buttonNormal);
}

bool ValueEntryPopup::PrimaryTextButton::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
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
