/*
  ==============================================================================

    LfoData.h
    Created: 28 Aug 2025 12:36:47am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <cmath>
#include <vector>

//
//  Represents the data model for a single LFO shape.
//  This ensures that every LFO always starts with a valid default state.
//
struct LfoData
{
    static constexpr size_t maximumNumberOfPoints = 64;

    std::vector<juce::Point<float>> points;
    std::vector<float> curvatures;
    float smoothness = 0.0f; // Add smoothness property, defaulting to 0 (no smoothing).

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
        curvatures.push_back(0.0f); // One segment, so one curvature value.
        smoothness = 0.0f; // Reset smoothness to default.
    }

    /** Keeps state loaded from presets (or supplied by callers) safe for DSP use. */
    void sanitise()
    {
        if (! std::isfinite(smoothness))
            smoothness = 0.0f;
        smoothness = juce::jlimit(0.0f, 1.0f, smoothness);

        if (points.size() < 2)
        {
            resetToDefault();
            return;
        }

        if (points.size() > maximumNumberOfPoints)
        {
            const size_t originalLastIndex = points.size() - 1;
            constexpr size_t reducedLastIndex = maximumNumberOfPoints - 1;
            const size_t indexStep = originalLastIndex / reducedLastIndex;
            const size_t indexRemainder = originalLastIndex % reducedLastIndex;

            std::vector<juce::Point<float>> reducedPoints;
            std::vector<float> reducedCurvatures;
            reducedPoints.reserve(maximumNumberOfPoints);
            reducedCurvatures.reserve(maximumNumberOfPoints - 1);

            for (size_t i = 0; i < maximumNumberOfPoints; ++i)
            {
                const size_t sourceIndex = indexStep * i + (indexRemainder * i) / reducedLastIndex;
                reducedPoints.push_back(points[sourceIndex]);

                if (i + 1 < maximumNumberOfPoints)
                    reducedCurvatures.push_back(sourceIndex < curvatures.size()
                                                    ? curvatures[sourceIndex]
                                                    : 0.0f);
            }

            points = std::move(reducedPoints);
            curvatures = std::move(reducedCurvatures);
        }

        for (auto& point : points)
        {
            if (! std::isfinite(point.x) || ! std::isfinite(point.y))
            {
                resetToDefault();
                return;
            }

            point.x = juce::jlimit(0.0f, 1.0f, point.x);
            point.y = juce::jlimit(0.0f, 1.0f, point.y);
        }

        if (! std::is_sorted(points.begin(), points.end(), [](const auto& lhs, const auto& rhs)
                             { return lhs.x < rhs.x; }))
        {
            std::stable_sort(points.begin(), points.end(), [](const auto& lhs, const auto& rhs)
                             { return lhs.x < rhs.x; });
            curvatures.assign(points.size() - 1, 0.0f);
        }
        else
        {
            curvatures.resize(points.size() - 1, 0.0f);
        }

        // A cyclic LFO must cover the complete phase domain. Preserve endpoint levels,
        // but prevent malformed presets from leaving large uninitialised phase regions.
        points.front().x = 0.0f;
        points.back().x = 1.0f;

        for (auto& curvature : curvatures)
        {
            if (! std::isfinite(curvature))
                curvature = 0.0f;
            curvature = juce::jlimit(-2.0f, 2.0f, curvature);
        }
    }

    /**
     * @brief Removes duplicate or very close points to clean up the LFO shape.
     * It uses an epsilon for robust floating-point comparison and also ensures
     * the 'curvatures' array remains synchronized with the 'points' array.
    */
    void mergeDuplicatePoints()
    {
        // Do nothing if there are not enough points to have duplicates.
        if (points.size() < 2)
            return;

        // A small tolerance to consider two float values as equal.
        constexpr float epsilon = 0.0001f;

        // Create new vectors to store the unique points and their corresponding curvatures.
        std::vector<juce::Point<float>> uniquePoints;
        std::vector<float> updatedCurvatures;

        // Always add the first point.
        uniquePoints.push_back(points.front());

        // Iterate through the rest of the points, starting from the second one.
        for (size_t i = 1; i < points.size(); ++i)
        {
            // Compare the distance from the current point to the last unique point found.
            if (points[i].getDistanceFrom(uniquePoints.back()) > epsilon)
            {
                // If the point is not a duplicate, add it to the unique list.
                uniquePoints.push_back(points[i]);

                // IMPORTANT: Also add the curvature of the segment *preceding* this new point.
                // The number of curvatures is always one less than the number of points.
                // So, the curvature at index i-1 corresponds to the segment between point i-1 and i.
                if (i - 1 < curvatures.size())
                    updatedCurvatures.push_back(curvatures[i - 1]);
                else
                    updatedCurvatures.push_back(0.0f);
            }
        }

        // After checking all points, replace the old data with the cleaned-up versions.
        if (uniquePoints.size() < 2)
        {
            resetToDefault();
            return;
        }

        points = std::move(uniquePoints);
        curvatures = std::move(updatedCurvatures);
        sanitise();
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

        // Save the new smoothness attribute directly to the main element.
        xml.setAttribute("smoothness", smoothness);
    }

    // Creates an LfoData object from an XmlElement.
    static LfoData readFromXml(const juce::XmlElement& xml)
    {
        LfoData data;
        data.points.clear();
        data.curvatures.clear();

        // Older brush builds could serialise more than the current 64-point
        // DSP limit. Read a bounded, evenly distributed view so sanitise()
        // preserves the complete curve instead of keeping only its first 64
        // points and stretching that truncated endpoint to phase 1.0.
        constexpr size_t maximumLegacyPointsToRead = maximumNumberOfPoints * 64;

        const auto forEachSampledChild = [](const juce::XmlElement& parent,
                                            size_t maximumChildren,
                                            auto&& callback)
        {
            const int totalChildren = juce::jmax(0, parent.getNumChildElements());
            const size_t count = juce::jmin(static_cast<size_t>(totalChildren),
                                             maximumChildren);
            if (count == 0)
                return;

            for (size_t i = 0; i < count; ++i)
            {
                const size_t sourceIndex = count == 1
                                               ? 0
                                               : (static_cast<size_t>(totalChildren - 1) * i)
                                                     / (count - 1);
                if (const auto* child = parent.getChildElement(static_cast<int>(sourceIndex)))
                    callback(*child);
            }
        };

        // Load points
        if (auto* pointsElement = xml.getChildByName("POINTS"))
        {
            data.points.reserve(juce::jmin(maximumLegacyPointsToRead,
                                            static_cast<size_t>(juce::jmax(
                                                0, pointsElement->getNumChildElements()))));
            forEachSampledChild(*pointsElement, maximumLegacyPointsToRead, [&](const auto& p)
            {
                data.points.push_back({ (float) p.getDoubleAttribute("x"),
                                        (float) p.getDoubleAttribute("y") });
            });
        }

        // Load curvatures
        if (auto* curvaturesElement = xml.getChildByName("CURVATURES"))
        {
            constexpr size_t maximumLegacyCurvaturesToRead = maximumLegacyPointsToRead - 1;
            data.curvatures.reserve(juce::jmin(maximumLegacyCurvaturesToRead,
                                                static_cast<size_t>(juce::jmax(
                                                    0, curvaturesElement->getNumChildElements()))));
            forEachSampledChild(*curvaturesElement, maximumLegacyCurvaturesToRead, [&](const auto& c)
            {
                data.curvatures.push_back((float) c.getDoubleAttribute("v"));
            });
        }

        // Load smoothness, providing a default value of 0.0 if the attribute doesn't exist.
        data.smoothness = (float) xml.getDoubleAttribute("smoothness", 0.0);

        data.sanitise();

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
        const bool replacementWouldExceedLimit = points.size() > maximumNumberOfPoints
                                                 || (points.size() >= 2
                                                     && newPoints.size() > maximumNumberOfPoints - (points.size() - 2));
        if (newPoints.size() < 2 || points.size() < 2 || replacementWouldExceedLimit || segmentIndex < 0
            || static_cast<size_t>(segmentIndex) >= points.size() - 1)
        {
            // Invalid input, do nothing.
            jassertfalse;
            return;
        }

        for (size_t i = 0; i < newPoints.size(); ++i)
        {
            const auto& point = newPoints[i];
            if (! std::isfinite(point.x) || ! std::isfinite(point.y)
                || point.x < 0.0f || point.x > 1.0f
                || point.y < 0.0f || point.y > 1.0f
                || (i > 0 && point.x < newPoints[i - 1].x))
            {
                jassertfalse;
                return;
            }
        }

        curvatures.resize(points.size() - 1, 0.0f);
        const auto safeSegmentIndex = static_cast<size_t>(segmentIndex);

        // The points to insert are all points from the new shape *except* the very first and very last one,
        // because they will replace the existing start and end points of the segment.
        std::vector<juce::Point<float>> pointsToInsert(newPoints.begin() + 1, newPoints.end() - 1);

        // Update the start and end points of the original segment.
        points[safeSegmentIndex] = newPoints.front();
        points[safeSegmentIndex + 1] = newPoints.back();

        // Insert the intermediate points, if any.
        if (! pointsToInsert.empty())
        {
            points.insert(points.begin() + static_cast<std::ptrdiff_t>(safeSegmentIndex + 1),
                          pointsToInsert.begin(), pointsToInsert.end());
        }

        // Now, update the curvatures array to match the new points.
        // We set all new segments to have a linear (0.0) curvature.
        const auto numNewSegments = newPoints.size() - 1;
        std::vector<float> newCurvatures(numNewSegments, 0.0f);

        // Replace the single old curvature value with the new set of curvatures.
        const auto curvaturePosition = curvatures.begin() + static_cast<std::ptrdiff_t>(safeSegmentIndex);
        curvatures.erase(curvaturePosition);
        curvatures.insert(curvatures.begin() + static_cast<std::ptrdiff_t>(safeSegmentIndex),
                          newCurvatures.begin(), newCurvatures.end());
        sanitise();
    }
};
