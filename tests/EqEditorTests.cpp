#include <PluginEditor.h>
#include <Panels/SpectrogramPanel/FilterControl.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
template <typename T, typename Predicate>
T* findEqControl(juce::Component& root, Predicate predicate)
{
    if (auto* c = dynamic_cast<T*>(&root); c && predicate(*c)) return c;
    for (auto* child : root.getChildren())
        if (auto* c = findEqControl<T>(*child, predicate)) return c;
    return nullptr;
}
void setEqParameter(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void saveEqView(juce::Component& editor, const juce::String& name)
{
    const auto path = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
    if (path.isEmpty()) return;
    auto stream = juce::File(path).getChildFile(name + ".png").createOutputStream();
    REQUIRE(stream != nullptr);
    stream->setPosition(0); stream->truncate();
    CHECK(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream));
}
}

TEST_CASE("EQ selection shows only its point controls and retains parameter identities after deletion",
          "[eq][ui][regression]")
{
    FireAudioProcessor p; p.hasUpdateCheckBeenPerformed = true;
    p.setRateAndBufferSizeDetails(48000, 128);
    p.prepareToPlay(48000, 128);
    GlobalPanel panel(p, {}, {}, {}, {}, {});
    panel.setSize(1000, 270);
    const int fourth = p.addEqNode(2200, 6);
    const int fifth = p.addEqNode(6500, -3);
    REQUIRE(fourth == 3); REQUIRE(fifth == 4);
    panel.selectEqNode(fifth);
    CHECK(panel.getSelectedEqNode() == fifth);
    int visibleCount = 0;
    for (auto* slider : panel.getModulatableSliders())
        if (slider->getParentComponent() == &panel.getEqControls() && slider->isVisible())
        {
            ++visibleCount;
            CHECK(slider->parameterID.startsWith("eqNode5"));
            CHECK(panel.getEqControls().getLocalBounds().contains(slider->getBounds()));
        }
    CHECK(visibleCount == 3);
    REQUIRE(p.removeEqNode(fourth));
    panel.getEqControls().refresh();
    CHECK(panel.getSelectedEqNode() == fifth);
    CHECK(p.getEqNodeState(fifth).frequency == Catch::Approx(6500));
    auto* dot = findEqControl<juce::Button>(panel, [](auto& b) {return b.getComponentID() == "eqPointNavigation5";});
    REQUIRE(dot != nullptr);
    CHECK(dot->getTitle() == "EQ point 4");
    auto* type = findEqControl<juce::ComboBox>(panel, [&](auto& c) {return c.getComponentID() == fire::eq::parameterID(fifth, fire::eq::Field::type);});
    REQUIRE(type != nullptr);
    type->setSelectedId(static_cast<int>(fire::eq::Type::notch) + 1, juce::sendNotificationSync);
    CHECK(p.getEqNodeState(fifth).type == fire::eq::Type::notch);
    CHECK(p.getEqNodeState(1).type == fire::eq::Type::bell);
    panel.getEqControls().refresh();
    auto* gain = findEqControl<ModulatableSlider>(panel, [&](auto& c) {return c.parameterID == fire::eq::parameterID(fifth, fire::eq::Field::gain);});
    REQUIRE(gain != nullptr); CHECK_FALSE(gain->isEnabled());
    for (int i = 0; i < fire::eq::maxNodes; ++i) p.removeEqNode(i);
    panel.getEqControls().refresh();
    CHECK(panel.getSelectedEqNode() == -1);
    const int reused = p.addEqNode(120, 2);
    panel.selectEqNode(reused);
    CHECK(panel.getSelectedEqNode() == reused);
}

TEST_CASE("Double clicking the EQ spectrum adds a point at the clicked coordinates",
          "[eq][ui][regression]")
{
    FireAudioProcessor p; p.hasUpdateCheckBeenPerformed = true;
    p.setRateAndBufferSizeDetails(48000, 128);
    p.prepareToPlay(48000, 128);
    GlobalPanel panel(p, {}, {}, {}, {}, {});
    FilterControl spectrum(p, panel);
    spectrum.setBounds(0, 0, 1000, 240);
    const auto time = juce::Time::getCurrentTime();
    const juce::Point<float> position {500, 90};
    const juce::MouseEvent event {juce::Desktop::getInstance().getMainMouseSource(), position,
        juce::ModifierKeys::leftButtonModifier, 0, 0, 0, 0, 0, &spectrum, &spectrum,
        time, position, time, 2, false};
    spectrum.mouseDoubleClick(event);
    REQUIRE(p.getEqNodeState(3).present);
    CHECK(p.getEqNodeState(3).frequency == Catch::Approx(std::sqrt(20.0 * 20000.0)).margin(0.51)); // whole-Hz parameter steps
    CHECK(p.getEqNodeState(3).gainDb == Catch::Approx(6));
    CHECK(panel.getSelectedEqNode() == 3);
    CHECK(p.treeState.getRawParameterValue(FILTER_BYPASS_ID)->load() > 0.5f);
    CHECK(spectrum.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    CHECK_FALSE(p.getEqNodeState(3).present);
}

TEST_CASE("EQ panel and twelve-point navigation fit all editor scales",
          "[eq][ui][layout]")
{
    FireAudioProcessor p; p.hasUpdateCheckBeenPerformed = true;
    p.setRateAndBufferSizeDetails(48000, 128);
    p.prepareToPlay(48000, 128);
    setEqParameter(p, FILTER_BYPASS_ID, 1);
    for (int i = 3; i < fire::eq::maxNodes; ++i)
        REQUIRE(p.addEqNode(90.0f * std::pow(1.72f, static_cast<float>(i - 3)), (i % 2 ? 5.0f : -4.0f)) == i);
    FireAudioProcessorEditor editor(p);
    editor.stopTimer();
    auto* master = findEqControl<juce::Button>(editor, [](auto& b) {return b.getButtonText() == "MASTER LAB";});
    REQUIRE(master != nullptr);
    master->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto* panel = findEqControl<GlobalPanel>(editor, [](auto&) {return true;});
    REQUIRE(panel != nullptr);
    panel->selectEqNode(5);
    for (int width : {1000, 1400, 2000})
    {
        editor.setSize(width, width / 2);
        panel->getEqControls().refresh();
        // The fixture has no peer/animation clock. Synchronise the spectrum's
        // selected marker before capturing the same-size first frame.
        if (auto* spectrum = findEqControl<FilterControl>(editor, [](auto&) {return true;})) spectrum->resized();
        for (auto* child : panel->getEqControls().getChildren())
            if (child->isVisible())
            {
                CAPTURE(width, child->getTitle(), child->getBounds().toString());
                CHECK(panel->getEqControls().getLocalBounds().contains(child->getBounds()));
            }
        saveEqView(editor, "eq-twelve-points-" + juce::String(width));
    }
}
