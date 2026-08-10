#include "ValueEntryPopup.h"
#include "InterfaceDefines.h"

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
        if (onOk)
            onOk(editor.getText().getDoubleValue());
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
