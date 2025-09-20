/*
  ==============================================================================

    InterfaceDefines.h
    Created: 13 Dec 2020 11:20:49pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

//#include "../../JuceLibraryCode/JucePluginDefines.h"

#define VERSION                 "1.5.0b"
#define GITHUB_LINK             "https://github.com/jerryuhoo/Fire"
#define GITHUB_TAG_LINK         "https://github.com/jerryuhoo/Fire/releases/tag/"
#define PRESET_EXETENSION       ".fire"
#define AUTO_UPDATE_ID          "autoUpdate"
#define AUTO_UPDATE_NAME        "Auto Check For Updates"

// no bands
#define HQ_ID                   "hq"
#define HQ_NAME                 "Hq"

// band params (has band index)
#define DRIVE_ID                "drive"
#define DRIVE_NAME              "Drive"
#define MODE_ID                 "mode"
#define MODE_NAME               "Mode"
#define DRIVE_NAME              "Drive"
#define LINKED_ID               "linked"
#define LINKED_NAME             "Linked"
#define SAFE_ID                 "safe"
#define SAFE_NAME               "Safe"
#define EXTREME_ID              "extreme"
#define EXTREME_NAME            "Extreme"
#define COMP_RATIO_ID           "compRatio"
#define COMP_RATIO_NAME         "CompRatio"
#define COMP_THRESH_ID          "compThresh"
#define COMP_THRESH_NAME        "CompThresh"
#define WIDTH_ID                "width"
#define WIDTH_NAME              "Width"
#define OUTPUT_ID               "output"     // Note: globalOutput is "output" itself
#define OUTPUT_NAME             "Output"
#define GLOBAL_OUTPUT_NAME      "GlobalOutput"
#define MIX_ID                  "mix"
#define MIX_NAME                "Mix"
#define GLOBAL_MIX_NAME         "GlobalMix"
#define BIAS_ID                 "bias"
#define BIAS_NAME               "Bias"
#define DC_BYPASS_ID            "dcBypass"
#define DC_BYPASS_NAME          "DcBypass"
#define REC_ID                  "rec"
#define REC_NAME                "Rec"

// global params
#define DOWNSAMPLE_ID           "downSample"
#define DOWNSAMPLE_NAME         "DownSample"
#define LOWCUT_FREQ_ID          "lowcutFreq"
#define LOWCUT_FREQ_NAME        "LowcutFreq"
#define HIGHCUT_FREQ_ID         "highcutFreq"
#define HIGHCUT_FREQ_NAME       "HighcutFreq"
#define PEAK_FREQ_ID            "peakFreq"
#define PEAK_FREQ_NAME          "PeakFreq"
#define LOWCUT_GAIN_ID          "lowCutGain"
#define LOWCUT_GAIN_NAME        "LowCutGain"
#define HIGHCUT_GAIN_ID         "highCutGain"
#define HIGHCUT_GAIN_NAME       "HighCutGain"
#define PEAK_GAIN_ID            "peakGain"
#define PEAK_GAIN_NAME          "PeakGain"
#define LOWCUT_Q_ID             "lowcutQ"
#define LOWCUT_Q_NAME           "LowcutQ"
#define PEAK_Q_ID               "peakQ"
#define PEAK_Q_NAME             "PeakQ"
#define HIGHCUT_Q_ID            "highcutQ"
#define HIGHCUT_Q_NAME          "HighcutQ"
#define LOWCUT_SLOPE_ID         "lowcutSlope"
#define LOWCUT_SLOPE_NAME       "LowcutSlope"
#define HIGHCUT_SLOPE_ID        "highcutSlope"
#define HIGHCUT_SLOPE_NAME      "HighcutSlope"
#define LOWCUT_BYPASSED_ID      "lowcutBypassed"
#define LOWCUT_BYPASSED_NAME    "LowcutBypassed"
#define PEAK_BYPASSED_ID        "peakBypassed"
#define PEAK_BYPASSED_NAME      "PeakBypassed"
#define HIGHCUT_BYPASSED_ID     "highcutBypassed"
#define HIGHCUT_BYPASSED_NAME   "HighcutBypassed"
#define OFF_ID                  "off"
#define OFF_NAME                "Off"
#define PRE_ID                  "pre"
#define PRE_NAME                "Pre"
#define POST_ID                 "post"
#define POST_NAME               "Post"
#define LOW_ID                  "low"
#define LOW_NAME                "Low"
#define BAND_ID                 "band"
#define BAND_NAME               "Band"
#define HIGH_ID                 "high"
#define HIGH_NAME               "High"
#define LIMITER_THRESH_ID       "limiterThresh"
#define LIMITER_THRESH_NAME     "LimiterThresh"
#define LIMITER_RELEASE_ID      "limiterRelease"
#define LIMITER_RELEASE_NAME    "LimiterRelease"

// multiband
#define FREQ_ID                 "freq" // freq1 - freq3
#define FREQ_NAME               "Freq"

#define BAND_ENABLE_ID          "multibandEnable"
#define BAND_ENABLE_NAME        "MultibandEnable"

#define BAND_SOLO_ID            "multibandSolo"
#define BAND_SOLO_NAME          "MultibandSolo"

#define BAND_FOCUS_ID           "multibandFocus"
#define BAND_FOCUS_NAME         "MultibandFocus"

#define LINE_STATE_ID           "lineState" // lineState1 - lineState3
#define LINE_STATE_NAME         "LineState"

#define COMP_BYPASS_ID          "compressorBypass"
#define COMP_BYPASS_NAME        "CompressorBypass"

#define WIDTH_BYPASS_ID         "widthBypass"
#define WIDTH_BYPASS_NAME       "WidthBypass"

#define FILTER_BYPASS_ID        "filterBypass"
#define FILTER_BYPASS_NAME      "FilterBypass"
#define DOWNSAMPLE_BYPASS_ID    "downsampleBypass"
#define DOWNSAMPLE_BYPASS_NAME  "DownsampleBypass"
#define LIMITER_BYPASS_ID       "limiterBypass"
#define LIMITER_BYPASS_NAME     "LimiterBypass"
#define NUM_BANDS_ID            "numBands"
#define NUM_BANDS_NAME          "NumBands"

// LFO PARAMETERS
#define LFO_SYNC_MODE_ID        "lfoSyncMode"
#define LFO_SYNC_MODE_NAME      "LfoSyncMode"
#define LFO_RATE_SYNC_ID        "lfoRateSync"
#define LFO_RATE_SYNC_NAME      "LfoRateSync"
#define LFO_RATE_HZ_ID          "lfoRateHz"
#define LFO_RATE_HZ_NAME        "LfoRateHz"


// Define Font
#define KNOB_FONT           "Futura"
#define KNOB_FONT_SIZE      18.0f
#define KNOB_FONT_COLOUR    COLOUR1
#define KNOB_SUBFONT_COLOUR COLOUR7.withBrightness(0.8f)
#define KNOB_INNER_COLOUR   COLOUR7.withBrightness(slider.isEnabled() ? 0.8f : 0.5f)
#define KNOB_TICK_COLOUR    COLOUR7

// Define Components size
#define TEXTBOX_WIDTH       60
#define TEXTBOX_HEIGHT      30
#define KNOBSIZE            100
#define SCALED_KNOBSIZE     static_cast<int>(knobSize * scale)


// Define Position
#define INIT_WIDTH          1000.0f
#define INIT_HEIGHT         500.0f

#define KNOB_PANEL_1_X      SWITCH_X + SWITCH_WIDTH
#define KNOB_PANEL_2_X      SWITCH_X + SWITCH_WIDTH + KNOB_PANEL_1_WIDTH
#define KNOB_PANEL_3_X      SWITCH_X + SWITCH_WIDTH + KNOB_PANEL_1_WIDTH + OSC_HEIGHT * 2 + BUTTON_WIDTH

#define KNOB_PANEL_1_WIDTH  getWidth() / 4.0f
#define KNOB_PANEL_2_WIDTH  OSC_HEIGHT * 2

#define OSC_X               getWidth() / 10.0f - OSC_WIDTH / 2.0f
#define OSC_Y               PART1 + PART2 + getHeight() / 5.0f
#define OSC_WIDTH           getWidth() / 8.0f
#define OSC_HEIGHT          (getHeight() - PART1 - PART2) / 4.0f

#define METER_X             getWidth() / 10.0f * 2.0f
#define METER_Y             PART1 + PART2 + getHeight() / 5.0f
#define METER_WIDTH         getWidth() / 8.0f
#define METER_HEIGHT        (getHeight() - PART1 - PART2) / 4.0f

#define D_GRAPH_X           getWidth() / 10.0f - D_GRAPH_WIDTH / 2.0f
#define D_GRAPH_Y           PART1 + PART2 + getHeight() / 10.0f + (getHeight() - PART1 - PART2) / 2.0f
#define D_GRAPH_WIDTH       getWidth() / 8.0f
#define D_GRAPH_HEIGHT      (getHeight() - PART1 - PART2) / 4.0f

#define WIDTH_GRAPH_X       getWidth() / 20.0f + getWidth() / 8.0f
#define WIDTH_GRAPH_Y       PART1 + PART2 + getHeight() / 10.0f + (getHeight() - PART1 - PART2) / 2.0f
#define WIDTH_GRAPH_WIDTH   getWidth() / 8.0f
#define WIDTH_GRAPH_HEIGHT  (getHeight() - PART1 - PART2) / 4.0f

#define SPEC_HEIGHT         getHeight() / 10.0f * 3.0f

#define DRIVE_X             KNOB_PANEL_2_X + OSC_HEIGHT - SCALED_KNOBSIZE / 2.0f
#define DRIVE_Y             OSC_Y + OSC_HEIGHT / 2.0f
#define DRIVE_SIZE          SCALED_KNOBSIZE * 2.0f

#define REC_X               KNOB_PANEL_1_X + SCALED_KNOBSIZE / 4.0f
#define BIAS_X              KNOB_PANEL_1_X + SCALED_KNOBSIZE + SCALED_KNOBSIZE / 4.0f
#define WIDTH_X             KNOB_PANEL_1_X + OSC_WIDTH - SCALED_KNOBSIZE / 2.0f

#define COMP_THRESH_X       KNOB_PANEL_1_X + SCALED_KNOBSIZE / 4.0f
#define COMP_RATIO_X        KNOB_PANEL_1_X + SCALED_KNOBSIZE + SCALED_KNOBSIZE / 4.0f
#define OUTPUT_X            KNOB_PANEL_3_X + SCALED_KNOBSIZE / 4.0f
#define MIX_X               KNOB_PANEL_3_X + SCALED_KNOBSIZE + SCALED_KNOBSIZE / 4.0f

#define CUTOFF_X            startX * 4.5f - SCALED_KNOBSIZE / 2.0f
#define RES_X               startX * 6.0f - SCALED_KNOBSIZE / 2.0f
#define DOWNSAMPLE_X        startX * 7.5f - SCALED_KNOBSIZE / 2.0f

#define FILTER_STATE_X      startX * 4.5f - SCALED_KNOBSIZE / 4.0f
#define FILTER_TYPE_X       startX * 6.0f - SCALED_KNOBSIZE / 4.0f
#define OUTPUT_X_G          startX * 9.0f - SCALED_KNOBSIZE / 2.0f
#define MIX_X_G             startX * 9.0f - SCALED_KNOBSIZE / 2.0f

#define VU_METER_WIDTH      getWidth() / 10.0f
#define VU_METER_HEIGHT     getHeight() / 10.0f * 9.0f
#define VU_METER_Y          getHeight() / 10.0f
#define VU_METER_X_1        getWidth() / 3.0f - VU_METER_WIDTH / 2.0f
#define VU_METER_X_2        getWidth() / 3.0f * 2.0f - VU_METER_WIDTH / 2.0f


// Switches
#define SWITCH_WIDTH        OSC_WIDTH / 10.0f
#define SWITCH_HEIGHT       OSC_HEIGHT / 2.0f
#define SWITCH_X            OSC_X + OSC_WIDTH * 2.0f

#define SWITCH_OSC_Y        OSC_Y
#define SWITCH_SHAPE_Y      OSC_Y + OSC_HEIGHT / 2.0f
#define SWITCH_COMP_Y       OSC_Y + OSC_HEIGHT
#define SWITCH_WIDTH_Y      OSC_Y + OSC_HEIGHT / 2.0f * 3.0f

// Buttons
#define BUTTON_X            SWITCH_X + SWITCH_WIDTH + KNOB_PANEL_1_WIDTH + OSC_HEIGHT * 2
#define BUTTON_WIDTH        OSC_WIDTH / 4.0f

// Define Color
#define COLOUR0             juce::Colour(244, 244, 210)
#define COLOUR1             juce::Colour(244, 208, 63)
#define COLOUR2             juce::Colour(243, 156, 18)
#define COLOUR3             juce::Colour(230, 126, 34)
#define COLOUR4             juce::Colour(211, 84, 0)
#define COLOUR5             juce::Colour(192, 57, 43)
#define COLOUR6             juce::Colour(40, 40, 40)
#define COLOUR7             juce::Colour(15, 15, 15)
#define COLOUR8             juce::Colour(10, 10, 10)
#define COLOUR_FONT         juce::Colour(100, 100, 100)
#define COLOUR_MASK_RED     juce::Colours::red.withAlpha(0.05f)
#define COLOUR_MASK_BLACK   juce::Colours::black.withAlpha(0.5f)
#define COLOUR_MASK_WHITE   juce::Colours::white.withAlpha(0.05f)
#define COLOUR_GROUP        juce::Colour(50, 50, 50)
#define DRIVE_COLOUR        juce::Colours::red
#define SHAPE_COLOUR        juce::Colours::yellow
#define WIDTH_COLOUR        juce::Colours::skyblue
#define COMP_COLOUR         juce::Colours::yellowgreen
#define FILTER_COLOUR       juce::Colours::hotpink
#define DOWNSAMPLE_COLOUR   juce::Colours::violet
#define LIMITER_COLOUR      juce::Colours::mediumpurple

// VU meters
#define SMOOTH_COEFF        0.5
