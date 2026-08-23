/*
  ==============================================================================

    DistortionGraph.h
    Created: 29 Nov 2020 10:31:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../PluginProcessor.h"
#include "../../../GUI/LookAndFeel.h"
#include "../../../DSP/ClippingFunctions.h"
#include "GraphTemplate.h"

struct DistortionGraphSourceEpochTestAccess;

//==============================================================================
/*
*/
class DistortionGraph : public GraphTemplate
{
public:
    DistortionGraph(FireAudioProcessor&);
    ~DistortionGraph() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void setState(int mode, float rec, float mix, float bias, float drive, float rateDivide);

private:
    friend struct DistortionGraphSourceEpochTestAccess;

    int mode = 0;
    float rec = 0.0f;
    float mix = 1.0f;
    float bias = 0.0f;
    float drive = 1.0f;
    juce::Path distortionCurve;
    juce::ColourGradient curveGradient;
    bool curveDirty = true;
    void updateDistortionCurve();
};
