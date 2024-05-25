#pragma once

#include "colour_property_component.h"
#include "juce_gui_extra/juce_gui_extra.h"
#include "melatonin_inspector/melatonin/components/overlay.h"
#include "preview.h"

namespace melatonin
{
    class RGBAToggle : public juce::Component
    {
    public:
        bool rgba = true;
        std::function<void()> onClick;

        RGBAToggle()
        {
            rgba = settings->props->getBoolValue ("rgba", true);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colors::customBackground);
            g.fillRoundedRectangle (getLocalBounds().withSizeKeepingCentre (38, 16).toFloat(), 3);
            g.setColour (colors::label);
            g.setFont (juce::Font ("Verdana", 9, juce::Font::FontStyleFlags::bold));
            g.drawText (rgba ? "RGBA" : "HEX", getLocalBounds(), juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& /*event*/) override
        {
            rgba = !rgba;
            settings->props->setValue ("rgba", rgba);
            if (onClick)
                onClick();
            repaint();
        }

    private:
        juce::SharedResourcePointer<InspectorSettings> settings;
    };

    class ColorPicker : public juce::Component,
                        private ComponentModel::Listener,
                        private juce::ComponentListener,
                        private juce::FocusChangeListener
    {
    public:
        std::function<void (bool)> togglePickerCallback;

        explicit ColorPicker (ComponentModel& _model, Preview& p) : model (_model), preview (p)
        {
            addAndMakeVisible (colorPickerButton);
            addAndMakeVisible (panel);
            addAndMakeVisible (rgbaToggle);

            selectedColor = juce::Colours::transparentBlack;

            // we overlap the header, so let people click that as usual
            setInterceptsMouseClicks (false, true);

            // needed for displaying colors
            model.addListener (*this);

            // needed for preview resizes
            preview.addComponentListener (this);

            // needed for color picking UX
            juce::Desktop::getInstance().addFocusChangeListener (this);

            colorPickerButton.onClick = [this]() {
                // uncertain why, but this must be accessed through "this"
                togglePicker (this->colorPickerButton.on);
            };

            // update color properties with the correct display format
            rgbaToggle.onClick = [this]() { componentModelChanged (model); };
        }

        ~ColorPicker() override
        {
            model.removeListener (*this);
            preview.removeComponentListener (this);
            juce::Desktop::getInstance().removeFocusChangeListener (this);

            if (root != nullptr && colorPickerButton.on)
                root->removeMouseListener (this);
        }

        void paint (juce::Graphics& g) override
        {
            if (colorPickerButton.on)
            {
                g.setColour (colors::black);

                // rect with only bottom corners rounded
                g.fillRect (colorValueBounds.withBottom (4));
                g.fillRoundedRectangle (colorValueBounds.withTrimmedBottom (1).toFloat(), 4);

                g.setColour (colors::text);
                g.setFont (juce::Font ("Verdana", 14.5, juce::Font::FontStyleFlags::plain));
                g.drawText (stringForColor (selectedColor), colorValueBounds.withTrimmedBottom (2), juce::Justification::centred);
            }

            if (model.colors.empty())
            {
                g.setColour (colors::propertyName);
                g.setFont (juce::Font ("Verdana", 15, juce::Font::FontStyleFlags::plain));
                g.drawText ("No Color Properties", panelBounds.withTrimmedLeft (3).withTrimmedTop (2), juce::Justification::topLeft);
            }
        }

        void resized() override
        {
            auto buttonsArea = getLocalBounds().removeFromTop (32);
            colorPickerButton.setBounds (buttonsArea.removeFromRight (32).translated (2, -4));
            buttonsArea.removeFromRight (12);
            rgbaToggle.setBounds (buttonsArea.removeFromRight (38));

            auto area = getLocalBounds();

            // overlaps with the panel + bit of padding
            colorValueBounds = area.removeFromTop (32).withTrimmedRight (36).withSizeKeepingCentre (rgbaToggle.rgba ? 100 : 90, 32);

            area.removeFromTop (5);
            panelBounds = area;
            if (!model.colors.empty())
            {
                panel.setBounds (panelBounds);
            }
        }

        void mouseEnter (const juce::MouseEvent& event) override
        {
            if (root == nullptr)
                return;
            cursorToRestore = event.eventComponent->getMouseCursor();
            event.eventComponent->setMouseCursor (eyedropperCursor);
            auto rootPos = event.getEventRelativeTo (root).getPosition();
            updatePicker (rootPos);
        }

        void mouseMove (const juce::MouseEvent& event) override
        {
            if (root == nullptr)
                return;

            // we see some interaction / glitching with the target app unless we always enforce this
            // event.eventComponent->setMouseCursor (eyedropperCursor);

            auto rootPos = event.getEventRelativeTo (root).getPosition();
            updatePicker (rootPos);
        }

        void mouseExit (const juce::MouseEvent& event) override
        {
            // always try to keep this cursor
            event.eventComponent->setMouseCursor (cursorToRestore);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (root == nullptr)
                return;

            if (mouseDownShouldOnlyFocus)
            {
                mouseDownShouldOnlyFocus = false;
                return;
            }

            if (colorPickerButton.on && selectedColor != juce::Colours::transparentBlack)
            {
                event.eventComponent->setMouseCursor (cursorToRestore);
                model.pickedColor.setValue ((int) selectedColor.getARGB());
                model.refresh(); // update Last Picked
                colorPickerButton.on = false;
                colorPickerButton.onClick();
                repaint(); // needed in case we have nothing selected
            }
        }

        // when the preview size changes, update our snapshot size
        void componentMovedOrResized (Component& component, bool, bool wasResized) override
        {
            if (&component == &preview && wasResized)
            {
                updateSnapshotDimensions();
            }
        }

        // prevents an accidental color pick when clicking for focus reasons
        // See Issue #67
        void globalFocusChanged (juce::Component* component) override
        {
            // when no component has focus, another app has focus
            // and we're coming back to the target app / inspector
            if (component == nullptr && colorPickerButton.on)
                mouseDownShouldOnlyFocus = true;
        }

        // called from the inspector component
        void setRootComponent (Component* newRoot)
        {
            root = newRoot;
            if (root == nullptr)
            {
                selectedColor = juce::Colours::transparentBlack;
                reset();
            }
        }

        void reset()
        {
            componentModelChanged (model);
        }

        // close the picker if we are hidden
        void visibilityChanged() override
        {
            if (!isVisible())
            {
                colorPickerButton.on = false;
                colorPickerButton.onClick();
            }
        }

    private:
        ComponentModel& model;
        Preview& preview;

        juce::PropertyPanel panel { "Properties" };
        InspectorImageButton colorPickerButton { "eyedropper", { 0, 6 }, true };
        juce::Rectangle<int> colorValueBounds;
        juce::Rectangle<int> panelBounds;
        RGBAToggle rgbaToggle;

        juce::MouseCursor cursorToRestore = juce::MouseCursor::NormalCursor;
        juce::Image eyedropperCursorImage = getIcon ("eyedropperon").rescaled (16, 16);
        juce::MouseCursor eyedropperCursor { eyedropperCursorImage, 0, 15 };
        bool mouseDownShouldOnlyFocus = false;

        juce::Colour selectedColor { juce::Colours::transparentBlack };
        std::unique_ptr<juce::Image> croppedSnapshot;
        int snapshotRadiusWidth = 21; // defaults align with initial dimensions of preview
        int snapshotRadiusHeight = 3;

        juce::Component* root {};

        void togglePicker (bool on)
        {
            TRACE_COMPONENT();

            if (on)
            {
                // get notified by all mouse activity in the target app/plugin
                root->addMouseListener (this, true);

                reset();

                preview.setVisible (true);
                // pick an arbitrary first position in the overlay
                if (root != nullptr)
                    updatePicker ({ root->getX() + 10, root->getY() + 10 });
            }
            else
            {
                if (root != nullptr)
                    root->removeMouseListener (this);

                preview.switchToPreview();
                selectedColor = juce::Colours::transparentBlack;
            }

            // might need to resize the panel if we need to toggle paint timings
            // or there's a change in number of colors
            getParentComponent()->resized();
            repaint();

            // toggle overlay visibility
            if (togglePickerCallback)
                togglePickerCallback (!on);
        }

        void updatePicker (juce::Point<int> positionInRoot)
        {
            TRACE_COMPONENT();

            if (!colorPickerButton.on)
                return;

            updateSnapshot (positionInRoot);
            auto snapshotBounds = croppedSnapshot->getBounds();
            selectedColor = croppedSnapshot->getPixelAt (snapshotBounds.getCentreX(), snapshotBounds.getCentreY());

            // our snapshotted image will be larger than the preview panel (due to the bleed)
            preview.setZoomedImage (*croppedSnapshot);
            repaint();
        }

        // we continually update a cropped snapshot when picking
        // this avoids a big cumbersome whole-plugin snapshot (bad for perf)
        // but still lets users navigate the UI (changing tabs, popups, etc)
        void updateSnapshot (juce::Point<int> positionInRoot)
        {
            TRACE_COMPONENT();

            if (root == nullptr)
                return;

            auto snappedBounds = juce::Rectangle<int> (positionInRoot.x - snapshotRadiusWidth, positionInRoot.y - snapshotRadiusHeight, snapshotRadiusWidth * 2 + 1, snapshotRadiusHeight * 2 + 1);
            croppedSnapshot = std::make_unique<juce::Image> (root->createComponentSnapshot (snappedBounds, false));
        }

        void updateSnapshotDimensions()
        {
            TRACE_COMPONENT();

            // we are creating a 20x zoom
            // (for example at the minimum width of 380, it's 19 pixels total)
            int numberOfPixelsWidth = int (juce::jmax (380, preview.maxPreviewImageBounds.getWidth()) / preview.zoomScale);
            int numberOfPixelsHeight = int (juce::jmax (100, preview.maxPreviewImageBounds.getHeight()) / preview.zoomScale);

            // remove 1 extra pixel to ensure odd number of pixels
            if (numberOfPixelsWidth % 2 == 0)
                numberOfPixelsWidth -= 1;

            if (numberOfPixelsHeight % 2 == 0)
                numberOfPixelsHeight -= 1;

            // add 1 extra pixel for potential bleed on each side
            int extraBleed = 2;

            // a width of 13 pixels results in 7 pixel radius
            // a width of 14 pixels ALSO results in a 7 pixel radius
            snapshotRadiusWidth = (numberOfPixelsWidth + extraBleed - 1) / 2;
            snapshotRadiusHeight = (numberOfPixelsHeight + extraBleed - 1) / 2;

            // TODO: remove when preview height is adjustable
            jassert (snapshotRadiusHeight == 3);
        }

        void componentModelChanged (ComponentModel&) override
        {
            TRACE_COMPONENT();

            panel.clear();
            juce::Array<juce::PropertyComponent*> props;

            for (auto& nv : model.colors)
            {
                auto* prop = new ColourPropertyComponent (nv.value, colors::enumNameIfPresent (nv.name), rgbaToggle.rgba, true);
                if (nv.name == "Last Picked")
                {
                    prop->setColour (juce::PropertyComponent::labelTextColourId, colors::highlight);
                }
                prop->setLookAndFeel (&getLookAndFeel());
                props.add (prop);
            }
            panel.addProperties (props, 5);
            resized();
        }

        juce::String stringForColor (juce::Colour& color) const
        {
            return rgbaToggle.rgba ? colors::rgbaString (color) : colors::hexString (color);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColorPicker)
    };
}
