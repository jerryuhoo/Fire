#include <Panels/SpectrogramPanel/FreqTextLabel.h>
#include <GUI/LookAndFeel.h>
#include "helpers/ScopedNumericLocale.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

struct PrimaryEditableLabelTestAccess
{
    static bool hasPrimaryGesture(const PrimaryEditableLabel& label) noexcept
    {
        return label.pointerGesture
            == PrimaryEditableLabel::PointerGesture::primary;
    }

    static void setTrackedPointerSource(
        PrimaryEditableLabel& label,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        label.pointerSourceType = type;
        label.pointerSourceIndex = index;
    }

    static bool commitEditorText(PrimaryEditableLabel& label,
                                 const juce::String& text)
    {
        auto* editor = label.getCurrentTextEditor();
        if (editor == nullptr)
            return false;

        editor->setText(text, false);
        label.textEditorReturnKeyPressed(*editor);
        return true;
    }
};

namespace
{
template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (auto* match = dynamic_cast<ComponentType*>(child))
            return match;

        if (child != nullptr)
            if (auto* nestedMatch = findDescendant<ComponentType>(*child))
                return nestedMatch;
    }

    return nullptr;
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::ModifierKeys modifiers = {},
                                bool wasDragged = false,
                                int clickCount = 1)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             modifiers,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             now,
             position,
             now,
             clickCount,
             wasDragged };
}

struct FrequencyLabelFixture
{
    FrequencyLabelFixture()
        : frequencyLabel(divider)
    {
        desktopHost.setBounds(0, 0, 140, 64);
        desktopHost.setVisible(false);
        divider.setRange(40.0, 10024.0, 1.0);
        divider.setValue(1000.0, juce::dontSendNotification);
        desktopHost.addAndMakeVisible(frequencyLabel);
        frequencyLabel.setBounds(0, 0, 90, 24);
        frequencyLabel.setVisible(true);
        frequencyLabel.setFreq(1000);
        desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        desktopHost.setVisible(true);
    }

    juce::Label* getEditableLabel()
    {
        return findDescendant<juce::Label>(frequencyLabel);
    }

    juce::Component desktopHost;
    VerticalLine divider;
    FreqTextLabel frequencyLabel;
};

void beginPointerGesture(juce::Label& label,
                         juce::ModifierKeys modifiers,
                         int clickCount = 1)
{
    auto& component = static_cast<juce::Component&>(label);
    component.mouseDown(makeMouseEvent(component, modifiers, false, clickCount));
}

void endPointerGesture(juce::Label& label,
                       juce::ModifierKeys modifiers = {},
                       int clickCount = 1)
{
    auto& component = static_cast<juce::Component&>(label);
    component.mouseUp(makeMouseEvent(component, modifiers, false, clickCount));
}

void enterTextAndPostReturn(juce::Label& label, const juce::String& text)
{
    auto* editor = label.getCurrentTextEditor();
    REQUIRE(editor != nullptr);
    editor->setText(text, false);
    REQUIRE(editor->keyPressed(
        juce::KeyPress(juce::KeyPress::returnKey)));
}
} // namespace

TEST_CASE("Frequency labels accept only complete primary pointer clicks",
          "[frequency-label][ui][input][primary]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("auxiliary and mixed clicks are rejected")
    {
        const std::vector<juce::ModifierKeys> rejectedModifiers {
            juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
            juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                                 | juce::ModifierKeys::middleButtonModifier },
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                                 | juce::ModifierKeys::rightButtonModifier }
        };

        for (const auto modifiers : rejectedModifiers)
        {
            FrequencyLabelFixture fixture;
            auto* label = fixture.getEditableLabel();
            REQUIRE(label != nullptr);
            CHECK_FALSE(label->getMouseClickGrabsKeyboardFocus());

            beginPointerGesture(*label, modifiers);
            endPointerGesture(*label, modifiers);

            CAPTURE(modifiers.getRawFlags());
            CHECK_FALSE(label->isBeingEdited());
        }
    }

    SECTION("an unpaired release is inert")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);

        endPointerGesture(*label);

        CHECK_FALSE(label->isBeingEdited());
    }

    SECTION("a complete primary click enters editing")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);

        beginPointerGesture(
            *label,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        endPointerGesture(*label);

        CHECK(label->isBeingEdited());
    }
}

TEST_CASE("Frequency labels recover missing releases across lifecycle boundaries",
          "[frequency-label][ui][input][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("a buttonless owner move cancels a lost release")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);
        auto& component = static_cast<juce::Component&>(*label);

        beginPointerGesture(*label, primary);
        component.mouseMove(makeMouseEvent(component));
        endPointerGesture(*label);

        CHECK_FALSE(label->isBeingEdited());
    }

    SECTION("a foreign source cannot complete or cancel the owned click")
    {
        FrequencyLabelFixture fixture;
        auto* label = dynamic_cast<PrimaryEditableLabel*>(
            fixture.getEditableLabel());
        REQUIRE(label != nullptr);
        auto& component = static_cast<juce::Component&>(*label);
        const auto source = juce::Desktop::getInstance().getMainMouseSource();

        beginPointerGesture(*label, primary);
        REQUIRE(PrimaryEditableLabelTestAccess::hasPrimaryGesture(*label));

        PrimaryEditableLabelTestAccess::setTrackedPointerSource(
            *label,
            source.getType() == juce::MouseInputSource::mouse
                ? juce::MouseInputSource::touch
                : juce::MouseInputSource::mouse,
            source.getIndex() + 17);
        endPointerGesture(*label);

        CHECK(PrimaryEditableLabelTestAccess::hasPrimaryGesture(*label));
        CHECK_FALSE(label->isBeingEdited());

        PrimaryEditableLabelTestAccess::setTrackedPointerSource(
            *label, source.getType(), source.getIndex());
        component.mouseMove(makeMouseEvent(component));
        REQUIRE_FALSE(
            PrimaryEditableLabelTestAccess::hasPrimaryGesture(*label));

        endPointerGesture(*label);
        CHECK_FALSE(label->isBeingEdited());
    }

    SECTION("hiding the owning frequency bubble invalidates a pending release")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);

        beginPointerGesture(*label, primary);
        fixture.frequencyLabel.setVisible(false);
        fixture.frequencyLabel.setVisible(true);
        endPointerGesture(*label);

        CHECK_FALSE(label->isBeingEdited());
    }

    SECTION("disabling the editable label invalidates a pending release")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);

        beginPointerGesture(*label, primary);
        label->setEnabled(false);
        label->setEnabled(true);
        endPointerGesture(*label);

        CHECK_FALSE(label->isBeingEdited());
    }

    SECTION("detaching from the peer cancels editing and a pending release")
    {
        juce::Component desktopHost;
        PrimaryEditableLabel label({}, "1 kHz");
        desktopHost.setBounds(0, 0, 140, 64);
        desktopHost.setVisible(false);
        desktopHost.addAndMakeVisible(label);
        label.setBounds(0, 0, 90, 24);
        label.setEditable(true);
        desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        desktopHost.setVisible(true);

        beginPointerGesture(label, primary);
        REQUIRE(PrimaryEditableLabelTestAccess::hasPrimaryGesture(label));
        label.showEditor();
        auto* editor = label.getCurrentTextEditor();
        REQUIRE(editor != nullptr);
        juce::Component::SafePointer<juce::TextEditor> editorLifetime(editor);
        editor->setText("2 kHz", false);

        desktopHost.removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        CHECK_FALSE(PrimaryEditableLabelTestAccess::hasPrimaryGesture(label));
        CHECK_FALSE(label.isBeingEdited());
        CHECK(editorLifetime == nullptr);
        CHECK(label.getText() == "1 kHz");

        desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        REQUIRE(label.isShowing());
        endPointerGesture(label);

        CHECK_FALSE(label.isBeingEdited());
        CHECK(label.getText() == "1 kHz");
    }
}

TEST_CASE("Frequency label single and double click edit modes keep JUCE semantics",
          "[frequency-label][ui][input][double-click]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("single-click editing uses one complete primary pair")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);
        label->setEditable(true, false);

        beginPointerGesture(*label, primary);
        endPointerGesture(*label);

        CHECK(label->isBeingEdited());
    }

    SECTION("double-click editing consumes an authorised second click")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);
        label->setEditable(false, true);
        auto& component = static_cast<juce::Component&>(*label);

        beginPointerGesture(*label, primary, 2);
        const auto doubleClick = makeMouseEvent(component, primary, false, 2);
        component.mouseUp(doubleClick);
        component.mouseDoubleClick(doubleClick);

        CHECK(label->isBeingEdited());
    }

    SECTION("an unpaired double-click callback is inert")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);
        label->setEditable(false, true);
        auto& component = static_cast<juce::Component&>(*label);

        component.mouseDoubleClick(makeMouseEvent(component, primary, false, 2));

        CHECK_FALSE(label->isBeingEdited());
    }
}

TEST_CASE("Frequency labels keep dot-decimal syntax under comma locales",
          "[frequency-label][ui][input][validation][locale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    fire::test::ScopedCommaNumericLocale numericLocale;
    if (! numericLocale.activate())
        SKIP("No comma-decimal LC_NUMERIC locale is installed");

    VerticalLine divider;
    divider.setRange(40.0, 10024.0, 1.0);
    divider.setValue(1000.0, juce::dontSendNotification);
    auto frequencyLabel = std::make_unique<FreqTextLabel>(divider);
    juce::Component desktopHost;
    desktopHost.setBounds(0, 0, 140, 64);
    desktopHost.setVisible(false);
    desktopHost.addAndMakeVisible(*frequencyLabel);
    frequencyLabel->setBounds(0, 0, 90, 24);
    frequencyLabel->setFreq(1000);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);

    int editCalls = 0;
    float lastNormalisedFrequency = 0.0f;
    divider.setParameterGestureCallbacks(
        [] {},
        [] { return std::make_shared<int>(0); },
        [] {});
    frequencyLabel->setFrequencyEditCallback(
        [&](float normalisedFrequency)
        {
            ++editCalls;
            lastNormalisedFrequency = normalisedFrequency;
        });

    auto* label = dynamic_cast<PrimaryEditableLabel*>(
        findDescendant<juce::Label>(*frequencyLabel));
    REQUIRE(label != nullptr);
    label->showEditor();
    REQUIRE(PrimaryEditableLabelTestAccess::commitEditorText(
        *label, "2.5 kHz"));
    CHECK(editCalls == 1);
    CHECK(std::isfinite(lastNormalisedFrequency));

    label->showEditor();
    REQUIRE(PrimaryEditableLabelTestAccess::commitEditorText(
        *label, "2,5 kHz"));
    CHECK(editCalls == 1);
}

TEST_CASE("Frequency label lifecycle focus loss discards hidden or disabled text",
          "[frequency-label][ui][input][focus][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto runLifecycleBoundary = [](bool hide)
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);

        int gestureBegins = 0;
        int gestureEnds = 0;
        int editCalls = 0;
        fixture.divider.setParameterGestureCallbacks(
            [&gestureBegins] { ++gestureBegins; },
            [] { return std::make_shared<int>(0); },
            [&gestureEnds] { ++gestureEnds; });
        fixture.frequencyLabel.setFrequencyEditCallback(
            [&editCalls](float) { ++editCalls; });

        label->showEditor();
        auto* editor = label->getCurrentTextEditor();
        REQUIRE(editor != nullptr);
        editor->setText("2 kHz", false);

        if (hide)
        {
            // Hiding the owning bubble now discards and destroys the editor
            // synchronously. Do not send a synthetic focus event through the
            // TextEditor pointer after crossing that lifecycle boundary.
            fixture.frequencyLabel.setVisible(false);
        }
        else
        {
            juce::Component::SafePointer<juce::TextEditor> editorLifetime(
                editor);
            fixture.frequencyLabel.setEnabled(false);

            REQUIRE_FALSE(label->isBeingEdited());
            REQUIRE(editorLifetime == nullptr);
            fixture.frequencyLabel.setEnabled(true);
            static_cast<juce::Component&>(*label).focusLost(
                juce::Component::focusChangedDirectly);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        CHECK_FALSE(label->isBeingEdited());
        CHECK(label->getText() == "1 kHz");
        CHECK(editCalls == 0);
        CHECK(gestureBegins == 0);
        CHECK(gestureEnds == 0);
    };

    SECTION("hidden")
    {
        runLifecycleBoundary(true);
    }

    SECTION("disabled")
    {
        runLifecycleBoundary(false);
    }
}

TEST_CASE("Frequency label editing preserves keyboard and accessibility entry",
          "[frequency-label][ui][input][keyboard][accessibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("tab focus retains JUCE single-click edit behaviour")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);
        label->setEditable(true, false);

        static_cast<juce::Component&>(*label).focusGained(
            juce::Component::focusChangedByTabKey);

        CHECK(label->isBeingEdited());
    }

    SECTION("the editable-text accessibility press action still opens the editor")
    {
        FrequencyLabelFixture fixture;
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);
        label->setTooltip("Enter a crossover frequency");

        auto* handler = label->getAccessibilityHandler();
        REQUIRE(handler != nullptr);
        CHECK(handler->getRole() == juce::AccessibilityRole::editableText);
        CHECK(handler->getTitle() == label->getText());
        CHECK(handler->getHelp() == label->getTooltip());
        auto* value = handler->getValueInterface();
        REQUIRE(value != nullptr);
        CHECK(value->isReadOnly());
        CHECK(value->getCurrentValueAsString() == label->getText());
        REQUIRE(handler->getActions().contains(
            juce::AccessibilityActionType::press));

        CHECK(handler->getActions().invoke(
            juce::AccessibilityActionType::press));
        CHECK(label->isBeingEdited());
        CHECK_FALSE(handler->getCurrentState().isFocusable());
    }
}

TEST_CASE("Fire label painting leaves editable text to the active editor",
          "[frequency-label][ui][paint][editing]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    juce::Component desktopHost;
    PrimaryEditableLabel label({}, "1 kHz");
    desktopHost.setBounds(0, 0, 120, 40);
    desktopHost.setVisible(false);
    desktopHost.addAndMakeVisible(label);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);
    label.setBounds(0, 0, 90, 24);
    label.setEditable(true);
    label.setLookAndFeel(&lookAndFeel);
    label.setColour(juce::Label::backgroundColourId,
                    juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId,
                    juce::Colours::transparentBlack);
    label.setColour(juce::Label::textColourId,
                    juce::Colours::white);
    label.setColour(juce::Label::outlineWhenEditingColourId,
                    juce::Colours::transparentBlack);
    label.setColour(juce::Label::textWhenEditingColourId,
                    juce::Colours::white);

    const auto countPaintedPixels = [](const juce::Image& image)
    {
        int count = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                if (image.getPixelAt(x, y).getAlpha() != 0)
                    ++count;
        return count;
    };

    juce::Image normalLayer(juce::Image::ARGB, 90, 24, true);
    juce::Graphics normalGraphics(normalLayer);
    lookAndFeel.drawLabel(normalGraphics, label);
    CHECK(countPaintedPixels(normalLayer) > 0);

    label.showEditor();
    REQUIRE(label.isBeingEdited());

    juce::Image transparentEditingLayer(juce::Image::ARGB, 90, 24, true);
    juce::Graphics transparentEditingGraphics(transparentEditingLayer);
    lookAndFeel.drawLabel(transparentEditingGraphics, label);
    CHECK(countPaintedPixels(transparentEditingLayer) == 0);

    label.setColour(juce::Label::outlineWhenEditingColourId,
                    juce::Colours::red);
    juce::Image outlinedEditingLayer(juce::Image::ARGB, 90, 24, true);
    juce::Graphics outlinedEditingGraphics(outlinedEditingLayer);
    lookAndFeel.drawLabel(outlinedEditingGraphics, label);
    CHECK(countPaintedPixels(outlinedEditingLayer) > 0);
    CHECK(outlinedEditingLayer.getPixelAt(45, 12) == fire::ui::colours::raised);

    label.hideEditor(true);
    label.setLookAndFeel(nullptr);
}

TEST_CASE("Cached frequency-label accessibility rejects stale lifecycle actions",
          "[frequency-label][ui][input][accessibility][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto cacheAccessibility = [](FrequencyLabelFixture& fixture)
    {
        auto* label = fixture.getEditableLabel();
        REQUIRE(label != nullptr);
        auto* handler = label->getAccessibilityHandler();
        REQUIRE(handler != nullptr);
        REQUIRE(handler->getActions().contains(
            juce::AccessibilityActionType::press));
        return std::pair { label, handler };
    };

    SECTION("hidden label")
    {
        FrequencyLabelFixture fixture;
        const auto [label, handler] = cacheAccessibility(fixture);

        label->setVisible(false);
        REQUIRE_FALSE(label->isShowing());
        REQUIRE(handler->getActions().invoke(
            juce::AccessibilityActionType::press));

        CHECK_FALSE(label->isBeingEdited());
    }

    SECTION("disabled label")
    {
        FrequencyLabelFixture fixture;
        const auto [label, handler] = cacheAccessibility(fixture);

        label->setEnabled(false);
        REQUIRE_FALSE(label->isEnabled());
        REQUIRE(handler->getActions().invoke(
            juce::AccessibilityActionType::press));

        CHECK_FALSE(label->isBeingEdited());
    }

    SECTION("detached peer")
    {
        PrimaryEditableLabel label({}, "1 kHz");
        label.setBounds(0, 0, 90, 24);
        label.setEditable(true);
        label.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        label.setVisible(true);
        auto* handler = label.getAccessibilityHandler();
        REQUIRE(handler != nullptr);
        REQUIRE(handler->getActions().contains(
            juce::AccessibilityActionType::press));

        label.removeFromDesktop();
        REQUIRE(label.isVisible());
        REQUIRE_FALSE(label.isShowing());
        REQUIRE(handler->getActions().invoke(
            juce::AccessibilityActionType::press));

        CHECK_FALSE(label.isBeingEdited());
    }
}

TEST_CASE("Frequency text gestures begin only for a valid commit and survive deletion",
          "[frequency-label][ui][automation][gesture][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    VerticalLine divider;
    divider.setRange(40.0, 10024.0, 1.0);
    divider.setValue(1000.0, juce::dontSendNotification);

    std::unique_ptr<FreqTextLabel> frequencyLabel =
        std::make_unique<FreqTextLabel>(divider);
    juce::Component desktopHost;
    desktopHost.setBounds(0, 0, 140, 64);
    desktopHost.setVisible(false);
    desktopHost.addAndMakeVisible(*frequencyLabel);
    frequencyLabel->setBounds(0, 0, 90, 24);
    frequencyLabel->setVisible(true);
    frequencyLabel->setFreq(1000);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);

    int gestureBegins = 0;
    int gestureEnds = 0;
    int editCalls = 0;
    bool deleteOnBegin = false;
    divider.setParameterGestureCallbacks(
        [&]
        {
            ++gestureBegins;
            if (deleteOnBegin)
                frequencyLabel.reset();
        },
        [] { return std::make_shared<int>(0); },
        [&gestureEnds] { ++gestureEnds; });
    frequencyLabel->setFrequencyEditCallback(
        [&editCalls](float) { ++editCalls; });

    auto* label = findDescendant<juce::Label>(*frequencyLabel);
    REQUIRE(label != nullptr);
    label->showEditor();

    CHECK(gestureBegins == 0);
    REQUIRE(frequencyLabel != nullptr);
    deleteOnBegin = true;
    enterTextAndPostReturn(*label, "2 kHz");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    CHECK(frequencyLabel == nullptr);
    CHECK(gestureBegins == 1);
    CHECK(gestureEnds == 1);
    CHECK(editCalls == 0);
}
