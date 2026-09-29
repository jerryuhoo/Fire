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
    static juce::File findProjectRoot()
    {
        auto candidate = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                             .getParentDirectory();

        while (candidate != candidate.getParentDirectory())
        {
            if (candidate.getChildFile("CMakeLists.txt").existsAsFile()
                && candidate.getChildFile("tests").isDirectory())
                return candidate;

            candidate = candidate.getParentDirectory();
        }

        return {};
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
    static juce::AudioBuffer<float> createSineWaveBuffer(double sampleRate, int numChannels, int numSamples, float frequency)
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
    static void requireBuffersAreEquivalent(const juce::AudioBuffer<float>& result, const juce::AudioBuffer<float>& expected)
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

                if (! std::isfinite(resultSample) || ! std::isfinite(expectedSample))
                {
                    FAIL("Non-finite sample at channel " << channel << ", sample " << sample
                                                          << ". Expected: " << expectedSample
                                                          << ", Got: " << resultSample);
                }

                if (std::abs(resultSample - expectedSample) > tolerance)
                {
                    FAIL("Sample mismatch at channel " << channel << ", sample " << sample
                                                       << ". Expected: " << expectedSample << ", Got: " << resultSample);
                }
            }
        }
        SUCCEED("Buffers are equivalent within tolerance.");
    }

    static void requireBufferContainsOnlyFiniteSamples(const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (! std::isfinite(buffer.getSample(channel, sample)))
                    FAIL("Non-finite processed sample at channel " << channel
                                                                    << ", sample " << sample);
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
    static bool safeReadFromReader(juce::AudioFormatReader* reader, juce::AudioBuffer<float>& buffer)
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
 * @param outputSamples Number of compensated samples to return.
 * @return A new juce::AudioBuffer<float> containing [latency, latency + N).
 */
    static juce::AudioBuffer<float> applyLatencyCompensation(
        const juce::AudioBuffer<float>& rawOutput,
        int latencySamples,
        int outputSamples)
    {
        REQUIRE(latencySamples >= 0);
        REQUIRE(outputSamples >= 0);
        juce::AudioBuffer<float> compensatedOutput(rawOutput.getNumChannels(),
                                                   outputSamples);
        compensatedOutput.clear();

        const int availableSamples = std::max(
            0,
            std::min(outputSamples,
                     rawOutput.getNumSamples() - latencySamples));

        for (int channel = 0; channel < rawOutput.getNumChannels(); ++channel)
        {
            if (availableSamples > 0)
                compensatedOutput.copyFrom(channel,
                                           0,
                                           rawOutput,
                                           channel,
                                           latencySamples,
                                           availableSamples);
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
    static void runTestForPreset(const juce::File& presetFile,
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

            // These historical audio fixtures deliberately load parameters
            // without the full preset/LFO loader. Mirror its missing-family
            // migration: the old reference recordings used Legacy Link, not
            // the modern compensation default for newly created instances.
            bool hasDriveCompState = xml->hasAttribute("driveCompSchemaVersion");
            for (const auto& id : fire::drive_comp::parameterIDs())
                hasDriveCompState = hasDriveCompState || xml->hasAttribute(id);
            if (! hasDriveCompState)
                for (const auto& id : fire::drive_comp::parameterIDs())
                {
                    auto* parameter = processor.treeState.getParameter(id);
                    REQUIRE(parameter != nullptr);
                    parameter->setValueNotifyingHost(0.0f);
                }

            // After loading the preset, manually enable the Shape module for all bands.
            // This is necessary because older presets won't have this new parameter,
            // and it defaults to 'off' (bypassed), causing a mismatch with the golden masters.
            for (int i = 0; i < 4; ++i)
            {
                const auto shapeBypassParamID = ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, i);
                if (auto* shapeBypassParam = processor.treeState.getParameter(shapeBypassParamID))
                {
                    // Set to 1.0f to ensure the shape module is ON.
                    shapeBypassParam->setValueNotifyingHost(1.0f);
                }
            }

            const int blockSize = 512;
            processor.prepareToPlay(sampleRate, blockSize);
            const int latencySamples = processor.getLatencySamples();
            REQUIRE(latencySamples > 0);

            const auto* hqParameter = processor.treeState.getParameter(HQ_ID);
            REQUIRE(hqParameter != nullptr);
            const bool useHq = hqParameter->getValue() > 0.5f;
            const int compensationLatency = useHq
                                                ? static_cast<int>(
                                                      processor.getTotalLatency())
                                                : latencySamples;
            REQUIRE(compensationLatency > 0);

            // --- 2. Process audio in blocks ---
            // Base presets need D tail samples because the new integer pad is
            // part of this fix. HQ audio itself is unchanged, so preserve the
            // historical golden harness (including its D zero tail) rather
            // than rewriting otherwise-identical HQ fixtures.
            const int sourceSamples = inputBuffer.getNumSamples();
            const int renderedSamples = sourceSamples
                                      + (useHq ? 0 : latencySamples);
            juce::AudioBuffer<float> rawOutputBuffer(inputBuffer.getNumChannels(),
                                                     renderedSamples);
            rawOutputBuffer.clear();
            juce::MidiBuffer midi;

            for (int startSample = 0;
                 startSample < renderedSamples;
                 startSample += blockSize)
            {
                const int numSamplesThisBlock = std::min(blockSize,
                                                         renderedSamples
                                                             - startSample);
                juce::AudioBuffer<float> blockToProcess(inputBuffer.getNumChannels(), numSamplesThisBlock);
                blockToProcess.clear();
                const int inputSamplesThisBlock = std::max(
                    0,
                    std::min(numSamplesThisBlock,
                             sourceSamples - startSample));
                if (inputSamplesThisBlock > 0)
                {
                    for (int channel = 0;
                         channel < inputBuffer.getNumChannels();
                         ++channel)
                    {
                        blockToProcess.copyFrom(channel,
                                                0,
                                                inputBuffer,
                                                channel,
                                                startSample,
                                                inputSamplesThisBlock);
                    }
                }

                processor.processBlock(blockToProcess, midi);

                for (int channel = 0; channel < rawOutputBuffer.getNumChannels(); ++channel)
                {
                    rawOutputBuffer.copyFrom(channel, startSample, blockToProcess, channel, 0, numSamplesThisBlock);
                }
            }

            // --- 3. Apply the host's fixed latency compensation ---
            // HQ automation must not change the latency reported to the host.
            // Base processing carries an integer delay of the same length, so
            // compensating every preset preserves the historical base golden
            // samples while the natural HQ signal path remains unchanged.
            auto compensatedOutputBuffer = applyLatencyCompensation(
                rawOutputBuffer,
                compensationLatency,
                sourceSamples);

            // --- 4. Write, Reload, and Compare ---
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            auto projectRoot = findProjectRoot();

            auto* wavFormat = formatManager.findFormatForFileExtension("wav");
            REQUIRE(wavFormat != nullptr);

            // Encode in memory so the regression suite does not depend on access to a
            // particular temporary directory. Reloading the encoded WAV still applies
            // exactly the same 24-bit quantisation as the file-based implementation.
            requireBufferContainsOnlyFiniteSamples(compensatedOutputBuffer);
            juce::MemoryBlock encodedAudio;
            std::unique_ptr<juce::OutputStream> outputStream =
                std::make_unique<juce::MemoryOutputStream>(encodedAudio, false);
            auto writer = wavFormat->createWriterFor(
                outputStream,
                juce::AudioFormatWriterOptions {}
                    .withSampleRate(sampleRate)
                    .withNumChannels(compensatedOutputBuffer.getNumChannels())
                    .withBitsPerSample(24));
            REQUIRE(writer != nullptr);
            REQUIRE(writer->writeFromAudioSampleBuffer(
                compensatedOutputBuffer, 0, compensatedOutputBuffer.getNumSamples()));
            writer.reset(); // Finalise the WAV header and flush the memory stream.

            if (keepFiles)
            {
                juce::File regressionOutputDir { projectRoot.getChildFile("tests/RegressionOutput") };
                if (! regressionOutputDir.isDirectory())
                    REQUIRE(regressionOutputDir.createDirectory().wasOk());

                const auto outputFile = regressionOutputDir.getChildFile(
                    presetName + "_" + testIdentifier + "_output.wav");
                REQUIRE(outputFile.replaceWithData(encodedAudio.getData(), encodedAudio.getSize()));
            }

            // Read the result back to account for file I/O precision changes
            std::unique_ptr<juce::AudioFormatReader> resultReader(
                formatManager.createReaderFor(
                    std::make_unique<juce::MemoryInputStream>(encodedAudio, false)));
            REQUIRE(resultReader != nullptr);
            juce::AudioBuffer<float> reloadedResultBuffer(
                static_cast<int>(resultReader->numChannels),
                static_cast<int>(resultReader->lengthInSamples));
            REQUIRE(safeReadFromReader(resultReader.get(), reloadedResultBuffer));

            // Load the golden master file
            juce::File goldenFile = projectRoot.getChildFile("tests/GoldenMasters/" + presetName + "_" + testIdentifier + "_output.wav");

            // Golden files are immutable during an ordinary test run. A
            // deliberate, auditable update requires an explicit environment
            // switch so a regression can never overwrite its own evidence.
            const auto updateGoldenMasters = juce::SystemStats::getEnvironmentVariable(
                "FIRE_UPDATE_GOLDEN_MASTERS", {});
            if (updateGoldenMasters.equalsIgnoreCase("all"))
                REQUIRE(goldenFile.replaceWithData(encodedAudio.getData(), encodedAudio.getSize()));

            REQUIRE(goldenFile.existsAsFile());
            std::unique_ptr<juce::AudioFormatReader> goldenReader(formatManager.createReaderFor(goldenFile));
            REQUIRE(goldenReader != nullptr);
            juce::AudioBuffer<float> goldenBuffer(
                static_cast<int>(goldenReader->numChannels),
                static_cast<int>(goldenReader->lengthInSamples));
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
