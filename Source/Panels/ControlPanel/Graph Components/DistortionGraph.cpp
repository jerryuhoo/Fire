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
    setGraphIdentity("TRANSFER", fire::ui::ModuleRole::shape);
    updateDistortionCurve();
}

DistortionGraph::~DistortionGraph() = default;

void DistortionGraph::resized()
{
    GraphTemplate::resized();
    curveDirty = true;
    if (isShowing())
        updateDistortionCurve();
}

void DistortionGraph::paint(juce::Graphics& g)
{
    GraphTemplate::paint(g);

    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(getGraphPlotBounds().getSmallestIntegerContainer());

    g.setColour(getGraphAccent().withAlpha(0.16f));
    g.strokePath(distortionCurve,
                 juce::PathStrokeType(6.0f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    g.setGradientFill(curveGradient);
    g.strokePath(distortionCurve,
                 juce::PathStrokeType(1.8f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
}

void DistortionGraph::visibilityChanged()
{
    GraphTemplate::visibilityChanged();
    if (isShowing() && curveDirty)
        updateDistortionCurve();
}

void DistortionGraph::graphShowingStateChanged(bool isNowShowing)
{
    if (isNowShowing && curveDirty)
        updateDistortionCurve();
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
    {
        if (curveDirty && isShowing())
            updateDistortionCurve();
        return;
    }

    mode = newMode;
    rec = newRec;
    mix = newMix;
    bias = newBias;
    drive = newDrive;
    curveDirty = true;
    if (isShowing())
        updateDistortionCurve();
}

void DistortionGraph::updateDistortionCurve()
{
    distortionCurve.clear();
    const auto plotBounds = getGraphPlotBounds();

    // Create a single state object for the graph using the parameters passed to this component.
    DistortionLogic::State graphState;
    graphState.drive = this->drive;
    graphState.bias = this->bias;
    graphState.rec = this->rec;
    graphState.mode = this->mode;

    const int numPix = juce::roundToInt(plotBounds.getWidth());
    if (numPix <= 0)
    {
        curveDirty = true;
        return;
    }

    distortionCurve.preallocateSpace(numPix * 3);

    curveGradient = juce::ColourGradient(fire::ui::colours::whiteHot,
                                         plotBounds.getCentreX(),
                                         plotBounds.getCentreY(),
                                         getGraphAccent().withAlpha(0.72f),
                                         plotBounds.getX(),
                                         plotBounds.getCentreY(),
                                         true);
    curveGradient.addColour(0.58, fire::ui::colours::gold);

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
        const float xPos = juce::jmap(static_cast<float>(i),
                                     0.0f,
                                     static_cast<float>(juce::jmax(1, numPix - 1)),
                                     plotBounds.getX(),
                                     plotBounds.getRight());
        const float yPos = juce::jmap(mixedValue,
                                     maxInput,
                                     -maxInput,
                                     plotBounds.getY(),
                                     plotBounds.getBottom());

        if (i == 0)
            distortionCurve.startNewSubPath(xPos, yPos);
        else
            distortionCurve.lineTo(xPos, yPos);

        input += inputInc;
    }

    curveDirty = false;
    repaint();
}
