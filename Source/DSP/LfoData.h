/*
  ==============================================================================

    LfoData.h
    Created: 28 Aug 2025 12:36:47am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

//
//  Represents the data model for a single LFO shape.
//  This ensures that every LFO always starts with a valid default state.
//
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

struct LfoData
{
    std::vector<juce::Point<float>> points;
    std::vector<float> curvatures;

    // Default constructor to initialize a valid shape
    LfoData()
    {
        points.push_back({ 0.0f, 0.0f });
        points.push_back({ 1.0f, 0.0f });
        curvatures.push_back(0.0f);
    }

    void resetToDefault()
    {
        points.clear();
        curvatures.clear();
        points.push_back({ 0.0f, 0.0f });
        points.push_back({ 1.0f, 0.0f });
        curvatures.push_back(0.0f); // English: One segment, so one curvature value.
    }

    // Writes the current LfoData to an XmlElement.
    void writeToXml(juce::XmlElement& xml) const
    {
        // Save points
        auto* pointsElement = xml.createNewChildElement("POINTS");
        for (const auto& point : points)
        {
            auto* p = pointsElement->createNewChildElement("P");
            p->setAttribute("x", point.x);
            p->setAttribute("y", point.y);
        }

        // Save curvatures
        auto* curvaturesElement = xml.createNewChildElement("CURVATURES");
        for (const auto& curvature : curvatures)
        {
            auto* c = curvaturesElement->createNewChildElement("C");
            c->setAttribute("v", curvature);
        }
    }

    // Creates an LfoData object from an XmlElement.
    static LfoData readFromXml(const juce::XmlElement& xml)
    {
        LfoData data;
        data.points.clear();
        data.curvatures.clear();

        // Load points
        if (auto* pointsElement = xml.getChildByName("POINTS"))
        {
            for (auto* p : pointsElement->getChildIterator())
            {
                data.points.push_back({ (float) p->getDoubleAttribute("x"),
                                        (float) p->getDoubleAttribute("y") });
            }
        }

        // Load curvatures
        if (auto* curvaturesElement = xml.getChildByName("CURVATURES"))
        {
            for (auto* c : curvaturesElement->getChildIterator())
            {
                data.curvatures.push_back((float) c->getDoubleAttribute("v"));
            }
        }

        // Basic data validation: if loading fails, return a default state.
        if (data.points.empty())
        {
            return {}; // Return default LfoData
        }

        return data;
    }

    /**
     * @brief Replaces a segment of the LFO with a new set of points.
     * This is the core logic for brush-based editing.
     * @param segmentIndex The index of the segment to replace (the curve between points[i] and points[i+1]).
     * @param newPoints A vector of new points to insert. Must contain at least 2 points.
    */
    void applyShapeToSegment(int segmentIndex, const std::vector<juce::Point<float>>& newPoints)
    {
        if (newPoints.size() < 2 || segmentIndex < 0 || segmentIndex >= points.size() - 1)
        {
            // English: Invalid input, do nothing.
            jassertfalse;
            return;
        }

        // English: The points to insert are all points from the new shape *except* the very first and very last one,
        // because they will replace the existing start and end points of the segment.
        std::vector<juce::Point<float>> pointsToInsert(newPoints.begin() + 1, newPoints.end() - 1);

        // English: Update the start and end points of the original segment.
        points[segmentIndex] = newPoints.front();
        points[segmentIndex + 1] = newPoints.back();

        // English: Insert the intermediate points, if any.
        if (! pointsToInsert.empty())
        {
            points.insert(points.begin() + segmentIndex + 1, pointsToInsert.begin(), pointsToInsert.end());
        }

        // English: Now, update the curvatures array to match the new points.
        // We set all new segments to have a linear (0.0) curvature.
        int numNewSegments = (int) newPoints.size() - 1;
        std::vector<float> newCurvatures(numNewSegments, 0.0f);

        // English: Replace the single old curvature value with the new set of curvatures.
        curvatures.erase(curvatures.begin() + segmentIndex);
        curvatures.insert(curvatures.begin() + segmentIndex, newCurvatures.begin(), newCurvatures.end());
    }
};
