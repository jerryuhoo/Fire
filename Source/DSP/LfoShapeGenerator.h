/*
  ==============================================================================

    LfoShapeGenerator.h
    Created: 17 Sep 2025 9:51:40pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once
#include "../Utility/Parameters.h"
#include "juce_core/juce_core.h"
#include <vector>

class LfoShapeGenerator
{
public:
    static std::vector<juce::Point<float>> generateShape(LfoPresetShape shape,
                                                         juce::Rectangle<float> cellBounds,
                                                         int numPoints = 16)
    {
        std::vector<juce::Point<float>> points;
        const float epsilon = 0.00001f; // A very small offset

        // English: Adjust the start and end X coordinates to be slightly inside the cell,
        // ONLY if they are NOT at the global boundaries.
        float effectiveStartX = cellBounds.getX();
        if (effectiveStartX > 0.0f) // This condition correctly handles the leftmost boundary
            effectiveStartX += epsilon;

        float effectiveEndX = cellBounds.getRight();
        if (effectiveEndX < 1.0f) // This condition correctly handles the rightmost boundary
            effectiveEndX -= epsilon;

        // English: Ensure start is always before end, even with epsilon.
        if (effectiveStartX >= effectiveEndX)
            effectiveEndX = effectiveStartX + epsilon;

        const float bottomY = cellBounds.getY();
        const float topY = cellBounds.getBottom();

        auto calculatePoint = [&](float t)
        {
            float x = effectiveStartX + t * (effectiveEndX - effectiveStartX);
            float y = 0.0f;
            switch (shape)
            {
                case LfoPresetShape::SawUp:
                    y = bottomY + t * (topY - bottomY);
                    break;
                case LfoPresetShape::SawDown:
                    y = topY - t * (topY - bottomY);
                    break;
                case LfoPresetShape::SineConvex:
                    y = bottomY + std::sin(t * juce::MathConstants<float>::pi) * (topY - bottomY);
                    break;
                case LfoPresetShape::SineConcave:
                    y = topY - std::sin(t * juce::MathConstants<float>::pi) * (topY - bottomY);
                    break;
                case LfoPresetShape::SquareHigh:
                    y = topY;
                    break;
                case LfoPresetShape::SquareLow:
                    y = bottomY;
                    break;
            }
            return juce::Point<float> { x, y };
        };

        bool isLinearShape = (shape == LfoPresetShape::SawUp || shape == LfoPresetShape::SawDown || shape == LfoPresetShape::SquareHigh || shape == LfoPresetShape::SquareLow);

        if (isLinearShape)
        {
            points.push_back(calculatePoint(0.0f));
            points.push_back(calculatePoint(1.0f));
        }
        else
        {
            for (int i = 0; i < numPoints; ++i)
                points.push_back(calculatePoint((float) i / (float) (numPoints - 1)));
        }

        return points;
    }
};