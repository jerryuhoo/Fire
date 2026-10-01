#include <PluginEditor.h>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

namespace
{
juce::Point<int> sizeOf(juce::Rectangle<int> bounds)
{ return {bounds.getWidth(), bounds.getHeight()}; }
template <typename T, typename Predicate>
T* findKnobLayoutControl(juce::Component& root, Predicate predicate)
{
    if (auto* control = dynamic_cast<T*>(&root); control && predicate(*control)) return control;
    for (auto* child : root.getChildren())
        if (auto* control = findKnobLayoutControl<T>(*child, predicate)) return control;
    return nullptr;
}

void activateKnobLayoutButton(juce::Component& root, const juce::String& name)
{
    auto* button = findKnobLayoutControl<juce::Button>(root,
        [&](auto& control) { return control.getButtonText() == name; });
    REQUIRE(button != nullptr);
    button->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
}

void checkOrdinaryKnobs(juce::Component& root, juce::Point<int> boxSize,
                        juce::Point<int> dialSize, int& checked)
{
    for (auto* child : root.getChildren())
    {
        if (! child->isVisible()) continue;
        if (auto* slider = dynamic_cast<juce::Slider*>(child); slider && slider->isRotary())
        {
            const auto layout = slider->getLookAndFeel().getSliderLayout(*slider);
            CAPTURE(slider->getComponentID(), slider->getTitle(), slider->getBounds().toString());
            if (slider->getComponentID() == "drive")
            {
                CHECK(layout.sliderBounds.getHeight() > dialSize.y);
                continue;
            }
            ++checked;
            CHECK(sizeOf(slider->getLocalBounds()) == boxSize);
            CHECK(sizeOf(layout.sliderBounds) == dialSize);
            CHECK(root.getLocalBounds().contains(slider->getBounds()));
            CHECK(slider->getLocalBounds().contains(layout.sliderBounds));
        }
        checkOrdinaryKnobs(*child, boxSize, dialSize, checked);
    }
}

void saveKnobLayout(juce::Component& editor, const juce::String& name)
{
    const auto path = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
    if (path.isEmpty()) return;
    auto stream = juce::File(path).getChildFile("ordinary-knobs-" + name + ".png").createOutputStream();
    REQUIRE(stream != nullptr);
    stream->setPosition(0);
    stream->truncate();
    CHECK(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream));
}
}

TEST_CASE("Ordinary rotary controls have identical boxes and dial areas across workspaces and scales",
          "[ui][layout][ordinary-knobs][regression][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    const std::array effects {fire::effects::Type::chorus, fire::effects::Type::delay,
                             fire::effects::Type::reverb, fire::effects::Type::granular,
                             fire::effects::Type::lofi};
    for (int scope : {0, 1})
        for (size_t slot = 0; slot < effects.size(); ++slot)
            REQUIRE(processor.addInsertEffect(scope, effects[slot]) == static_cast<int>(slot));

    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    auto* band = findKnobLayoutControl<BandPanel>(editor, [](auto&) { return true; });
    auto* master = findKnobLayoutControl<GlobalPanel>(editor, [](auto&) { return true; });
    REQUIRE(band != nullptr);
    REQUIRE(master != nullptr);
    auto* reference = findKnobLayoutControl<ModulatableSlider>(*master,
        [](auto& slider) { return slider.parameterID == OUTPUT_ID; });
    REQUIRE(reference != nullptr);

    for (int width : {1000, 1250, 1400, 2000})
    {
        CAPTURE(width);
        editor.setSize(width, width / 2);
        activateKnobLayoutButton(editor, "MASTER LAB");
        activateKnobLayoutButton(*master, "EQ");
        master->selectEqNode(1);
        const auto boxSize = sizeOf(reference->getLocalBounds());
        const auto dialSize = sizeOf(reference->getLookAndFeel().getSliderLayout(*reference).sliderBounds);
        REQUIRE(boxSize.x == juce::roundToInt(76.0f * static_cast<float>(width) / 1000.0f));
        REQUIRE(boxSize.y == boxSize.x + juce::roundToInt(20.0f * static_cast<float>(width) / 1000.0f));
        const auto checkPage = [&]
        {
            int checked = 0;
            checkOrdinaryKnobs(editor, boxSize, dialSize, checked);
            CHECK(checked >= 2);
        };
        checkPage();
        saveKnobLayout(editor, "master-eq-" + juce::String(width));
        activateKnobLayoutButton(*master, "Lo-Fi");
        checkPage();

        for (int scope : {0, 1})
        {
            auto& panel = scope == 0 ? static_cast<juce::Component&>(*master)
                                   : static_cast<juce::Component&>(*band);
            activateKnobLayoutButton(editor, scope == 0 ? "MASTER LAB" : "BAND LAB");
            auto* rack = findKnobLayoutControl<fire::ui::EffectRackNavigation>(panel, [](auto&) { return true; });
            REQUIRE(rack != nullptr);
            rack->refresh();
            for (size_t slot = 0; slot < effects.size(); ++slot)
            {
                CAPTURE(scope, slot);
                const auto id = fire::effects::parameterID(scope, static_cast<int>(slot), fire::effects::typeField);
                auto* row = findKnobLayoutControl<juce::Button>(panel,
                    [&](auto& button) { return button.getComponentID() == id; });
                REQUIRE(row != nullptr);
                row->triggerClick();
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
                checkPage();
                if (effects[slot] == fire::effects::Type::reverb)
                {
                    auto* model = findKnobLayoutControl<juce::ComboBox>(panel,
                        [](auto& menu) {return menu.getTitle() == "Reverb algorithm";});
                    REQUIRE(model != nullptr); REQUIRE(model->isVisible());
                    auto* parent = model->getParentComponent(); REQUIRE(parent != nullptr);
                    CHECK(parent->getLocalBounds().contains(model->getBounds()));
                    for (auto* child : parent->getChildren())
                        if (auto* slider = dynamic_cast<juce::Slider*>(child); slider && slider->isVisible())
                            CHECK_FALSE(model->getBounds().intersects(slider->getBounds()));
                    saveKnobLayout(editor, (scope == 0 ? "master-reverb-model-" : "band-reverb-model-") + juce::String(width));
                }
                if (effects[slot] == fire::effects::Type::granular)
                    saveKnobLayout(editor, (scope == 0 ? "master-clouds-" : "band-clouds-") + juce::String(width));
            }
        }

        for (const auto* name : {"Drive", "Shape", "Compressor", "Stereo", "OTT"})
        {
            CAPTURE(name);
            activateKnobLayoutButton(*band, name);
            checkPage();
            if (juce::String(name) == "Shape") saveKnobLayout(editor, "band-shape-" + juce::String(width));
        }

        activateKnobLayoutButton(editor, "MOD FORGE");
        checkPage();
        saveKnobLayout(editor, "lfo-" + juce::String(width));
    }
}

TEST_CASE("Expanded LFO source colours preserve the four historical identities",
          "[lfo-bank][ui][palette][compatibility]")
{
    REQUIRE(fire::ui::lfoBankCount == 16);
    const std::array<juce::Colour, 4> original {
        juce::Colour {0xffb968ff}, juce::Colour {0xff42d6ff},
        juce::Colour {0xff59e39b}, juce::Colour {0xffffc247}};
    for (size_t index = 0; index < original.size(); ++index)
        CHECK(fire::ui::lfoBankColour(static_cast<int>(index)) == original[index]);
    for (int index = 0; index < 16; ++index)
    {
        CHECK(fire::ui::isValidLfoSourceNumber(index + 1));
        CHECK(fire::ui::lfoBankColourForSource(index + 1) == fire::ui::lfoBankColour(index));
        CHECK(fire::ui::lfoBankColour(index) != fire::ui::colours::disabled);
        for (int other = 0; other < index; ++other)
            CHECK(fire::ui::lfoBankColour(index) != fire::ui::lfoBankColour(other));
    }
    CHECK_FALSE(fire::ui::isValidLfoSourceNumber(0));
    CHECK_FALSE(fire::ui::isValidLfoSourceNumber(17));
}
