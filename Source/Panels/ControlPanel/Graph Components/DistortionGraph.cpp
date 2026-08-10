/*
  ==============================================================================

    DistortionGraph.cpp
    Created: 29 Nov 2020 10:31:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "DistortionGraph.h"
#include "../../../DSP/DistortionLogic.h"

//==============================================================================
DistortionGraph::DistortionGraph(FireAudioProcessor& p)
{
    juce::ignoreUnused(p);
    updateDistortionCurve();
}

DistortionGraph::~DistortionGraph() = default;

void DistortionGraph::resized()
{
    updateDistortionCurve();
}

void DistortionGraph::paint(juce::Graphics& g)
{
    // The paint function is now very lightweight. It only draws the pre-calculated path.
    auto frameRight = getLocalBounds();

    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds(), 1);

    // Create the gradient and draw the stored path
    juce::ColourGradient grad(SHAPE_COLOUR.withBrightness(0.9f),
                              static_cast<float>(frameRight.getCentreX()),
                              static_cast<float>(frameRight.getCentreY()),
                              juce::Colours::yellow.withBrightness(0.9f).withAlpha(0.0f),
                              static_cast<float>(frameRight.getX()),
                              static_cast<float>(frameRight.getCentreY()),
                              true);
    g.setGradientFill(grad);
    g.strokePath(distortionCurve, juce::PathStrokeType(2.0f));
}

void DistortionGraph::setState(int newMode,
                               float newRec,
                               float newMix,
                               float newBias,
                               float newDrive,
                               float newRateDivide)
{
    juce::ignoreUnused(newRateDivide);
    if (mode == newMode
        && juce::approximatelyEqual(rec, newRec)
        && juce::approximatelyEqual(mix, newMix)
        && juce::approximatelyEqual(bias, newBias)
        && juce::approximatelyEqual(drive, newDrive))
        return;

    mode = newMode;
    rec = newRec;
    mix = newMix;
    bias = newBias;
    drive = newDrive;
    updateDistortionCurve();
}

void DistortionGraph::updateDistortionCurve()
{
    distortionCurve.clear();
    auto frameRight = getLocalBounds();

    // Create a single state object for the graph using the parameters passed to this component.
    DistortionLogic::State graphState;
    graphState.drive = this->drive;
    graphState.bias = this->bias;
    graphState.rec = this->rec;
    graphState.mode = this->mode;

    const int numPix = frameRight.getWidth();
    if (numPix <= 0)
        return;

    float maxInput = 2.0f; // Max input value for the graph's x-axis
    float input = -maxInput;
    const float inputInc = (maxInput * 2.0f) / numPix;

    for (int i = 0; i < numPix; ++i)
    {
        // Calculate the output using the shared processing function.
        float wetValue = DistortionLogic::processSample(input, graphState);

        // The mix logic is specific to the graph, so we keep it here.
        float mixedValue = (1.0f - this->mix) * input + this->mix * wetValue;

        // Map the input and output values to screen coordinates
        float xPos = juce::jmap((float) i, 0.0f, (float) numPix, (float) frameRight.getX(), (float) frameRight.getRight());
        float yPos = juce::jmap(mixedValue, maxInput, -maxInput, (float) frameRight.getY(), (float) frameRight.getBottom());

        if (i == 0)
            distortionCurve.startNewSubPath(xPos, yPos);
        else
            distortionCurve.lineTo(xPos, yPos);

        input += inputInc;
    }

    repaint();
}
