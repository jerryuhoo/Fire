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
}

// void ModulatableSlider::paint(juce::Graphics& g)
// {
//     // First, let the base class draw the slider itself.
//     juce::Slider::paint(g);

//     // Only draw modulation visuals if the parameter is modulated
//     if (isModulated)
//     {
//         auto bounds = getLocalBounds().toFloat().reduced(10);
//         auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
//         auto centre = bounds.getCentre();
//         auto rotaryParams = getRotaryParameters();

//         auto valueToAngle = [&](double value)
//         {
//             return rotaryParams.startAngleRadians
//                    + (rotaryParams.endAngleRadians - rotaryParams.startAngleRadians)
//                          * (float) valueToProportionOfLength(value);
//         };

//         // Draw modulation range arc (lfoAmount)
//         // This is your existing logic, which is great.
//         double range = getMaximum() - getMinimum();
//         double halfModRange = lfoAmount * range * 0.5;
//         double currentValue = getValue();

//         auto arcStartValue = juce::jlimit(getMinimum(), getMaximum(), currentValue - halfModRange);
//         auto arcEndValue = juce::jlimit(getMinimum(), getMaximum(), currentValue + halfModRange);

//         float startAngle = valueToAngle(arcStartValue);
//         float endAngle = valueToAngle(arcEndValue);

//         juce::Path modulationArc;
//         // Draw the arc from start to end angle
//         modulationArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);

//         g.setColour(juce::Colours::cyan.withAlpha(0.5f));
//         g.strokePath(modulationArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

//         // NEW: Draw the current LFO value indicator on the arc
//         if (lfoAmount > 0)
//         {
//             // lfoValue is from -1 to 1. Map it to the modulation arc.
//             double modulatedValue = currentValue + lfoValue * halfModRange;
//             float indicatorAngle = valueToAngle(juce::jlimit(getMinimum(), getMaximum(), modulatedValue));

//             juce::Point<float> indicatorPoint(centre.x + radius * std::sin(indicatorAngle),
//                                               centre.y - radius * std::cos(indicatorAngle));

//             g.setColour(juce::Colours::orange);
//             g.fillEllipse(indicatorPoint.x - 3.5f, indicatorPoint.y - 3.5f, 7.0f, 7.0f);
//         }

//         // Draw Modulation Handle
//         auto handleBounds = getModulationHandleBounds();
//         g.setColour(isModHandleMouseDown ? juce::Colours::white.darker(0.2f) : (isModHandleMouseOver ? juce::Colours::white : juce::Colours::grey));
//         g.fillEllipse(handleBounds);
//     }
// }

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
    else if (isModulated && getModulationHandleBounds().contains(event.getPosition().toFloat()))
    {
        isModHandleMouseDown = true;
        initialLfoAmount = lfoAmount;
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

        // This will update the UI
        repaint();
    }
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
