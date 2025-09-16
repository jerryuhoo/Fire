/*
  ==============================================================================

    Parameters.h
    Created: 15 Sep 2025 12:06:38am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once
#include "../GUI/InterfaceDefines.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_core/juce_core.h"
#include <vector>

// =============================================================================
// SHARED ENUMS
// =============================================================================

// Enum for LFO waveform types.
enum class LfoWaveform
{
    Sine,
    Triangle,
    SawtoothDown, // "Saw"
    SawtoothUp, // "Ramp"
    Square,
    User // For user-drawn shapes
};

// Enum for filter slope steepness.
enum Slope
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// A struct to hold all necessary info for a modulatable parameter.
struct ModulatableParameterInfo
{
    juce::String name; // The UI-facing name, e.g., "Drive"
    juce::String idBase; // The processor-facing ID base, e.g., "drive"
};

// A struct representing a final modulation target for UI lists.
struct ModulationTarget
{
    juce::String displayText; // e.g., "Drive (Band 1)"
    juce::String parameterID; // e.g., "drive_0"
};

struct MeterValues
{
    // Global Meters
    float inputLeftGlobal;
    float inputRightGlobal;
    float outputLeftGlobal;
    float outputRightGlobal;

    // Per-Band Meters (using a std::array for clean access)
    std::array<float, 4> inputLeftBands;
    std::array<float, 4> inputRightBands;
    std::array<float, 4> outputLeftBands;
    std::array<float, 4> outputRightBands;
};

struct ModulatedFilterValues
{
    float lowCutFreq;
    float lowCutGain;
    float lowCutQ;
    float highCutFreq;
    float highCutGain;
    float highCutQ;
    float peakFreq;
    float peakGain;
    float peakQ;
};

// =============================================================================
// PARAMETER DEFINITIONS
// =============================================================================

namespace ParameterIDAndName
{
    // Namespace for all raw parameter ID strings.
    const int versionNum = 1;

    // Todo: Replace InterfaceDefines.h
    // const juce::String DRIVE_ID = "drive";
    // const juce::String DRIVE_NAME = "Drive";

    // --- Part 1: Functions that return juce::String ---

    /**
     * @brief [String] Creates the raw string ID for an indexed parameter.
     * @param base      The base part of the parameter ID (e.g., DRIVE_ID).
     * @param index     The zero-based index of the band or channel.
     * @return juce::String Returns the final string ID (e.g., "drive1").
     */
    static inline juce::String getIDString(const juce::String& base, int index)
    {
        return base + juce::String(index + 1);
    }

    /**
     * @brief [String] Gets the raw string ID for a global (non-indexed) parameter.
     * @param base      The parameter ID (e.g., HQ_ID).
     * @return juce::String Returns the base string itself.
     */
    static inline juce::String getIDString(const juce::String& base)
    {
        return base;
    }

    /**
     * @brief [String] Creates the display name for an indexed parameter.
     * @param base      The base part of the parameter name (e.g., "Drive").
     * @param index     The zero-based index of the band or channel.
     * @return juce::String Returns the final display name (e.g., "Drive 1").
     */
    static inline juce::String getName(const juce::String& base, int index)
    {
        return base + " " + juce::String(index + 1);
    }

    // --- Part 2: Functions that return juce::ParameterID ---

    /**
     * @brief [ParameterID] Creates a full juce::ParameterID object for an indexed parameter.
     * @param base      The base part of the parameter ID (e.g., DRIVE_ID).
     * @param index     The zero-based index of the band or channel.
     * @return juce::ParameterID Returns the complete ParameterID object (e.g., {"drive1", 1}).
     */
    static inline juce::ParameterID getID(const juce::String& base, int index)
    {
        return { getIDString(base, index), versionNum };
    }

    /**
     * @brief [ParameterID] Creates a full juce::ParameterID object for a global parameter.
     * @param base      The parameter ID (e.g., HQ_ID).
     * @return juce::ParameterID Returns the complete ParameterID object (e.g., {"hq", 1}).
     */
    static inline juce::ParameterID getID(const juce::String& base)
    {
        return { base, versionNum };
    }

    /** * @brief Returns a definitive list of parameter names that are eligible for modulation.
     *
     * This static function provides a single source of truth for all parts of the plugin
     * that need to know which parameters can be targeted by an LFO. This includes:
     * - The BandPanel, for creating the ModulatableSlider components.
     * - The PluginEditor, for updating the modulation arcs on the sliders.
     * - The ModulationMatrixPanel, for populating the target selection dropdown menu.
     *
     * @return A constant reference to a vector of juce::String objects.
     */
    static const std::vector<juce::String>& getModulatableParameterNames()
    {
        // This static vector is initialized only once, the first time this function is called.
        static const std::vector<juce::String> modulatableNames = {
            DRIVE_NAME,
            COMP_RATIO_NAME,
            COMP_THRESH_NAME,
            WIDTH_NAME,
            OUTPUT_NAME,
            MIX_NAME,
            BIAS_NAME,
            REC_NAME
        };
        return modulatableNames;
    }

    /** * @brief Returns a definitive list of all modulatable parameters,
     * including their UI Name and their ID base.
     * This is the single source of truth for the entire application.
     */
    static const std::vector<ModulatableParameterInfo>& getModulatableParameterInfo()
    {
        // This static vector is initialized only once.
        static const std::vector<ModulatableParameterInfo> parameters = {
            { DRIVE_NAME, DRIVE_ID },
            { COMP_RATIO_NAME, COMP_RATIO_ID },
            { COMP_THRESH_NAME, COMP_THRESH_ID },
            { WIDTH_NAME, WIDTH_ID },
            { OUTPUT_NAME, OUTPUT_ID },
            { MIX_NAME, MIX_ID },
            { BIAS_NAME, BIAS_ID },
            { REC_NAME, REC_ID }
        };
        return parameters;
    }

    static const std::vector<ModulatableParameterInfo>& getGlobalParameterInfo()
    {
        static const std::vector<ModulatableParameterInfo> params = {
            { LOWCUT_FREQ_NAME, LOWCUT_FREQ_ID },
            { LOWCUT_Q_NAME, LOWCUT_Q_ID },
            { LOWCUT_GAIN_NAME, LOWCUT_GAIN_ID },
            { HIGHCUT_FREQ_NAME, HIGHCUT_FREQ_ID },
            { HIGHCUT_Q_NAME, HIGHCUT_Q_ID },
            { HIGHCUT_GAIN_NAME, HIGHCUT_GAIN_ID },
            { PEAK_FREQ_NAME, PEAK_FREQ_ID },
            { PEAK_Q_NAME, PEAK_Q_ID },
            { PEAK_GAIN_NAME, PEAK_GAIN_ID },
            { DOWNSAMPLE_NAME, DOWNSAMPLE_ID },
            { GLOBAL_OUTPUT_NAME, OUTPUT_ID },
            { GLOBAL_MIX_NAME, MIX_ID }
        };
        return params;
    }

    /**
     * @brief Returns a list of all modulation targets.
     *
     * This function generates a list of all possible modulation targets
     * by combining each modulatable parameter with each band.
     *
     * @return A vector of ModulationTarget objects.
     */
    static std::vector<ModulationTarget> getAllModulatableTargets()
    {
        std::vector<ModulationTarget> targets;
        const auto& modulatableParams = getModulatableParameterInfo();
        const auto& globalParams = getGlobalParameterInfo();

        for (int i = 0; i < 4; ++i)
        {
            for (const auto& paramInfo : modulatableParams)
            {
                targets.push_back({ paramInfo.name + " (Band " + juce::String(i + 1) + ")",
                                    getIDString(paramInfo.idBase, i) });
            }
        }

        for (const auto& paramInfo : globalParams)
        {
            // For global parameters, the name is the display text and the ID is the parameter ID
            targets.push_back({ paramInfo.name, getIDString(paramInfo.idBase) });
        }
        return targets;
    }
} // namespace ParameterIDAndName