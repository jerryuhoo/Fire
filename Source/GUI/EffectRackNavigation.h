#pragma once
#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "../Panels/SpectrogramPanel/CloseButton.h"

namespace fire::ui
{
inline juce::Colour effectColour(effects::Type type)
{
    switch (type)
    {
        case effects::Type::chorus: return colours::chorus;
        case effects::Type::delay: return colours::delay;
        case effects::Type::reverb: return colours::reverb;
        case effects::Type::granular: return colours::granular;
        case effects::Type::lofi: return colours::loFi;
        case effects::Type::none: case effects::Type::count: return colours::textMuted;
    }
    return colours::textMuted;
}

// Reuses the existing module buttons rather than duplicating their attachments,
// accessibility or selection behaviour. The row pitch always equals 1/5 of the
// viewport; adding modules extends the content, never shrinks existing rows.
class EffectRackNavigation final : public juce::Component
{
public:
    struct Row { juce::TextButton* button; juce::ToggleButton* power; };
    EffectRackNavigation(FireAudioProcessor& p, int initialScope) : processor(p), content(*this)
    {
        setInterceptsMouseClicks(false, true);
        viewport.setViewedComponent(&content, false);
        content.addMouseListener(this, true);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
        viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, colours::textMuted.withAlpha(0.25f));
        addAndMakeVisible(viewport);
        addAndMakeVisible(addButton);
        addButton.setButtonText("+"); addButton.setTitle("Add effect");
        addButton.setTooltip("Add an effect to this chain (8 insert slots)");
        addButton.setColour(juce::TextButton::buttonColourId, colours::raised);
        addButton.setColour(juce::TextButton::textColourOffId, colours::textSecondary);
        addButton.onClick = [this] { showAddMenu(); };
        for (int slot = 0; slot < effects::slotCount; ++slot)
        {
            auto& button = insertButtons[static_cast<size_t>(slot)];
            auto& power = powerButtons[static_cast<size_t>(slot)];
            auto& remove = removeButtons[static_cast<size_t>(slot)];
            content.addChildComponent(button); content.addChildComponent(power);
            content.addChildComponent(remove);
            button.getProperties().set("fireAnimatedSelection", true);
            button.getProperties().set("fireModuleRail", true);
            button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::textColourOffId, colours::textSecondary);
            button.setColour(juce::TextButton::textColourOnId, colours::textPrimary);
            button.onClick = [this, slot] { if (onSelectEffect) onSelectEffect(slot); };
            button.onContext = [this, slot] { showSlotMenu(slot); };
            button.onRowDrag = [this, slot](const juce::MouseEvent& e) { updateDrag(slot, e); };
            button.onRowDrop = [this](const juce::MouseEvent& e) { finishDrag(e); };
            button.onRowCancel = [this] { cancelDrag(); };
            remove.onClick = [this, slot] {
                if (dragSlot >= 0 || ! isShowing() || ! isEnabled()) return;
                const juce::Component::SafePointer<EffectRackNavigation> safe(this);
                processor.removeInsertEffect(scope, slot);
                if (safe) refresh();
            };
            power.getProperties().set("iconType", "power");
        }
        setScope(initialScope);
    }
    ~EffectRackNavigation() override { dismiss(); content.removeMouseListener(this); viewport.setViewedComponent(nullptr, false); }
    std::function<void(int)> onSelectEffect;

    void setBuiltins(std::vector<Row> rows)
    {
        builtins = std::move(rows);
        for (auto row : builtins)
        {
            content.addAndMakeVisible(*row.button);
            if (row.power) content.addAndMakeVisible(*row.power);
        }
        rebuildRows();
    }
    void setScope(int nextScope)
    {
        if (scope == nextScope) return;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        dismiss();
        if (! safe) return;
        scope = nextScope; selectedSlot = -1;
        cachedTypes.fill(-1); cachedOrders.fill(-1);
        addButton.setComponentID(scope == 0 ? "addMasterEffect" : "addBandEffect");
        for (int slot = 0; slot < effects::slotCount; ++slot)
        {
            const auto i = static_cast<size_t>(slot);
            attachments[i].reset();
            auto id = effects::parameterID(scope, slot, effects::enabledField);
            powerButtons[i].setComponentID(id);
            powerButtons[i].setTitle((scope == 0 ? "Master" : "Band " + juce::String(scope)) + " FX " + juce::String(slot + 1) + " power");
            attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, id, powerButtons[i]);
            insertButtons[i].setComponentID(effects::parameterID(scope, slot, effects::typeField));
            removeButtons[i].setComponentID(effects::parameterID(scope, slot, effects::typeField) + "Remove");
        }
        refresh();
    }
    void setSelectedSlot(int slot)
    {
        selectedSlot = slot;
        for (int i = 0; i < effects::slotCount; ++i) insertButtons[static_cast<size_t>(i)].setToggleState(i == slot, juce::dontSendNotification);
        if (slot >= 0)
            for (auto row : builtins) row.button->setToggleState(false, juce::dontSendNotification);
        updateSelection(true);
    }
    void refresh()
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        int replacement = -1;
        const bool selectionRemoved = selectedSlot >= 0 && processor.getInsertEffectType(scope, selectedSlot) == effects::Type::none;
        if (selectionRemoved)
        {
            const auto previous = std::find_if(visibleRows.begin(), visibleRows.end(), [&](const auto& row) {
                return row.button == &insertButtons[static_cast<size_t>(selectedSlot)];
            });
            if (previous != visibleRows.end())
            {
                for (auto row = previous + 1; row != visibleRows.end() && replacement < 0; ++row)
                    replacement = liveSlotForButton(row->button);
                for (auto row = previous; row != visibleRows.begin() && replacement < 0;)
                    replacement = liveSlotForButton((--row)->button);
            }
        }
        bool changed = false;
        for (int slot = 0; slot < effects::slotCount; ++slot)
        {
            const auto i = static_cast<size_t>(slot);
            const auto type = processor.getInsertEffectType(scope, slot);
            const auto order = processor.getInsertEffectOrder(scope, slot);
            if (cachedTypes[i] != static_cast<int>(type) || cachedOrders[i] != order) changed = true;
            if (cachedTypes[i] != static_cast<int>(type))
            {
                insertButtons[i].setButtonText(effects::name(type));
                insertButtons[i].setTooltip(juce::String(effects::name(type)) + " · slot " + juce::String(slot + 1) + ". Drag to reorder. Hover to remove.");
                powerButtons[i].setColour(juce::ToggleButton::tickColourId, effectColour(type));
                powerButtons[i].setColour(juce::ToggleButton::tickDisabledColourId, colours::disabled);
                powerButtons[i].setTooltip("Enable or bypass " + juce::String(effects::name(type)) + " in this chain");
                const auto description = "Remove " + juce::String(effects::name(type))
                    + (scope == 0 ? " from Master" : " from Band " + juce::String(scope));
                removeButtons[i].setButtonText("Remove");
                removeButtons[i].setTitle(description);
                removeButtons[i].setTooltip(description);
                removeButtons[i].setDescription(description);
                removeButtons[i].setHelpText(description);
            }
            cachedTypes[i] = static_cast<int>(type); cachedOrders[i] = order;
        }
        if (! changed) return;
        cancelRowInteractions();
        if (! safe) return;
        ++generation;
        rebuildRows();
        if (! safe) return;
        if (selectionRemoved)
        {
            selectedSlot = -1;
            if (onSelectEffect) onSelectEffect(replacement);
        }
    }
    void setScale(float value) { scale = value; resized(); }
    void resized() override
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        cancelRowInteractions();
        if (! safe) return;
        auto area = getLocalBounds().reduced(juce::roundToInt(8.0f * scale));
        const auto title = area.removeFromTop(juce::jmin(area.getHeight(), juce::roundToInt(22.0f * scale)));
        const auto side = juce::jmin(title.getHeight(), juce::roundToInt(21.0f * scale));
        addButton.setBounds(title.getRight() - side, title.getY() - juce::roundToInt(3.0f * scale), side, side);
        viewport.setScrollBarThickness(juce::jmax(3, juce::roundToInt(4.0f * scale)));
        viewport.setBounds(area);
        layoutRows();
    }
    void animationTick(float dt)
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        refresh();
        if (! safe) return;
        updateSelection(false);
        if (selectionY.advance(dt)) content.repaint();
        if (dragSlot >= 0)
        {
            const auto edge = juce::jmin(26.0f * scale, viewport.getHeight() * 0.2f);
            float speed = 0;
            if (viewport.getLocalBounds().toFloat().contains(dragViewPoint))
            {
                if (dragViewPoint.y < edge) speed = -(edge - dragViewPoint.y) / edge;
                else if (dragViewPoint.y > viewport.getHeight() - edge) speed = (dragViewPoint.y - viewport.getHeight() + edge) / edge;
            }
            scrollRemainder += speed * rowPitch * 7.0f * juce::jlimit(0.0f, 0.05f, dt);
            const auto pixels = static_cast<int>(scrollRemainder);
            scrollRemainder -= static_cast<float>(pixels);
            if (pixels != 0) viewport.setViewPosition(0, viewport.getViewPositionY() + pixels);
            updateDropTarget();
            content.repaint();
        }
        for (size_t i = 0; i < removeButtons.size(); ++i)
        {
            auto& remove = removeButtons[i];
            if (remove.advanceAnimation(dt)) remove.repaint();
            if (! safe) return;
            const bool reveal = dragSlot < 0 && cachedTypes[i] > 0 && isShowing() && isEnabled()
                && (hoveredSlot == static_cast<int>(i) || insertButtons[i].getFocusAnimation() > 0.01f
                    || powerButtons[i].getFocusAnimation() > 0.01f || remove.getFocusAnimation() > 0.01f);
            if (remove.isPresented() != reveal) remove.setPresented(reveal);
            if (! safe) return;
        }
    }
    void dismiss()
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        ++generation;
        cancelRowInteractions();
        if (! safe) return;
        addButton.dismissPointerGesture();
        if (! safe) return;
        for (auto& button : insertButtons) {button.dismissPointerGesture(); if (! safe) return;}
        for (auto& button : powerButtons) {button.dismissPointerGesture(); if (! safe) return;}
    }
    int getRowPitch() const { return rowPitch; }
    juce::Viewport& getViewport() { return viewport; }
    int getScope() const noexcept { return scope; }
    void showAddMenu()
    {
        if (! isShowing() || ! isEnabled()) return;
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr) return;
        bool room = false;
        for (int slot = 0; slot < effects::slotCount; ++slot) room = room || processor.getInsertEffectType(scope, slot) == effects::Type::none;
        juce::PopupMenu menu;
        for (int type = 1; type < static_cast<int>(effects::Type::count); ++type)
            menu.addItem(type, effects::name(static_cast<effects::Type>(type)), room);
        const auto epoch = generation;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        menu.showMenuAsync(prepareContextMenu(menu, addButton, addButton.getScreenBounds().getBottomLeft())
                              .withMinimumWidth(juce::roundToInt(170.0f * scale))
                              .withStandardItemHeight(juce::roundToInt(29.0f * scale)),
            [safe, epoch](int result) {
                if (! safe || result <= 0 || safe->generation != epoch || ! safe->isShowing() || ! safe->isEnabled()) return;
                const auto slot = safe->processor.addInsertEffect(safe->scope, static_cast<effects::Type>(result));
                if (! safe) return;
                safe->refresh();
                if (safe && slot >= 0 && safe->onSelectEffect) safe->onSelectEffect(slot);
            });
    }
private:
    class SlotButton final : public PrimaryTextButton
    {
    public:
        std::function<void()> onContext;
        std::function<void(const juce::MouseEvent&)> onRowDrag, onRowDrop;
        std::function<void()> onRowCancel;
        float dragThreshold = 6;
        void cancelRowPointer(bool notify = true)
        {
            const bool wasDragging = dragging;
            pointerIndex = -1; dragging = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            const juce::Component::SafePointer<SlotButton> safe(this);
            dismissPointerGesture();
            if (safe && wasDragging && notify && onRowCancel) onRowCancel();
        }
        void mouseDown(const juce::MouseEvent& event) override
        {
            if (pointerIndex >= 0 && ! owns(event)) return;
            const juce::Component::SafePointer<SlotButton> safe(this);
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
            const juce::Component::SafePointer<SlotButton> safe(this);
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
                const juce::Component::SafePointer<SlotButton> safe(this);
                cancelRowPointer(false);
                if (! safe) return;
                if (! event.mods.isAnyMouseButtonDown() && isShowing() && isEnabled())
                { if (onRowDrop) onRowDrop(event); }
                else if (onRowCancel) onRowCancel();
                return;
            }
            pointerIndex = -1;
            PrimaryTextButton::mouseUp(event);
        }
        void mouseMove(const juce::MouseEvent& event) override
        {
            const juce::Component::SafePointer<SlotButton> safe(this);
            if (owns(event) && ! event.mods.isAnyMouseButtonDown()) cancelRowPointer();
            if (safe) PrimaryTextButton::mouseMove(event);
        }
        void mouseEnter(const juce::MouseEvent& event) override
        {
            const juce::Component::SafePointer<SlotButton> safe(this);
            if (owns(event) && ! event.mods.isAnyMouseButtonDown()) cancelRowPointer();
            if (safe) PrimaryTextButton::mouseEnter(event);
        }
        void mouseExit(const juce::MouseEvent& event) override
        {
            const juce::Component::SafePointer<SlotButton> safe(this);
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
            const juce::Component::SafePointer<SlotButton> safe(this);
            cancelRowPointer();
            if (safe) PrimaryTextButton::focusLost(cause);
        }
        void visibilityChanged() override
        {
            const juce::Component::SafePointer<SlotButton> safe(this);
            PrimaryTextButton::visibilityChanged();
            if (safe && ! isShowing()) cancelRowPointer();
        }
        void enablementChanged() override
        {
            const juce::Component::SafePointer<SlotButton> safe(this);
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
    class Content final : public juce::Component
    {
    public:
        explicit Content(EffectRackNavigation& p) : owner(p) {}
        void paint(juce::Graphics& g) override
        {
            if (! owner.selectionInitialised || ! owner.selectedButton()) return;
            const auto margin = juce::jmax(1.0f, 2.0f * owner.scale);
            auto bounds = juce::Rectangle<float>(margin, owner.selectionY.current, getWidth() - margin * 2,
                                                 owner.rowPitch - margin * 2);
            g.setColour(colours::raised);
            g.fillRoundedRectangle(bounds, Metrics::radius * owner.scale);
        }
        void paintOverChildren(juce::Graphics& g) override { owner.paintDrag(g); }
    private:
        EffectRackNavigation& owner;
    };
    int liveSlotForButton(const juce::TextButton* button) const
    {
        for (size_t i = 0; i < insertButtons.size(); ++i)
            if (&insertButtons[i] == button && processor.getInsertEffectType(scope, static_cast<int>(i)) != effects::Type::none)
                return static_cast<int>(i);
        return -1;
    }
    void noteHover(const juce::MouseEvent& event)
    {
        if (dragSlot >= 0) return;
        const auto point = event.getEventRelativeTo(&viewport).position;
        hoveredSlot = -1;
        if (viewport.getLocalBounds().toFloat().contains(point))
        {
            const auto inContent = point + viewport.getViewPosition().toFloat();
            for (auto row : visibleRows)
                if (row.button->getBounds().toFloat().contains(inContent))
                { hoveredSlot = liveSlotForButton(row.button); break; }
        }
    }
    void mouseEnter(const juce::MouseEvent& event) override { noteHover(event); }
    void mouseMove(const juce::MouseEvent& event) override { noteHover(event); }
    void mouseExit(const juce::MouseEvent& event) override { noteHover(event); }
    void mouseDown(const juce::MouseEvent& event) override { noteHover(event); }
    void cancelDrag()
    {
        if (dragSlot >= 0)
        {
            insertButtons[static_cast<size_t>(dragSlot)].setAlpha(1);
            powerButtons[static_cast<size_t>(dragSlot)].setAlpha(1);
        }
        dragSlot = -1; dragTarget = -1; scrollRemainder = 0;
        content.repaint();
    }
    void cancelRowInteractions()
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        cancelDrag(); hoveredSlot = -1;
        for (auto& button : insertButtons) {button.cancelRowPointer(false); if (! safe) return;}
        for (auto& button : removeButtons) {button.setPresented(false, false); if (! safe) return;}
    }
    void updateDrag(int slot, const juce::MouseEvent& event)
    {
        if (! isShowing() || ! isEnabled() || processor.getInsertEffectType(scope, slot) == effects::Type::none) return;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        if (dragSlot < 0)
        {
            if (visibleRows.size() < builtins.size() + 2) return;
            dragSlot = slot; dragGeneration = generation;
            dragOffset = juce::jlimit(0.0f, static_cast<float>(rowPitch), static_cast<float>(event.getMouseDownY()));
            insertButtons[static_cast<size_t>(slot)].setAlpha(0.32f);
            powerButtons[static_cast<size_t>(slot)].setAlpha(0.32f);
            for (auto& button : removeButtons) {button.setPresented(false, false); if (! safe) return;}
        }
        if (dragSlot != slot || dragGeneration != generation) return;
        dragViewPoint = event.getEventRelativeTo(&viewport).position;
        updateDropTarget(); content.repaint();
    }
    void updateDropTarget()
    {
        dragTarget = -1;
        if (dragSlot < 0 || ! viewport.getLocalBounds().toFloat().contains(dragViewPoint)) return;
        const auto y = dragViewPoint.y + viewport.getViewPositionY();
        int position = 0;
        dropLineY = 0;
        for (auto row : visibleRows)
        {
            const auto slot = liveSlotForButton(row.button);
            if (slot < 0 || slot == dragSlot) continue;
            if (y < row.button->getBounds().getCentreY())
            { dropLineY = static_cast<float>(row.button->getY()) - 2.0f * scale; break; }
            ++position;
            dropLineY = static_cast<float>(row.button->getBottom()) + 2.0f * scale;
        }
        dragTarget = position;
    }
    void finishDrag(const juce::MouseEvent& event)
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        refresh(); // An intervening preset/order edit invalidates this gesture.
        if (! safe || dragSlot < 0 || dragGeneration != generation) return;
        dragViewPoint = event.getEventRelativeTo(&viewport).position;
        updateDropTarget();
        const auto slot = dragSlot, target = dragTarget;
        cancelDrag();
        if (target < 0) return;
        processor.moveInsertEffectToPosition(scope, slot, target);
        if (! safe) return;
        refresh();
        if (! safe) return;
        if (selectedSlot == slot) updateSelection(true);
        else if (onSelectEffect) onSelectEffect(slot);
    }
    void paintDrag(juce::Graphics& g)
    {
        if (dragSlot < 0) return;
        const auto view = viewport.getViewArea().toFloat();
        const auto margin = 4.0f * scale;
        const auto height = static_cast<float>(rowPitch) - margin;
        auto bounds = juce::Rectangle<float>(margin, juce::jlimit(view.getY(), juce::jmax(view.getY(), view.getBottom() - height),
            dragViewPoint.y + view.getY() - dragOffset), content.getWidth() - margin * 2, height);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.translated(0, 3 * scale), Metrics::radius * scale);
        g.setColour(colours::raised.brighter(0.08f));
        g.fillRoundedRectangle(bounds, Metrics::radius * scale);
        auto& button = insertButtons[static_cast<size_t>(dragSlot)];
        g.setFont(getLookAndFeel().getTextButtonFont(button, rowPitch));
        g.setColour(colours::textPrimary);
        g.drawFittedText(button.getButtonText(), bounds.reduced(12 * scale, 0).toNearestInt(), juce::Justification::centredLeft, 1);
        if (dragTarget >= 0)
        {
            g.setColour(colours::textPrimary.withAlpha(0.85f));
            const auto y = juce::jlimit(view.getY() + 1, view.getBottom() - 2, dropLineY);
            g.fillRoundedRectangle(margin, y, content.getWidth() - margin * 2, 2 * scale, scale);
        }
    }
    juce::TextButton* selectedButton() const
    {
        for (auto row : visibleRows) if (row.button->getToggleState()) return row.button;
        return nullptr;
    }
    void rebuildRows()
    {
        visibleRows = builtins;
        std::vector<int> active;
        for (int slot = 0; slot < effects::slotCount; ++slot)
            if (processor.getInsertEffectType(scope, slot) != effects::Type::none) active.push_back(slot);
        std::stable_sort(active.begin(), active.end(), [&](int a, int b) {return processor.getInsertEffectOrder(scope, a) < processor.getInsertEffectOrder(scope, b);});
        for (int slot = 0; slot < effects::slotCount; ++slot)
        {
            const bool visible = std::find(active.begin(), active.end(), slot) != active.end();
            insertButtons[static_cast<size_t>(slot)].setVisible(visible);
            powerButtons[static_cast<size_t>(slot)].setVisible(visible);
        }
        for (int slot : active) visibleRows.push_back({&insertButtons[static_cast<size_t>(slot)], &powerButtons[static_cast<size_t>(slot)]});
        layoutRows();
    }
    void layoutRows()
    {
        rowPitch = juce::jmax(1, viewport.getHeight() / 5);
        const auto width = juce::jmax(1, viewport.getWidth() - juce::roundToInt(6.0f * scale));
        content.setSize(width, juce::jmax(viewport.getHeight(), rowPitch * static_cast<int>(visibleRows.size())));
        const auto margin = juce::jmax(1, juce::roundToInt(2.0f * scale));
        for (size_t i = 0; i < visibleRows.size(); ++i)
        {
            const auto row = juce::Rectangle<int>(0, static_cast<int>(i) * rowPitch, width, rowPitch).reduced(margin);
            visibleRows[i].button->setBounds(row);
            const auto slot = liveSlotForButton(visibleRows[i].button);
            if (slot >= 0)
            {
                auto& button = insertButtons[static_cast<size_t>(slot)];
                button.dragThreshold = 6.0f * scale;
                const auto side = juce::jmin(row.getHeight(), juce::roundToInt(24.0f * scale));
                button.getProperties().set("fireModuleTrailingSpace", side + 5.0f * scale);
                auto& remove = removeButtons[static_cast<size_t>(slot)];
                remove.setBounds(row.getRight() - side - juce::roundToInt(3 * scale), row.getCentreY() - side / 2, side, side);
                remove.toFront(false);
            }
            if (auto* power = visibleRows[i].power)
            {
                const auto size = juce::roundToInt(juce::jlimit(16.0f * scale, 24.0f * scale, row.getHeight() * 0.46f));
                power->setBounds(row.getX() + juce::roundToInt(7.0f * scale), row.getCentreY() - size / 2, size, size);
                power->toFront(false);
            }
        }
        updateSelection(false); content.repaint();
    }
    void updateSelection(bool reveal)
    {
        auto* selected = selectedButton();
        if (! selected) return;
        const auto y = static_cast<float>(selected->getY());
        if (! selectionInitialised || ! isShowing()) selectionY.snapTo(y); else selectionY.setTarget(y);
        selectionInitialised = true;
        if (reveal)
        {
            const auto top = viewport.getViewPositionY();
            if (selected->getY() < top) viewport.setViewPosition(0, selected->getY());
            else if (selected->getBottom() > top + viewport.getHeight())
                viewport.setViewPosition(0, selected->getBottom() - viewport.getHeight());
        }
    }
    void showSlotMenu(int slot)
    {
        if (! isShowing() || ! isEnabled() || ! insertButtons[static_cast<size_t>(slot)].isShowing()
            || processor.getInsertEffectType(scope, slot) == effects::Type::none) return;
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr) return;
        juce::PopupMenu menu;
        const auto* selected = &insertButtons[static_cast<size_t>(slot)];
        const bool canMoveUp = visibleRows.size() > builtins.size() && visibleRows[builtins.size()].button != selected;
        const bool canMoveDown = ! visibleRows.empty() && visibleRows.back().button != selected;
        menu.addItem(1, "Move up", canMoveUp); menu.addItem(2, "Move down", canMoveDown);
        menu.addSeparator(); menu.addItem(3, "Remove effect");
        const auto epoch = generation;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        auto& button = insertButtons[static_cast<size_t>(slot)];
        menu.showMenuAsync(prepareContextMenu(menu, button, juce::Desktop::getMousePosition())
                              .withMinimumWidth(juce::roundToInt(170.0f * scale))
                              .withStandardItemHeight(juce::roundToInt(29.0f * scale)), [safe, epoch, slot](int result) {
            if (! safe || safe->generation != epoch || ! safe->isShowing() || ! safe->isEnabled()) return;
            if (result == 3) safe->processor.removeInsertEffect(safe->scope, slot);
            else if (result == 1 || result == 2) safe->processor.moveInsertEffect(safe->scope, slot, result == 1 ? -1 : 1);
            if (safe) safe->refresh();
        });
    }
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    FireAudioProcessor& processor;
    juce::Viewport viewport;
    Content content;
    PrimaryTextButton addButton;
    std::array<SlotButton, effects::slotCount> insertButtons;
    std::array<PrimaryToggleButton, effects::slotCount> powerButtons;
    std::array<CloseButton, effects::slotCount> removeButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, effects::slotCount> attachments;
    std::vector<Row> builtins, visibleRows;
    std::array<int, effects::slotCount> cachedTypes {}, cachedOrders {};
    SpringValue selectionY;
    int scope = -1, selectedSlot = -1, rowPitch = 1;
    bool selectionInitialised = false;
    float scale = 1;
    std::uint64_t generation = 0;
    int hoveredSlot = -1, dragSlot = -1, dragTarget = -1;
    std::uint64_t dragGeneration = 0;
    juce::Point<float> dragViewPoint;
    float dragOffset = 0, dropLineY = 0, scrollRemainder = 0;
};
}
