/*
  ==============================================================================

    InterfaceDefines.h
    Created: 13 Dec 2020 11:20:49pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../JuceLibraryCode/JucePluginDefines.h"

#define VERSION             JucePlugin_VersionString
#define GITHUB_LINK         "https://github.com/jerryuhoo/Fire"
#define GITHUB_TAG_LINK     "https://github.com/jerryuhoo/Fire/releases/tag/"
#define PRESET_EXETENSION   ".fire"

// paramID paramName
#define DRIVE_ID1           "drive1"
#define DRIVE_NAME1         "Drive1"
#define DRIVE_ID2           "drive2"
#define DRIVE_NAME2         "Drive2"
#define DRIVE_ID3           "drive3"
#define DRIVE_NAME3         "Drive3"
#define DRIVE_ID4           "drive4"
#define DRIVE_NAME4         "Drive4"

#define HQ_ID               "hq"
#define HQ_NAME             "Hq"

#define MODE_ID1            "mode1"
#define MODE_NAME1          "Mode1"
#define MODE_ID2            "mode2"
#define MODE_NAME2          "Mode2"
#define MODE_ID3            "mode3"
#define MODE_NAME3          "Mode3"
#define MODE_ID4            "mode4"
#define MODE_NAME4          "Mode4"

#define LINKED_ID1          "linked1"
#define LINKED_NAME1        "Linked1"
#define LINKED_ID2          "linked2"
#define LINKED_NAME2        "Linked2"
#define LINKED_ID3          "linked3"
#define LINKED_NAME3        "Linked3"
#define LINKED_ID4          "linked4"
#define LINKED_NAME4        "Linked4"

#define SAFE_ID1            "safe1"
#define SAFE_NAME1          "Safe1"
#define SAFE_ID2            "safe2"
#define SAFE_NAME2          "Safe2"
#define SAFE_ID3            "safe3"
#define SAFE_NAME3          "Safe3"
#define SAFE_ID4            "safe4"
#define SAFE_NAME4          "Safe4"

#define COMP_RATIO_ID1      "compRatio1"
#define COMP_RATIO_NAME1    "CompRatio1"
#define COMP_RATIO_ID2      "compRatio2"
#define COMP_RATIO_NAME2    "CompRatio2"
#define COMP_RATIO_ID3      "compRatio3"
#define COMP_RATIO_NAME3    "CompRatio3"
#define COMP_RATIO_ID4      "compRatio4"
#define COMP_RATIO_NAME4    "CompRatio4"

#define COMP_THRESH_ID1     "compThresh1"
#define COMP_THRESH_NAME1   "CompThresh1"
#define COMP_THRESH_ID2     "compThresh2"
#define COMP_THRESH_NAME2   "CompThresh2"
#define COMP_THRESH_ID3     "compThresh3"
#define COMP_THRESH_NAME3   "CompThresh3"
#define COMP_THRESH_ID4     "compThresh4"
#define COMP_THRESH_NAME4   "CompThresh4"

#define WIDTH_ID1           "width1"
#define WIDTH_NAME1         "Width1"
#define WIDTH_ID2           "width2"
#define WIDTH_NAME2         "Width2"
#define WIDTH_ID3           "width3"
#define WIDTH_NAME3         "Width3"
#define WIDTH_ID4           "width4"
#define WIDTH_NAME4         "Width4"

#define OUTPUT_ID1          "output1"
#define OUTPUT_NAME1        "Output1"
#define OUTPUT_ID2          "output2"
#define OUTPUT_NAME2        "Output2"
#define OUTPUT_ID3          "output3"
#define OUTPUT_NAME3        "Output3"
#define OUTPUT_ID4          "output4"
#define OUTPUT_NAME4        "Output4"
#define OUTPUT_ID           "output"
#define OUTPUT_NAME         "Output"

#define MIX_ID1             "mix1"
#define MIX_NAME1           "Mix1"
#define MIX_ID2             "mix2"
#define MIX_NAME2           "Mix2"
#define MIX_ID3             "mix3"
#define MIX_NAME3           "Mix3"
#define MIX_ID4             "mix4"
#define MIX_NAME4           "Mix4"
#define MIX_ID              "mix"
#define MIX_NAME            "Mix"

#define BIAS_ID1            "bias1"
#define BIAS_NAME1          "Bias1"
#define BIAS_ID2            "bias2"
#define BIAS_NAME2          "Bias2"
#define BIAS_ID3            "bias3"
#define BIAS_NAME3          "Bias3"
#define BIAS_ID4            "bias4"
#define BIAS_NAME4          "Bias4"

#define REC_ID1             "rec1"
#define REC_NAME1           "Rec1"
#define REC_ID2             "rec2"
#define REC_NAME2           "Rec2"
#define REC_ID3             "rec3"
#define REC_NAME3           "Rec3"
#define REC_ID4             "rec4"
#define REC_NAME4           "Rec4"

#define DOWNSAMPLE_ID       "downSample"
#define DOWNSAMPLE_NAME     "DownSample"

// filter
#define LOWCUT_FREQ_ID      "lowcutFreq"
#define LOWCUT_FREQ_NAME    "LowcutFreq"
#define HIGHCUT_FREQ_ID     "highcutFreq"
#define HIGHCUT_FREQ_NAME   "HighcutFreq"
#define PEAK_FREQ_ID        "peakFreq"
#define PEAK_FREQ_NAME      "PeakFreq"
#define LOWCUT_GAIN_ID      "lowCutGain"
#define LOWCUT_GAIN_NAME    "LowCutGain"
#define HIGHCUT_GAIN_ID     "highCutGain"
#define HIGHCUT_GAIN_NAME   "HighCutGain"

#define PEAK_GAIN_ID        "peakGain"
#define PEAK_GAIN_NAME      "PeakGain"

#define LOWCUT_Q_ID         "lowcutQ"
#define LOWCUT_Q_NAME       "LowcutQ"
#define PEAK_Q_ID           "peakQ"
#define PEAK_Q_NAME         "PeakQ"
#define HIGHCUT_Q_ID        "highcutQ"
#define HIGHCUT_Q_NAME      "HighcutQ"

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

#define OFF_ID              "off"
#define OFF_NAME            "Off"
#define PRE_ID              "pre"
#define PRE_NAME            "Pre"
#define POST_ID             "post"
#define POST_NAME           "Post"
#define LOW_ID              "low"
#define LOW_NAME            "Low"
#define BAND_ID             "band"
#define BAND_NAME           "Band"
#define HIGH_ID             "high"
#define HIGH_NAME           "High"

// multiband
#define FREQ_ID1            "freq1"
#define FREQ_NAME1          "Freq1"
#define FREQ_ID2            "freq2"
#define FREQ_NAME2          "Freq2"
#define FREQ_ID3            "freq3"
#define FREQ_NAME3          "Freq3"

#define BAND_ENABLE_ID1     "multibandEnable1"
#define BAND_ENABLE_NAME1   "MultibandEnable1"
#define BAND_ENABLE_ID2     "multibandEnable2"
#define BAND_ENABLE_NAME2   "MultibandEnable2"
#define BAND_ENABLE_ID3     "multibandEnable3"
#define BAND_ENABLE_NAME3   "MultibandEnable3"
#define BAND_ENABLE_ID4     "multibandEnable4"
#define BAND_ENABLE_NAME4   "MultibandEnable4"

#define BAND_SOLO_ID1       "multibandSolo1"
#define BAND_SOLO_NAME1     "MultibandSolo1"
#define BAND_SOLO_ID2       "multibandSolo2"
#define BAND_SOLO_NAME2     "MultibandSolo2"
#define BAND_SOLO_ID3       "multibandSolo3"
#define BAND_SOLO_NAME3     "MultibandSolo3"
#define BAND_SOLO_ID4       "multibandSolo4"
#define BAND_SOLO_NAME4     "MultibandSolo4"

#define BAND_FOCUS_ID1      "multibandFocus1"
#define BAND_FOCUS_NAME1    "MultibandFocus1"
#define BAND_FOCUS_ID2      "multibandFocus2"
#define BAND_FOCUS_NAME2    "MultibandFocus2"
#define BAND_FOCUS_ID3      "multibandFocus3"
#define BAND_FOCUS_NAME3    "MultibandFocus3"
#define BAND_FOCUS_ID4      "multibandFocus4"
#define BAND_FOCUS_NAME4    "MultibandFocus4"

#define LINE_STATE_ID1      "lineState1"
#define LINE_STATE_NAME1    "LineState1"
#define LINE_STATE_ID2      "lineState2"
#define LINE_STATE_NAME2    "LineState2"
#define LINE_STATE_ID3      "lineState3"
#define LINE_STATE_NAME3    "LineState3"

#define COMP_BYPASS_ID1         "compressorBypass1"
#define COMP_BYPASS_NAME1       "CompressorBypass1"
#define COMP_BYPASS_ID2         "compressorBypass2"
#define COMP_BYPASS_NAME2       "CompressorBypass2"
#define COMP_BYPASS_ID3         "compressorBypass3"
#define COMP_BYPASS_NAME3       "CompressorBypass3"
#define COMP_BYPASS_ID4         "compressorBypass4"
#define COMP_BYPASS_NAME4       "CompressorBypass4"
#define WIDTH_BYPASS_ID1        "widthBypass1"
#define WIDTH_BYPASS_NAME1      "WidthBypass1"
#define WIDTH_BYPASS_ID2        "widthBypass2"
#define WIDTH_BYPASS_NAME2      "WidthBypass2"
#define WIDTH_BYPASS_ID3        "widthBypass3"
#define WIDTH_BYPASS_NAME3      "WidthBypass3"
#define WIDTH_BYPASS_ID4        "widthBypass4"
#define WIDTH_BYPASS_NAME4      "WidthBypass4"
#define FILTER_BYPASS_ID        "filterBypass"
#define FILTER_BYPASS_NAME      "FilterBypass"
#define DOWNSAMPLE_BYPASS_ID    "downsampleBypass"
#define DOWNSAMPLE_BYPASS_NAME  "DownsampleBypass"


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
#define COLOUR_GROUP        juce::Colour(50, 50, 50)
#define DRIVE_COLOUR        juce::Colours::red
#define SHAPE_COLOUR        juce::Colours::yellow
#define WIDTH_COLOUR        juce::Colours::skyblue
#define COMP_COLOUR         juce::Colours::yellowgreen
#define FILTER_COLOUR       juce::Colours::hotpink
#define DOWNSAMPLE_COLOUR   juce::Colours::purple
    
// VU meters
#define SMOOTH_COEFF        0.2
