// RegressionTests.cpp
#include <catch2/catch_test_macros.hpp>
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <cmath>

// Helper function to generate a sine wave buffer for testing purposes.
inline juce::AudioBuffer<float> createSineWaveBuffer(double sampleRate, int numChannels, int numSamples, float frequency)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    double currentAngle = 0.0;
    double angleDelta = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentSample = (float)std::sin(currentAngle);
        for (int channel = 0; channel < numChannels; ++channel)
        {
            buffer.setSample(channel, sample, currentSample);
        }
        currentAngle += angleDelta;
    }
    return buffer;
}

// Compares two audio buffers sample by sample to check for equivalence within a small tolerance.
void requireBuffersAreEquivalent(const juce::AudioBuffer<float>& result, const juce::AudioBuffer<float>& expected)
{
    REQUIRE(result.getNumChannels() == expected.getNumChannels());
    REQUIRE(result.getNumSamples() == expected.getNumSamples());

    const float tolerance = 1e-6f; // A small tolerance to account for floating-point inaccuracies.

    for (int channel = 0; channel < result.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < result.getNumSamples(); ++sample)
        {
            float resultSample = result.getSample(channel, sample);
            float expectedSample = expected.getSample(channel, sample);

            if (std::abs(resultSample - expectedSample) > tolerance)
            {
                // If a sample differs more than the tolerance, the test fails.
                FAIL("Sample mismatch at channel " << channel << ", sample " << sample 
                     << ". Expected: " << expectedSample << ", Got: " << resultSample);
            }
        }
    }
    // If all samples match, the test succeeds.
    SUCCEED("Buffers are equivalent within tolerance.");
}

// Regression Test: Verify output against Golden Masters using block-based processing.
TEST_CASE("Regression Test: Verify output against Golden Masters")
{
    // +++ CONTROL VARIABLE +++
    // Set this to 'true' to generate .wav files in the RegressionOutput folder for manual inspection.
    // Set to 'false' for standard automated testing where temporary files are deleted.
    constexpr bool keepRegressionOutputFiles = true;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const int standardBlockSize = 512;
    
    // Load the input audio file once, as it's the same for all tests.
    juce::File inputFile { "/Users/yyf/Documents/GitHub/Fire/tests/TestAudioFiles/drum.wav" };
    REQUIRE(inputFile.existsAsFile());
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(inputFile));
    REQUIRE(reader != nullptr);
    
    // This buffer holds the original, unprocessed input audio.
    juce::AudioBuffer<float> originalInputBuffer(reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&originalInputBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

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
            // Create a fresh processor instance for each preset to ensure test isolation.
            FireAudioProcessor processor;

            // Load the preset parameters from the XML file.
            std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(presetFile);
            REQUIRE(xml != nullptr);
            for (int i = 0; i < xml->getNumAttributes(); ++i)
            {
                auto attributeName = xml->getAttributeName(i);
                auto attributeValue = xml->getAttributeValue(i);
                if (auto* parameter = processor.treeState.getParameter(attributeName))
                    parameter->setValueNotifyingHost(attributeValue.getFloatValue());
            }

            // --- START: MODIFIED BLOCK-BASED PROCESSING ---
            // This section now mimics the processing loop from GenerateGoldenMasters.cpp.
            juce::MidiBuffer midi;
            
            // Prepare the processor with the correct sample rate and block size.
            processor.prepareToPlay(reader->sampleRate, standardBlockSize);

            // Create an empty buffer to assemble the final processed output.
            juce::AudioBuffer<float> finalOutputBuffer(originalInputBuffer.getNumChannels(), originalInputBuffer.getNumSamples());
            finalOutputBuffer.clear();

            // Iterate through the input audio in blocks of 'standardBlockSize'.
            for (int startSample = 0; startSample < originalInputBuffer.getNumSamples(); startSample += standardBlockSize)
            {
                // Calculate the size of the current block (handles the last, possibly smaller, block).
                int numSamplesThisBlock = std::min(standardBlockSize, originalInputBuffer.getNumSamples() - startSample);

                // Create a temporary buffer for the current block.
                juce::AudioBuffer<float> blockToProcess(originalInputBuffer.getNumChannels(), numSamplesThisBlock);

                // Copy the slice of audio from the original input into the processing block.
                for (int channel = 0; channel < originalInputBuffer.getNumChannels(); ++channel)
                {
                    blockToProcess.copyFrom(channel, 0, originalInputBuffer, channel, startSample, numSamplesThisBlock);
                }

                // Process the current block.
                processor.processBlock(blockToProcess, midi);

                // Copy the processed audio from the block into the correct position in the final output buffer.
                for (int channel = 0; channel < finalOutputBuffer.getNumChannels(); ++channel)
                {
                    finalOutputBuffer.copyFrom(channel, startSample, blockToProcess, channel, 0, numSamplesThisBlock);
                }
            }
            // --- END: MODIFIED BLOCK-BASED PROCESSING ---


            // The following logic writes the final processed buffer and compares it to the golden master.
            
            juce::File fileForComparison;
            if (keepRegressionOutputFiles)
            {
                // If debugging, write to a persistent regression output directory.
                juce::File regressionOutputDir { "/Users/yyf/Documents/GitHub/Fire/tests/RegressionOutput/" };
                if (!regressionOutputDir.isDirectory())
                    REQUIRE(regressionOutputDir.createDirectory().wasOk());
                fileForComparison = regressionOutputDir.getChildFile(presetName + "_drums_output.wav");
            }
            else
            {
                // For automated tests, use a temporary file.
                fileForComparison = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                                .getChildFile("temp_regression_output.wav");
            }

            fileForComparison.deleteFile();
            
            // 1. Write the assembled output buffer to a .wav file.
            std::unique_ptr<juce::AudioFormatWriter> writer(
                formatManager.findFormatForFileExtension("wav")->createWriterFor(
                    new juce::FileOutputStream(fileForComparison),
                    reader->sampleRate, finalOutputBuffer.getNumChannels(), 24, {}, 0
                )
            );
            REQUIRE(writer != nullptr);
            writer->writeFromAudioSampleBuffer(finalOutputBuffer, 0, finalOutputBuffer.getNumSamples());
            writer.reset(); // This flushes the writer and closes the file stream.

            // 2. Read the newly written file back into a separate buffer to account for file I/O precision changes.
            std::unique_ptr<juce::AudioFormatReader> resultReader(formatManager.createReaderFor(fileForComparison));
            REQUIRE(resultReader != nullptr);
            juce::AudioBuffer<float> reloadedResultBuffer(resultReader->numChannels, (int)resultReader->lengthInSamples);
            resultReader->read(&reloadedResultBuffer, 0, (int)resultReader->lengthInSamples, 0, true, true);

            if (!keepRegressionOutputFiles)
            {
                // Clean up the temporary file if not in debug mode.
                fileForComparison.deleteFile();
            }

            // 3. Load the corresponding golden master file.
            juce::File goldenFile { "/Users/yyf/Documents/GitHub/Fire/tests/GoldenMasters/" + presetName + "_drums_output.wav" };
            REQUIRE(goldenFile.existsAsFile());
            std::unique_ptr<juce::AudioFormatReader> goldenReader(formatManager.createReaderFor(goldenFile));
            REQUIRE(goldenReader != nullptr);
            juce::AudioBuffer<float> goldenBuffer(goldenReader->numChannels, (int)goldenReader->lengthInSamples);
            goldenReader->read(&goldenBuffer, 0, (int)goldenReader->lengthInSamples, 0, true, true);

            // 4. Compare the reloaded result buffer against the golden master buffer.
            requireBuffersAreEquivalent(reloadedResultBuffer, goldenBuffer);
        }
    }
    
    REQUIRE(atLeastOnePresetTested);
}

// NOTE: This test case processes a single block of a sine wave and remains unchanged
// as its original logic is already consistent with single-block processing.
TEST_CASE("Regression Test (Sine Wave)")
{
    // +++ CONTROL VARIABLE +++
    constexpr bool keepRegressionOutputFiles = true;

    // --- Setup ---
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // --- Test Signal Parameters ---
    const double sampleRate = 48000.0;
    const int    blockSize = 512;
    const int    numChannels = 2;
    const float  frequency = 440.0f;

    // --- 1. Generate Input Audio Signal ---
    auto originalBuffer = createSineWaveBuffer(sampleRate, numChannels, blockSize, frequency);

    // --- 2. Iterate Through Preset Files ---
    juce::File presetsDir { "/Users/yyf/Library/Audio/Presets/Wings/Fire/User" };
    REQUIRE(presetsDir.isDirectory());
    const juce::String filePattern = "*.fire";
    bool atLeastOnePresetTested = false;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, filePattern))
    {
        atLeastOnePresetTested = true;
        auto presetFile = entry.getFile();
        juce::String presetName = presetFile.getFileNameWithoutExtension();

        SECTION("Preset (Sine Wave): " + presetName.toStdString())
        {
            FireAudioProcessor processor;
            auto bufferToProcess = originalBuffer;

            // Load the preset parameters
            std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(presetFile);
            REQUIRE(xml != nullptr);
            for (int i = 0; i < xml->getNumAttributes(); ++i)
            {
                auto attributeName = xml->getAttributeName(i);
                auto attributeValue = xml->getAttributeValue(i);
                if (auto* parameter = processor.treeState.getParameter(attributeName))
                    parameter->setValueNotifyingHost(attributeValue.getFloatValue());
            }

            // Process the audio block
            juce::MidiBuffer midi;
            processor.prepareToPlay(sampleRate, blockSize);
            processor.processBlock(bufferToProcess, midi);

            // --- Define output file path based on the control variable ---
            juce::File fileForComparison;
            if (keepRegressionOutputFiles)
            {
                juce::File regressionOutputDir { "/Users/yyf/Documents/GitHub/Fire/tests/RegressionOutput/" };
                if (!regressionOutputDir.isDirectory())
                    REQUIRE(regressionOutputDir.createDirectory().wasOk());
                fileForComparison = regressionOutputDir.getChildFile(presetName + "_sine_output.wav");
            }
            else
            {
                fileForComparison = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                                .getChildFile("temp_regression_sine_output.wav");
            }
            
            fileForComparison.deleteFile();

            // 1. Write the processed buffer to the file
            {
                std::unique_ptr<juce::AudioFormatWriter> writer(
                    formatManager.findFormatForFileExtension("wav")->createWriterFor(
                        new juce::FileOutputStream(fileForComparison),
                        sampleRate, bufferToProcess.getNumChannels(), 24, {}, 0
                    )
                );
                REQUIRE(writer != nullptr);
                writer->writeFromAudioSampleBuffer(bufferToProcess, 0, bufferToProcess.getNumSamples());
            } // writer is flushed and closed here

            // 2. Read the file back into a new buffer
            std::unique_ptr<juce::AudioFormatReader> resultReader(formatManager.createReaderFor(fileForComparison));
            REQUIRE(resultReader != nullptr);
            juce::AudioBuffer<float> reloadedResultBuffer(resultReader->numChannels, (int)resultReader->lengthInSamples);
            resultReader->read(&reloadedResultBuffer, 0, (int)resultReader->lengthInSamples, 0, true, true);
            
            if (!keepRegressionOutputFiles)
            {
                fileForComparison.deleteFile();
            }

            // 3. Load the golden master file
            juce::File goldenFile { "/Users/yyf/Documents/GitHub/Fire/tests/GoldenMasters/" + presetName + "_sine_output.wav" };
            REQUIRE(goldenFile.existsAsFile());
            std::unique_ptr<juce::AudioFormatReader> goldenReader(formatManager.createReaderFor(goldenFile));
            REQUIRE(goldenReader != nullptr);
            juce::AudioBuffer<float> goldenBuffer(goldenReader->numChannels, (int)goldenReader->lengthInSamples);
            goldenReader->read(&goldenBuffer, 0, (int)goldenReader->lengthInSamples, 0, true, true);

            // 4. Compare the reloaded buffer with the golden master
            requireBuffersAreEquivalent(reloadedResultBuffer, goldenBuffer);
        }
    }
    REQUIRE(atLeastOnePresetTested);
}