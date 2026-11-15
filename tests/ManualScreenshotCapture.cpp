#include <PluginEditor.h>
#include <GUI/DynamicEqControls.h>
#include <GUI/EffectRackNavigation.h>
#include <GUI/GranularMediaControls.h>
#include <GUI/LfoPlaybackControls.h>
#include <GUI/MidiControls.h>
#include <GUI/ModulationSourceControls.h>
#include <GUI/PresetLockControls.h>
#include <GUI/PresetMetadataControls.h>
#include <GUI/SettingsComponent.h>
#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <Utility/BuildDiagnostics.h>
#include <Utility/CloudsParameters.h>
#include <Utility/DynamicEqParameters.h>
#include <Utility/ReverbModelParameters.h>
#include <catch2/catch_test_macros.hpp>

// Opt-in documentation asset generator. Every figure is painted by the real
// product component; meters and motion receive actual processed audio.
namespace
{
namespace ui = fire::ui;
namespace fx = fire::effects;

template<class T, class Predicate>
T* findManual(juce::Component& root, Predicate predicate)
{
    if (auto* item = dynamic_cast<T*>(&root); item && predicate(*item)) return item;
    for (auto* child : root.getChildren())
        if (auto* item = findManual<T>(*child, predicate)) return item;
    return nullptr;
}
template<class T> T& manualComponent(juce::Component& root)
{
    auto* result = findManual<T>(root, [](auto&) { return true; });
    REQUIRE(result != nullptr);
    return *result;
}
template<class T> T& manualVisibleComponent(juce::Component& root)
{
    auto* result = findManual<T>(root, [](auto& item)
        { return item.isShowing() && !item.getLocalBounds().isEmpty(); });
    REQUIRE(result != nullptr);
    return *result;
}
void manualSet(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void manualClick(juce::Component& root, const juce::String& name)
{
    auto* button = findManual<juce::Button>(root, [&](auto& b)
        { return b.isShowing() && (b.getButtonText() == name || b.getComponentID() == name); });
    INFO(name);
    REQUIRE(button != nullptr);
    button->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
}
struct Capture
{
    juce::File directory;
    FireAudioProcessor processor;
    juce::File library = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("FireManualLibrary-" + juce::Uuid().toString());
    juce::String savedSkin;
    bool hadSkin = false;
    std::unique_ptr<FireAudioProcessorEditor> editor;
    std::uint64_t sample = 0;

    explicit Capture(juce::File target) : directory(std::move(target))
    {
        REQUIRE(directory.createDirectory().wasOk());
        REQUIRE(library.createDirectory().wasOk());
        processor.hasUpdateCheckBeenPerformed = true;
        processor.statePresets.setPresetDirectoryForTesting(library);
        processor.statePresets.enableFactoryPresets();
        hadSkin = processor.getAppSettings().containsKey(ui::skinSetting);
        savedSkin = processor.getAppSettings().getValue(ui::skinSetting);
        manualSet(processor, NUM_BANDS_ID, 3);
        manualSet(processor, "lineState1", 1);
        manualSet(processor, "lineState2", 1);
        manualSet(processor, "freq1", 250);
        manualSet(processor, "freq2", 2600);
        manualSet(processor, "drive1", 24);
        manualSet(processor, "drive2", 32);
        manualSet(processor, "drive3", 18);
        manualSet(processor, "shapeBypass1", 1);
        manualSet(processor, "shapeBypass2", 1);
        manualSet(processor, "shapeBypass3", 1);
        processor.statePresets.setPresetName("Manual / Three-band colour");
        processor.setRateAndBufferSizeDetails(48000, 256);
        processor.prepareToPlay(48000, 256);
        editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setSkinPreference(ui::Skin::ink);
        editor->setSize(1600, 800);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        audio(90);
    }
    ~Capture()
    {
        editor.reset();
        processor.releaseResources();
        if (hadSkin) processor.getAppSettings().setValue(ui::skinSetting, savedSkin);
        else processor.getAppSettings().removeValue(ui::skinSetting);
        processor.getAppSettings().saveIfNeeded();
        library.deleteRecursively();
    }
    void audio(int frames = 40)
    {
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        for (int frame = 0; frame < frames; ++frame)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i, ++sample)
            {
                const double t = static_cast<double>(sample) / 48000.0;
                const double envelope = .35 + .65 * (.5 + .5 * std::sin(t * 4.7));
                const float left = static_cast<float>(envelope * (.11 * std::sin(t * 2 * juce::MathConstants<double>::pi * 110)
                    + .055 * std::sin(t * 2 * juce::MathConstants<double>::pi * 440)
                    + .025 * std::sin(t * 2 * juce::MathConstants<double>::pi * 3300)));
                const float right = left * .76f + static_cast<float>(.02 * std::sin(t * 2 * juce::MathConstants<double>::pi * 660));
                buffer.setSample(0, i, left); buffer.setSample(1, i, right);
            }
            processor.processBlock(buffer, midi);
            editor->timerCallback();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(4);
        }
    }
    void save(juce::Component& component, const juce::String& name, float scale = 1)
    {
        INFO(name);
        INFO(component.getBounds().toString());
        REQUIRE(!component.getLocalBounds().isEmpty());
        auto stream = directory.getChildFile(name + ".png").createOutputStream();
        REQUIRE(stream != nullptr); stream->setPosition(0); stream->truncate();
        REQUIRE(juce::PNGImageFormat().writeImageToStream(
            component.createComponentSnapshot(component.getLocalBounds(), true, scale), *stream));
    }
    void save(const juce::String& name) { audio(); save(*editor, name); }
    template<class T> void detail(T& control, const juce::String& name, float scale = 2)
    {
        ui::setSkin(control, ui::Skin::ink); control.sendLookAndFeelChange();
        control.addToDesktop(juce::ComponentPeer::windowIsTemporary); control.setVisible(true);
        audio(12); save(control, name, scale); control.setVisible(false); control.removeFromDesktop();
    }
    void insert(int scope, int slot)
    {
        manualClick(*editor, scope == 0 ? "MASTER LAB" : "BAND LAB");
        if (scope == 0) manualComponent<GlobalPanel>(*editor).focusInsertEffect(slot);
        else
        {
            auto& band = manualComponent<BandPanel>(*editor); band.setFocusBandNum(scope - 1);
            auto& rack = manualComponent<ui::EffectRackNavigation>(band);
            REQUIRE(rack.onSelectEffect); rack.onSelectEffect(slot);
        }
    }
};
}

TEST_CASE("Capture English user manual figures from the current editor", "[.manual-screenshots]")
{
    const auto directory = juce::SystemStats::getEnvironmentVariable("FIRE_MANUAL_SCREENSHOT_DIR", {});
    if (directory.isEmpty()) SKIP("Set FIRE_MANUAL_SCREENSHOT_DIR to explicitly generate documentation assets");
    juce::ScopedJuceInitialiser_GUI gui;
    Capture c {juce::File(directory)};
    auto& p = c.processor;
    auto& band = manualComponent<BandPanel>(*c.editor);
    auto& master = manualComponent<GlobalPanel>(*c.editor);

    manualClick(*c.editor, "BAND LAB"); band.setFocusBandNum(1); manualClick(band, "Drive");
    c.save("01-overview-ink");
    c.editor->setSkinPreference(ui::Skin::paper); c.save("02-overview-paper");
    c.editor->setSkinPreference(ui::Skin::modern); c.save("03-overview-modern");
    c.editor->setSkinPreference(ui::Skin::vintage);
    band.setFocusBandNum(0); REQUIRE(p.setShapeMode(1, -1, 12)); manualClick(band, "Shape");
    manualSet(p, "drive1", 38); c.save("04-vintage-tube");
    c.audio(4);
    c.save(manualVisibleComponent<ui::HardwareColourPanel>(band), "04-tube-detail", 2);
    REQUIRE(p.setShapeMode(1, -1, 23)); c.save("05-vintage-tape");
    c.audio(4);
    c.save(manualVisibleComponent<ui::HardwareColourPanel>(band), "05-tape-detail", 2);
    c.editor->setSkinPreference(ui::Skin::ink); REQUIRE(p.setShapeMode(1, -1, 3));
    c.save("06-digital-shape");
    c.save(band, "06-shape-panel", 1.5f);

    manualClick(*c.editor, "MASTER LAB"); manualClick(master, "EQ"); master.selectEqNode(1);
    manualSet(p, FILTER_BYPASS_ID, 1);
    manualSet(p, fire::eq::parameterID(1, fire::eq::Field::frequency), 1800);
    manualSet(p, fire::eq::parameterID(1, fire::eq::Field::gain), 3);
    manualSet(p, fire::dynamic_eq::parameterID(0, -1, 1, fire::dynamic_eq::enabled), 1);
    manualSet(p, fire::dynamic_eq::parameterID(0, -1, 1, fire::dynamic_eq::threshold), -35);
    manualSet(p, fire::dynamic_eq::parameterID(0, -1, 1, fire::dynamic_eq::range), -6);
    c.save("07-master-eq");
    c.save(master, "07-eq-panel", 1.5f);
    { ui::DynamicEqControls panel(p, 1, -1, -1, [] { return true; }); c.detail(panel, "08-dynamic-eq"); }
    manualSet(p, FILTER_BYPASS_ID, 0);

    const int ott = p.addInsertEffect(0, fx::Type::ott); REQUIRE(ott >= 0);
    c.insert(0, ott); c.save("09-master-ott");
    c.audio(4);
    c.save(manualVisibleComponent<OttGraph>(master), "09-ott-detail", 2);
    c.audio(4);
    c.save(master, "09-ott-panel", 1.5f);
    const int reverb = p.addInsertEffect(0, fx::Type::reverb); REQUIRE(reverb >= 0);
    manualSet(p, fire::reverb_params::parameterID(0, reverb), 2); // Hall
    c.insert(0, reverb); c.save("10-reverb");
    c.save(master, "10-reverb-panel", 1.5f);
    const int granular = p.addInsertEffect(1, fx::Type::granular); REQUIRE(granular >= 0);
    c.insert(1, granular); c.audio(260);
    manualSet(p, fire::clouds_params::parameterID(1, granular, fire::clouds_params::freezeField), 1);
    c.audio(10); c.save("11-granular");
    c.save(band, "11-granular-panel", 1.5f);
    {
        auto target = p.getGranularAudioTarget(1, granular); REQUIRE(target);
        ui::GranularMediaControls panel(p, *target, [] { return true; });
        c.detail(panel, "12-granular-sample");
    }

    REQUIRE(p.setChainRoutingMode(1, fire::parallel_chain::parallel));
    REQUIRE(p.setModuleBranch(1, 1, fire::parallel_chain::b));
    auto& rack = manualComponent<ui::EffectRackNavigation>(band);
    // Drain topology/editor refresh before opening: a stale refresh correctly
    // dismisses transient controls, but would erase this documentation popup.
    c.audio();
    rack.showRouting();
    c.save(*c.editor, "13-parallel-routing");
    c.save(manualVisibleComponent<ui::ChainRoutingControls>(*c.editor), "13-routing-detail", 2);
    rack.dismiss();
    REQUIRE(p.setChainRoutingMode(1, fire::parallel_chain::serial));

    LfoData triangle;
    triangle.points = {{0, 0}, {.5f, 1}, {1, 0}};
    triangle.curvatures = {0, 0};
    p.getLfoManager().setLfoData(0, triangle);
    {
        auto& manager = p.getLfoManager();
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routes = manager.getModulationRoutings(); routes.clear();
        routes.add({0, "drive1", .35f, true, false});
        routes.add({fire::mod_sources::envelope, "drive1", .18f, false, false});
        routes.add({fire::mod_sources::firstMacro, "drive2", .45f, false, false});
        manager.advanceModulationRoutingRevisionLocked();
    }
    p.lfoDataHasChanged(); manualClick(*c.editor, "MOD FORGE"); c.save("14-mod-forge");
    c.save(manualComponent<LfoPanel>(*c.editor), "14-mod-forge-panel", 1.5f);
    {
        ModulationMatrixPanel matrix(p); matrix.setSize(1000, 530);
        c.detail(matrix, "15-modulation-matrix", 1.6f);
    }
    {
        manualSet(p, fire::envelope_params::parameterID(1, fire::envelope_params::source), fire::envelope_params::externalInput);
        manualSet(p, fire::envelope_params::parameterID(1, fire::envelope_params::highPassEnabled), 1);
        manualSet(p, fire::envelope_params::parameterID(1, fire::envelope_params::highPassFrequency), 180);
        c.audio(200); // Let the previous main-input envelope release naturally.
        ui::ModulationSourceControls panel(p); panel.selectEnvelope(1); c.detail(panel, "16-envelopes-macros");
    }
    {
        manualSet(p, fire::lfo_playback::parameterID(0, fire::lfo_playback::playMode), fire::lfo_playback::oneShot);
        manualSet(p, fire::lfo_playback::parameterID(0, fire::lfo_playback::waveMode), fire::lfo_playback::randomGlide);
        manualSet(p, fire::lfo_playback::parameterID(0, fire::lfo_playback::seed), 2026);
        ui::LfoPlaybackControls panel(p, 0); c.detail(panel, "17-lfo-playback");
    }
    {
        manualSet(p, fire::midi_mod::parameterID(0, fire::midi_mod::number), 1);
        manualSet(p, fire::midi_mod::parameterID(1, fire::midi_mod::number), 74);
        ui::MidiControls panel(p); c.detail(panel, "18-midi-controls");
    }
    {
        ui::PresetBrowserPanel browser(p.statePresets); browser.setSize(1600, 800);
        ui::setSkin(browser, ui::Skin::ink); browser.sendLookAndFeelChange();
        browser.addToDesktop(juce::ComponentPeer::windowIsTemporary); browser.setVisible(true);
        browser.open(); browser.selectCategory("Analog Drive"); c.audio(10); c.save(browser, "19-preset-library");
    }
    {
        REQUIRE(p.setPresetParameterLocked("drive1", true)); REQUIRE(p.setPresetParameterLocked("drive2", true));
        ui::PresetLockControls panel(p);
        if (auto* filter = findManual<juce::TextEditor>(panel, [](auto& e) { return e.getComponentID() == "preset_lock_search"; })) filter->setText("Drive");
        c.detail(panel, "20-preset-locks");
    }
    {
        const auto entries = p.statePresets.getBrowserEntries(); REQUIRE(!entries.empty());
        const auto found = std::find_if(entries.begin(), entries.end(), [](const auto& e) { return e.category == "Analog Drive"; });
        REQUIRE(found != entries.end()); const auto entry = *found;
        ui::PresetMetadataControls panel(p.statePresets, entry.key, entry.name);
        c.detail(panel, "21-preset-details");
    }
    {
        SettingsComponent settings(p.getAppSettings(), [&] { return fire::diagnostics::report(p, false); });
        settings.setSize(540, 640); c.detail(settings, "22-settings");
    }
}
