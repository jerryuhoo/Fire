#include <catch2/catch_test_macros.hpp>
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

TEST_CASE("Golden Master Generation")
{
    FireAudioProcessor processor;
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const double standardSampleRate = 48000.0;
    const int standardBlockSize = 512; // A typical block size is fine here.
    
    // Call prepareToPlay() immediately after creating the processor instance.
    // This ensures that getSampleRate() will always return a valid value from now on.
    processor.prepareToPlay(standardSampleRate, standardBlockSize);

    // --- 1. Load Input Audio File ---
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
        
        auto bufferToProcess = buffer;

        // --- 3. Load Preset Manually from Attributes (THE CORRECT WAY) ---
        std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(presetFile);
        REQUIRE(xml != nullptr);

        // 遍历 XML 元素的所有属性
        for (int i = 0; i < xml->getNumAttributes(); ++i)
        {
            // 获取属性的名字 (e.g., "drive1") 和值 (e.g., "0.0")
            auto attributeName = xml->getAttributeName(i);
            auto attributeValue = xml->getAttributeValue(i);
            
            // 在处理器的参数树 (treeState) 中找到对应的参数
            if (auto* parameter = processor.treeState.getParameter(attributeName))
            {
                // 将字符串值转换为归一化的浮点数值 (0.0 to 1.0)
                float normalizedValue = attributeValue.getFloatValue();
                // 设定参数值，并通知宿主（虽然在测试中这步不是必须的，但是是好习惯）
                parameter->setValueNotifyingHost(normalizedValue);
            }
        }

        // --- 4. Process Audio ---
        juce::MidiBuffer midi;
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