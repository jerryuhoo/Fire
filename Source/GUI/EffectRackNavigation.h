#pragma once
#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "ModuleDragButton.h"
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
        addButton.setTooltip("Enable a built-in module or add an effect to this chain (8 insert slots)");
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
            configureDragButton(button, module_order::firstInsert + slot);
            remove.onClick = [this, slot] {
                if (dragNode >= 0 || ! isShowing() || ! isEnabled()) return;
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
        ++generation; // A pending menu must never act on replacement rows.
        builtins = std::move(rows);
        for (size_t node = 0; node < builtins.size(); ++node)
        {
            auto row = builtins[node];
            if (auto* button = dynamic_cast<ModuleDragButton*>(row.button))
                configureDragButton(*button, static_cast<int>(node));
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
        cachedTypes.fill(-1); cachedModuleOrder.fill(-2);
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
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        selectedSlot = slot;
        for (int i = 0; i < effects::slotCount; ++i)
        {
            insertButtons[static_cast<size_t>(i)].setToggleState(i == slot, juce::dontSendNotification);
            if (! safe) return;
        }
        if (slot >= 0)
            for (auto row : builtins)
            {
                row.button->setToggleState(false, juce::dontSendNotification);
                if (! safe) return;
            }
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
                    replacement = nodeForButton(row->button);
                for (auto row = previous; row != visibleRows.begin() && replacement < 0;)
                    replacement = nodeForButton((--row)->button);
            }
        }
        const auto order = processor.getModuleOrder(scope);
        bool changed = cachedModuleOrder != order;
        cachedModuleOrder = order;
        for (int slot = 0; slot < effects::slotCount; ++slot)
        {
            const auto i = static_cast<size_t>(slot);
            const auto type = processor.getInsertEffectType(scope, slot);
            if (cachedTypes[i] != static_cast<int>(type)) changed = true;
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
            cachedTypes[i] = static_cast<int>(type);
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
            selectNode(replacement);
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
        if (dragNode >= 0)
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
            const bool reveal = dragNode < 0 && cachedTypes[i] > 0 && isShowing() && isEnabled()
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
    static constexpr int builtinMenuItemID(int node) noexcept { return 100 + node; }

    // Built-ins own permanent host parameters and already have a rail row.
    // Selecting one enables that row idempotently; it never consumes a slot.
    bool activateBuiltin(int node)
    {
        if (! isShowing() || ! isEnabled() || dragNode >= 0 || ! isAddableBuiltin(node)) return false;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        const auto epoch = generation;
        const auto originalScope = scope;
        const auto row = builtins[static_cast<size_t>(node)];
        const juce::Component::SafePointer<juce::TextButton> button(row.button);
        const juce::Component::SafePointer<juce::ToggleButton> power(row.power);
        if (! button || ! button->isEnabled() || (power && ! power->isEnabled())) return false;
        const auto stillCurrent = [&]
        {
            return safe && button && safe->generation == epoch && safe->scope == originalScope
                && safe->isShowing() && safe->isEnabled();
        };
        if (power && ! power->getToggleState())
            power->setToggleState(true, juce::sendNotificationSync);
        // Parameter/host callbacks may destroy the editor or rebind the band.
        if (! stillCurrent()) return false;
        if (! button->getToggleState()) button->triggerClick();
        if (! stillCurrent()) return false;
        setSelectedSlot(-1);
        if (! stillCurrent()) return false;
        updateSelection(true);
        return true;
    }

    juce::PopupMenu createAddMenu() const
    {
        juce::PopupMenu menu;
        bool hasBuiltins = false;
        const auto addBuiltin = [&](int node, const char* label)
        {
            if (! isAddableBuiltin(node)) return;
            const auto row = builtins[static_cast<size_t>(node)];
            menu.addItem(builtinMenuItemID(node), label,
                row.button->isEnabled() && (! row.power || row.power->isEnabled()),
                row.power && row.power->getToggleState());
            hasBuiltins = true;
        };
        if (scope == 0)
        {
            addBuiltin(0, "EQ");
            addBuiltin(1, "Lo-Fi");
        }
        else
        {
            addBuiltin(0, "Drive"); addBuiltin(1, "Shape");
            addBuiltin(2, "Compressor"); addBuiltin(4, "OTT"); addBuiltin(3, "Stereo");
        }
        if (hasBuiltins) menu.addSeparator();
        const bool room = hasInsertRoom();
        for (int type = 1; type < static_cast<int>(effects::Type::count); ++type)
        {
            // Master has one canonical Lo-Fi. Historical insert Lo-Fi rows
            // remain accessible, but the menu cannot create a duplicate form.
            if (scope == 0 && type == static_cast<int>(effects::Type::lofi)) continue;
            menu.addItem(type, effects::name(static_cast<effects::Type>(type)), room);
        }
        return menu;
    }

    std::function<void(int)> createAddMenuResultHandler()
    {
        const auto epoch = generation;
        const auto request = ++addMenuGeneration;
        const auto originalScope = scope;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        return [safe, epoch, request, originalScope](int result)
        {
            if (! safe || safe->generation != epoch || safe->addMenuGeneration != request
                || safe->scope != originalScope || ! safe->isShowing() || ! safe->isEnabled()) return;
            ++safe->addMenuGeneration; // Menu completion is a one-shot action.
            if (result >= builtinMenuItemID(0) && result <= builtinMenuItemID(4))
            {
                safe->activateBuiltin(result - builtinMenuItemID(0));
                return;
            }
            if (result <= 0 || result >= static_cast<int>(effects::Type::count)) return;
            if (originalScope == 0 && result == static_cast<int>(effects::Type::lofi))
            {
                safe->activateBuiltin(1);
                return;
            }
            if (! safe->hasInsertRoom()) return;
            const auto slot = safe->processor.addInsertEffect(originalScope, static_cast<effects::Type>(result));
            if (! safe || safe->scope != originalScope || safe->generation != epoch
                || ! safe->isShowing() || ! safe->isEnabled()) return;
            safe->refresh();
            if (! safe || safe->scope != originalScope || ! safe->isShowing() || ! safe->isEnabled()) return;
            auto callback = safe->onSelectEffect;
            if (slot >= 0 && callback) callback(slot);
        };
    }

    void showAddMenu()
    {
        if (! isShowing() || ! isEnabled()) return;
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr) return;
        auto menu = createAddMenu();
        menu.showMenuAsync(prepareContextMenu(menu, addButton, addButton.getScreenBounds().getBottomLeft())
                              .withMinimumWidth(juce::roundToInt(170.0f * scale))
                              .withStandardItemHeight(juce::roundToInt(29.0f * scale)),
            createAddMenuResultHandler());
    }
private:
    bool isAddableBuiltin(int node) const noexcept
    {
        const int count = scope == 0 ? 2 : 5; // Master Analysis is a view, not an effect.
        return juce::isPositiveAndBelow(node, count)
            && juce::isPositiveAndBelow(node, static_cast<int>(builtins.size()))
            && builtins[static_cast<size_t>(node)].button != nullptr;
    }
    bool hasInsertRoom() const
    {
        for (int slot = 0; slot < effects::slotCount; ++slot)
            if (processor.getInsertEffectType(scope, slot) == effects::Type::none) return true;
        return false;
    }
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
    void configureDragButton(ModuleDragButton& button, int node)
    {
        button.onContext = [this, node] { showModuleMenu(node); };
        button.onRowDrag = [this, node](const juce::MouseEvent& e) { updateDrag(node, e); };
        button.onRowDrop = [this](const juce::MouseEvent& e) { finishDrag(e); };
        button.onRowCancel = [this] { cancelDrag(); };
    }
    Row rowForNode(int node)
    {
        if (node >= 0 && node < static_cast<int>(builtins.size())) return builtins[static_cast<size_t>(node)];
        if (node >= module_order::firstInsert && node < module_order::capacity)
        {
            const auto slot = static_cast<size_t>(node - module_order::firstInsert);
            return {&insertButtons[slot], &powerButtons[slot]};
        }
        return {nullptr, nullptr};
    }
    int nodeForButton(const juce::TextButton* button) const
    {
        for (size_t node = 0; node < builtins.size(); ++node)
            if (builtins[node].button == button) return static_cast<int>(node);
        const auto slot = liveSlotForButton(button);
        return slot < 0 ? -1 : module_order::firstInsert + slot;
    }
    void selectNode(int node)
    {
        auto row = rowForNode(node);
        if (! row.button) { if (onSelectEffect) onSelectEffect(-1); return; }
        if (row.button->getToggleState()) updateSelection(true);
        else if (node >= module_order::firstInsert) { if (onSelectEffect) onSelectEffect(node - module_order::firstInsert); }
        else row.button->triggerClick();
    }
    int liveSlotForButton(const juce::TextButton* button) const
    {
        for (size_t i = 0; i < insertButtons.size(); ++i)
            if (&insertButtons[i] == button && processor.getInsertEffectType(scope, static_cast<int>(i)) != effects::Type::none)
                return static_cast<int>(i);
        return -1;
    }
    void noteHover(const juce::MouseEvent& event)
    {
        if (dragNode >= 0) return;
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
        if (dragNode >= 0)
        {
            const auto row = rowForNode(dragNode);
            if (row.button) row.button->setAlpha(1);
            if (row.power) row.power->setAlpha(1);
        }
        dragNode = -1; dragTarget = -1; scrollRemainder = 0;
        content.repaint();
    }
    void cancelRowInteractions()
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        cancelDrag(); hoveredSlot = -1;
        for (auto row : builtins)
            if (auto* button = dynamic_cast<ModuleDragButton*>(row.button))
            {button->cancelRowPointer(false); if (! safe) return;}
        for (auto& button : insertButtons) {button.cancelRowPointer(false); if (! safe) return;}
        for (auto& button : removeButtons) {button.setPresented(false, false); if (! safe) return;}
    }
    void updateDrag(int node, const juce::MouseEvent& event)
    {
        const auto row = rowForNode(node);
        if (! isShowing() || ! isEnabled() || ! row.button || nodeForButton(row.button) != node) return;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        if (dragNode < 0)
        {
            if (visibleRows.size() < 2) return;
            dragNode = node; dragGeneration = generation;
            dragOffset = juce::jlimit(0.0f, static_cast<float>(rowPitch), static_cast<float>(event.getMouseDownY()));
            row.button->setAlpha(0.32f);
            if (row.power) row.power->setAlpha(0.32f);
            for (auto& button : removeButtons) {button.setPresented(false, false); if (! safe) return;}
        }
        if (dragNode != node || dragGeneration != generation) return;
        dragViewPoint = event.getEventRelativeTo(&viewport).position;
        updateDropTarget(); content.repaint();
    }
    void updateDropTarget()
    {
        dragTarget = -1;
        if (dragNode < 0 || ! viewport.getLocalBounds().toFloat().contains(dragViewPoint)) return;
        const auto y = dragViewPoint.y + viewport.getViewPositionY();
        int position = 0;
        dropLineY = 0;
        dropBeforeNode = -1;
        for (auto row : visibleRows)
        {
            const auto node = nodeForButton(row.button);
            if (node < 0 || node == dragNode) continue;
            if (y < row.button->getBounds().getCentreY())
            { dropBeforeNode = node; dropLineY = static_cast<float>(row.button->getY()) - 2.0f * scale; break; }
            ++position;
            dropLineY = static_cast<float>(row.button->getBottom()) + 2.0f * scale;
        }
        dragTarget = position;
    }
    void finishDrag(const juce::MouseEvent& event)
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        refresh(); // An intervening preset/order edit invalidates this gesture.
        if (! safe || dragNode < 0 || dragGeneration != generation) return;
        dragViewPoint = event.getEventRelativeTo(&viewport).position;
        updateDropTarget();
        const auto node = dragNode, target = dragTarget, anchor = dropBeforeNode;
        cancelDrag();
        if (target < 0) return;
        processor.moveModuleBefore(scope, node, anchor);
        if (! safe) return;
        refresh();
        if (! safe) return;
        selectNode(node);
    }
    void paintDrag(juce::Graphics& g)
    {
        if (dragNode < 0) return;
        const auto view = viewport.getViewArea().toFloat();
        const auto margin = 4.0f * scale;
        const auto height = static_cast<float>(rowPitch) - margin;
        auto bounds = juce::Rectangle<float>(margin, juce::jlimit(view.getY(), juce::jmax(view.getY(), view.getBottom() - height),
            dragViewPoint.y + view.getY() - dragOffset), content.getWidth() - margin * 2, height);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.translated(0, 3 * scale), Metrics::radius * scale);
        g.setColour(colours::raised.brighter(0.08f));
        g.fillRoundedRectangle(bounds, Metrics::radius * scale);
        auto& button = *rowForNode(dragNode).button;
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
        visibleRows.clear();
        for (int slot = 0; slot < effects::slotCount; ++slot)
        {
            const bool visible = processor.getInsertEffectType(scope, slot) != effects::Type::none;
            insertButtons[static_cast<size_t>(slot)].setVisible(visible);
            powerButtons[static_cast<size_t>(slot)].setVisible(visible);
        }
        for (const auto node : processor.getModuleOrder(scope))
        {
            const auto row = rowForNode(node);
            if (row.button && nodeForButton(row.button) == node) visibleRows.push_back(row);
        }
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
            if (auto* button = dynamic_cast<ModuleDragButton*>(visibleRows[i].button)) button->dragThreshold = 6.0f * scale;
            const auto slot = liveSlotForButton(visibleRows[i].button);
            if (slot >= 0)
            {
                auto& button = insertButtons[static_cast<size_t>(slot)];
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
    void showModuleMenu(int node)
    {
        const auto row = rowForNode(node);
        if (! isShowing() || ! isEnabled() || ! row.button || ! row.button->isShowing()
            || nodeForButton(row.button) != node) return;
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr) return;
        juce::PopupMenu menu;
        const bool canMoveUp = ! visibleRows.empty() && visibleRows.front().button != row.button;
        const bool canMoveDown = ! visibleRows.empty() && visibleRows.back().button != row.button;
        menu.addItem(1, "Move up", canMoveUp); menu.addItem(2, "Move down", canMoveDown);
        if (node >= module_order::firstInsert) {menu.addSeparator(); menu.addItem(3, "Remove effect");}
        const auto epoch = generation;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        menu.showMenuAsync(prepareContextMenu(menu, *row.button, juce::Desktop::getMousePosition())
                              .withMinimumWidth(juce::roundToInt(170.0f * scale))
                              .withStandardItemHeight(juce::roundToInt(29.0f * scale)), [safe, epoch, node](int result) {
            if (! safe || safe->generation != epoch || ! safe->isShowing() || ! safe->isEnabled()) return;
            if (result == 3 && node >= module_order::firstInsert) safe->processor.removeInsertEffect(safe->scope, node - module_order::firstInsert);
            else if (result == 1 || result == 2) safe->processor.moveModuleBy(safe->scope, node, result == 1 ? -1 : 1);
            if (safe) safe->refresh();
        });
    }
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    FireAudioProcessor& processor;
    juce::Viewport viewport;
    Content content;
    PrimaryTextButton addButton;
    std::array<ModuleDragButton, effects::slotCount> insertButtons;
    std::array<PrimaryToggleButton, effects::slotCount> powerButtons;
    std::array<CloseButton, effects::slotCount> removeButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, effects::slotCount> attachments;
    std::vector<Row> builtins, visibleRows;
    std::array<int, effects::slotCount> cachedTypes {};
    module_order::Order cachedModuleOrder {};
    SpringValue selectionY;
    int scope = -1, selectedSlot = -1, rowPitch = 1;
    bool selectionInitialised = false;
    float scale = 1;
    std::uint64_t generation = 0, addMenuGeneration = 0;
    int hoveredSlot = -1, dragNode = -1, dragTarget = -1, dropBeforeNode = -1;
    std::uint64_t dragGeneration = 0;
    juce::Point<float> dragViewPoint;
    float dragOffset = 0, dropLineY = 0, scrollRemainder = 0;
};
}
