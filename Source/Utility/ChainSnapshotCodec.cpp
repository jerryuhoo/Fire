#include "ChainSnapshotCodec.h"
#include "ModuleSnapshotMapping.h"
#include "FrozenAudioState.h"
#include "ModulationStateValidation.h"
#include <array>
#include <cmath>
#include <initializer_list>

namespace fire::chain
{
namespace
{
juce::Result fail(const char* message) { return juce::Result::fail(message); }

bool hasOnlyAttributes(const juce::XmlElement& xml, std::initializer_list<const char*> names)
{
    if (xml.getNumAttributes() != static_cast<int>(names.size())) return false;
    for (const auto* name : names) if (!xml.hasAttribute(name)) return false;
    return true;
}
bool strictBool(const juce::XmlElement& xml, const char* name)
{ return modulation_state::hasStrictBooleanAttribute(xml, name); }

juce::Result validateRoute(const ModulationRouting& route, const Module& module)
{
    if (!mod_sources::validSource(route.sourceLfoIndex)
        || !std::isfinite(route.depth) || route.depth < -1 || route.depth > 1
        || !std::isfinite(route.rangeStart) || route.rangeStart < -1 || route.rangeStart > 1
        || !std::isfinite(route.rangeEnd) || route.rangeEnd < -1 || route.rangeEnd > 1
        || route.curve < 0 || route.curve > 3
        || route.depthMacro < -1 || route.depthMacro >= mod_sources::macroCount
        || (route.targetParameterID != "enabled" && !module.values.contains(route.targetParameterID)))
        return fail("The chain contains an invalid modulation route.");
    return juce::Result::ok();
}
juce::Result validateSnapshot(const Snapshot& snapshot)
{
    if (snapshot.modules.size() > maximumModules)
        return fail("A chain may contain at most 13 modules.");
    int routeCount = 0;
    for (const auto& module : snapshot.modules)
    {
        if (module.type <= effects::Type::none || module.type >= effects::Type::count)
            return fail("The chain contains an unsupported module type.");
        if (const auto result = validateModule(module); result.failed()) return result;
        for (const auto& [key, value] : module.values)
            if (key.isEmpty() || !std::isfinite(value) || value < 0 || value > 1)
                return fail("The chain contains an invalid parameter value.");
        routeCount += module.routes.size();
        if (routeCount > maximumRoutes || (!snapshot.includesRouting && !module.routes.isEmpty()))
            return fail("The chain contains too many or undeclared modulation routes.");
        for (const auto& route : module.routes)
            if (const auto result = validateRoute(route, module); result.failed()) return result;
        if (module.frozen && (module.type != effects::Type::granular || !module.frozen->isValid()))
            return fail("The chain contains invalid frozen audio.");
    }
    return juce::Result::ok();
}
struct Clipboards
{
    juce::CriticalSection lock;
    std::array<std::shared_ptr<const Snapshot>, 2> snapshots;
};
Clipboards& clipboards() { static Clipboards value; return value; }
int clipboardIndex(ClipboardKind kind)
{
    if (kind == ClipboardKind::module) return 0;
    if (kind == ClipboardKind::chain) return 1;
    return -1;
}
}

std::unique_ptr<juce::XmlElement> encode(const Snapshot& snapshot)
{
    if (validateSnapshot(snapshot).failed()) return {};
    auto xml = std::make_unique<juce::XmlElement>("FIRE_CHAIN");
    xml->setAttribute("schemaVersion", schemaVersion);
    xml->setAttribute("includesRouting", snapshot.includesRouting);
    for (const auto& module : snapshot.modules)
    {
        auto* item = xml->createNewChildElement("MODULE");
        item->setAttribute("type", static_cast<int>(module.type));
        item->setAttribute("enabled", module.enabled);
        item->setAttribute("sourceLegacy", module.sourceLegacy);
        item->setAttribute("legacyLinked", module.legacyLinked);
        item->setAttribute("modernDriveComp", module.modernDriveComp);
        auto* parameters = item->createNewChildElement("VALUES");
        for (const auto& [key, value] : module.values)
        {
            auto* parameter = parameters->createNewChildElement("PARAM");
            parameter->setAttribute("key", key); parameter->setAttribute("value", value);
        }
        auto* routes = item->createNewChildElement("MODULATION_STATE");
        routes->setAttribute("schemaVersion", modulation_state::schemaVersion);
        for (const auto& route : module.routes) route.writeToXml(*routes->createNewChildElement("ROUTING"));
        if (module.frozen)
        {
            auto* media = item->createNewChildElement("FROZEN_RECORDING");
            media->setAttribute("version", 1);
            media->setAttribute("data", effects::encodeFrozenRecording(*module.frozen));
        }
    }
    return xml;
}

juce::Result decode(const juce::XmlElement& xml, Snapshot& destination)
{
    int version = 0;
    if (!xml.hasTagName("FIRE_CHAIN")
        || !hasOnlyAttributes(xml, {"schemaVersion", "includesRouting"})
        || !modulation_state::readStrictIntegerAttribute(xml, "schemaVersion", version)
        || version != schemaVersion || !strictBool(xml, "includesRouting")
        || xml.getNumChildElements() > maximumModules)
        return fail("This is not a supported Fire chain file.");
    Snapshot snapshot;
    snapshot.includesRouting = xml.getBoolAttribute("includesRouting");
    int routeCount = 0;
    for (const auto* item : xml.getChildIterator())
    {
        int type = 0;
        if (!item->hasTagName("MODULE")
            || !hasOnlyAttributes(*item, {"type", "enabled", "sourceLegacy", "legacyLinked", "modernDriveComp"})
            || !modulation_state::readStrictIntegerAttribute(*item, "type", type)
            || type <= static_cast<int>(effects::Type::none) || type >= static_cast<int>(effects::Type::count)
            || !strictBool(*item, "enabled") || !strictBool(*item, "sourceLegacy")
            || !strictBool(*item, "legacyLinked") || !strictBool(*item, "modernDriveComp")
            || item->getNumChildElements() < 2 || item->getNumChildElements() > 3)
            return fail("The chain contains a malformed module.");
        Module module;
        module.type = static_cast<effects::Type>(type);
        module.enabled = item->getBoolAttribute("enabled");
        module.sourceLegacy = item->getBoolAttribute("sourceLegacy");
        module.legacyLinked = item->getBoolAttribute("legacyLinked");
        module.modernDriveComp = item->getBoolAttribute("modernDriveComp");
        const juce::XmlElement* values = nullptr;
        const juce::XmlElement* routes = nullptr;
        const juce::XmlElement* recording = nullptr;
        for (const auto* child : item->getChildIterator())
        {
            if (child->hasTagName("VALUES") && values == nullptr) values = child;
            else if (child->hasTagName("MODULATION_STATE") && routes == nullptr) routes = child;
            else if (child->hasTagName("FROZEN_RECORDING") && recording == nullptr) recording = child;
            else return fail("The chain contains an unknown or repeated module section.");
        }
        if (values == nullptr || routes == nullptr || values->getNumAttributes() != 0
            || values->getNumChildElements() < 1 || values->getNumChildElements() > 128)
            return fail("The chain is missing its module settings.");
        for (const auto* value : values->getChildIterator())
        {
            double normalised = 0;
            if (!value->hasTagName("PARAM") || !hasOnlyAttributes(*value, {"key", "value"})
                || value->getNumChildElements() != 0
                || !utility::parseStrictFiniteDouble(value->getStringAttribute("value"), normalised)
                || normalised < 0 || normalised > 1
                || !module.values.emplace(value->getStringAttribute("key"), static_cast<float>(normalised)).second)
                return fail("The chain contains a malformed or repeated parameter.");
        }
        if (!hasOnlyAttributes(*routes, {"schemaVersion"}) || !modulation_state::hasSchema(*routes, true)
            || routes->getNumChildElements() > maximumRoutes - routeCount)
            return fail("The chain contains an unsupported modulation section.");
        routeCount += routes->getNumChildElements();
        if (!snapshot.includesRouting && routes->getNumChildElements() != 0)
            return fail("The chain contains undeclared modulation routes.");
        for (const auto* routeXml : routes->getChildIterator())
        {
            int source = -1;
            double depth = 0;
            if (!routeXml->hasTagName("ROUTING") || routeXml->getNumChildElements() != 0
                || !hasOnlyAttributes(*routeXml, {"source", "target", "depth", "bipolar", "bypassed", "customRange", "rangeStart", "rangeEnd", "curve", "depthMacro"})
                || !modulation_state::readStrictIntegerAttribute(*routeXml, "source", source)
                || !mod_sources::validSource(source)
                || !utility::parseStrictFiniteDouble(routeXml->getStringAttribute("depth"), depth)
                || depth < -1 || depth > 1 || !strictBool(*routeXml, "bipolar") || !strictBool(*routeXml, "bypassed")
                || !modulation_state::hasValidMapping(*routeXml))
                return fail("The chain contains a malformed modulation route.");
            auto route = ModulationRouting::readFromXml(*routeXml);
            if (const auto result = validateRoute(route, module); result.failed()) return result;
            module.routes.add(std::move(route));
        }
        if (recording != nullptr)
        {
            if (module.type != effects::Type::granular || !hasOnlyAttributes(*recording, {"version", "data"})
                || recording->getStringAttribute("version") != "1" || recording->getNumChildElements() != 0)
                return fail("The chain contains unsupported frozen audio.");
            module.frozen = effects::decodeFrozenRecording(recording->getStringAttribute("data"));
            if (!module.frozen) return fail("The chain's frozen audio is damaged.");
        }
        if (const auto result = validateModule(module); result.failed()) return result;
        snapshot.modules.push_back(std::move(module));
    }
    if (const auto result = validateSnapshot(snapshot); result.failed()) return result;
    destination = std::move(snapshot);
    return juce::Result::ok();
}

juce::Result read(const juce::File& file, Snapshot& destination)
{
    if (file.isSymbolicLink()) return fail("A chain file cannot be a symbolic link.");
    juce::FileInputStream input(file);
    if (!input.openedOk()) return fail("The chain file could not be opened.");
    juce::MemoryOutputStream bytes;
    std::array<char, 65536> chunk;
    for (;;)
    {
        const auto remaining = maximumFileBytes + 1 - static_cast<juce::int64>(bytes.getDataSize());
        if (remaining <= 0) return fail("The chain file exceeds the 10 MB size limit.");
        const auto count = input.read(chunk.data(), static_cast<int>(std::min<juce::int64>(remaining, chunk.size())));
        if (count <= 0) break;
        bytes.write(chunk.data(), static_cast<size_t>(count));
    }
    if (input.getStatus().failed() || bytes.getDataSize() == 0 || bytes.getDataSize() > maximumFileBytes)
        return fail("The chain file is empty, unreadable, or exceeds the 10 MB size limit.");
    const auto source = juce::String::fromUTF8(static_cast<const char*>(bytes.getData()), static_cast<int>(bytes.getDataSize()));
    if (source.containsIgnoreCase("<!DOCTYPE") || source.containsIgnoreCase("<!ENTITY"))
        return fail("The chain file contains unsupported XML declarations.");
    auto xml = juce::XmlDocument::parse(source);
    return xml ? decode(*xml, destination) : fail("The chain file contains invalid XML.");
}

juce::Result write(const juce::File& file, const Snapshot& snapshot)
{
    auto xml = encode(snapshot);
    if (!xml) return fail("The chain could not be saved because its settings are invalid.");
    const auto contents = xml->toString();
    if (contents.getNumBytesAsUTF8() > maximumFileBytes) return fail("The chain exceeds the 10 MB size limit.");
    if (file.isSymbolicLink() || file.getParentDirectory().createDirectory().failed())
        return fail("The chain file location is not writable.");
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(contents) || !temporary.overwriteTargetFileWithTemporary())
        return fail("The chain file could not be saved.");
    return juce::Result::ok();
}

juce::Result setClipboard(ClipboardKind kind, const Snapshot& snapshot)
{
    const auto index = clipboardIndex(kind);
    if (index < 0 || (kind == ClipboardKind::module && snapshot.modules.size() != 1))
        return fail("The module clipboard requires exactly one module.");
    if (const auto result = validateSnapshot(snapshot); result.failed()) return result;
    auto replacement = std::make_shared<const Snapshot>(snapshot);
    auto& storage = clipboards(); const juce::ScopedLock lock(storage.lock);
    storage.snapshots[static_cast<size_t>(index)] = std::move(replacement);
    return juce::Result::ok();
}

std::shared_ptr<const Snapshot> getClipboard(ClipboardKind kind)
{
    const auto index = clipboardIndex(kind);
    if (index < 0) return {};
    auto& storage = clipboards(); const juce::ScopedLock lock(storage.lock);
    return storage.snapshots[static_cast<size_t>(index)];
}
}
