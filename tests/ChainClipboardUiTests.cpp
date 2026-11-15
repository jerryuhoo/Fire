#include <GUI/EffectRackNavigation.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>

struct ChainClipboardUiTestAccess
{
    using Rack = fire::ui::EffectRackNavigation;
    static void suppressAlerts(Rack& rack) { rack.suppressResultAlertsForTesting = true; }
    static juce::String error(const Rack& rack) { return rack.lastChainResult; }
    static bool fileActive(const Rack& rack) { return rack.chainFileActive; }
    static auto fileHandler(Rack& rack, bool save, bool routing)
    {
        auto request = rack.prepareChainFile(save, routing);
        REQUIRE(request.has_value());
        auto chooser = std::make_shared<juce::FileChooser>("Chain test", juce::File(), "*.firechain", false);
        rack.chainFileChooser = chooser;
        return rack.createChainFileResultHandler(*request, chooser);
    }
    static fire::ui::ChainMenuButton& button(Rack& rack) { return rack.chainButton; }
    static juce::Button& add(Rack& rack) { return rack.addButton; }
};
namespace
{
using Rack = fire::ui::EffectRackNavigation;
namespace chain = fire::chain;
namespace effects = fire::effects;
struct RackFixture
{
    FireAudioProcessor processor;
    FireLookAndFeel lookAndFeel;
    Rack rack {processor, 1};
    RackFixture()
    {
        rack.setLookAndFeel(&lookAndFeel);
        rack.setSize(210, 390);
        rack.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        rack.setVisible(true);
        ChainClipboardUiTestAccess::suppressAlerts(rack);
    }
    ~RackFixture() { rack.setLookAndFeel(nullptr); }
    int add(effects::Type type)
    {
        const auto slot = processor.addInsertEffect(rack.getScope(), type);
        REQUIRE(slot >= 0);
        rack.refresh();
        return fire::module_order::firstInsert + slot;
    }
};
void command(Rack& rack, int node, int action)
{ auto handler = rack.createModuleMenuResultHandler(node); handler(action); }
void chainCommand(Rack& rack, int action)
{ auto handler = rack.createChainMenuResultHandler(); handler(action); }
chain::Snapshot capture(FireAudioProcessor& processor, int scope, bool routing = false)
{
    chain::Snapshot result;
    REQUIRE(processor.captureChainSnapshot(scope, routing, result).wasOk());
    return result;
}
void clear(FireAudioProcessor& processor, int scope)
{
    for (auto node : processor.getModuleOrder(scope))
        if (processor.isModulePresent(scope, node)) processor.removeModule(scope, node);
}
struct TempFiles
{
    juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("FireChainUI-" + juce::Uuid().toString());
    TempFiles() { REQUIRE(root.createDirectory().wasOk()); }
    ~TempFiles() { root.deleteRecursively(); }
};
}

TEST_CASE("Module tools copy across instances and paste the exact clipboard shown when the menu opened",
          "[chain-tools][ui][clipboard]")
{
    RackFixture source, destination;
    const int node = source.add(effects::Type::chorus);
    const auto target = effects::parameterID(1, node - fire::module_order::firstInsert, 0);
    REQUIRE(source.processor.assignLfoToTarget(fire::mod_sources::envelopeSource(2), target) == LfoManager::AssignmentResult::changed);
    command(source.rack, node, Rack::copyWithRouting);
    auto clipboard = chain::getClipboard(chain::ClipboardKind::module);
    REQUIRE(clipboard != nullptr);
    REQUIRE(clipboard->modules.size() == 1);
    CHECK(clipboard->includesRouting);
    CHECK_FALSE(clipboard->modules[0].routes.isEmpty());
    destination.rack.setScope(2);
    auto paste = destination.rack.createAddMenuResultHandler();
    // Another editor's Copy must not silently change the pending Paste item.
    const int replacement = source.add(effects::Type::phaser);
    command(source.rack, replacement, Rack::copySettings);
    int selected = -1;
    destination.rack.onSelectEffect = [&](int slot) { selected = slot; };
    paste(Rack::pasteCopiedModule);
    REQUIRE(selected >= 0);
    CHECK(destination.processor.getInsertEffectType(2, selected) == effects::Type::chorus);
    CHECK(destination.processor.getModulationInfoForParameter(effects::parameterID(2, selected, 0)).isModulated);
    auto copied = chain::getClipboard(chain::ClipboardKind::module);
    REQUIRE(copied != nullptr);
    CHECK(copied->modules.front().type == effects::Type::phaser);
    CHECK_FALSE(copied->includesRouting);
    CHECK(copied->modules.front().routes.isEmpty());
}

TEST_CASE("Duplicate settings creates one undoable module and stale module commands are inert",
          "[chain-tools][ui][undo][lifecycle]")
{
    RackFixture fixture;
    const int node = fixture.add(effects::Type::delay);
    const auto original = capture(fixture.processor, 1).modules.size();
    auto duplicate = fixture.rack.createModuleMenuResultHandler(node);
    duplicate(Rack::duplicateSettings);
    CHECK(capture(fixture.processor, 1).modules.size() == original + 1);
    duplicate(Rack::duplicateSettings);
    CHECK(capture(fixture.processor, 1).modules.size() == original + 1);
    REQUIRE(fixture.processor.undoEdit());
    CHECK(capture(fixture.processor, 1).modules.size() == original);
    fixture.rack.refresh();
    auto stale = fixture.rack.createModuleMenuResultHandler(node);
    fixture.processor.removeModule(1, node);
    REQUIRE(fixture.processor.addInsertEffect(1, effects::Type::delay) == node - fire::module_order::firstInsert);
    const auto changed = capture(fixture.processor, 1).modules.size();
    stale(Rack::duplicateSettings);
    CHECK(capture(fixture.processor, 1).modules.size() == changed);
    fixture.rack.refresh();
    stale = fixture.rack.createModuleMenuResultHandler(node);
    fixture.rack.setVisible(false);
    fixture.rack.setVisible(true);
    stale(Rack::removeEffect);
    CHECK(fixture.processor.isModulePresent(1, node));
}

TEST_CASE("Chain menu replaces only its target scope and accepts an empty copied chain",
          "[chain-tools][ui][clipboard][chain]")
{
    RackFixture source, target;
    clear(source.processor, 1);
    source.rack.refresh();
    source.add(effects::Type::reverb);
    chainCommand(source.rack, Rack::copyChainSettings);
    auto clipboard = chain::getClipboard(chain::ClipboardKind::chain);
    REQUIRE(clipboard != nullptr);
    REQUIRE(clipboard->modules.size() == 1);
    const auto unaffected = capture(target.processor, 2);
    chainCommand(target.rack, Rack::pasteChain);
    const auto pasted = capture(target.processor, 1);
    REQUIRE(pasted.modules.size() == 1);
    CHECK(pasted.modules.front().type == effects::Type::reverb);
    CHECK(capture(target.processor, 2).modules.size() == unaffected.modules.size());
    clear(source.processor, 1);
    source.rack.refresh();
    chainCommand(source.rack, Rack::copyChainSettings);
    REQUIRE(chain::getClipboard(chain::ClipboardKind::chain)->modules.empty());
    target.rack.refresh();
    chainCommand(target.rack, Rack::pasteChain);
    CHECK(capture(target.processor, 1).modules.empty());
}

TEST_CASE("Chain file callbacks save an immutable capture and reject hidden rebound or replaced targets",
          "[chain-tools][ui][files][lifecycle]")
{
    RackFixture fixture;
    TempFiles files;
    clear(fixture.processor, 1);
    fixture.rack.refresh();
    fixture.add(effects::Type::granular);
    auto save = ChainClipboardUiTestAccess::fileHandler(fixture.rack, true, false);
    const auto withoutSuffix = files.root.getChildFile("Texture");
    save(withoutSuffix);
    const auto savedFile = files.root.getChildFile("Texture.firechain");
    REQUIRE(savedFile.existsAsFile());
    CHECK_FALSE(ChainClipboardUiTestAccess::fileActive(fixture.rack));
    chain::Snapshot saved;
    REQUIRE(chain::read(savedFile, saved).wasOk());
    REQUIRE(saved.modules.size() == 1);
    CHECK(saved.modules.front().type == effects::Type::granular);
    save(files.root.getChildFile("replayed"));
    CHECK_FALSE(files.root.getChildFile("replayed.firechain").existsAsFile());

    auto stale = ChainClipboardUiTestAccess::fileHandler(fixture.rack, true, false);
    fixture.rack.setScope(2);
    stale(files.root.getChildFile("wrong-band"));
    CHECK_FALSE(files.root.getChildFile("wrong-band.firechain").existsAsFile());
    auto load = ChainClipboardUiTestAccess::fileHandler(fixture.rack, false, false);
    fixture.processor.addInsertEffect(2, effects::Type::phaser);
    load(savedFile);
    CHECK_FALSE(ChainClipboardUiTestAccess::fileActive(fixture.rack));
    CHECK(capture(fixture.processor, 2).modules.size() > 1);
    fixture.rack.refresh();
    load = ChainClipboardUiTestAccess::fileHandler(fixture.rack, false, false);
    load(savedFile);
    const auto loaded = capture(fixture.processor, 2);
    REQUIRE(loaded.modules.size() == 1);
    CHECK(loaded.modules.front().type == effects::Type::granular);
    fixture.rack.refresh();
    stale = ChainClipboardUiTestAccess::fileHandler(fixture.rack, false, false);
    fixture.rack.setVisible(false);
    stale(savedFile);
    CHECK_FALSE(ChainClipboardUiTestAccess::fileActive(fixture.rack));
}

TEST_CASE("Failed chain operations expose a useful error without mutating the current chain",
          "[chain-tools][ui][errors]")
{
    RackFixture source, target;
    const int node = source.add(effects::Type::chorus);
    command(source.rack, node, Rack::copySettings);
    for (int slot = 0; slot < effects::slotCount; ++slot) target.add(effects::Type::phaser);
    const auto before = capture(target.processor, 1).modules.size();
    auto forcedPaste = target.rack.createAddMenuResultHandler();
    forcedPaste(Rack::pasteCopiedModule);
    CHECK(capture(target.processor, 1).modules.size() == before);
    CHECK(ChainClipboardUiTestAccess::error(target.rack).isNotEmpty());
    TempFiles files;
    const auto invalid = files.root.getChildFile("Invalid.firechain");
    REQUIRE(invalid.replaceWithText("<not-a-chain />"));
    auto load = ChainClipboardUiTestAccess::fileHandler(target.rack, false, false);
    load(invalid);
    CHECK(ChainClipboardUiTestAccess::error(target.rack).isNotEmpty());
    CHECK(capture(target.processor, 1).modules.size() == before);
}

TEST_CASE("Chain tool affordance remains light and separate from add in all skins",
          "[chain-tools][ui][skin][layout]")
{
    RackFixture fixture;
    fixture.add(effects::Type::chorus);
    auto& tools = ChainClipboardUiTestAccess::button(fixture.rack);
    auto& add = ChainClipboardUiTestAccess::add(fixture.rack);
    for (auto skin : fire::ui::skins)
    {
        fire::ui::setSkin(fixture.rack, skin);
        fixture.lookAndFeel.setSkin(skin);
        fixture.rack.sendLookAndFeelChange();
        for (float scale : {.75f, 1.f, 1.5f})
        {
            fixture.rack.setScale(scale);
            fixture.rack.setSize(juce::roundToInt(210 * scale), juce::roundToInt(390 * scale));
            CHECK_FALSE(tools.getBounds().intersects(add.getBounds()));
            CHECK(fixture.rack.getLocalBounds().contains(tools.getBounds()));
            CHECK(tools.getHeight() >= 15);
            CHECK(tools.getTooltip().contains("destination instance"));
            auto* access = tools.getAccessibilityHandler();
            REQUIRE(access != nullptr);
            CHECK(access->getTitle() == "Chain tools");
            const auto image = fixture.rack.createComponentSnapshot(fixture.rack.getLocalBounds());
            REQUIRE(image.isValid());
            if (scale == 1.f)
                if (const auto* directory = std::getenv("FIRE_CHAIN_PREVIEW_DIR"))
                {
                    const auto file = juce::File(directory).getChildFile(juce::String(fire::ui::skinName(skin)).toLowerCase() + "-chain-tools.png");
                    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
                    auto stream = file.createOutputStream(); REQUIRE(stream != nullptr);
                    stream->setPosition(0); stream->truncate();
                    REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *stream));
                }
        }
    }
}
