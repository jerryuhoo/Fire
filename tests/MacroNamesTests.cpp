#include <PluginEditor.h>
#include <Utility/FactoryPresets.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

struct MacroBarTestAccess { static void applySkin(FireAudioProcessorEditor& e, fire::ui::Skin s) { e.applySkin(s); } };

TEST_CASE("Macro names roundtrip presets and host state with legacy defaults", "[macro][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const fire::macro_names::Names names {"Drive", "Tone", "空间", "Dry blend"};
    for (int i = 0; i < 4; ++i) REQUIRE(processor.setMacroDisplayName(i, names[static_cast<size_t>(i)]));
    juce::XmlElement preset("WINGSFIRE");
    state::saveStateToXml(processor, preset);
    REQUIRE(state::canLoadStateFromXml(preset, processor));
    REQUIRE(processor.isCurrentStateEquivalentToPreset(preset));
    REQUIRE(processor.setMacroDisplayName(0, "Different"));
    REQUIRE_FALSE(processor.isCurrentStateEquivalentToPreset(preset));
    REQUIRE(state::loadStateFromXml(preset, processor));
    CHECK(processor.getMacroDisplayNames() == names);

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    FireAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(restored.getMacroDisplayNames() == names);
    auto malformed = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(malformed != nullptr);
    malformed->setAttribute("macroNamesSchemaVersion", 2);
    juce::AudioProcessor::copyXmlToBinary(*malformed, state);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(restored.getMacroDisplayNames() == names);

    preset.removeAttribute("macroNamesSchemaVersion");
    for (int i = 0; i < 4; ++i) preset.removeAttribute(fire::macro_names::attribute(i));
    REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK(restored.getMacroDisplayNames() == fire::macro_names::defaults());
    restored.statePresets.initPreset();
    CHECK(restored.getMacroDisplayNames() == fire::macro_names::defaults());
}

TEST_CASE("Macro rename participates in undo and AB without changing parameter identities", "[macro][undo][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto count = processor.getParameters().size();
    auto* macro = processor.treeState.getParameter("macro1");
    REQUIRE(macro != nullptr);
    REQUIRE(processor.setMacroDisplayName(0, "Warmth"));
    processor.checkpointEditHistory();
    REQUIRE(processor.undoEdit());
    CHECK(processor.getMacroDisplayName(0) == "Macro 1");
    REQUIRE(processor.redoEdit());
    CHECK(processor.getMacroDisplayName(0) == "Warmth");
    processor.stateAB.copyAB();
    REQUIRE(processor.setMacroDisplayName(0, "Edge"));
    processor.stateAB.toggleAB();
    CHECK(processor.getMacroDisplayName(0) == "Warmth");
    processor.stateAB.toggleAB();
    CHECK(processor.getMacroDisplayName(0) == "Edge");
    CHECK(processor.getParameters().size() == count);
    CHECK(processor.treeState.getParameter("macro1") == macro);
    CHECK_FALSE(processor.setMacroDisplayName(-1, "Invalid"));
    const auto oldRevision = processor.getMacroNamesRevision();
    REQUIRE(processor.setMacroDisplayName(1, "New scene"));
    CHECK_FALSE(processor.setMacroDisplayName(0, "Stale edit", oldRevision));
    CHECK(processor.getMacroDisplayName(0) == "Edge");
    REQUIRE(processor.setMacroDisplayName(0, "  Soft\n colour  "));
    CHECK(processor.getMacroDisplayName(0) == "Soft colour");
}

TEST_CASE("Macro bar fits all four skins and editor scales", "[macro][skin][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    for (auto skin : fire::ui::skins)
        for (int width : {1000, 1400, 2000})
        {
            MacroBarTestAccess::applySkin(editor, skin);
            editor.setSize(width, width / 2);
            juce::Component* bar = nullptr;
            for (auto* child : editor.getChildren())
                if (child->getComponentID() == "performance_macros") bar = child;
            REQUIRE(bar != nullptr);
            REQUIRE(bar->isVisible());
            CHECK(editor.getLocalBounds().contains(bar->getBounds()));
            for (auto* child : bar->getChildren()) CHECK(bar->getLocalBounds().contains(child->getBounds()));
            auto snapshot = editor.createComponentSnapshot(editor.getLocalBounds());
            REQUIRE(snapshot.isValid());
            if (const auto* directory = std::getenv("FIRE_MACRO_PREVIEW_DIR"))
            {
                auto file = juce::File(directory).getChildFile(juce::String(fire::ui::skinName(skin)).toLowerCase() + "-macros-" + juce::String(width) + ".png");
                REQUIRE(file.getParentDirectory().createDirectory().wasOk());
                auto stream = file.createOutputStream(); REQUIRE(stream != nullptr);
                stream->setPosition(0); stream->truncate();
                REQUIRE(juce::PNGImageFormat{}.writeImageToStream(snapshot, *stream));
            }
        }
}

namespace
{
juce::MouseEvent macroBarDragEvent(juce::Component& component,
                                    juce::Point<float> position,
                                    juce::Point<float> down,
                                    bool dragged)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), position,
             juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component,
             now, down, now, 1, dragged };
}

struct MacroBarHostGestureCapture final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged(int, float) override { ++valueChanges; }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting) ++starts;
        else
        {
            ++ends;
            sliderWasAliveAtEnd = slider != nullptr
                && slider->getParentComponent() != nullptr;
        }
    }
    juce::Component::SafePointer<PrimarySlider> slider;
    int starts = 0, ends = 0, valueChanges = 0;
    bool sliderWasAliveAtEnd = false;
};

struct MacroBarSliderGestureCapture final : juce::Slider::Listener
{
    void sliderValueChanged(juce::Slider*) override {}
    void sliderDragStarted(juce::Slider*) override { ++starts; }
    void sliderDragEnded(juce::Slider*) override { ++ends; }
    int starts = 0, ends = 0;
};
}

TEST_CASE("Closing editor balances a real macro drag before attachment teardown",
          "[macro][ui][gesture][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto* parameter = processor.treeState.getParameter("macro1");
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(.5f);
    MacroBarHostGestureCapture hostGesture;
    MacroBarSliderGestureCapture sliderGesture;
    parameter->addListener(&hostGesture);
    const juce::ScopeGuard removeHostListener { [&] { parameter->removeListener(&hostGesture); } };
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->stopTimer();
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::Component* bar = nullptr;
    for (auto* child : editor->getChildren())
        if (child->getComponentID() == "performance_macros") bar = child;
    REQUIRE(bar != nullptr);
    PrimarySlider* slider = nullptr;
    for (auto* child : bar->getChildren())
        if (child->getComponentID() == "macro1") slider = dynamic_cast<PrimarySlider*>(child);
    REQUIRE(slider != nullptr);
    REQUIRE(slider->isShowing());
    hostGesture.slider = slider;
    slider->addListener(&sliderGesture);
    const auto down = slider->getLocalBounds().toFloat().getCentre();
    slider->mouseDown(macroBarDragEvent(*slider, down, down, false));
    slider->mouseDrag(macroBarDragEvent(*slider, down + juce::Point<float> { 18, -12 }, down, true));
    REQUIRE(slider->hasActivePointerGesture());
    REQUIRE(hostGesture.starts == 1);
    REQUIRE(hostGesture.ends == 0);
    REQUIRE(hostGesture.valueChanges > 0);
    REQUIRE(sliderGesture.starts == 1);
    REQUIRE(sliderGesture.ends == 0);

    // Do not send mouseUp or hide first: editor destruction itself must end
    // the active Slider gesture while its APVTS attachment is still listening.
    editor.reset();
    CHECK(hostGesture.slider == nullptr);
    CHECK(hostGesture.sliderWasAliveAtEnd);
    CHECK(hostGesture.starts == 1);
    CHECK(hostGesture.ends == 1);
    CHECK(sliderGesture.starts == 1);
    CHECK(sliderGesture.ends == 1);
}
