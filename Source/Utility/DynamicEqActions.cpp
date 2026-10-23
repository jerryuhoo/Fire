#include "../PluginProcessor.h"

fire::dynamic_eq::Configuration FireAudioProcessor::readDynamicEqConfiguration(
    const DynamicEqParameterCache& cache) noexcept
{
    using namespace fire::dynamic_eq;
    return {loadCachedParameter(cache[enabled]) > .5f,
        loadCachedParameter(cache[threshold], defaults[threshold]),
        loadCachedParameter(cache[ratio], defaults[ratio]),
        loadCachedParameter(cache[range], defaults[range]),
        loadCachedParameter(cache[attack], defaults[attack]),
        loadCachedParameter(cache[release], defaults[release])};
}

void FireAudioProcessor::resetDynamicEqNodeParameters(int node, int scope, int slot)
{
    for (int field = 0; field < fire::dynamic_eq::fieldCount; ++field)
        if (auto* parameter = treeState.getParameter(fire::dynamic_eq::parameterID(scope, slot, node, field)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
}

void FireAudioProcessor::resetDynamicEqMeters() noexcept
{
    for (auto& scope : insertOttMeters)
        for (auto& meter : scope)
        {
            meter.sequence.store(0, std::memory_order_release);
            meter.valid.store(false, std::memory_order_relaxed);
        }
    for (auto& module : dynamicEqMeters)
        for (auto& meter : module)
        {
            meter.sequence.store(0, std::memory_order_release);
            meter.active.store(false, std::memory_order_relaxed);
        }
}

void FireAudioProcessor::publishDynamicEqMeters() noexcept
{
    const auto sequence = ++dynamicEqRenderSequence;
    const auto publish = [sequence](DynamicEqMeter& target, fire::dynamic_eq::VisualState value)
    {
        target.sequence.store(0, std::memory_order_release);
        target.gainChange.store(value.gainChangeDb, std::memory_order_relaxed);
        target.effectiveGain.store(value.effectiveGainDb, std::memory_order_relaxed);
        target.active.store(value.active, std::memory_order_relaxed);
        target.sequence.store(sequence, std::memory_order_release);
    };
    const auto& filter = activeAudioCallbackParameterSnapshot.globalFilter;
    for (int node = 0; node < fire::eq::maxNodes; ++node)
    {
        auto value = eqProcessor.getDynamicEqVisualState(node);
        if (node == 1 && fire::eq::usesLegacyShape(node, filter.eqNodes[1].state.type))
        {
            value.gainChangeDb = legacyPeakAppliedGainChange;
            value.effectiveGainDb = cachedGlobalFilterSettings.peakGainInDecibels;
            value.active = legacyPeakDetector.isActive();
        }
        value.active = value.active && filter.enabled && filter.eqNodes[static_cast<size_t>(node)].state.present
            && !filter.eqNodes[static_cast<size_t>(node)].state.bypassed;
        publish(dynamicEqMeters[0][static_cast<size_t>(node)], value);
    }
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
    {
        const bool scopeActive = scope == 0 || (scope <= numBands
            && activeMultibandTopologySnapshot.callbackContext.bandParameters[static_cast<size_t>(scope - 1)].isBandEnabled);
        const auto& rack = scope == 0 ? masterInserts : bands[static_cast<size_t>(scope - 1)]->inserts;
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            auto& ottMeter = insertOttMeters[static_cast<size_t>(scope)][static_cast<size_t>(slot)];
            const auto ott = scopeActive ? rack.getOttVisualState(slot) : OttProcessor::VisualState{};
            ottMeter.sequence.store(0, std::memory_order_release);
            ottMeter.inputLevelDb.store(ott.inputLevelDb, std::memory_order_relaxed);
            ottMeter.gainChangeDb.store(ott.gainChangeDb, std::memory_order_relaxed);
            ottMeter.dynamicsActivityDb.store(ott.dynamicsActivityDb, std::memory_order_relaxed);
            ottMeter.typeGeneration.store(ott.typeGeneration, std::memory_order_relaxed);
            ottMeter.valid.store(ott.valid, std::memory_order_relaxed);
            ottMeter.mixModulated.store(ott.mixModulated, std::memory_order_relaxed);
            ottMeter.sequence.store(sequence, std::memory_order_release);
            const auto index = static_cast<size_t>(1 + scope * fire::effects::slotCount + slot);
            for (int node = 0; node < fire::eq::maxNodes; ++node)
                publish(dynamicEqMeters[index][static_cast<size_t>(node)], scopeActive ? rack.getDynamicEqVisualState(slot, node) : fire::dynamic_eq::VisualState{});
        }
    }
}

OttProcessor::VisualState FireAudioProcessor::getInsertOttVisualState(int scope, int slot) const noexcept
{
    if (!juce::isPositiveAndBelow(scope, fire::effects::scopeCount)
        || !juce::isPositiveAndBelow(slot, fire::effects::slotCount) || getBypassedState()) return {};
    const auto scopeIndex = static_cast<size_t>(scope), slotIndex = static_cast<size_t>(slot);
    const auto& epoch = granularSlotGenerations[scopeIndex][slotIndex].value;
    const auto generation = epoch.load(std::memory_order_acquire);
    if (getInsertEffectType(scope, slot) != fire::effects::Type::ott
        || loadCachedParameter(insertParameters[scopeIndex][slotIndex][fire::effects::enabledField]) <= .5f
        || (scope > 0 && (scope > juce::roundToInt(loadCachedParameter(numBandsParameter, 1))
            || loadCachedParameter(bandParameterCache[scopeIndex - 1].enabled) <= .5f))) return {};
    const auto& meter = insertOttMeters[scopeIndex][slotIndex];
    OttProcessor::VisualState result;
    result.renderSequence = meter.sequence.load(std::memory_order_acquire);
    result.inputLevelDb = meter.inputLevelDb.load(std::memory_order_relaxed);
    result.gainChangeDb = meter.gainChangeDb.load(std::memory_order_relaxed);
    result.dynamicsActivityDb = meter.dynamicsActivityDb.load(std::memory_order_relaxed);
    result.typeGeneration = meter.typeGeneration.load(std::memory_order_relaxed);
    result.valid = meter.valid.load(std::memory_order_relaxed);
    result.mixModulated = meter.mixModulated.load(std::memory_order_relaxed);
    if (result.renderSequence == 0 || result.renderSequence != meter.sequence.load(std::memory_order_acquire)
        || result.typeGeneration != generation || generation != epoch.load(std::memory_order_acquire)) return {};
    // A stopped host may not deliver the next block after a Mix change. The
    // graph should still stop reporting gain/activity when Mix is now zero.
    if (!result.mixModulated && loadCachedParameter(insertParameters[scopeIndex][slotIndex][5]) <= 0.0f)
        result.gainChangeDb = result.dynamicsActivityDb = 0.0f;
    return result;
}

fire::dynamic_eq::VisualState FireAudioProcessor::getDynamicEqVisualState(int node, int scope, int slot) const noexcept
{
    if (!fire::eq::validSlot(node)) return {};
    const int index = slot < 0 ? 0 : 1 + scope * fire::effects::slotCount + slot;
    if (slot >= 0 && (!juce::isPositiveAndBelow(scope, fire::effects::scopeCount)
        || !juce::isPositiveAndBelow(slot, fire::effects::slotCount))) return {};
    if (getBypassedState()) return {};
    if (slot >= 0 && scope > 0 && (scope > juce::roundToInt(loadCachedParameter(numBandsParameter, 1))
        || loadCachedParameter(bandParameterCache[static_cast<size_t>(scope - 1)].enabled) <= .5f)) return {};
    // Do not expose a stale node after deletion, type replacement or bypass
    // while the next audio callback is still pending.
    const auto state = getEqNodeState(node, scope, slot);
    if (!state.present || state.bypassed || !fire::dynamic_eq::supports(state.type)) return {};
    if (slot < 0 && (!isModulePresent(0, 0) || loadCachedParameter(filterEnabledParameter) <= .5f)) return {};
    if (slot >= 0 && loadCachedParameter(insertParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)]
        [fire::effects::enabledField]) <= .5f) return {};
    const auto& meter = dynamicEqMeters[static_cast<size_t>(index)][static_cast<size_t>(node)];
    fire::dynamic_eq::VisualState result;
    result.renderSequence = meter.sequence.load(std::memory_order_acquire);
    result.gainChangeDb = meter.gainChange.load(std::memory_order_relaxed);
    result.effectiveGainDb = meter.effectiveGain.load(std::memory_order_relaxed);
    result.active = meter.active.load(std::memory_order_relaxed);
    if (result.renderSequence == 0 || result.renderSequence != meter.sequence.load(std::memory_order_acquire)) return {};
    return result;
}
