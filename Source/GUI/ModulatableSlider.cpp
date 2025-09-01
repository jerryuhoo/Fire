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
    lfoSource = 0;
    lfoAmount = 0.4;
    lfoValue = 0.2;
    isModHandleMouseOver = false;
    isModHandleMouseDown = false;
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

    if (getModulationHandleBounds().contains((float) x, (float) y))
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
    bool isOverHandleNow = getModulationHandleBounds().contains(event.getPosition().toFloat());

    if (isOverHandleNow != isModHandleMouseOver)
    {
        isModHandleMouseOver = isOverHandleNow;
        repaint();
    }

    // We still call the base class for its own internal state management
    juce::Slider::mouseMove(event);
}

void ModulatableSlider::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter(event);
    mouseMove(event);
}

void ModulatableSlider::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit(event);
    if (isModHandleMouseOver)
    {
        isModHandleMouseOver = false;
        repaint();
    }
}

void ModulatableSlider::mouseDown(const juce::MouseEvent& event)
{
    // Check if the click is on the main slider body
    if (isMouseOverMainSlider())
    {
        isDraggingMainSlider = true;
        // CRITICAL: Only call the base class mouseDown if we intend to start a drag on the main slider
        juce::Slider::mouseDown(event);
    }
    // Check if the click is on the modulation handle
    else if (getModulationHandleBounds().contains(event.getPosition().toFloat()))
    {
        isModHandleMouseDown = true;
        // Here you would start the logic for dragging the modulation amount
        // For now, we just show visual feedback.
        repaint();
    }
    // If the click is elsewhere (e.g., empty space), do nothing.
}

void ModulatableSlider::mouseDrag(const juce::MouseEvent& event)
{
    // CRITICAL: Only forward the drag event to the base class if our flag is set
    if (isDraggingMainSlider)
    {
        juce::Slider::mouseDrag(event);
    }
    // else if (isModHandleMouseDown)
    // {
    //     // Here you would implement the logic to change lfoAmount based on drag distance
    // }
}

void ModulatableSlider::mouseUp(const juce::MouseEvent& event)
{
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
        isModHandleMouseDown = false;
        repaint();
    }
}
