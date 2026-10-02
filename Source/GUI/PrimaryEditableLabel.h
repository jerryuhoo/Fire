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

namespace fire::ui
{
class GuardedLabelValueInterface final
    : public juce::AccessibilityTextValueInterface
{
public:
    explicit GuardedLabelValueInterface(juce::Label& labelToWrap)
        : label(labelToWrap)
    {
    }

    bool isReadOnly() const override { return true; }

    juce::String getCurrentValueAsString() const override
    {
        return label.getText();
    }

    void setValueAsString(const juce::String&) override {}

private:
    juce::Label& label;
};

class GuardedLabelAccessibilityHandler final
    : public juce::AccessibilityHandler
{
public:
    explicit GuardedLabelAccessibilityHandler(juce::Label& labelToWrap)
        : juce::AccessibilityHandler(
              labelToWrap,
              labelToWrap.isEditable()
                  ? juce::AccessibilityRole::editableText
                  : juce::AccessibilityRole::label,
              makeActions(labelToWrap),
              { std::make_unique<GuardedLabelValueInterface>(labelToWrap) }),
          label(labelToWrap)
    {
    }

    juce::String getTitle() const override { return label.getText(); }
    juce::String getHelp() const override { return label.getTooltip(); }

    juce::AccessibleState getCurrentState() const override
    {
        if (label.isBeingEdited())
            return {}; // allow focus to pass through to the TextEditor

        return juce::AccessibilityHandler::getCurrentState();
    }

private:
    static juce::AccessibilityActions makeActions(juce::Label& label)
    {
        if (! label.isEditable())
            return {};

        return juce::AccessibilityActions().addAction(
            juce::AccessibilityActionType::press,
            [&label]
            {
                if (label.isEnabled() && label.isShowing())
                    label.showEditor();
            });
    }

    juce::Label& label;
};
} // namespace fire::ui

/** An editable Label that opens only from a complete primary-pointer click.

    JUCE's Label starts single-click editing from mouseUp alone. This wrapper
    owns the corresponding mouseDown source, rejects auxiliary or mixed-button
    gestures, and invalidates delayed releases at lifecycle boundaries while
    retaining guarded keyboard and accessibility entry.
*/
class PrimaryEditableLabel final : public juce::Label,
                                   private juce::Timer
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

    ~PrimaryEditableLabel() override
    {
        stopTimer();
    }

    void dismissPointerGesture() noexcept
    {
        clearPointerState();
        completedDoubleClick.reset();
    }

private:
    friend struct PrimaryEditableLabelTestAccess;

    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override
    {
        return std::make_unique<fire::ui::GuardedLabelAccessibilityHandler>(
            *this);
    }

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
        juce::uint32 peerID = 0;
    };

    void mouseDown(const juce::MouseEvent& event) override
    {
        completedDoubleClick.reset();

        if (pointerGesture == PointerGesture::primary
            && pointerPeerID == currentPeerID()
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
        pointerPeerID = currentPeerID();
        startLifecycleMonitor();

        if (pointerGesture == PointerGesture::primary)
            juce::Label::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (pointerGesture != PointerGesture::none && pointerPeerID != currentPeerID())
        {
            dismissPointerGesture();
            return;
        }

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
        if (pointerGesture != PointerGesture::none && pointerPeerID != currentPeerID())
        {
            dismissPointerGesture();
            return;
        }

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
                event.getNumberOfClicks(),
                currentPeerID()
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
            && completedDoubleClick->peerID == currentPeerID()
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

        if (safeThis == nullptr)
            return;

        dismissPointerGesture();

        // Disabling an editable Label is a cancellation boundary just like
        // hiding it. hideEditor may synchronously delete the owner through
        // onEditorHide, so keep it last.
        if (! isEnabled() && isBeingEdited())
            hideEditor(true);
    }

    void parentHierarchyChanged() override
    {
        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        juce::Label::parentHierarchyChanged();

        if (safeThis == nullptr || (isShowing() && ! hasStalePeerSession()))
            return;

        dismissPointerGesture();

        // Losing the peer invalidates both an in-flight click and editor text
        // from the old UI context. onEditorHide may synchronously delete this
        // Label, so no component access may follow hideEditor.
        if (isBeingEdited())
            hideEditor(true);
    }

    void textEditorReturnKeyPressed(juce::TextEditor& editor) override
    {
        if (! isEditorPeerCurrent())
        {
            dismissPointerGesture();
            hideEditor(true);
            return;
        }

        juce::Label::textEditorReturnKeyPressed(editor);
    }

    void textEditorFocusLost(juce::TextEditor& editor) override
    {
        if (! isEditorPeerCurrent())
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

    void editorShown(juce::TextEditor* editor) override
    {
        editorPeerID = currentPeerID();
        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        juce::Label::editorShown(editor);

        if (safeThis != nullptr)
            startLifecycleMonitor();
    }

    void editorAboutToBeHidden(juce::TextEditor* editor) override
    {
        // Label::hideEditor(false) is non-virtual and can be called through a
        // Label reference. Neutralise stale text before that base path copies
        // it into the Label or publishes a change after this callback.
        if (editor != nullptr && ! isEditorPeerCurrent())
            editor->setText(getText(), false);
        editorPeerID = 0;
        juce::Label::editorAboutToBeHidden(editor);
    }

    void timerCallback() override
    {
        if (isShowing() && ! hasStalePeerSession())
        {
            if (pointerGesture == PointerGesture::none
                && ! completedDoubleClick.has_value()
                && ! isBeingEdited())
                stopTimer();

            return;
        }

        const juce::Component::SafePointer<PrimaryEditableLabel> safeThis(this);
        dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        stopTimer();

        // Component::removeFromDesktop does not emit a hierarchy callback.
        // Poll only while a click or editor is active; also compare the peer
        // ID so a detach/reattach between polls cannot revive the old session.
        // hideEditor
        // may delete this Label through onEditorHide and must remain last.
        if (isBeingEdited())
            hideEditor(true);
    }

    juce::uint32 currentPeerID() const noexcept
    {
        if (const auto* peer = getPeer())
            return peer->getUniqueID();
        return 0;
    }

    bool isEditorPeerCurrent() const noexcept
    {
        return isShowing() && isEnabled() && editorPeerID != 0
            && editorPeerID == currentPeerID();
    }

    bool hasStalePeerSession() const noexcept
    {
        const auto peerID = currentPeerID();
        return (pointerGesture != PointerGesture::none && pointerPeerID != peerID)
            || (completedDoubleClick.has_value() && completedDoubleClick->peerID != peerID)
            || (isBeingEdited() && editorPeerID != peerID);
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

        if (pointerPeerID != currentPeerID()
            || ! event.mods.isAnyMouseButtonDown()
            || (pointerGesture == PointerGesture::primary
                && ! isCompletePrimaryDown(event)))
            dismissPointerGesture();
    }

    void clearPointerState() noexcept
    {
        pointerGesture = PointerGesture::none;
        pointerSourceIndex = -1;
        pointerPeerID = 0;
    }

    void startLifecycleMonitor()
    {
        constexpr int lifecyclePollIntervalMs = 10;
        if (! isTimerRunning())
            startTimer(lifecyclePollIntervalMs);
    }

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    juce::uint32 pointerPeerID = 0, editorPeerID = 0;
    std::optional<CompletedClick> completedDoubleClick;
};
