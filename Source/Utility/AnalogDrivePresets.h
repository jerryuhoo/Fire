#pragma once

#include <array>

namespace fire::factory::analog_drive
{
enum class Layout { band, upperBand, master };
struct Tone { float lowCut, highCut, frequency, gain; };
struct Dynamics
{
    float threshold = -20, ratio = 2, attack = 20, release = 140, mix = 0;
};
struct Space { int model = 0; float size = 30, wet = 0; };
struct Envelope
{
    float attack = 8, release = 200, sensitivity = 12, depth = 0;
};
struct Scene
{
    const char* key;
    const char* name;
    const char* description;
    int model; // Zero-based Analog Hardware colour, independent of digital modes.
    float drive, blend, output;
    Tone tone;
    Layout layout = Layout::band;
    float crossover = 160;
    bool monoLow = false;
    Dynamics dynamics{};
    Space space{};
    Envelope envelope{};
};

// Deliberately authored signal paths, with one colour stage per scene. Blend
// sits around the entire Drive/Shape pair (or Master Shape), so quiet dry audio
// is never taken from an already boosted internal Shape input.
inline constexpr std::array<Scene, 36> scenes{{
    {"analog-valve-vocal-velvet", "Valve Vocal Velvet", "Soft vocal and acoustic-key warmth with light diction control.",
     0, 16, .86f, -2, {75, 18500, 2200, .5f}, Layout::band, 160, false, {-23, 1.8f, 8, 140, .35f}},
    {"analog-valve-bus-silk", "Valve Bus Silk", "Parallel Master valve colour for a smooth drum or instrument bus.",
     0, 12, .70f, -1.5f, {25, 18000, 1100, .4f}, Layout::master, 160, false, {-20, 1.5f, 28, 180, .25f}},
    {"analog-valve-bloom", "Valve Bloom", "The input envelope opens the valve blend on louder notes; quiet phrases stay clearer.",
     0, 22, .50f, -3, {45, 14000, 900, 1}, Layout::band, 160, false, {}, {}, {8, 260, 12, .32f}},

    {"analog-pentode-air", "Pentode Air", "Bright vocal or synth presence with restrained low frequencies.",
     1, 18, .82f, -3.2f, {100, 18000, 3600, 1.2f}},
    {"analog-pentode-pick", "Pentode Pick", "Pentode bite and a small plate behind picked strings or a clavinet.",
     1, 27, .91f, -4, {90, 13500, 2100, 1}, Layout::band, 160, false, {-24, 2.5f, 12, 100, .35f}, {3, 28, 8}},
    {"analog-pentode-parallel-edge", "Pentode Parallel Edge", "Envelope-driven upper-mid edge for drums; the lower stereo band stays clean.",
     1, 32, .60f, -3.5f, {45, 17000, 3400, 0}, Layout::upperBand, 2600, false, {}, {}, {1.5f, 80, 9, .25f}},

    {"analog-console-vocal-seat", "Console Vocal Seat", "Gentle console density brings a vocal forward without a long ambience tail.",
     2, 12, .92f, -1.5f, {75, 18500, 1600, .5f}, Layout::band, 160, false, {-23, 1.6f, 10, 130, .30f}},
    {"analog-console-bus-glue", "Console Bus Glue", "Light bus compression feeds a single Master console stage for shared colour.",
     2, 10, .85f, -1, {25, 18000, 700, .25f}, Layout::master, 160, false, {-20, 1.7f, 28, 200, .35f}},
    {"analog-console-keybed", "Console Keybed", "Console body for electric piano and organ with a short chamber behind the notes.",
     2, 17, .90f, -2, {45, 16500, 800, 1}, Layout::band, 160, false, {}, {5, 24, 7}},

    {"analog-tweed-porch", "Tweed Porch", "Warm guitar breakup, rounded highs and a close spring ambience.",
     3, 22, .90f, -4, {70, 7500, 1400, 1}, Layout::band, 160, false, {}, {4, 30, 9}},
    {"analog-tweed-touch", "Tweed Touch", "Tweed supply sag follows the playing; the envelope adds more colour to stronger attacks.",
     3, 30, .65f, -4, {80, 8200, 950, .5f}, Layout::band, 160, false, {}, {}, {5, 210, 10, .30f}},
    {"analog-tweed-split-growl", "Tweed Split Growl", "A centred clean sub supports tweed growl above 155 Hz for bass and low synths.",
     3, 31, .88f, -4, {30, 8000, 750, .4f}, Layout::upperBand, 155, true},

    {"analog-brit-rhythm-stack", "Brit Rhythm Stack", "Focused British rhythm crunch with tight lows and a rounded top end.",
     4, 30, 1, -5, {100, 7200, 1600, .8f}},
    {"analog-brit-lead-chamber", "Brit Lead Chamber", "Sustained British lead colour with a small chamber and a forward midrange.",
     4, 36, 1, -5, {110, 8000, 1800, 1.3f}, Layout::band, 160, false, {}, {5, 30, 8}},
    {"analog-brit-parallel-bass", "Brit Parallel Bass", "British upper-band crunch over a clean mono foundation; compression reins in the bite.",
     4, 34, .82f, -4, {35, 8500, 1200, .5f}, Layout::upperBand, 180, true, {-22, 2, 20, 120, .30f}},

    {"analog-modern-palm-mute", "Modern Palm Mute", "Tight high-gain rhythm colour with fast, light dynamics and controlled bass.",
     5, 32, 1, -6, {140, 6600, 1100, .6f}, Layout::band, 160, false, {-20, 2.2f, 5, 85, .30f}},
    {"analog-modern-singing-lead", "Modern Singing Lead", "Dense lead sustain and a low plate blend; Macro 1 pushes the upper harmonics.",
     5, 40, .94f, -6, {120, 7000, 1850, 1}, Layout::band, 160, false, {}, {3, 42, 8}},
    {"analog-modern-synth-bite", "Modern Synth Bite", "Velocity opens high-gain colour above 420 Hz while lower synth voices stay clean.",
     5, 35, .80f, -5, {45, 8400, 2400, .6f}, Layout::upperBand, 420, false, {}, {}, {3, 130, 8, .18f}},

    {"analog-diode-drum-snap", "Diode Drum Snap", "Parallel diode edge and a slow compressor attack preserve a drum loop's snap.",
     6, 25, .76f, -3.5f, {50, 12000, 2500, .5f}, Layout::band, 160, false, {-20, 2, 22, 95, .25f}},
    {"analog-diode-mid-bass", "Diode Mid Bass", "Diode midrange makes bass audible on small speakers above a clean mono low band.",
     6, 32, .90f, -4, {25, 10000, 950, .9f}, Layout::upperBand, 170, true},
    {"analog-diode-parallel-spark", "Diode Parallel Spark", "The envelope blends diode harmonics into stronger vocal, key or percussion attacks.",
     6, 28, .55f, -3, {70, 12000, 3200, 1.2f}, Layout::band, 160, false, {}, {}, {2, 160, 9, .30f}},

    {"analog-germanium-velvet-fuzz", "Germanium Velvet Fuzz", "Soft, asymmetric fuzz for guitar and monophonic synths with smoky upper frequencies.",
     7, 26, .88f, -5, {45, 5500, 650, 1}},
    {"analog-germanium-reed-keys", "Germanium Reed Keys", "Worn electric-key fuzz and a small spring with space left for the original attack.",
     7, 33, .80f, -5, {85, 6200, 1200, .8f}, Layout::band, 160, false, {}, {4, 24, 6}},
    {"analog-germanium-fuzz-bass", "Germanium Fuzz Bass", "Heavy germanium colour above 170 Hz keeps the fundamental clean and centred.",
     7, 35, .93f, -5, {25, 5800, 800, .7f}, Layout::upperBand, 170, true},

    {"analog-silicon-fuzz-wall", "Silicon Fuzz Wall", "Dense silicon fuzz for guitars and synth leads with a focused presence range.",
     8, 34, .90f, -5.5f, {100, 9800, 1900, .5f}},
    {"analog-silicon-percussion-sparks", "Silicon Percussion Sparks", "Fast envelope accents add silicon fuzz to hits while the rest of the loop stays clearer.",
     8, 25, .50f, -4, {60, 13000, 3800, 1}, Layout::band, 160, false, {}, {}, {1, 110, 12, .40f}},
    {"analog-silicon-split-lead", "Silicon Split Lead", "Upper-band silicon bite with a restrained plate; the lower register keeps its original tone.",
     8, 37, .90f, -5, {65, 11000, 2200, .8f}, Layout::upperBand, 430, false, {}, {3, 35, 7}},

    {"analog-mosfet-clean-push", "MOSFET Clean Push", "A mild push for strings or vocals with light dynamics and a mostly clear top end.",
     9, 15, .94f, -2, {75, 16000, 1800, .5f}, Layout::band, 160, false, {-24, 1.6f, 20, 100, .30f}},
    {"analog-mosfet-string-edge", "MOSFET String Edge", "MOSFET attack and a close spring add definition to plucked strings.",
     9, 26, .90f, -3.5f, {100, 12000, 2600, 1}, Layout::band, 160, false, {}, {4, 22, 6}},
    {"analog-mosfet-parallel-glue", "MOSFET Parallel Glue", "A parallel Master MOSFET stage adds bus edge without replacing the clean transient.",
     9, 20, .55f, -2.5f, {35, 15000, 1000, .25f}, Layout::master, 160, false, {-19, 1.8f, 30, 160, .20f}},

    {"analog-iron-vocal-weight", "Iron Vocal Weight", "One transformer stage adds low-mid vocal density with gentle level control.",
     10, 18, .90f, -3, {80, 16000, 1100, .5f}, Layout::band, 160, false, {-23, 2, 9, 135, .30f}},
    {"analog-iron-program-bus", "Iron Program Bus", "A restrained Master transformer gives drums, keys or a mixed bus shared magnetic weight.",
     10, 12, .65f, -2, {25, 18000, 650, .3f}, Layout::master, 160, false, {-20, 1.5f, 30, 180, .25f}},
    {"analog-iron-split-foundation", "Iron Split Foundation", "Transformer body above 120 Hz adds bass detail while a clean mono sub anchors the sound.",
     10, 27, .86f, -4, {20, 14000, 450, 1}, Layout::upperBand, 120, true},

    {"analog-tape-studio-print", "Tape Studio Print", "A single Master tape pass softens a bus's peaks and top end; use Match to judge the colour.",
     11, 15, .90f, -2, {25, 18000, 1400, .2f}, Layout::master},
    {"analog-tape-drum-round", "Tape Drum Round", "Parallel tape colour and light slow-attack compression round drum hits without losing all their punch.",
     11, 22, .75f, -3, {35, 16000, 1800, .6f}, Layout::band, 160, false, {-18, 1.8f, 28, 140, .20f}},
    {"analog-tape-rhodes-bloom", "Tape Rhodes Bloom", "Louder electric-key notes bloom into tape colour and a short chamber; soft notes retain clarity.",
     11, 25, .60f, -3, {45, 14500, 800, .9f}, Layout::band, 160, false, {}, {5, 30, 7}, {8, 360, 12, .30f}}
}};
}
