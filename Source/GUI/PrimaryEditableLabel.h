/*
  ==============================================================================

    PrimaryEditableLabel.h
    Created: 30 Aug 2026

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

#include <optional>

struct PrimaryEditableLabelTestAccess;

/** An editable Label that opens only from a complete primary-pointer click.

    JUCE's Label starts single-click editing from mouseUp alone. This wrapper
    owns the corresponding mouseDown source, rejects auxiliary or mixed-button
    gestures, and invalidates delayed releases at lifecycle boundaries while
    leaving keyboard and accessibility entry to Label.
*/
class PrimaryEditableLabel final : public juce::Label
{
public:
    PrimaryEditableLabel(const juce::String& componentName = {},
                         const juce::String& labelText = {})
        : juce::Label(componentName, labelText)
    {
        // Component normally grabs keyboard focus before dispatching
        // mouseDown. Editing already transfers focus to the TextEditor, so
        // disabling that implicit grab prevents rejected auxiliary clicks
        // from changing focus without affecting pointer, Tab, or
        // accessibility entry.
        setMouseClickGrabsKeyboardFocus(false);
    }

    void dismissPointerGesture() noexcept
    {
        clearPointerState();
        completedDoubleClick.reset();
    }

private:
    friend struct PrimaryEditableLabelTestAccess;

    enum class PointerGesture
    {
        none,
        rejected,
        primary
    };

    struct CompletedClick
    {
        juce::MouseInputSource::InputSourceType sourceType =
            juce::MouseInputSource::mouse;
        int sourceIndex = -1;
        juce::Time eventTime;
        juce::Time mouseDownTime;
        int clickCount = 0;
    };

    void mouseDown(const juce::MouseEvent& event) override
    {
        completedDoubleClick.reset();

        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        // A fresh down from the owning source is the first reliable boundary
        // when a host or window manager omitted the previous mouseUp. Rejected
        // auxiliary input does not own the control and may be replaced by a
        // primary down from another source.
        clearPointerState();

        if (! isEnabled())
            return;

        pointerGesture = isCompletePrimaryDown(event)
                             ? PointerGesture::primary
                             : PointerGesture::rejected;
        pointerSourceType = event.source.getType();
        pointerSourceIndex = event.source.getIndex();

        if (pointerGesture == PointerGesture::primary)
            juce::Label::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (pointerGesture != PointerGesture::primary
            || ! isPointerSource(event))
            return;

        if (! isCompletePrimaryDown(event))
        {
            dismissPointerGesture();
            return;
        }

        juce::Label::mouseDrag(event);
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        juce::Label::mouseEnter(event);

        if (safeThis != nullptr)
            recoverMissingPointerUp(event);
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        juce::Label::mouseMove(event);

        if (safeThis != nullptr)
            recoverMissingPointerUp(event);
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        juce::Label::mouseExit(event);

        if (safeThis != nullptr)
            recoverMissingPointerUp(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::none
            || ! isPointerSource(event))
            return;

        const auto completedGesture = pointerGesture;
        clearPointerState();
        completedDoubleClick.reset();

        if (completedGesture != PointerGesture::primary
            || ! isAcceptedRelease(event))
            return;

        if (isEditableOnDoubleClick() && event.getNumberOfClicks() >= 2)
        {
            completedDoubleClick.emplace(CompletedClick {
                event.source.getType(),
                event.source.getIndex(),
                event.eventTime,
                event.mouseDownTime,
                event.getNumberOfClicks()
            });
        }

        if (isEditableOnSingleClick())
        {
            // showEditor invokes onEditorShow synchronously. FreqTextLabel
            // keeps that callback presentation-only, so this can remain the
            // final operation in case another Label listener removes us.
            showEditor();
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        const bool wasAuthorised = completedDoubleClick.has_value()
            && matchesCompletedClick(event, *completedDoubleClick);
        completedDoubleClick.reset();

        if (! wasAuthorised
            || ! isEditableOnDoubleClick()
            || ! isEnabled()
            || ! isShowing()
            || event.mods.isPopupMenu()
            || event.mods.isRightButtonDown()
            || event.mods.isMiddleButtonDown())
            return;

        // As with the single-click path, showing the editor is deliberately
        // the final component access.
        showEditor();
    }

    void visibilityChanged() override
    {
        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        juce::Label::visibilityChanged();

        if (safeThis == nullptr || isShowing())
            return;

        dismissPointerGesture();

        // Hiding an editable Label is a cancellation boundary. hideEditor may
        // synchronously delete the owner through onEditorHide, so keep it last.
        if (isBeingEdited())
            hideEditor(true);
    }

    void enablementChanged() override
    {
        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        juce::Label::enablementChanged();

        if (safeThis != nullptr)
            dismissPointerGesture();
    }

    void textEditorFocusLost(juce::TextEditor& editor) override
    {
        if (! isShowing() || ! isEnabled())
        {
            // A hidden or disabled lifecycle transition must not commit text
            // that was typed for the previous UI context. onEditorHide may
            // delete this Label, so no operation may follow hideEditor.
            hideEditor(true);
            return;
        }

        // Preserve JUCE's normal focus-loss commit semantics. This may notify
        // listeners and synchronously delete the Label, so it stays last.
        juce::Label::textEditorFocusLost(editor);
    }

    bool isCompletePrimaryDown(const juce::MouseEvent& event) const noexcept
    {
        return event.mods.isLeftButtonDown()
            && ! event.mods.isRightButtonDown()
            && ! event.mods.isMiddleButtonDown()
            && ! event.mods.isPopupMenu();
    }

    bool isAcceptedRelease(const juce::MouseEvent& event) const noexcept
    {
        return isEnabled()
            && isShowing()
            && getLocalBounds().contains(event.getPosition())
            && ! event.mouseWasDraggedSinceMouseDown()
            && ! event.mods.isPopupMenu()
            && ! event.mods.isRightButtonDown()
            && ! event.mods.isMiddleButtonDown();
    }

    bool isPointerSource(const juce::MouseEvent& event) const noexcept
    {
        return event.source.getType() == pointerSourceType
            && event.source.getIndex() == pointerSourceIndex;
    }

    static bool matchesCompletedClick(const juce::MouseEvent& event,
                                      const CompletedClick& click) noexcept
    {
        return event.source.getType() == click.sourceType
            && event.source.getIndex() == click.sourceIndex
            && event.eventTime == click.eventTime
            && event.mouseDownTime == click.mouseDownTime
            && event.getNumberOfClicks() == click.clickCount;
    }

    void recoverMissingPointerUp(const juce::MouseEvent& event) noexcept
    {
        if (pointerGesture == PointerGesture::none
            || ! isPointerSource(event))
            return;

        if (! event.mods.isAnyMouseButtonDown()
            || (pointerGesture == PointerGesture::primary
                && ! isCompletePrimaryDown(event)))
            dismissPointerGesture();
    }

    void clearPointerState() noexcept
    {
        pointerGesture = PointerGesture::none;
        pointerSourceIndex = -1;
    }

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    std::optional<CompletedClick> completedDoubleClick;
};
