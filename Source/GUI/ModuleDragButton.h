#pragma once
#include "PrimaryButton.h"

namespace fire::ui
{
// Shared by fixed modules and insert slots so all rows own the same drag lifecycle.
class ModuleDragButton final : public PrimaryTextButton
{
public:
    using PrimaryTextButton::PrimaryTextButton;
    std::function<void()> onContext;
    std::function<void(const juce::MouseEvent&)> onRowDrag, onRowDrop;
    std::function<void()> onRowCancel;
    float dragThreshold = 6;
    void cancelRowPointer(bool notify = true)
    {
        const bool wasDragging = dragging;
        pointerIndex = -1; dragging = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        dismissPointerGesture();
        if (safe && wasDragging && notify && onRowCancel) onRowCancel();
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (pointerIndex >= 0 && ! owns(event)) return;
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        cancelRowPointer();
        if (! safe) return;
        if (event.mods.isPopupMenu() && ! event.mods.isMiddleButtonDown()
            && ! (event.mods.isLeftButtonDown() && event.mods.isRightButtonDown()))
        { if (onContext) onContext(); return; }
        PrimaryTextButton::mouseDown(event);
        if (safe && primary(event) && isEnabled() && isShowing())
        {
            pointerIndex = event.source.getIndex(); pointerType = event.source.getType();
            origin = event.position;
        }
    }
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (! owns(event)) return;
        if (! primary(event) || ! isShowing() || ! isEnabled()) { cancelRowPointer(); return; }
        if (! dragging && event.position.getDistanceFrom(origin) < dragThreshold)
        { PrimaryTextButton::mouseDrag(event); return; }
        dragging = true;
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        dismissPointerGesture(); // A reorder must never become a click on release.
        if (! safe) return;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        if (onRowDrag) onRowDrag(event);
    }
    void mouseUp(const juce::MouseEvent& event) override
    {
        if (pointerIndex >= 0 && ! owns(event)) return;
        if (dragging)
        {
            const juce::Component::SafePointer<ModuleDragButton> safe(this);
            cancelRowPointer(false);
            if (! safe) return;
            // JUCE delivers mouseUp with the modifiers from before the
            // release, including the released left button itself.
            if (! event.mods.isRightButtonDown() && ! event.mods.isMiddleButtonDown()
                && ! event.mods.isPopupMenu() && isShowing() && isEnabled())
            { if (onRowDrop) onRowDrop(event); }
            else if (onRowCancel) onRowCancel();
            return;
        }
        pointerIndex = -1;
        PrimaryTextButton::mouseUp(event);
    }
    void mouseMove(const juce::MouseEvent& event) override
    {
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        if (owns(event) && ! event.mods.isAnyMouseButtonDown()) cancelRowPointer();
        if (safe) PrimaryTextButton::mouseMove(event);
    }
    void mouseEnter(const juce::MouseEvent& event) override
    {
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        if (owns(event) && ! event.mods.isAnyMouseButtonDown()) cancelRowPointer();
        if (safe) PrimaryTextButton::mouseEnter(event);
    }
    void mouseExit(const juce::MouseEvent& event) override
    {
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        if (owns(event) && ! event.mods.isAnyMouseButtonDown()) cancelRowPointer();
        if (safe) PrimaryTextButton::mouseExit(event);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.isKeyCode(juce::KeyPress::escapeKey) && pointerIndex >= 0)
        { cancelRowPointer(); return true; }
        return PrimaryTextButton::keyPressed(key);
    }
    void focusLost(FocusChangeType cause) override
    {
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        cancelRowPointer();
        if (safe) PrimaryTextButton::focusLost(cause);
    }
    void visibilityChanged() override
    {
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        PrimaryTextButton::visibilityChanged();
        if (safe && ! isShowing()) cancelRowPointer();
    }
    void enablementChanged() override
    {
        const juce::Component::SafePointer<ModuleDragButton> safe(this);
        PrimaryTextButton::enablementChanged();
        if (safe && ! isEnabled()) cancelRowPointer();
    }
private:
    static bool primary(const juce::MouseEvent& event)
    {
        return event.mods.isLeftButtonDown() && ! event.mods.isRightButtonDown()
            && ! event.mods.isMiddleButtonDown() && ! event.mods.isPopupMenu();
    }
    bool owns(const juce::MouseEvent& event) const
    { return pointerIndex >= 0 && pointerIndex == event.source.getIndex() && pointerType == event.source.getType(); }
    int pointerIndex = -1;
    juce::MouseInputSource::InputSourceType pointerType = juce::MouseInputSource::mouse;
    juce::Point<float> origin;
    bool dragging = false;
};
}
