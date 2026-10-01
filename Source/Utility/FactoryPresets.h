#pragma once
#include "../PluginProcessor.h"
#include <array>
#include <vector>

namespace fire::factory
{
struct Definition { const char* key; const char* category; const char* name; const char* description; };
inline constexpr std::array<Definition, 12> definitions{{
    {"drum-bus-punch", "Drums", "Parallel Punch", "Parallel drum-bus colour. The low band stays restrained while the upper band adds attack and compression."},
    {"snare-edge", "Drums", "Crispy Edge", "Add bite to snares and percussion. Reduce Band Mix for a subtler edge."},
    {"tape-dust", "Drums", "Tape Dust", "Tape and low-bit texture for drum loops. Master Mix blends the original transient back in."},
    {"solid-sub", "Bass", "Solid Sub", "Keep the sub centred and add harmonics above 150 Hz. Adjust upper-band Drive to fit the arrangement."},
    {"moving-bass", "Bass", "Moving Texture", "Upper-band flanging adds movement while the mono low band anchors the bass."},
    {"warm-vocal", "Vocals", "Warm Presence", "Gentle saturation and compression with a small presence lift. Start with a recorded vocal at a normal level."},
    {"stereo-halo", "Vocals", "Stereo Halo", "Quiet chorus and filtered quarter-note echoes behind a dry vocal. Delay remains tempo synced."},
    {"clouds-bloom", "Atmosphere", "Clouds Bloom", "Feed a sustained chord or percussion into Clouds. Freeze after a phrase to hold a texture; the captured material saves with the project."},
    {"minor-resonance", "Atmosphere", "Minor Resonance", "C3 Minor 7 resonance with a long decay. Use a harmonically rich source, or change Root to match your song."},
    {"stereo-pulse", "Rhythm", "Stereo Pulse", "Quarter-note pan motion from LFO 1. The modulation matrix exposes the amount and polarity."},
    {"stepped-drive", "Rhythm", "Stepped Drive", "Tempo-synced LFO steps move the upper-band Drive. Keep the sub stable and change LFO 1's curve for a new pattern."},
    {"broken-radio", "Creative", "Broken Radio", "Band-limited, low-bit tape character. Master Mix restores clarity when the effect is too strong."}
}};

inline std::vector<std::unique_ptr<juce::XmlElement>> create(FireAudioProcessor& processor)
{
    // Build an independent complete default model; generating factory presets
    // never changes the live sound, user files, recorded material or A/B state.
    juce::XmlElement defaults("WINGSFIRE");
    ::state::saveStateToXml(processor, defaults);
    defaults.deleteAllChildElements();
    for (auto* parameter : processor.getParameters())
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
            defaults.setAttribute(identified->paramID, parameter->getDefaultValue());
    auto* shapes = defaults.createNewChildElement("LFO_STATE");
    for (int i = 0; i < fire::lfo_bank::capacity; ++i)
    {
        auto* lfo = shapes->createNewChildElement("LFO"); lfo->setAttribute("index", i);
        LfoData{}.writeToXml(*lfo);
    }
    defaults.createNewChildElement("MODULATION_STATE");
    std::vector<std::unique_ptr<juce::XmlElement>> results;
    for (size_t index = 0; index < definitions.size(); ++index)
    {
        auto preset = std::make_unique<juce::XmlElement>(defaults);
        const auto& definition = definitions[index];
        preset->setTagName("factory-" + juce::String(definition.key));
        preset->setAttribute("presetName", definition.name);
        preset->setAttribute("presetKey", "@factory/" + juce::String(definition.key));
        preset->setAttribute("factoryPreset", true);
        preset->setAttribute("presetDescription", definition.description);
        preset->setAttribute("presetCategory", definition.category);
        const auto set = [&](const juce::String& id, float value)
        {
            if (auto* parameter = processor.treeState.getParameter(id))
                preset->setAttribute(id, parameter->convertTo0to1(parameter->getNormalisableRange().snapToLegalValue(value)));
        };
        const auto band = [&](const char* base, int number, float value)
        { set(ParameterIDAndName::getIDString(base, number), value); };
        const auto split = [&](float frequency)
        { set(NUM_BANDS_ID, 2); set("lineState1", 1); set("freq1", frequency); };
        const auto insert = [&](int scope, int slot, fire::effects::Type type, const std::array<float, 6>& controls)
        {
            using namespace fire::effects;
            set(parameterID(scope, slot, typeField), static_cast<float>(type) <= 5 ? static_cast<float>(type) : 0);
            set(fire::modulation_fx::parameterID(scope, slot), type == Type::flanger ? 1 : type == Type::phaser ? 2 : 0);
            set(fire::resonator_params::parameterID(scope, slot), type == Type::chordResonator ? 1 : 0);
            set(parameterID(scope, slot, orderField), static_cast<float>(slot + 1));
            for (int control = 0; control < 6; ++control)
                set(parameterID(scope, slot, control), fire::effects::controls(type)[static_cast<size_t>(control)].toNormalised(controls[static_cast<size_t>(control)]));
        };
        const auto compressor = [&](int number, float threshold, float ratio, float attack, float release, float mix)
        {
            band(COMP_BYPASS_ID, number, 1); band(COMP_THRESH_ID, number, threshold);
            band(COMP_RATIO_ID, number, ratio); band(COMP_ATTACK_ID, number, attack);
            band(COMP_RELEASE_ID, number, release); band(COMP_MIX_ID, number, mix);
        };
        const auto motion = [&](const juce::String& target, float depth, bool steps)
        {
            auto* lfo = preset->getChildByName("LFO_STATE")->getFirstChildElement();
            lfo->deleteAllChildElements();
            LfoData shape;
            shape.points = steps ? std::vector<juce::Point<float>>{{0, 0.1f}, {0.24f, 0.1f}, {0.25f, 0.9f}, {0.49f, 0.9f}, {0.5f, 0.4f}, {0.74f, 0.4f}, {0.75f, 0.7f}, {0.99f, 0.7f}, {1, 0.1f}}
                                 : std::vector<juce::Point<float>>{{0, 0}, {0.5f, 1}, {1, 0}};
            shape.curvatures.resize(shape.points.size() - 1, 0); shape.writeToXml(*lfo);
            set("lfoSyncMode1", 1); set("lfoRateSync1", steps ? 11 : 8); set("lfoSmooth1", steps ? 0.06f : 0.1f);
            ModulationRouting routing; routing.targetParameterID = target; routing.depth = depth;
            routing.writeToXml(*preset->getChildByName("MODULATION_STATE")->createNewChildElement("ROUTING"));
        };
        set(OUTPUT_ID, -1);
        using Type = fire::effects::Type;
        switch (index)
        {
            case 0:
                split(180); band(DRIVE_ID, 0, 12); band(DRIVE_ID, 1, 30);
                compressor(1, -18, 3, 15, 100, 0.6f); set(MIX_ID, 0.65f); break;
            case 1:
                band(DRIVE_ID, 0, 32); band(BIAS_ID, 0, 0.025f); band(SHAPE_BYPASS_ID, 0, 1);
                band(SHAPE_MIX_ID, 0, 0.8f); set(MIX_ID, 0.7f); break;
            case 2:
                band(DRIVE_ID, 0, 12); set(DOWNSAMPLE_BYPASS_ID, 1); set(DOWNSAMPLE_ID, 4);
                set(BIT_DEPTH_ID, 12); set("lofiTape", 0.35f); set("lofiWow", 0.08f);
                set("lofiFlutter", 0.03f); set(MIX_ID, 0.65f); break;
            case 3:
                split(150); band(DRIVE_ID, 0, 6); band(DRIVE_ID, 1, 42);
                band(WIDTH_BYPASS_ID, 0, 1); band(WIDTH_ID, 0, 0); break;
            case 4:
                split(180); band(DRIVE_ID, 0, 8); band(DRIVE_ID, 1, 32);
                band(WIDTH_BYPASS_ID, 0, 1); band(WIDTH_ID, 0, 0);
                insert(2, 0, Type::flanger, {0.2f, 55, 2, 30, 75, 25}); break;
            case 5:
                band(DRIVE_ID, 0, 12); compressor(0, -20, 2, 8, 140, 0.5f);
                set(FILTER_BYPASS_ID, 1); set(LOWCUT_FREQ_ID, 100); set(PEAK_FREQ_ID, 2600); set(PEAK_GAIN_ID, 1.5f);
                set(MIX_ID, 0.8f); break;
            case 6:
                band(DRIVE_ID, 0, 5); insert(0, 0, Type::chorus, {0.3f, 25, 16, 8, 80, 16});
                insert(0, 1, Type::delay, {375, 25, 4800, 50, 4, 12}); set(MIX_ID, 0.8f); break;
            case 7:
                band(DRIVE_BYPASS_ID, 0, 0); insert(0, 0, Type::granular, {55, -40, 7, 25, 65, 40});
                set(fire::clouds_params::parameterID(0, 0, fire::clouds_params::spreadField), 0.8f);
                set(fire::clouds_params::parameterID(0, 0, fire::clouds_params::feedbackField), 0.25f);
                set(fire::clouds_params::parameterID(0, 0, fire::clouds_params::reverbField), 0.45f); break;
            case 8:
                band(DRIVE_ID, 0, 8); insert(0, 0, Type::chordResonator, {48, 3, 55, 1.5f, 80, 45});
                insert(0, 1, Type::reverb, {70, 45, 18, 100, 100, 20}); break;
            case 9:
                band(DRIVE_ID, 0, 18); band(WIDTH_BYPASS_ID, 0, 1); motion("pan1", 0.2f, false); break;
            case 10:
                split(180); band(DRIVE_ID, 0, 5); band(DRIVE_ID, 1, 35); motion("drive2", 0.25f, true); break;
            case 11:
                band(DRIVE_ID, 0, 20); set(DOWNSAMPLE_BYPASS_ID, 1); set(DOWNSAMPLE_ID, 6); set(BIT_DEPTH_ID, 8);
                set("lofiTape", 0.4f); set("lofiWow", 0.15f); set("lofiFlutter", 0.06f);
                set(FILTER_BYPASS_ID, 1); set(LOWCUT_FREQ_ID, 400); set(HIGHCUT_FREQ_ID, 5000); set(MIX_ID, 0.7f); break;
        }
        results.push_back(std::move(preset));
    }
    return results;
}
}
