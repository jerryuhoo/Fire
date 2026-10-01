#include <PluginEditor.h>
#include <GUI/EffectRackNavigation.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
using Type = fire::effects::Type;
template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate matches)
{
    if (auto* result = dynamic_cast<T*>(&root); result && matches(*result)) return result;
    for (auto* child : root.getChildren()) if (auto* result = find<T>(*child, matches)) return result;
    return nullptr;
}
void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void show(juce::Component& panel)
{
    panel.setSize(984, 258);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
}
}

TEST_CASE("Every default module can be deleted readded and undone without affecting another scope",
          "[shared-modules][state][edit-history]")
{
    FireAudioProcessor processor;
    for (int scope = 0; scope < 5; ++scope)
        for (int node = 0; node < (scope == 0 ? 3 : 5); ++node)
        {
            CAPTURE(scope, node);
            REQUIRE(processor.isModulePresent(scope, node));
            processor.removeModule(scope, node);
            CHECK_FALSE(processor.isModulePresent(scope, node));
            REQUIRE(processor.undoEdit());
            CHECK(processor.isModulePresent(scope, node));
            REQUIRE(processor.redoEdit());
            CHECK_FALSE(processor.isModulePresent(scope, node));
            REQUIRE(processor.restoreLegacyModule(scope, node));
            CHECK(processor.isModulePresent(scope, node));
            const auto order = processor.getModuleOrder(scope);
            int last = -1;
            for (int id : order) if (processor.isModulePresent(scope, id)) last = id;
            CHECK(last == node);
            REQUIRE(processor.undoEdit());
            CHECK_FALSE(processor.isModulePresent(scope, node));
            REQUIRE(processor.redoEdit());
            CHECK(processor.isModulePresent(scope, node));
            CHECK(processor.isModulePresent(scope == 0 ? 1 : 0, 0));
        }
}

TEST_CASE("Core module instances presence and EQ points survive host preset and AB state",
          "[shared-modules][state][preset][ab][eq]")
{
    FireAudioProcessor processor;
    for (int scope : {0, 1})
    {
        for (int i = 0; i < 6; ++i)
            REQUIRE(processor.addInsertEffect(scope, static_cast<Type>(static_cast<int>(Type::drive) + i)) == i);
        processor.removeModule(scope, 0);
        set(processor, fire::effects::parameterID(scope, 0, 0), 0.35f);
        REQUIRE(processor.addEqNode(2345, 7, fire::eq::Type::bell, scope, 5) == 3);
        REQUIRE(processor.assignLfoToTarget(0, fire::core_modules::eqParameterID(scope, 5, 3, fire::eq::Field::gain))
                == LfoManager::AssignmentResult::changed);
    }
    processor.stateAB.copyAB();
    juce::MemoryBlock host;
    processor.getStateInformation(host);
    FireAudioProcessor restored;
    restored.setStateInformation(host.getData(), static_cast<int>(host.getSize()));
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(processor, preset);
    FireAudioProcessor presetRestored;
    REQUIRE(state::loadStateFromXml(preset, presetRestored));
    for (auto* result : {&restored, &presetRestored})
        for (int scope : {0, 1})
        {
            CHECK_FALSE(result->isModulePresent(scope, 0));
            for (int i = 0; i < 6; ++i)
                CHECK(result->getInsertEffectType(scope, i) == static_cast<Type>(static_cast<int>(Type::drive) + i));
            CHECK(result->getEqNodeState(3, scope, 5).frequency == Catch::Approx(2345));
            CHECK(result->getEqNodeState(3, scope, 5).gainDb == Catch::Approx(7));
            CHECK(result->getModulationInfoForParameter(fire::core_modules::eqParameterID(scope, 5, 3, fire::eq::Field::gain)).isModulated);
        }
    processor.removeModule(0, 5);
    processor.stateAB.toggleAB();
    CHECK(processor.getInsertEffectType(0, 0) == Type::drive);
    CHECK_FALSE(processor.isModulePresent(0, 0));
}

TEST_CASE("Old projects restore default rows and partial new module families are rejected",
          "[shared-modules][state][compatibility][validation]")
{
    FireAudioProcessor source;
    juce::MemoryBlock host; source.getStateInformation(host);
    auto xml = juce::AudioProcessor::getXmlFromBinary(host.getData(), static_cast<int>(host.getSize()));
    REQUIRE(xml != nullptr);
    auto* parameters = xml->getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    for (int i = parameters->getNumChildElements() - 1; i >= 0; --i)
        if (fire::core_modules::isParameterID(parameters->getChildElement(i)->getStringAttribute("id")))
            parameters->removeChildElement(parameters->getChildElement(i), true);
    xml->removeAttribute("coreModulesSchemaVersion");
    xml->setAttribute("savedParameterCount", parameters->getNumChildElements());
    juce::AudioProcessor::copyXmlToBinary(*xml, host);
    FireAudioProcessor restored; restored.removeModule(1, 0);
    restored.setStateInformation(host.getData(), static_cast<int>(host.getSize()));
    CHECK(restored.isModulePresent(1, 0));
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(source, preset);
    preset.removeAttribute(fire::core_modules::presenceID(0, 0));
    CHECK_FALSE(state::loadStateFromXml(preset, restored));
    CHECK(restored.isModulePresent(0, 0));
}

TEST_CASE("Deleting every sidebar row leaves an empty chain and each module is readdable",
          "[shared-modules][ui][chain]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int scope : {0, 1})
    {
        FireAudioProcessor processor;
        const std::function<void(ModulatableSlider*)> callback;
        std::unique_ptr<juce::Component> panel;
        if (scope == 0) panel = std::make_unique<GlobalPanel>(processor, callback, callback, callback, callback, callback);
        else panel = std::make_unique<BandPanel>(processor, callback, callback, callback, callback, callback);
        show(*panel);
        auto* rack = find<fire::ui::EffectRackNavigation>(*panel, [](auto&) {return true;});
        REQUIRE(rack != nullptr);
        for (int node = 0; node < (scope == 0 ? 3 : 5); ++node)
        {
            auto* remove = find<CloseButton>(*rack, [&](auto& button) {return button.getComponentID() == fire::core_modules::presenceID(scope, node) + "Remove";});
            REQUIRE(remove != nullptr);
            remove->setPresented(true, false);
            remove->triggerClick();
            CHECK_FALSE(processor.isModulePresent(scope, node));
        }
        if (scope == 0) dynamic_cast<GlobalPanel*>(panel.get())->animationTick(0.02f);
        else dynamic_cast<BandPanel*>(panel.get())->animationTick(0.02f);
        for (auto* slider : dynamic_cast<PanelBase*>(panel.get())->getModulatableSliders())
            if (slider->isShowing()) CHECK((slider->getParamID() == (scope == 0 ? "output" : "output1")
                                          || slider->getParamID() == (scope == 0 ? "mix" : "mix1")));
        rack->createAddMenuResultHandler()(fire::ui::EffectRackNavigation::builtinMenuItemID(0));
        CHECK(processor.isModulePresent(scope, 0));
        auto* active = find<juce::Button>(*rack, [](auto& button) {return button.getToggleState() && button.isShowing();});
        REQUIRE(active != nullptr);
    }
}

TEST_CASE("Every shared core DSP processes finite stereo audio with active controls",
          "[shared-modules][dsp][insertfx]")
{
    for (auto type : {Type::drive, Type::shape, Type::compressor, Type::ott, Type::stereo, Type::eq})
        for (double rate : {44100.0, 96000.0})
        {
            CAPTURE(static_cast<int>(type), rate);
            fire::effects::InsertEffect effect;
            effect.prepare({rate, 128, 2});
            fire::effects::InsertEffect::Parameters parameters(type);
            if (type == Type::drive) {parameters.values[0].baseValue = 45; parameters.values[3].baseValue = 0;}
            if (type == Type::shape) parameters.values[1].baseValue = 0.25f;
            if (type == Type::compressor) {parameters.values[0].baseValue = -30; parameters.values[1].baseValue = 8;}
            if (type == Type::stereo) parameters.values[0].baseValue = 100;
            if (type == Type::eq)
            {
                auto& node = parameters.eq[0]; node.state.present = true; node.state.type = fire::eq::Type::bell;
                node.controls[0].value = 1500; node.controls[1].value = 12; node.controls[2].value = 1;
            }
            juce::AudioBuffer<float> buffer(2, 128);
            float difference = 0;
            for (int block = 0; block < 64; ++block)
            {
                juce::AudioBuffer<float> dry(2, 128);
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < 128; ++sample)
                        dry.setSample(channel, sample, 0.15f * std::sin(static_cast<float>(block * 128 + sample) * 0.19f + static_cast<float>(channel)));
                buffer.makeCopyOf(dry);
                auto audio = juce::dsp::AudioBlock<float>(buffer);
                effect.process(audio, parameters);
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < 128; ++sample)
                    {
                        REQUIRE(std::isfinite(buffer.getSample(channel, sample)));
                        if (block > 32) difference += std::abs(buffer.getSample(channel, sample) - dry.getSample(channel, sample));
                    }
            }
            CHECK(difference > 0.1f);
        }
}

TEST_CASE("Core module controls bind to independent slots in Band and Master",
          "[shared-modules][ui][eq][binding]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int scope : {0, 1})
    {
        FireAudioProcessor processor;
        const std::function<void(ModulatableSlider*)> callback;
        std::unique_ptr<juce::Component> panel;
        if (scope == 0) panel = std::make_unique<GlobalPanel>(processor, callback, callback, callback, callback, callback);
        else panel = std::make_unique<BandPanel>(processor, callback, callback, callback, callback, callback);
        show(*panel);
        auto* rack = find<fire::ui::EffectRackNavigation>(*panel, [](auto&) {return true;});
        REQUIRE(rack != nullptr);
        for (auto type : {Type::drive, Type::shape, Type::compressor, Type::ott, Type::stereo, Type::eq, Type::lofi})
        {
            const auto originalMasterGain = processor.getEqNodeState(1).gainDb;
            CAPTURE(scope, static_cast<int>(type));
            const int slot = processor.addInsertEffect(scope, type);
            REQUIRE(slot >= 0);
            rack->refresh();
            auto* row = find<juce::Button>(*rack, [&](auto& button)
                {return button.getComponentID() == fire::effects::parameterID(scope, slot, fire::effects::typeField);});
            REQUIRE(row != nullptr);
            row->triggerClick();
            if (type == Type::eq)
            {
                auto* controls = find<EqControlsPanel>(*panel, [](auto& view) {return view.isShowing();});
                REQUIRE(controls != nullptr);
                controls->selectNode(1);
                auto* gain = find<ModulatableSlider>(*panel, [&](auto& slider)
                    {return slider.isShowing() && slider.getParamID() == fire::core_modules::eqParameterID(scope, slot, 1, fire::eq::Field::gain);});
                REQUIRE(gain != nullptr);
                gain->setValue(6, juce::sendNotificationSync);
                CHECK(processor.getEqNodeState(1, scope, slot).gainDb == Catch::Approx(6));
                CHECK(processor.getEqNodeState(1).gainDb == originalMasterGain);
            }
            if (type == Type::shape)
            {
                auto* mode = find<ContextAwareComboBox>(*panel, [](auto& menu) {return menu.getTitle() == "Shape mode" && menu.isShowing();});
                REQUIRE(mode != nullptr);
                CHECK(mode->getText() == "Cubic");
            }
            if (type == Type::drive)
            {
                auto* safe = find<juce::Button>(*panel, [&](auto& button)
                    {return button.isShowing() && button.getComponentID() == fire::effects::parameterID(scope, slot, 1);});
                REQUIRE(safe != nullptr);
                safe->triggerClick();
                CHECK(processor.treeState.getRawParameterValue(fire::effects::parameterID(scope, slot, 1))->load() == Catch::Approx(0));
            }
            if (type == Type::lofi)
            {
                auto* jitter = find<ModulatableSlider>(*panel, [&](auto& slider)
                    {return slider.isShowing() && slider.getParamID() == fire::core_modules::parameterID(scope, slot, fire::core_modules::jitterField);});
                REQUIRE(jitter != nullptr);
                jitter->setValue(0.4, juce::sendNotificationSync);
                CHECK(processor.treeState.getRawParameterValue(jitter->getParamID())->load() == Catch::Approx(0.4));
            }
            for (auto* slider : dynamic_cast<PanelBase*>(panel.get())->getModulatableSliders())
                if (slider->isShowing())
                    CHECK(panel->getLocalBounds().contains(panel->getLocalArea(slider, slider->getLocalBounds())));
        }
    }
}

TEST_CASE("Band insertion and removal move core EQ instances deleted rows and their modulation together",
          "[shared-modules][state][topology][eq]")
{
    FireAudioProcessor processor;
    set(processor, NUM_BANDS_ID, 2); set(processor, "lineState1", 1); set(processor, "freq1", 800);
    REQUIRE(processor.addInsertEffect(1, Type::eq) == 0);
    REQUIRE(processor.addInsertEffect(2, Type::drive) == 0);
    processor.removeModule(1, 0);
    const auto eqGain = [](int scope) {return fire::core_modules::eqParameterID(scope, 0, 1, fire::eq::Field::gain);};
    set(processor, eqGain(1), 8.25f);
    REQUIRE(processor.assignLfoToTarget(fire::mod_sources::firstMacro, eqGain(1)) == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.addMultibandBand(0, 2, true, 210));
    CHECK(processor.getInsertEffectType(1, 0) == Type::none);
    CHECK(processor.getInsertEffectType(2, 0) == Type::eq);
    CHECK(processor.getInsertEffectType(3, 0) == Type::drive);
    CHECK_FALSE(processor.isModulePresent(2, 0));
    CHECK(processor.getEqNodeState(1, 2, 0).gainDb == Catch::Approx(8.25f));
    CHECK(processor.getModulationInfoForParameter(eqGain(2)).isModulated);
    REQUIRE(processor.deleteMultibandBand(0, 3));
    CHECK(processor.getInsertEffectType(1, 0) == Type::eq);
    CHECK(processor.getInsertEffectType(2, 0) == Type::drive);
    CHECK_FALSE(processor.isModulePresent(1, 0));
    CHECK(processor.getEqNodeState(1, 1, 0).gainDb == Catch::Approx(8.25f));
    CHECK(processor.getModulationInfoForParameter(eqGain(1)).isModulated);
}
