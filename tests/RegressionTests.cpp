#include <catch2/catch_test_macros.hpp>
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <cmath>

// The comparison function itself is correct.
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
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const double standardSampleRate = 48000.0;
    const int standardBlockSize = 512;
    
    // Load the input audio file once, as it's the same for all tests.
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
            // +++ CREATE A FRESH PROCESSOR FOR EACH PRESET SECTION +++
            // This is the critical fix. It ensures each test run is independent
            // and not affected by the state of the processor from a previous run.
            FireAudioProcessor processor;
            processor.prepareToPlay(standardSampleRate, standardBlockSize);

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

            // +++ CRITICAL FIX: WRITE-THEN-READ TO SIMULATE FILE I/O PRECISION LOSS +++
            // To ensure an apples-to-apples comparison, we write the in-memory buffer 
            // to a temporary file and read it back. This ensures both buffers being
            // compared have undergone the same float -> int24 -> float conversion.

            // 1. Write the freshly processed buffer to a temporary file.
            juce::File tempOutputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                            .getChildFile("temp_regression_output.wav");
            tempOutputFile.deleteFile();
            
            std::unique_ptr<juce::AudioFormatWriter> tempWriter(
                formatManager.findFormatForFileExtension("wav")->createWriterFor(
                    new juce::FileOutputStream(tempOutputFile),
                    reader->sampleRate, bufferToProcess.getNumChannels(), 24, {}, 0
                )
            );
            REQUIRE(tempWriter != nullptr);
            tempWriter->writeFromAudioSampleBuffer(bufferToProcess, 0, bufferToProcess.getNumSamples());
            tempWriter.reset(); // This flushes and closes the writer.

            // 2. Read the temporary file back into a new buffer.
            std::unique_ptr<juce::AudioFormatReader> tempReader(formatManager.createReaderFor(tempOutputFile));
            REQUIRE(tempReader != nullptr);
            juce::AudioBuffer<float> reloadedResultBuffer(tempReader->numChannels, (int)tempReader->lengthInSamples);
            tempReader->read(&reloadedResultBuffer, 0, (int)tempReader->lengthInSamples, 0, true, true);
            tempOutputFile.deleteFile(); // Clean up the temporary file.

            // 3. Load the golden master file (which has also undergone the same I/O process).
            juce::File goldenFile { "/Users/yyf/Documents/GitHub/Fire/tests/GoldenMasters/" + presetName + "_drums_output.wav" };
            REQUIRE(goldenFile.existsAsFile());
            std::unique_ptr<juce::AudioFormatReader> goldenReader(formatManager.createReaderFor(goldenFile));
            REQUIRE(goldenReader != nullptr);
            juce::AudioBuffer<float> goldenBuffer(goldenReader->numChannels, (int)goldenReader->lengthInSamples);
            goldenReader->read(&goldenBuffer, 0, (int)goldenReader->lengthInSamples, 0, true, true);

            // 4. Now, compare the two buffers that have identical origins. This should pass.
            requireBuffersAreEquivalent(reloadedResultBuffer, goldenBuffer);
        }
    }
    
    REQUIRE(atLeastOnePresetTested);
}
