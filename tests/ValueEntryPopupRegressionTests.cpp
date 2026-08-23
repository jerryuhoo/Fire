#include <GUI/InterfaceDefines.h>
#include <GUI/ValueEntryPopup.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

struct ValueEntryPopupTestAccess
{
    static void open(ValueEntryPopup& popup)
    {
        popup.setVisible(true);
    }

    static void setText(ValueEntryPopup& popup, const juce::String& text)
    {
        popup.editor.setText(text, juce::dontSendNotification);
    }

    static bool submit(ValueEntryPopup& popup)
    {
        return popup.submitEditorText();
    }

    static juce::Colour outlineColour(const ValueEntryPopup& popup)
    {
        return popup.editor.findColour(juce::TextEditor::outlineColourId);
    }

    static juce::String text(const ValueEntryPopup& popup)
    {
        return popup.editor.getText();
    }

    static bool pressReturn(ValueEntryPopup& popup)
    {
        return static_cast<juce::Component&>(popup.editor).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey });
    }

    static bool pressEscape(ValueEntryPopup& popup)
    {
        return static_cast<juce::Component&>(popup.editor).keyPressed(
            juce::KeyPress { juce::KeyPress::escapeKey });
    }

    static void loseFocus(ValueEntryPopup& popup)
    {
        popup.focusOfChildComponentChanged(
            juce::Component::FocusChangeType::focusChangedDirectly);
    }

    static void mouseDown(ValueEntryPopup& popup,
                          juce::Component& originalComponent,
                          juce::ModifierKeys modifiers)
    {
        popup.mouseDown(makeMouseEvent(originalComponent, modifiers));
    }

    static void clickOk(ValueEntryPopup& popup,
                        juce::ModifierKeys downModifiers,
                        juce::ModifierKeys upModifiers = {})
    {
        auto& button = static_cast<juce::Component&>(popup.okButton);
        button.mouseDown(makeMouseEvent(button, downModifiers));
        button.mouseUp(makeMouseEvent(button, upModifiers));
    }

    static void pressOkWithoutRelease(ValueEntryPopup& popup,
                                      juce::ModifierKeys downModifiers)
    {
        auto& button = static_cast<juce::Component&>(popup.okButton);
        button.mouseDown(makeMouseEvent(button, downModifiers));
    }

    static bool okIsDown(const ValueEntryPopup& popup)
    {
        return popup.okButton.isDown();
    }

    static bool pressOkReturn(ValueEntryPopup& popup)
    {
        return static_cast<juce::Component&>(popup.okButton).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey });
    }

    static void releaseOk(ValueEntryPopup& popup)
    {
        auto& button = static_cast<juce::Component&>(popup.okButton);
        button.mouseUp(makeMouseEvent(button, {}));
    }

    static void clickCancel(ValueEntryPopup& popup,
                            juce::ModifierKeys downModifiers,
                            juce::ModifierKeys upModifiers = {})
    {
        auto& button = static_cast<juce::Component&>(popup.cancelButton);
        button.mouseDown(makeMouseEvent(button, downModifiers));
        button.mouseUp(makeMouseEvent(button, upModifiers));
    }

private:
    static juce::MouseEvent makeMouseEvent(juce::Component& component,
                                           juce::ModifierKeys modifiers)
    {
        const auto position = component.getLocalBounds().toFloat().getCentre();
        const auto time = juce::Time::getCurrentTime();
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
                 time,
                 position,
                 time,
                 1,
                 false };
    }
};

namespace
{
template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;

    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
        if (auto* child = root.getChildComponent(childIndex))
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;

    return nullptr;
}
} // namespace

TEST_CASE("Value entry popup rejects incomplete and non-finite numbers",
          "[ui][modulation][value-entry][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ValueEntryPopup popup;
    ValueEntryPopupTestAccess::open(popup);
    int acceptedCount = 0;
    double acceptedValue = 0.0;
    popup.onOk = [&](double value)
    {
        ++acceptedCount;
        acceptedValue = value;
    };

    for (const auto& invalidText : { juce::String {},
                                    juce::String { "   " },
                                    juce::String { "abc" },
                                    juce::String { "12junk" },
                                    juce::String { "NaN" },
                                    juce::String { "inf" },
                                    juce::String { "1e9999" } })
    {
        INFO("input: " << invalidText);
        ValueEntryPopupTestAccess::setText(popup, invalidText);
        CHECK_FALSE(ValueEntryPopupTestAccess::submit(popup));
        CHECK(acceptedCount == 0);
        CHECK(ValueEntryPopupTestAccess::outlineColour(popup)
              == fire::ui::colours::danger);
    }

    ValueEntryPopupTestAccess::setText(popup, "  -12.5e-1  ");
    CHECK(ValueEntryPopupTestAccess::submit(popup));
    CHECK(acceptedCount == 1);
    CHECK(acceptedValue == Catch::Approx(-1.25));
    CHECK(ValueEntryPopupTestAccess::outlineColour(popup)
          == fire::ui::colours::hairline);
    CHECK_FALSE(popup.isVisible());
    CHECK(ValueEntryPopupTestAccess::text(popup).isEmpty());
}

TEST_CASE("Value entry popup completes each session at most once",
          "[ui][modulation][value-entry][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ValueEntryPopup popup;
    int acceptedCount = 0;
    int cancelledCount = 0;
    double acceptedValue = 0.0;
    popup.onOk = [&](double value)
    {
        ++acceptedCount;
        acceptedValue = value;
    };
    popup.onCancel = [&] { ++cancelledCount; };

    SECTION("Return commits synchronously and cannot be replayed")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "6.25");
        CHECK(ValueEntryPopupTestAccess::pressReturn(popup));
        CHECK(ValueEntryPopupTestAccess::pressReturn(popup));

        CHECK(acceptedCount == 1);
        CHECK(cancelledCount == 0);
        CHECK(acceptedValue == Catch::Approx(6.25));
        CHECK_FALSE(popup.isVisible());
        CHECK(ValueEntryPopupTestAccess::text(popup).isEmpty());
    }

    SECTION("Escape cancels once and clears stale input")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "42");
        CHECK(ValueEntryPopupTestAccess::pressEscape(popup));
        CHECK(ValueEntryPopupTestAccess::pressEscape(popup));

        CHECK(acceptedCount == 0);
        CHECK(cancelledCount == 1);
        CHECK_FALSE(popup.isVisible());
        CHECK(ValueEntryPopupTestAccess::text(popup).isEmpty());
    }

    SECTION("A new session does not inherit the previous text or completion")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "10");
        ValueEntryPopupTestAccess::pressEscape(popup);

        ValueEntryPopupTestAccess::open(popup);
        CHECK(ValueEntryPopupTestAccess::text(popup).isEmpty());
        ValueEntryPopupTestAccess::setText(popup, "11");
        ValueEntryPopupTestAccess::pressReturn(popup);

        CHECK(cancelledCount == 1);
        CHECK(acceptedCount == 1);
        CHECK(acceptedValue == Catch::Approx(11.0));
    }

    SECTION("Return cannot leave a queued completion for the next session")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "12");
        REQUIRE(ValueEntryPopupTestAccess::pressReturn(popup));

        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "99");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);

        CHECK(acceptedCount == 1);
        CHECK(cancelledCount == 0);
        CHECK(acceptedValue == Catch::Approx(12.0));
        CHECK(popup.isVisible());
        CHECK(ValueEntryPopupTestAccess::text(popup) == "99");
    }
}

TEST_CASE("Value entry popup cancels when focus or pointer leaves its session",
          "[ui][modulation][value-entry][dismissal]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ValueEntryPopup popup;
    juce::Component outsideComponent;
    int cancelledCount = 0;
    juce::String target { "drive0" };
    popup.onCancel = [&]
    {
        ++cancelledCount;
        target.clear();
    };

    SECTION("Clicking outside cancels and releases the captured target")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "75");
        ValueEntryPopupTestAccess::mouseDown(
            popup,
            outsideComponent,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(cancelledCount == 1);
        CHECK(target.isEmpty());
        CHECK_FALSE(popup.isVisible());
        CHECK(ValueEntryPopupTestAccess::text(popup).isEmpty());
    }

    SECTION("Losing keyboard focus cancels")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::loseFocus(popup);

        CHECK(cancelledCount == 1);
        CHECK(target.isEmpty());
        CHECK_FALSE(popup.isVisible());
    }
}

TEST_CASE("Value entry buttons require a primary click",
          "[ui][modulation][value-entry][input]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ValueEntryPopup popup;
    int acceptedCount = 0;
    int cancelledCount = 0;
    popup.onOk = [&](double) { ++acceptedCount; };
    popup.onCancel = [&] { ++cancelledCount; };

    ValueEntryPopupTestAccess::open(popup);
    ValueEntryPopupTestAccess::setText(popup, "3.5");
    for (const auto modifiers : {
             juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
             juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier } })
    {
        ValueEntryPopupTestAccess::clickOk(popup, modifiers);
        ValueEntryPopupTestAccess::clickCancel(popup, modifiers);
    }

    CHECK(acceptedCount == 0);
    CHECK(cancelledCount == 0);
    CHECK(popup.isVisible());

#if JUCE_MAC
    const auto controlClick = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::ctrlModifier };
    ValueEntryPopupTestAccess::clickOk(
        popup,
        controlClick,
        juce::ModifierKeys { juce::ModifierKeys::ctrlModifier });
    ValueEntryPopupTestAccess::clickCancel(
        popup,
        controlClick,
        juce::ModifierKeys { juce::ModifierKeys::ctrlModifier });

    CHECK(acceptedCount == 0);
    CHECK(cancelledCount == 0);
    CHECK(popup.isVisible());
#endif

    const auto leftClick = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier };
    ValueEntryPopupTestAccess::clickOk(popup, leftClick);
    ValueEntryPopupTestAccess::clickOk(popup, leftClick);

    CHECK(acceptedCount == 1);
    CHECK(cancelledCount == 0);
    CHECK_FALSE(popup.isVisible());

    ValueEntryPopupTestAccess::open(popup);
    ValueEntryPopupTestAccess::setText(popup, "8.0");
    ValueEntryPopupTestAccess::clickCancel(popup, leftClick);
    ValueEntryPopupTestAccess::clickCancel(popup, leftClick);

    CHECK(acceptedCount == 1);
    CHECK(cancelledCount == 1);
    CHECK_FALSE(popup.isVisible());
}

TEST_CASE("Value entry buttons reject stale releases after popup dismissal",
          "[ui][modulation][value-entry][input][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ValueEntryPopup popup;
    int acceptedCount = 0;
    int cancelledCount = 0;
    popup.onOk = [&](double) { ++acceptedCount; };
    popup.onCancel = [&] { ++cancelledCount; };

    ValueEntryPopupTestAccess::open(popup);
    ValueEntryPopupTestAccess::setText(popup, "3.5");
    ValueEntryPopupTestAccess::pressOkWithoutRelease(
        popup,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
    REQUIRE(ValueEntryPopupTestAccess::okIsDown(popup));

    popup.setVisible(false);
    CHECK(cancelledCount == 1);
    CHECK_FALSE(ValueEntryPopupTestAccess::okIsDown(popup));

    ValueEntryPopupTestAccess::open(popup);
    ValueEntryPopupTestAccess::setText(popup, "9.0");
    ValueEntryPopupTestAccess::clickOk(
        popup,
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier });

    CHECK(acceptedCount == 0);
    CHECK(cancelledCount == 1);
    CHECK(popup.isVisible());
}

TEST_CASE("Value entry button Return is synchronous across popup sessions",
          "[ui][modulation][value-entry][input][keyboard][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ValueEntryPopup popup;
    int acceptedCount = 0;
    double acceptedValue = 0.0;
    popup.onOk = [&](double value)
    {
        ++acceptedCount;
        acceptedValue = value;
    };

    ValueEntryPopupTestAccess::open(popup);
    ValueEntryPopupTestAccess::setText(popup, "4.5");
    REQUIRE(ValueEntryPopupTestAccess::pressOkReturn(popup));
    CHECK(acceptedCount == 1);
    CHECK(acceptedValue == Catch::Approx(4.5));
    CHECK_FALSE(popup.isVisible());

    ValueEntryPopupTestAccess::open(popup);
    ValueEntryPopupTestAccess::setText(popup, "8.5");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);

    CHECK(acceptedCount == 1);
    CHECK(popup.isVisible());
    CHECK(ValueEntryPopupTestAccess::text(popup) == "8.5");
}

TEST_CASE("Value entry keyboard completion may delete its popup",
          "[ui][modulation][value-entry][keyboard][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("Return callback deletes the popup")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* rawPopup = popup.get();
        rawPopup->onOk = [&](double) { popup.reset(); };
        ValueEntryPopupTestAccess::open(*rawPopup);
        ValueEntryPopupTestAccess::setText(*rawPopup, "7.5");

        const auto consumed = ValueEntryPopupTestAccess::pressReturn(*rawPopup);
        CHECK(consumed);
        CHECK(popup == nullptr);
    }

    SECTION("Escape callback deletes the popup")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* rawPopup = popup.get();
        rawPopup->onCancel = [&] { popup.reset(); };
        ValueEntryPopupTestAccess::open(*rawPopup);

        const auto consumed = ValueEntryPopupTestAccess::pressEscape(*rawPopup);
        CHECK(consumed);
        CHECK(popup == nullptr);
    }
}

TEST_CASE("Editor hiding cancels value entry ownership immediately",
          "[ui][modulation][value-entry][host-visibility][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    auto* popup = findDescendant<ValueEntryPopup>(*editor);
    REQUIRE(popup != nullptr);

    int acceptedCount = 0;
    int cancelledCount = 0;
    popup->onOk = [&](double) { ++acceptedCount; };
    popup->onCancel = [&] { ++cancelledCount; };

    ValueEntryPopupTestAccess::open(*popup);
    ValueEntryPopupTestAccess::setText(*popup, "3.5");
    ValueEntryPopupTestAccess::pressOkWithoutRelease(
        *popup,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
    REQUIRE(ValueEntryPopupTestAccess::okIsDown(*popup));

    editor->setVisible(false);
    CHECK(cancelledCount == 1);
    CHECK_FALSE(popup->isVisible());
    CHECK_FALSE(ValueEntryPopupTestAccess::okIsDown(*popup));

    editor->setVisible(true);
    ValueEntryPopupTestAccess::open(*popup);
    ValueEntryPopupTestAccess::setText(*popup, "9.0");
    ValueEntryPopupTestAccess::releaseOk(*popup);

    CHECK(acceptedCount == 0);
    CHECK(cancelledCount == 1);
    CHECK(popup->isVisible());
}
