#pragma once

#include "melatonin_inspector/melatonin/component_model.h"

namespace melatonin
{

    class Preview : public juce::Component, public ComponentModel::Listener
    {
    public:
        int zoomScale = 20;
        juce::Rectangle<int> maxPreviewImageBounds;

        explicit Preview (ComponentModel& _model) : model (_model)
        {
            setInterceptsMouseClicks (true, true);
            model.addListener (*this);
            addChildComponent (maxLabel);
            addAndMakeVisible (timingToggle);
            maxLabel.setColour (juce::Label::textColourId, colors::iconOff);
            maxLabel.setJustificationType (juce::Justification::centredTop);
            maxLabel.setFont (juce::Font ("Verdana", 18, juce::Font::FontStyleFlags::bold));

            // by default timings aren't on
            timingToggle.on = settings->props->getBoolValue ("showPerformanceTimings", false);
            timingToggle.onClick = [this] {
                // don't enable if we don't have timings
                if (!model.hasPerformanceTiming())
                {
                    timingToggle.on = false;
                    return;
                }

                settings->props->setValue ("showPerformanceTimings", timingToggle.on);
                getParentComponent()->resized();
            };
        }

        ~Preview() override
        {
            model.removeListener (*this);
        }

        void paint (juce::Graphics& g) override
        {
            TRACE_COMPONENT();

            g.setColour (colors::black);
            g.fillRect (contentBounds);

            if (showsPerformanceTimings())
            {
                // background for the max section
                maxLabel.setVisible (true);
                g.setColour (colors::propertyValueError.withAlpha (0.17f));
                g.fillRoundedRectangle (maxBounds.toFloat(), 3);

                g.setFont (g.getCurrentFont().withHeight (15.0f));
                double exclusiveSum = (double) model.timing1.getValue() + (double) model.timing2.getValue() + (double) model.timing3.getValue();
                bool hasExclusive = exclusiveSum * 1000 * 1000 > 1; // at least 1 microsecond
                bool hasChildren = model.hasChildren.getValue();

                auto exclusive = exclusiveBounds;
                g.setColour (hasExclusive ? colors::propertyName : colors::propertyValueDisabled);
                g.drawText ("Exclusive", exclusive.removeFromLeft (100), juce::Justification::topLeft);
                drawTimingText (g, exclusive.removeFromLeft (60), model.timing1.getValue(), !hasExclusive);
                drawTimingText (g, exclusive.removeFromLeft (60), model.timing2.getValue(), !hasExclusive);
                drawTimingText (g, exclusive.removeFromLeft (60), model.timing3.getValue(), !hasExclusive);
                drawTimingText (g, exclusive.removeFromLeft (60), model.timingMax.getValue(), !hasExclusive);

                auto withChildren = withChildrenBounds;
                g.setColour (hasChildren ? colors::propertyName : colors::propertyValueDisabled);
                g.drawText ("With Children", withChildren.removeFromLeft (100), juce::Justification::topLeft);
                drawTimingText (g, withChildren.removeFromLeft (60), model.timingWithChildren1, !hasChildren);
                drawTimingText (g, withChildren.removeFromLeft (60), model.timingWithChildren2, !hasChildren);
                drawTimingText (g, withChildren.removeFromLeft (60), model.timingWithChildren3, !hasChildren);
                drawTimingText (g, withChildren.removeFromLeft (60), model.timingWithChildrenMax, !hasChildren);
            }
            else
            {
                maxLabel.setVisible (false);
            }

            if (colorPicking)
            {
                // lets see them pixels!
                g.saveState();
                g.setImageResamplingQuality (juce::Graphics::ResamplingQuality::lowResamplingQuality);

                /* the zoomed snapshot is always *larger* than our preview area

                  bleed
                     │
                    ┌▼┌────────────────────┬─┐
                    │ │                   │ │
                    │ │                   │ │
                    │ │                   │ │
                    │ │                   │ │
                    └─┴────────────────────┴─┘
                 */
                int imageY = contentBounds.getY();
                int bleedPerSide = (previewImage.getWidth() * zoomScale - getWidth()) / 2;
                g.drawImageTransformed (previewImage, juce::AffineTransform::scale ((float) zoomScale, (float) zoomScale).translated ((float) -bleedPerSide, (float) imageY));

                // draw grid
                g.setColour (juce::Colours::grey.withAlpha (0.3f));
                for (auto i = 0; i < contentBounds.getHeight() / zoomScale; i++)
                    g.drawHorizontalLine (imageY + i * zoomScale, 0, (float) getWidth());

                int numberOfVerticalLines = previewImage.getWidth() - 1;
                auto inset = zoomScale - bleedPerSide;
                for (auto i = 0; i < numberOfVerticalLines; i++)
                    g.drawVerticalLine (inset + i * zoomScale, (float) imageY, (float) contentBounds.getBottom());

                // highlight the center pixel in first black, then white boxes
                g.setColour (juce::Colours::black);

                // grab the top left of the center pixel
                int highlightedPixelX = inset + (numberOfVerticalLines - 1) / 2 * zoomScale;
                int highlightY = (int) imageY + contentBounds.getHeight() / 2;
                g.drawRect (highlightedPixelX, highlightY - 10, zoomScale, zoomScale);
                g.setColour (juce::Colours::white);
                g.drawRect (highlightedPixelX - 2, highlightY - 12, 24, 24, 2);
                g.restoreState(); // back to full quality drawing
            }
            else if (!previewImage.isNull())
            {
                // TODO: odd this is needed (otherwise there's alpha in the state from somewhere)
                g.setOpacity (1.0f);

                // don't want our checkers aliased
                // so lets draw exact pixels
                g.saveState();
                g.setImageResamplingQuality (juce::Graphics::ResamplingQuality::lowResamplingQuality);

                // fits and scale the preview image and while doing so, grab the transform
                // this lets us reuse the position/scaling for clipping the transparency grid
                auto transform = juce::RectanglePlacement (juce::RectanglePlacement::centred).getTransformToFit (previewImage.getBounds().toFloat(), maxPreviewImageBounds.toFloat());
                auto resizedPreviewImageBounds = previewImage.getBounds().transformedBy (transform);

                // the transform is relative to maxPreviewImageBounds and has an "offset" already
                // For example, the Y offset will be at 48px from the Preview component
                // this would clip too much of the checkerboard (which is already placed at that offset).
                // An alternative solution here would be to have the checkerboard have the same size as Preview
                // but only start drawing the checkers at maxPreviewImageBounds
                auto checkersClipBounds = resizedPreviewImageBounds.translated(-maxPreviewImageBounds.getX(), -maxPreviewImageBounds.getY());

                // clipping keeps the checkerboard background fixed across image positions / sizes
                auto clippedCheckers = checkerboard.getClippedImage (checkersClipBounds);
                g.drawImage (clippedCheckers, resizedPreviewImageBounds.toFloat(), juce::RectanglePlacement::doNotResize);

                // back to drawing hi-res for the image
                g.restoreState();
                g.drawImageTransformed (previewImage, transform);
            }
        }

        void resized() override
        {
            TRACE_COMPONENT();

            auto area = getLocalBounds();
            buttonsBounds = area.removeFromTop (32);
            timingToggle.setBounds (buttonsBounds.removeFromRight (32));
            buttonsBounds.removeFromRight (12);
            contentBounds = area;

            if (showsPerformanceTimings())
            {
                auto performanceBounds = area.removeFromBottom (50).withLeft (32);
                maxBounds = performanceBounds.withLeft (304).withWidth (80).translated (0, -4).withTrimmedBottom (4);
                auto pivot = maxBounds.getTopRight().toFloat();
                exclusiveBounds = performanceBounds.removeFromTop (25);
                withChildrenBounds = performanceBounds;
                maxLabel.setBounds (maxBounds.withLeft ((int) pivot.getX() - 50));
                maxLabel.setTransform (juce::AffineTransform().rotated (-juce::MathConstants<float>::halfPi, pivot.getX(), pivot.getY()).translated (-22, -2));
            }
            else
            {
                exclusiveBounds = juce::Rectangle<int>();
                withChildrenBounds = juce::Rectangle<int>();
            }

            // default for this ends up being 32 48 382 68
            maxPreviewImageBounds = area.reduced (32, 16);
            drawCheckerboard();
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (model.getSelectedComponent())
            {
                // clear timings
                // TODO: these should be settable from model
                auto props = model.getSelectedComponent()->getProperties();
                if (model.hasPerformanceTiming())
                {
                    juce::StringArray items = { "timing1", "timing2", "timing3", "timingMax", "timingWithChildren1", "timingWithChildren2", "timingWithChildren3", "timingWithChildrenMax" };
                    for (auto& item : items)
                        props.set (item, 0.0);
                    model.refresh();
                }

                // force repaint to grab new timings
                model.getSelectedComponent()->repaint();

                // update the UI
                repaint();
            }
        }

        // called by color picker
        void setZoomedImage (const juce::Image& image)
        {
            previewImage = image;
            colorPicking = true;
            repaint();
        }

        void switchToPreview()
        {
            colorPicking = false;
            componentModelChanged (model);
            repaint();
        }

        [[nodiscard]] bool showsPerformanceTimings()
        {
            return !colorPicking && model.hasPerformanceTiming() && timingToggle.on;
        }

    private:
        juce::Image previewImage;
        juce::Image checkerboard;
        juce::SharedResourcePointer<InspectorSettings> settings;
        ComponentModel& model;
        bool colorPicking = false;

        juce::Rectangle<int> buttonsBounds;
        juce::Rectangle<int> contentBounds;
        juce::Rectangle<int> exclusiveBounds;
        juce::Rectangle<int> withChildrenBounds;
        juce::Rectangle<int> maxBounds;

        InspectorImageButton timingToggle { "timing", { 4, 4 }, true };
        juce::Label maxLabel { "max", "MAX" };

        void componentModelChanged (ComponentModel&) override
        {
            TRACE_COMPONENT();

            if (auto component = model.getSelectedComponent())
                previewImage = component->createComponentSnapshot ({ component->getWidth(), component->getHeight() }, false, 2.0f);
            else
                previewImage = juce::Image();

            colorPicking = false;
        }

        static void drawTimingText (juce::Graphics& g, juce::Rectangle<int> bounds, double value, bool disabled = false)
        {
            auto text = timingWithUnits (disabled ? 0 : value);

            auto ms = value * 1000;
            if (disabled || ms * 1000 < 1)
                g.setColour (colors::propertyValueDisabled);
            else if (ms > 3)
                g.setColour (colors::propertyValueWarn);
            else if (ms > 8)
                g.setColour (colors::propertyValueError);
            else
                g.setColour (colors::propertyValue);

            g.drawText (text, bounds, juce::Justification::topLeft);
        }

        static juce::String timingWithUnits (double value)
        {
            double ms = value * 1000;
            if (ms * 1000 < 1)
                return "-";
            else if (ms < 1)
                return juce::String (ms * 1000, 1).dropLastCharacters (2) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb5")) + "s"; // µs
            else
                return juce::String (ms, 1) + "ms";
        }

        // we draw the checkerboard at the full preview width and cache it
        // it's later clipped as needed
        void drawCheckerboard()
        {
            TRACE_COMPONENT();

            if (maxPreviewImageBounds.isEmpty())
                return;

            checkerboard = { juce::Image::RGB, maxPreviewImageBounds.getWidth(), maxPreviewImageBounds.getHeight(), true };
            juce::Graphics g2 (checkerboard);
            int checkerSize = settings->props->getIntValue ("checkerSize", 4);

            for (int i = 0; i < maxPreviewImageBounds.getWidth(); i += checkerSize)
            {
                // keeps checkerboard background consistent across image positions / sizes
                // allows for initial or ending partial checker
                for (auto j = 0; j < maxPreviewImageBounds.getHeight(); j += checkerSize)
                {
                    g2.setColour (((i + j) / checkerSize) % 2 == 0 ? colors::checkerLight : colors::checkerDark);
                    g2.fillRect (i, j, checkerSize, checkerSize);
                }
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Preview)
    };
}
