/*
  ==============================================================================

    ModulatableSlider.cpp
    Created: 2 Sep 2025 1:42:12am
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulatableSlider.h"
#include "LookAndFeel.h"
#include <cmath>

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
    const auto point = juce::Point<float>(static_cast<float>(x), static_cast<float>(y));
    if (getHeaderBounds().toFloat().contains(point))
        return true;

    auto outerBounds = getRotarySliderBounds().reduced(juce::jmax(5.0f, 7.0f * getUiScale()));
    auto center = outerBounds.getCentre();
    auto distFromCentre = point.getDistanceFrom(center);
    auto outerRadius = juce::jmin(outerBounds.getWidth(), outerBounds.getHeight()) / 2.0f;
    float maxSliderClickableRadius = outerRadius * 1.1f;

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

juce::Rectangle<float> ModulatableSlider::getModulationHandleBounds() const
{
    auto bounds = getRotarySliderBounds().reduced(juce::jmax(5.0f, 7.0f * getUiScale()));
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    float handleSize = radius * 0.4f;

    float handleCenterX = bounds.getCentreX() + (radius * 0.85f);
    float handleCenterY = bounds.getCentreY() + (radius * 0.85f);

    return juce::Rectangle<float>(handleSize, handleSize).withCentre({ handleCenterX, handleCenterY });
}

float ModulatableSlider::getUiScale() const noexcept
{
    if (const auto* fireLookAndFeel = dynamic_cast<const FireLookAndFeel*>(&getLookAndFeel()))
        return juce::jmax(0.1f, fireLookAndFeel->scale);

    return 1.0f;
}

juce::Rectangle<float> ModulatableSlider::getRotarySliderBounds() const
{
    auto& mutableSlider = const_cast<ModulatableSlider&>(*this);
    return getLookAndFeel().getSliderLayout(mutableSlider).sliderBounds.toFloat();
}

juce::Rectangle<int> ModulatableSlider::getHeaderBounds() const
{
    auto header = getLocalBounds();
    const auto rotaryBounds = getRotarySliderBounds();
    header.setBottom(juce::jlimit(0, getHeight(), juce::roundToInt(rotaryBounds.getY())));
    return header;
}

// New helper function
bool ModulatableSlider::isMouseOverMainSlider() const
{
    // The main slider is highlighted only if the mouse is over the component
    // in general, but NOT specifically over the modulation handle.
    return isMouseOver() && ! isModHandleMouseOver;
}

bool ModulatableSlider::advanceAnimation(float deltaSeconds) noexcept
{
    deltaSeconds = juce::jlimit(0.0f, 0.05f, deltaSeconds);
    const auto oldHover = hoverAnimation;
    const auto oldPress = pressAnimation;
    const auto hoverTarget = isMouseOverMainSlider() && isEnabled() ? 1.0f : 0.0f;
    const auto pressTarget = (isDraggingMainSlider || (isMouseButtonDown() && ! isModHandleMouseDown))
                                 && isEnabled()
                             ? 1.0f
                             : 0.0f;
    const auto hoverStep = juce::jmin(1.0f, deltaSeconds * 10.0f);
    const auto pressStep = juce::jmin(1.0f, deltaSeconds * 16.0f);
    hoverAnimation += (hoverTarget - hoverAnimation) * hoverStep;
    pressAnimation += (pressTarget - pressAnimation) * pressStep;

    if (std::abs(hoverAnimation - hoverTarget) < 0.002f)
        hoverAnimation = hoverTarget;
    if (std::abs(pressAnimation - pressTarget) < 0.002f)
        pressAnimation = pressTarget;

    return std::abs(oldHover - hoverAnimation) > 0.001f
           || std::abs(oldPress - pressAnimation) > 0.001f;
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
    stopTimer();
    juce::Slider::mouseEnter(event);
    mouseMove(event);
    label.setVisible(false);
    const auto uiScale = getUiScale();
    setTextBoxStyle(juce::Slider::TextBoxAbove, false,
                    juce::roundToInt(TEXTBOX_WIDTH * uiScale),
                    juce::roundToInt(TEXTBOX_HEIGHT * uiScale));
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

    startTimer(100);
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
    if (onClickInAssignMode && event.mods.isLeftButtonDown()
        && ! event.mods.isPopupMenu())
    {
        // Assigning a target exits assign mode, which clears the callback on
        // every slider (including this one).  Keep the callable alive until
        // it returns instead of destroying the std::function target while it
        // is still executing.
        auto assignCallback = onClickInAssignMode;
        assignCallback(parameterID);
        return;
    }

    const bool isOnModulationHandle = isModulated
        && getModulationHandleBounds().contains(event.getPosition().toFloat());

    // On macOS a Ctrl-left-click is a popup-menu click. Handle it before the
    // polarity shortcut or Slider's drag state so it behaves like a physical
    // right click on every platform.
    if (event.mods.isPopupMenu())
        return;

    // Test the actual mouse-down position rather than relying on the last
    // mouseMove event. A fast click can otherwise start a main-slider drag
    // while the pointer is already over the modulation handle.
    if (isOnModulationHandle)
    {
        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            if (onBipolarModeToggled)
                onBipolarModeToggled(parameterID);

            return;
        }

        if (event.mods.isLeftButtonDown())
        {
            isModHandleMouseDown = true;
            initialLfoAmount = lfoAmount;

            if (onModDragStart)
                onModDragStart(this);

            repaint();
        }

        return;
    }

    if (! event.mods.isLeftButtonDown())
        return;

    isDraggingMainSlider = true;

    if (onMainDragStart)
        onMainDragStart(this);

    juce::Slider::mouseDown(event);
}

void ModulatableSlider::mouseDrag(const juce::MouseEvent& event)
{
    // CRITICAL: Only forward the drag event to the base class if our flag is set
    if (isDraggingMainSlider)
    {
        if (onMainDragMove)
            onMainDragMove(this);
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

        if (onModDragMove)
            onModDragMove(this);

        // This will update the UI
        repaint();
    }
}

void ModulatableSlider::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        const bool isOnModulationHandle = isModulated
            && getModulationHandleBounds().contains(event.getPosition().toFloat());
        juce::PopupMenu menu;
        if (isOnModulationHandle)
        {
            menu.addItem(1, "Set Value");
            menu.addItem(2, "Clear LFO");
            menu.addItem(3, "Invert Depth");

            juce::String bipolarToggleText = isBipolar ? "Switch to Unipolar" : "Switch to Bipolar";
            menu.addItem(4, bipolarToggleText);

            juce::String bypassToggleText = isBypassed ? "Enable modulation" : "Bypass modulation";
            menu.addItem(5, bypassToggleText);

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                               createModulationMenuResultHandler());
        }
        else if (parameterID.isNotEmpty())
        {
            menu.addSectionHeader("Assign modulation");
            for (int lfoIndex = 0; lfoIndex < 4; ++lfoIndex)
                menu.addItem(lfoIndex + 1,
                             "LFO " + juce::String(lfoIndex + 1),
                             true,
                             isModulated && lfoSource == lfoIndex + 1);

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                               createLfoAssignmentMenuResultHandler());
        }

        return;
    }

    if (isDraggingMainSlider)
    {
        // Pair the base-class mouseUp only with a mouseDown that was actually
        // forwarded to it. Handle clicks never enter Slider's drag state.
        juce::Slider::mouseUp(event);

        if (onMainDragEnd)
            onMainDragEnd(this);
        isDraggingMainSlider = false;
        repaint();
    }

    if (isModHandleMouseDown)
    {
        if (onModDragEnd)
            onModDragEnd(this);

        isModHandleMouseDown = false;
        repaint();
    }
}

std::function<void(int)> ModulatableSlider::createModulationMenuResultHandler()
{
    return [safeThis = juce::Component::SafePointer<ModulatableSlider>(this),
            targetParameterIDAtOpen = parameterID](int result)
    {
        if (! safeThis || result <= 0)
            return;

        safeThis->executeModulationMenuCommand(
            static_cast<ModulationMenuCommand>(result),
            targetParameterIDAtOpen);
    };
}

std::function<void(int)> ModulatableSlider::createLfoAssignmentMenuResultHandler()
{
    return [safeThis = juce::Component::SafePointer<ModulatableSlider>(this),
            targetParameterIDAtOpen = parameterID](int result)
    {
        if (! safeThis || ! juce::isPositiveAndBelow(result - 1, 4)
            || targetParameterIDAtOpen.isEmpty())
            return;

        auto callback = safeThis->onLfoAssignmentRequested;
        if (callback)
            callback(result - 1, targetParameterIDAtOpen);
    };
}

void ModulatableSlider::executeModulationMenuCommand(
    ModulationMenuCommand command,
    const juce::String& targetParameterID)
{
    if (targetParameterID.isEmpty())
        return;

    switch (command)
    {
        case ModulationMenuCommand::setValue:
            if (onSetValueRequested)
                onSetValueRequested(this, targetParameterID);
            break;
        case ModulationMenuCommand::clearModulation:
            if (onModulationCleared)
                onModulationCleared(targetParameterID);
            break;
        case ModulationMenuCommand::invertDepth:
            if (onModulationInverted)
                onModulationInverted(targetParameterID);
            break;
        case ModulationMenuCommand::togglePolarity:
            if (onBipolarModeToggled)
                onBipolarModeToggled(targetParameterID);
            break;
        case ModulationMenuCommand::toggleBypass:
            if (onBypassToggled)
                onBypassToggled(targetParameterID);
            break;
        default:
            break;
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

    // Match the exact header reserved by FireLookAndFeel. This keeps the static
    // label, hover text editor and rotary drawing area aligned at every zoom.
    const auto headerBounds = getHeaderBounds();
    label.setBounds(headerBounds);
    label.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(juce::jmax(1.0f,
                                   juce::jmin(KNOB_FONT_SIZE * getUiScale(),
                                              static_cast<float>(headerBounds.getHeight()) * 0.68f)))
            .withStyle("Plain") });
}

void ModulatableSlider::timerCallback()
{
    // This function is called when the timer finishes.
    stopTimer();

    // Check if the mouse is truly outside the slider and its children (like the textbox).
    if (! isMouseOver(true))
    {
        label.setVisible(true);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }
}
