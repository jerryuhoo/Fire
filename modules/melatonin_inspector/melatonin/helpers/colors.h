#pragma once
#include "colour_ids.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace melatonin::colors
{
    const juce::Colour white = juce::Colours::white;
    const juce::Colour black = juce::Colour::fromRGB (0, 0, 0);

    // background & panel
    const juce::Colour background = juce::Colour::fromRGB (43, 29, 50);
    const juce::Colour headerBackground = juce::Colour::fromRGB (28, 14, 34);
    const juce::Colour panelBackgroundLighter = juce::Colour::fromRGB (45, 22, 55);
    const juce::Colour panelBackgroundDarker = juce::Colour::fromRGB (38, 18, 52);
    const juce::Colour panelLineSeparator = juce::Colour::fromRGB (12, 7, 19);
    const juce::Colour disclosure = juce::Colour::fromRGB (154, 190, 255);
    const juce::Colour scrollbar = juce::Colour::fromRGB (154, 190, 255);
    const juce::Colour scrollbarBackground = juce::Colour::fromRGB (12, 17, 9);
    const juce::Colour highlight = juce::Colour::fromRGB (255, 229, 138);

    // text
    const juce::Colour text = juce::Colour::fromRGB (118, 143, 190);
    const juce::Colour label = juce::Colour::fromRGB (131, 146, 175);
    const juce::Colour highlightedText = juce::Colour::fromRGB (55, 55, 55);
    const juce::Colour caret = juce::Colour::fromRGB (20, 157, 249);
    const juce::Colour searchText = juce::Colour::fromRGB (92, 82, 111);

    // tree
    const juce::Colour treeBackgroundLighter = juce::Colour::fromRGB (34, 17, 46);
    const juce::Colour treeBackgroundDarker = juce::Colour::fromRGB (24, 10, 35);
    const juce::Colour treeItemTextSelected = juce::Colour::fromRGB (206, 234, 255);
    const juce::Colour treeItemTextDisabled = juce::Colour::fromRGBA (119, 111, 129, 165);
    const juce::Colour treeViewMinusPlusColor = juce::Colour::fromRGB (119, 111, 129);

    // props
    const juce::Colour propertyName = text;
    const juce::Colour propertyValue = juce::Colour::fromRGB (206, 235, 255);
    const juce::Colour checkboxCheck = juce::Colour::fromRGB (41, 11, 79);
    const juce::Colour customBackground = juce::Colour::fromRGB (25, 11, 36).darker();
    const juce::Colour propertyValueWarn = juce::Colour::fromRGB (255, 117, 117);
    const juce::Colour propertyValueError = juce::Colour::fromRGB (255, 51, 51);
    const juce::Colour propertyValueDisabled = treeItemTextDisabled;
    const juce::Colour iconOff = juce::Colour::fromRGB (95, 82, 111);

    // overlay
    const juce::Colour overlayBoundingBox = juce::Colour::fromRGB (66, 157, 226);
    const juce::Colour overlayLabelBackground = juce::Colour::fromRGB (20, 157, 249);
    const juce::Colour boxModelBoundingBox = juce::Colour::fromRGB (66, 157, 226);
    const juce::Colour overlayDistanceToHovered = juce::Colour::fromRGB (212, 86, 63);

    const juce::Colour checkerDark = juce::Colour::fromRGB (51, 51, 51);
    const juce::Colour checkerLight = juce::Colour::fromRGB (104, 104, 104);

    static inline juce::String rgbaString (juce::Colour& color)
    {
        if (color.getAlpha() < 255)
            return juce::String::formatted ("%d, %d, %d, %d",
                color.getRed(),
                color.getGreen(),
                color.getBlue(),
                color.getAlpha());
        else
            return juce::String::formatted ("%d, %d, %d",
                color.getRed(),
                color.getGreen(),
                color.getBlue());
    }

    static inline juce::String hexString (juce::Colour& color)
    {
        return color.toDisplayString (true);
    }

    // juce::ColourHelpers is an internal implementation, this is copied
    static inline bool areContrasting (const juce::Colour& c1, const juce::Colour& c2, float minContrast = 0.5f)
    {
        auto c1Luma = 0.2999f * c1.getFloatRed() + 0.5870f * c1.getFloatGreen() + 0.1140f * c1.getFloatBlue();
        auto c2Luma = 0.2999f * c2.getFloatRed() + 0.5870f * c2.getFloatGreen() + 0.1140f * c2.getFloatBlue();

        if (std::abs (c1Luma - c2Luma) >= minContrast)
            return true;

        return false;
    }

    static inline juce::String enumNameIfPresent (juce::String idString)
    {
        if (!idString.startsWith ("jcclr_"))
            return idString; // or handle the error as needed

        int colourId = idString.substring (6).getHexValue32();

        for (const auto& mapping : colourIdNames)
            if (mapping.value == colourId)
                return mapping.name;

        return idString;
    }
}
