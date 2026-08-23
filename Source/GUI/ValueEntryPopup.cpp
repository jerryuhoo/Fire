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
    // ==================================================================
    // 1. Configure TextEditor colors
    // ==================================================================
    addAndMakeVisible(editor);
    editor.setTextToShowWhenEmpty("Enter value...", fire::ui::colours::textMuted);
    editor.setJustification(juce::Justification::centred);
    editor.addListener(this); // Listen for the return key

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
        if (onCancel)
            onCancel();
    };

    setSize(160, 60);
}

ValueEntryPopup::~ValueEntryPopup()
{
    editor.removeListener(this);
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

void ValueEntryPopup::textEditorReturnKeyPressed(juce::TextEditor&)
{
    okButton.triggerClick(); // Trigger the "OK" button's click action
}

void ValueEntryPopup::textEditorTextChanged(juce::TextEditor&)
{
    setInputError(false);
}

bool ValueEntryPopup::submitEditorText()
{
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
    if (onOk)
    {
        auto callback = onOk;
        callback(value);
    }
    return true;
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
