#include <PluginProcessor.h>
#include <GUI/SettingsComponent.h>
#include <Utility/BuildDiagnostics.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Support diagnostics identify the loaded build and current audio configuration", "[diagnostics][settings]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.setRateAndBufferSizeDetails(96000.0, 64);
    processor.setLatencySamples(83);
    const auto report = fire::diagnostics::report(processor, true);
    CHECK(report.contains("Fire " + juce::String(VERSION)));
    CHECK(report.contains("Build: " + fire::diagnostics::buildIdentifier()));
    CHECK(report.contains("Architecture: " + fire::diagnostics::architecture()));
    CHECK(report.contains("Sample rate: 96000 Hz"));
    CHECK(report.contains("Block size: 64 samples"));
    CHECK(report.contains("Latency: 83 samples"));
    CHECK(report.contains("HQ: On (4x Shape oversampling)"));
    CHECK_FALSE(report.contains(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()));
    CHECK(fire::diagnostics::report(processor, false).contains("HQ: Off"));
}

TEST_CASE("Settings expose a readable selectable diagnostic report and local copy control", "[ui][diagnostics][settings][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::PropertiesFile::Options options;
    options.applicationName = "FireDiagnosticsTests";
    options.filenameSuffix = ".settings";
    options.doNotSave = true;
    options.millisecondsBeforeSaving = -1;
    juce::PropertiesFile settings(juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("FireDiagnostics-" + juce::Uuid().toString() + ".settings"), options);
    SettingsComponent panel(settings, [] { return "Build: fixture\nFormat: AU\nHQ: On"; });
    auto* text = dynamic_cast<juce::TextEditor*>(panel.findChildWithID("buildDiagnostics"));
    auto* button = dynamic_cast<juce::Button*>(panel.findChildWithID("copyDiagnostics"));
    REQUIRE(text != nullptr);
    REQUIRE(button != nullptr);
    CHECK(text->isReadOnly());
    CHECK(text->getText().contains("Format: AU"));
    panel.setSize(SettingsComponent::minimumContentWidth, SettingsComponent::minimumContentHeight);
    CHECK(panel.getLocalBounds().contains(text->getBounds()));
    CHECK(panel.getLocalBounds().contains(button->getBounds()));
    CHECK(text->getHeight() >= 64);
    CHECK_FALSE(text->getBounds().intersects(button->getBounds()));
}
