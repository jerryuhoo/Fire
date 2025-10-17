#include "../Source/DSP/LfoManager.h" // <-- Add this line
#include "../Source/DSP/ModulationRouting.h" // <-- And this line

TEST_CASE("Boot performance")
{
    BENCHMARK_ADVANCED("Processor constructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::storage_for<FireAudioProcessor>> storage(size_t(meter.runs()));
        meter.measure([&](int i)
                      { storage[(size_t) i].construct(); });
    };

    BENCHMARK_ADVANCED("Processor destructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::destructable_object<FireAudioProcessor>> storage(size_t(meter.runs()));
        for (auto& s : storage)
            s.construct();
        meter.measure([&](int i)
                      { storage[(size_t) i].destruct(); });
    };

    BENCHMARK_ADVANCED("Editor open and close")
    (Catch::Benchmark::Chronometer meter)
    {
        FireAudioProcessor plugin;

        // due to complex construction logic of the editor, let's measure open/close together
        meter.measure([&](int /* i */)
                      {
            auto editor = plugin.createEditorIfNeeded();
            plugin.editorBeingDeleted (editor);
            delete editor;
            return plugin.getActiveEditor(); });
    };
}

TEST_CASE("Real-time performance")
{
    // Create an instance of the plugin processor
    FireAudioProcessor plugin;

    // Basic setup for the processor
    const double sampleRate = 44100.0;
    const int blockSize = 512;
    plugin.prepareToPlay(sampleRate, blockSize);

    // Create a dummy audio buffer to process
    juce::AudioBuffer<float> buffer(2, blockSize);

    // To fill the buffer, get the write pointer for each channel and manually fill the samples.
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        // Get a pointer to the start of the channel's sample data
        auto* channelData = buffer.getWritePointer(channel);

        // Fill the channel with a constant value
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = 0.5f;
        }
    }

    // Create an empty midi buffer
    juce::MidiBuffer midiMessages;

    BENCHMARK("Process Block 512 samples")
    {
        // This is the part that gets measured
        plugin.processBlock(buffer, midiMessages);
    };

    // You can also test with different block sizes
    const int largeBlockSize = 2048;
    plugin.prepareToPlay(sampleRate, largeBlockSize);
    juce::AudioBuffer<float> largeBuffer(2, largeBlockSize);

    // Fill the larger buffer using the same correct method
    for (int channel = 0; channel < largeBuffer.getNumChannels(); ++channel)
    {
        auto* channelData = largeBuffer.getWritePointer(channel);
        for (int sample = 0; sample < largeBuffer.getNumSamples(); ++sample)
        {
            channelData[sample] = 0.5f;
        }
    }

    BENCHMARK("Process Block 2048 samples")
    {
        plugin.processBlock(largeBuffer, midiMessages);
    };
}

// TEST_CASE("UI performance")
// {
//     // Create an instance of the plugin processor and its editor
//     FireAudioProcessor processor;
//     std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());

//     // Set a typical size for the editor
//     editor->setSize(800, 600);

//     // Create a dummy image to render onto. This prevents anything from actually
//     // appearing on screen, but allows us to measure the drawing operations.
//     juce::Image dummyImage(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
//     juce::Graphics g(dummyImage);

//     BENCHMARK("Editor paint() call")
//     {
//         // We measure how long it takes to execute one full paint of the editor
//         editor->paint(g);
//     };

//     // It's also good practice to clean up the editor properly
//     processor.editorBeingDeleted(editor.get());
// }

TEST_CASE("Modular DSP Performance")
{
    // Common setup for all modular tests
    FireAudioProcessor plugin;
    const double sampleRate = 44100.0;
    const int blockSize = 1024;
    plugin.prepareToPlay(sampleRate, blockSize);

    // Create a reusable dummy audio buffer
    juce::AudioBuffer<float> buffer(2, blockSize);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Using a sine wave is slightly more realistic than a constant value
            channelData[sample] = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * sample / sampleRate);
        }
    }

    // --- Benchmark for Downsampling Effect ---
    // Section to isolate and test only the applyDownsamplingEffect function
    SECTION("Downsampling Effect")
    {
        // Get the parameter state tree to control the plugin's behavior
        auto& treeState = plugin.treeState;
        auto& lfoManager = plugin.getLfoManager(); // Get a reference to the LfoManager

        // --- Test Case 1: Not Modulated (Block-based processing) ---
        {
            // Enable the downsampling effect and set a static ratio
            treeState.getParameter(DOWNSAMPLE_BYPASS_ID)->setValueNotifyingHost(true);
            treeState.getParameter(DOWNSAMPLE_ID)->setValueNotifyingHost(16.0f);

            auto bufferCopy = buffer;
            juce::AudioBuffer<float> lfoOutputBuffer(4, buffer.getNumSamples());
            lfoOutputBuffer.clear(); // Empty buffer, as no modulation is active

            BENCHMARK("applyDownsamplingEffect (Not Modulated, Rate: 16)")
            {
                plugin.applyDownsamplingEffect(bufferCopy, lfoOutputBuffer);
            };
        }

        // --- Test Case 2: Modulated (Sample-accurate processing) ---
        {
            // Enable the effect
            treeState.getParameter(DOWNSAMPLE_BYPASS_ID)->setValueNotifyingHost(true);

            // Manually create a routing from LFO 1 to the Downsample parameter
            ModulationRouting routing;
            routing.sourceLfoIndex = 0; // LFO 1 (0-based)
            routing.targetParameterID = DOWNSAMPLE_ID;
            routing.depth = 1.0f; // Full depth
            routing.isBipolar = false; // Unipolar

            // Add the routing to the manager
            lfoManager.getModulationRoutings().add(routing);

            auto bufferCopy = buffer;
            juce::AudioBuffer<float> lfoOutputBuffer(4, buffer.getNumSamples());

            // Fill the LFO buffer with a simple ramping signal to simulate a saw wave
            auto* lfoData = lfoOutputBuffer.getWritePointer(routing.sourceLfoIndex);
            for (int i = 0; i < lfoOutputBuffer.getNumSamples(); ++i)
            {
                lfoData[i] = (float) i / (float) lfoOutputBuffer.getNumSamples();
            }

            BENCHMARK("applyDownsamplingEffect (Modulated)")
            {
                plugin.applyDownsamplingEffect(bufferCopy, lfoOutputBuffer);
            };

            // Clean up by removing the routing after the test
            lfoManager.getModulationRoutings().clear();
        }
    }

    // --- Benchmark for MultiBand Processing ---
    // Section to isolate and test the core multiband splitting, processing, and summing
    SECTION("MultiBand Processing")
    {
        auto& treeState = plugin.treeState;

        // Configure the plugin for 4-band processing
        treeState.getParameter(NUM_BANDS_ID)->setValueNotifyingHost(4);

        // Ensure other effects are disabled to isolate the multiband logic
        treeState.getParameter(DOWNSAMPLE_BYPASS_ID)->setValueNotifyingHost(false);
        treeState.getParameter(FILTER_BYPASS_ID)->setValueNotifyingHost(false);

        auto bufferCopy = buffer;
        juce::AudioBuffer<float> lfoOutputBuffer(4, buffer.getNumSamples());

        BENCHMARK("processMultiBand (4 Bands)")
        {
            plugin.processMultiBand(bufferCopy, lfoOutputBuffer, sampleRate);
        };
    }

    // You can add more SECTIONS here to test other specific functions like applyGlobalEffects etc.
}