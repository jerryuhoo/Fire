/*
 ==============================================================================
 
 Preset.cpp
 Created: 12 Jul 2020 9:06:49pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#include "Preset.h"
#include "../../PluginProcessor.h"
#include "../../Utility/StrictNumberParser.h"
#include "../../Utility/LfoBankParameters.h"
#include <cmath>
#include <limits>
#include <utility>

namespace
{
constexpr juce::int64 maximumPresetFileBytes = 4 * 1024 * 1024;
constexpr int maximumPresetFolderDepth = 16;
constexpr int maximumPresetCount = 4096;
constexpr int maximumPresetCandidateCount = maximumPresetCount * 2;
constexpr int maximumPresetDirectoryEntryCount = maximumPresetCount * 8;
constexpr int maximumPresetRawDirectoryEntryCount = maximumPresetCount * 16;
constexpr juce::int64 maximumPresetScanParsedBytes = 64 * 1024 * 1024;

void deleteDialogSynchronously(
    juce::Component::SafePointer<juce::DialogWindow> dialog) noexcept
{
    if (dialog == nullptr)
        return;

    dialog->exitModalState(0);
    dialog.deleteAndZero();
}

bool parseStrictDouble(const juce::String& text, double& result) noexcept
{
    return fire::utility::parseStrictFiniteDouble(text, result);
}

float readNormalisedAttribute(const juce::XmlElement& xml,
                              const juce::String& name,
                              float fallback) noexcept
{
    double parsed = 0.0;
    if (! parseStrictDouble(xml.getStringAttribute(name), parsed))
        return fallback;

    return juce::jlimit(0.0f, 1.0f, static_cast<float>(parsed));
}

bool readStrictIntegerAttribute(const juce::XmlElement& xml,
                                const juce::String& name,
                                int& result) noexcept
{
    double parsed = 0.0;
    if (! xml.hasAttribute(name)
        || ! parseStrictDouble(xml.getStringAttribute(name), parsed)
        || parsed < static_cast<double>(std::numeric_limits<int>::min())
        || parsed > static_cast<double>(std::numeric_limits<int>::max())
        || std::floor(parsed) != parsed)
        return false;

    result = static_cast<int>(parsed);
    return true;
}

bool readSupportedPresetFormatVersion(const juce::XmlElement& xml,
                                      int& version) noexcept
{
    // Alternate snapshots must obey the same version contract as the loader
    // that will consume them when the user switches A/B.
    version = 0;
    return ! xml.hasAttribute("presetFormatVersion")
        || (readStrictIntegerAttribute(xml, "presetFormatVersion", version)
            && version >= 0 && version <= 2);
}

bool isStrictNumberInRange(const juce::XmlElement& xml,
                           const juce::String& name,
                           double minimum,
                           double maximum) noexcept
{
    double parsed = 0.0;
    return xml.hasAttribute(name)
           && parseStrictDouble(xml.getStringAttribute(name), parsed)
           && parsed >= minimum
           && parsed <= maximum;
}

bool isValidLfoState(const juce::XmlElement& lfoState, bool legacyBank) noexcept
{
    const int expectedCount = legacyBank ? fire::lfo_bank::defaultCount
                                         : fire::lfo_bank::capacity;
    if (! lfoState.hasTagName("LFO_STATE")
        || lfoState.getNumChildElements() != expectedCount)
        return false;

    std::array<bool, fire::lfo_bank::capacity> seenIndices {};
    for (auto* lfo : lfoState.getChildIterator())
    {
        int index = -1;
        if (! lfo->hasTagName("LFO")
            || ! readStrictIntegerAttribute(*lfo, "index", index)
            || ! juce::isPositiveAndBelow(index, expectedCount)
            || seenIndices[static_cast<size_t>(index)]
            || lfo->getNumChildElements() > 2)
            return false;

        seenIndices[static_cast<size_t>(index)] = true;
        if (lfo->hasAttribute("smoothness")
            && ! isStrictNumberInRange(*lfo, "smoothness", 0.0, 1.0))
            return false;

        const juce::XmlElement* points = nullptr;
        const juce::XmlElement* curvatures = nullptr;
        for (auto* payload : lfo->getChildIterator())
        {
            if (payload->hasTagName("POINTS") && points == nullptr)
                points = payload;
            else if (payload->hasTagName("CURVATURES") && curvatures == nullptr)
                curvatures = payload;
            else
                return false;
        }

        if (points == nullptr || curvatures == nullptr
            || points->getNumChildElements() < 2
            || points->getNumChildElements() > static_cast<int>(LfoData::maximumNumberOfPoints)
            || curvatures->getNumChildElements() != points->getNumChildElements() - 1)
            return false;

        for (auto* point : points->getChildIterator())
            if (! point->hasTagName("P")
                || ! isStrictNumberInRange(*point, "x", 0.0, 1.0)
                || ! isStrictNumberInRange(*point, "y", 0.0, 1.0))
                return false;

        for (auto* curvature : curvatures->getChildIterator())
            if (! curvature->hasTagName("C")
                || ! isStrictNumberInRange(*curvature, "v", -2.0, 2.0))
                return false;
    }

    return true;
}

bool isValidRoutingState(const juce::XmlElement& routingState,
                         const juce::AudioProcessor& processor,
                         bool legacyBank) noexcept
{
    if (! routingState.hasTagName("MODULATION_STATE")
        || routingState.getNumChildElements()
               > LfoManager::maximumModulationRoutings)
        return false;

    juce::StringArray seenTargets;
    for (auto* routing : routingState.getChildIterator())
    {
        int source = -1;
        if (! routing->hasTagName("ROUTING")
            || ! readStrictIntegerAttribute(*routing, "source", source)
            || ! juce::isPositiveAndBelow(source, legacyBank ? fire::lfo_bank::defaultCount
                                                             : fire::lfo_bank::capacity)
            || ! isStrictNumberInRange(*routing, "depth", -1.0, 1.0)
            || routing->getNumChildElements() != 0)
            return false;

        const auto target = routing->getStringAttribute("target");
        if (target.isEmpty())
            continue; // Empty preallocated routing slots are part of Init.

        bool isKnownTarget = false;
        for (const auto* parameter : processor.getParameters())
            if (const auto* parameterWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
                parameterWithID != nullptr && parameterWithID->paramID == target)
            {
                isKnownTarget = true;
                break;
            }

        if (! isKnownTarget || seenTargets.contains(target))
            return false;
        seenTargets.add(target);
    }

    return true;
}

bool validateParameterFamily(const juce::XmlElement& xml,
                                const juce::AudioProcessor& processor,
                                bool& legacyWithoutOtt,
                                const char* marker = "ottSchemaVersion",
                                bool (*belongs)(const juce::String&) = ParameterIDAndName::isOttParameterID,
                                int maximumVersion = 1)
{
    if (! xml.hasAttribute(marker))
    {
        bool anyPresent = false;
        for (const auto& attribute : xml.getAttributeIterator())
            if (belongs(attribute.name.toString())) { anyPresent = true; break; }
        if (! anyPresent) { legacyWithoutOtt = true; return true; }
    }
    int present = 0;
    int expected = 0;
    for (const auto* parameter : processor.getParameters())
        if (const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
            identified != nullptr && belongs(identified->paramID))
        {
            ++expected;
            if (xml.hasAttribute(identified->paramID))
            {
                if (! isStrictNumberInRange(xml, identified->paramID, 0.0, 1.0))
                    return false;
                ++present;
            }
        }
    legacyWithoutOtt = ! xml.hasAttribute(marker) && present == 0;
    if (legacyWithoutOtt)
        return true;
    if (xml.hasAttribute(marker))
    {
        int version = 0;
        if (! readStrictIntegerAttribute(xml, marker, version) || version < 1 || version > maximumVersion)
            return false;
    }
    return present == expected;
}

bool isValidABSnapshot(const juce::XmlElement& snapshot,
                       const juce::AudioProcessor& processor) noexcept
{
    int formatVersion = 0;
    if (! snapshot.hasTagName("AB_STATE") || snapshot.getNumChildElements() > 2
        || ! readSupportedPresetFormatVersion(snapshot, formatVersion))
        return false;

    bool legacyWithoutOtt = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutOtt))
        return false;
    bool legacyWithoutInserts = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutInserts, "insertEffectsSchemaVersion", fire::effects::isParameterID))
        return false;
    bool legacyWithoutModulationEffects = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutModulationEffects, "modulationEffectsSchemaVersion",
                                  fire::modulation_fx::isParameterID, fire::modulation_fx::schemaVersion))
        return false;
    bool legacyWithoutResonator = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutResonator, "resonatorSchemaVersion",
                                  fire::resonator_params::isParameterID, fire::resonator_params::schemaVersion))
        return false;
    bool legacyWithoutModuleOrder = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutModuleOrder, "moduleOrderSchemaVersion", fire::module_order::isParameterID))
        return false;
    bool legacyWithoutClouds = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutClouds, "cloudsSchemaVersion", fire::clouds_params::isParameterID,
                                  fire::clouds_params::schemaVersion))
        return false;

    bool legacyWithoutEq = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutEq, "eqSchemaVersion", fire::eq::isAppendedParameterID))
        return false;
    bool legacyWithoutLfoBank = false;
    if (! validateParameterFamily(snapshot, processor, legacyWithoutLfoBank, "lfoBankSchemaVersion",
                                  fire::lfo_bank::isAppendedParameterID, fire::lfo_bank::schemaVersion))
        return false;

    int expectedParameterCount = 0;
    for (const auto* parameter : processor.getParameters())
    {
        const auto* parameterWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
        if (parameterWithID == nullptr)
            continue;

        if (legacyWithoutOtt && ParameterIDAndName::isOttParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutInserts && fire::effects::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutModulationEffects && fire::modulation_fx::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutResonator && fire::resonator_params::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutModuleOrder && fire::module_order::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutClouds && fire::clouds_params::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutEq && fire::eq::isAppendedParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutLfoBank && fire::lfo_bank::isAppendedParameterID(parameterWithID->paramID))
            continue;
        ++expectedParameterCount;
        if (! snapshot.hasAttribute(parameterWithID->paramID))
            return false;

        if (! isStrictNumberInRange(snapshot, parameterWithID->paramID, 0.0, 1.0))
            return false;
    }

    if (expectedParameterCount == 0 || snapshot.getNumChildElements() != 2)
        return false;

    bool hasLfoState = false;
    bool hasRoutingState = false;
    for (auto* child : snapshot.getChildIterator())
    {
        if (child->hasTagName("LFO_STATE") && ! hasLfoState)
        {
            hasLfoState = true;
            if (! isValidLfoState(*child, legacyWithoutLfoBank))
                return false;
        }
        else if (child->hasTagName("MODULATION_STATE") && ! hasRoutingState)
        {
            hasRoutingState = true;
            if (! isValidRoutingState(*child, processor, legacyWithoutLfoBank))
                return false;
        }
        else
        {
            return false;
        }
    }

    return hasLfoState && hasRoutingState;
}

bool isLoadablePresetState(const juce::XmlElement& xml,
                           const juce::AudioProcessor& processor) noexcept
{
    bool legacyWithoutOtt = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutOtt))
        return false;
    bool legacyWithoutInserts = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutInserts, "insertEffectsSchemaVersion", fire::effects::isParameterID))
        return false;
    bool legacyWithoutModulationEffects = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutModulationEffects, "modulationEffectsSchemaVersion",
                                  fire::modulation_fx::isParameterID, fire::modulation_fx::schemaVersion))
        return false;
    bool legacyWithoutResonator = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutResonator, "resonatorSchemaVersion",
                                  fire::resonator_params::isParameterID, fire::resonator_params::schemaVersion))
        return false;
    bool legacyWithoutModuleOrder = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutModuleOrder, "moduleOrderSchemaVersion", fire::module_order::isParameterID))
        return false;
    bool legacyWithoutClouds = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutClouds, "cloudsSchemaVersion", fire::clouds_params::isParameterID,
                                  fire::clouds_params::schemaVersion))
        return false;
    bool legacyWithoutEq = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutEq, "eqSchemaVersion", fire::eq::isAppendedParameterID))
        return false;
    bool legacyWithoutLfoBank = false;
    if (! validateParameterFamily(xml, processor, legacyWithoutLfoBank, "lfoBankSchemaVersion",
                                  fire::lfo_bank::isAppendedParameterID, fire::lfo_bank::schemaVersion))
        return false;
    // Unversioned and v1 files predate complete model snapshots. Preserve
    // their historical default/migration behaviour. A v2 document is an
    // explicit complete snapshot, so accepting a sparse or truncated one
    // would silently reset every omitted parameter, LFO, and routing.
    int formatVersion = 0;
    if (! readSupportedPresetFormatVersion(xml, formatVersion))
        return false;

    if (formatVersion < 2)
        return true;

    int parameterCount = 0;
    for (const auto* parameter : processor.getParameters())
    {
        const auto* parameterWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
        if (parameterWithID == nullptr)
            continue;

        if (legacyWithoutOtt && ParameterIDAndName::isOttParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutInserts && fire::effects::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutModulationEffects && fire::modulation_fx::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutResonator && fire::resonator_params::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutModuleOrder && fire::module_order::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutClouds && fire::clouds_params::isParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutEq && fire::eq::isAppendedParameterID(parameterWithID->paramID))
            continue;
        if (legacyWithoutLfoBank && fire::lfo_bank::isAppendedParameterID(parameterWithID->paramID))
            continue;
        ++parameterCount;
        if (! isStrictNumberInRange(xml, parameterWithID->paramID, 0.0, 1.0))
            return false;
    }

    if (parameterCount == 0 || xml.getNumChildElements() != 2)
        return false;

    bool hasLfoState = false;
    bool hasRoutingState = false;
    for (auto* child : xml.getChildIterator())
    {
        if (child->hasTagName("LFO_STATE") && ! hasLfoState)
        {
            hasLfoState = true;
            if (! isValidLfoState(*child, legacyWithoutLfoBank))
                return false;
        }
        else if (child->hasTagName("MODULATION_STATE") && ! hasRoutingState)
        {
            hasRoutingState = true;
            if (! isValidRoutingState(*child, processor, legacyWithoutLfoBank))
                return false;
        }
        else
        {
            return false;
        }
    }

    return hasLfoState && hasRoutingState;
}

bool isSupportedPresetDocument(const juce::XmlElement& xml) noexcept
{
    return xml.hasTagName("WINGSFIRE");
}

void writeSerializablePresetSnapshotToXml(
    const FireAudioProcessor& processor,
    const FireAudioProcessor::SerializablePresetStateSnapshot& snapshot,
    juce::XmlElement& xml)
{
    xml.deleteAllChildElements();
    xml.setAttribute("presetFormatVersion", 2);
    xml.setAttribute("ottSchemaVersion", 1);
    xml.setAttribute("insertEffectsSchemaVersion", 1);
    xml.setAttribute("modulationEffectsSchemaVersion", fire::modulation_fx::schemaVersion);
    xml.setAttribute("resonatorSchemaVersion", fire::resonator_params::schemaVersion);
    xml.setAttribute("moduleOrderSchemaVersion", 1);
    xml.setAttribute("eqSchemaVersion", 1);
    xml.setAttribute("lfoBankSchemaVersion", fire::lfo_bank::schemaVersion);
    xml.setAttribute("cloudsSchemaVersion", fire::clouds_params::schemaVersion);
    xml.setAttribute("pluginVersion", VERSION);

    for (const auto& param : processor.getParameters())
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            float normalisedValue = p->getDefaultValue();
            for (const auto& child : snapshot.parameterState)
            {
                if (child.getProperty("id").toString() != p->paramID
                    || ! child.hasProperty("value"))
                    continue;

                if (auto* ranged = processor.treeState.getParameter(p->paramID))
                {
                    const float plainValue = static_cast<float>(
                        child.getProperty("value"));
                    if (std::isfinite(plainValue))
                    {
                        const auto& range = ranged->getNormalisableRange();
                        normalisedValue = ranged->convertTo0to1(
                            range.snapToLegalValue(juce::jlimit(
                                range.start, range.end, plainValue)));
                    }
                }
                break;
            }
            xml.setAttribute(p->paramID, fire::clouds_params::isReservedEngineParameterID(p->paramID)
                                            ? 1.0f : normalisedValue);
        }

    auto* lfoState = xml.createNewChildElement("LFO_STATE");
    for (int i = 0; i < static_cast<int>(snapshot.lfoData.size()); ++i)
    {
        auto lfoXml = std::make_unique<juce::XmlElement>("LFO");
        lfoXml->setAttribute("index", i);
        snapshot.lfoData[static_cast<size_t>(i)].writeToXml(*lfoXml);
        lfoState->addChildElement(lfoXml.release());
    }

    auto* modMatrixState = xml.createNewChildElement("MODULATION_STATE");
    for (const auto& routing : snapshot.routings)
    {
        if (! routing.targetParameterID.isEmpty())
        {
            auto routingXml = std::make_unique<juce::XmlElement>("ROUTING");
            routing.writeToXml(*routingXml);
            modMatrixState->addChildElement(routingXml.release());
        }
    }
}
} // namespace

namespace state
{
    bool canonicaliseCloudsPresetState(juce::XmlElement& xml)
    {
        const int version = xml.getIntAttribute("cloudsSchemaVersion", 0);
        bool anyLegacyGranularMigrated = false;
        for (int scope = 0; scope < fire::clouds_params::scopeCount; ++scope)
            for (int slot = 0; slot < fire::clouds_params::slotCount; ++slot)
            {
                const auto engineID = fire::clouds_params::parameterID(scope, slot, fire::clouds_params::engineField);
                const auto typeID = fire::effects::parameterID(scope, slot, fire::effects::typeField);
                const auto type = juce::roundToInt(readNormalisedAttribute(xml, typeID, 0.0f) * 5.0f);
                const bool wasLegacy = ! xml.hasAttribute(engineID)
                                    || readNormalisedAttribute(xml, engineID, 0.0f) < 0.5f;
                const bool migratingLegacy = version < fire::clouds_params::schemaVersion && type == 4 && wasLegacy;
                if (migratingLegacy)
                {
                    anyLegacyGranularMigrated = true;
                    std::array<float, 6> values;
                    for (int control = 0; control < 6; ++control)
                        values[static_cast<size_t>(control)] = readNormalisedAttribute(
                            xml, fire::effects::parameterID(scope, slot, control), 0.5f);
                    values = fire::clouds_params::migrateLegacyGranular(values);
                    for (int control = 0; control < 6; ++control)
                        xml.setAttribute(fire::effects::parameterID(scope, slot, control), values[static_cast<size_t>(control)]);
                    // These targets were inaudible in Legacy. Reset their
                    // recipes along with the extension bases, so a latent
                    // full-depth feedback assignment cannot become active.
                    if (auto* routings = xml.getChildByName("MODULATION_STATE"))
                        for (int index = routings->getNumChildElements(); --index >= 0;)
                        {
                            auto* routing = routings->getChildElement(index);
                            if (! routing->hasTagName("ROUTING"))
                                continue;
                            const auto target = routing->getStringAttribute("target");
                            for (int field = fire::clouds_params::spreadField; field < fire::clouds_params::fieldCount; ++field)
                                if (target == fire::clouds_params::parameterID(scope, slot, field))
                                {
                                    routings->removeChildElement(routing, true);
                                    break;
                                }
                        }
                }
                for (int field = 0; field < fire::clouds_params::fieldCount; ++field)
                {
                    const auto id = fire::clouds_params::parameterID(scope, slot, field);
                    if (migratingLegacy || field == fire::clouds_params::engineField || ! xml.hasAttribute(id))
                        xml.setAttribute(id, fire::clouds_params::defaults[static_cast<size_t>(field)]);
                }
            }
        xml.setAttribute("cloudsSchemaVersion", fire::clouds_params::schemaVersion);
        return anyLegacyGranularMigrated;
    }

    //==============================================================================
    void saveStateToXml(const juce::AudioProcessor& proc, juce::XmlElement& xml)
    {
        auto& fireProc = static_cast<const FireAudioProcessor&>(proc);
        auto snapshot = fireProc.captureSerializablePresetStateSnapshot();
        writeSerializablePresetSnapshotToXml(fireProc, snapshot, xml);
    }

    bool canLoadStateFromXml(const juce::XmlElement& xml, const juce::AudioProcessor& processor)
    {
        return isLoadablePresetState(xml, processor);
    }

    bool loadStateFromXml(const juce::XmlElement& incomingXml, juce::AudioProcessor& proc,
                          bool preserveLoudnessComparison)
    {
        if (! canLoadStateFromXml(incomingXml, proc))
            return false;

        juce::XmlElement xml(incomingXml);
        canonicaliseCloudsPresetState(xml);

        auto& fireProc = static_cast<FireAudioProcessor&>(proc);
        {
            fireProc.beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [&fireProc]
            {
                fireProc.requestMultibandTopologyReset();
            } };

            if (! preserveLoudnessComparison)
                fireProc.clearCurrentLoudnessMatch();

            for (const auto& param : proc.getParameters())
            {
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                {
                    float valueToLoad = p->getDefaultValue();
                    if (xml.hasAttribute(p->paramID))
                    {
                        // XmlElement's numeric helpers accept a valid numeric prefix
                        // (for example "0.5oops"). Presets are an external file
                        // format, so only a complete, finite normalised number is
                        // allowed to override the parameter default.
                        valueToLoad = readNormalisedAttribute(xml, p->paramID, p->getDefaultValue());
                    }
                    else if (p->paramID.startsWith(SHAPE_BYPASS_ID))
                    {
                        valueToLoad = 1.0f;
                    }

                    // JUCE's Bool parameter retains a fractional normalised
                    // value even though its APVTS raw value snaps to 0/1.
                    // Canonicalise this new family so Freeze/Engine, the
                    // saved snapshot and preset-equivalence checks agree.
                    if (fire::clouds_params::isParameterID(p->paramID)
                        || fire::resonator_params::isParameterID(p->paramID)
                        || fire::lfo_bank::isPresentParameterID(p->paramID))
                        if (auto* ranged = fireProc.treeState.getParameter(p->paramID))
                            valueToLoad = ranged->convertTo0to1(
                                ranged->convertFrom0to1(valueToLoad));

                    p->setValueNotifyingHost(valueToLoad);
                }
            }

        std::array<LfoData, fire::lfo_bank::capacity> lfoDataToLoad;
        std::array<bool, fire::lfo_bank::capacity> loadedLfoSmoothness {};
        std::array<bool, fire::lfo_bank::capacity> loadedSmoothnessParameter {};
        std::array<bool, fire::lfo_bank::capacity> loadedLfoIndices {};
        for (int i = 0; i < static_cast<int>(loadedSmoothnessParameter.size()); ++i)
            loadedSmoothnessParameter[static_cast<size_t>(i)] = xml.hasAttribute(
                ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i));

        if (auto* lfoState = xml.getChildByName("LFO_STATE"))
        {
            for (auto* lfoXml : lfoState->getChildIterator())
            {
                if (! lfoXml->hasTagName("LFO"))
                    continue;

                const int index = lfoXml->getIntAttribute("index", -1);
                if (juce::isPositiveAndBelow(index, static_cast<int>(lfoDataToLoad.size()))
                    && ! loadedLfoIndices[static_cast<size_t>(index)])
                {
                    lfoDataToLoad[static_cast<size_t>(index)] = LfoData::readFromXml(*lfoXml);
                    loadedLfoSmoothness[static_cast<size_t>(index)] = lfoXml->hasAttribute("smoothness");
                    loadedLfoIndices[static_cast<size_t>(index)] = true;
                }
            }
        }

        // Old preset files may contain APVTS smooth parameters but no matching
        // LFO_STATE smoothness attribute. Preserve that parameter value instead
        // of replacing it with LfoData's default during the shape reset below.
        for (int i = 0; i < static_cast<int>(lfoDataToLoad.size()); ++i)
        {
            const auto index = static_cast<size_t>(i);
            // The parameter attribute is authoritative when present. The LFO
            // XML value is retained only for presets old enough to lack it.
            if (! loadedSmoothnessParameter[index] && loadedLfoSmoothness[index])
            {
                const auto parameterID = ParameterIDAndName::getIDString(
                    LFO_SMOOTH_ID, i);
                if (auto* parameter = fireProc.treeState.getParameter(parameterID))
                {
                    const auto& range = parameter->getNormalisableRange();
                    const float smoothness = range.snapToLegalValue(
                        juce::jlimit(range.start,
                                     range.end,
                                     lfoDataToLoad[index].smoothness));
                    lfoDataToLoad[index].smoothness = smoothness;
                    parameter->setValueNotifyingHost(
                        parameter->convertTo0to1(smoothness));
                }
                continue;
            }

            if (const auto* smoothness = fireProc.treeState.getRawParameterValue(
                    ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i)))
                lfoDataToLoad[index].smoothness = smoothness->load(std::memory_order_relaxed);
        }

        juce::Array<ModulationRouting> routingsToLoad;
        if (auto* modMatrixState = xml.getChildByName("MODULATION_STATE"))
        {
            for (auto* routingXml : modMatrixState->getChildIterator())
            {
                if (! routingXml->hasTagName("ROUTING")
                    || routingsToLoad.size()
                           >= LfoManager::maximumModulationRoutings)
                    continue;

                auto routing = ModulationRouting::readFromXml(*routingXml);
                // Legacy snapshots had exactly four source identities. New
                // snapshots keep all fixed slots and never retarget a missing source.
                const bool extendedBank = xml.hasAttribute("lfoBankSchemaVersion")
                    || xml.hasAttribute(fire::lfo_bank::presentParameterID(0));
                const int sourceLimit = extendedBank ? fire::lfo_bank::capacity
                                                      : fire::lfo_bank::defaultCount;
                routing.sourceLfoIndex = juce::jlimit(0, sourceLimit - 1, routing.sourceLfoIndex);
                routing.depth = std::isfinite(routing.depth) ? juce::jlimit(-1.0f, 1.0f, routing.depth) : 0.5f;

                bool targetAlreadyUsed = false;
                for (const auto& existing : routingsToLoad)
                    if (existing.targetParameterID == routing.targetParameterID)
                    {
                        targetAlreadyUsed = true;
                        break;
                    }

                if (! targetAlreadyUsed
                    && routing.targetParameterID.isNotEmpty()
                    && fireProc.treeState.getParameter(routing.targetParameterID) != nullptr)
                    routingsToLoad.add(std::move(routing));
            }
        }

            fireProc.getLfoManager().replaceLfoDataAndRoutings(
                lfoDataToLoad, std::move(routingsToLoad));

            // A preset/A-B swap may replace every logical band while retaining
            // the same NUM_BANDS value. The scope guard publishes the complete
            // migration, including when a foreign listener throws.
        }
        fireProc.sendChangeMessage();
        return true;
    }

    //==============================================================================
    StateAB::StateAB(juce::AudioProcessor& p)
        : pluginProcessor { p }
    {
        // The processor constructs StateAB before its complete serializable
        // main model is ready. No other thread can observe this object yet, so
        // initialise the alternate directly without entering the topology
        // transaction used by runtime mutations.
        saveStateToXml(pluginProcessor, ab);
    }

    void StateAB::toggleAB()
    {
        auto& fireProc = static_cast<FireAudioProcessor&>(pluginProcessor);
        {
            // Every A/B state replacement follows topology-writer ->
            // stateLock. Host restore uses the same order, eliminating the
            // former ABBA cycle.
            fireProc.beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [&fireProc]
            {
                fireProc.requestMultibandTopologyReset();
            } };
            const juce::ScopedLock lock(stateLock);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
            invokeMutationLockAcquiredHookForTesting();
#endif
            juce::XmlElement temp { "AB" };
            saveStateToXml(pluginProcessor, temp); // current to temp
            if (! loadStateFromXml(ab, pluginProcessor, true)) // ab to current
                return;
            fireProc.cancelLoudnessMatchMeasurement();
            ab = std::move(temp); // temp to ab
            currentSideIsA.store(! currentSideIsA.load(std::memory_order_relaxed), std::memory_order_release);
        }

        pluginProcessor.updateHostDisplay(
            juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
    }

    void StateAB::copyAB(bool notifyHost)
    {
        auto& fireProc = static_cast<FireAudioProcessor&>(pluginProcessor);
        {
            // Copy changes only the inactive snapshot. Publishing the audio
            // topology generation here would unnecessarily fade and reset the
            // live DSP graph even though its active sound did not change. It
            // still takes the writer mutex so it cannot overwrite the result
            // of a toggle/host restore between that transaction's stateLock
            // release and its final odd-to-even publication.
            const juce::ScopedLock serializableWriterLock(
                fireProc.multibandTopologyWriterLock);
            const juce::ScopedLock lock(stateLock);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
            invokeMutationLockAcquiredHookForTesting();
#endif
            juce::XmlElement replacement { "AB" };
            saveStateToXml(pluginProcessor, replacement);
            ab = std::move(replacement);
            fireProc.copyLoudnessMatchToOtherSide();
        }

        if (notifyHost)
        {
            pluginProcessor.updateHostDisplay(
                juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
        }
    }

    void StateAB::reset()
    {
        pluginProcessor.reset();
    }

    juce::XmlElement StateAB::captureSerializableStateSnapshot() const
    {
        const juce::ScopedLock lock(stateLock);
        juce::XmlElement snapshot { ab };
        canonicaliseCloudsPresetState(snapshot);
        snapshot.setTagName("AB_STATE");
        snapshot.setAttribute("currentSideIsA",
                              currentSideIsA.load(std::memory_order_relaxed));
        return snapshot;
    }

    bool StateAB::readFromXml(const juce::XmlElement* state, bool* migratedGranular)
    {
        if (migratedGranular != nullptr)
            *migratedGranular = false;
        const juce::ScopedLock lock(stateLock);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        invokeMutationLockAcquiredHookForTesting();
#endif
        if (state == nullptr || ! isValidABSnapshot(*state, pluginProcessor))
        {
            // Old host states never persisted A/B, and a damaged alternate
            // must never turn the next toggle into an implicit Init. Preserve
            // the historical default: A is current and B mirrors the live
            // state that has just been restored.
            auto& fireProc = static_cast<const FireAudioProcessor&>(
                pluginProcessor);
            auto fallback = fireProc
                                .captureCurrentSerializablePresetStateSnapshotForABFallback();
            juce::XmlElement replacement { "AB" };
            writeSerializablePresetSnapshotToXml(
                fireProc, fallback, replacement);
            ab = std::move(replacement);
            currentSideIsA.store(true, std::memory_order_release);
            return false;
        }

        juce::XmlElement replacement { *state };
        const bool migrated = canonicaliseCloudsPresetState(replacement);
        if (migratedGranular != nullptr)
            *migratedGranular = migrated;
        replacement.setTagName("AB");
        replacement.removeAttribute("currentSideIsA");
        ab = std::move(replacement);
        currentSideIsA.store(state->getBoolAttribute("currentSideIsA", true), std::memory_order_release);
        return true;
    }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    void StateAB::setMutationLockAcquiredHookForTesting(
        std::function<void()> hook)
    {
        const juce::ScopedLock lock(stateLock);
        mutationLockAcquiredHookForTesting = std::move(hook);
    }

    void StateAB::invokeMutationLockAcquiredHookForTesting()
    {
        auto hook = std::move(mutationLockAcquiredHookForTesting);
        mutationLockAcquiredHookForTesting = {};
        if (hook)
            hook();
    }
#endif

    //==============================================================================

    static juce::int64 measurePresetSnapshotLength(
        juce::FileInputStream& stream)
    {
        if (! stream.openedOk())
            return -1;

        const auto byteExistsAt = [&](juce::int64 offset,
                                      bool& exists) -> bool
        {
            if (offset < 0 || ! stream.setPosition(offset))
                return false;

            char byte = 0;
            const auto bytesRead = stream.read(&byte, 1);
            if (bytesRead != 0 && bytesRead != 1)
                return false;

            exists = bytesRead == 1;
            return true;
        };

        // JUCE's getTotalLength() asks the path for its current size, not the
        // already-open native handle. Use it only as a fast hint, and verify
        // both sides of the claimed EOF against this stream. A normal file
        // therefore costs two one-byte probes; a concurrently replaced path
        // falls back to a bounded binary search on the open handle.
        const auto hintedLength = stream.getTotalLength();
        juce::int64 measuredLength = -1;
        bool lastHintedByteExists = false;
        bool byteAfterHintExists = false;
        if (hintedLength > 0
            && hintedLength <= maximumPresetFileBytes
            && byteExistsAt(hintedLength - 1, lastHintedByteExists)
            && byteExistsAt(hintedLength, byteAfterHintExists)
            && lastHintedByteExists
            && ! byteAfterHintExists)
        {
            measuredLength = hintedLength;
        }
        else
        {
            bool firstByteExists = false;
            bool byteBeyondLimitExists = false;
            if (! byteExistsAt(0, firstByteExists))
                measuredLength = -1;
            else if (! firstByteExists)
                measuredLength = 0;
            else if (! byteExistsAt(maximumPresetFileBytes,
                                    byteBeyondLimitExists))
                measuredLength = -1;
            else if (byteBeyondLimitExists)
                measuredLength = maximumPresetFileBytes + 1;
            else
            {
                juce::int64 firstPossibleEof = 1;
                juce::int64 lastPossibleEof = maximumPresetFileBytes;
                bool searchFailed = false;
                while (firstPossibleEof < lastPossibleEof)
                {
                    const auto midpoint = firstPossibleEof
                                        + (lastPossibleEof
                                           - firstPossibleEof)
                                              / 2;
                    bool midpointExists = false;
                    if (! byteExistsAt(midpoint, midpointExists))
                    {
                        searchFailed = true;
                        break;
                    }

                    if (midpointExists)
                        firstPossibleEof = midpoint + 1;
                    else
                        lastPossibleEof = midpoint;
                }

                measuredLength = searchFailed ? -1 : firstPossibleEof;
            }
        }

        if (! stream.setPosition(0))
            return -1;

        return measuredLength;
    }

    static bool parsePresetSnapshot(juce::FileInputStream& stream,
                                    juce::int64 snapshotLength,
                                    juce::XmlElement& xml)
    {
        if (! stream.openedOk()
            || snapshotLength <= 0
            || snapshotLength > maximumPresetFileBytes)
            return false;

        // snapshotLength belongs to this already-open handle. Never reopen the
        // path: it may be replaced or extended between directory enumeration
        // and parsing. The explicit direct-read limit prevents an append from
        // expanding the charged parser input; JUCE's MemoryOutputStream helper
        // is deliberately avoided because it re-queries the path length.
        juce::MemoryBlock snapshot(static_cast<size_t>(snapshotLength),
                                   false);
        juce::int64 bytesRead = 0;
        while (bytesRead < snapshotLength)
        {
            const auto chunk = stream.read(
                static_cast<char*>(snapshot.getData())
                    + static_cast<size_t>(bytesRead),
                static_cast<int>(snapshotLength - bytesRead));
            if (chunk <= 0)
                break;

            bytesRead += chunk;
        }

        if (bytesRead != snapshotLength)
            return false;

        auto parsed = juce::XmlDocument::parse(
            juce::String::createStringFromData(
                snapshot.getData(), static_cast<int>(snapshot.getSize())));
        if (parsed == nullptr || ! isSupportedPresetDocument(*parsed))
            return false;

        xml = *parsed;
        return true;
    }

    bool parseFileToXmlElement(const juce::File& file, juce::XmlElement& xml)
    {
        juce::FileInputStream stream(file);
        if (! stream.openedOk())
            return false;

        const auto snapshotLength = measurePresetSnapshotLength(stream);
        return parsePresetSnapshot(stream, snapshotLength, xml);
    }

    bool writeXmlElementToFile(const juce::XmlElement& xml,
                               juce::File& file,
                               const juce::String& presetName,
                               bool confirmOverwrite)
    {
        if (! file.exists())
            return xml.writeTo(file);

        if (file.isDirectory())
            return false;

        if (confirmOverwrite)
        {
            const bool choice = juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                                        "Replace preset?",
                                                                        "\"" + presetName + PRESET_EXETENSION
                                                                            + "\" already exists. Replacing it will overwrite its current contents.",
                                                                        nullptr,
                                                                        nullptr);
            if (! choice)
                return false;
        }

        return xml.writeTo(file);
    }

    static juce::String getFolderDisplayName(const juce::XmlElement& folder)
    {
        return folder.getStringAttribute("folderName", folder.getTagName());
    }

    //==============================================================================
    //sorter
    class PresetNameSorter
    {
    public:
        int compareElements(juce::XmlElement* first, juce::XmlElement* second) const
        {
            const bool firstIsPreset = first->hasAttribute("presetName");
            const bool secondIsPreset = second->hasAttribute("presetName");

            if (firstIsPreset && secondIsPreset)
            {
                return first->getStringAttribute("presetName")
                    .compareNatural(second->getStringAttribute("presetName"));
            }

            if (! firstIsPreset && ! secondIsPreset)
            {
                return getFolderDisplayName(*first).compareNatural(getFolderDisplayName(*second));
            }

            return firstIsPreset ? 1 : -1;
        }
    };

    //==============================================================================
    StatePresets::PresetScanLimits
    StatePresets::getDefaultPresetScanLimits() noexcept
    {
        return {
            maximumPresetCount,
            maximumPresetCandidateCount,
            maximumPresetDirectoryEntryCount,
            maximumPresetRawDirectoryEntryCount,
            maximumPresetScanParsedBytes
        };
    }

    StatePresets::StatePresets(juce::AudioProcessor& proc, const juce::String& presetFileLocation)
        : pluginProcessor { proc },
          presetFile { juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile(presetFileLocation) },
          presetScanLimits { getDefaultPresetScanLimits() }
    {
        scanAllPresets();
        //parseFileToXmlElement(presetFile, mPresetXml);
    }

    StatePresets::~StatePresets()
    {
    }

    juce::String StatePresets::getNextAvailablePresetId()
    {
        const int newPresetIdNumber = getNumPresets();
        return "preset" + static_cast<juce::String>(newPresetIdNumber); // format: preset##
    }

    juce::String StatePresets::normalisePresetKey(juce::String key)
    {
        // Relative paths are stable identities, so preserve legal leading and
        // trailing spaces exactly. Trimming here makes distinct on-disk files
        // such as "Preset.fire" and " Preset.fire" alias one another.
        key = key.replaceCharacter('\\', '/');
        while (key.startsWith("./"))
            key = key.substring(2);
        while (key.contains("//"))
            key = key.replace("//", "/");

        // A key is always relative to the configured preset root. Reject
        // traversal and absolute paths rather than allowing identity to escape
        // that root when it arrives from a host state.
        if (key.trim().isEmpty() || juce::File::isAbsolutePath(key))
            return {};

        const auto segments = juce::StringArray::fromTokens(key, "/", {});
        for (const auto& segment : segments)
            if (segment.isEmpty() || segment == "." || segment == "..")
                return {};

        return key;
    }

    void StatePresets::recursiveFileSearch(juce::XmlElement& parentXML,
                                           const juce::File& dir,
                                           int depth)
    {
        PresetScanState scanState { presetScanLimits, {}, false };
        recursiveFileSearchImpl(parentXML,
                                dir,
                                depth,
                                scanState);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        lastPresetScanStatistics = scanState.statistics;
#endif
        numPresets.store(scanState.statistics.acceptedPresetCount,
                         std::memory_order_release);
    }

    void StatePresets::recursiveFileSearchImpl(
        juce::XmlElement& parentXML,
        const juce::File& dir,
        int depth,
        PresetScanState& scanState)
    {
        auto& limits = scanState.limits;
        auto& statistics = scanState.statistics;

        if (scanState.stopScanning
            || depth > maximumPresetFolderDepth
            || dir.isSymbolicLink())
            return;

        // Hard count limits stop the complete depth-first traversal. Checking
        // at every recursive entry makes an exhausted child unwind all of its
        // ancestors instead of merely advancing to the next sibling folder.
        if (statistics.acceptedPresetCount
                >= limits.maximumAcceptedPresetCount)
        {
            statistics.acceptedPresetLimitReached = true;
            scanState.stopScanning = true;
            return;
        }

        if (statistics.candidateFileCount
                >= limits.maximumCandidateFileCount)
        {
            statistics.candidateFileLimitReached = true;
            scanState.stopScanning = true;
            return;
        }

        if (statistics.visitedDirectoryEntryCount
                >= limits.maximumDirectoryEntryCount)
        {
            statistics.directoryEntryLimitReached = true;
            scanState.stopScanning = true;
            return;
        }

        if (statistics.rawDirectoryEntryCount
                >= limits.maximumRawDirectoryEntryCount)
        {
            statistics.rawDirectoryEntryLimitReached = true;
            scanState.stopScanning = true;
            return;
        }

        juce::RangedDirectoryIterator iterator(dir,
                                                false,
                                                "*",
                                                juce::File::findFilesAndDirectories,
                                                juce::File::FollowSymlinks::no);
        for (auto file : iterator)
        {
            if (scanState.stopScanning)
                break;

            if (statistics.rawDirectoryEntryCount
                    >= limits.maximumRawDirectoryEntryCount)
            {
                statistics.rawDirectoryEntryLimitReached = true;
                scanState.stopScanning = true;
                break;
            }

            ++statistics.rawDirectoryEntryCount;
            const juce::ScopeGuard finishRawDirectoryEntry { [&]
            {
                if (statistics.rawDirectoryEntryCount
                        >= limits.maximumRawDirectoryEntryCount)
                {
                    statistics.rawDirectoryEntryLimitReached = true;
                    scanState.stopScanning = true;
                }
            } };

            const auto candidate = file.getFile();
            if (file.isHidden())
                continue;

            if (statistics.visitedDirectoryEntryCount
                    >= limits.maximumDirectoryEntryCount)
            {
                statistics.directoryEntryLimitReached = true;
                scanState.stopScanning = true;
                continue;
            }

            ++statistics.visitedDirectoryEntryCount;
            const juce::ScopeGuard finishDirectoryEntry { [&]
            {
                if (statistics.visitedDirectoryEntryCount
                        >= limits.maximumDirectoryEntryCount)
                {
                    statistics.directoryEntryLimitReached = true;
                    scanState.stopScanning = true;
                }
            } };

            // RangedDirectoryIterator is configured not to follow links, but
            // reject them explicitly before either file parsing or recursive
            // descent. A non-hidden link consumes raw and visible traversal
            // slots, never a .fire candidate slot.
            if (candidate.isSymbolicLink())
                continue;

            if (file.isDirectory())
            {
                auto currentState = std::make_unique<juce::XmlElement>("FOLDER");
                currentState->setAttribute("folderName",
                                           candidate.getFileName());
                recursiveFileSearchImpl(*currentState,
                                        candidate,
                                        depth + 1,
                                        scanState);
                if (currentState->getNumChildElements() > 0)
                    parentXML.addChildElement(currentState.release());
            }
            else if (candidate.hasFileExtension(PRESET_EXETENSION))
            {
                if (statistics.candidateFileCount
                        >= limits.maximumCandidateFileCount)
                {
                    statistics.candidateFileLimitReached = true;
                    scanState.stopScanning = true;
                    continue;
                }

                // Every .fire path consumes a candidate slot, including an
                // empty, oversized, malformed, foreign, or otherwise rejected
                // document. Invalid files therefore cannot make the scan
                // unbounded while leaving the accepted count at zero.
                ++statistics.candidateFileCount;
                const juce::ScopeGuard finishCandidate { [&]
                {
                    if (statistics.candidateFileCount
                            >= limits.maximumCandidateFileCount)
                    {
                        statistics.candidateFileLimitReached = true;
                        scanState.stopScanning = true;
                    }
                } };

                // Open once, then charge and parse the length belonging to
                // that same handle. Reopening by path would allow an external
                // preset manager to replace a small file with a large one
                // between accounting and XML parsing.
                juce::FileInputStream candidateStream(candidate);
                if (! candidateStream.openedOk())
                {
                    ++statistics.rejectedCandidateCount;
                    continue;
                }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
                if (presetStreamOpenedHookForTesting)
                    presetStreamOpenedHookForTesting(candidate);
#endif

                const auto candidateBytes =
                    measurePresetSnapshotLength(candidateStream);
                if (candidateBytes <= 0
                    || candidateBytes > maximumPresetFileBytes)
                {
                    ++statistics.rejectedCandidateCount;
                    continue;
                }

                const auto parsedByteBudgetRemaining =
                    juce::jmax<juce::int64>(
                        0,
                        limits.maximumParsedFileBytes
                            - statistics.parsedFileBytes);
                if (candidateBytes > parsedByteBudgetRemaining)
                {
                    // Do not end traversal here: a later small preset can
                    // still fit in the deterministic byte budget.
                    ++statistics.skippedForByteBudgetCount;
                    statistics.parsedFileByteLimitReached = true;
                    continue;
                }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
                if (presetSnapshotOpenedHookForTesting)
                    presetSnapshotOpenedHookForTesting(candidate,
                                                       candidateBytes);
#endif

                statistics.parsedFileBytes += candidateBytes;
                ++statistics.parsedFileCount;
                auto currentState = std::make_unique<juce::XmlElement>("PRESET");
                if (! parsePresetSnapshot(candidateStream,
                                           candidateBytes,
                                           *currentState))
                {
                    ++statistics.rejectedCandidateCount;
                    continue;
                }

                if (! isLoadablePresetState(*currentState, pluginProcessor))
                {
                    ++statistics.rejectedCandidateCount;
                    continue;
                }

                bool hasKnownParameter = false;
                for (const auto* parameter : pluginProcessor.getParameters())
                    if (const auto* parameterWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
                        parameterWithID != nullptr && currentState->hasAttribute(parameterWithID->paramID))
                    {
                        hasKnownParameter = true;
                        break;
                    }

                // A bare <WINGSFIRE/> document is syntactically valid XML but
                // not a preset. Exposing it would create a menu action that
                // silently resets every parameter to its default.
                if (! hasKnownParameter)
                {
                    ++statistics.rejectedCandidateCount;
                    continue;
                }

                const auto presetKey = normalisePresetKey(
                    candidate.getRelativePathFrom(presetFile));
                if (presetKey.isEmpty())
                {
                    ++statistics.rejectedCandidateCount;
                    continue;
                }

                ++statistics.acceptedPresetCount;
                const juce::String newPresetId =
                    "preset"
                    + juce::String(statistics.acceptedPresetCount);
                currentState->setTagName(newPresetId);

                const juce::String newName =
                    candidate.getFileNameWithoutExtension();
                if (newName != currentState->getStringAttribute("presetName"))
                    currentState->setAttribute("presetName", newName);
                currentState->setAttribute("presetKey", presetKey);

                parentXML.addChildElement(currentState.release());

                if (statistics.acceptedPresetCount
                        >= limits.maximumAcceptedPresetCount)
                {
                    statistics.acceptedPresetLimitReached = true;
                    scanState.stopScanning = true;
                }
            }
        }
    }

    void StatePresets::recursiveSort(juce::XmlElement* parent)
    {
        PresetNameSorter sorter;
        parent->sortChildElements(sorter);

        for (auto* child : parent->getChildIterator())
        {
            // If the child element is a folder (i.e., it doesn't have a "presetName" attribute), sort it recursively.
            if (child->hasTagName("FOLDER"))
            {
                recursiveSort(child);
            }
        }
    }

    void StatePresets::scanAllPresets()
    {
        PresetScanState scanState { presetScanLimits, {}, false };
        mPresetXml.deleteAllChildElements();
        //RangedDirectoryIterator iterator(presetFile, true, "*.fire", 2);

        recursiveFileSearchImpl(mPresetXml,
                                presetFile,
                                0,
                                scanState);

        recursiveSort(&mPresetXml);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        lastPresetScanStatistics = scanState.statistics;
#endif

        // Host state restoration may query the count from a non-message
        // thread. Publish only the complete scan so it never clamps a legacy
        // preset ID against a transient zero or partial result.
        numPresets.store(scanState.statistics.acceptedPresetCount,
                         std::memory_order_release);

        //mPresetXml.writeTo(File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Audio/Presets/Wings/Fire/test.xml"));
    }

    juce::String StatePresets::savePreset(juce::File savePath, bool overwriteAlreadyConfirmed)
    {
        if (savePath == juce::File())
            return juce::String();

        juce::File userPresetFile = savePath.hasFileExtension(PRESET_EXETENSION)
                                        ? savePath
                                        : juce::File(savePath.getFullPathName() + PRESET_EXETENSION);
        const auto presetName = userPresetFile.getFileNameWithoutExtension();
        if (presetName.trim().isEmpty())
            return juce::String();

        const auto parentDirectory = userPresetFile.getParentDirectory();
        if ((! parentDirectory.isDirectory() && parentDirectory.createDirectory().failed())
            || ! parentDirectory.isDirectory())
            return juce::String();

        // Save the single preset to a real file.
        presetXmlSingle.removeAllAttributes(); // Clear all first.
        presetXmlSingle.deleteAllChildElements();
        presetXmlSingle.setAttribute("presetName", presetName); // Set preset name.
        presetXmlSingle.setAttribute("presetFormatVersion", 2);
        presetXmlSingle.setAttribute("pluginVersion", VERSION);
        saveStateToXml(pluginProcessor, presetXmlSingle);

        const bool isSaved = writeXmlElementToFile(presetXmlSingle,
                                                   userPresetFile,
                                                   presetName,
                                                   ! overwriteAlreadyConfirmed);

        if (isSaved)
        {
            const auto savedPresetKey = normalisePresetKey(userPresetFile.getRelativePathFrom(presetFile));
            {
                const juce::ScopedLock lock(identityLock);
                currentPresetKey = savedPresetKey;
                if (savedPresetKey.isNotEmpty())
                {
                    statePresetName = presetName;
                }
                else
                {
                    // Exporting outside the configured preset root is valid,
                    // but that file cannot be represented by a stable library
                    // key and must not accidentally select a same-named preset.
                    statePresetName.clear();
                    mCurrentPresetId.store(0, std::memory_order_relaxed);
                }
            }
            scanAllPresets(); // Rescan and sort all presets.

            // Key change: Return the preset name on success.
            return presetName;
        }
        else
        {
            // Key change: Return an empty string on failure or cancellation.
            return juce::String();
        }
    }

    bool StatePresets::recursivePresetLoad(const juce::XmlElement& parentXml, const juce::String& presetId)
    {
        for (auto* child : parentXml.getChildIterator())
        {
            if (child->hasAttribute("presetName") && child->getTagName() == presetId)
            {
                if (! isLoadablePresetState(*child, pluginProcessor))
                    return false;

                {
                    auto& fireProc = static_cast<FireAudioProcessor&>(
                        pluginProcessor);
                    fireProc.beginMultibandTopologyEdit();
                    const juce::ScopeGuard finishTopologyEdit { [&fireProc]
                    {
                        fireProc.requestMultibandTopologyReset();
                    } };

                    if (! loadStateFromXml(*child, pluginProcessor))
                        return false;
                    {
                        const juce::ScopedLock lock(identityLock);
                        statePresetName = child->getStringAttribute("presetName");
                        currentPresetKey = normalisePresetKey(
                            child->getStringAttribute("presetKey"));
                    }
                }
                return true;
            }

            if (child->hasTagName("FOLDER") && recursivePresetLoad(*child, presetId))
                return true;
        }

        return false;
    }

    bool StatePresets::loadPreset(const juce::String& presetId,
                                  bool notifyHost)
    {
        const bool presetWasLoaded = recursivePresetLoad(mPresetXml, presetId);
        if (presetWasLoaded && notifyHost)
        {
            // recursivePresetLoad commits its topology transaction before
            // telling the host to request a new state snapshot.
            pluginProcessor.updateHostDisplay(
                juce::AudioProcessorListener::ChangeDetails {}
                    .withNonParameterStateChanged(true));
        }

        return presetWasLoaded;
    }

    void StatePresets::deletePreset()
    {
        const int currentId = mCurrentPresetId.load(std::memory_order_relaxed);
        const auto presetTag = comboBoxIdToTagNameMap[currentId];
        if (currentId <= 0 || presetTag.isEmpty())
            return;

        std::function<bool(juce::XmlElement&)> removeByTag = [&](juce::XmlElement& parent)
        {
            for (auto* child : parent.getChildIterator())
            {
                if (child->getTagName() == presetTag && child->hasAttribute("presetName"))
                {
                    parent.removeChildElement(child, true);
                    return true;
                }

                if (removeByTag(*child))
                    return true;
            }
            return false;
        };

        if (removeByTag(mPresetXml))
        {
            auto previousCount = numPresets.load(std::memory_order_relaxed);
            while (previousCount > 0
                   && ! numPresets.compare_exchange_weak(
                       previousCount,
                       previousCount - 1,
                       std::memory_order_release,
                       std::memory_order_relaxed))
            {
            }
            const juce::ScopedLock lock(identityLock);
            mCurrentPresetId.store(0, std::memory_order_relaxed);
            statePresetName.clear();
            currentPresetKey.clear();
        }
    }

    void StatePresets::setPresetName(juce::String name)
    {
        const juce::ScopedLock lock(identityLock);
        statePresetName = std::move(name);
    }

    juce::String StatePresets::getPresetName() const
    {
        const juce::ScopedLock lock(identityLock);
        return statePresetName;
    }

    void StatePresets::recursivePresetNameAdd(const juce::XmlElement& parentXml, juce::ComboBox& menu, int& index)
    {
        for (auto* child : parentXml.getChildIterator())
        {
            if (child->hasAttribute("presetName"))
            {
                // is preset
                index++;
                juce::String n = child->getStringAttribute("presetName");
                if (n == "")
                    n = "(Unnamed preset)";
                menu.addItem(n, index);
                comboBoxIdToTagNameMap.set(index, child->getTagName());
                const auto childKey = normalisePresetKey(child->getStringAttribute("presetKey"));
                comboBoxIdToPresetKeyMap.set(index, childKey);

                // Stable relative-path identity wins. Name matching is retained
                // only for legacy in-memory state created before preset keys.
                if ((currentPresetKey.isNotEmpty() && currentPresetKey == childKey)
                    || (currentPresetKey.isEmpty()
                        && mCurrentPresetId.load(std::memory_order_relaxed) == 0
                        && statePresetName == n))
                {
                    mCurrentPresetId.store(index);
                    currentPresetKey = childKey;
                    statePresetName = n;
                }
            }
            else if (child->hasTagName("FOLDER"))
            {
                // is folder
                if (index != 0)
                {
                    menu.addSeparator();
                }
                juce::String n = getFolderDisplayName(*child);
                menu.addSectionHeading(n);

                recursivePresetNameAdd(*child, menu, index);
            }
        }
    }

    void StatePresets::setPresetAndFolderNames(juce::ComboBox& menu)
    {
        const juce::ScopedLock lock(identityLock);
        const int pendingLegacyId = mCurrentPresetId.load(std::memory_order_relaxed);
        const bool hasStableIdentity = currentPresetKey.isNotEmpty() || statePresetName.isNotEmpty();
        comboBoxIdToTagNameMap.clear();
        comboBoxIdToPresetKeyMap.clear();
        if (hasStableIdentity)
            mCurrentPresetId.store(0, std::memory_order_relaxed);
        int index = 0;
        recursivePresetNameAdd(mPresetXml, menu, index);

        // Host states created before preset keys only contain the sorted menu
        // ID. Keep that best-effort fallback until the first menu exists, then
        // bind it to the discovered stable key for future saves.
        if (! hasStableIdentity)
        {
            const int safeLegacyId = juce::isPositiveAndBelow(pendingLegacyId, index + 1)
                                         ? pendingLegacyId
                                         : 0;
            mCurrentPresetId.store(safeLegacyId, std::memory_order_relaxed);
            currentPresetKey = normalisePresetKey(comboBoxIdToPresetKeyMap[safeLegacyId]);
            if (safeLegacyId > 0)
                statePresetName = menu.getItemText(menu.indexOfItemId(safeLegacyId));
        }
    }

    int StatePresets::getNumPresets() const
    {
        //return mPresetXml.getNumChildElements();
        return numPresets.load(std::memory_order_acquire);
    }

    int StatePresets::getCurrentPresetId() const
    {
        return mCurrentPresetId.load();
    }

    StatePresets::PresetIdentitySnapshot
    StatePresets::getCurrentPresetIdentity() const
    {
        const juce::ScopedLock lock(identityLock);
        return { mCurrentPresetId.load(std::memory_order_relaxed),
                 currentPresetKey };
    }

    void StatePresets::setCurrentPresetId(int currentPresetId)
    {
        const juce::ScopedLock lock(identityLock);
        const int safeId = juce::jmax(0, currentPresetId);
        mCurrentPresetId.store(safeId);
        currentPresetKey = normalisePresetKey(comboBoxIdToPresetKeyMap[safeId]);
        if (safeId == 0 || currentPresetKey.isEmpty())
            statePresetName.clear();
    }

    juce::String StatePresets::getCurrentPresetKey() const
    {
        const juce::ScopedLock lock(identityLock);
        return currentPresetKey;
    }

    void StatePresets::setCurrentPresetKey(juce::String key)
    {
        const juce::ScopedLock lock(identityLock);
        currentPresetKey = normalisePresetKey(std::move(key));
        statePresetName.clear();
        mCurrentPresetId.store(0, std::memory_order_relaxed);
    }

    juce::File StatePresets::getFile()
    {
        return presetFile;
    }

    const juce::XmlElement& StatePresets::getPresetXml() const
    {
        return mPresetXml;
    }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    void StatePresets::setPresetDirectoryForTesting(juce::File directory)
    {
        presetFile = std::move(directory);
        scanAllPresets();
    }
#endif

    void StatePresets::initPreset()
    {
        auto& fireProc = static_cast<FireAudioProcessor&>(pluginProcessor);
        {
            fireProc.beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [&fireProc]
            {
                fireProc.requestMultibandTopologyReset();
            } };
            for (const auto& param : pluginProcessor.getParameters())
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                    // if not in xml set current
                    p->setValueNotifyingHost(p->getDefaultValue());
            // set preset combobox to 0
            {
                const juce::ScopedLock lock(identityLock);
                statePresetName.clear();
                currentPresetKey.clear();
                mCurrentPresetId.store(0);
            }

            fireProc.getLfoManager().replaceLfoDataAndRoutings(
                std::array<LfoData, fire::lfo_bank::capacity> {}, {});
        }
        fireProc.sendChangeMessage();
        pluginProcessor.updateHostDisplay(
            juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
    }

    //==============================================================================

    //==============================================================================
    StateComponent::ManualUpdateCheckThread::ManualUpdateCheckThread(StateComponent& ownerToUse)
        : juce::Thread("Fire manual update check"), owner(ownerToUse)
    {
    }

    void StateComponent::ManualUpdateCheckThread::run()
    {
        const auto generation = requestGeneration;
        auto result = fetchOperation.fetchLatest();
        if (! threadShouldExit())
            owner.publishManualUpdateResult(std::move(result), generation);
    }

    void StateComponent::ManualUpdateCheckThread::cancel() noexcept
    {
        signalThreadShouldExit();
        fetchOperation.cancel();
    }

    void StateComponent::ManualUpdateCheckThread::stop()
    {
        cancel();
        stopThread(-1);
    }

    void StateComponent::ManualUpdateCheckThread::prepareForStart(
        std::uint64_t generation)
    {
        requestGeneration = generation;
        fetchOperation.reset();
    }

    void StateComponent::PresetComboBox::capturePopupRequest() noexcept
    {
        popupRequestContextRevision = popupContextRevision;
        popupRequestArmed = true;
    }

    bool StateComponent::PresetComboBox::isPopupContextCurrent(
        std::uint64_t contextRevision) const noexcept
    {
        return popupContextRevision == contextRevision
               && isShowing()
               && isEnabled();
    }

    bool StateComponent::PresetComboBox::keyPressed(
        const juce::KeyPress& key)
    {
        noteKeyboardInteraction();

        const bool movesBackward = key == juce::KeyPress::upKey
                                   || key == juce::KeyPress::leftKey;
        const bool movesForward = key == juce::KeyPress::downKey
                                  || key == juce::KeyPress::rightKey;

        if (movesBackward || movesForward)
        {
            if (! isShowing() || ! isEnabled()
                || isPopupActive() || popupRequestArmed)
                return true;

            const int delta = movesBackward ? -1 : 1;
            for (int itemIndex = getSelectedItemIndex() + delta;
                 juce::isPositiveAndBelow(itemIndex, getNumItems());
                 itemIndex += delta)
            {
                const int itemId = getItemId(itemIndex);
                if (itemId == 0 || ! isItemEnabled(itemId))
                    continue;

                // A direction key is a complete user decision. Notify now so
                // its numeric ID cannot be interpreted after a preset rescan.
                // The listener may synchronously delete this complete editor,
                // so selection is deliberately the final operation.
                popupSessionActive = false;
                ++popupSessionRevision;
                setSelectedId(itemId, juce::sendNotificationSync);
                return true;
            }

            return true;
        }

        if (key == juce::KeyPress::returnKey)
        {
            if (! isShowing() || ! isEnabled()
                || isPopupActive() || popupRequestArmed)
                return true;

            capturePopupRequest();
        }

        return juce::ComboBox::keyPressed(key);
    }

    bool StateComponent::PresetComboBox::isCompletePrimaryDown(
        const juce::MouseEvent& event) const noexcept
    {
        return event.mods.isLeftButtonDown()
               && ! event.mods.isRightButtonDown()
               && ! event.mods.isMiddleButtonDown()
               && ! event.mods.isPopupMenu();
    }

    bool StateComponent::PresetComboBox::isPointerSource(
        const juce::MouseEvent& event) const noexcept
    {
        return event.source.getType() == pointerSourceType
               && event.source.getIndex() == pointerSourceIndex;
    }

    void StateComponent::PresetComboBox::mouseDown(
        const juce::MouseEvent& event)
    {
        if (! isShowing() || ! isEnabled()
            || ! isCompletePrimaryDown(event)
            || popupRequestArmed || isPopupActive())
            return;

        notePointerInteraction();

        const juce::Component::SafePointer<PresetComboBox> safeThis(this);
        if (pointerInteractionActive)
        {
            if (! isPointerSource(event))
                return;

            // Recover a release omitted by the host before accepting another
            // press from the same physical source.
            releasePointerInteractionWithoutSelection(event);
            if (safeThis == nullptr)
                return;
        }

        pointerInteractionActive = true;
        cancelPendingPointerRelease = false;
        pointerSourceType = event.source.getType();
        pointerSourceIndex = event.source.getIndex();
        capturePopupRequest();

        juce::ComboBox::mouseDown(event);
        if (safeThis == nullptr)
            return;

        if (! isPopupActive())
        {
            popupRequestArmed = false;
            clearPointerInteraction();
        }
    }

    void StateComponent::PresetComboBox::mouseDrag(
        const juce::MouseEvent& event)
    {
        if (! pointerInteractionActive
            || ! isPointerSource(event)
            || cancelPendingPointerRelease)
            return;

        // This non-editable ComboBox opens on mouseDown. Forwarding a drag
        // after an asynchronous menu dismissal could reopen it under a stale
        // preset-list revision, so the opener press owns no drag command.
    }

    void StateComponent::PresetComboBox::mouseEnter(
        const juce::MouseEvent& event)
    {
        const juce::Component::SafePointer<PresetComboBox> safeThis(this);
        juce::ComboBox::mouseEnter(event);
        if (safeThis != nullptr)
        {
            recoverMissingPointerUp(event);

            if (safeThis != nullptr)
                repaint();
        }
    }

    void StateComponent::PresetComboBox::mouseMove(
        const juce::MouseEvent& event)
    {
        const juce::Component::SafePointer<PresetComboBox> safeThis(this);
        juce::ComboBox::mouseMove(event);
        if (safeThis != nullptr)
        {
            recoverMissingPointerUp(event);

            if (safeThis != nullptr)
                repaint();
        }
    }

    void StateComponent::PresetComboBox::mouseExit(
        const juce::MouseEvent& event)
    {
        const juce::Component::SafePointer<PresetComboBox> safeThis(this);
        juce::ComboBox::mouseExit(event);
        if (safeThis != nullptr)
        {
            recoverMissingPointerUp(event);

            if (safeThis != nullptr)
                repaint();
        }
    }

    void StateComponent::PresetComboBox::mouseUp(
        const juce::MouseEvent& event)
    {
        if (! pointerInteractionActive || ! isPointerSource(event))
            return;

        releasePointerInteractionWithoutSelection(event);
    }

    void StateComponent::PresetComboBox::recoverMissingPointerUp(
        const juce::MouseEvent& event)
    {
        if (pointerInteractionActive
            && isPointerSource(event)
            && ! event.mods.isLeftButtonDown())
            releasePointerInteractionWithoutSelection(event);
    }

    void StateComponent::PresetComboBox::releasePointerInteractionWithoutSelection(
        const juce::MouseEvent& event)
    {
        // Clear custom ownership first: ComboBox::mouseUp() can synchronously
        // notify code that destroys this component. An off-control position
        // only releases JUCE's private pressed bit and cannot open a menu.
        clearPointerInteraction();
        juce::ComboBox::mouseUp(
            event.getEventRelativeTo(this).withNewPosition(
                juce::Point<float> { -1.0f, -1.0f }));
    }

    void StateComponent::PresetComboBox::clearPointerInteraction() noexcept
    {
        pointerInteractionActive = false;
        cancelPendingPointerRelease = false;
        pointerSourceIndex = -1;
    }

    void StateComponent::PresetComboBox::mouseWheelMove(
        const juce::MouseEvent& event,
        const juce::MouseWheelDetails& wheel)
    {
        // Preset loading must be an explicit decision and must never inherit
        // ComboBox's asynchronous wheel-nudge notification path.
        juce::Component::mouseWheelMove(event, wheel);
    }

    void StateComponent::PresetComboBox::closePopupWindow() noexcept
    {
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (popupCloserForTesting != nullptr)
        {
            // Move the callback off the component before invoking it: the
            // popup close path is allowed to synchronously destroy this box.
            std::function<void()> closer;
            closer.swap(popupCloserForTesting);
            closer();
            return;
        }
#endif
        juce::ComboBox::hidePopup();
    }

    void StateComponent::PresetComboBox::dismissTransientInteraction() noexcept
    {
        ++popupContextRevision;
        popupSessionActive = false;
        ++popupSessionRevision;
        cancelPendingPointerRelease = cancelPendingPointerRelease
                                      || pointerInteractionActive;

        // Do not clear popupRequestArmed. showPopup() may already be queued by
        // JUCE and must consume that old request instead of treating it as a
        // new accessibility command after the component becomes visible again.
        closePopupWindow();
    }

    void StateComponent::PresetComboBox::invalidateMenuContents() noexcept
    {
        dismissTransientInteraction();
    }

    void StateComponent::PresetComboBox::visibilityChanged()
    {
        const juce::Component::SafePointer<PresetComboBox> safeThis(this);
        juce::Component::visibilityChanged();
        if (safeThis == nullptr)
            return;

        if (! isShowing())
            dismissTransientInteraction();
    }

    void StateComponent::PresetComboBox::enablementChanged()
    {
        const juce::Component::SafePointer<PresetComboBox> safeThis(this);
        juce::ComboBox::enablementChanged();
        if (safeThis == nullptr)
            return;

        // Keep dismissal last: closing a native popup may synchronously tear
        // down the owning editor and therefore this ComboBox.
        if (! isEnabled())
            dismissTransientInteraction();
    }

    void StateComponent::PresetComboBox::parentHierarchyChanged()
    {
        const juce::Component::SafePointer<PresetComboBox> safeThis(this);

        juce::ComboBox::parentHierarchyChanged();
        if (safeThis == nullptr)
            return;

        // This must remain the final operation for the same reason as the
        // enablement callback above.
        if (! isShowing())
            dismissTransientInteraction();
    }

    std::function<void(int)>
    StateComponent::PresetComboBox::createPopupResultHandler(
        std::uint64_t contextRevision)
    {
        popupSessionActive = true;
        const auto sessionRevision = ++popupSessionRevision;

        return [safeThis = juce::Component::SafePointer<PresetComboBox>(this),
                contextRevision,
                sessionRevision](int result)
        {
            if (safeThis == nullptr
                || ! safeThis->popupSessionActive
                || safeThis->popupSessionRevision != sessionRevision)
                return;

            const int itemIndex = safeThis->indexOfItemId(result);
            const bool mayCommit = result != 0
                                   && safeThis->isPopupContextCurrent(
                                       contextRevision)
                                   && itemIndex >= 0
                                   && safeThis->isItemEnabled(result);

            // Consume before notifying. Preset loading notifies the host and
            // may synchronously destroy this ComboBox and the whole editor.
            safeThis->popupSessionActive = false;
            ++safeThis->popupSessionRevision;
            safeThis->cancelPendingPointerRelease =
                safeThis->cancelPendingPointerRelease
                || safeThis->pointerInteractionActive;
            safeThis->closePopupWindow();

            if (! mayCommit
                || safeThis == nullptr
                || ! safeThis->isPopupContextCurrent(contextRevision))
                return;

            // Synchronous delivery keeps this numeric item ID bound to the
            // exact menu contents that were validated above.
            safeThis->setSelectedId(result, juce::sendNotificationSync);
        };
    }

    void StateComponent::PresetComboBox::showPopup()
    {
        if (! isShowing() || ! isEnabled())
        {
            popupRequestArmed = false;
            dismissTransientInteraction();
            return;
        }

        // Accessibility actions call showPopup() directly. Route them through
        // JUCE's normal asynchronous opener so its private menu-active state is
        // established before the actual menu is constructed.
        if (! popupRequestArmed)
        {
            if (isPopupActive())
                return;

            capturePopupRequest();
            juce::ComboBox::keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey });
            return;
        }

        const auto requestContextRevision = popupRequestContextRevision;
        popupRequestArmed = false;
        if (! isPopupContextCurrent(requestContextRevision))
        {
            popupSessionActive = false;
            ++popupSessionRevision;
            closePopupWindow();
            return;
        }

        auto menu = *getRootMenu();
        if (menu.getNumItems() > 0)
        {
            const int selectedId = getSelectedId();
            for (juce::PopupMenu::MenuItemIterator iterator(menu, true);
                 iterator.next();)
            {
                auto& item = iterator.getItem();
                if (item.itemID != 0)
                    item.isTicked = item.itemID == selectedId;
            }
        }
        else
        {
            menu.addItem(1, getTextWhenNoChoicesAvailable(), false, false);
        }

        auto& lookAndFeel = getLookAndFeel();
        menu.setLookAndFeel(&lookAndFeel);

        auto options = juce::PopupMenu::Options()
                           .withTargetComponent(this)
                           .withItemThatMustBeVisible(getSelectedId())
                           .withInitiallySelectedItem(getSelectedId())
                           .withMinimumWidth(getWidth())
                           .withMaximumNumColumns(1)
                           .withStandardItemHeight(getHeight());

        for (auto* child : getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*>(child))
            {
                options = lookAndFeel.getOptionsForComboBoxPopupMenu(
                    *this, *label);
                break;
            }
        }

        menu.showMenuAsync(
            options,
            createPopupResultHandler(requestContextRevision));
    }

    StateComponent::StateComponent(StateAB& sab, StatePresets& sp, juce::AudioProcessorValueTreeState& vts)
        : procStateAB { sab },
          procStatePresets { sp },
          valueTreeState { vts },
          manualUpdateCheckThread { *this },
          toggleABButton { "A" },
          copyABButton { "Copy" },
          previousButton { "" },
          nextButton { "" },
          savePresetButton { "Save" },
          //deletePresetButton{"Delete"},
          menuButton { "Menu" }
    {
        auto& params = procStatePresets.getProcessor().getParameters();
        for (auto param : params)
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                valueTreeState.addParameterListener(p->paramID, this);
            }
        }
        addAndMakeVisible(toggleABButton);
        addAndMakeVisible(copyABButton);
        addAndMakeVisible(loudnessMatchControls);
        const juce::Component::SafePointer<StateComponent> safeMatchOwner(this);
        loudnessMatchControls.onEnabledChanged = [safeMatchOwner](bool enabled)
        {
            if (! safeMatchOwner)
                return;
            auto& processor = static_cast<FireAudioProcessor&>(safeMatchOwner->procStatePresets.getProcessor());
            if (safeMatchOwner->loudnessMatchControls.getState().enabled != processor.getLoudnessMatchState().enabled)
            {
                safeMatchOwner->updateLoudnessMatchState();
                return;
            }
            processor.setLoudnessMatchEnabled(enabled);
            if (safeMatchOwner)
                safeMatchOwner->updateLoudnessMatchState();
        };
        loudnessMatchControls.onLearn = [safeMatchOwner]
        {
            if (! safeMatchOwner)
                return;
            auto& processor = static_cast<FireAudioProcessor&>(safeMatchOwner->procStatePresets.getProcessor());
            const auto shown = safeMatchOwner->loudnessMatchControls.getState();
            const auto current = processor.getLoudnessMatchState();
            if (shown.side != current.side || shown.enabled != current.enabled
                || shown.measuring != current.measuring || shown.bypassed != current.bypassed)
            {
                safeMatchOwner->updateLoudnessMatchState();
                return;
            }
            processor.learnLoudnessMatch();
            if (safeMatchOwner)
                safeMatchOwner->updateLoudnessMatchState();
        };
        updateLoudnessMatchState();
        toggleABButton.addListener(this);
        copyABButton.addListener(this);

        addAndMakeVisible(previousButton);
        addAndMakeVisible(nextButton);
        previousButton.addListener(this);
        nextButton.addListener(this);

        addAndMakeVisible(presetBox);

        presetBox.setComponentID("header_preset");
        presetBox.setTitle("Preset browser");
        presetBox.setTooltip("Select a preset");
        presetBox.setJustificationType(juce::Justification::centred);
        presetBox.setColour(juce::ComboBox::textColourId, fire::ui::colours::textPrimary);
        presetBox.setColour(juce::ComboBox::arrowColourId, fire::ui::colours::flame);
        presetBox.setColour(juce::ComboBox::buttonColourId, fire::ui::colours::flame);
        presetBox.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
        presetBox.setColour(juce::ComboBox::focusedOutlineColourId, fire::ui::colours::ember);
        presetBox.setColour(juce::ComboBox::backgroundColourId, fire::ui::colours::surface0);
        presetBox.setTextWhenNothingSelected("- Init -");

        synchronisePresetSelectionFromManager();

        addAndMakeVisible(savePresetButton);
        savePresetButton.addListener(this);
        //addAndMakeVisible(deletePresetButton);
        //deletePresetButton.addListener(this);
        addAndMakeVisible(menuButton);
        menuButton.addListener(this);

        auto styleHeaderButton = [](juce::TextButton& button, juce::Colour accent)
        {
            button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
            button.setColour(juce::TextButton::textColourOnId, accent);
            button.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        };

        styleHeaderButton(toggleABButton, fire::ui::colours::gold);
        toggleABButton.setComponentID("header_ab");
        toggleABButton.setTitle("A/B state");
        toggleABButton.setTooltip("Switch between the A and B states");
        toggleABButton.setColour(juce::TextButton::textColourOffId,
                                 fire::ui::colours::gold.withAlpha(0.84f));
        styleHeaderButton(copyABButton, fire::ui::colours::flame);
        copyABButton.setComponentID("header_action");
        copyABButton.setTitle("Copy A/B state");
        copyABButton.setTooltip(
            "Copy the current state to the other A/B slot");
        styleHeaderButton(previousButton, fire::ui::colours::flame);
        previousButton.setComponentID("header_previous");
        previousButton.setTooltip("Previous preset");
        previousButton.setTitle("Previous preset");
        styleHeaderButton(nextButton, fire::ui::colours::flame);
        nextButton.setComponentID("header_next");
        nextButton.setTooltip("Next preset");
        nextButton.setTitle("Next preset");
        styleHeaderButton(savePresetButton, fire::ui::colours::positive);
        savePresetButton.setComponentID("header_action");
        savePresetButton.setTitle("Save preset");
        savePresetButton.setTooltip("Save preset");
        savePresetButton.setColour(juce::TextButton::textColourOffId,
                                   fire::ui::colours::positive.withAlpha(0.86f));
        //deletePresetButton.setColour(TextButton::textColourOffId, COLOUR1);
        //deletePresetButton.setColour(TextButton::buttonColourId, COLOUR5);
        //deletePresetButton.setColour(ComboBox::outlineColourId, COLOUR5);
        styleHeaderButton(menuButton, fire::ui::colours::flame);
        menuButton.setComponentID("header_menu");
        menuButton.setTooltip("Preset and application menu");
        menuButton.setTitle("Preset and application menu");
        toggleABButton.setButtonText(procStateAB.isCurrentA() ? "A" : "B");
        startTimerHz(30);
    }

    StateComponent::~StateComponent()
    {
        loudnessMatchControls.onEnabledChanged = nullptr;
        loudnessMatchControls.onLearn = nullptr;
        loudnessMatchControls.dismiss();
        stopTimer();
        invalidateManualUpdateRequest();
        manualUpdateCheckThread.stop();
        invalidateManualUpdateAlert();
        invalidateSaveErrorAlert();
        presetBox.dismissTransientInteraction();
        invalidatePresetMenuSession();
        dismissSettingsDialog();
        presetMenu.setLookAndFeel(nullptr);

        cancelPendingUpdate();
        invalidateSaveChooserSession();
        presetBox.onChange = nullptr;

        // Remove listeners from all buttons that had them added in the constructor
        toggleABButton.removeListener(this);
        copyABButton.removeListener(this);
        previousButton.removeListener(this);
        nextButton.removeListener(this);
        savePresetButton.removeListener(this);
        menuButton.removeListener(this);

        auto& params = procStatePresets.getProcessor().getParameters();
        for (auto param : params)
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                valueTreeState.removeParameterListener(p->paramID, this);
            }
        }
    }

    void StateComponent::parameterChanged(const juce::String& parameterID, float newValue)
    {
        juce::ignoreUnused(newValue);

        if (fire::clouds_params::isReservedEngineParameterID(parameterID)
            || programmaticChangeDepth.load(std::memory_order_acquire) > 0)
            return;

        dirtyUpdatePending.store(true, std::memory_order_release);
    }

    void StateComponent::timerCallback()
    {
        if (dirtyUpdatePending.exchange(false, std::memory_order_acq_rel))
            markAsDirty();
    }

    void StateComponent::beginProgrammaticChange()
    {
        dirtyUpdatePending.store(false, std::memory_order_release);
        programmaticChangeDepth.fetch_add(1, std::memory_order_acq_rel);
    }

    void StateComponent::endProgrammaticChange()
    {
        dirtyUpdatePending.store(false, std::memory_order_release);
        const int previousDepth = programmaticChangeDepth.fetch_sub(1, std::memory_order_acq_rel);
        jassert(previousDepth > 0);
        if (previousDepth <= 0)
            programmaticChangeDepth.store(0, std::memory_order_release);
    }

    void StateComponent::handleAsyncUpdate()
    {
        if (dirtyUpdatePending.exchange(false, std::memory_order_acq_rel))
            markAsDirty();

        if (versionCheckReady.exchange(false, std::memory_order_acq_rel))
            showManualUpdateResult();
    }

    std::uint64_t StateComponent::beginManualUpdateRequest()
    {
        // A fresh check supersedes any result dialog from the previous check.
        invalidateManualUpdateAlert();

        const juce::ScopedLock lock(updateResultLock);
        ++manualUpdateRequestGeneration;
        manualUpdateRequestActive = true;
        pendingVersionInfo.reset();
        pendingManualUpdateRequestGeneration = 0;
        versionCheckReady.store(false, std::memory_order_release);
        return manualUpdateRequestGeneration;
    }

    void StateComponent::invalidateManualUpdateRequest() noexcept
    {
        {
            const juce::ScopedLock lock(updateResultLock);
            manualUpdateRequestActive = false;
            ++manualUpdateRequestGeneration;
            pendingVersionInfo.reset();
            pendingManualUpdateRequestGeneration = 0;
            versionCheckReady.store(false, std::memory_order_release);
        }

        // Cancellation is non-blocking here. Destruction additionally joins
        // the worker before any owner state can be released.
        manualUpdateCheckThread.cancel();
    }

    void StateComponent::publishManualUpdateResult(
        std::unique_ptr<VersionInfo> result,
        std::uint64_t requestGeneration)
    {
        {
            const juce::ScopedLock lock(updateResultLock);
            if (! manualUpdateRequestActive
                || manualUpdateRequestGeneration != requestGeneration)
                return;

            pendingVersionInfo = std::move(result);
            pendingManualUpdateRequestGeneration = requestGeneration;
            versionCheckReady.store(true, std::memory_order_release);
        }

        triggerAsyncUpdate();
    }

    void StateComponent::showManualUpdateResult()
    {
        std::unique_ptr<VersionInfo> result;
        {
            const juce::ScopedLock lock(updateResultLock);
            if (! manualUpdateRequestActive
                || pendingManualUpdateRequestGeneration == 0
                || pendingManualUpdateRequestGeneration
                       != manualUpdateRequestGeneration)
            {
                pendingVersionInfo.reset();
                pendingManualUpdateRequestGeneration = 0;
                return;
            }

            result = std::move(pendingVersionInfo);
            manualUpdateRequestActive = false;
            ++manualUpdateRequestGeneration;
            pendingManualUpdateRequestGeneration = 0;
            versionCheckReady.store(false, std::memory_order_release);
        }

        if (! isShowing() || ! isEnabled())
            return;

        if (result == nullptr)
        {
            showManualUpdateAlert(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Error")
                    .withMessage(
                        "No release found or disconnected from the network!")
                    .withButton("OK"));
            return;
        }

        if (result->isNewerVersionThanCurrent())
        {
            const auto versionToDownload = result->versionString;
            showManualUpdateAlert(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::InfoIcon)
                    .withTitle("New Version")
                    .withMessage("New version " + versionToDownload
                                 + " available, do you want to download it?")
                    .withButton("OK")
                    .withButton("Cancel"),
                versionToDownload);
            return;
        }

        showManualUpdateAlert(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("New Version")
                .withMessage("You are up to date!")
                .withButton("OK"));
    }

    void StateComponent::showManualUpdateAlert(
        const juce::MessageBoxOptions& options,
        juce::String versionToDownload)
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);
        invalidateManualUpdateAlert();
        if (safeThis == nullptr
            || ! safeThis->isShowing()
            || ! safeThis->isEnabled())
            return;

        safeThis->manualUpdateAlertActive = true;
        const auto alertGeneration = safeThis->manualUpdateAlertGeneration;
        const auto ownedOptions = options
                                      .withAssociatedComponent(safeThis.getComponent())
                                      .withParentComponent(safeThis.getComponent());
        const auto callback = [safeThis,
                               alertGeneration,
                               versionToDownload = std::move(versionToDownload)](
                                  int result)
        {
            if (safeThis != nullptr)
                safeThis->handleManualUpdateAlertResult(
                    result, alertGeneration, versionToDownload);
        };

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (safeThis->manualUpdateDialogPresenterForTesting)
        {
            auto presenter = safeThis->manualUpdateDialogPresenterForTesting;
            auto closer = presenter(ownedOptions, callback);
            if (safeThis == nullptr)
            {
                if (closer)
                    closer();
                return;
            }

            if (safeThis->manualUpdateAlertActive
                && safeThis->manualUpdateAlertGeneration == alertGeneration)
            {
                safeThis->manualUpdateDialogCloserForTesting =
                    std::move(closer);
            }
            else if (closer)
            {
                closer();
            }
            return;
        }
#endif

        auto alert = juce::NativeMessageBox::showScopedAsync(ownedOptions,
                                                              callback);
        if (safeThis != nullptr
            && safeThis->manualUpdateAlertActive
            && safeThis->manualUpdateAlertGeneration == alertGeneration)
        {
            safeThis->manualUpdateAlert = std::move(alert);
        }
        else
        {
            alert.close();
        }
    }

    void StateComponent::handleManualUpdateAlertResult(
        int result,
        std::uint64_t alertGeneration,
        const juce::String& versionToDownload)
    {
        if (! manualUpdateAlertActive
            || manualUpdateAlertGeneration != alertGeneration)
            return;

        const bool shouldLaunchDownload = result == 1
                                          && versionToDownload.isNotEmpty()
                                          && isShowing()
                                          && isEnabled();
        const juce::Component::SafePointer<StateComponent> safeThis(this);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        auto launcher = manualUpdateUrlLauncherForTesting;
        auto closer = std::move(manualUpdateDialogCloserForTesting);
        manualUpdateDialogCloserForTesting = nullptr;
#endif

        manualUpdateAlertActive = false;
        ++manualUpdateAlertGeneration;
        manualUpdateAlert.close();
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (closer)
            closer();
#endif

        if (! shouldLaunchDownload || safeThis == nullptr)
            return;

        const juce::URL downloadUrl(GITHUB_TAG_LINK + versionToDownload);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (launcher)
        {
            launcher(downloadUrl);
            return;
        }
#endif
        downloadUrl.launchInDefaultBrowser();
    }

    void StateComponent::invalidateManualUpdateAlert() noexcept
    {
        manualUpdateAlertActive = false;
        ++manualUpdateAlertGeneration;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        auto closer = std::move(manualUpdateDialogCloserForTesting);
        manualUpdateDialogCloserForTesting = nullptr;
#endif
        manualUpdateAlert.close();
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (closer)
            closer();
#endif
    }

    void StateComponent::paint(juce::Graphics& g)
    {
        g.setColour(fire::ui::colours::hairline.withAlpha(0.5f));
        for (const auto x : { copyABButton.getRight() + 3, savePresetButton.getX() - 3 })
            g.drawVerticalLine(x, getHeight() * 0.32f, getHeight() * 0.68f);
    }

    void StateComponent::resized()
    {
        const auto uiScale = juce::jmax(0.5f, static_cast<float>(getHeight()) / 48.0f);
        const auto gap = juce::jmax(3, juce::roundToInt(6.0f * uiScale));
        const auto controlHeight = juce::jmin(getHeight(),
                                              juce::roundToInt(32.0f * uiScale));
        const auto compactWidth = juce::jmax(28, juce::roundToInt(32.0f * uiScale));
        const auto actionWidth = juce::jmax(42, juce::roundToInt(46.0f * uiScale));
        auto r = getLocalBounds().withSizeKeepingCentre(getWidth(), controlHeight);

        auto placeLeft = [&](juce::Component& component, int width)
        {
            component.setBounds(r.removeFromLeft(width));
            r.removeFromLeft(gap);
        };
        auto placeRight = [&](juce::Component& component, int width)
        {
            component.setBounds(r.removeFromRight(width));
            r.removeFromRight(gap);
        };

        placeLeft(toggleABButton, compactWidth);
        placeLeft(copyABButton, actionWidth);
        placeLeft(loudnessMatchControls, juce::roundToInt(164.0f * uiScale));
        placeRight(menuButton, actionWidth);
        placeRight(savePresetButton, actionWidth);
        placeRight(nextButton, compactWidth);
        placeRight(previousButton, compactWidth);
        presetBox.setBounds(r);
    }

    void StateComponent::visibilityChanged()
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);
        juce::Component::visibilityChanged();

        if (safeThis != nullptr && ! safeThis->isShowing())
            safeThis->dismissPointerGestures();
    }

    void StateComponent::enablementChanged()
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);
        juce::Component::enablementChanged();
        if (safeThis == nullptr || safeThis->isEnabled())
            return;

        safeThis->dismissPointerGestures();
    }

    void StateComponent::buttonClicked(juce::Button* clickedButton)
    {
        //    if (clickedButton == &toggleABButton)
        //    {
        //        the code is moved to Editor
        //    }
        if (clickedButton == &copyABButton)
        {
            // copyAB notifies the host synchronously and may delete this
            // component. Returning immediately leaves no stale member access.
            procStateAB.copyAB();
            return;
        }

        if (clickedButton == &previousButton)
        {
            setPreviousPreset();
            return;
        }

        if (clickedButton == &nextButton)
        {
            setNextPreset();
            return;
        }

        if (clickedButton == &savePresetButton)
        {
            savePresetAlertWindow();
            return;
        }
        //if (clickedButton == &deletePresetButton)
        //    deletePresetAndRefresh();
        if (clickedButton == &menuButton)
        {
            popPresetMenu();
            return;
        }
    }

    void StateComponent::markAsDirty()
    {
        // Only perform the operation if a "clean" preset is currently selected (ID > 0 and does not have a *).
        if (presetBox.getSelectedId() > 0 && ! presetBox.getText().endsWith("*"))
        {
            const auto currentText = presetBox.getText();

            // 1. First, set the selectedId to 0 in the background.
            //    This makes the ComboBox's internal state believe "nothing is selected," preparing it to fire onChange
            //    for the next step when we select the same item.
            presetBox.setSelectedId(0, juce::dontSendNotification);

            // 2. Then, immediately overwrite the default text ("- Init -") that might appear due to ID=0
            //    with our desired "dirty" text.
            presetBox.setText(currentText + "*", juce::dontSendNotification);
        }
    }

    void StateComponent::setPreviousPreset()
    {
        int presetIndex = procStatePresets.getCurrentPresetId() - 1;
        if (presetIndex > 0)
        {
            const juce::Component::SafePointer<StateComponent> safeThis(this);
            presetBox.dismissTransientInteraction();
            if (safeThis == nullptr)
                return;

            // Loading a preset may synchronously close the editor. Keep this
            // synchronous selection as the final component operation so its
            // numeric ID cannot cross a later preset-list revision.
            presetBox.setSelectedId(presetIndex,
                                    juce::sendNotificationSync);
        }
    }

    void StateComponent::setNextPreset()
    {
        int presetIndex = procStatePresets.getCurrentPresetId() + 1;
        if (presetIndex <= procStatePresets.getNumPresets())
        {
            const juce::Component::SafePointer<StateComponent> safeThis(this);
            presetBox.dismissTransientInteraction();
            if (safeThis == nullptr)
                return;

            // See setPreviousPreset(): no queued ComboBox notification may
            // reinterpret this item ID after a rescan or state restore.
            presetBox.setSelectedId(presetIndex,
                                    juce::sendNotificationSync);
        }
    }

    void StateComponent::comboBoxChanged(juce::ComboBox* changedComboBox)
    {
        juce::ignoreUnused(changedComboBox);
    }

    void StateComponent::updatePresetBox(int selectedId) // when preset is changed
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);

        if (selectedId > 0)
        {
            // Get preset name
            juce::String presetName = presetBox.getItemText(presetBox.indexOfItemId(selectedId));

            // If the name ends with '*', it means the user is trying to restore a preset
            if (presetName.endsWith("*"))
            {
                // Remove '*' to get the actual preset name
                presetName = presetName.dropLastCharacters(1);
            }

            auto* presetManager = &procStatePresets;

            const juce::String internalIdToLoad = presetManager->comboBoxIdToTagNameMap[selectedId];

            if (internalIdToLoad.isNotEmpty())
            {
                beginProgrammaticChange();
                const juce::ScopeGuard finishProgrammaticLoad { [safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->endProgrammaticChange();
                } };
                bool presetWasLoaded = false;
                {
                    auto& fireProc = static_cast<FireAudioProcessor&>(
                        presetManager->getProcessor());
                    fireProc.beginMultibandTopologyEdit();
                    const juce::ScopeGuard finishTopologyEdit { [&fireProc]
                    {
                        fireProc.requestMultibandTopologyReset();
                    } };

                    // Identity is part of the same serializable generation as
                    // the preset's parameters, shapes and routings.
                    presetManager->setCurrentPresetId(selectedId);
                    presetWasLoaded = presetManager->loadPreset(
                        internalIdToLoad, false);
                    if (! presetWasLoaded)
                        presetManager->setCurrentPresetId(0);
                }

                if (! presetWasLoaded)
                    return;

                if (safeThis != nullptr)
                    safeThis->requestFocusResetAfterStateLoad();

                // The UI owns an outer transaction which also includes the
                // selected preset ID. Notify only after that transaction has
                // published the complete sound and identity generation.
                presetManager->getProcessor().updateHostDisplay(
                    juce::AudioProcessorListener::ChangeDetails {}
                        .withNonParameterStateChanged(true));

                if (safeThis == nullptr)
                    return;

                const juce::String loadedPresetName =
                    safeThis->presetBox.getItemText(
                        safeThis->presetBox.indexOfItemId(selectedId));

                safeThis->presetBox.setText(
                    loadedPresetName, juce::dontSendNotification);

                safeThis->presetBox.setSelectedId(
                    selectedId, juce::dontSendNotification);
            }
            else
            {
                presetManager->setCurrentPresetId(0);
                jassertfalse;
            }
        }
    }

    void StateComponent::refreshPresetBox() // rescan, init, save, or delete
    {
        // clear()/repopulation can reuse the same numeric IDs for different
        // files. Invalidate any queued opener or result before the first item
        // and the manager's ID maps are changed.
        presetBox.invalidateMenuContents();
        presetBox.clear(juce::dontSendNotification);
        procStatePresets.setPresetAndFolderNames(presetBox);
    }

    void StateComponent::synchronisePresetSelectionFromManager()
    {
        // A processor state restore changes the live state first and then
        // publishes its stable preset key. Rebuild the menu mapping and only
        // reflect that state in the ComboBox; this path must never load a file.
        dirtyUpdatePending.store(false, std::memory_order_release);
        refreshPresetBox();

        const auto currentPresetKey = procStatePresets.getCurrentPresetKey();
        const int currentPresetId = procStatePresets.getCurrentPresetId();
        if (currentPresetKey.isEmpty()
            || ! juce::isPositiveAndBelow(currentPresetId, procStatePresets.getNumPresets() + 1))
        {
            presetBox.setSelectedId(0, juce::dontSendNotification);
            presetBox.setText({}, juce::dontSendNotification);
            return;
        }

        std::function<const juce::XmlElement*(const juce::XmlElement&)> findPresetByKey =
            [&](const juce::XmlElement& parentXml) -> const juce::XmlElement*
        {
            for (auto* child : parentXml.getChildIterator())
            {
                if (child->hasAttribute("presetName")
                    && child->getStringAttribute("presetKey") == currentPresetKey)
                    return child;

                if (child->hasTagName("FOLDER"))
                    if (auto* found = findPresetByKey(*child))
                        return found;
            }

            return nullptr;
        };

        const auto* presetXml = findPresetByKey(procStatePresets.getPresetXml());
        if (presetXml == nullptr)
        {
            presetBox.setSelectedId(0, juce::dontSendNotification);
            presetBox.setText({}, juce::dontSendNotification);
            return;
        }

        const int itemIndex = presetBox.indexOfItemId(currentPresetId);
        const auto presetName = itemIndex >= 0
                                    ? presetBox.getItemText(itemIndex)
                                    : presetXml->getStringAttribute("presetName");
        presetBox.setSelectedId(currentPresetId, juce::dontSendNotification);
        presetBox.setText(presetName, juce::dontSendNotification);

        auto& fireProc = static_cast<FireAudioProcessor&>(procStatePresets.getProcessor());
        if (! fireProc.isCurrentStateEquivalentToPreset(*presetXml))
        {
            markAsDirty();
        }

        // Discard parameter callbacks already queued by the host restore so a
        // just-reconciled clean preset is not dirtied on the next timer tick.
        dirtyUpdatePending.store(false, std::memory_order_release);
    }

    void StateComponent::synchroniseABButtonFromManager()
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);
        toggleABButton.setButtonText(procStateAB.isCurrentA() ? "A" : "B");
        if (safeThis)
            safeThis->updateLoudnessMatchState();
    }

    void StateComponent::updateLoudnessMatchState()
    {
        const auto view = static_cast<FireAudioProcessor&>(procStatePresets.getProcessor()).getLoudnessMatchState();
        loudnessMatchControls.setState({view.enabled, view.measuring, view.ready, view.limited,
            view.noSignal, view.bypassed, view.side, view.gainDb, view.progress});
    }

    void StateComponent::deletePresetAndRefresh()
    {
        if (procStatePresets.getNumPresets() > 0)
        {
            juce::Component::SafePointer<StateComponent> safeThis(this);
            const auto callback = juce::ModalCallbackFunction::create([safeThis](int choice)
                                                                      {
                                                                          if (choice != 0 && safeThis != nullptr)
                                                                          {
                                                                              safeThis->procStatePresets.deletePreset();
                                                                              safeThis->refreshPresetBox();
                                                                          }
                                                                      });
            juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::NoIcon,
                                                    "Warning",
                                                    "Delete preset?",
                                                    nullptr,
                                                    callback);
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::NoIcon, "Warning", "No preset!");
        }
    }

    void StateComponent::savePresetAlertWindow()
    {
        if (fileChooserSessionActive || fileChooser != nullptr)
            return;

        juce::File userFile = procStatePresets.getFile().getChildFile("User");
        creatFolderIfNotExist(userFile);
        if (! userFile.isDirectory())
        {
            showSaveErrorAlert("The preset folder could not be created.");
            return;
        }

        auto chooser = std::make_shared<juce::FileChooser>(
            "Save preset", userFile, "*.fire", true, false, this);
        fileChooser = chooser;
        fileChooserSessionActive = true;
        const auto sessionGeneration = ++fileChooserSessionGeneration;
        const auto folderChooserFlags = juce::FileBrowserComponent::saveMode
                                        | juce::FileBrowserComponent::canSelectFiles;
        auto resultHandler = createSaveChooserResultHandler(
            chooser, sessionGeneration);

        // Keep the launch call free of member access after JUCE/native code:
        // opening a platform dialog may synchronously close the editor.
        chooser->launchAsync(
            folderChooserFlags,
            [resultHandler = std::move(resultHandler)](
                const juce::FileChooser& completedChooser)
            {
                resultHandler(completedChooser.getResult());
            });
    }

    std::function<void(const juce::File&)>
    StateComponent::createSaveChooserResultHandler(
        std::weak_ptr<juce::FileChooser> expectedChooser,
        std::uint64_t expectedGeneration)
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);
        return [safeThis,
                expectedChooser = std::move(expectedChooser),
                expectedGeneration](const juce::File& inputName)
        {
            auto chooserKeepAlive = expectedChooser.lock();
            if (chooserKeepAlive == nullptr)
                return;

            // FileChooser::finished() invokes client code from a member stack.
            // If a nested dialog or host callback destroys/hides the editor,
            // its owner reference is released immediately, while this local
            // reference keeps the chooser alive. Transfer that last reference
            // to the next message only as this callback exits.
            const juce::ScopeGuard releaseChooserAfterCallback { [chooserKeepAlive]
            {
                juce::MessageManager::callAsync([chooserKeepAlive] {});
            } };

            if (safeThis == nullptr
                || ! safeThis->isSaveChooserSessionCurrent(
                    expectedGeneration, chooserKeepAlive.get()))
                return;

            safeThis->handleSaveChooserResult(
                inputName, expectedGeneration, chooserKeepAlive.get());
        };
    }

    bool StateComponent::isSaveChooserSessionCurrent(
        std::uint64_t expectedGeneration,
        const juce::FileChooser* expectedChooser) const noexcept
    {
        return fileChooserSessionActive
            && fileChooserSessionGeneration == expectedGeneration
            && fileChooser.get() == expectedChooser
            && isEnabled()
            && isShowing();
    }

    void StateComponent::handleSaveChooserResult(
        const juce::File& inputName,
        std::uint64_t expectedGeneration,
        const juce::FileChooser* expectedChooser)
    {
        if (! isSaveChooserSessionCurrent(expectedGeneration,
                                           expectedChooser))
            return;

        if (inputName == juce::File())
        {
            invalidateSaveChooserSession();
            return;
        }

        // Confirm against the final path after appending .fire. Native
        // choosers do not consistently apply the extension before their
        // overwrite check, which could otherwise silently replace an
        // existing preset when the user omitted the suffix.
        const auto finalPath = inputName.hasFileExtension(PRESET_EXETENSION)
                                   ? inputName
                                   : juce::File(inputName.getFullPathName()
                                                + PRESET_EXETENSION);
        juce::Component::SafePointer<StateComponent> safeThis(this);
        const bool mayOverwrite = ! finalPath.existsAsFile()
                                  || juce::NativeMessageBox::showOkCancelBox(
                                      juce::AlertWindow::WarningIcon,
                                      "Replace preset?",
                                      "\"" + finalPath.getFileName()
                                          + "\" already exists. Replacing it will overwrite its current contents.",
                                      this,
                                      nullptr);

        // A synchronous native dialog runs a nested event loop. It may hide
        // or destroy this owner and may even start a replacement chooser.
        if (safeThis == nullptr
            || ! safeThis->isSaveChooserSessionCurrent(
                expectedGeneration, expectedChooser))
            return;

        if (! mayOverwrite)
        {
            safeThis->invalidateSaveChooserSession();
            return;
        }

        auto& presets = safeThis->procStatePresets;

        // Consume the exact chooser before serialising. Re-entrant UI code may
        // now start a new chooser, which this old callback must never clear.
        safeThis->invalidateSaveChooserSession();
        const juce::String savedPresetName = presets.savePreset(finalPath, true);

        // State capture and host observers may synchronously delete the editor.
        if (safeThis == nullptr)
            return;

        if (savedPresetName.isNotEmpty())
        {
            safeThis->dirtyUpdatePending.store(false,
                                               std::memory_order_release);
            safeThis->refreshPresetBox();
            if (safeThis == nullptr)
                return;

            const int newPresetIdToSelect =
                safeThis->procStatePresets.getCurrentPresetId();
            if (newPresetIdToSelect > 0)
            {
                safeThis->presetBox.setSelectedId(
                    newPresetIdToSelect, juce::dontSendNotification);
                safeThis->presetBox.setText(
                    savedPresetName, juce::dontSendNotification);
            }
        }
        else
        {
            safeThis->showSaveErrorAlert(
                "The preset could not be written to the selected location.");
        }
    }

    void StateComponent::invalidateSaveChooserSession() noexcept
    {
        fileChooserSessionActive = false;
        ++fileChooserSessionGeneration;
        fileChooser.reset();
    }

    void StateComponent::showSaveErrorAlert(juce::String message)
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);
        invalidateSaveErrorAlert();
        if (safeThis == nullptr
            || ! safeThis->isShowing()
            || ! safeThis->isEnabled())
            return;

        safeThis->saveErrorAlertActive = true;
        const auto alertGeneration = safeThis->saveErrorAlertGeneration;
        const auto options = juce::MessageBoxOptions()
                                 .withIconType(
                                     juce::MessageBoxIconType::WarningIcon)
                                 .withTitle("Preset save failed")
                                 .withMessage(std::move(message))
                                 .withButton("OK")
                                 .withAssociatedComponent(
                                     safeThis.getComponent())
                                 .withParentComponent(
                                     safeThis.getComponent());
        const auto callback = [safeThis, alertGeneration](int result)
        {
            if (safeThis != nullptr)
                safeThis->handleSaveErrorAlertResult(
                    result, alertGeneration);
        };

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (safeThis->saveErrorDialogPresenterForTesting)
        {
            auto presenter = safeThis->saveErrorDialogPresenterForTesting;
            auto closer = presenter(options, callback);
            if (safeThis == nullptr)
            {
                if (closer)
                    closer();
                return;
            }

            if (safeThis->saveErrorAlertActive
                && safeThis->saveErrorAlertGeneration == alertGeneration)
            {
                safeThis->saveErrorDialogCloserForTesting =
                    std::move(closer);
            }
            else if (closer)
            {
                closer();
            }
            return;
        }
#endif

        auto alert = juce::NativeMessageBox::showScopedAsync(options,
                                                              callback);
        if (safeThis != nullptr
            && safeThis->saveErrorAlertActive
            && safeThis->saveErrorAlertGeneration == alertGeneration)
        {
            safeThis->saveErrorAlert = std::move(alert);
        }
        else
        {
            alert.close();
        }
    }

    void StateComponent::handleSaveErrorAlertResult(
        int result,
        std::uint64_t alertGeneration)
    {
        juce::ignoreUnused(result);
        if (! saveErrorAlertActive
            || saveErrorAlertGeneration != alertGeneration)
            return;

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        auto closer = std::move(saveErrorDialogCloserForTesting);
        saveErrorDialogCloserForTesting = nullptr;
#endif
        saveErrorAlertActive = false;
        ++saveErrorAlertGeneration;
        saveErrorAlert.close();
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (closer)
            closer();
#endif
    }

    void StateComponent::invalidateSaveErrorAlert() noexcept
    {
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        auto closer = std::move(saveErrorDialogCloserForTesting);
        saveErrorDialogCloserForTesting = nullptr;
#endif
        saveErrorAlertActive = false;
        ++saveErrorAlertGeneration;
        saveErrorAlert.close();
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        if (closer)
            closer();
#endif
    }

    void StateComponent::openPresetFolder()
    {
        // open preset folder
        juce::File userFile = procStatePresets.getFile();
        creatFolderIfNotExist(userFile);
        if (! userFile.existsAsFile())
        {
            juce::File(userFile).startAsProcess();
        }
    }

    void StateComponent::rescanPresetFolder()
    {
        const auto previouslySelectedKey = procStatePresets.getCurrentPresetKey();
        const auto previousText = presetBox.getText();
        const bool wasDirty = previousText.endsWith("*");

        procStatePresets.scanAllPresets();
        procStatePresets.setCurrentPresetKey(previouslySelectedKey);
        // Reconcile the rebuilt entry with the current processor state. A
        // rescan is display-only: an externally replaced preset must make the
        // live sound dirty rather than silently loading the new file.
        synchronisePresetSelectionFromManager();

        const int newPresetIdToSelect = procStatePresets.getCurrentPresetId();
        if (newPresetIdToSelect > 0)
        {
            // synchronisePresetSelectionFromManager() already selected a
            // semantically matching file or marked a mismatch dirty. Preserve
            // a pre-existing edit even if the replacement happens to match it.
            if (wasDirty)
                markAsDirty();
        }
        else if (previouslySelectedKey.isNotEmpty() || wasDirty)
        {
            // The selected backing file was removed or became invalid. Keep
            // its stable identity and an honest dirty label so a rescan never
            // makes the still-live sound appear to be Init.
            auto dirtyText = previousText;
            if (dirtyText.isEmpty())
                dirtyText = juce::File(previouslySelectedKey)
                                .getFileNameWithoutExtension();
            if (! dirtyText.endsWith("*"))
                dirtyText += "*";

            presetBox.setSelectedId(0, juce::dontSendNotification);
            presetBox.setText(dirtyText, juce::dontSendNotification);
        }
    }

    void StateComponent::creatFolderIfNotExist(juce::File userFile)
    {
        if (! userFile.exists())
        {
            userFile.createDirectory();
        }
    }

    juce::String StateComponent::getPresetName()
    {
        return procStatePresets.getPresetName();
    }

    float StateComponent::getPresetMenuScale() const noexcept
    {
        constexpr float minimumScale = 1.0f;
        constexpr float maximumScale = 2.0f;

        const auto* scaleSource = getTopLevelComponent();
        if (scaleSource == nullptr
            || scaleSource->getWidth() <= 0
            || scaleSource->getHeight() <= 0)
            return minimumScale;

        const auto widthScale = static_cast<float>(scaleSource->getWidth()) / INIT_WIDTH;
        const auto heightScale = static_cast<float>(scaleSource->getHeight()) / INIT_HEIGHT;
        return juce::jlimit(minimumScale, maximumScale,
                            juce::jmin(widthScale, heightScale));
    }

    juce::PopupMenu::Options StateComponent::createPresetMenuOptions(float menuScale)
    {
        auto options = juce::PopupMenu::Options()
                           .withTargetComponent(menuButton)
                           .withDeletionCheck(*this)
                           .withStandardItemHeight(
                               juce::roundToInt(30.0f * menuScale))
                           .withMinimumWidth(
                               juce::roundToInt(250.0f * menuScale));

        if (auto* topLevel = getTopLevelComponent();
            topLevel != nullptr && topLevel != &menuButton)
            options = options.withParentComponent(topLevel);

        return options;
    }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    juce::PopupMenu::Options StateComponent::getPresetMenuOptionsForTesting()
    {
        return createPresetMenuOptions(getPresetMenuScale());
    }
#endif

    void StateComponent::popPresetMenu()
    {
        if (! isShowing())
            return;

        auto resultHandler = createPresetMenuResultHandler();
        presetMenu.clear();
        presetMenu.addItem(1, "Init", true);
        presetMenu.addItem(2, "Open Preset Folder", true);
        presetMenu.addItem(3, "Rescan Preset Folder", true);
        presetMenu.addItem(4, "Give a Star on GitHub!", true);
        presetMenu.addItem(5, "Check for New Version", true);
        presetMenu.addItem(6, "Settings", true);

        const auto menuScale = getPresetMenuScale();
        auto menuLookAndFeel = std::make_shared<FireLookAndFeel>();
        menuLookAndFeel->scale = menuScale;
        presetMenu.setLookAndFeel(menuLookAndFeel.get());

        presetMenu.showMenuAsync(createPresetMenuOptions(menuScale),
                                 [consumeResult = std::move(resultHandler),
                                  menuLookAndFeel](int result) mutable
                                 {
                                     juce::ignoreUnused(menuLookAndFeel);
                                     consumeResult(result);
                                 });
    }

    std::function<void(int)> StateComponent::createPresetMenuResultHandler()
    {
        // Only the newest visible menu may issue a command. PopupMenu closes
        // asynchronously, so cancellation from an older menu can arrive after
        // a replacement has already opened.
        invalidatePresetMenuSession();
        const auto sessionGeneration = presetMenuSessionGeneration;
        presetMenuSessionActive = true;

        return [safeThis = juce::Component::SafePointer<StateComponent>(this),
                sessionGeneration](int result)
        {
            if (safeThis == nullptr
                || ! safeThis->presetMenuSessionActive
                || safeThis->presetMenuSessionGeneration != sessionGeneration)
                return;

            // Consume before dispatch. Init and other commands can synchronously
            // notify the host, which is allowed to destroy the complete editor.
            safeThis->presetMenuSessionActive = false;
            ++safeThis->presetMenuSessionGeneration;
            safeThis->presetMenu.setLookAndFeel(nullptr);

            if (! safeThis->isShowing() || ! safeThis->isEnabled())
                return;

            safeThis->handlePresetMenuResult(result);
        };
    }

    void StateComponent::invalidatePresetMenuSession() noexcept
    {
        presetMenuSessionActive = false;
        ++presetMenuSessionGeneration;
        presetMenu.setLookAndFeel(nullptr);
    }

    void StateComponent::handlePresetMenuResult(int result)
    {
        if (result == 1)
        {
            const juce::Component::SafePointer<StateComponent> safeThis(this);
            auto* presetManager = &procStatePresets;
            auto* abState = &procStateAB;
            requestFocusResetAfterStateLoad();
            beginProgrammaticChange();
            const juce::ScopeGuard finishInit { [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->endProgrammaticChange();
            } };

            // Init and the DSP reset are processor-owned operations. Complete
            // both even if a synchronous host notification closes the UI.
            presetManager->initPreset();
            abState->reset();

            if (safeThis != nullptr)
                safeThis->presetBox.setSelectedId(
                    0, juce::dontSendNotification);
            return;
        }

        if (result == 2)
        {
            openPresetFolder();
            return;
        }

        if (result == 3)
        {
            rescanPresetFolder();
            return;
        }

        if (result == 4)
        {
            juce::URL(GITHUB_LINK).launchInDefaultBrowser();
            return;
        }

        if (result == 5)
        {
            if (isShowing() && isEnabled()
                && ! manualUpdateCheckThread.isThreadRunning())
            {
                const auto requestGeneration = beginManualUpdateRequest();
                manualUpdateCheckThread.prepareForStart(requestGeneration);
                if (! manualUpdateCheckThread.startThread())
                    invalidateManualUpdateRequest();
            }
            return;
        }

        if (result == 6)
        {
            showSettingsDialog();
            return;
        }
    }

    void StateComponent::showSettingsDialog()
    {
        const juce::Component::SafePointer<StateComponent> safeThis(this);

        // PopupMenu results are delivered asynchronously. A host may hide the
        // editor after the click but before this callback reaches us.
        if (! safeThis->isShowing() || ! safeThis->isEnabled())
            return;

        if (auto* existingDialog = safeThis->settingsDialog.getComponent())
        {
            if (existingDialog->isShowing()
                && existingDialog->isCurrentlyModal(false))
            {
                existingDialog->toFront(true);
                return;
            }

            // A title-bar close leaves the auto-delete queued until the modal
            // manager's next update. Remove that stale window before reopening.
            safeThis->dismissSettingsDialog();
            if (safeThis == nullptr)
                return;
        }

        const auto launchSessionGeneration =
            safeThis->interactionSessionGeneration;
        juce::Component::SafePointer<juce::DialogWindow> launchedDialog;

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        // Copy the callable before invoking it: test launchers deliberately
        // exercise hosts which destroy the editor while a dialog is opening.
        auto dialogFactory = safeThis->settingsDialogFactoryForTesting;
        if (dialogFactory)
        {
            launchedDialog = dialogFactory();
        }
        else
#endif
        {
            auto& processor = static_cast<FireAudioProcessor&>(
                safeThis->procStatePresets.getProcessor());
            auto settingsPanel = std::make_unique<SettingsComponent>(
                processor.getAppSettings());

            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(settingsPanel.release());
            options.content->setSize(400, 300);
            options.dialogTitle = "Settings";
            options.dialogBackgroundColour = COLOUR6;
            options.escapeKeyTriggersCloseButton = true;
            options.useNativeTitleBar = true;
            options.resizable = true;
            options.componentToCentreAround = safeThis.getComponent();
            launchedDialog = options.launchAsync();
        }

        // launchAsync(), or a host callback reached while it runs, may destroy
        // this StateComponent before returning. Keep the returned auto-delete
        // window local until it is fully configured so no member of a deleted
        // owner is written and an orphaned dialog cannot outlive the processor.
        if (safeThis == nullptr
            || safeThis->interactionSessionGeneration
                   != launchSessionGeneration
            || ! safeThis->isShowing()
            || ! safeThis->isEnabled())
        {
            deleteDialogSynchronously(launchedDialog);
            return;
        }

        if (launchedDialog == nullptr)
            return;

        StateComponent::configureSettingsDialogResizeLimits(*launchedDialog);
        if (safeThis == nullptr
            || safeThis->interactionSessionGeneration
                   != launchSessionGeneration
            || ! safeThis->isShowing()
            || ! safeThis->isEnabled())
        {
            deleteDialogSynchronously(launchedDialog);
            return;
        }

        safeThis->settingsDialog = launchedDialog;
    }

    void StateComponent::configureSettingsDialogResizeLimits(
        juce::DialogWindow& dialog)
    {
        // LaunchOptions makes the window resizable but does not install a
        // lower bound. Without one the fixed-height rows can be reduced to
        // empty rectangles, leaving visible settings clipped or stacked.
        dialog.setResizeLimits(SettingsComponent::minimumDialogWidth,
                               SettingsComponent::minimumDialogHeight,
                               4096,
                               4096);
    }

    void StateComponent::requestFocusResetAfterStateLoad() noexcept
    {
        focusResetAfterStateLoadPending = true;
    }

    bool StateComponent::consumeFocusResetAfterStateLoad() noexcept
    {
        const bool wasPending = focusResetAfterStateLoadPending;
        focusResetAfterStateLoadPending = false;
        return wasPending;
    }

    juce::ComboBox* StateComponent::getPresetBox()
    {
        return &presetBox;
    }

    juce::Button* StateComponent::getToggleABButton()
    {
        return &toggleABButton;
    }

    StatePresets* StateComponent::getProcStatePresets()
    {
        return &procStatePresets;
    }

    StateAB* StateComponent::getProcStateAB()
    {
        return &procStateAB;
    }

    juce::TextButton* StateComponent::getCopyABButton()
    {
        return &copyABButton;
    }

    juce::TextButton* StateComponent::getPreviousButton()
    {
        return &previousButton;
    }

    juce::TextButton* StateComponent::getNextButton()
    {
        return &nextButton;
    }

    void StateComponent::dismissPointerGestures() noexcept
    {
        // The editor calls this when an ancestor is hidden. JUCE does not send
        // visibilityChanged() to every descendant. Treat it as the same full
        // interaction-session boundary as hiding or disabling this component.
        const juce::Component::SafePointer<StateComponent> safeThis(this);
        ++safeThis->interactionSessionGeneration;

        safeThis->loudnessMatchControls.dismiss();
        if (safeThis == nullptr)
            return;

        safeThis->invalidatePresetMenuSession();
        if (safeThis == nullptr)
            return;

        safeThis->invalidateSaveChooserSession();
        if (safeThis == nullptr)
            return;

        safeThis->presetBox.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        safeThis->invalidateManualUpdateRequest();
        if (safeThis == nullptr)
            return;

        safeThis->invalidateManualUpdateAlert();
        if (safeThis == nullptr)
            return;

        safeThis->invalidateSaveErrorAlert();
        if (safeThis == nullptr)
            return;

        safeThis->toggleABButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        safeThis->copyABButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        safeThis->previousButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        safeThis->nextButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        safeThis->savePresetButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        safeThis->menuButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        safeThis->dismissSettingsDialog();
    }

    void StateComponent::dismissSettingsDialog() noexcept
    {
        // launchAsync() otherwise leaves owned SettingsComponent deletion to a
        // later modal-manager update. Its PropertiesFile reference must not
        // survive the processor that owns the application settings.
        auto dialog = settingsDialog;
        settingsDialog = nullptr;

        deleteDialogSynchronously(dialog);
    }

} // namespace state
