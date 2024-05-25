#pragma once
//#include "InspectorBinaryData.h" // uncomment when adding assets via CMake
#include "../../LatestCompiledAssets/InspectorBinaryData.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace melatonin
{
    static inline juce::String dimensionsString (juce::Rectangle<int> bounds)
    {
        // account for outline border drawing (1 on each side)
        return juce::String (bounds.getWidth() - 2) + L" × " + juce::String (bounds.getHeight() - 2);
    }

    static inline juce::String distanceString (juce::Line<float> line)
    {
        // account for outline border drawing (1 on each side)
        auto v = line.getLength() + 3;
        return juce::String (v);
    }

    static inline juce::Image getIcon (const juce::String& iconName)
    {
        int size = 0;
        auto filename = iconName.replace (" ", "") + "_png";
        auto data = InspectorBinaryData::getNamedResource (filename.getCharPointer(), size);
        if (size > 0)
        {
            return juce::ImageCache::getFromMemory (data, size);
        }
        else
        {
            // you told me to load an image that doesn't exist
            jassertfalse;
            return {};
        }
    }

}
