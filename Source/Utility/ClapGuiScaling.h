#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fire::utility
{
// This CLAP adapter already expresses wrapper bounds in physical pixels by
// applying the host scale to the editor. JUCE 9 embedded peers also inherit
// native DPI, so leave that second layer at 1:1. Updating the native bounds is
// essential: addToDesktop() has already created the window at the old scale.
// See free-audio/clap-juce-extensions#183 and Fire#101.
inline void configureClapPhysicalPixelPeer(juce::Component& wrapper)
{
#if JUCE_WINDOWS || JUCE_LINUX
    if (auto* peer = wrapper.getPeer())
    {
        peer->setCustomPlatformScaleFactor(1.0);
        peer->updateBounds();
    }
#else
    // Cocoa's CLAP window sizes are logical points, not physical pixels.
    juce::ignoreUnused(wrapper);
#endif
}
}
