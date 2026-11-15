#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <Utility/ClapGuiScaling.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

TEST_CASE("CLAP host scale preserves logical editor size and fractional resize bounds",
          "[clap][ui][dpi][regression]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    juce::Component wrapper;
    FireAudioProcessorEditor editor(processor);
    wrapper.addAndMakeVisible(editor);
    for (const auto logicalSize : {juce::Point<int>{1000, 500}, juce::Point<int>{1600, 800}})
    {
        editor.setSize(logicalSize.x, logicalSize.y);
        for (const float scale : {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 1.25f, 1.0f})
        {
            CAPTURE(scale, logicalSize.x);
            editor.setScaleFactor(scale);
            const auto rendered = wrapper.getLocalArea(&editor, editor.getLocalBounds());
            CHECK(rendered.getWidth() == juce::roundToInt(logicalSize.x * scale));
            CHECK(rendered.getHeight() == juce::roundToInt(logicalSize.y * scale));
            CHECK(editor.getWidth() == logicalSize.x);
            CHECK(editor.getHeight() == logicalSize.y);
            CHECK(processor.getSavedEditorSize().width == logicalSize.x);
            CHECK(processor.getSavedEditorSize().height == logicalSize.y);
        }
    }
}

#if JUCE_WINDOWS
TEST_CASE("CLAP Windows peer uses host pixels once at fractional native DPI",
          "[clap][ui][dpi][windows][regression]")
{
    // Model the JUCE 9 peer scale inherited from a Windows host, independent
    // of the CI display's actual DPI. Exercise real HWND client dimensions.
    juce::Component wrapper, content;
    wrapper.addAndMakeVisible(content);
    // Multiples of 16 isolate duplicate scaling from native subpixel rounding.
    content.setSize(1024, 512);
    wrapper.setSize(1024, 512);
    wrapper.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    auto* peer = wrapper.getPeer();
    REQUIRE(peer != nullptr);
    const auto clientSize = [&]
    {
        RECT area {};
        REQUIRE(GetClientRect(static_cast<HWND>(wrapper.getWindowHandle()), &area) != 0);
        return juce::Point<int>{static_cast<int>(area.right - area.left),
                                static_cast<int>(area.bottom - area.top)};
    };
    for (const double nativeScale : {1.25, 1.5, 2.0})
    {
        for (const float hostScale : {1.0f, 1.25f, 1.5f, 2.0f})
        {
            CAPTURE(nativeScale, hostScale);
            content.setTransform(juce::AffineTransform::scale(hostScale));
            const auto hostPixels = wrapper.getLocalArea(&content, content.getLocalBounds());
            const juce::Point<int> expectedSize {hostPixels.getWidth(), hostPixels.getHeight()};
            wrapper.setSize(hostPixels.getWidth(), hostPixels.getHeight());
            peer->setCustomPlatformScaleFactor(nativeScale);
            peer->updateBounds();
            REQUIRE(clientSize().x > hostPixels.getWidth());

            fire::utility::configureClapPhysicalPixelPeer(wrapper);
            CHECK(peer->getPlatformScaleFactor() == Catch::Approx(1.0));
            CHECK(clientSize() == expectedSize);
            // Native resize notifications must not reintroduce the old bounds.
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
            CHECK(clientSize() == expectedSize);
            CHECK(wrapper.getWidth() == expectedSize.x);
            CHECK(wrapper.getHeight() == expectedSize.y);
        }
    }
}
#endif
