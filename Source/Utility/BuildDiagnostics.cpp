#include "BuildDiagnostics.h"

juce::String fire::diagnostics::buildIdentifier()
{
    // Keep the compile timestamp in one translation unit so all callers report
    // the same identifier, including non-CMake (Projucer) builds.
    return juce::String(FIRE_BUILD_REVISION) + " / " + FIRE_BUILD_CONFIGURATION
        + " / " + __DATE__ + " " + __TIME__;
}
