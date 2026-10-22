#pragma once
#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "ModuleDragButton.h"
#include "ChainClipboardUI.h"
#include "ContextAwareComboBox.h"
#include "../Utility/ChannelModeParameters.h"
#include <optional>
#include "../Panels/SpectrogramPanel/CloseButton.h"

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct SkinNavigationMotionTestAccess;
#endif

struct ChainClipboardUiTestAccess;
struct ChainChannelModeUiTestAccess;

namespace fire::ui
{
inline juce::Colour effectColour(effects::Type type)
{
    switch (type)
    {
        case effects::Type::chorus: return colours::chorus;
        case effects::Type::flanger: return colours::flanger;
        case effects::Type::phaser: return colours::phaser;
        case effects::Type::chordResonator: return colours::chordResonator;
        case effects::Type::delay: return colours::delay;
        case effects::Type::reverb: return colours::reverb;
        case effects::Type::granular: return colours::granular;
        case effects::Type::lofi: return colours::loFi;
        case effects::Type::drive: return colours::drive;
        case effects::Type::shape: return colours::shape;
        case effects::Type::compressor: return colours::compressor;
        case effects::Type::ott: return colours::ott;
        case effects::Type::stereo: return colours::stereo;
        case effects::Type::eq: return colours::filter;
        case effects::Type::none: case effects::Type::count: return colours::textMuted;
    }
    return colours::textMuted;
}

// Reuses the existing module buttons rather than duplicating their attachments,
// accessibility or selection behaviour. Adding modules extends the content,
// never shrinks existing rows. Line skins cap tall rails at a compact row size.
class EffectRackNavigation final : public juce::Component
{
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct ::SkinNavigationMotionTestAccess;
#endif
    friend struct ::ChainClipboardUiTestAccess;
    friend struct ::ChainChannelModeUiTestAccess;
public:
    enum MenuAction
    {
        moveUp = 1, moveDown = 2, removeEffect = 3,
        copySettings = 10, copyWithRouting = 11, duplicateSettings = 12, pasteAfter = 13,
        pasteCopiedModule = 200,
        copyChainSettings = 301, copyChainWithRouting = 302, pasteChain = 303,
        saveChainSettings = 304, saveChainWithRouting = 305, loadChain = 306
    };
    struct Row { juce::TextButton* button; juce::ToggleButton* power; };
    EffectRackNavigation(FireAudioProcessor& p, int initialScope) : processor(p), content(*this)
    {
        setInterceptsMouseClicks(false, true);
        viewport.setViewedComponent(&content, false);
        content.addMouseListener(this, true);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
        viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, paletteFor(*this).textMuted.withAlpha(0.25f));
        addAndMakeVisible(viewport);
        addAndMakeVisible(addButton);
        addButton.setButtonText("+"); addButton.setTitle("Add effect");
        addButton.setTooltip("Add a DSP module to this chain");
        addButton.setColour(juce::TextButton::buttonColourId, paletteFor(*this).raised);
        addButton.setColour(juce::TextButton::textColourOffId, paletteFor(*this).textSecondary);
        addButton.onClick = [this] { showAddMenu(); };
        addAndMakeVisible(chainButton);
        chainButton.onClick = [this] { showChainMenu(); };
        addAndMakeVisible(channelMode);
        channelMode.addItem("Stereo", 1);
        channelMode.addItem("Mid", 2);
        channelMode.addItem("Side", 3);
        channelMode.setEditableText(false);
        channelMode.setTooltip("Processing channel for this entire effect chain. Stereo processes both channels. "
            "Mid processes the centre; Side processes the stereo difference, preserving the other component. "
            "Side passes mono input through. This does not change the spectrum display.");
        applyChannelColours();
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
            button.setColour(juce::TextButton::textColourOffId, paletteFor(*this).textSecondary);
            button.setColour(juce::TextButton::textColourOnId, paletteFor(*this).textPrimary);
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
        for (int node = 0; node < 5; ++node)
        {
            auto& remove = legacyRemoveButtons[static_cast<size_t>(node)];
            content.addChildComponent(remove);
            remove.onClick = [this, node]
            {
                if (dragNode >= 0 || ! isShowing() || ! isEnabled()) return;
                const juce::Component::SafePointer<EffectRackNavigation> safe(this);
                processor.removeModule(scope, node);
                if (safe) refresh();
            };
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
        channelAttachment.reset();
        const auto channelID = channel_mode::parameterID(scope);
        channelMode.setComponentID(channelID);
        channelMode.setTitle((scope == 0 ? "Master" : "Band " + juce::String(scope)) + " processing channel");
        channelMode.setDescription("Choose Stereo, Mid (centre) or Side (stereo difference) processing for this chain");
        channelMode.setHelpText(channelMode.getTooltip());
        channelMode.configurePopupSession(
            [safe] { return safe ? safe->generation + safe->processor.getChainEditRevision() : 0; },
            [safe, nextScope] { return safe && safe->scope == nextScope && safe->isShowing() && safe->isEnabled(); },
            processor.treeState.getParameter(channelID));
        channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.treeState, channelID, channelMode);
        cachedTypes.fill(-1); cachedModuleOrder.fill(-2); cachedPresence.fill(-1);
        addButton.setComponentID(scope == 0 ? "addMasterEffect" : "addBandEffect");
        chainButton.setComponentID(scope == 0 ? "masterChainTools" : "bandChainTools");
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
        for (int node = 0; node < 5; ++node)
            legacyRemoveButtons[static_cast<size_t>(node)].setComponentID(core_modules::presenceID(scope, node) + "Remove");
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
        int selectedNode = selectedSlot >= 0 ? module_order::firstInsert + selectedSlot : -1;
        if (selectedNode < 0)
            for (size_t node = 0; node < builtins.size(); ++node)
                if (builtins[node].button->getToggleState()) {selectedNode = static_cast<int>(node); break;}
        int replacement = -1;
        const bool selectionRemoved = selectedNode >= 0 && ! processor.isModulePresent(scope, selectedNode);
        if (selectionRemoved)
        {
            const auto selected = rowForNode(selectedNode).button;
            const auto previous = std::find_if(visibleRows.begin(), visibleRows.end(), [&](const auto& row) {return row.button == selected;});
            if (previous != visibleRows.end())
            {
                for (auto row = previous + 1; row != visibleRows.end() && replacement < 0; ++row)
                {const int node = nodeForButton(row->button); if (processor.isModulePresent(scope, node)) replacement = node;}
                for (auto row = previous; row != visibleRows.begin() && replacement < 0;)
                {const int node = nodeForButton((--row)->button); if (processor.isModulePresent(scope, node)) replacement = node;}
            }
            if (replacement < 0)
                for (const int node : processor.getModuleOrder(scope))
                    if (processor.isModulePresent(scope, node)) {replacement = node; break;}
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
                insertButtons[i].setButtonText(type == effects::Type::chordResonator ? "Resonator" : effects::name(type));
                insertButtons[i].setTitle(effects::name(type));
                insertButtons[i].setTooltip(juce::String(effects::name(type)) + ". Drag to reorder. Right-click or Shift+F10 for copy, duplicate and paste. " + chainRoutingNote);
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
        for (size_t node = 0; node < builtins.size(); ++node)
        {
            const int present = processor.isModulePresent(scope, static_cast<int>(node)) ? 1 : 0;
            if (cachedPresence[node] != present) changed = true;
            cachedPresence[node] = present;
            auto& remove = legacyRemoveButtons[node];
            const auto description = "Remove " + builtins[node].button->getButtonText()
                + (scope == 0 ? " from Master" : " from Band " + juce::String(scope));
            remove.setTitle(description); remove.setTooltip(description); remove.setDescription(description); remove.setHelpText(description);
            builtins[node].button->setTooltip(builtins[node].button->getButtonText() + ". Drag to reorder. Right-click or Shift+F10 for copy, duplicate and paste. " + chainRoutingNote);
        }
        if (! changed && ! selectionRemoved) return;
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
        chainButton.setBounds(addButton.getX() - side - juce::roundToInt(5.0f * scale), addButton.getY(), side, side);
        // Audio routing lives in the existing header, keeping all module rails
        // aligned with the Mod Forge bank and preserving their five-row pitch.
        const auto modeWidth = juce::roundToInt(70.0f * scale);
        channelMode.setBounds(chainButton.getX() - juce::roundToInt(4.0f * scale) - modeWidth,
                              addButton.getY(), modeWidth, side);
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
        if (!usesNavigationMotion(*this))
        {
            // Skin changes also settle an in-flight selection whose row was
            // removed; drag auto-scroll below remains fully operational.
            if (! selectionY.isSettled())
            {
                selectionY.snapTo(selectionY.target);
                content.repaint();
            }
        }
        else if (selectionY.advance(dt)) content.repaint();
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
        for (const int node : processor.getModuleOrder(scope))
        {
            auto* remove = removeForNode(node);
            const auto row = rowForNode(node);
            if (!remove || !row.button) continue;
            if (remove->advanceAnimation(dt)) remove->repaint();
            if (!safe) return;
            const bool reveal = dragNode < 0 && processor.isModulePresent(scope, node) && isShowing() && isEnabled()
                && (hoveredNode == node || row.button->hasKeyboardFocus(true)
                    || (row.power && row.power->hasKeyboardFocus(true)) || remove->getFocusAnimation() > 0.01f);
            if (remove->isPresented() != reveal) remove->setPresented(reveal);
            if (!safe) return;
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
        chainButton.dismissPointerGesture();
        if (! safe) return;
        channelMode.dismissTransientInteraction();
        if (! safe) return;
        dismissChainFiles();
        resultAlert.close();
        if (! safe) return;
        for (auto& button : insertButtons) {button.dismissPointerGesture(); if (! safe) return;}
        for (auto& button : powerButtons) {button.dismissPointerGesture(); if (! safe) return;}
    }
    int getRowPitch() const { return rowPitch; }
    juce::Viewport& getViewport() { return viewport; }
    int getScope() const noexcept { return scope; }
    static constexpr int builtinMenuItemID(int node) noexcept { return 100 + node; }

    // Reusing a removed historical row retains its permanent automation IDs.
    bool activateBuiltin(int node)
    {
        if (! isShowing() || ! isEnabled() || dragNode >= 0 || ! isAddableBuiltin(node)) return false;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        const auto originalScope = scope;
        if (! processor.restoreLegacyModule(scope, node)) return false;
        if (! safe) return false;
        refresh();
        if (! safe) return false;
        const auto currentEpoch = generation;
        const auto row = builtins[static_cast<size_t>(node)];
        const juce::Component::SafePointer<juce::TextButton> button(row.button);
        const juce::Component::SafePointer<juce::ToggleButton> power(row.power);
        if (! button || ! button->isEnabled() || (power && ! power->isEnabled())) return false;
        const auto stillCurrent = [&]
        {
            return safe && button && safe->generation == currentEpoch && safe->scope == originalScope
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
        const bool room = hasInsertRoom();
        constexpr effects::Type catalog[] {effects::Type::drive, effects::Type::shape, effects::Type::compressor,
            effects::Type::ott, effects::Type::stereo, effects::Type::eq, effects::Type::lofi,
            effects::Type::chorus, effects::Type::flanger, effects::Type::phaser, effects::Type::delay,
            effects::Type::reverb, effects::Type::granular, effects::Type::chordResonator};
        for (const auto type : catalog)
        {
            const int legacy = core_modules::legacyNode(scope, type);
            const bool restore = legacy >= 0 && !processor.isModulePresent(scope, legacy);
            menu.addItem(restore ? builtinMenuItemID(legacy) : static_cast<int>(type), effects::name(type), restore || room);
        }
        if (scope == 0 && !processor.isModulePresent(scope, 2)) menu.addItem(builtinMenuItemID(2), "Analysis");
        if (const auto copied = chain::getClipboard(chain::ClipboardKind::module))
        {
            menu.addSeparator();
            menu.addItem(pasteCopiedModule, "Paste " + copiedModuleName(copied));
        }
        return menu;
    }

    std::function<void(int)> createAddMenuResultHandler()
    {
        ++toolMenuGeneration;
        const auto epoch = generation;
        const auto request = ++addMenuGeneration;
        const auto originalScope = scope;
        const auto stateRevision = processor.getChainEditRevision();
        const auto copied = chain::getClipboard(chain::ClipboardKind::module);
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        return [safe, epoch, request, originalScope, stateRevision, copied](int result)
        {
            if (! safe || safe->generation != epoch || safe->addMenuGeneration != request
                || safe->scope != originalScope || ! safe->isShowing() || ! safe->isEnabled()
                || safe->processor.getChainEditRevision() != stateRevision) return;
            ++safe->addMenuGeneration; // Menu completion is a one-shot action.
            if (result == pasteCopiedModule)
            {
                if (copied) safe->pasteModule(-1, copied);
                return;
            }
            if (result >= builtinMenuItemID(0) && result <= builtinMenuItemID(4))
            {
                safe->activateBuiltin(result - builtinMenuItemID(0));
                return;
            }
            if (result <= 0 || result >= static_cast<int>(effects::Type::count)) return;
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
    juce::PopupMenu createModuleMenu(int node)
    {
        juce::PopupMenu menu;
        const auto row = rowForNode(node);
        const bool realModule = processor.isModulePresent(scope, node)
            && !(scope == 0 && node == 2);
        menu.addItem(copySettings, "Copy settings", realModule);
        menu.addItem(copyWithRouting, "Copy with routing", realModule);
        menu.addItem(duplicateSettings, "Duplicate settings", realModule);
        const auto copied = chain::getClipboard(chain::ClipboardKind::module);
        menu.addItem(pasteAfter, copied ? "Paste " + copiedModuleName(copied) + " after" : "Paste after", copied != nullptr);
        menu.addSeparator();
        menu.addItem(moveUp, "Move up", row.button != nullptr && !visibleRows.empty() && visibleRows.front().button != row.button);
        menu.addItem(moveDown, "Move down", row.button != nullptr && !visibleRows.empty() && visibleRows.back().button != row.button);
        menu.addSeparator();
        menu.addItem(removeEffect, "Remove effect", processor.isModulePresent(scope, node));
        menu.addSectionHeader("Routing uses destination sources");
        return menu;
    }

    std::function<void(int)> createModuleMenuResultHandler(int node)
    {
        ++addMenuGeneration;
        const auto epoch = generation;
        const auto request = ++toolMenuGeneration;
        const auto stateRevision = processor.getChainEditRevision();
        const auto originalScope = scope;
        const auto copied = chain::getClipboard(chain::ClipboardKind::module);
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        return [safe, epoch, request, stateRevision, originalScope, node, copied](int result)
        {
            if (!safe || safe->generation != epoch || safe->toolMenuGeneration != request
                || safe->scope != originalScope || !safe->isShowing() || !safe->isEnabled()
                || safe->processor.getChainEditRevision() != stateRevision
                || !safe->processor.isModulePresent(originalScope, node)) return;
            ++safe->toolMenuGeneration;
            if (result == copySettings || result == copyWithRouting || result == duplicateSettings)
            {
                chain::Snapshot snapshot;
                auto status = safe->processor.captureModuleSnapshot(originalScope, node, result == copyWithRouting, snapshot);
                if (!safe) return;
                if (status.failed()) { safe->showChainResult(status); return; }
                if (result == duplicateSettings)
                {
                    safe->pasteModule(node, std::make_shared<const chain::Snapshot>(std::move(snapshot)));
                    return;
                }
                safe->showChainResult(chain::setClipboard(chain::ClipboardKind::module, snapshot));
                return;
            }
            if (result == pasteAfter) { if (copied) safe->pasteModule(node, copied); return; }
            if (result == removeEffect) safe->processor.removeModule(originalScope, node);
            else if (result == moveUp || result == moveDown)
                safe->processor.moveModuleBy(originalScope, node, result == moveUp ? -1 : 1);
            if (safe && safe->scope == originalScope) safe->refresh();
        };
    }

    juce::PopupMenu createChainMenu() const
    {
        juce::PopupMenu menu;
        menu.addItem(copyChainSettings, "Copy chain settings");
        menu.addItem(copyChainWithRouting, "Copy chain with routing");
        const auto copied = chain::getClipboard(chain::ClipboardKind::chain);
        menu.addItem(pasteChain, copied
            ? "Paste chain — replace with " + juce::String(static_cast<int>(copied->modules.size())) + " modules"
            : "Paste chain — replace current", copied != nullptr);
        menu.addSeparator();
        menu.addItem(saveChainSettings, "Save chain preset...");
        menu.addItem(saveChainWithRouting, "Save chain preset with routing...");
        menu.addItem(loadChain, "Load chain preset — replace...");
        menu.addSectionHeader("Routing uses destination sources");
        return menu;
    }

    std::function<void(int)> createChainMenuResultHandler()
    {
        ++addMenuGeneration;
        const auto epoch = generation;
        const auto request = ++toolMenuGeneration;
        const auto stateRevision = processor.getChainEditRevision();
        const auto originalScope = scope;
        const auto copied = chain::getClipboard(chain::ClipboardKind::chain);
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        return [safe, epoch, request, stateRevision, originalScope, copied](int result)
        {
            if (!safe || safe->generation != epoch || safe->toolMenuGeneration != request
                || safe->scope != originalScope || !safe->isShowing() || !safe->isEnabled()
                || safe->processor.getChainEditRevision() != stateRevision) return;
            ++safe->toolMenuGeneration;
            if (result == copyChainSettings || result == copyChainWithRouting)
            {
                chain::Snapshot snapshot;
                auto status = safe->processor.captureChainSnapshot(originalScope, result == copyChainWithRouting, snapshot);
                if (!safe) return;
                if (status.wasOk()) status = chain::setClipboard(chain::ClipboardKind::chain, snapshot);
                if (safe) safe->showChainResult(status);
            }
            else if (result == pasteChain) { if (copied) safe->replaceChain(copied); }
            else if (result == saveChainSettings || result == saveChainWithRouting)
                safe->chooseChainFile(true, result == saveChainWithRouting);
            else if (result == loadChain) safe->chooseChainFile(false, false);
        };
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (!key.isKeyCode(juce::KeyPress::F10Key) || !key.getModifiers().isShiftDown()) return false;
        for (auto row : visibleRows)
            if (row.button->hasKeyboardFocus(true) || (row.power && row.power->hasKeyboardFocus(true)))
            {
                showModuleMenu(nodeForButton(row.button));
                return true;
            }
        if (chainButton.hasKeyboardFocus(true)) { showChainMenu(); return true; }
        return false;
    }

    void showChainMenu()
    {
        if (!isShowing() || !isEnabled() || chainFileActive) return;
        auto menu = createChainMenu();
        menu.showMenuAsync(prepareContextMenu(menu, chainButton, chainButton.getScreenBounds().getBottomLeft())
            .withMinimumWidth(juce::roundToInt(280.0f * scale))
            .withStandardItemHeight(juce::roundToInt(29.0f * scale)), createChainMenuResultHandler());
    }
private:
    struct ChainFileContext
    {
        std::uint64_t epoch = 0, request = 0, stateRevision = 0;
        int scope = -1;
        bool saving = false;
        std::shared_ptr<const chain::Snapshot> snapshot;
    };
    void showChainResult(const juce::Result& result)
    {
        lastChainResult = result.getErrorMessage();
        if (result.wasOk()) return;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (suppressResultAlertsForTesting) return;
#endif
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        resultAlert.close();
        if (!safe || !safe->isShowing()) return;
        auto alert = juce::AlertWindow::showScopedAsync(juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Chain tools").withMessage(result.getErrorMessage())
            .withButton("OK").withAssociatedComponent(this).withParentComponent(getTopLevelComponent()), {});
        if (safe && safe->isShowing()) safe->resultAlert = std::move(alert);
    }
    void pasteModule(int afterNode, const std::shared_ptr<const chain::Snapshot>& snapshot)
    {
        if (!snapshot) return;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        const int destination = scope;
        int added = -1;
        const auto status = processor.pasteModuleSnapshot(destination, afterNode, *snapshot, added);
        if (!safe || safe->scope != destination) return;
        safe->showChainResult(status);
        if (!safe || status.failed()) return;
        safe->refresh();
        if (safe && safe->scope == destination) safe->selectNode(added);
    }
    void replaceChain(const std::shared_ptr<const chain::Snapshot>& snapshot)
    {
        if (!snapshot) return;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        const int destination = scope;
        const auto status = processor.pasteChainSnapshot(destination, *snapshot);
        if (!safe || safe->scope != destination) return;
        safe->showChainResult(status);
        if (!safe || status.failed()) return;
        safe->refresh();
        if (!safe || safe->scope != destination) return;
        for (const auto node : safe->processor.getModuleOrder(destination))
            if (safe->processor.isModulePresent(destination, node))
            { safe->selectNode(node); return; }
    }
    void dismissChainFiles()
    {
        ++chainFileGeneration;
        chainFileActive = false;
        chainFileChooser.reset();
        overwriteAlert.close();
    }
    bool isChainFileCurrent(const ChainFileContext& context) const
    {
        return chainFileActive && chainFileGeneration == context.request
            && generation == context.epoch && scope == context.scope
            && processor.getChainEditRevision() == context.stateRevision
            && isShowing() && isEnabled();
    }
    std::optional<ChainFileContext> prepareChainFile(bool saving, bool includeRouting)
    {
        if (chainFileActive || !isShowing() || !isEnabled()) return std::nullopt;
        ++addMenuGeneration;
        ++toolMenuGeneration;
        ChainFileContext context;
        context.epoch = generation;
        context.request = ++chainFileGeneration;
        context.stateRevision = processor.getChainEditRevision();
        context.scope = scope;
        context.saving = saving;
        if (saving)
        {
            chain::Snapshot snapshot;
            const auto status = processor.captureChainSnapshot(scope, includeRouting, snapshot);
            if (status.failed()) { showChainResult(status); return std::nullopt; }
            context.snapshot = std::make_shared<const chain::Snapshot>(std::move(snapshot));
        }
        chainFileActive = true;
        return context;
    }
    void chooseChainFile(bool saving, bool includeRouting)
    {
        const auto request = prepareChainFile(saving, includeRouting);
        if (!request) return;
        const auto directory = processor.statePresets.getFile().getChildFile("Chains");
        if (directory.createDirectory().failed())
        {
            dismissChainFiles();
            showChainResult(juce::Result::fail("The chain preset folder could not be created."));
            return;
        }
        auto chooser = std::make_shared<juce::FileChooser>(saving ? "Save chain preset" : "Load and replace chain",
            saving ? directory.getChildFile("Untitled.firechain") : directory, "*.firechain", true, false, this);
        chainFileChooser = chooser;
        auto completion = createChainFileResultHandler(*request, chooser);
        chooser->launchAsync((saving ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode)
            | juce::FileBrowserComponent::canSelectFiles,
            [completion = std::move(completion)](const juce::FileChooser& completed) { completion(completed.getResult()); });
    }
    std::function<void(const juce::File&)> createChainFileResultHandler(
        ChainFileContext context, std::weak_ptr<juce::FileChooser> expectedChooser)
    {
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        return [safe, context = std::move(context), expectedChooser = std::move(expectedChooser)](const juce::File& chosen)
        {
            auto keepAlive = expectedChooser.lock();
            if (!keepAlive) return;
            const juce::ScopeGuard deferRelease {[keepAlive] { juce::MessageManager::callAsync([keepAlive] {}); }};
            if (!safe || safe->chainFileChooser != keepAlive || !safe->isChainFileCurrent(context))
            {
                // Consume only this stale request; a newer chooser owns its own lifetime.
                if (safe && safe->chainFileGeneration == context.request) safe->dismissChainFiles();
                return;
            }
            safe->chainFileChooser.reset();
            safe->handleChainFile(context, chosen);
        };
    }
    void handleChainFile(const ChainFileContext& context, const juce::File& chosen)
    {
        if (!isChainFileCurrent(context)) return;
        if (chosen == juce::File()) { dismissChainFiles(); return; }
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        if (context.saving)
        {
            const auto finalPath = chosen.hasFileExtension("firechain") ? chosen
                : juce::File(chosen.getFullPathName() + ".firechain");
            if (finalPath.existsAsFile())
            {
                auto alert = juce::AlertWindow::showScopedAsync(juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Replace chain preset?")
                    .withMessage("\"" + finalPath.getFileName() + "\" already exists. Replace its contents?")
                    .withButton("Replace").withButton("Cancel")
                    .withAssociatedComponent(this).withParentComponent(getTopLevelComponent()),
                    [safe, context, finalPath](int result)
                    {
                        if (!safe) return;
                        if (!safe->isChainFileCurrent(context))
                        {
                            if (safe->chainFileGeneration == context.request) safe->dismissChainFiles();
                            return;
                        }
                        if (result == 1) safe->writeChainFile(context, finalPath);
                        else safe->dismissChainFiles();
                    });
                if (safe && safe->isChainFileCurrent(context)) safe->overwriteAlert = std::move(alert);
                return;
            }
            writeChainFile(context, finalPath);
            return;
        }
        chain::Snapshot snapshot;
        const auto status = chain::read(chosen, snapshot);
        if (!safe || !safe->isChainFileCurrent(context)) return;
        safe->dismissChainFiles();
        if (!safe) return;
        if (status.failed()) { safe->showChainResult(status); return; }
        safe->replaceChain(std::make_shared<const chain::Snapshot>(std::move(snapshot)));
    }
    void writeChainFile(const ChainFileContext& context, const juce::File& file)
    {
        if (!isChainFileCurrent(context) || !context.snapshot) return;
        const auto snapshot = context.snapshot;
        const juce::Component::SafePointer<EffectRackNavigation> safe(this);
        dismissChainFiles();
        if (!safe) return;
        const auto status = chain::write(file, *snapshot);
        if (safe) safe->showChainResult(status);
    }
    bool isAddableBuiltin(int node) const noexcept
    {
        const int count = scope == 0 ? 3 : 5;
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
            if (isLineSkin(owner)) return;
            if (! owner.selectionInitialised || ! owner.selectedButton()) return;
            const auto margin = juce::jmax(1.0f, 2.0f * owner.scale);
            auto bounds = juce::Rectangle<float>(margin, owner.selectionY.current, getWidth() - margin * 2,
                                                 owner.rowPitch - margin * 2);
            g.setColour(paletteFor(*this).raised);
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
    CloseButton* removeForNode(int node)
    {
        if (core_modules::validLegacy(scope, node)) return &legacyRemoveButtons[static_cast<size_t>(node)];
        if (node >= module_order::firstInsert && node < module_order::capacity) return &removeButtons[static_cast<size_t>(node - module_order::firstInsert)];
        return nullptr;
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
        hoveredNode = -1;
        if (viewport.getLocalBounds().toFloat().contains(point))
        {
            const auto inContent = point + viewport.getViewPosition().toFloat();
            for (auto row : visibleRows)
                if (row.button->getBounds().toFloat().contains(inContent))
                { hoveredNode = nodeForButton(row.button); break; }
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
        cancelDrag(); hoveredNode = -1;
        for (auto row : builtins)
            if (auto* button = dynamic_cast<ModuleDragButton*>(row.button))
            {button->cancelRowPointer(false); if (! safe) return;}
        for (auto& button : insertButtons) {button.cancelRowPointer(false); if (! safe) return;}
        for (auto& button : removeButtons) {button.setPresented(false, false); if (! safe) return;}
        for (auto& button : legacyRemoveButtons) {button.setPresented(false, false); if (! safe) return;}
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
        for (auto& button : legacyRemoveButtons) {button.setPresented(false, false); if (! safe) return;}
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
        g.setColour(paletteFor(*this).raised.brighter(0.08f));
        g.fillRoundedRectangle(bounds, Metrics::radius * scale);
        auto& button = *rowForNode(dragNode).button;
        g.setFont(getLookAndFeel().getTextButtonFont(button, rowPitch));
        g.setColour(paletteFor(*this).textPrimary);
        g.drawFittedText(button.getButtonText(), bounds.reduced(12 * scale, 0).toNearestInt(), juce::Justification::centredLeft, 1);
        if (dragTarget >= 0)
        {
            g.setColour(paletteFor(*this).textPrimary.withAlpha(0.85f));
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
        for (size_t node = 0; node < builtins.size(); ++node)
        {
            const bool visible = processor.isModulePresent(scope, static_cast<int>(node));
            builtins[node].button->setVisible(visible);
            if (builtins[node].power) builtins[node].power->setVisible(visible);
        }
        for (int slot = 0; slot < effects::slotCount; ++slot)
        {
            const bool visible = processor.getInsertEffectType(scope, slot) != effects::Type::none;
            insertButtons[static_cast<size_t>(slot)].setVisible(visible);
            powerButtons[static_cast<size_t>(slot)].setVisible(visible);
        }
        for (const auto node : processor.getModuleOrder(scope))
        {
            const auto row = rowForNode(node);
            if (row.button && processor.isModulePresent(scope, node)) visibleRows.push_back(row);
        }
        layoutRows();
    }
    void layoutRows()
    {
        rowPitch = juce::jmax(1, viewport.getHeight() / 5);
        if (isLineSkin(*this))
            rowPitch = juce::jmin(rowPitch, juce::jmax(1, juce::roundToInt(44.0f * scale)));
        const auto width = juce::jmax(1, viewport.getWidth() - juce::roundToInt(6.0f * scale));
        content.setSize(width, juce::jmax(viewport.getHeight(), rowPitch * static_cast<int>(visibleRows.size())));
        const auto margin = juce::jmax(1, juce::roundToInt(2.0f * scale));
        for (size_t i = 0; i < visibleRows.size(); ++i)
        {
            const auto row = juce::Rectangle<int>(0, static_cast<int>(i) * rowPitch, width, rowPitch).reduced(margin);
            visibleRows[i].button->setBounds(row);
            if (auto* button = dynamic_cast<ModuleDragButton*>(visibleRows[i].button)) button->dragThreshold = 6.0f * scale;
            if (auto* remove = removeForNode(nodeForButton(visibleRows[i].button)))
            {
                const auto side = juce::jmin(row.getHeight(), juce::roundToInt(24.0f * scale));
                visibleRows[i].button->getProperties().set("fireModuleTrailingSpace", side + 5.0f * scale);
                remove->setBounds(row.getRight() - side - juce::roundToInt(3 * scale), row.getCentreY() - side / 2, side, side);
                remove->toFront(false);
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
        if (! selectionInitialised || ! isShowing() || !usesNavigationMotion(*this))
        {
            const auto previousY = selectionY.current;
            selectionY.snapTo(y);
            if (previousY != selectionY.current) content.repaint();
        }
        else selectionY.setTarget(y);
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
        auto menu = createModuleMenu(node);
        auto completion = createModuleMenuResultHandler(node);
        menu.showMenuAsync(prepareContextMenu(menu, *row.button, juce::Desktop::getMousePosition())
                              .withMinimumWidth(juce::roundToInt(240.0f * scale))
                              .withStandardItemHeight(juce::roundToInt(29.0f * scale)), std::move(completion));
    }
    void lookAndFeelChanged() override
    {
        applyChannelColours();
        // Switching back from a compact skin must also restore the classic
        // five-row geometry without requiring the host to resize its window.
        layoutRows();
        if (usesNavigationMotion(*this)) return;
        updateSelection(false);
        selectionY.snapTo(selectionY.target);
        content.repaint();
    }
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    void applyChannelColours()
    {
        const auto& palette = paletteFor(*this);
        channelMode.setColour(juce::ComboBox::backgroundColourId, palette.surface0);
        channelMode.setColour(juce::ComboBox::textColourId, palette.textPrimary);
        channelMode.setColour(juce::ComboBox::outlineColourId, palette.hairline);
        channelMode.setColour(juce::ComboBox::arrowColourId, palette.textSecondary);
        channelMode.setColour(juce::ComboBox::focusedOutlineColourId, palette.accent);
    }
    FireAudioProcessor& processor;
    juce::Viewport viewport;
    Content content;
    PrimaryTextButton addButton;
    ChainMenuButton chainButton;
    ContextAwareComboBox channelMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;
    std::array<ModuleDragButton, effects::slotCount> insertButtons;
    std::array<PrimaryToggleButton, effects::slotCount> powerButtons;
    std::array<CloseButton, effects::slotCount> removeButtons;
    std::array<CloseButton, 5> legacyRemoveButtons;
    std::array<int, 5> cachedPresence {-1, -1, -1, -1, -1};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, effects::slotCount> attachments;
    std::vector<Row> builtins, visibleRows;
    std::array<int, effects::slotCount> cachedTypes {};
    module_order::Order cachedModuleOrder {};
    SpringValue selectionY;
    int scope = -1, selectedSlot = -1, rowPitch = 1;
    bool selectionInitialised = false;
    float scale = 1;
    std::uint64_t generation = 0, addMenuGeneration = 0, toolMenuGeneration = 0;
    std::shared_ptr<juce::FileChooser> chainFileChooser;
    juce::ScopedMessageBox resultAlert, overwriteAlert;
    std::uint64_t chainFileGeneration = 0;
    bool chainFileActive = false;
    juce::String lastChainResult;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    bool suppressResultAlertsForTesting = false;
#endif
    int hoveredNode = -1, dragNode = -1, dragTarget = -1, dropBeforeNode = -1;
    std::uint64_t dragGeneration = 0;
    juce::Point<float> dragViewPoint;
    float dragOffset = 0, dropLineY = 0, scrollRemainder = 0;
};
}
