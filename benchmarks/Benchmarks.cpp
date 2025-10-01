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

TEST_CASE("UI performance")
{
    // Create an instance of the plugin processor and its editor
    FireAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());

    // Set a typical size for the editor
    editor->setSize(800, 600);

    // Create a dummy image to render onto. This prevents anything from actually
    // appearing on screen, but allows us to measure the drawing operations.
    juce::Image dummyImage(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    juce::Graphics g(dummyImage);

    BENCHMARK("Editor paint() call")
    {
        // We measure how long it takes to execute one full paint of the editor
        editor->paint(g);
    };

    // It's also good practice to clean up the editor properly
    processor.editorBeingDeleted(editor.get());
}