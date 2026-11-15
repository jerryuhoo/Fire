#include <Utility/ModuleSnapshotMapping.h>
#include <Utility/AnalogShapeParameters.h>
#include <Utility/ReverbModelParameters.h>
#include <Utility/DriveCompensationParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
namespace cs = fire::chain;
using Type = fire::effects::Type;
using Patch = std::map<juce::String,float>;
void moduleSet(FireAudioProcessor& processor, const juce::String& id, float value, bool normalised = false)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(normalised ? value : parameter->convertTo0to1(value));
}
void moduleApply(FireAudioProcessor& processor, const Patch& values)
{ for (const auto& [id,value] : values) moduleSet(processor,id,value,true); }
float modulePlain(const FireAudioProcessor& processor, const Patch& values, const juce::String& id)
{
    const auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    const auto value = values.find(id); REQUIRE(value != values.end());
    return parameter->convertFrom0to1(value->second);
}
}

TEST_CASE("Canonical module schemas cover every effect and reject incomplete or unknown controls", "[module-snapshot][schema]")
{
    for (int type = 1; type < static_cast<int>(Type::count); ++type)
    {
        CAPTURE(type);
        const auto valid = cs::defaultModule(static_cast<Type>(type));
        REQUIRE(cs::validateModule(valid).wasOk());
        auto missing = valid; missing.values.erase(missing.values.begin());
        CHECK(cs::validateModule(missing).failed());
        auto extra = valid; extra.values.emplace("unknown",.5f);
        CHECK(cs::validateModule(extra).failed());
        auto invalid = valid; invalid.values.begin()->second = std::numeric_limits<float>::quiet_NaN();
        CHECK(cs::validateModule(invalid).failed());
    }
    for (const auto type : {Type::drive,Type::shape,Type::compressor,Type::ott,Type::stereo,Type::eq,Type::lofi})
        CHECK(cs::validateModule(cs::defaultModule(type,true)).wasOk());
    CHECK(cs::validateModule(cs::defaultModule(Type::reverb,true)).failed());
    auto discrete = cs::defaultModule(Type::shape); discrete.values["shape.model"] = .012345f;
    CHECK(cs::validateModule(discrete).failed());
}

TEST_CASE("Every insert type maps complete control values and advanced routes across scopes", "[module-snapshot][mapping][routing]")
{
    for (int number = 1; number < static_cast<int>(Type::count); ++number)
    {
        const auto type = static_cast<Type>(number); CAPTURE(fire::effects::name(type));
        FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
        const auto slot = processor.addInsertEffect(1,type); REQUIRE(slot >= 0);
        for (int control = 0; control < 6; ++control)
            moduleSet(processor,fire::effects::parameterID(1,slot,control),.17f + .1f * control,true);
        // Independent Comp is a boolean recipe, although its storage remains
        // an ordinary normalised slot parameter.
        if (type == Type::drive) moduleSet(processor,fire::effects::parameterID(1,slot,3),1,true);
        if (type == Type::shape)
        {
            moduleSet(processor,fire::analog_params::parameterID(1,slot),3);
            moduleSet(processor,fire::analog_params::driveID(1,slot),24);
        }
        if (type == Type::reverb) moduleSet(processor,fire::reverb_params::parameterID(1,slot),4);
        auto& manager = processor.getLfoManager();
        {
            const juce::ScopedLock lock(manager.getLfoDataLock());
            auto& routes = manager.getModulationRoutings(); routes.clear();
            ModulationRouting route {fire::mod_sources::envelopeSource(3),fire::effects::parameterID(1,slot,5),-.4f,false,false};
            route.customRange = true; route.rangeStart = -.3f; route.rangeEnd = .7f; route.curve = 3; route.depthMacro = 2;
            routes.add(route); manager.advanceModulationRoutingRevisionLocked();
        }
        cs::Module source; REQUIRE(cs::captureModule(processor,1,fire::module_order::firstInsert + slot,source).wasOk());
        REQUIRE(source.routes.size() == 1); CHECK(source.routes[0].targetParameterID == "control.6");
        Patch patch; juce::Array<ModulationRouting> routes;
        REQUIRE(cs::mapModule(processor,source,0,fire::module_order::firstInsert + 6,true,patch,routes).wasOk());
        REQUIRE(routes.size() == 1);
        CHECK(routes[0].targetParameterID == fire::effects::parameterID(0,6,5));
        auto expectedRoute = source.routes[0]; expectedRoute.targetParameterID = routes[0].targetParameterID;
        CHECK(routes[0].hasSameState(expectedRoute));
        for (int control = 0; control < 6; ++control)
            CHECK(patch.at(fire::effects::parameterID(0,6,control)) == source.values.at("control." + juce::String(control + 1)));
        // Preflight accepts empty destinations and changes no parameters.
        CHECK(processor.getInsertEffectType(0,6) == Type::none);
        moduleApply(processor,patch);
        CHECK(processor.getInsertEffectType(0,6) == type);
        cs::Module restored; REQUIRE(cs::captureModule(processor,0,fire::module_order::firstInsert + 6,restored).wasOk());
        REQUIRE(restored.values.size() == source.values.size());
        for (const auto& [key,value] : source.values)
            CHECK(restored.values.at(key) == Catch::Approx(value).margin(.000002f));
    }
}

TEST_CASE("Legacy module copies preserve normalised controls and respect Drive Shape ownership", "[module-snapshot][legacy][ownership]")
{
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    moduleSet(processor,"drive1",67.3f); moduleSet(processor,"drive2",12.5f);
    moduleSet(processor,"shapeMix1",.37f); moduleSet(processor,"bias1",.24f);
    moduleSet(processor,fire::analog_params::bandID(0),4);
    for (int node = 0; node < 5; ++node)
    {
        cs::Module source; REQUIRE(cs::captureModule(processor,1,node,source).wasOk());
        Patch patch; juce::Array<ModulationRouting> routes;
        REQUIRE(cs::mapModule(processor,source,2,node,true,patch,routes).wasOk());
        const auto owned = cs::ownedParameterIDs(2,node,source.type);
        for (const auto& [id,value] : patch) { juce::ignoreUnused(value); CHECK(owned.contains(id)); }
        if (node == 1)
        {
            CHECK(source.values.at("shape.drive") == 0);
            CHECK_FALSE(patch.contains("drive2")); CHECK_FALSE(patch.contains("linked2"));
            CHECK_FALSE(owned.contains("drive2"));
            CHECK(modulePlain(processor,patch,"shapeMix2") == Catch::Approx(.37f));
            REQUIRE(cs::mapModule(processor,source,0,fire::module_order::firstInsert,false,patch,routes).wasOk());
            CHECK(modulePlain(processor,patch,fire::analog_params::driveID(0,0)) == 0);
        }
    }
    CHECK(processor.treeState.getRawParameterValue("drive2")->load() == Catch::Approx(12.5f));
}

TEST_CASE("Legacy compensation remains legacy and incompatible transfer fails atomically", "[module-snapshot][legacy][drive-comp]")
{
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    moduleSet(processor,"drive1",43); moduleSet(processor,"linked1",1);
    moduleSet(processor,fire::drive_comp::parameterID(0),0);
    cs::Module source; REQUIRE(cs::captureModule(processor,1,0,source).wasOk());
    CHECK(source.legacyLinked); CHECK_FALSE(source.modernDriveComp);
    Patch patch{{"sentinel",.5f}}; juce::Array<ModulationRouting> routes{{0,"sentinel",.2f,false,false}};
    const auto failure = cs::mapModule(processor,source,0,fire::module_order::firstInsert,true,patch,routes);
    CHECK(failure.failed()); CHECK(failure.getErrorMessage().contains("Upgrade"));
    CHECK(patch.size() == 1); CHECK(patch.contains("sentinel")); CHECK(routes[0].targetParameterID == "sentinel");
    REQUIRE(cs::mapModule(processor,source,2,0,true,patch,routes).wasOk());
    CHECK(modulePlain(processor,patch,"drive2") == Catch::Approx(43));
    CHECK(modulePlain(processor,patch,"linked2") == 1);
    CHECK(modulePlain(processor,patch,fire::drive_comp::parameterID(1)) == 0);
    CHECK_FALSE(patch.contains("output2"));
}

TEST_CASE("Cross representation conversion uses physical values and rejects incompatible ranges and routes", "[module-snapshot][legacy][ranges]")
{
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    moduleSet(processor,"compThresh1",-24); moduleSet(processor,"compAttack1",73); moduleSet(processor,"compRelease1",750);
    cs::Module source; REQUIRE(cs::captureModule(processor,1,2,source).wasOk());
    Patch patch; juce::Array<ModulationRouting> routes;
    REQUIRE(cs::mapModule(processor,source,0,fire::module_order::firstInsert,false,patch,routes).wasOk());
    const auto& controls = fire::effects::controls(Type::compressor);
    CHECK(controls[0].fromNormalised(patch.at(fire::effects::parameterID(0,0,0))) == Catch::Approx(-24));
    CHECK(controls[2].fromNormalised(patch.at(fire::effects::parameterID(0,0,2))) == Catch::Approx(73));
    CHECK(controls[3].fromNormalised(patch.at(fire::effects::parameterID(0,0,3))) == Catch::Approx(750));
    source.routes.add({0,"control.3",.2f,true,false});
    CHECK(cs::mapModule(processor,source,0,fire::module_order::firstInsert,true,patch,routes).failed());
    REQUIRE(cs::mapModule(processor,source,2,2,true,patch,routes).wasOk());
    REQUIRE(routes.size() == 1); CHECK(routes[0].targetParameterID == "compAttack2");
    moduleSet(processor,"compRelease1",1700);
    REQUIRE(cs::captureModule(processor,1,2,source).wasOk());
    CHECK(cs::mapModule(processor,source,0,fire::module_order::firstInsert,false,patch,routes).failed());
    cs::Module lofi; REQUIRE(cs::captureModule(processor,0,1,lofi).wasOk());
    CHECK(cs::mapModule(processor,lofi,1,fire::module_order::firstInsert,false,patch,routes).failed());
}

TEST_CASE("EQ snapshot includes all twelve stable nodes with models presence and bypass", "[module-snapshot][eq]")
{
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    const auto slot = processor.addInsertEffect(1,Type::eq); REQUIRE(slot >= 0);
    for (int node = 0; node < fire::eq::maxNodes; ++node)
    {
        const auto id = [&](fire::eq::Field field) { return fire::core_modules::eqParameterID(1,slot,node,field); };
        moduleSet(processor,id(fire::eq::Field::frequency),120.0f + 1000.0f * node);
        moduleSet(processor,id(fire::eq::Field::gain),static_cast<float>(node - 6));
        moduleSet(processor,id(fire::eq::Field::q),.5f + node);
        moduleSet(processor,id(fire::eq::Field::type),static_cast<float>(node % 7));
        moduleSet(processor,id(fire::eq::Field::slope),static_cast<float>(node % 4));
        moduleSet(processor,id(fire::eq::Field::present),node % 2);
        moduleSet(processor,id(fire::eq::Field::bypassed),node % 3 == 0 ? 1 : 0);
    }
    cs::Module source; REQUIRE(cs::captureModule(processor,1,fire::module_order::firstInsert + slot,source).wasOk());
    CHECK(source.values.size() == 6u + fire::eq::maxNodes * (fire::eq::fieldCount + fire::dynamic_eq::fieldCount));
    Patch patch; juce::Array<ModulationRouting> routes;
    REQUIRE(cs::mapModule(processor,source,3,fire::module_order::firstInsert + 7,false,patch,routes).wasOk());
    moduleApply(processor,patch);
    for (int node = 0; node < fire::eq::maxNodes; ++node)
    {
        const auto first = processor.getEqNodeState(node,1,slot), second = processor.getEqNodeState(node,3,7);
        CHECK(first.present == second.present); CHECK(first.bypassed == second.bypassed);
        CHECK(first.type == second.type); CHECK(first.slope == second.slope);
        CHECK(first.frequency == Catch::Approx(second.frequency)); CHECK(first.gainDb == Catch::Approx(second.gainDb));
        CHECK(first.q == Catch::Approx(second.q));
    }
}

TEST_CASE("Capture from one snapshot retains frozen media and original identity after live edits", "[module-snapshot][snapshot][frozen]")
{
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    const auto slot = processor.addInsertEffect(1,Type::granular); REQUIRE(slot >= 0);
    moduleSet(processor,fire::clouds_params::parameterID(1,slot,fire::clouds_params::freezeField),1);
    auto snapshot = processor.captureSerializablePresetStateSnapshot();
    auto recording = std::make_shared<fire::effects::FrozenRecording>();
    recording->head = 32; recording->validFrames = 32; recording->samples[0][0] = 1234;
    snapshot.frozenAudio[1][static_cast<size_t>(slot)] = recording;
    processor.removeInsertEffect(1,slot);
    cs::Module module;
    REQUIRE(cs::captureModuleFromSnapshot(processor,snapshot,1,fire::module_order::firstInsert + slot,module).wasOk());
    CHECK(module.type == Type::granular); REQUIRE(module.frozen != nullptr);
    CHECK(module.frozen.get() == recording.get()); CHECK(module.frozen->samples[0][0] == 1234);
    CHECK(module.values.at("granular.freeze") == 1);
    CHECK(cs::captureModule(processor,1,fire::module_order::firstInsert + slot,module).failed());
}

TEST_CASE("Existing power parameter routes survive canonical module capture", "[module-snapshot][routing]")
{
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    // The picker does not offer power controls, but the public routing API and
    // historical state can contain these identities. Do not silently drop them.
    const auto enabled = ParameterIDAndName::getIDString(SHAPE_BYPASS_ID,0);
    REQUIRE(processor.assignLfoToTarget(0,enabled) == LfoManager::AssignmentResult::changed);
    cs::Module module; REQUIRE(cs::captureModule(processor,1,1,module).wasOk());
    REQUIRE(module.routes.size() == 1); CHECK(module.routes[0].targetParameterID == "enabled");
    Patch patch; juce::Array<ModulationRouting> routes;
    REQUIRE(cs::mapModule(processor,module,0,fire::module_order::firstInsert,true,patch,routes).wasOk());
    REQUIRE(routes.size() == 1);
    CHECK(routes[0].targetParameterID == fire::effects::parameterID(0,0,fire::effects::enabledField));
}
