#pragma once

#include "components/inspector_image_button.h"
#include "helpers/misc.h"
#include "melatonin_inspector/melatonin/components/accesibility.h"
#include "melatonin_inspector/melatonin/components/box_model.h"
#include "melatonin_inspector/melatonin/components/color_picker.h"
#include "melatonin_inspector/melatonin/components/component_tree_view_item.h"
#include "melatonin_inspector/melatonin/components/preview.h"
#include "melatonin_inspector/melatonin/components/properties.h"
#include "melatonin_inspector/melatonin/lookandfeel.h"

/*
 * Right now this unfortunately bundles all inspector components
 * as well as the tree view and selection logic.
 */

namespace melatonin
{
    class InspectorComponent : public juce::Component
    {
    public:
        explicit InspectorComponent()
        {
            TRACE_COMPONENT();

            setMouseClickGrabsKeyboardFocus (false);

            addAndMakeVisible (enabledButton);
            addAndMakeVisible (logo);
            addAndMakeVisible (fpsToggle);
            addAndMakeVisible (moveToggle);
            addAndMakeVisible (tabToggle);

            addChildComponent (tree);
            addChildComponent (emptySearchLabel);

            // visibilities of these are managed by the panels above
            addChildComponent (boxModel);
            addChildComponent (colorPicker);
            addChildComponent (preview);
            addChildComponent (properties);
            addChildComponent (accessibility);

            // z-order on panels is higher so they are clickable
            addAndMakeVisible (boxModelPanel);
            addAndMakeVisible (colorPickerPanel);
            addAndMakeVisible (previewPanel);
            addAndMakeVisible (propertiesPanel);
            addAndMakeVisible (accessibilityPanel);

            addAndMakeVisible (searchBox);
            addAndMakeVisible (searchIcon);
            addChildComponent (clearButton);

            colorPicker.togglePickerCallback = [this] (bool value) {
                if (toggleOverlayCallback)
                {
                    // re-enabling the color picker re-enables the overlay too quickly
                    // resulting in an unwanted click on the overlay and selection
                    if (value)
                    {
                        juce::Timer::callAfterDelay (500, [this] { toggleOverlayCallback (true); });
                    }
                    else
                        toggleOverlayCallback (false);
                }
            };

            emptySelectionPrompt.setJustificationType (juce::Justification::centredTop);
            emptySearchLabel.setJustificationType (juce::Justification::centredTop);
            emptySearchLabel.setColour (juce::Label::textColourId, colors::treeItemTextSelected);

            // the JUCE widget is unfriendly for theming, so indenting is also manually handled
            tree.setIndentSize (12);

            // JUCE makes it impossible to add any vertical padding within the viewport
            tree.getViewport()->setViewPosition (0, 0);
            tree.getViewport()->setScrollBarThickness (20);

            searchBox.setHelpText ("search");
            searchBox.setFont (juce::Font ("Verdana", 17, juce::Font::FontStyleFlags::plain));
            searchBox.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            searchBox.setColour (juce::Label::textColourId, colors::treeItemTextSelected);
            searchBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            searchBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            searchBox.setTextToShowWhenEmpty ("Filter components...", colors::searchText);
            searchBox.setJustification (juce::Justification::centredLeft);
            searchBox.onEscapeKey = [&] {
                searchBox.setText ("");
                searchBox.giveAwayKeyboardFocus();
                lastSearchText = {};
                getRoot()->validateSubItems();
            };

            logo.onClick = []() { juce::URL ("https://github.com/sudara/melatonin_inspector/").launchInDefaultBrowser(); };
            searchBox.onTextChange = [this] {
                auto searchText = searchBox.getText();
                ensureTreeIsConstructed();

                if (lastSearchText.isNotEmpty() && !searchText.startsWith (lastSearchText))
                {
                    getRoot()->validateSubItems();
                }

                lastSearchText = searchText;

                // try to find the first item that matches the search string
                if (searchText.isNotEmpty())
                {
                    getRoot()->filterNodesRecursively (searchText);
                }

                // display empty label
                if (getRoot()->getNumSubItems() == 0
                    && !searchText.containsIgnoreCase (getRoot()->getComponentName())
                    && tree.getNumSelectedItems() == 0)
                {
                    tree.setVisible (false);
                    emptySearchLabel.setVisible (true);

                    resized();
                }
                else
                {
                    tree.setVisible (true);
                    emptySearchLabel.setVisible (false);
                }

                clearButton.setVisible (searchBox.getText().isNotEmpty());
            };

            enabledButton.on = inspectorEnabled;
            enabledButton.onClick = [this] {
                toggleCallback (!inspectorEnabled);
            };

            // TODO: refactor this "on" state, it's terribly named
            fpsToggle.on = false;
            fpsToggle.onClick = [this] {
                // TODO: I don't like that the "on" state implicitly was toggled here
                settings->props->setValue ("fpsEnabled", fpsToggle.on);
                toggleFPSCallback (fpsToggle.on);
            };

            clearButton.onClick = [this] {
                searchBox.setText ("");
                searchBox.giveAwayKeyboardFocus();
            };

            // TODO: sorta sketchy to "know" the enum default...
            tabToggle.on = settings->props->getIntValue ("selectionMode", 0);
            tabToggle.onClick = [this] {
                settings->props->setValue ("selectionMode", fpsToggle.on);
                toggleSelectionMode (tabToggle.on);
            };

            // the tree view is empty even if inspector is enabled
            // since at the moment when this panel getting initialized, the root component most likely doesn't have any children YET
            // we can either wait and launch async update or add empty label
        }

        ~InspectorComponent() override
        {
            tree.setRootItem (nullptr);
        }

        void setRoot (juce::Component& r)
        {
            root = &r;
            colorPicker.setRootComponent (root);

            tree.setRootItem (nullptr);
            rootItem = nullptr;

            if (inspectorEnabled)
                ensureTreeIsConstructed();
        }

        void clearRoot()
        {
            root = nullptr;
            colorPicker.setRootComponent (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            auto mainPanelGradient = juce::ColourGradient::horizontal (colors::panelBackgroundDarker, (float) mainColumnBounds.getX(), colors::panelBackgroundLighter, (float) mainColumnBounds.getWidth());
            g.setGradientFill (mainPanelGradient);
            g.fillRect (mainColumnBounds);

            g.setColour (colors::headerBackground);
            g.fillRect (topArea);

            g.setColour (colors::black);
            g.fillRect (searchBoxBounds.expanded (0, 2));

            auto treeGradient = juce::ColourGradient::horizontal (colors::treeBackgroundLighter, (float) treeViewBounds.getX(), colors::treeBackgroundDarker, (float) treeViewBounds.getWidth());
            g.setGradientFill (treeGradient);
            g.fillRect (treeViewBounds);
        }

        void ensureTreeIsConstructed()
        {
            TRACE_COMPONENT();

            jassert (selectComponentCallback);

            // don't perform unnecessary work
            if (rootItem && rootItem.get() == getRoot())
                return;

            // if the root was set to something else, wipe it
            else if (rootItem)
                tree.setRootItem (nullptr);

            // construct the root item
            rootItem = std::make_unique<ComponentTreeViewItem> (root, outlineComponentCallback, selectComponentCallback);
            tree.setRootItem (rootItem.get());
            getRoot()->setOpenness (ComponentTreeViewItem::Openness::opennessOpen);

            tree.setVisible (true);
            auto numComponents = getRoot()->countItemsRecursively();
            searchBox.setTextToShowWhenEmpty (juce::String ("Filter " + juce::String (numComponents) + " components..."), colors::searchText);

            resized();
        }

        void resized() override
        {
            TRACE_COMPONENT();

            auto area = getLocalBounds();

            if (!inspectorEnabled)
                mainColumnBounds = area.removeFromLeft (380);
            else
                mainColumnBounds = area.removeFromRight (juce::jmax (380, int ((float) area.getWidth() * 0.6f)));

            auto mainCol = mainColumnBounds;
            auto headerHeight = 48;

            topArea = mainCol.removeFromTop (headerHeight);
            auto toolbar = topArea;
            enabledButton.setBounds (toolbar.removeFromLeft (48));
            fpsToggle.setBounds (toolbar.removeFromLeft (42));
            tabToggle.setBounds (toolbar.removeFromLeft (42));
            logo.setBounds (toolbar.removeFromRight (56));

            mainCol.removeFromTop (12);
            boxModelPanel.setBounds (mainCol.removeFromTop (32));
            boxModel.setBounds (mainCol.removeFromTop (boxModel.isVisible() ? 280 : 0));

            auto previewHeight = (preview.showsPerformanceTimings()) ? 182 : 132;
            auto previewBounds = mainCol.removeFromTop (preview.isVisible() ? previewHeight : 32);
            preview.setBounds (previewBounds);
            previewPanel.setBounds (previewBounds.removeFromTop (32).removeFromLeft (200));

            // the picker icon + rgba toggle overlays the panel header, so we overlap it
            auto colorPickerHeight = 72;
            int numColorsToDisplay = juce::jlimit (0, properties.isVisible() ? 12 : 3, (int) model.colors.size());
            if (colorPicker.isVisible() && !model.colors.empty())
                colorPickerHeight += 24 * numColorsToDisplay;
            auto colorPickerBounds = mainCol.removeFromTop (colorPicker.isVisible() ? colorPickerHeight : 32);
            colorPicker.setBounds (colorPickerBounds.withTrimmedLeft (32));
            colorPickerPanel.setBounds (colorPickerBounds.removeFromTop (32).removeFromLeft (200));

            accessibilityPanel.setBounds (mainCol.removeFromTop (32));
            accessibility.setBounds (mainCol.removeFromTop (accessibility.isVisible() ? 110 : 0).withTrimmedLeft (32));

            propertiesPanel.setBounds (mainCol.removeFromTop (33)); // extra pixel for divider
            properties.setBounds (mainCol.withTrimmedLeft (32));

            searchBoxBounds = area.removeFromTop (headerHeight);
            auto b = searchBoxBounds;
            clearButton.setBounds (b.removeFromRight (48));
            searchIcon.setBounds (b.removeFromLeft (48));
            searchBox.setBounds (b.reduced (0, 2));

            emptySearchLabel.setBounds (searchBoxBounds.reduced (4, 24));

            // these bounds are used to paint the background
            treeViewBounds = area;
            tree.setBounds (treeViewBounds);
        }

        void displayComponentInfo (Component* component, bool collapseTreeBeforeSelection = false)
        {
            TRACE_COMPONENT();

            ensureTreeIsConstructed();

            // only show on hover if there isn't something selected
            if (!selectedComponent || selectedComponent == component)
            {
                model.selectComponent (component);

                resized();
                repaint();

                // Selects and highlights
                if (component && getRoot())
                {
                    if (collapseTreeBeforeSelection)
                        getRoot()->recursivelyCloseSubItems();

                    getRoot()->openTreeAndSelect (component);
                    tree.scrollToKeepItemVisible (tree.getSelectedItem (0));
                }
            }
        }

        void redisplaySelectedComponent()
        {
            if (selectedComponent)
            {
                displayComponentInfo (selectedComponent);
            }
        }

        void selectComponent (Component* component, bool collapseTreeBeforeSelection = false)
        {
            TRACE_COMPONENT();

            if (component && selectedComponent == component)
            {
                deselectComponent();
                return;
            }
            selectedComponent = component;
            displayComponentInfo (selectedComponent, collapseTreeBeforeSelection);
        }

        void deselectComponent()
        {
            TRACE_COMPONENT();

            selectedComponent = nullptr;
            tree.clearSelectedItems();

            properties.reset();
            model.deselectComponent();
            tree.setRootItem (getRoot());

            preview.repaint();
            colorPicker.reset();

            resized();
        }

        // called from the main melatonin_inspector.h
        // for example, on load of the entire inspector
        // or after a toggle button click
        void toggle (bool nowEnabled)
        {
            TRACE_COMPONENT();

            enabledButton.on = nowEnabled;
            inspectorEnabled = nowEnabled;

            // content visibility is handled by the panel
            previewPanel.setVisible (nowEnabled);
            colorPickerPanel.setVisible (nowEnabled);
            propertiesPanel.setVisible (nowEnabled);
            tree.setVisible (nowEnabled);

            if (!nowEnabled)
            {
                model.deselectComponent();
                if (getRoot())
                    getRoot()->recursivelyCloseSubItems();
            }

            // populate the tree view if nothing selected
            else if (selectedComponent == nullptr)
                ensureTreeIsConstructed();

            colorPicker.reset();

            resized();
        }

        std::function<void (Component* c)> selectComponentCallback;
        std::function<void (Component* c)> outlineComponentCallback;
        std::function<void (bool enabled)> toggleCallback;
        std::function<void (bool enabled)> toggleOverlayCallback;
        std::function<void (bool enabled)> toggleFPSCallback;
        std::function<void (bool enabled)> toggleSelectionMode;

    private:
        Component::SafePointer<Component> selectedComponent;
        Component* root = nullptr;
        juce::SharedResourcePointer<InspectorSettings> settings;
        ComponentModel model;
        bool inspectorEnabled = false;

        juce::Rectangle<int> mainColumnBounds, topArea, searchBoxBounds, treeViewBounds;
        InspectorImageButton logo { "logo" };

        BoxModel boxModel { model };
        CollapsablePanel boxModelPanel { "BOX MODEL", &boxModel };

        Preview preview { model };
        CollapsablePanel previewPanel { "PREVIEW", &preview };

        ColorPicker colorPicker { model, preview };
        CollapsablePanel colorPickerPanel { "COLORS", &colorPicker };

        Properties properties { model };
        CollapsablePanel propertiesPanel { "PROPERTIES", &properties, true };

        Accessibility accessibility { model };
        CollapsablePanel accessibilityPanel { "ACCESSIBILITY", &accessibility, false };

        // TODO: move to its own component
        juce::TreeView tree;
        juce::Label emptySelectionPrompt { "SelectionPrompt", "Select any component to see components tree" };
        juce::Label emptySearchLabel { "EmptySearchResultsPrompt", "No component found" };
        juce::TextEditor searchBox { "Search box" };
        InspectorImageButton clearButton { "clear", { 0, 6 } };
        InspectorImageButton searchIcon { "search", { 8, 8 } };
        InspectorImageButton enabledButton { "enabled", { 8, 6 }, true };
        InspectorImageButton fpsToggle { "speedometer", { 2, 7 }, true };
        InspectorImageButton moveToggle { "move", { 0, 6 }, true };
        InspectorImageButton tabToggle { "tab", { 0, 6 }, true };

        juce::String lastSearchText;

        std::unique_ptr<ComponentTreeViewItem> rootItem;

        ComponentTreeViewItem* getRoot()
        {
            return dynamic_cast<ComponentTreeViewItem*> (tree.getRootItem());
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorComponent)
    };
}
