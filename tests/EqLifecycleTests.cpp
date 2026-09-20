#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace
{
class EqGestureObserver final : public juce::AudioProcessorListener
{
public:
    EqGestureObserver(FireAudioProcessor& source, int index)
        : processor(source), parameterIndex(index)
    {
        processor.addListener(this);
    }

    ~EqGestureObserver() override { processor.removeListener(this); }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*,
                               const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int index) override
    {
        if (index == parameterIndex) ++starts;
    }
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int index) override
    {
        if (index != parameterIndex) return;
        ++ends;
        auto callback = std::move(onEnd);
        if (callback) callback();
    }

    std::function<void()> onEnd;
    int starts = 0, ends = 0;

private:
    FireAudioProcessor& processor;
    int parameterIndex;
};

void startEqGainGesture(ModulatableSlider& slider)
{
    REQUIRE(slider.isVisible());
    REQUIRE(slider.isEnabled());
    REQUIRE_FALSE(slider.getLocalBounds().isEmpty());
    const auto position = slider.getLocalBounds().toFloat().getCentre();
    const auto time = juce::Time::getCurrentTime();
    slider.mouseDown({juce::Desktop::getInstance().getMainMouseSource(), position,
        juce::ModifierKeys::leftButtonModifier, 0, 0, 0, 0, 0, &slider, &slider,
        time, position, time, 1, false});
    REQUIRE(slider.hasActiveInteraction());
}

void setEqPlainValue(FireAudioProcessor& processor, int slot,
                     fire::eq::Field field, float value)
{
    auto* parameter = processor.treeState.getParameter(fire::eq::parameterID(slot, field));
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

juce::ComboBox* findEqMenu(juce::Component& component, const juce::String& id)
{
    if (auto* menu = dynamic_cast<juce::ComboBox*>(&component);
        menu != nullptr && menu->getComponentID() == id) return menu;
    for (auto* child : component.getChildren())
        if (auto* menu = findEqMenu(*child, id)) return menu;
    return nullptr;
}
}

TEST_CASE("EQ refresh can end an active Gain gesture while the host closes its panel",
          "[eq][ui][lifecycle][gestures][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto panel = std::make_unique<GlobalPanel>(processor, nullptr, nullptr, nullptr, nullptr, nullptr);
    panel->setBounds(0, 0, 1000, 300);
    panel->setVisible(true);
    panel->selectEqNode(1);
    auto* gain = &panel->getPeakGainKnob();
    auto* parameter = processor.treeState.getParameter(PEAK_GAIN_ID);
    REQUIRE(parameter != nullptr);
    EqGestureObserver observer(processor, parameter->getParameterIndex());
    bool callbackCompleted = false;

    SECTION("host hides the panel")
    {
        observer.onEnd = [&] { panel->setVisible(false); callbackCompleted = true; };
        startEqGainGesture(*gain);
        REQUIRE(observer.starts == 1);
        setEqPlainValue(processor, 1, fire::eq::Field::type, static_cast<float>(fire::eq::Type::notch));
        panel->getEqControls().refresh();
        CHECK(callbackCompleted);
        CHECK_FALSE(panel->isVisible());
        CHECK_FALSE(gain->hasActiveInteraction());
    }
    SECTION("host deletes the panel")
    {
        observer.onEnd = [&] { panel.reset(); callbackCompleted = true; };
        startEqGainGesture(*gain);
        REQUIRE(observer.starts == 1);
        setEqPlainValue(processor, 1, fire::eq::Field::type, static_cast<float>(fire::eq::Type::notch));
        auto* controls = &panel->getEqControls();
        controls->refresh();
        CHECK(callbackCompleted);
        CHECK(panel == nullptr);
    }
    CHECK(observer.starts == 1);
    CHECK(observer.ends == 1);
}

TEST_CASE("An empty EQ emits its selection change only once across idle refreshes",
          "[eq][ui][lifecycle][idle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    panel.setBounds(0, 0, 1000, 300);
    REQUIRE(panel.getSelectedEqNode() >= 0);
    std::vector<int> selections;
    panel.getEqControls().onSelectionChanged = [&](int selected) { selections.push_back(selected); };
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot) processor.removeEqNode(slot);
    panel.getEqControls().refresh();
    REQUIRE(panel.getSelectedEqNode() == -1);
    REQUIRE(selections == std::vector<int> {-1});
    for (int frame = 0; frame < 120; ++frame) panel.getEqControls().refresh();
    CHECK(selections == std::vector<int> {-1});
    panel.getEqControls().onSelectionChanged = nullptr;
}

TEST_CASE("Nested EQ selection from a gesture end wins over an older selection request",
          "[eq][ui][lifecycle][gestures][reentrant][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    panel.setBounds(0, 0, 1000, 300);
    panel.selectEqNode(1);
    auto& gain = panel.getPeakGainKnob();
    auto* parameter = processor.treeState.getParameter(PEAK_GAIN_ID);
    REQUIRE(parameter != nullptr);
    EqGestureObserver observer(processor, parameter->getParameterIndex());
    int newestSelection = -1;

    SECTION("explicit selection ends the previous point gesture")
    {
        newestSelection = 2;
        observer.onEnd = [&] { panel.selectEqNode(newestSelection); };
        startEqGainGesture(gain);
        REQUIRE(observer.starts == 1);
        panel.selectEqNode(0);
    }
    SECTION("deleting the selected point triggers a replacement selection")
    {
        newestSelection = 0;
        observer.onEnd = [&] { panel.selectEqNode(newestSelection); };
        startEqGainGesture(gain);
        REQUIRE(observer.starts == 1);
        REQUIRE(processor.removeEqNode(1));
        panel.getEqControls().refresh();
    }

    CHECK(observer.starts == 1);
    CHECK(observer.ends == 1);
    CHECK_FALSE(gain.hasActiveInteraction());
    REQUIRE(panel.getSelectedEqNode() == newestSelection);
    CHECK(panel.getLowcutGainKnob().isVisible() == (newestSelection == 0));
    CHECK(panel.getHighcutGainKnob().isVisible() == (newestSelection == 2));
    auto* type = findEqMenu(panel, fire::eq::parameterID(newestSelection, fire::eq::Field::type));
    REQUIRE(type != nullptr);
    type->setSelectedId(static_cast<int>(fire::eq::Type::lowShelf) + 1, juce::sendNotificationSync);
    CHECK(processor.getEqNodeState(newestSelection).type == fire::eq::Type::lowShelf);
}
