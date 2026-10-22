#include "ModuleSnapshotMapping.h"
#include "../PluginProcessor.h"
#include "CoreModuleParameters.h"
#include "AnalogShapeParameters.h"
#include "ReverbModelParameters.h"
#include "CloudsParameters.h"
#include "DriveCompensationParameters.h"
#include "ModulationEffectParameters.h"
#include "ResonatorParameters.h"
#include "DynamicEqParameters.h"
#include <set>

namespace fire::chain
{
namespace
{
using Type = effects::Type;
struct Field
{
    juce::String key, id;
    juce::NormalisableRange<float> range {0, 1}; // Physical units of the owning representation.
    float scale = 1, initial = 0;
    bool discrete = false;
};
using Fields = std::vector<Field>;
bool validType(Type type) { return type > Type::none && type < Type::count; }
juce::String controlKey(int control) { return "control." + juce::String(control + 1); }
juce::String eqKey(int node, int field)
{
    static constexpr const char* fields[]{"frequency", "gain", "q", "slope", "type", "present", "bypassed"};
    return "eq." + juce::String(node) + "." + fields[field];
}
Fields fieldsFor(Type type, bool legacy, int scope = 1, int slot = 0)
{
    Fields fields;
    if (!validType(type)) return fields;
    if (legacy && core_modules::legacyNode(type == Type::lofi || type == Type::eq ? 0 : 1, type) < 0) return fields;
    const auto add = [&](juce::String key, juce::String id, juce::NormalisableRange<float> range,
                         float initial, float scale = 1, bool discrete = false)
    { fields.push_back({std::move(key), std::move(id), range, scale, initial, discrete}); };
    for (int control = 0; control < static_cast<int>(effects::controlCount); ++control)
    {
        const auto& description = effects::controls(type)[static_cast<size_t>(control)];
        add(controlKey(control), legacy ? juce::String() : effects::parameterID(scope, slot, control),
            legacy ? description.range() : juce::NormalisableRange<float>{0, 1},
            legacy ? description.initial : description.toNormalised(description.initial));
    }
    // Inserts store six unit-normalised host parameters; their effective
    // control ranges live in effects::controls, not in the APVTS range.
    const auto bandId = [&](const char* base) { return juce::String(base) + juce::String(scope); };
    const auto bind = [&](int control, const juce::String& id, juce::NormalisableRange<float> range,
                          float initial, float scale = 1, bool discrete = false)
    { fields[static_cast<size_t>(control)] = {controlKey(control), id, range, scale, initial, discrete}; };
    if (legacy)
    {
        if (type == Type::drive)
        {
            bind(0, bandId(DRIVE_ID), {0,100,.01f}, 0);
            bind(1, bandId(SAFE_ID), {0,1,1}, 1, 1, true);
            bind(2, bandId(EXTREME_ID), {0,1,1}, 0, 1, true);
            bind(3, bandId(LINKED_ID), {0,1,1}, 1, 1, true);
        }
        else if (type == Type::shape)
        {
            bind(0, bandId(MODE_ID), {0,11,1}, 3, 1, true);
            bind(1, bandId(BIAS_ID), {-1,1,.01f}, 0);
            bind(2, bandId(REC_ID), {0,1,.01f}, 0);
            bind(3, bandId(DC_FILTER_ID), {0,1,1}, 0, 1, true);
            bind(5, bandId(SHAPE_MIX_ID), {0,1,.01f}, 1, 100);
        }
        else if (type == Type::compressor)
        {
            bind(0, bandId(COMP_THRESH_ID), {-48,0,.1f}, 0);
            bind(1, bandId(COMP_RATIO_ID), {1,20,.1f}, 1);
            bind(2, bandId(COMP_ATTACK_ID), {.1f,200,.01f,.3f}, 10);
            bind(3, bandId(COMP_RELEASE_ID), {10,2000,1,.3f}, 100);
            bind(5, bandId(COMP_MIX_ID), {0,1,.01f}, 1, 100);
        }
        else if (type == Type::stereo)
        {
            bind(0, bandId(WIDTH_ID), {0,1,.01f}, .5f, 100);
            bind(1, bandId(PAN_ID), {-1,1,.01f}, 0, 100);
            bind(5, bandId(WIDTH_MIX_ID), {0,1,.01f}, 1, 100);
        }
        else if (type == Type::ott)
        {
            for (int control = 0; control < 6; ++control)
            {
                const auto i = static_cast<size_t>(control);
                bind(control, bandId(ParameterIDAndName::ottControlIDs[i]),
                    {OttProcessor::minimums[i], OttProcessor::maximums[i], control == 0 || control == 5 ? .01f : .1f},
                    OttProcessor::defaults[i], control == 0 || control == 5 ? 100 : 1);
            }
        }
        else if (type == Type::lofi)
        {
            bind(0, DOWNSAMPLE_ID, {1,64,.01f}, 1);
            bind(1, BIT_DEPTH_ID, {4,32,1}, 32, 1, true);
            for (int control = 2; control < 5; ++control)
                bind(control, effects::tapeIDs[static_cast<size_t>(control - 2)], {0,1}, 0, 100);
            bind(5, DOWNSAMPLE_MIX_ID, {0,1,.01f}, 1, 100);
        }
    }
    if (type == Type::drive)
        add("drive.modernComp", legacy ? drive_comp::parameterID(scope - 1) : juce::String(), {0,1,1}, 1, 1, true);
    if (type == Type::shape)
    {
        add("shape.model", legacy ? analog_params::bandID(scope - 1) : analog_params::parameterID(scope, slot),
            {0, static_cast<float>(analog::count), 1}, 0, 1, true);
        // The legacy Shape does not own the neighbouring Drive parameter.
        add("shape.drive", legacy ? juce::String() : analog_params::driveID(scope, slot), {0,100}, 0);
    }
    if (type == Type::reverb)
        add("reverb.model", reverb_params::parameterID(scope, slot), {0,static_cast<float>(space::count - 1),1}, 0, 1, true);
    if (type == Type::lofi)
        add("lofi.jitter", legacy ? juce::String(JITTER_ID) : core_modules::parameterID(scope, slot, core_modules::jitterField),
            {0,1}, 0);
    if (type == Type::granular)
    {
        constexpr const char* keys[]{"granular.freeze", "granular.spread", "granular.feedback", "granular.reverb"};
        for (int field = 1; field < clouds_params::fieldCount; ++field)
            add(keys[field - 1], clouds_params::parameterID(scope, slot, field),
                {0,1,field == clouds_params::freezeField ? 1.0f : 0.0f}, clouds_params::defaults[static_cast<size_t>(field)],
                1, field == clouds_params::freezeField);
    }
    if (type == Type::eq)
        for (int node = 0; node < eq::maxNodes; ++node)
        {
            const auto defaults = eq::defaultNode(node);
            for (int field = 0; field < eq::fieldCount; ++field)
            {
                const auto kind = static_cast<eq::Field>(field);
                const auto id = legacy ? eq::parameterID(node, kind) : core_modules::eqParameterID(scope, slot, node, kind);
                juce::NormalisableRange<float> range {0,1,1};
                float value = 0;
                switch (kind)
                {
                    case eq::Field::frequency:
                        range = legacy ? juce::NormalisableRange<float>{20,20000,1} : juce::NormalisableRange<float>{20,20000,0,.3f};
                        if (legacy) range.setSkewForCentre(1000);
                        value = defaults.frequency; break;
                    case eq::Field::gain: range = {-24,24,legacy ? .1f : 0.0f}; break;
                    case eq::Field::q:
                        range = legacy ? (node < 3 ? juce::NormalisableRange<float>{1,5,.1f} : juce::NormalisableRange<float>{.1f,18,.01f})
                                       : juce::NormalisableRange<float>{.1f,20,0,.35f};
                        value = defaults.q; break;
                    case eq::Field::slope: range = {0,3,1}; break;
                    case eq::Field::type: range = {0,6,1}; value = static_cast<float>(defaults.type); break;
                    case eq::Field::present: value = defaults.present ? 1.0f : 0.0f; break;
                    case eq::Field::bypassed: break;
                }
                add(eqKey(node,field), id, range, value, 1, field >= static_cast<int>(eq::Field::slope));
            }
            static constexpr const char* dynamicFields[]{"enabled","threshold","ratio","range","attack","release"};
            for(int field=0;field<dynamic_eq::fieldCount;++field)
                add("eq."+juce::String(node)+".dynamic."+dynamicFields[field],
                    dynamic_eq::parameterID(legacy?0:scope,legacy?-1:slot,node,field),
                    dynamic_eq::rangeFor(field),dynamic_eq::defaults[static_cast<size_t>(field)],1,field==dynamic_eq::enabled);
        }
    return fields;
}
juce::String enabledId(Type type, int scope, int node)
{
    if (node >= module_order::firstInsert) return effects::parameterID(scope,node - module_order::firstInsert,effects::enabledField);
    if (scope == 0) return type == Type::eq ? FILTER_BYPASS_ID : DOWNSAMPLE_BYPASS_ID;
    const char* base = type == Type::drive ? DRIVE_BYPASS_ID : type == Type::shape ? SHAPE_BYPASS_ID
        : type == Type::compressor ? COMP_BYPASS_ID : type == Type::ott ? OTT_ENABLED_ID : WIDTH_BYPASS_ID;
    return juce::String(base) + juce::String(scope);
}
float effectivePlain(const Field& field, float value, Type type, bool legacy, int control)
{
    if (!legacy && control >= 0) return effects::controls(type)[static_cast<size_t>(control)].fromNormalised(value);
    return field.range.convertFrom0to1(value) * field.scale;
}
juce::NormalisableRange<float> effectiveRange(const Field& field, Type type, bool legacy, int control)
{
    if (!legacy && control >= 0) return effects::controls(type)[static_cast<size_t>(control)].range();
    return {field.range.start * field.scale, field.range.end * field.scale, 0, field.range.skew};
}
int controlIndex(const juce::String& key)
{ return key.startsWith("control.") ? key.fromFirstOccurrenceOf("control.",false,false).getIntValue() - 1 : -1; }
juce::String fieldLabel(const juce::String& key, Type type)
{
    const auto control = controlIndex(key);
    if (juce::isPositiveAndBelow(control,static_cast<int>(effects::controlCount)))
    {
        const auto name = juce::String(effects::controls(type)[static_cast<size_t>(control)].name);
        return name.isEmpty() ? "Reserved control " + juce::String(control + 1) : name;
    }
    if (key == "shape.drive") return "Shape Drive";
    if (key == "shape.model") return "Shape model";
    if (key == "drive.modernComp") return "Drive compensation mode";
    if (key == "reverb.model") return "Reverb model";
    if (key == "lofi.jitter") return "Jitter";
    if (key.startsWith("eq."))
    {
        const auto rest = key.substring(3);
        return "EQ point " + juce::String(rest.getIntValue() + 1) + " " + rest.fromFirstOccurrenceOf(".",false,false);
    }
    return key.replaceCharacter('.',' ');
}
bool equalRange(const juce::NormalisableRange<float>& a, const juce::NormalisableRange<float>& b)
{ return a.start == b.start && a.end == b.end && a.skew == b.skew; }
}

Module defaultModule(Type type, bool sourceLegacy)
{
    Module module; module.type = type; module.sourceLegacy = sourceLegacy; module.enabled = true;
    for (const auto& field : fieldsFor(type,sourceLegacy))
        module.values.emplace(field.key, field.range.convertTo0to1(field.initial));
    if (type == Type::drive)
    {
        module.legacyLinked = sourceLegacy && module.values.at("control.4") >= .5f;
        module.modernDriveComp = module.values.at("drive.modernComp") >= .5f;
    }
    return module;
}

juce::StringArray ownedParameterIDs(int scope, int node, Type type)
{
    juce::StringArray ids;
    if (!module_order::valid(scope,node) || !validType(type)) return ids;
    const bool legacy = node < module_order::firstInsert;
    if (legacy && core_modules::legacyType(scope,node) != type) return ids;
    for (const auto& field : fieldsFor(type,legacy,scope,node - module_order::firstInsert))
        if (field.id.isNotEmpty()) ids.addIfNotAlreadyThere(field.id);
    ids.addIfNotAlreadyThere(enabledId(type,scope,node));
    if (legacy) ids.addIfNotAlreadyThere(core_modules::presenceID(scope,node));
    else
    {
        const auto slot = node - module_order::firstInsert;
        ids.addIfNotAlreadyThere(effects::parameterID(scope,slot,effects::typeField));
        ids.addIfNotAlreadyThere(core_modules::parameterID(scope,slot,core_modules::typeField));
        ids.addIfNotAlreadyThere(modulation_fx::parameterID(scope,slot));
        ids.addIfNotAlreadyThere(resonator_params::parameterID(scope,slot));
    }
    return ids;
}

juce::Result validateModule(const Module& module)
{
    const auto fields = fieldsFor(module.type,module.sourceLegacy);
    if (fields.empty()) return juce::Result::fail("Unsupported module representation.");
    if (module.values.size() != fields.size()) return juce::Result::fail("The module has incomplete or unknown controls.");
    std::set<juce::String> routedKeys {"enabled"};
    for (const auto& field : fields)
    {
        const auto value = module.values.find(field.key);
        if (value == module.values.end() || !std::isfinite(value->second) || value->second < 0 || value->second > 1)
            return juce::Result::fail("Invalid module control: " + field.key);
        if (field.discrete)
        {
            const auto plain = field.range.convertFrom0to1(value->second);
            if (std::abs(plain - std::round(plain)) > .0001f)
                return juce::Result::fail("Invalid discrete module control: " + field.key);
        }
        if (field.id.isNotEmpty())
        {
            if(!dynamic_eq::isParameterID(field.id))routedKeys.insert(field.key);
        }
        else if (value->second != field.range.convertTo0to1(field.initial))
            return juce::Result::fail("Unsupported control in this module representation: " + field.key);
    }
    if (module.frozen && (module.type != Type::granular || !module.frozen->isValid()))
        return juce::Result::fail("Invalid frozen recording for this module.");
    if (module.type == Type::drive
        && (module.legacyLinked != (module.sourceLegacy && module.values.at("control.4") >= .5f)
            || module.modernDriveComp != (module.values.at("drive.modernComp") >= .5f)))
        return juce::Result::fail("Inconsistent Drive compensation metadata.");
    if (module.routes.size() > mod_sources::maximumRoutings) return juce::Result::fail("Too many module modulation routes.");
    for (const auto& route : module.routes)
    {
        if (!routedKeys.contains(route.targetParameterID) || !mod_sources::validSource(route.sourceLfoIndex))
            return juce::Result::fail("Invalid module modulation target or source.");
        auto safe = route; safe.sanitise();
        if (!safe.hasSameState(route)) return juce::Result::fail("Invalid module modulation mapping.");
    }
    return juce::Result::ok();
}

juce::Result captureModule(const FireAudioProcessor& processor, int scope, int node, Module& destination)
{
    return captureModuleFromSnapshot(processor,processor.captureSerializablePresetStateSnapshot(),scope,node,destination);
}

juce::Result captureModuleFromSnapshot(const FireAudioProcessor& processor,
    const FireAudioProcessor::SerializablePresetStateSnapshot& snapshot, int scope, int node, Module& destination)
{
    if (!module_order::valid(scope,node))
        return juce::Result::fail("This module is no longer present.");
    const auto plain = [&](const juce::String& id, float fallback)
    {
        const auto parameter = snapshot.parameterState.getChildWithProperty("id",id);
        return parameter.isValid() ? static_cast<float>(parameter.getProperty("value")) : fallback;
    };
    Module module;
    module.sourceLegacy = node < module_order::firstInsert;
    if (module.sourceLegacy)
    {
        if (plain(core_modules::presenceID(scope,node),0) < .5f)
            return juce::Result::fail("This module is no longer present.");
        module.type = core_modules::legacyType(scope,node);
    }
    else
    {
        const auto slot = node - module_order::firstInsert;
        module.type = core_modules::decodeType(juce::roundToInt(plain(core_modules::parameterID(scope,slot,core_modules::typeField),0)));
        if (module.type == Type::none)
        {
            const auto modulation = juce::roundToInt(plain(modulation_fx::parameterID(scope,slot),0));
            module.type = plain(resonator_params::parameterID(scope,slot),0) > .5f ? Type::chordResonator
                : modulation == 1 ? Type::flanger : modulation == 2 ? Type::phaser
                : static_cast<Type>(juce::jlimit(0,5,juce::roundToInt(plain(effects::parameterID(scope,slot,effects::typeField),0))));
        }
    }
    const auto fields = fieldsFor(module.type,module.sourceLegacy,scope,node - module_order::firstInsert);
    if (fields.empty()) return juce::Result::fail("Analysis is not an audio module and cannot be copied.");
    juce::ignoreUnused(processor);
    module.enabled = plain(enabledId(module.type,scope,node),1) > .5f;
    for (auto route : snapshot.routings)
        if (route.targetParameterID == enabledId(module.type,scope,node))
        { route.targetParameterID = "enabled"; module.routes.add(std::move(route)); }
    for (const auto& field : fields)
    {
        const float value = field.id.isEmpty() ? field.initial : plain(field.id,field.initial);
        module.values.emplace(field.key,field.range.convertTo0to1(value));
        for (auto route : snapshot.routings)
            if (field.id.isNotEmpty() && route.targetParameterID == field.id)
            { route.targetParameterID = field.key; module.routes.add(std::move(route)); }
    }
    if (module.type == Type::granular)
        module.frozen = snapshot.frozenAudio[static_cast<size_t>(scope)][static_cast<size_t>(node - module_order::firstInsert)];
    if (module.type == Type::drive)
    {
        module.legacyLinked = module.sourceLegacy && module.values.at("control.4") >= .5f;
        module.modernDriveComp = module.values.at("drive.modernComp") >= .5f;
    }
    if (const auto result = validateModule(module); result.failed()) return result;
    destination = std::move(module);
    return juce::Result::ok();
}

juce::Result mapModule(const FireAudioProcessor& processor, const Module& module, int scope, int node,
                       bool includeRoutes, std::map<juce::String,float>& values,
                       juce::Array<ModulationRouting>& routes)
{
    if (const auto result = validateModule(module); result.failed()) return result;
    if (!module_order::valid(scope,node)) return juce::Result::fail("Invalid destination module slot.");
    const bool legacy = node < module_order::firstInsert;
    if (legacy && core_modules::legacyType(scope,node) != module.type)
        return juce::Result::fail("The destination does not support this legacy module.");
    const auto sourceFields = fieldsFor(module.type,module.sourceLegacy);
    const auto targetFields = fieldsFor(module.type,legacy,scope,node - module_order::firstInsert);
    std::map<juce::String,float> proposed;
    juce::Array<ModulationRouting> proposedRoutes;
    if (module.sourceLegacy != legacy && module.type == Type::lofi)
        return juce::Result::fail("Legacy Master Lo-Fi uses a different engine. Paste into the original Master Lo-Fi module to retain its sound.");
    if (module.sourceLegacy && !legacy && module.type == Type::drive
        && module.values.at("drive.modernComp") < .5f && module.values.at("control.4") >= .5f)
        return juce::Result::fail("Legacy Link cannot be copied to an insert. Upgrade the source to Drive Comp first, or paste into a legacy Drive module.");
    for (size_t index = 0; index < sourceFields.size(); ++index)
    {
        const auto& source = sourceFields[index]; const auto& target = targetFields[index];
        const auto label = fieldLabel(target.key,module.type);
        const auto original = module.values.at(source.key);
        const int control = controlIndex(source.key);
        const auto sourceRange = effectiveRange(source,module.type,module.sourceLegacy,control);
        const auto targetRange = effectiveRange(target,module.type,legacy,control);
        const float physical = effectivePlain(source,original,module.type,module.sourceLegacy,control);
        if (target.id.isEmpty())
        {
            const float required = effectivePlain(target,target.range.convertTo0to1(target.initial),module.type,legacy,control);
            // Modern compensation is dormant while Link/Comp is disabled.
            const bool dormantComp = target.key == "drive.modernComp" && module.values.at("control.4") < .5f;
            if (!dormantComp && std::abs(physical - required) > .0001f)
                return juce::Result::fail("The destination cannot represent " + label + " without changing another module.");
            if (includeRoutes)
                for (const auto& route : module.routes)
                    if (route.targetParameterID == target.key)
                        return juce::Result::fail("The destination cannot accept modulation for " + label + ".");
            continue;
        }
        if (physical < targetRange.start - .0001f || physical > targetRange.end + .0001f)
            return juce::Result::fail("The destination range cannot preserve " + label + ". Paste into the same module representation.");
        auto normalised = original;
        if (module.sourceLegacy != legacy)
            normalised = targetRange.convertTo0to1(juce::jlimit(targetRange.start,targetRange.end,physical));
        if (includeRoutes)
            for (auto route : module.routes)
                if (route.targetParameterID == source.key)
                {
                    // Different normalised curves and saturation limits do
                    // not preserve a mapped LFO by simply relabelling its ID.
                    if (!equalRange(sourceRange,targetRange))
                        return juce::Result::fail("Modulation of " + label + " uses a different parameter range. Paste without modulation or use the same module representation.");
                    route.targetParameterID = target.id;
                    proposedRoutes.add(std::move(route));
                }
        const auto* destinationParameter = processor.treeState.getParameter(target.id);
        if (destinationParameter == nullptr)
            return juce::Result::fail("Missing destination parameter: " + target.id);
        if (module.sourceLegacy != legacy && legacy)
        {
            const auto snapped = destinationParameter->convertFrom0to1(normalised) * target.scale;
            const auto tolerance = std::max(.0001f,4 * std::numeric_limits<float>::epsilon() * std::max(1.0f,std::abs(physical)));
            if (std::abs(snapped - physical) > tolerance)
                return juce::Result::fail("The destination's parameter steps cannot preserve " + label + ". Paste into an insert module instead.");
        }
        proposed.emplace(target.id,normalised);
    }
    const auto setPlain = [&](const juce::String& id, float value)
    {
        if (const auto* parameter = processor.treeState.getParameter(id))
            proposed[id] = parameter->convertTo0to1(value);
    };
    setPlain(enabledId(module.type,scope,node),module.enabled ? 1 : 0);
    if (includeRoutes)
        for (auto route : module.routes)
            if (route.targetParameterID == "enabled")
            { route.targetParameterID = enabledId(module.type,scope,node); proposedRoutes.add(std::move(route)); }
    if (!legacy)
    {
        const auto slot = node - module_order::firstInsert;
        setPlain(effects::parameterID(scope,slot,effects::typeField),static_cast<int>(module.type) <= 5 ? static_cast<float>(module.type) : 0);
        setPlain(core_modules::parameterID(scope,slot,core_modules::typeField),static_cast<float>(core_modules::encodeType(module.type)));
        setPlain(modulation_fx::parameterID(scope,slot),module.type == Type::flanger ? 1 : module.type == Type::phaser ? 2 : 0);
        setPlain(resonator_params::parameterID(scope,slot),module.type == Type::chordResonator ? 1 : 0);
        if (module.type == Type::granular) setPlain(clouds_params::parameterID(scope,slot,clouds_params::engineField),1);
    }
    values = std::move(proposed); routes = std::move(proposedRoutes);
    return juce::Result::ok();
}
}
