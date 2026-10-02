#include <GUI/PrimaryButton.h>
#include <GUI/PrimaryEditableLabel.h>
#include <GUI/Skin.h>
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

namespace
{
const juce::ModifierKeys primary {juce::ModifierKeys::leftButtonModifier};

juce::MouseEvent pointerEvent(juce::Component& component, juce::ModifierKeys modifiers,
                              int clicks = 1)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
    const auto time = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(), position, modifiers,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component,
            time, position, time, clicks, false};
}

template <typename Predicate>
bool dispatchUntil(const Predicate& finished)
{
    const auto started = juce::Time::getMillisecondCounter();
    while (! finished())
        if (juce::Time::getMillisecondCounter() - started >= 1000
            || ! juce::MessageManager::getInstance()->runDispatchLoopUntil(5))
            return false;
    return true;
}

template <typename Control>
struct PeerFixture
{
    juce::Component host;
    Control control;

    PeerFixture()
    {
        host.setBounds(0, 0, 180, 80);
        control.setBounds(12, 12, 140, 28);
        host.addAndMakeVisible(control);
        host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        host.setVisible(true);
    }

    ~PeerFixture()
    {
        if constexpr (std::is_base_of_v<juce::Label, Control>)
        {
            control.onTextChange = nullptr;
            control.onEditorShow = nullptr;
            control.onEditorHide = nullptr;
            control.hideEditor(true);
        }
        else
        {
            control.onClick = nullptr;
            control.onStateChange = nullptr;
        }
        host.removeChildComponent(&control);
        host.removeFromDesktop();
    }

    void replacePeerWithoutDispatch()
    {
        REQUIRE(host.getPeer() != nullptr);
        const auto previousID = host.getPeer()->getUniqueID();
        // Deliberately do not pump messages while the hierarchy has no peer.
        // A pointer comparison or isShowing-only monitor misses this ABA.
        host.removeFromDesktop();
        host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        REQUIRE(host.getPeer() != nullptr);
        REQUIRE(host.getPeer()->getUniqueID() != previousID);
        REQUIRE(control.isShowing());
    }
};

template <typename Button>
void checkStaleButtonRelease(bool sendDrag)
{
    PeerFixture<Button> fixture;
    int clicks = 0;
    fixture.control.onClick = [&] { ++clicks; };
    auto& component = static_cast<juce::Component&>(fixture.control);
    component.mouseDown(pointerEvent(component, primary));
    REQUIRE(fixture.control.isDown());
    fixture.replacePeerWithoutDispatch();
    if (sendDrag)
    {
        component.mouseDrag(pointerEvent(component, primary));
        CHECK_FALSE(fixture.control.isDown());
    }
    component.mouseUp(pointerEvent(component, {}));
    CHECK_FALSE(fixture.control.isDown());
    CHECK(clicks == 0);

    // A new gesture on the replacement peer is valid immediately.
    component.mouseDown(pointerEvent(component, primary));
    component.mouseUp(pointerEvent(component, {}));
    CHECK(clicks == 1);
}
}

TEST_CASE("Primary button gestures cannot cross an unobserved native peer replacement",
          "[peer-epoch][header-button][ui][input][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SECTION("text release") { checkStaleButtonRelease<PrimaryTextButton>(false); }
    SECTION("toggle release") { checkStaleButtonRelease<PrimaryToggleButton>(false); }
    SECTION("hyperlink release") { checkStaleButtonRelease<PrimaryHyperlinkButton>(false); }
    SECTION("drag before release") { checkStaleButtonRelease<PrimaryTextButton>(true); }
}

TEST_CASE("Hidden primary button animation cleanup cancels its peer gesture before stopping",
          "[peer-epoch][header-button][ui][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PeerFixture<PrimaryTextButton> fixture;
    int clicks = 0;
    fixture.control.onClick = [&] { ++clicks; };
    auto& component = static_cast<juce::Component&>(fixture.control);
    component.mouseDown(pointerEvent(component, primary));
    REQUIRE(fixture.control.isDown());
    fixture.host.removeFromDesktop();
    // Focus presentation can update before the lifecycle timer notices loss.
    fixture.control.focusLost(juce::Component::focusChangedDirectly);
    CHECK_FALSE(fixture.control.isDown());
    fixture.host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.mouseUp(pointerEvent(component, {}));
    CHECK(clicks == 0);
}

TEST_CASE("Editable label clicks and double clicks are bound to their native peer",
          "[peer-epoch][frequency-label][ui][input][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PeerFixture<PrimaryEditableLabel> fixture;
    fixture.control.setText("1 kHz", juce::dontSendNotification);
    auto& component = static_cast<juce::Component&>(fixture.control);

    SECTION("pending single click")
    {
        fixture.control.setEditable(true, false);
        component.mouseDown(pointerEvent(component, primary));
        fixture.replacePeerWithoutDispatch();
        component.mouseUp(pointerEvent(component, {}));
        CHECK_FALSE(fixture.control.isBeingEdited());
        component.mouseDown(pointerEvent(component, primary));
        component.mouseUp(pointerEvent(component, {}));
        CHECK(fixture.control.isBeingEdited());
    }

    SECTION("completed double click authorization")
    {
        fixture.control.setEditable(false, true);
        component.mouseDown(pointerEvent(component, primary, 2));
        const auto release = pointerEvent(component, {}, 2);
        component.mouseUp(release);
        REQUIRE_FALSE(fixture.control.isBeingEdited());
        fixture.replacePeerWithoutDispatch();
        component.mouseDoubleClick(release);
        CHECK_FALSE(fixture.control.isBeingEdited());
    }
}

TEST_CASE("Editable label text from a replaced peer cannot commit",
          "[peer-epoch][frequency-label][ui][editing][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PeerFixture<PrimaryEditableLabel> fixture;
    fixture.control.setText("1 kHz", juce::dontSendNotification);
    fixture.control.setEditable(true);
    int changes = 0;
    fixture.control.onTextChange = [&] { ++changes; };
    fixture.control.showEditor();
    REQUIRE(fixture.control.getCurrentTextEditor() != nullptr);
    fixture.control.getCurrentTextEditor()->setText("2 kHz", false);
    fixture.replacePeerWithoutDispatch();

    SECTION("explicit base Label commit")
    {
        static_cast<juce::Label&>(fixture.control).hideEditor(false);
        CHECK_FALSE(fixture.control.isBeingEdited());
    }

    SECTION("Return on surviving editor")
    {
        if (auto* editor = fixture.control.getCurrentTextEditor())
            editor->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        REQUIRE(dispatchUntil([&] { return ! fixture.control.isBeingEdited(); }));
    }

    SECTION("lifecycle monitor")
    {
        REQUIRE(dispatchUntil([&] { return ! fixture.control.isBeingEdited(); }));
    }

    CHECK(fixture.control.getText() == "1 kHz");
    CHECK(changes == 0);
}

TEST_CASE("Resizing and changing skin preserve gestures on the same peer",
          "[peer-epoch][ui][skin][input][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SECTION("button click")
    {
        PeerFixture<PrimaryTextButton> fixture;
        int clicks = 0;
        fixture.control.onClick = [&] { ++clicks; };
        auto& component = static_cast<juce::Component&>(fixture.control);
        component.mouseDown(pointerEvent(component, primary));
        REQUIRE(fixture.host.getPeer() != nullptr);
        const auto peerID = fixture.host.getPeer()->getUniqueID();
        fixture.host.setSize(220, 100);
        fire::ui::setSkin(fixture.host, fire::ui::Skin::vintage);
        fixture.host.sendLookAndFeelChange();
        REQUIRE(fixture.host.getPeer()->getUniqueID() == peerID);
        component.mouseUp(pointerEvent(component, {}));
        CHECK(clicks == 1);
    }

    SECTION("label editor commit")
    {
        PeerFixture<PrimaryEditableLabel> fixture;
        fixture.control.setText("1 kHz", juce::dontSendNotification);
        fixture.control.setEditable(true);
        fixture.control.showEditor();
        REQUIRE(fixture.control.getCurrentTextEditor() != nullptr);
        fixture.control.getCurrentTextEditor()->setText("2 kHz", false);
        REQUIRE(fixture.host.getPeer() != nullptr);
        const auto peerID = fixture.host.getPeer()->getUniqueID();
        fixture.host.setSize(220, 100);
        fire::ui::setSkin(fixture.host, fire::ui::Skin::vintage);
        fixture.host.sendLookAndFeelChange();
        REQUIRE(fixture.host.getPeer()->getUniqueID() == peerID);
        static_cast<juce::Label&>(fixture.control).hideEditor(false);
        CHECK(fixture.control.getText() == "2 kHz");
    }
}
