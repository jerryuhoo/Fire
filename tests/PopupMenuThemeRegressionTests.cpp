#include <GUI/FireTheme.h>
#include <GUI/LookAndFeel.h>
#include <GUI/ModulatableSlider.h>
#include <Panels/ControlPanel/LfoPanel.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>

namespace
{
juce::MouseEvent makePopupMouseEvent(juce::Component& component,
                                     juce::Point<float> position)
{
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
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

juce::Component* findPopupMenu(juce::Component& root)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (child != nullptr && child->getName() == "menu")
            return child;

        if (child != nullptr)
            if (auto* nested = findPopupMenu(*child))
                return nested;
    }

    return nullptr;
}

void dismissMenus()
{
    juce::PopupMenu::dismissAllActiveMenus();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
}

std::uint64_t imageFingerprint(const juce::Image& image)
{
    std::uint64_t fingerprint = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            fingerprint ^= image.getPixelAt(x, y).getARGB();
            fingerprint *= 1099511628211ull;
        }

    return fingerprint;
}

juce::Image renderIdleComboBox(FireLookAndFeel& lookAndFeel,
                               juce::Colour background,
                               const juce::String& componentID = {})
{
    constexpr int width = 180;
    constexpr int height = 32;
    juce::ComboBox comboBox;
    comboBox.setBounds(0, 0, width, height);
    comboBox.setComponentID(componentID);
    comboBox.setColour(juce::ComboBox::backgroundColourId, background);
    comboBox.setLookAndFeel(&lookAndFeel);

    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics graphics(image);
    lookAndFeel.drawComboBox(graphics,
                             width,
                             height,
                             false,
                             0,
                             0,
                             0,
                             0,
                             comboBox);
    comboBox.setLookAndFeel(nullptr);
    return image;
}

int colourChannelError(juce::Colour actual, juce::Colour expected)
{
    return std::abs(static_cast<int>(actual.getRed())
                    - static_cast<int>(expected.getRed()))
         + std::abs(static_cast<int>(actual.getGreen())
                    - static_cast<int>(expected.getGreen()))
         + std::abs(static_cast<int>(actual.getBlue())
                    - static_cast<int>(expected.getBlue()))
         + std::abs(static_cast<int>(actual.getAlpha())
                    - static_cast<int>(expected.getAlpha()));
}
} // namespace

TEST_CASE("Fire ComboBoxes honour their configured idle background colour",
          "[ui][combo-box][theme][background][pixels]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    constexpr int sampleX = 54;
    constexpr int sampleY = 16;

    const auto firstBackground = fire::ui::colours::surface0;
    const auto secondBackground = juce::Colour { 0xff26384c };
    const auto firstImage = renderIdleComboBox(lookAndFeel,
                                                firstBackground);
    const auto secondImage = renderIdleComboBox(lookAndFeel,
                                                 secondBackground);
    const auto firstInterior = firstImage.getPixelAt(sampleX, sampleY);
    const auto secondInterior = secondImage.getPixelAt(sampleX, sampleY);

    CHECK(firstInterior != secondInterior);
    CHECK(imageFingerprint(firstImage) != imageFingerprint(secondImage));
    CHECK(colourChannelError(
              firstInterior,
              firstBackground)
          <= 4);
    CHECK(colourChannelError(
              secondInterior,
              secondBackground)
          <= 4);

    // The preset selector follows the same flat, configurable surface.
    const auto firstHeader = renderIdleComboBox(lookAndFeel,
                                                 firstBackground,
                                                 "header_preset");
    const auto secondHeader = renderIdleComboBox(lookAndFeel,
                                                  secondBackground,
                                                  "header_preset");
    CHECK(imageFingerprint(firstHeader) != imageFingerprint(secondHeader));
    CHECK(colourChannelError(firstHeader.getPixelAt(sampleX, sampleY), firstBackground) <= 4);
    CHECK(colourChannelError(secondHeader.getPixelAt(sampleX, sampleY), secondBackground) <= 4);
}

TEST_CASE("Fire ComboBox animation cache discards expired idle controls",
          "[ui][combo-box][theme][animation][lifecycle][cache][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;

    for (int session = 0; session < 128; ++session)
    {
        auto image = renderIdleComboBox(
            lookAndFeel,
            session % 2 == 0 ? fire::ui::colours::surface0
                             : fire::ui::colours::surface1);
        CHECK(image.isValid());
        CHECK(lookAndFeel.getTrackedComboBoxAnimationCountForTesting() == 1);
    }

    // One final lookup prunes the expired entry from the preceding session
    // before inserting its own state.
    auto finalImage = renderIdleComboBox(lookAndFeel,
                                         fire::ui::colours::surface0);
    CHECK(finalImage.isValid());
    CHECK(lookAndFeel.getTrackedComboBoxAnimationCountForTesting() == 1);
}

TEST_CASE("Fire context menus inherit their target theme and cursor anchor",
          "[ui][popup-menu][theme][anchor]")
{
    FireLookAndFeel lookAndFeel;
    lookAndFeel.scale = 1.5f;

    juce::Component root;
    root.setBounds(0, 0, 1000, 500);
    root.setLookAndFeel(&lookAndFeel);
    root.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    root.setVisible(true);

    SECTION("shared options retain the target, editor parent, and cursor")
    {
        juce::Component target;
        root.addAndMakeVisible(target);
        target.setBounds(100, 80, 120, 90);
        juce::PopupMenu menu;
        const auto cursor = root.localPointToGlobal(
            juce::Point<int> { 157, 113 });
        const auto options = fire::ui::prepareContextMenu(menu, target, cursor);

        CHECK(options.getTargetComponent() == &target);
        CHECK(options.getTopLevelTargetComponent() == &target);
        CHECK(options.getParentComponent() == &root);
        CHECK(options.getTargetScreenArea()
              == juce::Rectangle<int>(cursor.x, cursor.y, 1, 1));
        CHECK_FALSE(options.hasWatchedComponentBeenDeleted());
        CHECK_FALSE(lookAndFeel.findColour(
            juce::PopupMenu::backgroundColourId).isOpaque());
        CHECK(lookAndFeel.getPopupMenuBorderSize() == 0);
    }

    SECTION("Fire owns the complete rounded menu surface")
    {
        juce::Image background(juce::Image::ARGB, 180, 84, true);
        juce::Graphics backgroundGraphics(background);
        lookAndFeel.drawPopupMenuBackground(
            backgroundGraphics, background.getWidth(), background.getHeight());

        CHECK(background.getPixelAt(0, 0).getAlpha() == 0);
        CHECK(background.getPixelAt(background.getWidth() / 2,
                                    background.getHeight() / 2).getAlpha() > 0);

        juce::Image idleItem(juce::Image::ARGB, 180, 34, true);
        juce::Graphics idleGraphics(idleItem);
        lookAndFeel.drawPopupMenuItem(idleGraphics,
                                      idleItem.getBounds(),
                                      false,
                                      true,
                                      false,
                                      false,
                                      false,
                                      "Assign modulation",
                                      {},
                                      nullptr,
                                      nullptr);

        juce::Image highlightedItem(juce::Image::ARGB, 180, 34, true);
        juce::Graphics highlightedGraphics(highlightedItem);
        lookAndFeel.drawPopupMenuItem(highlightedGraphics,
                                      highlightedItem.getBounds(),
                                      false,
                                      true,
                                      true,
                                      false,
                                      false,
                                      "Assign modulation",
                                      {},
                                      nullptr,
                                      nullptr);

        CHECK(imageFingerprint(highlightedItem)
              != imageFingerprint(idleItem));
        CHECK(highlightedItem.getPixelAt(8, highlightedItem.getHeight() / 2)
                  .getAlpha() > 0);
    }

    SECTION("parented menus release their explicit theme after construction")
    {
        juce::Component simulatedMenuWindow;
        root.addAndMakeVisible(simulatedMenuWindow);
        simulatedMenuWindow.setLookAndFeel(&lookAndFeel);
        REQUIRE(&simulatedMenuWindow.getLookAndFeel() == &lookAndFeel);

        lookAndFeel.preparePopupMenuWindow(simulatedMenuWindow);
        root.setLookAndFeel(nullptr);
        CHECK(&simulatedMenuWindow.getLookAndFeel()
              == &juce::LookAndFeel::getDefaultLookAndFeel());

        root.setLookAndFeel(&lookAndFeel);
        CHECK(&simulatedMenuWindow.getLookAndFeel() == &lookAndFeel);
    }

    SECTION("slider assignment menu is an editor-owned themed child")
    {
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()
            == nullptr)
        {
            SUCCEED("Headless runner has no display for a real PopupMenu window");
            return;
        }

        ModulatableSlider slider;
        slider.parameterID = "drive0";
        root.addAndMakeVisible(slider);
        slider.setBounds(120, 70, 100, 120);

        const auto menuPosition = slider.getLocalBounds().toFloat().getCentre();
        slider.mouseDown(makePopupMouseEvent(slider, menuPosition));
        slider.mouseUp(makePopupMouseEvent(slider, menuPosition));
        const juce::ScopeGuard cleanup { [] { dismissMenus(); } };

        auto* popup = findPopupMenu(root);
        REQUIRE(popup != nullptr);
        CHECK(popup->getParentComponent() == &root);
        CHECK(&popup->getLookAndFeel() == &lookAndFeel);
        CHECK(popup->isOpaque()
              == ! juce::Desktop::canUseSemiTransparentWindows());
    }

    SECTION("LFO edit menu uses the same editor-owned theme")
    {
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()
            == nullptr)
        {
            SUCCEED("Headless runner has no display for a real PopupMenu window");
            return;
        }

        LfoEditor editor;
        root.addAndMakeVisible(editor);
        editor.setBounds(260, 70, 500, 300);
        editor.setDataToDisplay(LfoData {});

        const auto menuPosition =
            editor.getLocalBounds().toFloat().getCentre();
        editor.mouseDown(makePopupMouseEvent(editor, menuPosition));
        editor.mouseUp(makePopupMouseEvent(editor, menuPosition));
        const juce::ScopeGuard cleanup { [] { dismissMenus(); } };

        auto* popup = findPopupMenu(root);
        REQUIRE(popup != nullptr);
        CHECK(popup->getParentComponent() == &root);
        CHECK(&popup->getLookAndFeel() == &lookAndFeel);
        CHECK(popup->isOpaque()
              == ! juce::Desktop::canUseSemiTransparentWindows());
    }

    dismissMenus();
    root.setLookAndFeel(nullptr);
}
