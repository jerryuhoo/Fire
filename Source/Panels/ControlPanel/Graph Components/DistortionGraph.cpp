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
DistortionGraph::DistortionGraph(FireAudioProcessor& p) : processor(p)
{
    const auto& params = processor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    }
    updateDistortionCurve();
}

DistortionGraph::~DistortionGraph()
{
    const auto& params = processor.getParameters();
    for (auto param : params)
    {
        param->removeListener(this);
    }
}

void DistortionGraph::paint(juce::Graphics& g)
{
    // The paint function is now very lightweight. It only draws the pre-calculated path.
    auto frameRight = getLocalBounds();

    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds(), 1);

    // Create the gradient and draw the stored path
    juce::ColourGradient grad(SHAPE_COLOUR.withBrightness(0.9), frameRight.getX() + frameRight.getWidth() / 2, frameRight.getY() + frameRight.getHeight() / 2, juce::Colours::yellow.withBrightness(0.9).withAlpha(0.0f), frameRight.getX(), frameRight.getY() + frameRight.getHeight() / 2, true);
    g.setGradientFill(grad);
    g.strokePath(distortionCurve, juce::PathStrokeType(2.0f));

    if (isMouseOn && ! mZoomState)
    {
        g.setColour(juce::Colours::yellow.withAlpha(0.05f));
        g.fillAll();
    }
}

void DistortionGraph::setState(int mode, float rec, float mix, float bias, float drive, float rateDivide)
{
    this->mode = mode;
    this->rec = rec;
    this->mix = mix;
    this->bias = bias;
    this->drive = drive;
    this->rateDivide = rateDivide;
    updateDistortionCurve();
}

void DistortionGraph::parameterValueChanged(int parameterIndex, float newValue)
{
    // When a parameter changes (on ANY thread), we simply trigger an async update.
    // This is a very lightweight, non-blocking, and thread-safe call.
    triggerAsyncUpdate();
}

void DistortionGraph::handleAsyncUpdate()
{
    // This function is guaranteed to be called on the message thread.
    // All UI updates are now safely and efficiently handled here.
    updateDistortionCurve();
}

void DistortionGraph::updateDistortionCurve()
{
    distortionCurve.clear();
    auto frameRight = getLocalBounds();

    // <<<< 2. USE THE SHARED LOGIC >>>>
    // Create a single state object for the graph using the parameters passed to this component.
    DistortionLogic::State graphState;
    graphState.drive = this->drive;
    graphState.bias = this->bias;
    graphState.rec = this->rec;
    graphState.mode = this->mode;

    const int numPix = frameRight.getWidth();
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