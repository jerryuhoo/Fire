#include <catch2/catch_test_macros.hpp>
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <cmath>

// Compare two audio buffers for equality
void requireBuffersAreEquivalent(const juce::AudioBuffer<float>& result, const juce::AudioBuffer<float>& expected)
{
    REQUIRE(result.getNumChannels() == expected.getNumChannels());
    REQUIRE(result.getNumSamples() == expected.getNumSamples());

    const float tolerance = 1e-6f;

    for (int channel = 0; channel < result.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < result.getNumSamples(); ++sample)
        {
            float resultSample = result.getSample(channel, sample);
            float expectedSample = expected.getSample(channel, sample);

            if (std::abs(resultSample - expectedSample) > tolerance)
            {
                FAIL("Sample mismatch at channel " << channel << ", sample " << sample 
                     << ". Expected: " << expectedSample << ", Got: " << resultSample);
            }
        }
    }
    SUCCEED("Buffers are equivalent within tolerance.");
}

// Regression Test: Verify output against Golden Masters
TEST_CASE("Regression Test: Verify output against Golden Masters")
{
    FireAudioProcessor processor;
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const double standardSampleRate = 48000.0;
    const int standardBlockSize = 512; // A typical block size is fine here.
    
    // Call prepareToPlay() immediately after creating the processor instance.
    // This ensures that getSampleRate() will always return a valid value from now on.
    processor.prepareToPlay(standardSampleRate, standardBlockSize);

    // Load the input audio file
    juce::File inputFile { "/Users/yyf/Documents/GitHub/Fire/tests/TestAudioFiles/drum.wav" };
    REQUIRE(inputFile.existsAsFile());
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(inputFile));
    REQUIRE(reader != nullptr);
    juce::AudioBuffer<float> buffer(reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);

    // Load the preset files
    juce::File presetsDir { "/Users/yyf/Library/Audio/Presets/Wings/Fire/User" };
    REQUIRE(presetsDir.isDirectory());
    const juce::String filePattern = "*.fire";

    bool atLeastOnePresetTested = false;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, filePattern))
    {
        atLeastOnePresetTested = true;
        auto presetFile = entry.getFile();
        juce::String presetName = presetFile.getFileNameWithoutExtension();

        SECTION("Preset: " + presetName.toStdString())
        {
            auto bufferToProcess = buffer;

            // Load the preset parameters from the XML file
            std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(presetFile);
            REQUIRE(xml != nullptr);
            for (int i = 0; i < xml->getNumAttributes(); ++i)
            {
                auto attributeName = xml->getAttributeName(i);
                auto attributeValue = xml->getAttributeValue(i);
                if (auto* parameter = processor.treeState.getParameter(attributeName))
                    parameter->setValueNotifyingHost(attributeValue.getFloatValue());
            }

            // Set up the MIDI buffer and process the audio
            juce::MidiBuffer midi;
            processor.prepareToPlay(reader->sampleRate, bufferToProcess.getNumSamples());
            processor.processBlock(bufferToProcess, midi);

            // Create the output directory if it doesn't exist
            juce::File regressionOutputDir { "/Users/yyf/Documents/GitHub/Fire/tests/RegressionOutput/" };
            if (!regressionOutputDir.isDirectory()) {
                REQUIRE(regressionOutputDir.createDirectory().wasOk());
            }
            juce::File regressionOutputFile = regressionOutputDir.getChildFile(presetName + "_drums_output.wav");
            regressionOutputFile.deleteFile();

            std::unique_ptr<juce::AudioFormatWriter> writer(
                formatManager.findFormatForFileExtension("wav")->createWriterFor(
                    new juce::FileOutputStream(regressionOutputFile),
                    reader->sampleRate, bufferToProcess.getNumChannels(), 24, {}, 0
                )
            );
            REQUIRE(writer != nullptr);
            bool writeOk = writer->writeFromAudioSampleBuffer(bufferToProcess, 0, bufferToProcess.getNumSamples());
            REQUIRE(writeOk);
            // ===========================================

            // Load the golden master file
            juce::File goldenFile { "/Users/yyf/Documents/GitHub/Fire/tests/GoldenMasters/" + presetName + "_drums_output.wav" };
            REQUIRE(goldenFile.existsAsFile());
            std::unique_ptr<juce::AudioFormatReader> goldenReader(formatManager.createReaderFor(goldenFile));
            REQUIRE(goldenReader != nullptr);
            juce::AudioBuffer<float> goldenBuffer(goldenReader->numChannels, (int)goldenReader->lengthInSamples);
            goldenReader->read(&goldenBuffer, 0, (int)goldenReader->lengthInSamples, 0, true, true);

            // Compare the processed buffer with the golden buffer
            requireBuffersAreEquivalent(bufferToProcess, goldenBuffer);
        }
    }
    
    REQUIRE(atLeastOnePresetTested);
}