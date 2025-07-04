/*
  ==============================================================================

    DistortionGraph.cpp
    Created: 29 Nov 2020 10:31:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "DistortionGraph.h"

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
    // Clear the old path before calculating the new one.
    distortionCurve.clear();

    auto frameRight = getLocalBounds();

    // Symmetrical distortion drawing logic, migrated from your original paint() function.
    if (mode <= 15)
    {
        float functionValue = 0.0f;
        float mixValue;

        const int numPix = frameRight.getWidth();
        float driveScale = 1.0f;
        float maxValue = 2.0f * driveScale * mix + 2.0f * (1.0f - mix);
        float value = -maxValue;
        float valInc = (maxValue - value) / numPix;
        float xPos = frameRight.getX();
        const float posInc = (float) frameRight.getWidth() / (float) numPix;

        bool edgePointL = false;
        bool edgePointR = false;
        float step = 0.5f;

        for (float i = 1; i < numPix; i += step)
        {
            value += valInc * step;
            xPos += posInc * step;

            float valueAfterDrive = value;

            // downsample
            if (rateDivide > 1.0f)
            {
                valueAfterDrive = ceilf(valueAfterDrive / (rateDivide / 256.0f)) * (rateDivide / 256.0f);
            }

            valueAfterDrive = valueAfterDrive * drive + bias;

            switch (mode)
            {
                case 0:
                    functionValue = waveshaping::arctanSoftClipping(valueAfterDrive);
                    break;
                case 1:
                    functionValue = waveshaping::expSoftClipping(valueAfterDrive);
                    break;
                case 2:
                    functionValue = waveshaping::tanhSoftClipping(valueAfterDrive);
                    break;
                case 3:
                    functionValue = waveshaping::cubicSoftClipping(valueAfterDrive);
                    break;
                case 4:
                    functionValue = waveshaping::hardClipping(valueAfterDrive);
                    break;
                case 5:
                    functionValue = waveshaping::sausageFattener(valueAfterDrive);
                    break;
                case 6:
                    functionValue = waveshaping::sinFoldback(valueAfterDrive);
                    break;
                case 7:
                    functionValue = waveshaping::linFoldback(valueAfterDrive);
                    break;
                case 8:
                    functionValue = waveshaping::limitClip(valueAfterDrive);
                    break;
                case 9:
                    functionValue = waveshaping::singleSinClip(valueAfterDrive);
                    break;
                case 10:
                    functionValue = waveshaping::logicClip(valueAfterDrive);
                    break;
                case 11:
                    functionValue = waveshaping::tanclip(valueAfterDrive);
                    break;
            }

            functionValue = waveshaping::rectificationProcess(functionValue, rec);
            functionValue = (1.0f - mix) * value + mix * functionValue;
            mixValue = (2.0f / 3.0f) * functionValue;
            float yPos = frameRight.getCentreY() - frameRight.getHeight() * mixValue / 2.0f;

            if (yPos < frameRight.getY())
            {
                if (edgePointR == false)
                {
                    yPos = frameRight.getY();
                    edgePointR = true;
                }
                else
                {
                    continue;
                }
            }

            if (yPos > frameRight.getBottom())
            {
                if (edgePointL == false)
                {
                    continue;
                }
                else
                {
                    yPos = frameRight.getBottom();
                }
            }
            else if (edgePointL == false)
            {
                if (mode == 0)
                {
                    distortionCurve.startNewSubPath(xPos, frameRight.getBottom());
                    distortionCurve.lineTo(xPos, yPos);
                }
                else
                {
                    distortionCurve.startNewSubPath(xPos, yPos);
                }
                edgePointL = true;
            }
            distortionCurve.lineTo(xPos, yPos);
        }
    }
    repaint();
}
