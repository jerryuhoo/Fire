#include <GUI/InterfaceDefines.h>
#include <GUI/ValueEntryPopup.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

struct ValueEntryPopupTestAccess
{
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
};

TEST_CASE("Value entry popup rejects incomplete and non-finite numbers",
          "[ui][modulation][value-entry][validation]")
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
}
