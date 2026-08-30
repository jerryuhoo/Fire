#include <GUI/InterfaceDefines.h>
#include <GUI/PrimaryButton.h>
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

    static juce::Button& okButton(ValueEntryPopup& popup)
    {
        return popup.okButton;
    }

    static void forceOkButtonDown(ValueEntryPopup& popup)
    {
        popup.okButton.setState(juce::Button::buttonDown);
    }

    static void resetTransientState(ValueEntryPopup& popup)
    {
        popup.resetTransientState();
    }

    static void moveOk(ValueEntryPopup& popup,
                       juce::ModifierKeys modifiers = {})
    {
        auto& button = static_cast<juce::Component&>(popup.okButton);
        button.mouseMove(makeMouseEvent(button, modifiers));
    }

    static void triggerOk(ValueEntryPopup& popup)
    {
        popup.okButton.triggerClick();
    }

    static void triggerCancel(ValueEntryPopup& popup)
    {
        popup.cancelButton.triggerClick();
    }

    static bool buttonsUseSharedPrimaryControl(const ValueEntryPopup& popup)
    {
        return dynamic_cast<const ::PrimaryTextButton*>(&popup.okButton) != nullptr
            && dynamic_cast<const ::PrimaryTextButton*>(&popup.cancelButton) != nullptr;
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

class DeletePopupOnVisibility final : public juce::ComponentListener
{
public:
    explicit DeletePopupOnVisibility(
        std::unique_ptr<ValueEntryPopup>& popupToDelete,
        bool deleteWhenVisible = false)
        : popup(popupToDelete), expectedVisibility(deleteWhenVisible)
    {
    }

    void componentVisibilityChanged(juce::Component& component) override
    {
        if (popup != nullptr && component.isVisible() == expectedVisibility)
            popup.reset();
    }

private:
    std::unique_ptr<ValueEntryPopup>& popup;
    bool expectedVisibility = false;
};

class DeletePopupOnButtonState final : public juce::Button::Listener
{
public:
    DeletePopupOnButtonState(juce::Button& buttonToWatch,
                             std::unique_ptr<ValueEntryPopup>& popupToDelete)
        : popup(popupToDelete)
    {
        buttonToWatch.addListener(this);
    }

    void buttonClicked(juce::Button*) override {}

    void buttonStateChanged(juce::Button*) override
    {
        if (popup != nullptr)
            popup.reset();
    }

private:
    std::unique_ptr<ValueEntryPopup>& popup;
};
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
                                    juce::String { "1e100" },
                                    juce::String { "-1e100" },
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

TEST_CASE("Value entry buttons recover a lost primary release through the shared control",
          "[ui][modulation][value-entry][input][lifecycle][stale][primary-button]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ValueEntryPopup popup;
    int acceptedCount = 0;
    int cancelledCount = 0;
    popup.onOk = [&](double) { ++acceptedCount; };
    popup.onCancel = [&] { ++cancelledCount; };

    CHECK(ValueEntryPopupTestAccess::buttonsUseSharedPrimaryControl(popup));
    ValueEntryPopupTestAccess::open(popup);
    ValueEntryPopupTestAccess::setText(popup, "3.5");
    ValueEntryPopupTestAccess::pressOkWithoutRelease(
        popup,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
    REQUIRE(ValueEntryPopupTestAccess::okIsDown(popup));

    ValueEntryPopupTestAccess::moveOk(popup);
    CHECK_FALSE(ValueEntryPopupTestAccess::okIsDown(popup));

    // A release delivered after ownership recovery belongs to the abandoned
    // gesture and must not submit the still-active popup session.
    ValueEntryPopupTestAccess::releaseOk(popup);
    CHECK(acceptedCount == 0);
    CHECK(cancelledCount == 0);
    CHECK(popup.isVisible());

    ValueEntryPopupTestAccess::clickOk(
        popup,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
    CHECK(acceptedCount == 1);
    CHECK(cancelledCount == 0);
    CHECK_FALSE(popup.isVisible());
}

TEST_CASE("Value entry button commands cannot cross popup sessions",
          "[ui][modulation][value-entry][input][accessibility][lifecycle]")
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

    SECTION("OK command")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "4.5");
        ValueEntryPopupTestAccess::triggerOk(popup);

        CHECK(acceptedCount == 1);
        CHECK(acceptedValue == Catch::Approx(4.5));
        CHECK(cancelledCount == 0);
        CHECK_FALSE(popup.isVisible());

        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "8.5");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);

        CHECK(acceptedCount == 1);
        CHECK(acceptedValue == Catch::Approx(4.5));
        CHECK(cancelledCount == 0);
        CHECK(popup.isVisible());
        CHECK(ValueEntryPopupTestAccess::text(popup) == "8.5");
    }

    SECTION("Cancel command")
    {
        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "4.5");
        ValueEntryPopupTestAccess::triggerCancel(popup);

        CHECK(acceptedCount == 0);
        CHECK(cancelledCount == 1);
        CHECK_FALSE(popup.isVisible());

        ValueEntryPopupTestAccess::open(popup);
        ValueEntryPopupTestAccess::setText(popup, "8.5");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);

        CHECK(acceptedCount == 0);
        CHECK(cancelledCount == 1);
        CHECK(popup.isVisible());
        CHECK(ValueEntryPopupTestAccess::text(popup) == "8.5");
    }
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

TEST_CASE("Value entry lifecycle callbacks may delete the popup synchronously",
          "[ui][modulation][value-entry][lifecycle][self-delete][reentrant]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("submit stops when the hide notification deletes the popup")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* const rawPopup = popup.get();
        int acceptedCount = 0;
        rawPopup->onOk = [&](double) { ++acceptedCount; };
        ValueEntryPopupTestAccess::open(*rawPopup);
        ValueEntryPopupTestAccess::setText(*rawPopup, "7.5");

        DeletePopupOnVisibility deleteOnHide(popup);
        rawPopup->addComponentListener(&deleteOnHide);

        CHECK(ValueEntryPopupTestAccess::submit(*rawPopup));
        CHECK(popup == nullptr);
        CHECK(acceptedCount == 0);
    }

    SECTION("dismiss stops when the hide notification deletes the popup")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* const rawPopup = popup.get();
        int cancelledCount = 0;
        rawPopup->onCancel = [&] { ++cancelledCount; };
        ValueEntryPopupTestAccess::open(*rawPopup);

        DeletePopupOnVisibility deleteOnHide(popup);
        rawPopup->addComponentListener(&deleteOnHide);

        rawPopup->dismissSession();
        CHECK(popup == nullptr);
        CHECK(cancelledCount == 0);
    }

    SECTION("transient reset stops when button dismissal deletes the popup")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* const rawPopup = popup.get();
        ValueEntryPopupTestAccess::forceOkButtonDown(*rawPopup);
        REQUIRE(ValueEntryPopupTestAccess::okIsDown(*rawPopup));

        DeletePopupOnButtonState deleteOnStateChange(
            ValueEntryPopupTestAccess::okButton(*rawPopup), popup);
        ValueEntryPopupTestAccess::resetTransientState(*rawPopup);

        CHECK(popup == nullptr);
    }
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

TEST_CASE("Value entry pointer and command completion may delete its popup",
          "[ui][modulation][value-entry][input][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("OK pointer callback")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* rawPopup = popup.get();
        rawPopup->onOk = [&](double) { popup.reset(); };
        ValueEntryPopupTestAccess::open(*rawPopup);
        ValueEntryPopupTestAccess::setText(*rawPopup, "7.5");

        ValueEntryPopupTestAccess::clickOk(*rawPopup, leftButton);
        CHECK(popup == nullptr);
    }

    SECTION("Cancel pointer callback")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* rawPopup = popup.get();
        rawPopup->onCancel = [&] { popup.reset(); };
        ValueEntryPopupTestAccess::open(*rawPopup);

        ValueEntryPopupTestAccess::clickCancel(*rawPopup, leftButton);
        CHECK(popup == nullptr);
    }

    SECTION("synchronous command callback")
    {
        auto popup = std::make_unique<ValueEntryPopup>();
        auto* rawPopup = popup.get();
        rawPopup->onOk = [&](double) { popup.reset(); };
        ValueEntryPopupTestAccess::open(*rawPopup);
        ValueEntryPopupTestAccess::setText(*rawPopup, "7.5");

        ValueEntryPopupTestAccess::triggerOk(*rawPopup);
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
