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
                                                         juce::Rectangle<float> cellBounds)
    {
        std::vector<juce::Point<float>> points;
        const float epsilon = 0.00001f;

        float effectiveStartX = cellBounds.getX();
        if (effectiveStartX > 0.0f)
            effectiveStartX += epsilon;

        float effectiveEndX = cellBounds.getRight();
        if (effectiveEndX < 1.0f)
            effectiveEndX -= epsilon;

        if (effectiveStartX >= effectiveEndX)
            effectiveEndX = effectiveStartX + epsilon;

        const float bottomY = cellBounds.getY();
        const float topY = cellBounds.getBottom();
        const float midX = effectiveStartX + (effectiveEndX - effectiveStartX) * 0.5f;

        // Sine shapes now generate 3 points
        bool isSineShape = (shape == LfoPresetShape::SineConvex || shape == LfoPresetShape::SineConcave);

        if (isSineShape)
        {
            if (shape == LfoPresetShape::SineConvex)
            {
                points.push_back({ effectiveStartX, bottomY });
                points.push_back({ midX, topY });
                points.push_back({ effectiveEndX, bottomY });
            }
            else // SineConcave
            {
                points.push_back({ effectiveStartX, topY });
                points.push_back({ midX, bottomY });
                points.push_back({ effectiveEndX, topY });
            }
        }
        else // All other shapes are linear and only need two points
        {
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
                    case LfoPresetShape::SquareHigh:
                        y = topY;
                        break;
                    case LfoPresetShape::SquareLow:
                        y = bottomY;
                        break;
                    default:
                        break; // Should not happen for linear shapes
                }
                return juce::Point<float> { x, y };
            };
            points.push_back(calculatePoint(0.0f));
            points.push_back(calculatePoint(1.0f));
        }

        return points;
    }
};