/*
 ==============================================================================
 
 FreqTextLabel.cpp
 Created: 2 Dec 2020 7:53:08pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#include "FreqTextLabel.h"
#include "../../Utility/AudioHelpers.h"
#include "../../Utility/StrictNumberParser.h"

//==============================================================================
FreqTextLabel::FreqTextLabel(VerticalLine& v) : verticalLine(v)
{
    setOpaque(false);
    revealAnimation.snapTo(0.0f);
    hoverAnimation.snapTo(0.0f);

    // Add and configure the child juce::Label component.
    addAndMakeVisible(freqLabel);
    freqLabel.setEditable(true);
    freqLabel.setMouseCursor(juce::MouseCursor::IBeamCursor);
    freqLabel.setMinimumHorizontalScale(0.78f);
    freqLabel.setBorderSize({ 1, 5, 1, 5 });

    // --- One-time setup for the child Label ---
    // These properties are set once here instead of inefficiently in paint().
    freqLabel.setColour(juce::Label::textColourId, fire::ui::colours::textPrimary);
    freqLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    freqLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    freqLabel.setColour(juce::Label::backgroundWhenEditingColourId,
                        juce::Colours::transparentBlack);
    freqLabel.setColour(juce::Label::outlineWhenEditingColourId,
                        juce::Colours::transparentBlack);
    freqLabel.setJustificationType(juce::Justification::centred);
    freqLabel.setAlpha(0.0f);

    freqLabel.onEditorShow = [this]
    {
        // A legitimate pointer, keyboard, or accessibility entry is already
        // showing this component. Keep this callback free of Component
        // visibility notifications because Label::showEditor continues to
        // access itself after onEditorShow returns.
        revealAnimation.setTarget(1.0f);

        if (auto* editor = freqLabel.getCurrentTextEditor())
        {
            editor->setJustification(juce::Justification::centred);
            editor->setColour(juce::TextEditor::backgroundColourId,
                              juce::Colours::transparentBlack);
            editor->setColour(juce::TextEditor::outlineColourId,
                              juce::Colours::transparentBlack);
            editor->setColour(juce::TextEditor::focusedOutlineColourId,
                              juce::Colours::transparentBlack);
            editor->setColour(juce::TextEditor::highlightColourId,
                              fire::ui::colours::ember.withAlpha(0.38f));
            editor->setColour(juce::TextEditor::highlightedTextColourId,
                              fire::ui::colours::whiteHot);
            editor->selectAll();
        }
    };

    freqLabel.onEditorHide = [this]
    {
        // JUCE's Return/focus-loss commit has copied the TextEditor value into
        // the Label before this callback. Apply it now so the host sees the
        // value change before the matching endChangeGesture notification.
        juce::Component::SafePointer<FreqTextLabel> safeThis(this);
        applyEditedText();

        if (safeThis == nullptr)
            return;

        updateLabelText();

        // A host end notification can synchronously destroy this label, so it
        // must be the final operation in the callback.
        finishEditorGesture();
    };
}

FreqTextLabel::~FreqTextLabel()
{
    // Close the editor while our callbacks and the VerticalLine are still alive.
    // This also balances a gesture if the containing editor is destroyed midway
    // through frequency text entry.
    dismissImmediately();

    freqLabel.onTextChange = nullptr;
    freqLabel.onEditorShow = nullptr;
    freqLabel.onEditorHide = nullptr;
    frequencyEditCallback = nullptr;

    // Unregister the look and feel if one was set.
    setLookAndFeel(nullptr);
}

void FreqTextLabel::paint(juce::Graphics& g)
{
    const auto reveal = juce::jlimit(0.0f, 1.0f, revealAnimation.current);
    if (reveal <= 0.001f)
        return;

    const auto hover = juce::jlimit(0.0f, 1.0f, hoverAnimation.current);
    const juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(reveal);

    auto pill = getLocalBounds().toFloat().reduced(0.75f);
    pill = pill.reduced((1.0f - reveal) * 2.5f,
                        (1.0f - reveal) * 0.75f)
               .translated(0.0f, (1.0f - reveal) * 1.5f);
    const auto radius = juce::jmin(pill.getHeight() * 0.5f,
                                   fire::ui::Metrics::radiusSmall + 1.0f);

    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(pill.translated(0.0f, 1.0f), radius);

    auto top = fire::ui::colours::surface2.brighter(0.02f + hover * 0.025f);
    auto bottom = fire::ui::colours::surface0.darker(0.04f);
    juce::ColourGradient fill(top, pill.getX(), pill.getY(),
                              bottom, pill.getX(), pill.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(pill, radius);

    const auto edge = fire::ui::colours::hairline.interpolatedWith(
        fire::ui::colours::flame, hover * 0.62f);
    g.setColour(edge.withAlpha(0.58f + hover * 0.30f));
    g.drawRoundedRectangle(pill.reduced(0.5f), radius, 1.0f);

    const auto railWidth = juce::jmin(pill.getWidth() * 0.32f,
                                      (11.0f + hover * 9.0f) * mScale);
    const auto railHeight = juce::jmax(1.0f, 1.15f * mScale);
    g.setColour(fire::ui::colours::flame.withAlpha(0.58f + hover * 0.36f));
    g.fillRoundedRectangle(pill.getCentreX() - railWidth * 0.5f,
                           pill.getBottom() - railHeight,
                           railWidth,
                           railHeight,
                           railHeight * 0.5f);

}

void FreqTextLabel::resized()
{
    // The resized() method is the correct place to set the bounds of child components.
    freqLabel.setBounds(getLocalBounds().reduced(1, 0));

    // It's also a good place to update anything that depends on size, like font height.
    freqLabel.setFont(fire::ui::labelFont(
        juce::jlimit(9.0f, 13.5f, 10.75f * mScale)));
}

void FreqTextLabel::visibilityChanged()
{
    if (isShowing())
        return;

    // Component visibility notifications are not propagated to children, so
    // the editable Label cannot observe its owning bubble being hidden. Treat
    // the outer boundary as cancellation too, otherwise a delayed mouseUp can
    // reopen the editor after the bubble is shown again.
    freqLabel.dismissPointerGesture();

    // Discard text typed for the previous visible UI context. onEditorHide can
    // synchronously delete the owning FreqTextLabel, so this must stay last.
    if (freqLabel.isBeingEdited())
        freqLabel.hideEditor(true);
}

bool FreqTextLabel::advanceAnimation(float deltaSeconds)
{
    hoverAnimation.setTarget(isMouseOverCustom() ? 1.0f : 0.0f);

    const auto previousReveal = revealAnimation.current;
    const auto previousHover = hoverAnimation.current;
    const bool revealIsMoving = revealAnimation.advance(deltaSeconds, 0.14f);
    const bool hoverIsMoving = hoverAnimation.advance(deltaSeconds, 0.11f);
    const auto alpha = juce::jlimit(0.0f, 1.0f, revealAnimation.current);

    if (! juce::approximatelyEqual(freqLabel.getAlpha(), alpha))
        freqLabel.setAlpha(alpha);

    const bool visualChanged = revealIsMoving || hoverIsMoving
                               || ! juce::approximatelyEqual(previousReveal,
                                                             revealAnimation.current)
                               || ! juce::approximatelyEqual(previousHover,
                                                             hoverAnimation.current);
    if (visualChanged)
        repaint();

    if (revealAnimation.target <= 0.0f && revealAnimation.isSettled()
        && isVisible())
        setVisible(false);

    return visualChanged;
}

void FreqTextLabel::setFade(bool update, bool isFadeIn)
{
    if (! update)
        return;

    const float newTarget = isFadeIn ? 1.0f : 0.0f;
    if (juce::approximatelyEqual(revealAnimation.target, newTarget)
        && (isFadeIn || ! isVisible()))
        return;

    revealAnimation.setTarget(newTarget);
    if (newTarget > 0.0f)
        setVisible(true);
}

void FreqTextLabel::dismissImmediately()
{
    // A release delayed across a host hide or divider removal must not open a
    // new TextEditor when this bubble is shown again.
    freqLabel.dismissPointerGesture();

    // hideEditor(true) discards the TextEditor contents and invokes our
    // onEditorHide callback while the VerticalLine is still alive.  Keep the
    // explicit guard as a defensive balance for a host tearing down the view
    // between Label callbacks.
    juce::Component::SafePointer<FreqTextLabel> safeThis(this);
    if (freqLabel.isBeingEdited())
    {
        freqLabel.hideEditor(true);

        if (safeThis == nullptr)
            return;
    }

    revealAnimation.snapTo(0.0f);
    hoverAnimation.snapTo(0.0f);
    freqLabel.setAlpha(0.0f);
    setVisible(false);

    if (safeThis == nullptr)
        return;

    // Keep the potentially destructive host notification last.
    finishEditorGesture();
}

void FreqTextLabel::setFreq(int freq)
{
    mFrequency = freq;
    updateLabelText();
}

void FreqTextLabel::applyEditedText()
{
    double requestedFrequency = 0.0;
    if (! fire::utility::parseStrictFrequency(freqLabel.getText(),
                                               requestedFrequency)
        || ! frequencyEditCallback)
        return;

    const auto range = verticalLine.getNormalisableRange();
    if (requestedFrequency < range.start || requestedFrequency > range.end)
        return;

    const double constrainedFrequency = range.snapToLegalValue(requestedFrequency);
    if (juce::approximatelyEqual(constrainedFrequency, verticalLine.getValue()))
        return;

    auto editCallback = frequencyEditCallback;
    juce::Component::SafePointer<FreqTextLabel> safeThis(this);

    if (! editorGestureOpen)
    {
        // Open the gesture only after a valid changed value is known. This
        // keeps Label::onEditorShow presentation-only: JUCE's showEditor keeps
        // accessing the Label after that callback and cannot survive its
        // synchronous deletion. Mark first so destructor cleanup can balance
        // a begin callback that removes the owning editor.
        editorGestureOpen = true;
        verticalLine.beginParameterGesture();

        if (safeThis == nullptr || ! editorGestureOpen)
            return;
    }

    editCallback(static_cast<float>(transformToLog(constrainedFrequency)));

    if (safeThis == nullptr)
        return;

    mFrequency = juce::roundToInt(verticalLine.getValue());
}

void FreqTextLabel::finishEditorGesture()
{
    if (! editorGestureOpen)
        return;

    editorGestureOpen = false;
    verticalLine.endParameterGesture();
}

void FreqTextLabel::setFrequencyEditCallback(FrequencyEditCallback callback)
{
    frequencyEditCallback = std::move(callback);
}

void FreqTextLabel::updateLabelText()
{
    if (freqLabel.isBeingEdited())
        return;

    if (mFrequency < 0)
    {
        freqLabel.setText({}, juce::dontSendNotification);
        return;
    }

    juce::String freqText;
    if (mFrequency >= 1000)
    {
        auto compactKilohertz = juce::String(mFrequency / 1000.0f, 2)
                                    .trimCharactersAtEnd("0")
                                    .trimCharactersAtEnd(".");
        freqText = compactKilohertz + " kHz";
    }
    else
    {
        freqText = juce::String(mFrequency) + " Hz";
    }

    freqLabel.setText(freqText, juce::dontSendNotification);
}

int FreqTextLabel::getFreq() const noexcept
{
    return mFrequency;
}

void FreqTextLabel::setScale(float scale)
{
    mScale = juce::jlimit(0.75f, 2.0f, scale);
    resized(); // Call resized() to update font size based on the new scale.
}

bool FreqTextLabel::isMouseOverCustom() const
{
    // Checks if the mouse is over this component OR its child label.
    return isMouseOver() || freqLabel.isMouseOverOrDragging() || freqLabel.isBeingEdited();
}
