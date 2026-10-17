#include <Utility/ChainSnapshotCodec.h>
#include <Utility/ModuleSnapshotMapping.h>
#include <Utility/FrozenAudioState.h>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace
{
namespace chain = fire::chain;
using Type = fire::effects::Type;
fire::effects::FrozenRecordingPtr chainRecording()
{
    auto recording = std::make_shared<fire::effects::FrozenRecording>();
    recording->head = recording->validFrames = 73;
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < 73; ++frame)
            recording->samples[static_cast<size_t>(channel)][static_cast<size_t>(frame)]
                = static_cast<std::int16_t>((channel ? -1 : 1) * (frame + 1) * 311);
    return recording;
}
chain::Snapshot codecSnapshot()
{
    chain::Snapshot result;
    result.includesRouting = true;
    result.modules.push_back(chain::defaultModule(Type::drive, true));
    result.modules.back().values["control.1"] = .37f;
    result.modules.back().enabled = false;
    for (const auto source : {0, 16, 20, 23})
    {
        ModulationRouting route {source, "control.1", -.6f, false, false};
        route.customRange = true; route.rangeStart = .2f; route.rangeEnd = -.5f;
        route.curve = 2; route.depthMacro = 3;
        result.modules.back().routes.add(route);
    }
    result.modules.back().routes.add({17, "enabled", .25f, false, true});
    result.modules.push_back(chain::defaultModule(Type::granular));
    result.modules.back().frozen = chainRecording();
    result.modules.push_back(chain::defaultModule(Type::reverb));
    return result;
}
void requireSame(const chain::Snapshot& actual, const chain::Snapshot& expected)
{
    REQUIRE(actual.includesRouting == expected.includesRouting);
    REQUIRE(actual.modules.size() == expected.modules.size());
    for (size_t index = 0; index < actual.modules.size(); ++index)
    {
        CAPTURE(index); const auto& a = actual.modules[index]; const auto& b = expected.modules[index];
        CHECK(a.type == b.type); CHECK(a.enabled == b.enabled); CHECK(a.sourceLegacy == b.sourceLegacy);
        CHECK(a.legacyLinked == b.legacyLinked); CHECK(a.modernDriveComp == b.modernDriveComp);
        CHECK(a.values == b.values); REQUIRE(a.routes.size() == b.routes.size());
        for (int route = 0; route < a.routes.size(); ++route) CHECK(a.routes[route].hasSameState(b.routes[route]));
        REQUIRE(static_cast<bool>(a.frozen) == static_cast<bool>(b.frozen));
        if (a.frozen) { CHECK(a.frozen->head == b.frozen->head); CHECK(a.frozen->validFrames == b.frozen->validFrames); CHECK(a.frozen->samples == b.frozen->samples); }
    }
}
struct CodecDirectory
{
    juce::File path = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("fire-chain-codec-" + juce::Uuid().toString());
    CodecDirectory() { REQUIRE(path.createDirectory().wasOk()); }
    ~CodecDirectory() { path.deleteRecursively(); }
};
}

TEST_CASE("Chain codec retains settings modulation mappings and exact frozen PCM", "[chain][codec][state]")
{
    const auto source = codecSnapshot(); auto xml = chain::encode(source); REQUIRE(xml != nullptr);
    CHECK(xml->getIntAttribute("schemaVersion") == 1);
    chain::Snapshot restored;
    REQUIRE(chain::decode(*xml, restored).wasOk()); requireSame(restored, source);
    for (int type = static_cast<int>(Type::chorus); type < static_cast<int>(Type::count); ++type)
    {
        CAPTURE(type);
        chain::Snapshot one; one.modules.push_back(chain::defaultModule(static_cast<Type>(type)));
        auto encoded = chain::encode(one); REQUIRE(encoded != nullptr);
        REQUIRE(chain::decode(*encoded, restored).wasOk()); requireSame(restored, one);
    }
    chain::Snapshot empty; auto encodedEmpty = chain::encode(empty); REQUIRE(encodedEmpty != nullptr);
    REQUIRE(chain::decode(*encodedEmpty, restored).wasOk()); requireSame(restored, empty);
}

TEST_CASE("Chain codec rejects unknown missing and malformed module settings without replacing the destination", "[chain][codec][validation]")
{
    const auto source = codecSnapshot(); auto valid = chain::encode(source); REQUIRE(valid != nullptr);
    auto destination = source;
    const auto reject = [&](const juce::XmlElement& xml)
    { CHECK(chain::decode(xml, destination).failed()); requireSame(destination, source); };
    for (const auto* schema : {"0", "2", "1.5", "1e0", "1tail"})
    { auto bad = *valid; bad.setAttribute("schemaVersion", schema); reject(bad); }
    for (const auto* type : {"0", "15", "-1", "1.5", "1e0", "1tail"})
    { auto bad = *valid; bad.getFirstChildElement()->setAttribute("type", type); reject(bad); }
    for (const auto* boolean : {"enabled", "sourceLegacy", "legacyLinked", "modernDriveComp"})
    { auto bad = *valid; bad.getFirstChildElement()->setAttribute(boolean, "true"); reject(bad); }
    for (const auto* value : {"nan", "inf", "-0.01", "1.01", "0.5tail"})
    {
        auto bad = *valid;
        bad.getFirstChildElement()->getChildByName("VALUES")->getFirstChildElement()->setAttribute("value", value); reject(bad);
    }
    SECTION("duplicate parameter")
    {
        auto bad = *valid; auto* values = bad.getFirstChildElement()->getChildByName("VALUES");
        values->addChildElement(new juce::XmlElement(*values->getFirstChildElement())); reject(bad);
    }
    SECTION("unknown global target")
    {
        auto bad = *valid;
        bad.getFirstChildElement()->getChildByName("VALUES")->getFirstChildElement()->setAttribute("key", "macro1"); reject(bad);
    }
    SECTION("missing setting")
    {
        auto bad = *valid; auto* values = bad.getFirstChildElement()->getChildByName("VALUES");
        values->removeChildElement(values->getFirstChildElement(), true); reject(bad);
    }
    SECTION("unknown module section")
    { auto bad = *valid; bad.getFirstChildElement()->createNewChildElement("MACROS"); reject(bad); }
    SECTION("repeated settings section")
    {
        auto bad = *valid; auto* item = bad.getFirstChildElement();
        item->addChildElement(new juce::XmlElement(*item->getChildByName("VALUES"))); reject(bad);
    }
    SECTION("unknown root metadata")
    { auto bad = *valid; bad.setAttribute("env1Source", 1); reject(bad); }
    SECTION("module limit")
    {
        auto bad = *valid;
        while (bad.getNumChildElements() <= chain::maximumModules) bad.addChildElement(new juce::XmlElement(*valid->getFirstChildElement()));
        reject(bad);
    }
}

TEST_CASE("Chain codec validates every source and advanced route field", "[chain][codec][mapping][validation]")
{
    const auto source = codecSnapshot(); auto valid = chain::encode(source); REQUIRE(valid != nullptr);
    auto destination = source;
    const auto reject = [&](const juce::XmlElement& xml)
    { CHECK(chain::decode(xml, destination).failed()); requireSame(destination, source); };
    const std::array<std::pair<const char*, const char*>, 15> cases {{
        {"source", "24"}, {"source", "-1"}, {"source", "1e0"}, {"source", "1.5"},
        {"target", "macro1"}, {"target", "drive1"}, {"depth", "nan"}, {"depth", "1.01"},
        {"bipolar", "true"}, {"bypassed", "2"}, {"customRange", "true"},
        {"rangeStart", "-1.01"}, {"rangeEnd", "inf"}, {"curve", "4"}, {"depthMacro", "4"} }};
    for (const auto& [field, value] : cases)
    {
        CAPTURE(field, value); auto bad = *valid;
        bad.getFirstChildElement()->getChildByName("MODULATION_STATE")->getFirstChildElement()->setAttribute(field, value); reject(bad);
    }
    for (const auto* field : {"source", "target", "depth", "bipolar", "bypassed", "customRange", "rangeStart", "rangeEnd", "curve", "depthMacro"})
    {
        auto bad = *valid;
        bad.getFirstChildElement()->getChildByName("MODULATION_STATE")->getFirstChildElement()->removeAttribute(field); reject(bad);
    }
    auto wrongSchema = *valid; wrongSchema.getFirstChildElement()->getChildByName("MODULATION_STATE")->setAttribute("schemaVersion", 1); reject(wrongSchema);
    auto undeclared = *valid; undeclared.setAttribute("includesRouting", false); reject(undeclared);
    auto tooMany = *valid;
    auto* first = tooMany.getFirstChildElement()->getChildByName("MODULATION_STATE");
    while (first->getNumChildElements() < chain::maximumRoutes)
        first->addChildElement(new juce::XmlElement(*first->getFirstChildElement()));
    auto* second = tooMany.getChildElement(1)->getChildByName("MODULATION_STATE");
    auto* extra = new juce::XmlElement(*first->getFirstChildElement()); extra->setAttribute("target", "control.1"); second->addChildElement(extra);
    reject(tooMany);
}

TEST_CASE("Chain codec rejects corrupt frozen audio and preserves existing files on invalid writes", "[chain][codec][file][frozen-audio]")
{
    const auto source = codecSnapshot(); auto valid = chain::encode(source); REQUIRE(valid != nullptr);
    auto restored = source;
    auto damaged = *valid; auto* media = damaged.getChildElement(1)->getChildByName("FROZEN_RECORDING"); REQUIRE(media != nullptr);
    media->setAttribute("data", media->getStringAttribute("data").dropLastCharacters(8));
    CHECK(chain::decode(damaged, restored).failed()); requireSame(restored, source);
    auto wrongType = *valid;
    wrongType.getFirstChildElement()->addChildElement(new juce::XmlElement(*valid->getChildElement(1)->getChildByName("FROZEN_RECORDING")));
    CHECK(chain::decode(wrongType, restored).failed()); requireSame(restored, source);
    CodecDirectory directory;
    const auto file = directory.path.getChildFile("colour.firechain");
    REQUIRE(chain::write(file, source).wasOk());
    const auto contents = file.loadFileAsString();
    chain::Snapshot readBack; REQUIRE(chain::read(file, readBack).wasOk()); requireSame(readBack, source);
    auto invalid = source; invalid.modules[0].values["control.1"] = std::numeric_limits<float>::quiet_NaN();
    CHECK(chain::encode(invalid) == nullptr); CHECK(chain::write(file, invalid).failed()); CHECK(file.loadFileAsString() == contents);
    const auto oversized = directory.path.getChildFile("oversized.firechain");
    { juce::FileOutputStream stream(oversized); REQUIRE(stream.openedOk()); REQUIRE(stream.setPosition(chain::maximumFileBytes)); stream.writeByte('x'); }
    CHECK(chain::read(oversized, restored).failed()); requireSame(restored, source);
    const auto malformed = directory.path.getChildFile("broken.firechain"); REQUIRE(malformed.replaceWithText("<FIRE_CHAIN>"));
    CHECK(chain::read(malformed, restored).failed()); requireSame(restored, source);
    REQUIRE(malformed.replaceWithText("<!DOCTYPE FIRE_CHAIN [<!ENTITY x 'x'>]><FIRE_CHAIN schemaVersion='1' includesRouting='0'/>"));
    CHECK(chain::read(malformed, restored).failed()); requireSame(restored, source);
}

TEST_CASE("Module and chain clipboards are independent immutable validated snapshots", "[chain][clipboard]")
{
    auto source = codecSnapshot();
    REQUIRE(chain::setClipboard(chain::ClipboardKind::chain, source).wasOk());
    auto stored = chain::getClipboard(chain::ClipboardKind::chain); REQUIRE(stored != nullptr); requireSame(*stored, source);
    chain::Snapshot one; one.modules.push_back(chain::defaultModule(Type::delay));
    REQUIRE(chain::setClipboard(chain::ClipboardKind::module, one).wasOk());
    requireSame(*chain::getClipboard(chain::ClipboardKind::module), one);
    source.modules.clear(); requireSame(*stored, codecSnapshot());
    CHECK(chain::setClipboard(chain::ClipboardKind::module, source).failed());
    requireSame(*chain::getClipboard(chain::ClipboardKind::module), one);
    auto invalid = one; invalid.modules[0].values["foreign"] = 0;
    CHECK(chain::setClipboard(chain::ClipboardKind::module, invalid).failed());
    requireSame(*chain::getClipboard(chain::ClipboardKind::module), one);
    chain::Snapshot empty; REQUIRE(chain::setClipboard(chain::ClipboardKind::chain, empty).wasOk());
    CHECK(chain::getClipboard(chain::ClipboardKind::chain)->modules.empty());
    requireSame(*chain::getClipboard(chain::ClipboardKind::module), one);
}
