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
struct LfoData
{
    std::vector<juce::Point<float>> points;
    std::vector<float> curvatures;

    // Default constructor to initialize a valid shape
    LfoData()
    {
        points.push_back({ 0.0f, 0.5f });
        points.push_back({ 1.0f, 0.5f });
        curvatures.push_back(0.0f);
    }
};
