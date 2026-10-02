#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace fire::ui
{
enum class Skin { modern = 0, vintage = 1 };
inline constexpr const char* skinProperty = "fireSkin";
inline constexpr const char* skinSetting = "appearanceSkin";
inline Skin skinFromValue(int value) noexcept { return value == 1 ? Skin::vintage : Skin::modern; }
inline Skin skinFor(const juce::Component& component)
{
    for (auto* owner = &component; owner != nullptr; owner = owner->getParentComponent())
        if (owner->getProperties().contains(skinProperty))
            return skinFromValue(static_cast<int>(owner->getProperties()[skinProperty]));
    return Skin::modern;
}
inline bool isVintage(const juce::Component& component) { return skinFor(component) == Skin::vintage; }
inline void setSkin(juce::Component& component, Skin skin)
{ component.getProperties().set(skinProperty, static_cast<int>(skin)); component.repaint(); }

struct SkinPalette
{
    juce::Colour canvas, surface0, surface1, surface2, raised, hairline;
    juce::Colour textPrimary, textSecondary, textMuted, accent;
};
inline const SkinPalette& skinPalette(Skin skin)
{
    static const SkinPalette modern {
        juce::Colour(0xff080a0e), juce::Colour(0xff0d1118), juce::Colour(0xff121824),
        juce::Colour(0xff182131), juce::Colour(0xff202b3a), juce::Colour(0xff2a3545),
        juce::Colour(0xfff3f6fa), juce::Colour(0xffa8b2c1), juce::Colour(0xff8590a1), juce::Colour(0xffffc247)};
    static const SkinPalette vintage {
        juce::Colour(0xff35362f), juce::Colour(0xff3a3b33), juce::Colour(0xff43443a),
        juce::Colour(0xff505146), juce::Colour(0xff5b5b4e), juce::Colour(0xff777765),
        juce::Colour(0xffeee6cf), juce::Colour(0xffc3baa2), juce::Colour(0xffa89f89), juce::Colour(0xffdfb572)};
    return skin == Skin::vintage ? vintage : modern;
}
inline const SkinPalette& paletteFor(const juce::Component& component) { return skinPalette(skinFor(component)); }

// Constructor-assigned JUCE colours override LookAndFeel defaults. Translate
// only neutral tones, leaving module and modulation-source colours intact.
inline void remapSkinColours(juce::Component& component, Skin from, Skin to)
{
    if (from == to) return;
    const auto& a = skinPalette(from); const auto& b = skinPalette(to);
    const std::array<juce::Colour, 9> old {a.canvas,a.surface0,a.surface1,a.surface2,a.raised,a.hairline,a.textPrimary,a.textSecondary,a.textMuted};
    const std::array<juce::Colour, 9> next {b.canvas,b.surface0,b.surface1,b.surface2,b.raised,b.hairline,b.textPrimary,b.textSecondary,b.textMuted};
    for (const int id : std::initializer_list<int>{juce::TextButton::buttonColourId,juce::TextButton::buttonOnColourId,
            juce::TextButton::textColourOffId,juce::TextButton::textColourOnId,
            juce::Label::textColourId,juce::Label::backgroundColourId,juce::Label::outlineColourId,
            juce::Label::textWhenEditingColourId,juce::Label::backgroundWhenEditingColourId,
            juce::ComboBox::backgroundColourId,juce::ComboBox::outlineColourId,juce::ComboBox::textColourId,juce::ComboBox::arrowColourId,
            juce::TextEditor::backgroundColourId,juce::TextEditor::outlineColourId,juce::TextEditor::textColourId,
            juce::TextEditor::highlightColourId,juce::TextEditor::focusedOutlineColourId,juce::ScrollBar::thumbColourId,
            juce::ToggleButton::textColourId,juce::ToggleButton::tickDisabledColourId,
            juce::Slider::backgroundColourId,juce::Slider::textBoxTextColourId,
            juce::Slider::textBoxBackgroundColourId,juce::Slider::textBoxOutlineColourId})
    {
        if (!component.isColourSpecified(id)) continue;
        const auto colour = component.findColour(id);
        for (size_t index = 0; index < old.size(); ++index)
            if (colour.withAlpha(1.0f) == old[index])
            {component.setColour(id, next[index].withAlpha(colour.getFloatAlpha())); break;}
    }
    for (auto* child : component.getChildren()) remapSkinColours(*child, from, to);
}
}
