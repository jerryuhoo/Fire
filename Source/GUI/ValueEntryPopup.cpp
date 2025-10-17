#include "ValueEntryPopup.h"
#include "InterfaceDefines.h"

ValueEntryPopup::ValueEntryPopup()
{
    // ==================================================================
    // 1. Configure TextEditor colors
    // ==================================================================
    addAndMakeVisible(editor);
    editor.setTextToShowWhenEmpty("Enter value...", COLOUR6.withAlpha(0.4f));
    editor.setJustification(juce::Justification::centred);
    editor.addListener(this); // Listen for the return key

    // Set colors for the text editor
    editor.setColour(juce::TextEditor::backgroundColourId, COLOUR7);
    editor.setColour(juce::TextEditor::textColourId, COLOUR0);
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, COLOUR7);

    // ==================================================================
    // 2. Configure "OK" button colors
    // ==================================================================
    addAndMakeVisible(okButton);
    okButton.setButtonText("OK");
    okButton.setColour(juce::TextButton::buttonColourId, COLOUR6.brighter(0.2f));
    okButton.setColour(juce::TextButton::buttonOnColourId, COLOUR1);
    okButton.setColour(juce::TextButton::textColourOffId, COLOUR0);
    okButton.setColour(juce::TextButton::textColourOnId, COLOUR7);
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
    cancelButton.setColour(juce::TextButton::buttonColourId, COLOUR6.brighter(0.2f));
    cancelButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    cancelButton.setColour(juce::TextButton::textColourOffId, COLOUR0.withAlpha(0.8f));
    cancelButton.setColour(juce::TextButton::textColourOnId, COLOUR0);
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
    // Use COLOUR6 for the dark background
    g.fillAll(COLOUR6);

    // Use COLOUR1 for the bright, highlighted border
    g.setColour(COLOUR1);
    g.drawRect(getLocalBounds(), 1.5f);
}

void ValueEntryPopup::textEditorReturnKeyPressed(juce::TextEditor&)
{
    okButton.triggerClick(); // Trigger the "OK" button's click action
}
