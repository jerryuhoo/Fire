#include "../PluginProcessor.h"
#include "MidiModulationParameters.h"

std::uint32_t FireAudioProcessor::getLfoSlotGeneration(int index) const noexcept
{ return lfoManager->getLfoSlotGeneration(index); }

bool FireAudioProcessor::triggerLfo(int index, std::uint32_t expectedGeneration) noexcept
{ return lfoManager->requestLfoTrigger(index, expectedGeneration); }

std::uint64_t FireAudioProcessor::beginMidiLearn(int slot)
{
    const juce::ScopedLock lock(multibandTopologyWriterLock);
    midiLearnStartRevision.store(getChainEditRevision(), std::memory_order_release);
    return lfoManager->beginMidiLearn(slot);
}

void FireAudioProcessor::endMidiLearn(std::uint64_t token) noexcept
{ lfoManager->cancelMidiLearn(token); }

bool FireAudioProcessor::isMidiLearnActive(std::uint64_t token) const noexcept
{
    return lfoManager->isMidiLearnActive(token)
        && midiLearnStartRevision.load(std::memory_order_acquire) == getChainEditRevision();
}

bool FireAudioProcessor::applyMidiLearn(std::uint64_t token)
{
    // Re-check under the writer lock: a project/preset restore must not race
    // the UI's polling and apply a captured CC to a different sound.
    const juce::ScopedLock lock(multibandTopologyWriterLock);
    if (!isMidiLearnActive(token)) { endMidiLearn(token); return false; }
    const auto learned = lfoManager->pollMidiLearn(token);
    if (!learned) return false;
    endMidiLearn(token);
    if (!juce::isPositiveAndBelow(learned->slot, fire::midi_mod::count)) return false;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish {[this] { finishMainStateEdit(false); lfoDataHasChanged(); }};
    const auto write = [&](int field, int value)
    {
        if (auto* parameter = treeState.getParameter(fire::midi_mod::parameterID(learned->slot, field)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
            parameter->endChangeGesture();
        }
    };
    write(fire::midi_mod::number, learned->number);
    write(fire::midi_mod::channel, learned->channel);
    lfoManager->stageMidiCCInitialValue(learned->slot, learned->number, learned->channel, learned->value);
    return true;
}

float FireAudioProcessor::getMidiCCValue(int slot) const noexcept
{ return lfoManager->getMidiCCValue(slot); }
