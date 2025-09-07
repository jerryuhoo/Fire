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
        points.push_back({ 0.0f, 0.0f });
        points.push_back({ 1.0f, 0.0f });
        curvatures.push_back(0.0f);
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
};
