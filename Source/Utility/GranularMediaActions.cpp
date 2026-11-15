#include "../PluginProcessor.h"
#include "EditHistory.h"

namespace
{
bool validGranularSlot(int scope, int slot) noexcept
{
    return juce::isPositiveAndBelow(scope, fire::effects::scopeCount)
        && juce::isPositiveAndBelow(slot, fire::effects::slotCount);
}
juce::Result staleGranularTarget()
{
    return juce::Result::fail("The Granular module or sound changed. Reopen Sample and try again.");
}
}

void FireAudioProcessor::initialiseGranularSlotGenerations()
{
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            auto& listener = granularSlotGenerations[static_cast<size_t>(scope)][static_cast<size_t>(slot)];
            listener.parameters = {
                treeState.getParameter(fire::effects::parameterID(scope, slot, fire::effects::typeField)),
                treeState.getParameter(fire::core_modules::parameterID(scope, slot, fire::core_modules::typeField)),
                treeState.getParameter(fire::modulation_fx::parameterID(scope, slot)),
                treeState.getParameter(fire::resonator_params::parameterID(scope, slot))
            };
            for (auto* parameter : listener.parameters)
                if (parameter != nullptr) parameter->addListener(&listener);
        }
}

void FireAudioProcessor::detachGranularSlotGenerations()
{
    for (auto& scope : granularSlotGenerations)
        for (auto& listener : scope)
            for (auto*& parameter : listener.parameters)
                if (parameter != nullptr)
                {
                    parameter->removeListener(&listener);
                    parameter = nullptr;
                }
}

std::optional<FireAudioProcessor::GranularAudioTarget>
FireAudioProcessor::getGranularAudioTarget(int scope, int slot) const
{
    if (!validGranularSlot(scope, slot)) return std::nullopt;
    GranularAudioTarget target { scope, slot, getChainEditRevision(),
        granularSlotGenerations[static_cast<size_t>(scope)][static_cast<size_t>(slot)].value.load(std::memory_order_acquire) };
    return isGranularAudioTargetCurrent(target) ? std::optional{target} : std::nullopt;
}

bool FireAudioProcessor::isGranularAudioTargetCurrent(const GranularAudioTarget& target) const noexcept
{
    if (!validGranularSlot(target.scope, target.slot) || (target.revision & 1u) != 0u) return false;
    const auto& generation = granularSlotGenerations[static_cast<size_t>(target.scope)][static_cast<size_t>(target.slot)].value;
    return getChainEditRevision() == target.revision
        && generation.load(std::memory_order_acquire) == target.slotGeneration
        && getInsertEffectType(target.scope, target.slot) == fire::effects::Type::granular
        && generation.load(std::memory_order_acquire) == target.slotGeneration
        && getChainEditRevision() == target.revision;
}

juce::Result FireAudioProcessor::applyImportedGranularAudio(
    const GranularAudioTarget& target, const fire::effects::FrozenRecordingPtr& recording)
{
    if (recording == nullptr || !recording->isValid())
        return juce::Result::fail("The imported recording is empty or invalid.");
    {
        const juce::ScopedLock lock(multibandTopologyWriterLock);
        if (!isGranularAudioTargetCurrent(target)) return staleGranularTarget();
        auto* freeze = treeState.getParameter(fire::clouds_params::parameterID(
            target.scope, target.slot, fire::clouds_params::freezeField));
        if (freeze == nullptr) return juce::Result::fail("The Freeze parameter is unavailable.");
        beginMultibandTopologyEdit();
        const juce::ScopeGuard publish { [this] { finishMainStateEdit(false); } };
        const auto stillOwnsSlot = [&]
        {
            return !mainStateEditRequiresDspReset
                && getChainEditRevision() == target.revision + 1u
                && granularSlotGenerations[static_cast<size_t>(target.scope)][static_cast<size_t>(target.slot)].value.load(std::memory_order_acquire) == target.slotGeneration
                && getInsertEffectType(target.scope, target.slot) == fire::effects::Type::granular;
        };
        freeze->beginChangeGesture();
        // Host callbacks can synchronously load a different sound at any of
        // these notifications. That replacement owns its parameters and media.
        if (stillOwnsSlot()) freeze->setValueNotifyingHost(1.0f);
        freeze->endChangeGesture();
        if (!stillOwnsSlot() || freeze->getValue() < 0.5f) return staleGranularTarget();
        auto& rack = target.scope == 0 ? masterInserts : bands[static_cast<size_t>(target.scope - 1)]->inserts;
        rack.stageFrozenRecording(target.slot, recording,
            multibandTopologyResetGeneration.load(std::memory_order_seq_cst) + 1u, true);
        // A second import with Freeze already on is still one undoable edit.
        if (editHistory) editHistory->changed();
    }
    sendChangeMessage();
    // Final host notification may destroy the processor; no member access after it.
    lfoDataHasChanged();
    return juce::Result::ok();
}

juce::Result FireAudioProcessor::captureGranularAudioForExport(
    const GranularAudioTarget& target, fire::effects::FrozenRecordingPtr& result) const
{
    result.reset();
    const juce::ScopedLock lock(const_cast<FireAudioProcessor*>(this)->multibandTopologyWriterLock);
    if (!isGranularAudioTargetCurrent(target)) return staleGranularTarget();
    const auto* freeze = treeState.getRawParameterValue(fire::clouds_params::parameterID(
        target.scope, target.slot, fire::clouds_params::freezeField));
    if (freeze == nullptr || freeze->load(std::memory_order_acquire) < 0.5f)
        return juce::Result::fail("Enable Freeze to capture audio before exporting.");
    const auto& rack = target.scope == 0 ? masterInserts : bands[static_cast<size_t>(target.scope - 1)]->inserts;
    auto snapshot = rack.copyFrozenRecording(target.slot);
    if (!isGranularAudioTargetCurrent(target) || freeze->load(std::memory_order_acquire) < 0.5f)
        return staleGranularTarget();
    if (snapshot == nullptr || !snapshot->isValid())
        return juce::Result::fail("There is no frozen audio yet. Record or import a sample first.");
    result = std::move(snapshot);
    return juce::Result::ok();
}
