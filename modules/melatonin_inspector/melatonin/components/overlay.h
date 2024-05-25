#pragma once
#include "../helpers/misc.h"

namespace melatonin
{

    class Overlay : public juce::Component, public juce::ComponentListener
    {
    public:
        Overlay()
        {
            setAlwaysOnTop (true);
            setName ("Melatonin Overlay");
            // need to click on the resizeable corners of the component outlines
            setInterceptsMouseClicks (false, true);
            addAndMakeVisible (dimensions);

            juce::Label* labels[4] = { &distanceToTopHoveredLabel, &distanceToLeftHoveredLabel, &distanceToBottomHoveredLabel, &distanceToRightHoveredLabel };

            for (auto* l : labels)
            {
                l->setFont (13.f);
                l->setJustificationType (juce::Justification::centred);
                l->setColour (juce::Label::textColourId, colors::white);

                addAndMakeVisible (l);
            }

            dimensions.setFont (13.f);
            dimensions.setJustificationType (juce::Justification::centred);
            dimensions.setColour (juce::Label::textColourId, colors::white);
        }

        ~Overlay() override
        {
            if (selectedComponent)
                deselectComponent();
        }

        void paint (juce::Graphics& g) override
        {
            TRACE_COMPONENT();
            g.setColour (colors::overlayBoundingBox);

            // draws inwards as the line thickens
            if (outlinedComponent)
                g.drawRect (outlinedBounds, 2);
            if (selectedComponent)
            {
                // Thinner border than hover (draws inwards)
                g.drawRect (selectedBounds, 1);

                const float dashes[] { 2.0f, 2.0f };
                g.drawDashedLine (lineFromTopToParent, dashes, 2, 1.0f);
                g.drawDashedLine (lineFromLeftToParent, dashes, 2, 1.0f);

                // corners outside
                g.fillRect (juce::Rectangle<int> (selectedBounds.getTopLeft().translated (-4, -4), selectedBounds.getTopLeft().translated (4, 4)));
                g.fillRect (juce::Rectangle<int> (selectedBounds.getTopRight().translated (-4, -4), selectedBounds.getTopRight().translated (4, 4)));
                g.fillRect (juce::Rectangle<int> (selectedBounds.getBottomRight().translated (-4, -4), selectedBounds.getBottomRight().translated (4, 4)));
                g.fillRect (juce::Rectangle<int> (selectedBounds.getBottomLeft().translated (-4, -4), selectedBounds.getBottomLeft().translated (4, 4)));

                // corners inside
                g.setColour (colors::white);
                g.fillRect (juce::Rectangle<int> (selectedBounds.getTopLeft().translated (-3, -3), selectedBounds.getTopLeft().translated (3, 3)));
                g.fillRect (juce::Rectangle<int> (selectedBounds.getTopRight().translated (-3, -3), selectedBounds.getTopRight().translated (3, 3)));
                g.fillRect (juce::Rectangle<int> (selectedBounds.getBottomRight().translated (-3, -3), selectedBounds.getBottomRight().translated (3, 3)));
                g.fillRect (juce::Rectangle<int> (selectedBounds.getBottomLeft().translated (-3, -3), selectedBounds.getBottomLeft().translated (3, 3)));

                g.setColour (colors::overlayLabelBackground);
                // text doesn't vertically center very nicely without manual offset
                g.fillRoundedRectangle (dimensionsLabelBounds.withBottom (dimensionsLabelBounds.getBottom()).toFloat(), 2.0f);
            }

            if (!hoveredBounds.isEmpty())
            {
                g.setColour (colors::overlayDistanceToHovered);
                g.drawRect (hoveredBounds.reduced (1));
                g.drawRect (selectedBounds.reduced (1));

                const float dashes[] { 2.0f, 2.0f };
                g.drawLine (lineToTopHoveredComponent, 2);
                g.drawLine (lineToLeftHoveredComponent, 2);
                g.drawLine (lineToRightHoveredComponent, 2);
                g.drawLine (lineToBottomHoveredComponent, 2);

                g.drawDashedLine (horConnectingLineToComponent, dashes, 2, 2.0f);
                g.drawDashedLine (vertConnectingLineToComponent, dashes, 2, 2.0f);

                // text doesn't vertically center very nicely without manual offset
                if (distanceToTopHoveredLabel.isVisible())
                    g.fillRoundedRectangle (distanceToTopLabelBounds.withBottom (distanceToTopLabelBounds.getBottom()).toFloat(), 2.0f);
                if (distanceToBottomHoveredLabel.isVisible())
                    g.fillRoundedRectangle (distanceToBottomLabelBounds.withBottom (distanceToBottomLabelBounds.getBottom()).toFloat(), 2.0f);
                if (distanceToLeftHoveredLabel.isVisible())
                    g.fillRoundedRectangle (distanceToLeftLabelBounds.withBottom (distanceToLeftLabelBounds.getBottom()).toFloat(), 2.0f);
                if (distanceToRightHoveredLabel.isVisible())
                    g.fillRoundedRectangle (distanceToRightLabelBounds.withBottom (distanceToRightLabelBounds.getBottom()).toFloat(), 2.0f);
            }
        }

        void resized() override
        {
            TRACE_COMPONENT();
            if (outlinedComponent)
            {
                outlineComponent (outlinedComponent);
            }
        }

        // Components that belong to overlay are screened out by the caller (inspector)
        void outlineComponent (Component* component)
        {
            TRACE_COMPONENT();

            // get rid of another outline when re-entering a selected component
            if (selectedComponent == component)
            {
                outlinedComponent = nullptr;
            }

            outlinedComponent = component;

            if (outlinedComponent)
                outlinedBounds = getLocalAreaForOutline (component);

            repaint();
        }

        void resetDistanceLinesToHovered()
        {
            lineToTopHoveredComponent = juce::Line<float>();
            lineToLeftHoveredComponent = juce::Line<float>();
            lineToRightHoveredComponent = juce::Line<float>();
            lineToBottomHoveredComponent = juce::Line<float>();

            horConnectingLineToComponent = juce::Line<float>();
            vertConnectingLineToComponent = juce::Line<float>();

            distanceToTopHoveredLabel.setVisible (false);
            distanceToBottomHoveredLabel.setVisible (false);
            distanceToLeftHoveredLabel.setVisible (false);
            distanceToRightHoveredLabel.setVisible (false);
        }
        // draws a distance line when component is selected, showing distance to parent or hovered element
        void outlineDistanceCallback (Component* hovComponent)
        {
            hoveredComponent = hovComponent;
            if (hovComponent != selectedComponent && hovComponent != nullptr)
            {
                hoveredBounds = getLocalAreaForOutline (hovComponent);
                calculateDistanceLinesToHovered();
                drawDistanceLabel();
            }
            else
            {
                hoveredBounds = juce::Rectangle<int>();

                resetDistanceLinesToHovered();
            }

            repaint();
        }

        void selectComponent (Component* component)
        {
            TRACE_COMPONENT();

            // allow us to clear selection
            if (!component)
            {
                deselectComponent();
                return;
            }

            if (selectedComponent)
            {
                bool isSameComponent = selectedComponent == component;
                deselectComponent();
                if (isSameComponent)
                    return;
            }

            // listen for those sweet resize calls
            component->addComponentListener (this);
            component->addMouseListener (this, false);

            // take over the outline from the hover
            outlinedComponent = nullptr;
            selectedComponent = component;
            resizable = std::make_unique<juce::ResizableBorderComponent> (component, &constrainer);
            resizable->setBorderThickness (juce::BorderSize<int> (6));
            addAndMakeVisible (*resizable);
            setSelectedAndResizeableBounds (component);
            repaint();

            if (selectedComponent)
            {
                constrainer.setMinimumOnscreenAmounts (selectedComponent->getHeight(), selectedComponent->getWidth(), selectedComponent->getHeight(), selectedComponent->getWidth());
                // reset previous selection and update mouse cursor
                selectedComponent->setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            }
        }

        // When our selected component has been dragged or resized this is our callback
        // We *must* then manually manage the size of the ResizableBorderComponent
        void componentMovedOrResized (Component& component, bool wasMoved, bool wasResized) override
        {
            TRACE_COMPONENT();

            if (wasResized || wasMoved)
            {
                // sort of annoying if hover triggers on resize
                if (outlinedComponent)
                    outlinedComponent = nullptr;
                setSelectedAndResizeableBounds (&component);
            }
        }

        void mouseExit (const juce::MouseEvent& /*event*/) override
        {
            outlinedComponent = nullptr;
            if (!selectedComponent)
                return;

            selectedComponent->setMouseCursor (juce::MouseCursor::NormalCursor);
            repaint();
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            Component::mouseUp (event);
            isDragging = false;
        }

        void mouseEnter (const juce::MouseEvent&) override
        {
            if (!selectedComponent)
                return;

            selectedComponent->setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            repaint();
        }

        void mouseMove (const juce::MouseEvent&) override
        {
            if (!selectedComponent)
                return;
            selectedComponent->setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            repaint();
        }

        void startDraggingComponent (const juce::MouseEvent& e)
        {
            // only allow dragging if the mouse is inside the selected component
            if (selectedComponent && selectedComponent->getLocalBounds().contains (e.getEventRelativeTo (selectedComponent).getPosition()))
            {
                componentDragger.startDraggingComponent (selectedComponent, e);
                isDragging = true;
            }
        }

        void dragSelectedComponent (const juce::MouseEvent& e)
        {
            // only allow dragging if the mouse is inside the selected component
            bool isInside = selectedComponent && selectedComponent->getLocalBounds().contains (e.getEventRelativeTo (selectedComponent).getPosition());

            if (isInside || (selectedComponent && isDragging))
            {
                isDragging = true;
                componentDragger.dragComponent (selectedComponent, e, &constrainer);
            }
        }

    private:
        Component::SafePointer<Component> outlinedComponent;
        Component::SafePointer<Component> hoveredComponent;
        juce::Rectangle<int> outlinedBounds;

        bool isDragging = false;
        juce::ComponentDragger componentDragger;
        juce::ComponentBoundsConstrainer boundsConstrainer;

        Component::SafePointer<Component> selectedComponent;
        juce::Rectangle<int> selectedBounds;
        juce::Line<float> lineFromTopToParent;
        juce::Line<float> lineFromLeftToParent;

        juce::Rectangle<int> hoveredBounds;
        juce::Line<float> lineToTopHoveredComponent,
            lineToLeftHoveredComponent,
            lineToRightHoveredComponent,
            lineToBottomHoveredComponent,
            horConnectingLineToComponent,
            vertConnectingLineToComponent;

        juce::Label distanceToTopHoveredLabel,
            distanceToBottomHoveredLabel,
            distanceToLeftHoveredLabel,
            distanceToRightHoveredLabel;
        juce::Rectangle<int> distanceToTopLabelBounds,
            distanceToLeftLabelBounds,
            distanceToRightLabelBounds,
            distanceToBottomLabelBounds;

        std::unique_ptr<juce::ResizableBorderComponent> resizable;
        juce::ComponentBoundsConstrainer constrainer;

        juce::Label dimensions;
        juce::Rectangle<int> dimensionsLabelBounds;

        juce::Rectangle<int> getLocalAreaForOutline (Component* component, int borderSize = 2)
        {
            auto boundsPlusOutline = component->getBounds().expanded (borderSize);
            return getLocalArea (component->getParentComponent(), boundsPlusOutline);
        }

        void drawDimensionsLabel()
        {
            int labelWidth = (int) dimensions.getFont().getStringWidthFloat (dimensionsString (selectedBounds)) + 15;
            int labelHeight = 15;
            auto paddingToLabel = 4;
            auto labelCenterX = selectedBounds.getX() + selectedBounds.getWidth() / 2;

            if ((selectedBounds.getBottom() + 20 + paddingToLabel) < getBottom())
            {
                // label on bottom
                dimensionsLabelBounds = juce::Rectangle<int> ((int) (labelCenterX - labelWidth / 2), selectedBounds.getBottom() + paddingToLabel, labelWidth, labelHeight).expanded (2, 1);
            }
            else
            {
                // label on top
                dimensionsLabelBounds = juce::Rectangle<int> ((int) (labelCenterX - labelWidth / 2), selectedBounds.getY() - labelHeight - paddingToLabel, labelWidth, labelHeight).expanded (2, 1);
            }
            dimensions.setText (dimensionsString (selectedBounds), juce::dontSendNotification);
            dimensions.setBounds (dimensionsLabelBounds);
            dimensions.setVisible (true);
        }

        void calculateLinesToParent()
        {
            // Todo(Investigate why this is happening)
            if (selectedComponent == nullptr)
            {
                jassertfalse;
                return;
            }

            auto topOfComponent = selectedComponent->getBoundsInParent().getPosition().translated (selectedBounds.getWidth() / 2, -1);
            auto leftOfComponent = selectedComponent->getBoundsInParent().getPosition().translated (-1, selectedBounds.getHeight() / 2);

            auto localTop = getLocalPoint (selectedComponent->getParentComponent(), topOfComponent);
            auto localParentTop = getLocalPoint (selectedComponent->getParentComponent(), topOfComponent.withY (0));
            auto localLeft = getLocalPoint (selectedComponent->getParentComponent(), leftOfComponent);
            auto localParentLeft = getLocalPoint (selectedComponent->getParentComponent(), leftOfComponent.withX (0));

            lineFromTopToParent = juce::Line<int> (localTop, localParentTop).toFloat();
            lineFromLeftToParent = juce::Line<int> (localLeft, localParentLeft).toFloat();
        }

        void setSelectedAndResizeableBounds (Component* component)
        {
            selectedBounds = getLocalAreaForOutline (component, 1);
            drawDimensionsLabel();
            calculateLinesToParent();
            resizable->setBounds (selectedBounds);
            repaint();
        }

        void drawDistanceLabel()
        {
            // todo avoid overlapping of labels with dimensions label
            if (selectedComponent && hoveredComponent)
            {
                int labelHeight = 15;
                auto paddingToLabel = 4.0f;

                // top
                if (lineToTopHoveredComponent.getLength() > 0)
                {
                    int labelWidth = (int) distanceToTopHoveredLabel.getFont().getStringWidthFloat (distanceString (lineToTopHoveredComponent)) + 15;

                    // todo draw on left or right side of line
                    auto labelCenterY = lineToTopHoveredComponent.getPointAlongLineProportionally (0.5f).getY();
                    distanceToTopLabelBounds = juce::Rectangle<int> ((int) (lineToTopHoveredComponent.getStartX() + paddingToLabel),
                        (int) labelCenterY - labelHeight / 2,
                        labelWidth,
                        labelHeight)
                                                   .expanded (2, 1);
                    distanceToTopHoveredLabel.setText (distanceString (lineToTopHoveredComponent), juce::dontSendNotification);
                    distanceToTopHoveredLabel.setBounds (distanceToTopLabelBounds);
                }

                // bottom
                if (lineToBottomHoveredComponent.getLength() > 0)
                {
                    int labelWidth = (int) distanceToBottomHoveredLabel.getFont().getStringWidthFloat (distanceString (lineToBottomHoveredComponent)) + 15;

                    // todo draw on left or right side of line
                    auto labelCenterY = lineToBottomHoveredComponent.getPointAlongLineProportionally (0.5f).getY();
                    distanceToBottomLabelBounds = juce::Rectangle<int> ((int) (lineToBottomHoveredComponent.getStartX() + paddingToLabel),
                        (int) labelCenterY - labelHeight / 2,
                        labelWidth,
                        labelHeight)
                                                      .expanded (2, 1);
                    distanceToBottomHoveredLabel.setText (distanceString (lineToBottomHoveredComponent), juce::dontSendNotification);
                    distanceToBottomHoveredLabel.setBounds (distanceToBottomLabelBounds);
                }

                // right
                if (lineToRightHoveredComponent.getLength() > 0)
                {
                    int labelWidth = (int) distanceToRightHoveredLabel.getFont().getStringWidthFloat (distanceString (lineToRightHoveredComponent)) + 15;

                    // todo draw on top or bottom side of line
                    auto labelCenterX = lineToRightHoveredComponent.getPointAlongLineProportionally (0.5f).getX();
                    distanceToRightLabelBounds = juce::Rectangle<int> ((int) labelCenterX - labelWidth / 2,
                        (int) (lineToRightHoveredComponent.getStartY() + paddingToLabel),
                        labelWidth,
                        labelHeight)
                                                     .expanded (2, 1);
                    distanceToRightHoveredLabel.setText (distanceString (lineToRightHoveredComponent), juce::dontSendNotification);
                    distanceToRightHoveredLabel.setBounds (distanceToRightLabelBounds);
                }

                // left
                if (lineToLeftHoveredComponent.getLength() > 0)
                {
                    int labelWidth = (int) distanceToLeftHoveredLabel.getFont().getStringWidthFloat (distanceString (lineToLeftHoveredComponent)) + 15;

                    // todo draw on top or bottom side of line
                    auto labelCenterX = lineToLeftHoveredComponent.getPointAlongLineProportionally (0.5f).getX();
                    distanceToLeftLabelBounds = juce::Rectangle<int> ((int) labelCenterX - labelWidth / 2,
                        (int) (lineToLeftHoveredComponent.getStartY() + paddingToLabel),
                        labelWidth,
                        labelHeight)
                                                    .expanded (2, 1);
                    distanceToLeftHoveredLabel.setText (distanceString (lineToLeftHoveredComponent), juce::dontSendNotification);
                    distanceToLeftHoveredLabel.setBounds (distanceToLeftLabelBounds);
                }
            }

            distanceToTopHoveredLabel.setVisible (lineToTopHoveredComponent.getLength() > 0);
            distanceToBottomHoveredLabel.setVisible (lineToBottomHoveredComponent.getLength() > 0);
            distanceToLeftHoveredLabel.setVisible (lineToLeftHoveredComponent.getLength() > 0);
            distanceToRightHoveredLabel.setVisible (lineToRightHoveredComponent.getLength() > 0);
        }
        void calculateDistanceLinesToHovered()
        {
            lineToTopHoveredComponent = juce::Line<float>();
            lineToLeftHoveredComponent = juce::Line<float>();
            lineToRightHoveredComponent = juce::Line<float>();
            lineToBottomHoveredComponent = juce::Line<float>();

            horConnectingLineToComponent = juce::Line<float>();
            vertConnectingLineToComponent = juce::Line<float>();

            if (hoveredComponent && selectedComponent)
            {
                bool hovOnLeft = selectedBounds.getCentreX() > hoveredBounds.getCentreX();
                bool hovOnTop = selectedBounds.getCentreY() > hoveredBounds.getCentreY();

                // if the hovered component is above the selected component
                if (hovOnTop)
                {
                    // if the hovered component is left of the selected component
                    if (hovOnLeft)
                    {
                        auto p1 = selectedBounds.getTopLeft().translated (0, selectedBounds.getHeight() / 2);
                        lineToRightHoveredComponent = juce::Line<int> (p1, p1.withX (hoveredBounds.getRight())).toFloat();
                    }
                    // if the hovered component is right of the selected component
                    else
                    {
                        auto p1 = selectedBounds.getTopRight().translated (0, selectedBounds.getHeight() / 2);
                        lineToLeftHoveredComponent = juce::Line<int> (p1, p1.withX (hoveredBounds.getX())).toFloat();
                    }

                    auto p1 = selectedBounds.getTopLeft().translated (selectedBounds.getWidth() / 2, 0);

                    // avoid drawing of top line if the hovered component is left or right of the selected component
                    // if(selectedBounds.getY() > hoveredBounds.getBottom() || selectedBounds.getBottom() < hoveredBounds.getY())
                    lineToBottomHoveredComponent = juce::Line<int> (p1, p1.withY (hoveredBounds.getBottom())).toFloat();

                    // avoid drawing horizontal line and if line is going into component
                    if (juce::approximatelyEqual (lineToBottomHoveredComponent.getStartY(), lineToBottomHoveredComponent.getEndY()) || lineToBottomHoveredComponent.getStartY() < lineToBottomHoveredComponent.getEndY())
                        lineToBottomHoveredComponent = juce::Line<float>();

                    // avoid drawing strictly vertical line lineToLeftHoveredComponent
                    if (juce::approximatelyEqual (lineToLeftHoveredComponent.getStartX(), lineToLeftHoveredComponent.getEndX()) || lineToLeftHoveredComponent.getStartX() > lineToLeftHoveredComponent.getEndX())
                        lineToLeftHoveredComponent = juce::Line<float>();

                    // avoid drawing strictly vertical line lineToRightHoveredComponent
                    if (juce::approximatelyEqual (lineToRightHoveredComponent.getStartX(), lineToRightHoveredComponent.getEndX()) || lineToRightHoveredComponent.getStartX() < lineToRightHoveredComponent.getEndX())
                        lineToRightHoveredComponent = juce::Line<float>();
                }
                else
                {
                    // if the hovered component is left of the selected component
                    if (hovOnLeft)
                    {
                        auto p1 = selectedBounds.getBottomLeft().translated (0, -selectedBounds.getHeight() / 2);
                        lineToRightHoveredComponent = juce::Line<int> (p1, p1.withX (hoveredBounds.getRight())).toFloat();
                    }
                    else
                    {
                        auto p1 = selectedBounds.getBottomRight().translated (0, -selectedBounds.getHeight() / 2);
                        lineToLeftHoveredComponent = juce::Line<int> (p1, p1.withX (hoveredBounds.getX())).toFloat();
                    }

                    auto p1 = selectedBounds.getBottomLeft().translated (selectedBounds.getWidth() / 2, 0);
                    lineToTopHoveredComponent = juce::Line<int> (p1, p1.withY (hoveredBounds.getY())).toFloat();

                    // avoid drawing horizontal line and if line is going into component
                    if (juce::approximatelyEqual (lineToTopHoveredComponent.getStartY(), lineToTopHoveredComponent.getEndY()) || lineToTopHoveredComponent.getStartY() > lineToTopHoveredComponent.getEndY())
                        lineToTopHoveredComponent = juce::Line<float>();

                    // avoid drawing strictly vertical line lineToLeftHoveredComponent
                    if (juce::approximatelyEqual (lineToLeftHoveredComponent.getStartX(), lineToLeftHoveredComponent.getEndX()) || lineToLeftHoveredComponent.getStartX() > lineToLeftHoveredComponent.getEndX())
                        lineToLeftHoveredComponent = juce::Line<float>();

                    // avoid drawing strictly vertical line lineToRightHoveredComponent
                    if (juce::approximatelyEqual (lineToRightHoveredComponent.getStartX(), lineToRightHoveredComponent.getEndX()) || lineToRightHoveredComponent.getStartX() < lineToRightHoveredComponent.getEndX())
                        lineToRightHoveredComponent = juce::Line<float>();
                }

                // adding missing lines to connect to hovered component
                if (!hoveredBounds.contains (lineToRightHoveredComponent.getEnd().toInt().translated (-2, 0)) && lineToRightHoveredComponent.getLength() > 0)
                {
                    juce::Point<int> hoveredPoint;
                    if (hovOnTop)
                    {
                        hoveredPoint = hovOnLeft ? hoveredBounds.getBottomRight() : hoveredBounds.getBottomLeft();
                    }
                    else
                        hoveredPoint = hovOnLeft ? hoveredBounds.getTopRight() : hoveredBounds.getTopLeft();
                    vertConnectingLineToComponent = juce::Line<float> (lineToRightHoveredComponent.getEnd().toFloat(), hoveredPoint.toFloat());
                }
                if (!hoveredBounds.contains (lineToLeftHoveredComponent.getEnd().translated (2, 0).toInt()) && lineToLeftHoveredComponent.getLength() > 0)
                {
                    juce::Point<int> hoveredPoint;
                    if (hovOnTop)
                    {
                        hoveredPoint = hovOnLeft ? hoveredBounds.getBottomRight() : hoveredBounds.getBottomLeft();
                    }
                    else
                        hoveredPoint = hovOnLeft ? hoveredBounds.getTopRight() : hoveredBounds.getTopLeft();
                    vertConnectingLineToComponent = juce::Line<float> (lineToLeftHoveredComponent.getEnd().toFloat(), hoveredPoint.toFloat());
                }

                if (!hoveredBounds.contains (lineToTopHoveredComponent.getEnd().toInt().translated (0, 2)) && lineToTopHoveredComponent.getLength() > 0)
                {
                    juce::Point<int> hoveredPoint = hovOnLeft ? hoveredBounds.getTopRight() : hoveredBounds.getTopLeft();
                    horConnectingLineToComponent = juce::Line<float> (lineToTopHoveredComponent.getEnd().toFloat(), hoveredPoint.toFloat());
                }
                if (!hoveredBounds.contains (lineToBottomHoveredComponent.getEnd().toInt().translated (0, -2)) && lineToBottomHoveredComponent.getLength() > 0)
                {
                    auto hoveredPoint = hovOnLeft ? hoveredBounds.getBottomRight() : hoveredBounds.getBottomLeft();
                    horConnectingLineToComponent = juce::Line<float> (lineToBottomHoveredComponent.getEnd().toFloat(), hoveredPoint.toFloat());
                }
            }
        }

        void deselectComponent()
        {
            dimensions.setVisible (false);

            if (selectedComponent != nullptr)
            {
                selectedComponent->removeComponentListener (this);
                selectedComponent->removeMouseListener (this);
                selectedComponent->setMouseCursor (juce::MouseCursor::NormalCursor);
            }

            selectedComponent = nullptr;
            repaint();
        }
    };

}
