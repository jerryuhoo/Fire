#include <PluginEditor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdlib>
#include <vector>

struct SpectrumHostBypassPresentationTestAccess final
{
    static void applySkin(FireAudioProcessorEditor& editor, fire::ui::Skin skin) { editor.applySkin(skin); }
    static void selectWorkspace(FireAudioProcessorEditor& editor, int workspace)
    {
        editor.windowLeftButton.setToggleState(workspace==0,juce::dontSendNotification);
        editor.windowLfoButton.setToggleState(workspace==1,juce::dontSendNotification);
        editor.windowRightButton.setToggleState(workspace==2,juce::dontSendNotification);
        editor.selectWorkspace(workspace,false);
    }
    static juce::ComboBox& channel(FireAudioProcessorEditor& editor) { return editor.spectrumChannelBox; }
    static juce::Button& zoom(FireAudioProcessorEditor& editor) { return editor.zoomButton; }
    static juce::Button& collapse(FireAudioProcessorEditor& editor) { return editor.spectrumCollapseButton; }
    static juce::Rectangle<int> spectrumBounds(const FireAudioProcessorEditor& editor) { return editor.spectrumCardArea; }
    static juce::Rectangle<int> navigationBounds(const FireAudioProcessorEditor& editor) { return editor.navigationArea; }

    static void selectChannel(FireAudioProcessorEditor& editor, int id)
    { editor.spectrumChannelBox.setSelectedId(id, juce::sendNotificationSync); }

    static void showLibrary(FireAudioProcessorEditor& editor, bool show)
    { if (show) editor.showPresetBrowser(); else editor.hidePresetBrowser(); }

    static void collapseSpectrum(FireAudioProcessorEditor& editor, bool collapse)
    {
        editor.selectWorkspace(1, false);
        editor.spectrumCollapseButton.setToggleState(collapse, juce::dontSendNotification);
        editor.resized();
    }

    static void synchronise(FireAudioProcessorEditor& editor,
                            bool animate)
    {
        editor.synchroniseSpectrumHostBypassState(animate);
    }

    static void advanceIndicator(FireAudioProcessorEditor& editor,
                                 float deltaSeconds)
    {
        editor.hostBypassIndicatorOpacity.advance(deltaSeconds, 0.10f);
    }

    static void tickSpectra(FireAudioProcessorEditor& editor)
    {
        editor.processedSpectrum.timerCallback();
        editor.originalSpectrum.timerCallback();
    }

    static void suspendPresentation(FireAudioProcessorEditor& editor)
    {
        editor.suspendSpectrumPresentation();
    }

    static void snapIndicator(FireAudioProcessorEditor& editor,
                              float opacity)
    {
        editor.hostBypassIndicatorOpacity.snapTo(opacity);
    }

    static float indicatorOpacity(const FireAudioProcessorEditor& editor)
    {
        return editor.hostBypassIndicatorOpacity.current;
    }

    static float indicatorTarget(const FireAudioProcessorEditor& editor)
    {
        return editor.hostBypassIndicatorOpacity.target;
    }

    static bool processedSpectrumIsBypassed(
        const FireAudioProcessorEditor& editor)
    {
        return editor.processedSpectrum.hostBypassed;
    }

    static bool originalSpectrumIsBypassed(
        const FireAudioProcessorEditor& editor)
    {
        return editor.originalSpectrum.hostBypassed;
    }

    static bool spectraAwaitFreshFrames(
        const FireAudioProcessorEditor& editor)
    {
        return editor.processedSpectrum.awaitingFreshFrame
            && editor.originalSpectrum.awaitingFreshFrame;
    }

    static bool presentationIsSuspended(
        const FireAudioProcessorEditor& editor)
    {
        return editor.spectrumPresentationSuspended;
    }

    static std::uint64_t requiredCaptureEpoch(
        const FireAudioProcessorEditor& editor)
    {
        return editor.requiredFftCaptureEpoch;
    }

    static std::uint64_t processedPendingGeneration(
        const FireAudioProcessorEditor& editor)
    {
        return editor.processedSpectrum.pendingGeneration.load(
            std::memory_order_acquire);
    }

    static juce::Point<int> spectrumCentre(
        const FireAudioProcessorEditor& editor)
    {
        return editor.spectrumCardArea.getCentre();
    }
};

TEST_CASE("Editor propagates host bypass to both freshness-gated spectra",
          "[ui][editor][spectrum][host-bypass][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(48000.0, 512);
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        editor.removeFromDesktop();
    } };
    REQUIRE(editor.isShowing());

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> emptyBuffer;
    processor.processBlockBypassed(emptyBuffer, midi);
    SpectrumHostBypassPresentationTestAccess::synchronise(editor, true);

    CHECK(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::indicatorTarget(editor)
          == Catch::Approx(1.0f));

    SpectrumHostBypassPresentationTestAccess::advanceIndicator(editor, 0.05f);
    CHECK(SpectrumHostBypassPresentationTestAccess::indicatorOpacity(editor)
          > 0.0f);

    processor.processBlock(emptyBuffer, midi);
    SpectrumHostBypassPresentationTestAccess::synchronise(editor, true);
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::indicatorTarget(editor)
          == Catch::Approx(0.0f));
}

TEST_CASE("Host bypass status paints a themed overlay above the spectrum card",
          "[ui][editor][spectrum][host-bypass][visual][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);

    const auto centre =
        SpectrumHostBypassPresentationTestAccess::spectrumCentre(editor);
    REQUIRE(editor.getLocalBounds().contains(centre));

    juce::Image clearImage(juce::Image::ARGB,
                           editor.getWidth(),
                           editor.getHeight(),
                           true);
    {
        juce::Graphics clearGraphics(clearImage);
        SpectrumHostBypassPresentationTestAccess::snapIndicator(editor, 0.0f);
        editor.paintOverChildren(clearGraphics);
    }
    CHECK(clearImage.getPixelAt(centre.x, centre.y).getAlpha() == 0);

    juce::Image bypassImage(juce::Image::ARGB,
                            editor.getWidth(),
                            editor.getHeight(),
                            true);
    {
        juce::Graphics bypassGraphics(bypassImage);
        SpectrumHostBypassPresentationTestAccess::snapIndicator(editor, 1.0f);
        editor.paintOverChildren(bypassGraphics);
    }
    CHECK(bypassImage.getPixelAt(centre.x, centre.y).getAlpha() > 0);
}

TEST_CASE("Editor detects a complete host-bypass epoch between UI polls",
          "[ui][editor][spectrum][host-bypass][rapid][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(48000.0, 512);
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        editor.removeFromDesktop();
    } };
    REQUIRE(editor.isShowing());

    SpectrumHostBypassPresentationTestAccess::synchronise(editor, false);
    REQUIRE_FALSE(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));

    // No editor synchronisation occurs between these callbacks.  The final
    // boolean is unchanged, so only the monotonic processor epoch can reveal
    // that a complete bypass session crossed this presentation boundary.
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> emptyBuffer;
    processor.processBlockBypassed(emptyBuffer, midi);
    processor.processBlock(emptyBuffer, midi);
    REQUIRE_FALSE(processor.getBypassedState());

    SpectrumHostBypassPresentationTestAccess::synchronise(editor, true);

    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
}

TEST_CASE("A visible timer restores spectra suspended by a peer-detach fallback",
          "[ui][editor][spectrum][host-bypass][lifecycle][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(48000.0, 512);
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        editor.removeFromDesktop();
    } };
    REQUIRE(editor.isShowing());

    SpectrumHostBypassPresentationTestAccess::synchronise(editor, false);
    REQUIRE_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    REQUIRE_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));

    // The hidden-timer fallback can suspend presentation without a matching
    // visibilityChanged() callback when a host detaches its peer.  If the host
    // restores that peer just as silently, the next visible timer tick must
    // override the unchanged processor-bypass cache and reopen both inputs.
    SpectrumHostBypassPresentationTestAccess::suspendPresentation(editor);
    REQUIRE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    REQUIRE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));

    editor.timerCallback();

    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
}

TEST_CASE("Editor reattachment discards FFT frames completed while hidden",
          "[ui][editor][spectrum][lifecycle][freshness][hidden][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        editor.removeFromDesktop();
    } };
    REQUIRE(editor.isShowing());

    editor.setVisible(false);
    REQUIRE_FALSE(editor.isShowing());
    REQUIRE(SpectrumHostBypassPresentationTestAccess::presentationIsSuspended(
        editor));
    REQUIRE(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(
        editor));

    juce::AudioBuffer<float> hiddenFrame(
        1, SpectrumProcessor::fftSize);
    for (int sample = 0; sample < hiddenFrame.getNumSamples(); ++sample)
        hiddenFrame.setSample(0, sample, 9.0f);
    processor.pushDataPairToFFT(hiddenFrame, hiddenFrame);

    // No audio is published after the peer becomes visible. The hidden frame
    // must therefore leave both traces waiting rather than masquerading as a
    // post-reattachment update and remaining on screen indefinitely.
    editor.setVisible(true);
    REQUIRE(editor.isShowing());
    REQUIRE_FALSE(
        SpectrumHostBypassPresentationTestAccess::presentationIsSuspended(
            editor));
    const auto visibleCaptureEpoch =
        SpectrumHostBypassPresentationTestAccess::requiredCaptureEpoch(editor);
    REQUIRE(visibleCaptureEpoch > 0);
    editor.timerCallback();
    SpectrumHostBypassPresentationTestAccess::tickSpectra(editor);

    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(
        editor));
    REQUIRE_FALSE(
        SpectrumHostBypassPresentationTestAccess::presentationIsSuspended(
            editor));
    REQUIRE(
        SpectrumHostBypassPresentationTestAccess::requiredCaptureEpoch(editor)
        == visibleCaptureEpoch);

    juce::AudioBuffer<float> visibleFrame(
        1, SpectrumProcessor::fftSize);
    for (int sample = 0; sample < visibleFrame.getNumSamples(); ++sample)
        visibleFrame.setSample(0, sample, 4.0f);
    const auto pendingGenerationBeforeVisibleFrame =
        SpectrumHostBypassPresentationTestAccess::processedPendingGeneration(
            editor);
    processor.pushDataPairToFFT(visibleFrame, visibleFrame);
    editor.timerCallback();
    REQUIRE(
        SpectrumHostBypassPresentationTestAccess::processedPendingGeneration(
            editor)
        > pendingGenerationBeforeVisibleFrame);
    SpectrumHostBypassPresentationTestAccess::tickSpectra(editor);

    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(
        editor));
}

TEST_CASE("Spectrum demand follows source changes library and collapsed presentation",
          "[ui][editor][spectrum][capture-demand][source-epoch][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    const auto previousMode = processor.getAppSettings().getIntValue("spectrumChannel", 3);
    const juce::ScopeGuard restoreSetting { [&]
    {
        processor.getAppSettings().setValue("spectrumChannel", previousMode);
        processor.getAppSettings().saveIfNeeded();
    } };
    FireAudioProcessorEditor editor(processor);
    CHECK_FALSE(processor.isSpectrumCaptureEnabled());
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&] { editor.removeFromDesktop(); } };
    REQUIRE(processor.isSpectrumCaptureEnabled());
    std::vector<float> parameterValues;
    for(auto* parameter:processor.getParameters()) parameterValues.push_back(parameter->getValue());
    SpectrumHostBypassPresentationTestAccess::selectChannel(editor, 3);
    juce::AudioBuffer<float> frame(2, SpectrumProcessor::fftSize);
    for(int sample=0;sample<frame.getNumSamples();++sample)
    {frame.setSample(0,sample,0.2f);frame.setSample(1,sample,-0.1f);}
    for(int id:{2,1,3,4,5})
    {
        CAPTURE(id);
        processor.pushDataPairToFFT(frame,frame); // Pending frame belongs to the previous source.
        const auto originalEpoch=SpectrumHostBypassPresentationTestAccess::requiredCaptureEpoch(editor);
        SpectrumHostBypassPresentationTestAccess::selectChannel(editor,id);
        CHECK(SpectrumHostBypassPresentationTestAccess::requiredCaptureEpoch(editor)>originalEpoch);
        editor.timerCallback(); SpectrumHostBypassPresentationTestAccess::tickSpectra(editor);
        CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
        CHECK(processor.getAppSettings().getIntValue("spectrumChannel",0)==id);
        if(id>=4)
        {
            FireAudioProcessor reopenedProcessor; reopenedProcessor.hasUpdateCheckBeenPerformed=true;
            FireAudioProcessorEditor reopened(reopenedProcessor);
            CHECK(SpectrumHostBypassPresentationTestAccess::channel(reopened).getSelectedId()==id);
        }
        processor.pushDataPairToFFT(frame,frame);
        editor.timerCallback(); SpectrumHostBypassPresentationTestAccess::tickSpectra(editor);
        CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
    }
    for(int index=0;index<processor.getParameters().size();++index)
        CHECK(processor.getParameters()[index]->getValue()==parameterValues[static_cast<size_t>(index)]);
    SpectrumHostBypassPresentationTestAccess::showLibrary(editor, true);
    CHECK_FALSE(processor.isSpectrumCaptureEnabled());
    CHECK_FALSE(processor.isHistoryCaptureEnabled());
    SpectrumHostBypassPresentationTestAccess::showLibrary(editor, false);
    CHECK(processor.isSpectrumCaptureEnabled());
    SpectrumHostBypassPresentationTestAccess::collapseSpectrum(editor, true);
    CHECK_FALSE(processor.isSpectrumCaptureEnabled());
    SpectrumHostBypassPresentationTestAccess::collapseSpectrum(editor, false);
    CHECK(processor.isSpectrumCaptureEnabled());
    editor.setVisible(false);
    CHECK_FALSE(processor.isSpectrumCaptureEnabled());
    CHECK_FALSE(processor.isHistoryCaptureEnabled());
}


TEST_CASE("Spectrum toolbar stays aligned and reachable beside macros in every skin and scale",
          "[ui][editor][spectrum][toolbar][layout][skin][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed=true;
    FireAudioProcessorEditor editor(processor); editor.stopTimer();
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);editor.setVisible(true);
    const juce::ScopeGuard cleanup{[&] {editor.removeFromDesktop();}};
    auto* toolbar=editor.findChildWithID("spectrum_toolbar");REQUIRE(toolbar);
    auto* macros=editor.findChildWithID("performance_macros");REQUIRE(macros);
    auto& source=SpectrumHostBypassPresentationTestAccess::channel(editor);
    auto& zoom=SpectrumHostBypassPresentationTestAccess::zoom(editor);
    auto& collapse=SpectrumHostBypassPresentationTestAccess::collapse(editor);
    for(auto* control:std::array<juce::Component*,3>{&source,&zoom,&collapse})
        REQUIRE(control->getParentComponent()==&editor);
    CHECK(source.getItemText(source.indexOfItemId(4))=="Mono");
    CHECK(source.getItemText(source.indexOfItemId(5))=="Side");
    const auto click=[&](juce::Button& button,bool state)
    {
        REQUIRE(button.isShowing());REQUIRE(button.isEnabled());button.triggerClick();
        for(int elapsed=0;button.getToggleState()!=state && elapsed<1000;elapsed+=5)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        REQUIRE(button.getToggleState()==state);
    };
    const auto snapshot=[&](const juce::String& name)
    {
        if(const auto* directory=std::getenv("FIRE_SPECTRUM_TOOLBAR_PREVIEW_DIR"))
        {
            auto file=juce::File(directory).getChildFile(name+".png");REQUIRE(file.getParentDirectory().createDirectory().wasOk());
            auto stream=file.createOutputStream();REQUIRE(stream);stream->setPosition(0);stream->truncate();
            REQUIRE(juce::PNGImageFormat{}.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*stream));
        }
    };
    const auto checkExpanded=[&](bool canCollapse)
    {
        REQUIRE(toolbar->isShowing());REQUIRE(source.isShowing());REQUIRE(zoom.isShowing());
        CHECK(collapse.isShowing()==canCollapse);
        CHECK(SpectrumHostBypassPresentationTestAccess::spectrumBounds(editor).contains(toolbar->getBounds()));
        std::vector<juce::Component*> controls{&source,&zoom}; if(canCollapse) controls.push_back(&collapse);
        for(size_t i=0;i<controls.size();++i)
        {
            CAPTURE(controls[i]->getComponentID());
            CHECK(toolbar->getBounds().contains(controls[i]->getBounds()));
            CHECK(controls[i]->getHeight()>0);
            CHECK(controls[i]->getY()==source.getY());CHECK(controls[i]->getHeight()==source.getHeight());
            if(macros->isShowing()) CHECK_FALSE(controls[i]->getBounds().intersects(macros->getBounds()));
            for(size_t other=i+1;other<controls.size();++other)
                CHECK_FALSE(controls[i]->getBounds().intersects(controls[other]->getBounds()));
        }
        CHECK(zoom.getWidth()==zoom.getHeight());
        if(canCollapse) CHECK(collapse.getWidth()==collapse.getHeight());
    };
    for(auto skin:fire::ui::skins)
        for(int width:{1000,1400,2000})
        {
            CAPTURE(fire::ui::skinName(skin),width);
            SpectrumHostBypassPresentationTestAccess::applySkin(editor,skin);editor.setSize(width,width/2);
            for(int workspace:{0,2,1})
            {SpectrumHostBypassPresentationTestAccess::selectWorkspace(editor,workspace);checkExpanded(workspace==1);}
            const auto imagePrefix=juce::String(fire::ui::skinName(skin)).toLowerCase()+"-"+juce::String(width);
            snapshot(imagePrefix+"-mod-expanded");
            click(collapse,true);
            REQUIRE(collapse.isShowing());CHECK_FALSE(source.isShowing());CHECK_FALSE(zoom.isShowing());
            CHECK(toolbar->isShowing());CHECK(toolbar->getWidth()==toolbar->getHeight());
            CHECK(toolbar->getBounds().contains(collapse.getBounds()));
            CHECK(SpectrumHostBypassPresentationTestAccess::navigationBounds(editor).contains(toolbar->getBounds()));
            CHECK_FALSE(toolbar->getBounds().intersects(macros->getBounds()));
            CHECK(macros->isShowing());snapshot(imagePrefix+"-mod-collapsed");
            click(collapse,false);checkExpanded(true);
            click(zoom,true);checkExpanded(false);CHECK_FALSE(macros->isShowing());
            click(zoom,false);checkExpanded(true);CHECK(macros->isShowing());
        }
}
