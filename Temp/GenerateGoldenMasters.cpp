// GenerateGoldenMasters.cpp
// This is a utility for generating the "golden master" audio files that the regression tests will compare against.
// It should be run manually whenever a change is made that intentionally alters the audio output.
#include "../Source/PluginProcessor.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

namespace GoldenMasterHelpers
{

    /**
 * @brief Finds the project root directory by navigating up from the executable's location.
 */
    juce::File findProjectRoot()
    {
        auto executableFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        auto projectRoot = executableFile.getParentDirectory().getParentDirectory();
        jassert(projectRoot.getChildFile("tests").isDirectory());
        return projectRoot;
    }

    /**
 * @brief Generates an AudioBuffer containing a sine wave.
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
 * @brief Safely reads audio data from an AudioFormatReader into a buffer.
 */
    bool safeReadFromReader(juce::AudioFormatReader* reader, juce::AudioBuffer<float>& buffer)
    {
        if (reader == nullptr)
            return false;
        buffer.clear();
        const int64_t readerLength = reader->lengthInSamples;
        const int bufferSamples = buffer.getNumSamples();
        if (readerLength <= 0 || bufferSamples <= 0)
            return false;
        const int samplesToRead = static_cast<int>(std::min<int64_t>(readerLength, bufferSamples));
        return reader->read(&buffer, 0, samplesToRead, 0, true, true);
    }

    /**
 * @brief Applies latency compensation to an audio buffer by shifting its content.
 */
    juce::AudioBuffer<float> applyLatencyCompensation(const juce::AudioBuffer<float>& rawOutput, int latencySamples)
    {
        juce::AudioBuffer<float> compensatedOutput(rawOutput.getNumChannels(), rawOutput.getNumSamples());
        compensatedOutput.clear();

        if (latencySamples > 0)
        {
            const int numSamplesToCopy = rawOutput.getNumSamples() - latencySamples;
            if (numSamplesToCopy > 0)
            {
                for (int channel = 0; channel < rawOutput.getNumChannels(); ++channel)
                {
                    compensatedOutput.copyFrom(channel, 0, rawOutput, channel, latencySamples, numSamplesToCopy);
                }
            }
        }
        else
        {
            compensatedOutput.makeCopyOf(rawOutput);
        }
        return compensatedOutput;
    }

    /**
 * @brief Core logic for processing an input buffer with a preset and saving the result.
 */
    void generateMasterFileForPreset(const juce::File& presetFile,
                                     const juce::AudioBuffer<float>& inputBuffer,
                                     double sampleRate,
                                     const juce::String& testIdentifier)
    {
        juce::String presetName = presetFile.getFileNameWithoutExtension();
        std::cout << "  Processing preset: " << presetName << " for " << testIdentifier << "..." << std::endl;

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
        int latencySamples = 0;
        if (auto* hqParam = processor.treeState.getParameter("hq"))
        {
            if (hqParam->getValue() > 0.5f)
                latencySamples = static_cast<int>(processor.getTotalLatency());
        }
        auto compensatedOutputBuffer = applyLatencyCompensation(rawOutputBuffer, latencySamples);

        // --- 4. Write Compensated Output File ---
        auto projectRoot = findProjectRoot();
        juce::File outputDir { projectRoot.getChildFile("tests/GoldenMasters") };
        if (! outputDir.isDirectory())
            REQUIRE(outputDir.createDirectory().wasOk());

        juce::File outputFile = outputDir.getChildFile(presetName + "_" + testIdentifier + "_output.wav");
        outputFile.deleteFile();

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatWriter> writer(
            formatManager.findFormatForFileExtension("wav")->createWriterFor(
                new juce::FileOutputStream(outputFile), sampleRate, compensatedOutputBuffer.getNumChannels(), 24, {}, 0));
        REQUIRE(writer != nullptr);
        bool writeOk = writer->writeFromAudioSampleBuffer(compensatedOutputBuffer, 0, compensatedOutputBuffer.getNumSamples());
        REQUIRE(writeOk);
    }

} // namespace GoldenMasterHelpers

TEST_CASE("Golden Master Generation (Drums)")
{
    // --- 1. Setup ---
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    auto projectRoot = GoldenMasterHelpers::findProjectRoot();

    // --- 2. Load Input Audio ---
    juce::File inputFile { projectRoot.getChildFile("tests/TestAudioFiles/drum.wav") };
    REQUIRE(inputFile.existsAsFile());
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(inputFile));
    REQUIRE(reader != nullptr);

    juce::AudioBuffer<float> originalInputBuffer((int) reader->numChannels, (int) reader->lengthInSamples);
    REQUIRE(GoldenMasterHelpers::safeReadFromReader(reader.get(), originalInputBuffer));
    std::cout << "Input audio loaded: " << inputFile.getFileName() << std::endl;

    // --- 3. Iterate Through Presets and Generate Masters ---
    juce::File presetsDir { projectRoot.getChildFile("tests/Presets") };
    REQUIRE(presetsDir.isDirectory());
    int numPresetsFound = presetsDir.getNumberOfChildFiles(juce::File::TypesOfFileToFind::findFiles, "*.fire");
    REQUIRE(numPresetsFound > 0);
    std::cout << "Found " << numPresetsFound << " preset files. Generating drum masters..." << std::endl;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, "*.fire"))
    {
        GoldenMasterHelpers::generateMasterFileForPreset(entry.getFile(), originalInputBuffer, reader->sampleRate, "drums");
    }
    std::cout << "Drum master generation complete." << std::endl;
}

TEST_CASE("Golden Master Generation (Sine Wave)")
{
    // --- 1. Setup ---
    auto projectRoot = GoldenMasterHelpers::findProjectRoot();
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int numChannels = 2;
    const float frequency = 440.0f;

    // --- 2. Generate Input Signal ---
    auto sineBuffer = GoldenMasterHelpers::createSineWaveBuffer(sampleRate, numChannels, blockSize, frequency);
    std::cout << "Generated 440Hz sine wave test signal." << std::endl;

    // --- 3. Iterate Through Presets and Generate Masters ---
    juce::File presetsDir { projectRoot.getChildFile("tests/Presets") };
    REQUIRE(presetsDir.isDirectory());
    int numPresetsFound = presetsDir.getNumberOfChildFiles(juce::File::TypesOfFileToFind::findFiles, "*.fire");
    REQUIRE(numPresetsFound > 0);
    std::cout << "Found " << numPresetsFound << " preset files. Generating sine wave masters..." << std::endl;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, "*.fire"))
    {
        GoldenMasterHelpers::generateMasterFileForPreset(entry.getFile(), sineBuffer, sampleRate, "sine");
    }
    std::cout << "Sine wave master generation complete." << std::endl;
}
