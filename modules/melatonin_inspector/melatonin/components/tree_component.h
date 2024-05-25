#pragma once

#include "component_tree_view_item.h"
#include "melatonin_inspector/melatonin/component_model.h"

namespace melatonin
{
    class TreeComponent : public juce::Component, private ComponentModel::Listener, private juce::FocusChangeListener
    {
    public:

        TreeComponent (ComponentModel& _model): model(_model)
        {
            juce::Desktop::getInstance ().addFocusListener (this);
            model.addListener (*this);
        }

        ~TreeComponent() override
        {
            juce::Desktop::getInstance ().removeFocusListener (this);
            model.removeListener (*this);
        }

        void globalFocusChanged (juce::Component*) override
        {
            repaint();
        }

        void resized() override
        {
            auto area = getLocalBounds();

            //using btn toggle state (better to switch to using class variable
            //or inspectors prop)

            if (tree.isVisible())
            {
                auto searchRow = area.removeFromTop (30).reduced (4, 4);
                clearBtn.setBounds (searchRow.removeFromRight (56));
                searchRow.removeFromRight (8);
                searchBox.setBounds (searchRow);

                tree.setBounds (area); // padding in these default components are a mess
                emptySearchLabel.setBounds (area.reduced (4));
            }
            else
                emptySelectionPrompt.setBounds (area);
        }

    private:
        ComponentModel& model;

        juce::TreeView tree;
        juce::Label emptySelectionPrompt { "SelectionPrompt", "Select any component to see components tree" };
        juce::Label emptySearchLabel { "EmptySearchResultsPrompt", "No component found" };
        juce::TextEditor searchBox { "Search box" };
        juce::TextButton clearBtn { "clear" };
        std::unique_ptr<ComponentTreeViewItem> rootItem;

    };
}
