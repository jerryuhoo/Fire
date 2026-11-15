#include <GUI/EffectRackNavigation.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

struct ChainChannelModeUiTestAccess
{
    static ContextAwareComboBox& selector(fire::ui::EffectRackNavigation& rack)
    {if(rack.isShowing() && !rack.channelMode.isShowing())rack.showRouting();return rack.channelMode;}
    static auto& summary(fire::ui::EffectRackNavigation& rack){return rack.routingButton;}
    static void reopen(fire::ui::EffectRackNavigation& rack){rack.closeRouting();rack.showRouting();}
    static const juce::Button& tools(const fire::ui::EffectRackNavigation& rack) { return rack.chainButton; }
    static const juce::Button& add(const fire::ui::EffectRackNavigation& rack) { return rack.addButton; }
    static std::function<void(int)> choice(ContextAwareComboBox& combo) { return combo.createPopupResultHandler(); }
    static bool key(ContextAwareComboBox& combo, int key) { return combo.keyPressed(juce::KeyPress(key)); }
};

namespace
{
using Rack = fire::ui::EffectRackNavigation;
using Access = ChainChannelModeUiTestAccess;
namespace mode = fire::channel_mode;

struct Fixture
{
    FireAudioProcessor processor;
    FireLookAndFeel look;
    juce::Component host;
    Rack rack {processor, 1};
    Fixture()
    {
        rack.setLookAndFeel(&look);
        rack.setSize(210, 258);
        host.setSize(800,620);host.addAndMakeVisible(rack);
        host.addToDesktop(juce::ComponentPeer::windowIsTemporary);host.setVisible(true);
        rack.setVisible(true);
    }
    ~Fixture() { rack.setLookAndFeel(nullptr); }
    float value(int scope)
    {
        auto* parameter = processor.treeState.getRawParameterValue(mode::parameterID(scope));
        REQUIRE(parameter != nullptr);
        return parameter->load();
    }
    void choose(int id) { Access::choice(Access::selector(rack))(id); }
};
struct GestureCounter : juce::AudioProcessorParameter::Listener
{
    int start = 0, end = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool beginning) override { beginning ? ++start : ++end; }
};
}

TEST_CASE("Chain processing selector binds each Band and Master independently", "[chain-channel-ui][ui][midside]")
{
    Fixture fixture;
    auto& selector = Access::selector(fixture.rack);
    REQUIRE(selector.getSelectedId() == 1);
    CHECK(selector.getText() == "Stereo");
    CHECK(selector.getNumItems() == 3);
    fixture.choose(2);
    CHECK(fixture.value(1) == mode::mid);
    for (int scope : {0, 2, 3, 4})
    {
        fixture.rack.setScope(scope);
        CHECK(selector.getSelectedId() == 1);
        fixture.choose(3);
        CHECK(fixture.value(scope) == mode::side);
        CHECK(fixture.value(1) == mode::mid);
        CHECK(selector.getComponentID() == mode::parameterID(scope));
    }
    fixture.rack.setScope(1);
    CHECK(selector.getSelectedId() == 2);
    CHECK(selector.getText() == "Mid");
    fixture.processor.treeState.getParameter(mode::parameterID(1))->setValueNotifyingHost(0);
    CHECK(selector.getSelectedId() == 1);
}

TEST_CASE("Chain processing keyboard edits are deliberate complete host gestures and undoable", "[chain-channel-ui][ui][keyboard][undo]")
{
    Fixture fixture;
    GestureCounter gestures;
    auto* parameter = fixture.processor.treeState.getParameter(mode::parameterID(1));
    REQUIRE(parameter != nullptr);
    parameter->addListener(&gestures);
    auto& selector = Access::selector(fixture.rack);
    CHECK(Access::key(selector, juce::KeyPress::rightKey));
    CHECK(fixture.value(1) == mode::mid);
    CHECK(gestures.start == 1);
    CHECK(gestures.end == 1);
    CHECK(Access::key(selector, juce::KeyPress::leftKey));
    CHECK(fixture.value(1) == mode::stereo);
    CHECK(gestures.start == 2);
    CHECK(gestures.end == 2);
    REQUIRE(fixture.processor.undoEdit());
    CHECK(fixture.value(1) == mode::mid);
    parameter->removeListener(&gestures);
}

TEST_CASE("Chain processing popup cannot change rebound hidden disabled or replaced chains", "[chain-channel-ui][ui][lifecycle]")
{
    Fixture fixture;
    auto& selector = Access::selector(fixture.rack);
    auto stale = Access::choice(selector);
    fixture.rack.setScope(2);
    stale(3);
    CHECK(fixture.value(1) == mode::stereo);
    CHECK(fixture.value(2) == mode::stereo);
    stale = Access::choice(Access::selector(fixture.rack));
    fixture.rack.setVisible(false);
    fixture.rack.setVisible(true);
    stale(3);
    CHECK(fixture.value(2) == mode::stereo);
    stale = Access::choice(Access::selector(fixture.rack));
    fixture.rack.setEnabled(false);
    fixture.rack.setEnabled(true);
    stale(3);
    CHECK(fixture.value(2) == mode::stereo);
    stale = Access::choice(Access::selector(fixture.rack));
    fixture.processor.removeModule(2, 0);
    stale(3);
    CHECK(fixture.value(2) == mode::stereo);
    Access::reopen(fixture.rack);
    auto current = Access::choice(Access::selector(fixture.rack));
    current(2);
    current(3);
    CHECK(fixture.value(2) == mode::mid);
    auto ephemeral = std::make_unique<Rack>(fixture.processor, 3);
    auto orphaned = Access::choice(Access::selector(*ephemeral));
    ephemeral.reset();
    orphaned(3);
    CHECK(fixture.value(3) == mode::stereo);
}

TEST_CASE("Processing channel is accessible inside the existing rail header in every skin", "[chain-channel-ui][ui][skin][layout]")
{
    Fixture fixture;
    auto& selector = Access::selector(fixture.rack);
    auto& summary=Access::summary(fixture.rack);
    for (auto skin : fire::ui::skins)
    {
        fire::ui::setSkin(fixture.rack, skin);
        fixture.look.setSkin(skin);
        fixture.rack.sendLookAndFeelChange();
        for (float scale : {.75f, 1.f, 1.5f, 2.f})
        {
            fixture.look.scale = scale;
            fixture.rack.setScale(scale);
            fixture.rack.setSize(juce::roundToInt(180 * scale), juce::roundToInt(258 * scale));
            CHECK(fixture.rack.getLocalBounds().contains(summary.getBounds()));
            CHECK(summary.getWidth() == juce::roundToInt(70 * scale));
            CHECK(summary.getHeight() == juce::roundToInt(21 * scale));
            CHECK(summary.getX() >= juce::roundToInt(8 * scale) + juce::roundToInt(40 * scale));
            CHECK(summary.getY() == Access::tools(fixture.rack).getY());
            CHECK_FALSE(summary.getBounds().intersects(Access::tools(fixture.rack).getBounds()));
            CHECK_FALSE(summary.getBounds().intersects(Access::add(fixture.rack).getBounds()));
            CHECK_FALSE(summary.getBounds().intersects(fixture.rack.getViewport().getBounds()));
            CHECK(fixture.rack.getViewport().getY() == juce::roundToInt(8 * scale) + juce::roundToInt(22 * scale));
            auto* accessibility = selector.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK(accessibility->getTitle() == "Band 1 processing channel");
            CHECK(accessibility->getHelp().contains("stereo difference"));
            CHECK(accessibility->getHelp().contains("mono input"));
            CHECK(accessibility->getHelp().contains("spectrum display"));
        }
    }
}

TEST_CASE("Band processing channel previews show the complete module panel in all four skins", "[chain-channel-ui][ui][skin][preview]")
{
    const auto* directory = std::getenv("FIRE_CHANNEL_MODE_PREVIEW_DIR");
    if (!directory) return;
    FireAudioProcessor processor;
    FireLookAndFeel look;
    BandPanel panel(processor, {}, {}, {}, {}, {});
    panel.setLookAndFeel(&look);
    panel.setSize(984, 258);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    processor.treeState.getParameter(mode::parameterID(1))->setValueNotifyingHost(.5f);
    for (auto skin : fire::ui::skins)
    {
        fire::ui::setSkin(panel, skin);
        look.setSkin(skin);
        panel.sendLookAndFeelChange();
        auto image = panel.createComponentSnapshot(panel.getLocalBounds());
        REQUIRE(image.isValid());
        auto file = juce::File(directory).getChildFile(juce::String(fire::ui::skinName(skin)).toLowerCase() + "-mid-chain.png");
        REQUIRE(file.getParentDirectory().createDirectory().wasOk());
        auto stream = file.createOutputStream(); REQUIRE(stream != nullptr);
        stream->setPosition(0); stream->truncate();
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *stream));
    }
    panel.setLookAndFeel(nullptr);
}
