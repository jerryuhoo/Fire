#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#if !defined(_MSC_VER)
    #include <cxxabi.h>
namespace melatonin
{
    // https://stackoverflow.com/a/4541470
    static inline std::string demangle (const char* name)
    {
        int status = -4; // some arbitrary value to eliminate the compiler warning

        std::unique_ptr<char, void (*) (void*)> res {
            abi::__cxa_demangle (name, nullptr, nullptr, &status),
            std::free
        };

        return (status == 0) ? res.get() : name;
    }

    template <class T>
    static inline juce::String type (const T& t)
    {
        return demangle (typeid (t).name());
    }
}
#else
namespace melatonin
{
    template <class T>
    static inline juce::String type (const T& t)
    {
        return juce::String (typeid (t).name()).replace ("class ", "").replace ("struct ", "");
    }
}
#endif
namespace melatonin
{
    // do our best to derive a useful UI string from a component
    static inline juce::String componentString (juce::Component* c)
    {
        if (auto* label = dynamic_cast<juce::Label*> (c))
        {
            return juce::String ("Label: ") + label->getText().substring (0, 20);
        }
#if JUCE_MODULE_AVAILABLE_juce_audio_processors
        else if (auto editor = dynamic_cast<juce::AudioProcessorEditor*> (c))
        {
            return juce::String ("Editor: ") + editor->getAudioProcessor()->getName();
        }
#endif
        else if (c && c->isAccessible() && c->getAccessibilityHandler() && !c->getAccessibilityHandler()->getTitle().isEmpty())
        {
            auto acctitle = c->getAccessibilityHandler()->getTitle();
            return acctitle;
        }
        else if (c && !c->getName().isEmpty())
        {
            return c->getName();
        }
        else if (c)
        {
            return type (*c);
        }
        else
        {
            return "";
        }
    }

    // do our best to derive a useful UI string from a component
    static inline juce::String componentFontValue (juce::Component* c)
    {
        if (auto* label = dynamic_cast<juce::Label*> (c))
        {
            auto font = label->getFont();
            return juce::String (font.getTypefaceName() + " " + font.getTypefaceStyle() + " " + juce::String (font.getHeight()));
        }
        else if (auto btn = dynamic_cast<juce::TextButton*> (c))
        {
            auto font = btn->getLookAndFeel().getTextButtonFont (*btn, btn->getHeight());
            return juce::String (font.getTypefaceName() + " " + font.getTypefaceStyle() + " " + juce::String (font.getHeight()));
        }
        else if (auto slider = dynamic_cast<juce::Slider*> (c))
        {
            auto font = slider->getLookAndFeel().getSliderPopupFont (*slider);
            return juce::String (font.getTypefaceName() + " " + font.getTypefaceStyle() + " " + juce::String (font.getHeight()));
        }
        else if (auto cb = dynamic_cast<juce::ComboBox*> (c))
        {
            auto font = cb->getLookAndFeel().getComboBoxFont (*cb);
            return juce::String (font.getTypefaceName() + " " + font.getTypefaceStyle() + " " + juce::String (font.getHeight()));
        }
        else
        {
            return "-";
        }
    }

    // returns name of assigned LnF
    static inline juce::String lnfString (juce::Component* c)
    {
        if (c)
        {
            auto& lnf = c->getLookAndFeel();
            return type (lnf);
        }
        else
        {
            return "-";
        }
    }
}
