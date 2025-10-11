/*
  ==============================================================================

    ModulatableSlider.cpp
    Created: 2 Sep 2025 1:42:12am
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulatableSlider.h"

ModulatableSlider::ModulatableSlider()
{
    // Default values
    parameterID = "";
    lfoSource = 0;
    lfoAmount = 0.0;
    lfoValue = 0.0;
    isModHandleMouseOver = false;
    isModHandleMouseDown = false;
    isDraggingMainSlider = false;

    addAndMakeVisible(label);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(KNOB_FONT_SIZE).withStyle("Plain") });
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}

bool ModulatableSlider::hitTest(int x, int y)
{
    auto sliderBounds = getLocalBounds();
    auto center = sliderBounds.getCentre().toFloat();
    auto distFromCentre = juce::Point<float>((float) x, (float) y).getDistanceFrom(center);

    auto outerBounds = sliderBounds.toFloat().reduced(10);
    auto outerRadius = juce::jmin(outerBounds.getWidth(), outerBounds.getHeight()) / 2.0f;
    float maxSliderClickableRadius = outerRadius + (outerRadius * 0.2f * 0.5f);

    if (distFromCentre <= maxSliderClickableRadius)
    {
        return true;
    }

    if (isModulated && getModulationHandleBounds().contains((float) x, (float) y))
    {
        return true;
    }

    return false;
}

juce::Rectangle<float> ModulatableSlider::getModulationHandleBounds()
{
    auto bounds = getLocalBounds().toFloat().reduced(10 * 1.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    float handleSize = radius * 0.4f;

    float handleCenterX = bounds.getCentreX() + (radius * 0.85f);
    float handleCenterY = bounds.getCentreY() + (radius * 0.85f);

    return juce::Rectangle<float>(handleSize, handleSize).withCentre({ handleCenterX, handleCenterY });
}

// New helper function
bool ModulatableSlider::isMouseOverMainSlider() const
{
    // The main slider is highlighted only if the mouse is over the component
    // in general, but NOT specifically over the modulation handle.
    return isMouseOver() && ! isModHandleMouseOver;
}

// New/updated mouse handlers
void ModulatableSlider::mouseMove(const juce::MouseEvent& event)
{
    bool isOverHandleNow = isModulated && getModulationHandleBounds().contains(event.getPosition().toFloat());

    if (isOverHandleNow != isModHandleMouseOver)
    {
        isModHandleMouseOver = isOverHandleNow;
        if (isModHandleMouseOver)
        {
            if (onHoverStart)
                onHoverStart(this);
        }
        else
        {
            if (onHoverEnd)
                onHoverEnd(this);
        }
        repaint();
    }

    // We still call the base class for its own internal state management
    juce::Slider::mouseMove(event);
}

void ModulatableSlider::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter(event);
    mouseMove(event);
    label.setVisible(false);
    setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
}

void ModulatableSlider::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit(event);
    if (isModHandleMouseOver)
    {
        isModHandleMouseOver = false;
        if (onHoverEnd)
            onHoverEnd(this);
        repaint();
    }

    // Get the global screen position of the mouse cursor
    auto screenPosition = event.originalComponent->localPointToGlobal(event.getPosition());

    // Find what component is now at that screen position
    auto* componentUnderMouse = juce::Desktop::getInstance().findComponentAt(screenPosition);

    // If the component under the mouse is NOT a child of this slider,
    // it means the mouse has truly left our slider and its textbox.
    if (! isParentOf(componentUnderMouse))
    {
        label.setVisible(true);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }
}

void ModulatableSlider::mouseDoubleClick(const juce::MouseEvent& event)
{
    // Check if the double-click is on the modulation handle
    if (isModulated && getModulationHandleBounds().contains(event.getPosition().toFloat()))
    {
        if (onModulationReset)
        {
            onModulationReset();
        }

        repaint();
    }
    else
    {
        // Otherwise, perform the default slider action (resetting the slider's value)
        juce::Slider::mouseDoubleClick(event);
    }
}

void ModulatableSlider::mouseDown(const juce::MouseEvent& event)
{
    if (onClickInAssignMode)
    {
        onClickInAssignMode(parameterID);
        return;
    }
    // Check for Cmd+Click (macOS) or Ctrl+Click (Windows) on the handle
    if (isModulated && (event.mods.isCommandDown() || event.mods.isCtrlDown()) && getModulationHandleBounds().contains(event.getPosition().toFloat()))
    {
        // If the modifier is pressed on the handle, toggle the bipolar mode
        if (onBipolarModeToggled)
        {
            onBipolarModeToggled();
        }
        // We don't start a drag in this case
    }
    else if (isMouseOverMainSlider())
    {
        isDraggingMainSlider = true;
        // CRITICAL: Only call the base class mouseDown if we intend to start a drag on the main slider
        juce::Slider::mouseDown(event);
    }
    // Check if the click is on the modulation handle
    else if (isModulated && getModulationHandleBounds().contains(event.getPosition().toFloat()))
    {
        isModHandleMouseDown = true;
        initialLfoAmount = lfoAmount;

        if (onDragStart)
            onDragStart(this);

        repaint();
    }
}

void ModulatableSlider::mouseDrag(const juce::MouseEvent& event)
{
    // CRITICAL: Only forward the drag event to the base class if our flag is set
    if (isDraggingMainSlider)
    {
        juce::Slider::mouseDrag(event);
    }
    else if (isModHandleMouseDown)
    {
        auto diff = event.getPosition() - event.getMouseDownPosition();

        // Use the initial amount for a more stable drag interaction
        auto newAmount = initialLfoAmount - diff.y * 0.005;

        // Allow amount to be from -1.0 to 1.0
        lfoAmount = juce::jlimit(-1.0, 1.0, newAmount);

        // If the callback is set, call it to notify the processor
        if (onModAmountChanged)
        {
            onModAmountChanged(lfoAmount);
        }

        if (onDragMove)
            onDragMove(this);

        // This will update the UI
        repaint();
    }
}

void ModulatableSlider::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown() && isModulated && getModulationHandleBounds().contains(event.getPosition().toFloat()))
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Set Value");
        menu.addItem(2, "Clear LFO");
        menu.addItem(3, "Invert Depth");

        juce::String bipolarToggleText = isBipolar ? "Switch to Unipolar" : "Switch to Bipolar";
        menu.addItem(4, bipolarToggleText);

        juce::String bypassToggleText = isBypassed ? "Enable modulation" : "Bypass modulation";
        menu.addItem(5, bypassToggleText);

        // This is the outer lambda. We create 'safeThis' here.
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safeThis = juce::Component::SafePointer(this)](int result)
                           {
                               if (! safeThis)
                                   return;

                               switch (result)
                               {
                                   case 1: // Set Value
                                   {
                                       if (safeThis->onSetValueRequested)
                                           safeThis->onSetValueRequested(safeThis);
                                       break;
                                   }
                                   case 2: // Clear LFO
                                       if (safeThis->onModulationCleared)
                                           safeThis->onModulationCleared();
                                       break;
                                   case 3: // Invert Depth
                                       if (safeThis->onModulationInverted)
                                           safeThis->onModulationInverted();
                                       break;
                                   case 4: // Switch Bi/Uni Mode
                                       if (safeThis->onBipolarModeToggled)
                                           safeThis->onBipolarModeToggled();
                                       break;
                                   case 5: // Toggle Bypass
                                       if (safeThis->onBypassToggled)
                                           safeThis->onBypassToggled();
                                       break;
                                   default:
                                       break;
                               }
                           });
    }

    // Always call the base class mouseUp to ensure proper state cleanup
    juce::Slider::mouseUp(event);

    // Reset our custom flags regardless of where the mouseUp happened
    if (isDraggingMainSlider)
    {
        isDraggingMainSlider = false;
        repaint();
    }

    if (isModHandleMouseDown)
    {
        if (onDragEnd)
            onDragEnd(this);

        isModHandleMouseDown = false;
        repaint();
    }
}

void ModulatableSlider::setLabel(const juce::String& text, juce::Colour colour)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, colour);
}

void ModulatableSlider::resized()
{
    // First, call the base class's resized() to let it draw the slider itself.
    juce::Slider::resized();

    // Then, place our label in the area designated for the textbox.
    // This ensures it's perfectly aligned above the knob.
    label.setBounds(0, 0, getWidth(), TEXTBOX_HEIGHT);
}
