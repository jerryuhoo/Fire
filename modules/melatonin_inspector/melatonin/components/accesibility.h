
#pragma once
#include "melatonin_inspector/melatonin/component_model.h"

namespace melatonin
{
    class Accessibility : public juce::Component, private ComponentModel::Listener
    {
    public:
        explicit Accessibility (ComponentModel& m) : model (m)
        {
            addAndMakeVisible (&panel);
            model.addListener (*this);
        }

        ~Accessibility() override
        {
            model.removeListener (*this);
        }

        void updateProperties()
        {
            panel.clear();

            if (!model.getSelectedComponent())
                return;

            auto& ad = model.accessiblityDetail;
            auto aprops = juce::Array<juce::PropertyComponent*> {
                new juce::TextPropertyComponent (ad.title, "Title", 200, false, false),
                new juce::TextPropertyComponent (ad.value, "Value", 200, false, false),
                new juce::TextPropertyComponent (ad.role, "Role", 200, false, false),
                new juce::TextPropertyComponent (ad.handlerType, "Handler", 200, false, false),
            };
            for (auto* p : aprops)
            {
                p->setLookAndFeel (&getLookAndFeel());
            }
            panel.addProperties (aprops, 0);
            resized();
        }

    protected:
        ComponentModel& model;

        void componentModelChanged (ComponentModel&) override
        {
            updateProperties();
        }

        void resized() override
        {
            TRACE_COMPONENT();
            // let the property panel know what total height we need to be
            panel.setBounds (getLocalBounds().withTrimmedTop (padding));
        }

        int padding { 3 };

        juce::PropertyPanel panel { "Accessibility" };
    };
}
