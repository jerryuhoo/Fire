#include <GUI/LookAndFeel.h>
#include <GUI/ModulatableSlider.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Modulatable slider titles preserve the advertised header hit target",
          "[modulatable-slider][ui][hit-test]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    ModulatableSlider slider;
    slider.setLookAndFeel(&lookAndFeel);
    slider.setBounds(0, 0, 120, 120);
    slider.setVisible(true);

    juce::Label* title = nullptr;
    for (auto* child : slider.getChildren())
        if (auto* candidate = dynamic_cast<juce::Label*>(child))
        {
            title = candidate;
            break;
        }

    REQUIRE(title != nullptr);
    REQUIRE_FALSE(title->getBounds().isEmpty());
    const auto titleCentre = title->getBounds().getCentre();
    REQUIRE(slider.hitTest(titleCentre.x, titleCentre.y));

    // Event dispatch must fall through the presentation label to the Slider.
    CHECK_FALSE(title->hitTest(title->getWidth() / 2, title->getHeight() / 2));
    CHECK(slider.getComponentAt(titleCentre) == &slider);

    slider.setLookAndFeel(nullptr);
}
