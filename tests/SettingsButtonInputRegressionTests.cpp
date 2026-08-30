#include <GUI/InterfaceDefines.h>
#include <GUI/PrimaryButton.h>
#include <GUI/SettingsComponent.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

struct SettingsComponentTestAccess
{
    static PrimaryHyperlinkButton& companyButton(SettingsComponent& component)
    {
        return component.companyLabel;
    }

    static PrimaryToggleButton& autoUpdateButton(SettingsComponent& component)
    {
        return component.autoUpdateToggle;
    }

    static void disableCompanyLaunch(SettingsComponent& component)
    {
        component.companyLabel.setURL({});
    }

    static std::array<juce::Rectangle<int>, 4> controlBounds(
        const SettingsComponent& component)
    {
        return { component.versionLabel.getBounds(),
                 component.authorLabel.getBounds(),
                 component.companyLabel.getBounds(),
                 component.autoUpdateToggle.getBounds() };
    }

    static juce::Rectangle<int> glyphBounds(const SettingsComponent& component)
    {
        return component.fireGlyphArea;
    }
};

namespace
{
class TestPropertiesFile final : public juce::PropertiesFile
{
public:
    TestPropertiesFile()
        : juce::PropertiesFile(makeFile(), makeOptions())
    {
    }

    std::function<void()> onPropertyChanged;

protected:
    void propertyChanged() override
    {
        // Tests need the real virtual setValue() completion boundary without
        // scheduling disk writes or change messages into another test case.
        auto callback = onPropertyChanged;
        if (callback)
            callback();
    }

private:
    static juce::PropertiesFile::Options makeOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "FireSettingsButtonTests";
        options.filenameSuffix = ".settings";
        options.doNotSave = true;
        options.millisecondsBeforeSaving = -1;
        return options;
    }

    static juce::File makeFile()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("FireSettingsButtonTests-"
                          + juce::Uuid().toString()
                          + ".settings");
    }
};

class SettingsOwner final : public juce::Component
{
public:
    explicit SettingsOwner(juce::PropertiesFile& properties)
        : settings(std::make_unique<SettingsComponent>(properties))
    {
        addAndMakeVisible(*settings);
    }

    std::unique_ptr<SettingsComponent> settings;
};

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::ModifierKeys modifiers,
                                bool wasDragged = false)
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
             wasDragged };
}

void beginPointerGesture(juce::Button& button,
                         juce::ModifierKeys modifiers)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(component, modifiers));
}

void dragPointerGesture(juce::Button& button,
                        juce::ModifierKeys modifiers)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDrag(makeMouseEvent(component, modifiers, true));
}

void movePointer(juce::Button& button,
                 juce::ModifierKeys modifiers = {})
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseMove(makeMouseEvent(component, modifiers));
}

void endPointerGesture(juce::Button& button,
                       juce::ModifierKeys modifiers = {})
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseUp(makeMouseEvent(component, modifiers));
}

void performPointerGesture(juce::Button& button,
                           juce::ModifierKeys downModifiers,
                           juce::ModifierKeys upModifiers = {})
{
    beginPointerGesture(button, downModifiers);
    endPointerGesture(button, upModifiers);
}

std::vector<juce::ModifierKeys> rejectedPointerModifiers()
{
    std::vector<juce::ModifierKeys> result {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::rightButtonModifier }
    };

#if JUCE_MAC
    result.emplace_back(juce::ModifierKeys::leftButtonModifier
                        | juce::ModifierKeys::ctrlModifier);
#endif

    return result;
}

std::array<juce::Button*, 2> getSettingsButtons(SettingsComponent& settings)
{
    return { &SettingsComponentTestAccess::companyButton(settings),
             &SettingsComponentTestAccess::autoUpdateButton(settings) };
}

void prepareSettings(SettingsComponent& settings)
{
    settings.setBounds(0, 0, 400, 300);
    settings.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    settings.setVisible(true);
    SettingsComponentTestAccess::disableCompanyLaunch(settings);
}

std::uint64_t renderFingerprint(juce::Component& component)
{
    juce::Image image(juce::Image::ARGB,
                      component.getWidth(),
                      component.getHeight(),
                      true);
    juce::Graphics graphics(image);
    component.paintEntireComponent(graphics, true);

    std::uint64_t fingerprint = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            fingerprint ^= image.getPixelAt(x, y).getARGB();
            fingerprint *= 1099511628211ull;
        }

    return fingerprint;
}
} // namespace

TEST_CASE("Settings controls retain native roles behind primary-only input",
          "[ui][settings][settings-button][input][accessibility][primary-button]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestPropertiesFile properties;
    SettingsComponent settings(properties);
    prepareSettings(settings);

    auto& company = SettingsComponentTestAccess::companyButton(settings);
    auto& autoUpdate = SettingsComponentTestAccess::autoUpdateButton(settings);

    CHECK(dynamic_cast<juce::HyperlinkButton*>(&company) != nullptr);
    CHECK(dynamic_cast<PrimaryHyperlinkButton*>(&company) != nullptr);
    CHECK(dynamic_cast<juce::ToggleButton*>(&autoUpdate) != nullptr);
    CHECK(dynamic_cast<PrimaryToggleButton*>(&autoUpdate) != nullptr);

    auto* companyAccessibility = company.getAccessibilityHandler();
    auto* autoUpdateAccessibility = autoUpdate.getAccessibilityHandler();
    REQUIRE(companyAccessibility != nullptr);
    REQUIRE(autoUpdateAccessibility != nullptr);
    CHECK(companyAccessibility->getRole() == juce::AccessibilityRole::hyperlink);
    CHECK(autoUpdateAccessibility->getRole() == juce::AccessibilityRole::toggleButton);
}

TEST_CASE("Settings company link renders animated Fire keyboard feedback",
          "[ui][settings][settings-button][hyperlink][animation][focus]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestPropertiesFile properties;
    SettingsComponent settings(properties);
    prepareSettings(settings);

    auto& company = SettingsComponentTestAccess::companyButton(settings);
    juce::Component::unfocusAllComponents();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(350);
    const auto restingFingerprint = renderFingerprint(company);
    CHECK(company.getFocusAnimation() < 0.01f);

    company.grabKeyboardFocus();
    REQUIRE(company.hasKeyboardFocus(true));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(350);
    CHECK(company.getFocusAnimation() > 0.95f);
    CHECK(renderFingerprint(company) != restingFingerprint);

    // A pointer-acquired focus must remove the keyboard-only outline while
    // retaining the same native hyperlink control and URL behaviour.
    static_cast<juce::Component&>(company).focusGained(
        juce::Component::focusChangedByMouseClick);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(600);
    CHECK(company.getFocusAnimation() < 0.01f);
    CHECK(renderFingerprint(company) == restingFingerprint);

    // The first keyboard activation after pointer focus must restore the
    // keyboard-only outline through PrimaryPointerButton's synchronous path.
    REQUIRE(static_cast<juce::Component&>(company).keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(350);
    CHECK(company.getFocusAnimation() > 0.95f);
    CHECK(renderFingerprint(company) != restingFingerprint);
}

TEST_CASE("Settings layout remains usable at its minimum and narrow tall sizes",
          "[ui][settings][layout][resize][scale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestPropertiesFile properties;
    SettingsComponent settings(properties);

    const std::array<juce::Rectangle<int>, 3> sizes {
        juce::Rectangle<int>(0, 0,
                             SettingsComponent::minimumContentWidth,
                             SettingsComponent::minimumContentHeight),
        juce::Rectangle<int>(0, 0,
                             SettingsComponent::minimumContentWidth,
                             420),
        juce::Rectangle<int>(0, 0, 450, 280)
    };

    for (const auto bounds : sizes)
    {
        CAPTURE(bounds.toString());
        settings.setBounds(bounds);
        const auto localBounds = settings.getLocalBounds();
        const auto glyph = SettingsComponentTestAccess::glyphBounds(settings);
        const auto controls = SettingsComponentTestAccess::controlBounds(settings);

        REQUIRE_FALSE(glyph.isEmpty());
        CHECK(localBounds.contains(glyph));
        for (size_t index = 0; index < controls.size(); ++index)
        {
            CAPTURE(index, controls[index].toString());
            REQUIRE_FALSE(controls[index].isEmpty());
            CHECK(localBounds.contains(controls[index]));
            if (index > 0)
            {
                CHECK(controls[index - 1].getBottom()
                      <= controls[index].getY());
                CHECK_FALSE(controls[index - 1].intersects(controls[index]));
            }
        }

        CHECK(glyph.getBottom() <= controls.front().getY());

        // Exercise non-default rendering scale without changing logical hit
        // bounds or forcing a second layout policy.
        const auto scaledSnapshot = settings.createComponentSnapshot(
            localBounds, true, 1.5f);
        CHECK(scaledSnapshot.getWidth()
              == juce::roundToInt(static_cast<float>(bounds.getWidth()) * 1.5f));
        CHECK(scaledSnapshot.getHeight()
              == juce::roundToInt(static_cast<float>(bounds.getHeight()) * 1.5f));
    }
}

TEST_CASE("Settings controls reject popup and auxiliary pointer gestures",
          "[ui][settings][settings-button][input][primary-button]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestPropertiesFile properties;
    SettingsComponent settings(properties);
    prepareSettings(settings);

    for (auto* button : getSettingsButtons(settings))
    {
        CAPTURE(button->getButtonText());
        for (const auto modifiers : rejectedPointerModifiers())
        {
            CAPTURE(modifiers.getRawFlags());
            int clickCount = 0;
            button->setToggleState(false, juce::dontSendNotification);
            button->onClick = [&clickCount] { ++clickCount; };

            beginPointerGesture(*button, modifiers);
            dragPointerGesture(*button, modifiers);
            endPointerGesture(*button);

            CHECK_FALSE(button->isDown());
            CHECK_FALSE(button->getToggleState());
            CHECK(clickCount == 0);
            button->onClick = nullptr;
        }
    }
}

TEST_CASE("Settings controls discard stale gestures at dialog boundaries",
          "[ui][settings][settings-button][input][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("hide and show")
    {
        TestPropertiesFile properties;
        SettingsComponent settings(properties);
        prepareSettings(settings);

        for (auto* button : getSettingsButtons(settings))
        {
            CAPTURE(button->getButtonText());
            int clickCount = 0;
            button->onClick = [&clickCount] { ++clickCount; };
            beginPointerGesture(*button, leftButton);
            REQUIRE(button->isDown());

            settings.setVisible(false);
            CHECK_FALSE(button->isDown());
            settings.setVisible(true);
            endPointerGesture(*button);

            CHECK(clickCount == 0);
            button->onClick = nullptr;
        }
    }

    SECTION("disable and enable")
    {
        TestPropertiesFile properties;
        SettingsComponent settings(properties);
        prepareSettings(settings);

        for (auto* button : getSettingsButtons(settings))
        {
            CAPTURE(button->getButtonText());
            int clickCount = 0;
            button->onClick = [&clickCount] { ++clickCount; };
            beginPointerGesture(*button, leftButton);
            REQUIRE(button->isDown());

            settings.setEnabled(false);
            CHECK_FALSE(button->isDown());
            settings.setEnabled(true);
            endPointerGesture(*button);

            CHECK(clickCount == 0);
            button->onClick = nullptr;
        }
    }

    SECTION("lost release")
    {
        TestPropertiesFile properties;
        SettingsComponent settings(properties);
        prepareSettings(settings);

        for (auto* button : getSettingsButtons(settings))
        {
            CAPTURE(button->getButtonText());
            int clickCount = 0;
            button->onClick = [&clickCount] { ++clickCount; };
            beginPointerGesture(*button, leftButton);
            REQUIRE(button->isDown());

            movePointer(*button);
            CHECK_FALSE(button->isDown());
            endPointerGesture(*button);
            CHECK(clickCount == 0);

            performPointerGesture(*button, leftButton);
            CHECK(clickCount == 1);
            button->onClick = nullptr;
        }
    }
}

TEST_CASE("Settings commands complete synchronously within one dialog session",
          "[ui][settings][settings-button][input][keyboard][accessibility][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestPropertiesFile properties;
    SettingsComponent settings(properties);
    prepareSettings(settings);

    for (auto* button : getSettingsButtons(settings))
    {
        CAPTURE(button->getButtonText());
        int clickCount = 0;
        button->onClick = [&clickCount] { ++clickCount; };

        button->triggerClick();
        CHECK(clickCount == 1);

        REQUIRE(static_cast<juce::Component&>(*button).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK(clickCount == 2);

        REQUIRE(static_cast<juce::Component&>(*button).keyPressed(
            juce::KeyPress { juce::KeyPress::spaceKey }));
        CHECK(clickCount == 3);

        auto* accessibility = button->getAccessibilityHandler();
        REQUIRE(accessibility != nullptr);
        REQUIRE(accessibility->getActions().invoke(
            juce::AccessibilityActionType::press));
        CHECK(clickCount == 4);

        // No posted trigger from the previous visible dialog state may arrive
        // after it is shown again.
        settings.setVisible(false);
        settings.setVisible(true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        CHECK(clickCount == 4);
        button->onClick = nullptr;
    }
}

TEST_CASE("Hidden and disabled settings reject stale direct commands",
          "[ui][settings][settings-button][input][keyboard][accessibility][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto exerciseBoundary = [] (bool hideSettings)
    {
        TestPropertiesFile properties;
        SettingsComponent settings(properties);
        prepareSettings(settings);

        for (auto* button : getSettingsButtons(settings))
        {
            CAPTURE(button->getButtonText(), hideSettings);
            int clickCount = 0;
            button->onClick = [&clickCount] { ++clickCount; };
            auto* accessibility = button->getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);

            if (hideSettings)
                settings.setVisible(false);
            else
                settings.setEnabled(false);

            button->triggerClick();
            CHECK_FALSE(static_cast<juce::Component&>(*button).keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey }));
            CHECK_FALSE(static_cast<juce::Component&>(*button).keyPressed(
                juce::KeyPress { juce::KeyPress::spaceKey }));
            REQUIRE(accessibility->getActions().invoke(
                juce::AccessibilityActionType::press));
            CHECK(clickCount == 0);

            if (hideSettings)
                settings.setVisible(true);
            else
                settings.setEnabled(true);

            button->triggerClick();
            REQUIRE(static_cast<juce::Component&>(*button).keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey }));
            REQUIRE(static_cast<juce::Component&>(*button).keyPressed(
                juce::KeyPress { juce::KeyPress::spaceKey }));
            REQUIRE(accessibility->getActions().invoke(
                juce::AccessibilityActionType::press));
            CHECK(clickCount == 4);
            button->onClick = nullptr;
        }
    };

    SECTION("hidden dialog")
    {
        exerciseBoundary(true);
    }

    SECTION("disabled dialog")
    {
        exerciseBoundary(false);
    }
}

TEST_CASE("Settings button completion may synchronously delete its owning UI",
          "[ui][settings][settings-button][input][keyboard][accessibility][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("pointer callback")
    {
        TestPropertiesFile properties;
        auto settings = std::make_unique<SettingsComponent>(properties);
        prepareSettings(*settings);
        auto* button = &SettingsComponentTestAccess::companyButton(*settings);
        button->onClick = [&settings] { settings.reset(); };

        performPointerGesture(*button, leftButton);
        CHECK(settings == nullptr);
    }

    SECTION("Return callback")
    {
        TestPropertiesFile properties;
        auto settings = std::make_unique<SettingsComponent>(properties);
        prepareSettings(*settings);
        auto* button = &SettingsComponentTestAccess::companyButton(*settings);
        button->onClick = [&settings] { settings.reset(); };

        CHECK(static_cast<juce::Component&>(*button).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK(settings == nullptr);
    }

    SECTION("Space callback")
    {
        TestPropertiesFile properties;
        auto settings = std::make_unique<SettingsComponent>(properties);
        prepareSettings(*settings);
        auto* button = &SettingsComponentTestAccess::companyButton(*settings);
        button->onClick = [&settings] { settings.reset(); };

        CHECK(static_cast<juce::Component&>(*button).keyPressed(
            juce::KeyPress { juce::KeyPress::spaceKey }));
        CHECK(settings == nullptr);
    }

    SECTION("synchronous accessibility command callback")
    {
        TestPropertiesFile properties;
        auto settings = std::make_unique<SettingsComponent>(properties);
        prepareSettings(*settings);
        auto* button = &SettingsComponentTestAccess::companyButton(*settings);
        button->onClick = [&settings] { settings.reset(); };

        button->triggerClick();
        CHECK(settings == nullptr);
    }

    SECTION("real preference callback deletes the settings owner")
    {
        TestPropertiesFile properties;
        auto owner = std::make_unique<SettingsOwner>(properties);
        owner->setBounds(0, 0, 400, 300);
        owner->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        owner->setVisible(true);
        owner->settings->setBounds(owner->getLocalBounds());
        owner->settings->setVisible(true);
        SettingsComponentTestAccess::disableCompanyLaunch(*owner->settings);
        auto* button = &SettingsComponentTestAccess::autoUpdateButton(
            *owner->settings);
        properties.onPropertyChanged = [&owner] { owner.reset(); };

        performPointerGesture(*button, leftButton);
        CHECK(owner == nullptr);
    }
}

TEST_CASE("Dialog gesture teardown tolerates synchronous owner deletion",
          "[ui][settings][settings-button][input][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    for (int buttonIndex = 0; buttonIndex < 2; ++buttonIndex)
    {
        CAPTURE(buttonIndex);
        TestPropertiesFile properties;
        auto settings = std::make_unique<SettingsComponent>(properties);
        prepareSettings(*settings);
        auto buttons = getSettingsButtons(*settings);
        auto* button = buttons[static_cast<size_t>(buttonIndex)];
        beginPointerGesture(*button, leftButton);
        REQUIRE(button->isDown());
        button->onStateChange = [&settings] { settings.reset(); };

        settings->setVisible(false);
        CHECK(settings == nullptr);
    }
}
