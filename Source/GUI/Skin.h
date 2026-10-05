#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace fire::ui
{
enum class Skin { modern = 0, vintage = 1, paper = 2, ink = 3 };
inline constexpr std::array<Skin, 4> skins {Skin::modern, Skin::vintage, Skin::paper, Skin::ink};
inline constexpr std::array<const char*, 4> skinNames {"Modern", "Vintage", "Paper", "Ink"};
inline constexpr const char* skinProperty = "fireSkin";
inline constexpr const char* skinSetting = "appearanceSkin";
inline Skin skinFromValue(int value) noexcept
{ return value >= 0 && value < static_cast<int>(skins.size()) ? skins[static_cast<size_t>(value)] : Skin::modern; }
inline bool isLineSkin(Skin skin) noexcept { return skin == Skin::paper || skin == Skin::ink; }
inline bool isLightSkin(Skin skin) noexcept { return skin == Skin::paper; }
inline const char* skinName(Skin skin) noexcept { return skinNames[static_cast<size_t>(skinFromValue(static_cast<int>(skin)))]; }
inline Skin skinFor(const juce::Component& component)
{
    for (auto* owner = &component; owner != nullptr; owner = owner->getParentComponent())
        if (owner->getProperties().contains(skinProperty))
            return skinFromValue(static_cast<int>(owner->getProperties()[skinProperty]));
    return Skin::modern;
}
inline bool isVintage(const juce::Component& component) { return skinFor(component) == Skin::vintage; }
inline bool isLineSkin(const juce::Component& component) { return isLineSkin(skinFor(component)); }
inline bool usesNavigationMotion(const juce::Component& component) { return skinFor(component) == Skin::modern; }
inline void setSkin(juce::Component& component, Skin skin)
{ component.getProperties().set(skinProperty, static_cast<int>(skin)); component.repaint(); }

struct SkinPalette
{
    juce::Colour canvas, surface0, surface1, surface2, raised, hairline;
    juce::Colour textPrimary, textSecondary, textMuted, accent, textBright;
};
inline const SkinPalette& skinPalette(Skin skin)
{
    static const SkinPalette modern {
        juce::Colour(0xff080a0e), juce::Colour(0xff0d1118), juce::Colour(0xff121824),
        juce::Colour(0xff182131), juce::Colour(0xff202b3a), juce::Colour(0xff2a3545),
        juce::Colour(0xfff3f6fa), juce::Colour(0xffa8b2c1), juce::Colour(0xff8590a1), juce::Colour(0xffffc247), juce::Colour(0xfffff1d0)};
    static const SkinPalette vintage {
        juce::Colour(0xff35362f), juce::Colour(0xff3a3b33), juce::Colour(0xff43443a),
        juce::Colour(0xff505146), juce::Colour(0xff5b5b4e), juce::Colour(0xff777765),
        juce::Colour(0xffeee6cf), juce::Colour(0xffc3baa2), juce::Colour(0xffa89f89), juce::Colour(0xffdfb572), juce::Colour(0xffffedc8)};
    // The website's theme.css and Fire project.css are the source of truth.
    // Shared CSS variables intentionally give several semantic roles the
    // same colour; remapSkinColours retains their role across skin changes.
    static const SkinPalette paper {
        juce::Colour(0xfff3f2ee), juce::Colour(0xffe8e9e1), juce::Colour(0xfffcfbf8),
        juce::Colour(0xffe8e9e1), juce::Colour(0xffe8e9e1), juce::Colour(0xffd6d6cd),
        juce::Colour(0xff22231f), juce::Colour(0xff6b6c65), juce::Colour(0xff6b6c65), juce::Colour(0xff98442a), juce::Colour(0xff22231f)};
    static const SkinPalette ink {
        juce::Colour(0xff11161d), juce::Colour(0xff1a222d), juce::Colour(0xff1a222d),
        juce::Colour(0xff263242), juce::Colour(0xff263242), juce::Colour(0xff343e4c),
        juce::Colour(0xffeeece4), juce::Colour(0xffa7b0bd), juce::Colour(0xffa7b0bd), juce::Colour(0xffefac88), juce::Colour(0xffeeece4)};
    switch (skin)
    {
        case Skin::vintage: return vintage;
        case Skin::paper: return paper;
        case Skin::ink: return ink;
        case Skin::modern: return modern;
    }
    return modern;
}
inline const SkinPalette& paletteFor(const juce::Component& component) { return skinPalette(skinFor(component)); }
inline juce::Colour lineOnAccent(Skin skin) noexcept
{ return skin == Skin::paper ? juce::Colours::white : juce::Colour(0xff111923); }
inline juce::Colour lineBrandAccent(Skin skin) noexcept
{ return skin == Skin::paper ? juce::Colour(0xff2948dc) : juce::Colour(0xffa3b5ff); }
inline juce::Colour lineProjectTint(Skin skin) noexcept
{ return skin == Skin::paper ? juce::Colour(0xffeae1d6) : juce::Colour(0xff292320); }

// Constructor-assigned JUCE colours override LookAndFeel defaults. Translate
// only neutral tones, leaving module and modulation-source colours intact.
inline void remapSkinColours(juce::Component& component, Skin from, Skin to)
{
    if (from == to) return;
    const auto& a = skinPalette(from); const auto& b = skinPalette(to);
    const std::array<juce::Colour, 10> old {a.canvas,a.surface0,a.surface1,a.surface2,a.raised,a.hairline,a.textPrimary,a.textSecondary,a.textMuted,a.textBright};
    const std::array<juce::Colour, 10> next {b.canvas,b.surface0,b.surface1,b.surface2,b.raised,b.hairline,b.textPrimary,b.textSecondary,b.textMuted,b.textBright};
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
        const auto roleKey=juce::Identifier("fireThemeRole"+juce::String(id));
        auto role=static_cast<int>(component.getProperties().getWithDefault(roleKey,-1));
        if (role<0 || role>=static_cast<int>(old.size()) || colour.withAlpha(1.0f)!=old[static_cast<size_t>(role)])
        {
            role=-1;
            for (size_t index=0;index<old.size();++index)
                if (colour.withAlpha(1.0f)==old[index]) {role=static_cast<int>(index);break;}
        }
        if (role>=0)
        {
            component.getProperties().set(roleKey,role);
            component.setColour(id,next[static_cast<size_t>(role)].withAlpha(colour.getFloatAlpha()));
        }
    }
    for (auto* child : component.getChildren()) remapSkinColours(*child, from, to);
}

inline juce::Colour lineInk(juce::Colour colour, Skin skin) noexcept
{
    if (!isLineSkin(skin) || colour.isTransparent()) return colour;
    // Preserve source hues, but move light signals away from a paper surface.
    if (isLightSkin(skin) && colour.getPerceivedBrightness() > 0.46f)
        return colour.interpolatedWith(skinPalette(skin).textPrimary.withAlpha(colour.getFloatAlpha()), 0.55f);
    if (!isLightSkin(skin) && colour.getPerceivedBrightness() < 0.48f)
        return colour.interpolatedWith(skinPalette(skin).textPrimary.withAlpha(colour.getFloatAlpha()), 0.55f);
    return colour;
}
}
