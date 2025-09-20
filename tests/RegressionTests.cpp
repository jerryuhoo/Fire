// RegressionTests.cpp
#include "../Source/PluginProcessor.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

// A namespace to hold all helper functions for the tests.
namespace TestHelpers
{

    /**
 * @brief Finds the project root directory by navigating up from the executable's location.
 *
 * This function assumes a standard project structure where the test executable is located in a build directory
 * (e.g., build/tests/Debug).
 * @return juce::File object pointing to the project root directory.
 */
    juce::File findProjectRoot()
    {
        auto executableFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        auto projectRoot = executableFile.getParentDirectory().getParentDirectory();
        jassert(projectRoot.getChildFile("tests").isDirectory()); // Assert that we found the correct directory
        return projectRoot;
    }

    /**
 * @brief Generates an AudioBuffer containing a sine wave.
 *
 * @param sampleRate The sample rate for the wave generation.
 * @param numChannels The number of channels for the buffer.
 * @param numSamples The number of samples for the buffer.
 * @param frequency The frequency of the sine wave.
 * @return A juce::AudioBuffer<float> filled with the sine wave.
 */
    juce::AudioBuffer<float> createSineWaveBuffer(double sampleRate, int numChannels, int numSamples, float frequency)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        double currentAngle = 0.0;
        const double angleDelta = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentSample = (float) std::sin(currentAngle);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                buffer.setSample(channel, sample, currentSample);
            }
            currentAngle += angleDelta;
        }
        return buffer;
    }

    /**
 * @brief Compares two audio buffers for equivalence within a given tolerance.
 *
 * Fails the test if the buffers differ in size or if any sample's value difference exceeds the tolerance.
 * @param result The buffer produced by the processor.
 * @param expected The golden master buffer.
 */
    void requireBuffersAreEquivalent(const juce::AudioBuffer<float>& result, const juce::AudioBuffer<float>& expected)
    {
        REQUIRE(result.getNumChannels() == expected.getNumChannels());
        REQUIRE(result.getNumSamples() == expected.getNumSamples());

        const float tolerance = 1e-3f; // A small tolerance to account for floating point inaccuracies.

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

    /**
 * @brief Safely reads audio data from an AudioFormatReader into a buffer.
 *
 * This function performs boundary checks to ensure that it does not read past the end of the source
 * or write past the end of the destination buffer, preventing potential overflows (CWE-120).
 * @param reader The AudioFormatReader to read from.
 * @param buffer The AudioBuffer to write to.
 * @return True if the read operation was successful, false otherwise.
 */
    bool safeReadFromReader(juce::AudioFormatReader* reader, juce::AudioBuffer<float>& buffer)
    {
        if (reader == nullptr)
            return false;

        buffer.clear(); // Clear buffer first to ensure clean state.

        const int64_t readerLength = reader->lengthInSamples;
        const int bufferSamples = buffer.getNumSamples();

        if (readerLength <= 0 || bufferSamples <= 0)
            return false;

        // Determine the actual number of samples to read, clamped by both source and destination limits.
        const int samplesToRead = static_cast<int>(std::min<int64_t>(readerLength, bufferSamples));

        if (samplesToRead < 0 || samplesToRead > buffer.getNumSamples())
        {
            jassertfalse; // This path should be unreachable.
            return false;
        }

        // Read data, allowing JUCE to handle channel mapping and zero-filling if necessary.
        return reader->read(&buffer, 0, samplesToRead, 0, true, true);
    }

    /**
 * @brief Applies latency compensation to an audio buffer by shifting its content.
 *
 * This simulates how a DAW compensates for plugin-reported latency.
 * @param rawOutput The unprocessed buffer straight from the plugin.
 * @param latencySamples The number of samples to shift the audio by.
 * @return A new juce::AudioBuffer<float> with latency compensation applied.
 */
    juce::AudioBuffer<float> applyLatencyCompensation(const juce::AudioBuffer<float>& rawOutput, int latencySamples)
    {
        juce::AudioBuffer<float> compensatedOutput(rawOutput.getNumChannels(), rawOutput.getNumSamples());
        compensatedOutput.clear();

        if (latencySamples > 0)
        {
            // Calculate how many samples will remain after shifting.
            const int numSamplesToCopy = rawOutput.getNumSamples() - latencySamples;
            if (numSamplesToCopy > 0)
            {
                for (int channel = 0; channel < rawOutput.getNumChannels(); ++channel)
                {
                    // Copy the relevant part of the raw output into the start of the compensated buffer.
                    compensatedOutput.copyFrom(channel, 0, rawOutput, channel, latencySamples, numSamplesToCopy);
                }
            }
            // If numSamplesToCopy is not > 0, the buffer is shorter than the latency,
            // so the output remains silent, which is correct.
        }
        else
        {
            // If there's no latency, just make a direct copy.
            compensatedOutput.makeCopyOf(rawOutput);
        }
        return compensatedOutput;
    }

    /**
 * @brief Core test logic: processes an input buffer with a given preset and compares it to a golden master file.
 *
 * @param presetFile The .fire preset file to load.
 * @param inputBuffer The raw audio to be processed.
 * @param sampleRate The sample rate of the input audio.
 * @param testIdentifier A unique string (e.g., "drums", "sine") for naming output files.
 * @param keepFiles A flag to determine if generated output files should be kept for inspection.
 */
    void runTestForPreset(const juce::File& presetFile,
                          const juce::AudioBuffer<float>& inputBuffer,
                          double sampleRate,
                          const juce::String& testIdentifier,
                          bool keepFiles)
    {
        juce::String presetName = presetFile.getFileNameWithoutExtension();
        SECTION("Preset: " + presetName.toStdString() + " (" + testIdentifier.toStdString() + ")")
        {
            // --- 1. Setup Processor and Load Preset ---
            FireAudioProcessor processor;
            std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(presetFile);
            REQUIRE(xml != nullptr);
            for (int i = 0; i < xml->getNumAttributes(); ++i)
            {
                auto attributeName = xml->getAttributeName(i);
                auto attributeValue = xml->getAttributeValue(i);
                if (auto* parameter = processor.treeState.getParameter(attributeName))
                    parameter->setValueNotifyingHost(attributeValue.getFloatValue());
            }

            const int blockSize = 512;
            processor.prepareToPlay(sampleRate, blockSize);

            // --- 2. Process Audio in Blocks ---
            juce::AudioBuffer<float> rawOutputBuffer(inputBuffer.getNumChannels(), inputBuffer.getNumSamples());
            rawOutputBuffer.clear();
            juce::MidiBuffer midi;

            for (int startSample = 0; startSample < inputBuffer.getNumSamples(); startSample += blockSize)
            {
                int numSamplesThisBlock = std::min(blockSize, inputBuffer.getNumSamples() - startSample);
                juce::AudioBuffer<float> blockToProcess(inputBuffer.getNumChannels(), numSamplesThisBlock);
                for (int channel = 0; channel < inputBuffer.getNumChannels(); ++channel)
                {
                    blockToProcess.copyFrom(channel, 0, inputBuffer, channel, startSample, numSamplesThisBlock);
                }

                processor.processBlock(blockToProcess, midi);

                for (int channel = 0; channel < rawOutputBuffer.getNumChannels(); ++channel)
                {
                    rawOutputBuffer.copyFrom(channel, startSample, blockToProcess, channel, 0, numSamplesThisBlock);
                }
            }

            // --- 3. Apply Latency Compensation for HQ Mode ---
            // This is the crucial step for handling the delay introduced by the oversampler.
            int latencySamples = 0;
            if (auto* hqParam = processor.treeState.getParameter("hq"))
            {
                if (hqParam->getValue() > 0.5f) // HQ mode is ON
                {
                    // Get the latency introduced by the oversampling process.
                    // This value is used to simulate the host's latency compensation.
                    latencySamples = static_cast<int>(processor.getTotalLatency());
                }
            }

            auto compensatedOutputBuffer = applyLatencyCompensation(rawOutputBuffer, latencySamples);

            // --- 4. Write, Reload, and Compare ---
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            auto projectRoot = findProjectRoot();

            juce::File fileForComparison;
            if (keepFiles)
            {
                juce::File regressionOutputDir { projectRoot.getChildFile("tests/RegressionOutput") };
                if (! regressionOutputDir.isDirectory())
                    REQUIRE(regressionOutputDir.createDirectory().wasOk());
                fileForComparison = regressionOutputDir.getChildFile(presetName + "_" + testIdentifier + "_output.wav");
            }
            else
            {
                fileForComparison = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                        .getChildFile("temp_" + presetName + "_" + testIdentifier + ".wav");
            }
            fileForComparison.deleteFile();

            // Write to file
            std::unique_ptr<juce::AudioFormatWriter> writer(
                formatManager.findFormatForFileExtension("wav")->createWriterFor(
                    new juce::FileOutputStream(fileForComparison), sampleRate, compensatedOutputBuffer.getNumChannels(), 24, {}, 0));
            REQUIRE(writer != nullptr);
            writer->writeFromAudioSampleBuffer(compensatedOutputBuffer, 0, compensatedOutputBuffer.getNumSamples());
            writer.reset(); // Close file stream

            // Read the result back to account for file I/O precision changes
            std::unique_ptr<juce::AudioFormatReader> resultReader(formatManager.createReaderFor(fileForComparison));
            REQUIRE(resultReader != nullptr);
            juce::AudioBuffer<float> reloadedResultBuffer(resultReader->numChannels, (int) resultReader->lengthInSamples);
            REQUIRE(safeReadFromReader(resultReader.get(), reloadedResultBuffer));

            if (! keepFiles)
                fileForComparison.deleteFile();

            // Load the golden master file
            juce::File goldenFile = projectRoot.getChildFile("tests/GoldenMasters/" + presetName + "_" + testIdentifier + "_output.wav");
            REQUIRE(goldenFile.existsAsFile());
            std::unique_ptr<juce::AudioFormatReader> goldenReader(formatManager.createReaderFor(goldenFile));
            REQUIRE(goldenReader != nullptr);
            juce::AudioBuffer<float> goldenBuffer(goldenReader->numChannels, (int) goldenReader->lengthInSamples);
            REQUIRE(safeReadFromReader(goldenReader.get(), goldenBuffer));

            // Finally, compare the buffers
            requireBuffersAreEquivalent(reloadedResultBuffer, goldenBuffer);
        }
    }

} // namespace TestHelpers

TEST_CASE("Regression Test: Verify output against Golden Masters (Drums)")
{
    // Set to 'true' to generate .wav files for manual inspection.
    constexpr bool keepRegressionOutputFiles = false;

    // --- 1. Setup ---
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    auto projectRoot = TestHelpers::findProjectRoot();

    // --- 2. Load Input Audio ---
    juce::File inputFile { projectRoot.getChildFile("tests/TestAudioFiles/drum.wav") };
    REQUIRE(inputFile.existsAsFile());
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(inputFile));
    REQUIRE(reader != nullptr);

    juce::AudioBuffer<float> originalInputBuffer((int) reader->numChannels, (int) reader->lengthInSamples);
    REQUIRE(TestHelpers::safeReadFromReader(reader.get(), originalInputBuffer));

    // --- 3. Iterate Through Presets and Run Tests ---
    juce::File presetsDir { projectRoot.getChildFile("tests/Presets") };
    REQUIRE(presetsDir.isDirectory());
    bool atLeastOnePresetTested = false;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, "*.fire"))
    {
        atLeastOnePresetTested = true;
        TestHelpers::runTestForPreset(entry.getFile(), originalInputBuffer, reader->sampleRate, "drums", keepRegressionOutputFiles);
    }
    REQUIRE(atLeastOnePresetTested);
}

TEST_CASE("Regression Test: Verify output against Golden Masters (Sine Wave)")
{
    // Set to 'true' to generate .wav files for manual inspection.
    constexpr bool keepRegressionOutputFiles = false;

    // --- 1. Setup ---
    auto projectRoot = TestHelpers::findProjectRoot();
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int numChannels = 2;
    const float frequency = 440.0f;

    // --- 2. Generate Input Signal ---
    auto sineBuffer = TestHelpers::createSineWaveBuffer(sampleRate, numChannels, blockSize, frequency);

    // --- 3. Iterate Through Presets and Run Tests ---
    juce::File presetsDir { projectRoot.getChildFile("tests/Presets") };
    REQUIRE(presetsDir.isDirectory());
    bool atLeastOnePresetTested = false;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, "*.fire"))
    {
        atLeastOnePresetTested = true;
        TestHelpers::runTestForPreset(entry.getFile(), sineBuffer, sampleRate, "sine", keepRegressionOutputFiles);
    }
    REQUIRE(atLeastOnePresetTested);
}
