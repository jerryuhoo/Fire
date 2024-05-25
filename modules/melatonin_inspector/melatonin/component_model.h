#pragma once

#include <utility>
#include "helpers/component_helpers.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace melatonin
{
    class ComponentModel : private juce::Value::Listener, private juce::ComponentListener
    {
    public:
        class Listener
        {
        public:
            virtual ~Listener() = default;
            virtual void componentModelChanged (ComponentModel& model) = 0;
        };

        juce::Value nameValue;
        juce::Value widthValue, heightValue, xValue, yValue;
        juce::Value enabledValue, opaqueValue, hasCachedImageValue, accessibilityHandledValue;
        juce::Value visibleValue, wantsFocusValue, interceptsMouseValue, childrenInterceptsMouseValue;
        juce::Value lookAndFeelValue, typeValue, fontValue, alphaValue;
        juce::Value pickedColor;
        juce::Value timing1, timing2, timing3, timingMax, hasChildren;

        juce::Value isToggleable, toggleState, clickTogglesState, radioGroupId;

        struct AccessiblityDetail
        {
            juce::Value title, value, role, handlerType;
        } accessiblityDetail;

        double timingWithChildren1, timingWithChildren2, timingWithChildren3, timingWithChildrenMax;

        ComponentModel() = default;

        ~ComponentModel() override
        {
            selectComponent (nullptr);
        }

        void selectComponent (juce::Component* component)
        {
            TRACE_COMPONENT();

            // add component listener to component and unsubscribe from previous component
            if (selectedComponent)
                selectedComponent->removeComponentListener (this);

            selectedComponent = component;

            if (selectedComponent)
                selectedComponent->addComponentListener (this);

            updateModel();
        }

        void deselectComponent()
        {
            TRACE_COMPONENT();

            if (selectedComponent)
                selectedComponent->removeComponentListener (this);

            selectedComponent = nullptr;
            updateModel();
        }

        struct NamedProperty
        {
            NamedProperty() = default;
            NamedProperty (juce::String n, const juce::var& v)
                : name (std::move (n)), value (v)
            {
            }

            juce::String name;
            juce::Value value;
        };

        std::vector<NamedProperty> namedProperties;
        std::vector<NamedProperty> colors;

        void refresh()
        {
            updateModel();
        }

        void removeListener (Listener& listener)
        {
            listenerList.remove (&listener);
        }

        void addListener (Listener& listener)
        {
            listenerList.add (&listener);
        }

        // this may return nullptr if no component is selected
        juce::Component* getSelectedComponent()
        {
            return selectedComponent;
        }

        [[nodiscard]] bool hasPerformanceTiming()
        {
            return timing1.getValue().isDouble();
        }

    private:
        juce::ListenerList<Listener> listenerList;
        juce::Component::SafePointer<juce::Component> selectedComponent;

        void updateModel()
        {
            TRACE_COMPONENT();

            removeListeners();

            // always show picked color, even with no component selected
            if (!pickedColor.getValue().isVoid())
                colors.emplace_back ("Last Picked", pickedColor);

            if (!selectedComponent)
            {
                // if not manually removed, it'll linger in the model...
                removePerformanceData();
                notifyListeners();
                return;
            }

            nameValue = selectedComponent->getName();
            lookAndFeelValue = lnfString (selectedComponent);
            visibleValue = selectedComponent->isVisible();
            enabledValue = selectedComponent->isEnabled();
            alphaValue = juce::String (selectedComponent->getAlpha());
            opaqueValue = selectedComponent->isOpaque();
            wantsFocusValue = selectedComponent->getWantsKeyboardFocus();
            fontValue = componentFontValue (selectedComponent);
            hasCachedImageValue = selectedComponent->getCachedComponentImage() != nullptr;
            typeValue = type (*selectedComponent);
            accessibilityHandledValue = selectedComponent->isAccessible();

            if (auto button = dynamic_cast<juce::Button*> (selectedComponent.getComponent()))
            {
                isToggleable = button->isToggleable();
                toggleState = button->getToggleState();
                clickTogglesState = button->getClickingTogglesState();
                radioGroupId = button->getRadioGroupId();
            }

            nameValue.addListener(this);
            widthValue.addListener (this);
            heightValue.addListener (this);
            xValue.addListener (this);
            yValue.addListener (this);
            visibleValue.addListener (this);
            wantsFocusValue.addListener (this);
            enabledValue.addListener (this);
            opaqueValue.addListener (this);
            alphaValue.addListener (this);
            accessibilityHandledValue.addListener (this);
            interceptsMouseValue.addListener (this);
            childrenInterceptsMouseValue.addListener (this);

            isToggleable.addListener (this);
            toggleState.addListener (this);
            clickTogglesState.addListener (this);
            radioGroupId.addListener (this);

            if (selectedComponent->isAccessible() && selectedComponent->getAccessibilityHandler())
            {
                auto* accH = selectedComponent->getAccessibilityHandler();
                accessiblityDetail.handlerType = type (*accH);
                if (accH->getValueInterface())
                {
                    accessiblityDetail.value = accH->getValueInterface()->getCurrentValueAsString();
                }
                else
                {
                    accessiblityDetail.value = "no value interface";
                }
                accessiblityDetail.title = accH->getTitle();
                auto role = accH->getRole();
                switch (role)
                {
                    // Amazingly juce doesn' thave a display name fn for these
#define DN(x)                          \
    case juce::AccessibilityRole::x:   \
        accessiblityDetail.role = #x; \
        break;
                    DN (button)
                    DN (toggleButton)
                    DN (radioButton)
                    DN (comboBox)
                    DN (image)
                    DN (slider)
                    DN (label)
                    DN (staticText)
                    DN (editableText)
                    DN (menuItem)
                    DN (menuBar)
                    DN (popupMenu)
                    DN (table)
                    DN (tableHeader)
                    DN (column)
                    DN (row)
                    DN (cell)
                    DN (hyperlink)
                    DN (list)
                    DN (listItem)
                    DN (tree)
                    DN (treeItem)
                    DN (progressBar)
                    DN (group)
                    DN (dialogWindow)
                    DN (window)
                    DN (scrollBar)
                    DN (tooltip)
                    DN (splashScreen)
                    DN (ignored)
                    DN (unspecified)
#undef DN
                    default:
                        accessiblityDetail.role = juce::String ("Unknown ") + juce::String ((int) role);
                        break;
                }
            }

            {
                bool interceptsMouse = false;
                bool childrenInterceptsMouse = false;
                selectedComponent->getInterceptsMouseClicks (interceptsMouse, childrenInterceptsMouse);
                interceptsMouseValue = interceptsMouse;
                childrenInterceptsMouseValue = childrenInterceptsMouse;
            }

            hasChildren.setValue (selectedComponent->getNumChildComponents() > 0);
            populatePerformanceData (selectedComponent->getProperties());

            {
                auto& properties = selectedComponent->getProperties();
                for (const auto& nv : properties)
                {
                    if (nv.name.toString().startsWith ("jcclr_"))
                        colors.emplace_back (nv.name.toString(), nv.value);
                    else
                        namedProperties.emplace_back (nv.name.toString(), nv.value);
                }

                for (auto& nv : namedProperties)
                    nv.value.addListener (this);

                for (auto& nv : colors)
                    nv.value.addListener (this);
            }
            notifyListeners();
        }

        void removeListeners()
        {
            TRACE_COMPONENT();

            widthValue.removeListener (this);
            heightValue.removeListener (this);
            xValue.removeListener (this);
            yValue.removeListener (this);
            enabledValue.removeListener (this);
            opaqueValue.removeListener (this);
            alphaValue.removeListener (this);
            visibleValue.removeListener (this);
            wantsFocusValue.removeListener (this);
            accessibilityHandledValue.removeListener (this);
            interceptsMouseValue.removeListener (this);
            childrenInterceptsMouseValue.removeListener (this);

            isToggleable.removeListener (this);
            toggleState.removeListener (this);
            clickTogglesState.removeListener (this);
            radioGroupId.removeListener (this);

            for (auto& np : namedProperties)
                np.value.removeListener (this);

            for (auto& np : colors)
                np.value.removeListener (this);

            colors.clear();
            namedProperties.clear();
        }

        // allows properties to be set from our properties
        void valueChanged (juce::Value& value) override
        {
            TRACE_COMPONENT();

            if (selectedComponent)
            {
                if (value.refersToSameSourceAs (widthValue) || value.refersToSameSourceAs (heightValue))
                {
                    selectedComponent->setSize ((int) widthValue.getValue(), (int) heightValue.getValue());
                }
                else if (value.refersToSameSourceAs (xValue) || value.refersToSameSourceAs (yValue))
                {
                    int leftVal = xValue.getValue();
                    int topVal = yValue.getValue();

                    // in cases where components are animated or moved via AffineTransforms
                    // we can get a feedback loop, as the left/top values are no longer
                    // the actual position in the component
                    // so first remove any transform present
                    selectedComponent->setTransform (juce::AffineTransform());
                    selectedComponent->setTopLeftPosition (leftVal, topVal);
                }
                else if (value.refersToSameSourceAs (visibleValue))
                {
                    selectedComponent->setVisible (visibleValue.getValue());
                }
                else if (value.refersToSameSourceAs (wantsFocusValue))
                {
                    selectedComponent->setWantsKeyboardFocus (wantsFocusValue.getValue());
                }
                else if (value.refersToSameSourceAs (enabledValue))
                {
                    selectedComponent->setEnabled (enabledValue.getValue());
                }
                else if (value.refersToSameSourceAs (alphaValue))
                {
                    selectedComponent->setAlpha ((float) alphaValue.getValue());
                }
                else if (value.refersToSameSourceAs (opaqueValue))
                {
                    selectedComponent->setOpaque (opaqueValue.getValue());
                }
                else if (value.refersToSameSourceAs (accessibilityHandledValue))
                {
                    selectedComponent->setAccessible (accessibilityHandledValue.getValue());
                }
                else if (value.refersToSameSourceAs (interceptsMouseValue) || value.refersToSameSourceAs (childrenInterceptsMouseValue))
                {
                    selectedComponent->setInterceptsMouseClicks (interceptsMouseValue.getValue(), childrenInterceptsMouseValue.getValue());
                }
                else if (value.refersToSameSourceAs (isToggleable))
                {
                    if (auto button = dynamic_cast<juce::Button*> (selectedComponent.getComponent()))
                        button->setToggleable (isToggleable.getValue());
                }
                else if (value.refersToSameSourceAs (toggleState))
                {
                    if (auto button = dynamic_cast<juce::Button*> (selectedComponent.getComponent()))
                        button->setToggleState (toggleState.getValue(), juce::dontSendNotification);
                }
                else if (value.refersToSameSourceAs (clickTogglesState))
                {
                    if (auto button = dynamic_cast<juce::Button*> (selectedComponent.getComponent()))
                        button->setClickingTogglesState (clickTogglesState.getValue());
                }
                else if (value.refersToSameSourceAs (radioGroupId))
                {
                    if (auto button = dynamic_cast<juce::Button*> (selectedComponent.getComponent()))
                        button->setRadioGroupId (radioGroupId.getValue());
                }
                else
                {
                    for (auto& nv : namedProperties)
                    {
                        if (value.refersToSameSourceAs (nv.value))
                        {
                            selectedComponent->getProperties().set (nv.name, nv.value.getValue());
                            selectedComponent->repaint();
                            break;
                        }
                    }

                    for (auto& nv : colors)
                    {
                        if (value.refersToSameSourceAs (nv.value))
                        {
                            selectedComponent->getProperties().set (nv.name, nv.value.getValue());
                            selectedComponent->repaint();
                            break;
                        }
                    }
                }
            }
            else
            {
                jassertfalse;
            }
        }

        void componentMovedOrResized (juce::Component&, bool wasMoved, bool wasResized) override
        {
            TRACE_COMPONENT();

            if (wasResized || wasMoved)
            {
                updateModel();
            }
        }

        void populatePerformanceData (const juce::NamedValueSet& props)
        {
            if (props.contains ("timing1"))
            {
                // assume they are all there
                timing1 = props["timing1"];
                timing2 = props["timing2"];
                timing3 = props["timing3"];
                timingMax = props["timingMax"];

                timingWithChildren1 = timing1.getValue();
                timingWithChildren2 = timing2.getValue();
                timingWithChildren3 = timing3.getValue();
                timingWithChildrenMax = timingMax.getValue();
                getTimingWithChildren (selectedComponent);
            }
            else
            {
                removePerformanceData();
            }
        }

        void notifyListeners()
        {
            listenerList.call ([this] (Listener& listener) {
                listener.componentModelChanged (*this);
            });
        }

        void removePerformanceData()
        {
            timing1 = juce::var();
            timing2 = juce::var();
            timing3 = juce::var();
            timingMax = juce::var();
            timingWithChildren1 = juce::var();
            timingWithChildren2 = juce::var();
            timingWithChildren3 = juce::var();
            timingWithChildrenMax = juce::var();
        }

        void getTimingWithChildren (juce::Component* component)
        {
            for (auto child : component->getChildren())
            {
                if (child->getProperties().contains ("timing1"))
                {
                    timingWithChildren1 += (double) child->getProperties()["timing1"];
                    timingWithChildren2 += (double) child->getProperties()["timing2"];
                    timingWithChildren3 += (double) child->getProperties()["timing3"];
                    timingWithChildrenMax += (double) child->getProperties()["timingMax"];
                    getTimingWithChildren (child);
                }
            }
        }
    };
}
