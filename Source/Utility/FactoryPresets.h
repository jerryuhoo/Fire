#pragma once
#include "../PluginProcessor.h"
#include <array>
#include <vector>

namespace fire::factory
{
struct Definition
{
    juce::String key, category, name, description;
    int categoryIndex = -1, variation = 0, colourModel = 0, spaceModel = 1;
};

inline int colourForName(const juce::String& name, int category, int variation)
{
    const auto has = [&](const char* text) {return name.containsIgnoreCase(text);};
    if (has("Pentode")) return 1;
    if (has("Console")) return 2;
    if (has("Tweed")) return 3;
    if (has("British")) return 4;
    if (has("Modern")) return 5;
    if (has("Diode")) return 6;
    if (has("Germanium")) return 7;
    if (has("Silicon")) return 8;
    if (has("MOSFET")) return 9;
    if (has("Transformer") || has("Iron")) return 10;
    if (has("Tape") || has("Oxide") || has("Reels") || has("Transport")) return 11;
    if (has("Tube") || has("Triode") || has("Valve")) return 0;
    if (has("Fuzz")) return variation % 2 == 0 ? 7 : 8;
    // Voice, acoustic keys and worn-media scenes start with gentler circuits.
    constexpr std::array<int, 5> gentle {0, 2, 11, 1, 10};
    if (category == 0 || category == 5) return gentle[static_cast<size_t>(variation % 5)];
    if (category == 7) return 11;
    return variation % analog::count;
}

inline int spaceForName(const juce::String& name, int variation)
{
    for (int model = 1; model < space::count; ++model)
        if (name.containsIgnoreCase(space::names[static_cast<size_t>(model)])) return model;
    return 1 + variation % 5;
}
inline const std::array<Definition, 12> legacyDefinitions{{
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

inline const std::array<const char*, 10> categories {"Vocals", "Synths", "Drums", "Bass", "Guitar", "Keys", "Spaces", "Lo-Fi", "Rhythm", "Effects"};
inline const std::vector<Definition> definitions = []
{
    constexpr const char* names[10][20] {
        {"Velvet Voice", "Air Presence", "Tube Whisper", "Console Intimacy", "Gentle Pentode", "Plate Silk", "Spring Close", "Hall Breath", "Chamber Seat", "Warm Doubler", "Amber Chorus", "Ribbon Echo", "Soft Edge", "Radio Voice", "Parallel Glow", "Dark Halo", "Wide Air", "Vocal Motion", "Presence Bite", "Fuzz Formants"},
        {"Neon Triode", "Glass Pulse", "Pentode Sweep", "Analog Bloom", "British Lead", "Modern Stack", "Tape Keys", "Transformer Pad", "MOSFET Bassline", "Germanium Drone", "Silicon Spark", "Diode Prism", "Hall Orbit", "Plate Horizon", "Spring Circuit", "Chamber Pluck", "Echo Lattice", "Chorus Cloud", "Granular Skyline", "Rhythm Engine"},
        {"Parallel Impact", "Snare Voltage", "Tape Memory", "Console Punch", "Tweed Attack", "British Break", "Tube Crush", "Pentode Snap", "MOSFET Kick", "Germanium Room", "Silicon Crack", "Diode Glue", "Plate Snare", "Hall Toms", "Spring Percussion", "Chamber Kit", "Dusty Loop", "Stereo Shards", "Transient Echo", "Moving Breaks"},
        {"Solid Foundation", "Moving Harmonics", "Triode Body", "Pentode Bite", "Console Weight", "Tweed Growl", "British Bass", "Modern Grind", "Diode Midrange", "Germanium Sub", "Silicon Edge", "MOSFET Focus", "Transformer Iron", "Tape Round", "Parallel Clamp", "Stereo Overtones", "Filtered Echo", "Short Chamber", "Rhythmic Low End", "Resonant Bass"},
        {"Warm Valve", "Clean Console", "Tweed Porch", "British Crunch", "Modern Lead", "Diode Drive", "Germanium Velvet", "Silicon Wall", "MOSFET Edge", "Iron Breakup", "Tape Amp", "Pentode Shine", "Spring Surf", "Plate Solo", "Hall Sustain", "Chamber Cab", "Stereo Chorus", "Dotted Echo", "Shattered Strings", "Resonant Chord"},
        {"Triode Piano", "Console Rhodes", "Tape Wurl", "Pentode Clav", "Warm Organ", "Tweed Keys", "British Electric", "MOSFET Pluck", "Transformer Body", "Diode Accent", "Plate Felt", "Hall Concert", "Spring Electric", "Chamber Upright", "Chorus Velvet", "Echo Arpeggio", "Lo-Fi Study", "Granular Bell", "Resonance Bed", "Stepped Keys"},
        {"Clouds Bloom", "Minor Resonance", "Room Linen", "Hall Cathedral", "Plate Silver", "Spring Amber", "Chamber Oak", "Room Close", "Hall Horizon", "Plate Mist", "Spring Night", "Chamber Dark", "Wide Hall", "Bright Plate", "Deep Spring", "Tiny Room", "Long Chamber", "Echo Room", "Granular Skyline", "Resonant Space"},
        {"Tape Cassette", "Dusty Console", "Soft Oxide", "Worn Reels", "Pitch Memory", "Flutter Portrait", "Warm Grain", "Radio Band", "Broken Speaker", "Pixel Dust", "Vinyl Room", "Low Bit Halo", "Tape Spring", "Dark Transport", "Mono Photograph", "Stereo Patina", "Slow Drift", "Crunch Diary", "Grainy Keys", "Fuzz Archive"},
        {"Stereo Pulse", "Stepped Drive", "Quarter Voltage", "Eighth Prism", "Dotted Motion", "Triplet Echo", "Gate Horizon", "Slow Breathing", "Fast Sparks", "Swing Texture", "Clocked Hall", "Pulsed Plate", "Spring Groove", "Chamber Steps", "Pan Orbit", "Filter Dance", "Drive Sequence", "Flutter Grid", "Granular Tick", "Resonant Rhythm"},
        {"Broken Radio", "Glass Collapse", "Tube Feedback", "Iron Cinema", "Spring Collision", "Hall Fragments", "Plate Ghost", "Resonant Texture", "Pitch Corridor", "Minor Machinery", "Fuzz Storm", "Silicon Shards", "MOSFET Gate", "Tape Geometry", "Bitcrush Room", "Stereo Tornado", "Delay Maze", "Granular Orbit", "Resonant Metal", "Signal Sculpture"}
    };
    std::vector<Definition> result(legacyDefinitions.begin(), legacyDefinitions.end());
    std::array<int, 10> counts {};
    for (auto& definition : result)
    {
        if (definition.category == "Atmosphere") definition.category = "Spaces";
        if (definition.category == "Creative") definition.category = "Effects";
        for (int category = 0; category < 10; ++category)
            if (definition.category == categories[static_cast<size_t>(category)])
            {definition.categoryIndex = category; definition.variation = counts[static_cast<size_t>(category)]++;}
    }
    for (int category = 0; category < 10; ++category)
        for (int variation = counts[static_cast<size_t>(category)]; variation < 20; ++variation)
        {
            const auto group = juce::String(categories[static_cast<size_t>(category)]);
            Definition definition;
            definition.key = group.toLowerCase().removeCharacters(" -") + "-" + juce::String(variation + 1).paddedLeft('0', 2);
            definition.category = group; definition.name = names[category][variation];
            definition.colourModel = colourForName(definition.name, category, variation);
            definition.spaceModel = spaceForName(definition.name, variation);
            const auto colour = juce::String(analog::names[static_cast<size_t>(definition.colourModel)]);
            const auto ambience = juce::String(space::names[static_cast<size_t>(definition.spaceModel)]);
            definition.description = category == 6 ? ambience + " ambience with a clean source and a diffuse stereo tail."
                : category == 3 ? colour + " harmonics above a centred, restrained sub band."
                : category == 8 ? "Clocked modulation and echoes with " + colour + " colour."
                : group + " sound design with " + colour + " colour and " + ambience + " space.";
            definition.description += " Macros: intensity, width, tone/space and dry blend.";
            definition.categoryIndex = category; definition.variation = variation; result.push_back(std::move(definition));
        }
    return result;
}();

inline std::vector<std::unique_ptr<juce::XmlElement>> create(FireAudioProcessor& processor, int requestedIndex = -1)
{
    // Build an independent complete default model; generating factory presets
    // never changes the live sound, user files, recorded material or A/B state.
    juce::XmlElement defaults("WINGSFIRE");
    defaults.setAttribute("presetFormatVersion", 2);
    for (const auto* marker : {"ottSchemaVersion", "insertEffectsSchemaVersion", "modulationEffectsSchemaVersion", "resonatorSchemaVersion", "driveCompSchemaVersion",
        "modulationSourcesSchemaVersion", "moduleOrderSchemaVersion", "eqSchemaVersion", "lfoBankSchemaVersion", "coreModulesSchemaVersion", "analogShapesSchemaVersion", "reverbModelsSchemaVersion"}) defaults.setAttribute(marker, 1);
    defaults.setAttribute("cloudsSchemaVersion", fire::clouds_params::schemaVersion);
    defaults.setAttribute("pluginVersion", VERSION);
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
        if (requestedIndex >= 0 && index != static_cast<size_t>(requestedIndex)) continue;
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
            set(fire::core_modules::parameterID(scope, slot, fire::core_modules::typeField), fire::core_modules::encodeType(type));
            set(parameterID(scope, slot, enabledField), 1);
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
            default:
            {
                const int category = definition.categoryIndex, variant = definition.variation;
                const float v = static_cast<float>(variant);
                const auto colour = [&](int number, float amount)
                {
                    band(DRIVE_ID, number, amount); band(SHAPE_BYPASS_ID, number, 1); band(SHAPE_MIX_ID, number, .75f);
                    set(fire::analog_params::bandID(number), static_cast<float>(1 + definition.colourModel));
                };
                const auto room = [&](int slot, float wet, float size)
                {
                    insert(0, slot, Type::reverb, {size, 25 + v * 1.5f, 8 + v * 2, 85 + v * .7f, 60 + v * 6, wet});
                    set(fire::reverb_params::parameterID(0, slot), static_cast<float>(definition.spaceModel));
                };
                colour(0, 9 + v * .75f); set(MIX_ID, .72f + v * .012f);
                if (category == 0)
                {compressor(0, -23 + v * .3f, 2 + v * .06f, 7 + v * .5f, 95 + v * 4, .55f); room(0, 10 + v * .6f, 30 + v * 2); set(FILTER_BYPASS_ID, 1); set(LOWCUT_FREQ_ID, 80 + v * 2); set(PEAK_FREQ_ID, 1900 + v * 130); set(PEAK_GAIN_ID, 1 + v * .08f);}
                else if (category == 1)
                {split(180 + v * 9); colour(1, 22 + v); band(SHAPE_BYPASS_ID, 0, 0); band(DRIVE_ID, 0, 4); insert(0, 0, Type::chorus, {.13f + v * .03f, 20 + v, 12 + v * .3f, 5, 85, 12 + v * .5f}); room(1, 14 + v * .9f, 45 + v * 1.7f);}
                else if (category == 2)
                {colour(0, 18 + v); compressor(0, -20, 2.5f + v * .1f, 13 + v * .7f, 70 + v * 5, .55f); room(0, 6 + v * .55f, 20 + v * 1.5f); set(MIX_ID, .65f + v * .01f);}
                else if (category == 3)
                {split(160 + v * 4); colour(1, 25 + v * 1.1f); band(SHAPE_BYPASS_ID, 0, 0); band(DRIVE_ID, 0, 5); band(WIDTH_BYPASS_ID, 0, 1); band(WIDTH_ID, 0, 0); compressor(1, -22, 3 + v * .05f, 16, 140, .5f);}
                else if (category == 4)
                {colour(0, 20 + v * 1.25f); room(0, 12 + v, 35 + v * 2); if (variant % 3 == 0) insert(0, 1, Type::delay, {180 + v * 12, 20 + v * .4f, 5200, 65, 0, 10 + v * .3f});}
                else if (category == 5)
                {colour(0, 8 + v * .55f); room(0, 15 + v * .8f, 40 + v * 2); insert(0, 1, Type::chorus, {.18f + v * .012f, 12 + v * .6f, 16, 3, 80, 8 + v * .4f});}
                else if (category == 6)
                {band(DRIVE_BYPASS_ID, 0, 0); band(SHAPE_BYPASS_ID, 0, 0); room(0, 28 + v * 1.7f, 30 + v * 3); if (variant > 14) insert(0, 1, Type::delay, {240 + v * 17, 25 + v * .5f, 6500, 70, 0, 18});}
                else if (category == 7)
                {set(DOWNSAMPLE_BYPASS_ID, 1); set(DOWNSAMPLE_ID, 1 + static_cast<float>(variant % 8)); set(BIT_DEPTH_ID, 9 + static_cast<float>(variant % 7)); set("lofiTape", .22f + v * .027f); set("lofiWow", .02f + v * .009f); set("lofiFlutter", .015f + v * .005f); set(JITTER_ID, .006f * v); room(0, 8 + v * .6f, 25 + v * 2);}
                else if (category == 8)
                {band(WIDTH_BYPASS_ID, 0, 1); motion(variant % 2 == 0 ? "drive1" : "pan1", .12f + v * .009f, variant % 3 == 0); set("lfoRateSync1", static_cast<float>(4 + variant % 12)); insert(0, 0, Type::delay, {180 + v * 10, 25, 4500 + v * 100, 65, static_cast<float>(1 + variant % 7), 12 + v * .8f});}
                else
                {
                    if (variant % 3 == 0) {insert(0, 0, Type::granular, {35 + v, -55 + v * 2, static_cast<float>(variant % 25 - 12), 10 + v * 2, 40 + v, 28 + v});}
                    else if (variant % 3 == 1) insert(0, 0, Type::chordResonator, {48 + static_cast<float>(variant % 12), static_cast<float>(variant % 8), 35 + v * 2, .3f + v * .075f, 85, 30 + v});
                    else insert(0, 0, Type::phaser, {.12f + v * .04f, 55 + v, 350 + v * 120, 35, 90, 25 + v});
                    room(1, 16 + v, 40 + v * 2); set(MIX_ID, .6f + v * .013f);
                }
                // Named scenes select different arrangements, beyond their
                // circuit, drive, damping and timing variations.
                const auto named = [&](const char* text) {return definition.name.containsIgnoreCase(text);};
                const auto echo = [&](int slot, bool synced)
                {insert(0, slot, Type::delay, {240 + v * 13, 22 + v * .65f, 4200 + v * 90, 65, synced ? 5.0f : 0.0f, 12 + v * .75f});};
                const auto chorus = [&](int slot)
                {insert(0, slot, Type::chorus, {.19f + v * .018f, 25 + v, 16, 6, 90, 12 + v * .6f});};
                const auto grains = [&](int slot)
                {insert(0, slot, Type::granular, {40 + v, -25, 5 + static_cast<float>(variant % 7), 22 + v, 60, 25 + v});};
                const auto resonance = [&](int slot, float root)
                {insert(0, slot, Type::chordResonator, {root, 3, 45 + v, .55f + v * .04f, 80, 22 + v});};
                if (category == 0)
                {
                    if (named("Echo")) echo(1, true);
                    else if (named("Doubler") || named("Chorus")) chorus(1);
                    else if (named("Motion")) motion("pan1", .12f, false);
                    if (named("Radio")) {set(LOWCUT_FREQ_ID, 350); set(HIGHCUT_FREQ_ID, 4400);}
                }
                else if (category == 1)
                {
                    if (named("Echo")) echo(0, true);
                    else if (named("Granular")) grains(0);
                    else if (named("Rhythm")) {echo(0, true); motion("mix2", .16f, true);}
                }
                else if (category == 2)
                {
                    if (named("Echo")) echo(1, false);
                    else if (named("Shards")) chorus(1);
                    else if (named("Moving")) motion("pan1", .08f, false);
                }
                else if (category == 3)
                {
                    if (named("Echo")) echo(0, true);
                    else if (named("Chamber")) room(0, 13, 20);
                    else if (named("Rhythmic")) motion("mix2", .14f, true);
                    else if (named("Resonant")) resonance(0, 36);
                }
                else if (category == 4)
                {
                    if (named("Chorus")) chorus(1);
                    else if (named("Echo")) echo(1, true);
                    else if (named("Shattered")) grains(1);
                    else if (named("Resonant")) resonance(1, 48);
                }
                else if (category == 5)
                {
                    if (named("Echo")) echo(1, true);
                    else if (named("Granular")) grains(1);
                    else if (named("Resonance")) resonance(1, 48);
                    else if (named("Stepped")) motion("pan1", .18f, true);
                    else if (named("Lo-Fi")) {set(DOWNSAMPLE_BYPASS_ID, 1); set(BIT_DEPTH_ID, 12); set("lofiTape", .35f);}
                }
                else if (category == 6)
                {
                    if (named("Granular")) grains(1);
                    else if (named("Resonant")) resonance(1, 48);
                    if (named("Tiny") || named("Close")) room(0, 32, 12);
                    else if (named("Cathedral") || named("Long")) room(0, 60, 95);
                }
                else if (category == 7)
                {
                    if (named("Mono")) {band(WIDTH_BYPASS_ID, 0, 1); band(WIDTH_ID, 0, 0);}
                    else if (named("Stereo")) {band(WIDTH_BYPASS_ID, 0, 1); band(WIDTH_ID, 0, 1.3f);}
                    if (named("Slow")) set("lofiWow", .28f);
                }
                else if (category == 8)
                {
                    if (named("Hall") || named("Plate") || named("Spring") || named("Chamber")) room(1, 24, 58);
                    else if (named("Granular")) grains(1);
                    else if (named("Resonant")) resonance(1, 48);
                }
                else if (category == 9)
                {
                    if (named("Feedback") || named("Delay")) echo(0, false);
                    else if (named("Granular") || named("Pitch")) grains(0);
                    else if (named("Resonant") || named("Minor")) resonance(0, 48);
                    if (named("Bitcrush")) {set(DOWNSAMPLE_BYPASS_ID, 1); set(DOWNSAMPLE_ID, 5); set(BIT_DEPTH_ID, 8);}
                    if (named("Gate")) motion("mix1", .35f, true);
                }
                break;
            }
        }
        if (processor.treeState.getParameter("macro1") != nullptr)
        {
            const auto* bandCount = processor.treeState.getParameter(NUM_BANDS_ID);
            const bool splitBand = bandCount && bandCount->convertFrom0to1(static_cast<float>(preset->getDoubleAttribute(NUM_BANDS_ID))) > 1.0f;
            const int targetBand = splitBand ? 1 : 0;
            const auto routeMacro = [&](int macro, const juce::String& target, float depth)
            {
                auto* routes = preset->getChildByName("MODULATION_STATE");
                for (auto* child : routes->getChildIterator()) if (child->getStringAttribute("target") == target) return;
                ModulationRouting route; route.sourceLfoIndex = fire::mod_sources::firstMacro + macro;
                route.targetParameterID = target; route.depth = depth; route.isBipolar = false;
                route.writeToXml(*routes->createNewChildElement("ROUTING"));
            };
            auto intensity = (definition.categoryIndex == 6 && index >= 12) ? fire::effects::parameterID(0, 0, 5) : index == 7 ? fire::effects::parameterID(0, 0, 5)
                : index == 10 ? juce::String("mix2") : ParameterIDAndName::getIDString(DRIVE_ID, targetBand);
            if (index >= 12)
                for (auto* routing : preset->getChildByName("MODULATION_STATE")->getChildIterator())
                    if (routing->getStringAttribute("target") == intensity)
                    {intensity = ParameterIDAndName::getIDString(SHAPE_MIX_ID, targetBand); break;}
            routeMacro(0, intensity, index == 10 ? -0.3f : 0.2f);
            band(WIDTH_BYPASS_ID, targetBand, 1);
            routeMacro(1, ParameterIDAndName::getIDString(WIDTH_ID, targetBand), 0.25f);
            if (index == 6) routeMacro(2, fire::effects::parameterID(0, 1, 5), 0.25f);
            else if (index == 7) routeMacro(2, fire::clouds_params::parameterID(0, 0, fire::clouds_params::feedbackField), 0.3f);
            else if (index == 8) routeMacro(2, fire::effects::parameterID(0, 1, 5), 0.3f);
            else if (index >= 12 && definition.categoryIndex == 6) routeMacro(2, fire::effects::parameterID(0, 0, 0), 0.2f);
            else { set(FILTER_BYPASS_ID, 1); routeMacro(2, PEAK_GAIN_ID, 0.12f); }
            routeMacro(3, MIX_ID, -0.35f);
            if (index < 12)
                preset->setAttribute("presetDescription", preset->getStringAttribute("presetDescription")
                    + " Macros: 1 intensity, 2 width, 3 tone/space, 4 dry blend.");
        }
        results.push_back(std::move(preset));
    }
    return results;
}
}
