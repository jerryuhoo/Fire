// GenerateGoldenMasters.cpp (Corrected for State Isolation)
#include <catch2/catch_test_macros.hpp>
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

TEST_CASE("Golden Master Generation")
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const double standardSampleRate = 48000.0;
    const int standardBlockSize = 512;

    // --- 1. Load Input Audio File (do this once) ---
    juce::File inputFile { "/Users/yyf/Documents/GitHub/Fire/tests/TestAudioFiles/drum.wav" };
    REQUIRE(inputFile.existsAsFile());
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(inputFile));
    REQUIRE(reader != nullptr);
    juce::AudioBuffer<float> buffer(reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);
    std::cout << "Input audio loaded successfully." << std::endl;

    // --- 2. Iterate Through Preset Files ---
    juce::File presetsDir { "/Users/yyf/Library/Audio/Presets/Wings/Fire/User" };
    REQUIRE(presetsDir.isDirectory());
    const juce::String filePattern = "*.fire";
    int numPresetsFound = presetsDir.getNumberOfChildFiles(juce::File::TypesOfFileToFind::findFiles, filePattern);
    REQUIRE(numPresetsFound > 0);
    std::cout << "Found " << numPresetsFound << " preset files. Processing..." << std::endl;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, filePattern))
    {
        auto presetFile = entry.getFile();
        juce::String presetName = presetFile.getFileNameWithoutExtension();
        std::cout << "  Processing preset: " << presetName << std::endl;
        
        // +++ CREATE A FRESH PROCESSOR FOR EACH PRESET +++
        // This is the critical fix. It ensures no state is carried over from the previous iteration.
        FireAudioProcessor processor;
        processor.prepareToPlay(standardSampleRate, standardBlockSize);

        auto bufferToProcess = buffer;

        // --- 3. Load Preset Manually from Attributes ---
        std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(presetFile);
        REQUIRE(xml != nullptr);

        for (int i = 0; i < xml->getNumAttributes(); ++i)
        {
            auto attributeName = xml->getAttributeName(i);
            auto attributeValue = xml->getAttributeValue(i);
            
            if (auto* parameter = processor.treeState.getParameter(attributeName))
            {
                float normalizedValue = attributeValue.getFloatValue();
                parameter->setValueNotifyingHost(normalizedValue);
            }
        }

        // --- 4. Process Audio ---
        juce::MidiBuffer midi;
        // Prepare the processor with the specifics of the audio file being processed.
        processor.prepareToPlay(reader->sampleRate, bufferToProcess.getNumSamples());
        processor.processBlock(bufferToProcess, midi);

        // --- 5. Write Output File ---
        juce::File outputDir { "/Users/yyf/Documents/GitHub/Fire/tests/GoldenMasters/" };
        if (!outputDir.isDirectory()) {
            REQUIRE(outputDir.createDirectory().wasOk());
        }

        juce::File outputFile = outputDir.getChildFile(presetName + "_drums_output.wav");
        outputFile.deleteFile();

        std::unique_ptr<juce::AudioFormatWriter> writer(
            formatManager.findFormatForFileExtension("wav")->createWriterFor(
                new juce::FileOutputStream(outputFile),
                reader->sampleRate,
                bufferToProcess.getNumChannels(),
                24, {}, 0
            )
        );
        REQUIRE(writer != nullptr);
        
        bool writeOk = writer->writeFromAudioSampleBuffer(bufferToProcess, 0, bufferToProcess.getNumSamples());
        REQUIRE(writeOk);
        std::cout << "    -> Successfully wrote to: " << outputFile.getFullPathName() << std::endl;
    }
}