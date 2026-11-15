#include "PresetParameterLocks.h"
#include "../PluginProcessor.h"
#include "EditHistory.h"
#include "ParallelChainParameters.h"
#include <set>

namespace fire::preset_locks
{
namespace
{
const std::set<juce::String>& structuralParameters()
{
    static const auto ids = []
    {
        std::set<juce::String> result{NUM_BANDS_ID, OFF_ID, PRE_ID, POST_ID, LOW_ID, BAND_ID, HIGH_ID};
        for (int divider = 1; divider <= 3; ++divider)
        {
            result.insert(juce::String(FREQ_ID) + juce::String(divider));
            result.insert(juce::String(LINE_STATE_ID) + juce::String(divider));
        }
        for (int source = 0; source < lfo_bank::capacity; ++source) result.insert(lfo_bank::presentParameterID(source));
        for (int node = 0; node < eq::maxNodes; ++node) result.insert(eq::parameterID(node,eq::Field::present));
        for (int scope = 0; scope < effects::scopeCount; ++scope)
        {
            for (int node = 0; node < module_order::capacity; ++node)
            {
                if (module_order::valid(scope,node)) result.insert(module_order::parameterID(scope,node));
                if (core_modules::validLegacy(scope,node)) result.insert(core_modules::presenceID(scope,node));
            }
            for (int slot = 0; slot < effects::slotCount; ++slot)
            {
                result.insert(effects::parameterID(scope,slot,effects::typeField));
                result.insert(effects::parameterID(scope,slot,effects::orderField));
                result.insert(core_modules::parameterID(scope,slot,core_modules::typeField));
                result.insert(modulation_fx::parameterID(scope,slot));
                result.insert(resonator_params::parameterID(scope,slot));
                result.insert(clouds_params::parameterID(scope,slot,clouds_params::engineField));
                for (int node = 0; node < eq::maxNodes; ++node)
                    result.insert(core_modules::eqParameterID(scope,slot,node,eq::Field::present));
            }
        }
        return result;
    }();
    return ids;
}
bool eligible(const juce::RangedAudioParameter& parameter)
{
    return parameter.isAutomatable() && !parameter.isMetaParameter()
        && !structuralParameters().contains(parameter.paramID) && !parallel_chain::isStructuralID(parameter.paramID);
}
}

bool isLockableParameter(const juce::AudioProcessor& processor, const juce::String& id)
{
    if (id.isEmpty() || structuralParameters().contains(id) || parallel_chain::isStructuralID(id)) return false;
    for (const auto* parameter : processor.getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter);
            ranged != nullptr && ranged->paramID == id) return eligible(*ranged);
    return false;
}

std::vector<Entry> enumerate(const juce::AudioProcessor& processor)
{
    std::vector<Entry> result;
    for (const auto* parameter : processor.getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter); ranged && eligible(*ranged))
            result.push_back({ranged->paramID,ranged->getName(128)});
    return result;
}

Values captureValues(const juce::AudioProcessor& processor, const juce::StringArray& locked)
{
    Values values;
    if (locked.isEmpty()) return values;
    for (const auto* parameter : processor.getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter);
            ranged && locked.contains(ranged->paramID) && eligible(*ranged))
            values.emplace(ranged->paramID,ranged->getValue());
    return values;
}

void writePreferences(juce::XmlElement& xml, const juce::StringArray& ids)
{
    juce::Array<juce::var> values;
    for (const auto& id : ids) values.add(id);
    xml.setAttribute(versionAttribute,schemaVersion);
    xml.setAttribute(valuesAttribute,juce::JSON::toString(juce::var(values),true));
}

juce::StringArray readPreferences(const juce::XmlElement* xml, const juce::AudioProcessor& processor)
{
    juce::StringArray result;
    if (!xml || xml->getStringAttribute(versionAttribute) != juce::String(schemaVersion)) return result;
    const auto text = xml->getStringAttribute(valuesAttribute);
    if (text.length() > 2 * 1024 * 1024) return result;
    const auto parsed = juce::JSON::parse(text);
    const auto* values = parsed.getArray();
    if (!values || values->isEmpty() || values->size() > processor.getParameters().size()) return result;
    std::set<juce::String> available;
    for (const auto& entry : enumerate(processor)) available.insert(entry.id);
    for (const auto& value : *values)
        if (value.isString() && available.contains(value.toString())) result.addIfNotAlreadyThere(value.toString());
    result.sort(false);
    return result;
}
}

bool FireAudioProcessor::isPresetParameterLocked(const juce::String& id) const
{ const juce::ScopedLock lock(presetLocksMutex); return presetParameterLocks.contains(id); }

bool FireAudioProcessor::setPresetParameterLocked(const juce::String& id, bool locked)
{
    if (!fire::preset_locks::isLockableParameter(*this,id)) return false;
    {
        const juce::ScopedLock guard(presetLocksMutex);
        if (presetParameterLocks.contains(id) == locked) return false;
        if (locked) { presetParameterLocks.add(id); presetParameterLocks.sort(false); }
        else presetParameterLocks.removeString(id);
    }
    // A project preference, not a sound edit or an undoable parameter gesture.
    if (editHistory) editHistory->ignoreNextHostDisplayChange();
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
    return true;
}

void FireAudioProcessor::clearPresetParameterLocks()
{
    {
        const juce::ScopedLock lock(presetLocksMutex);
        if (presetParameterLocks.isEmpty()) return;
        presetParameterLocks.clear();
    }
    if (editHistory) editHistory->ignoreNextHostDisplayChange();
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
}

juce::StringArray FireAudioProcessor::getPresetParameterLocks() const
{ const juce::ScopedLock lock(presetLocksMutex); return presetParameterLocks; }

std::vector<FireAudioProcessor::LockablePresetParameter> FireAudioProcessor::getLockableParameters() const
{ return fire::preset_locks::enumerate(*this); }
