/*
  ==============================================================================

    VUPanel.cpp
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "VUPanel.h"
#include "../../../Utility/AudioHelpers.h"

//==============================================================================
VUPanel::VUPanel(FireAudioProcessor& p) : processor(p),
                                          focusBandNum(0),
                                          vuMeterIn(&p),
                                          vuMeterOut(&p),
                                          realtimeThresholdDb(-100.0f)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

    vuMeterIn.setParameters(true, 0);
    vuMeterOut.setParameters(false, 0);

    addAndMakeVisible(vuMeterIn);
    addAndMakeVisible(vuMeterOut);

    startTimerHz(60);
}

VUPanel::~VUPanel()
{
    stopTimer();
}

void VUPanel::paint(juce::Graphics& g)
{
    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds(), 1);

    // --- 1. 復現宏定義的佈局變量 ---
    // 這樣可以確保 paint() 中的計算與 resized() 中的佈局完全一致
    const float meterHeight = (float) getHeight() / 10.0f * 9.0f;
    const float meterY = (float) getHeight() / 10.0f;

    // --- 2. 建立與 VUMeter.cpp 標準完全一致的 dB 轉 Y 坐標系 ---
    auto dbToY = [&](float db)
    {
        // 該標準來源於 dBToNormalizedGain: (-96dB -> 0.0, 0dB -> 1.0)
        const float minDb = -96.0f;
        const float maxDb = 0.0f;

        // 將 dB 值限制在視覺範圍內
        db = juce::jlimit(minDb, maxDb, db);

        // 將 dB 值轉換為標準化的 [0, 1] 位置
        float normalizedPosition = (db - minDb) / (maxDb - minDb);

        // 將標準化位置映射到宏定義的 VU Meter 像素空間
        // Y 坐標是反的，所以用 1.0f 減去
        return meterY + meterHeight * (1.0f - normalizedPosition);
    };

    // --- 3. 繪製背景刻度 ---
    g.setColour(KNOB_SUBFONT_COLOUR);
    // 刻度文本的邊界現在被精確定義在兩個 VU 表之間
    auto textBounds = getLocalBounds().withX(vuMeterIn.getRight()).withRight(vuMeterOut.getX());
    float textHeight = 12.0f;
    g.setFont(textHeight);

    g.drawText("0", textBounds.withY(dbToY(0.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);
    g.drawText("-24", textBounds.withY(dbToY(-24.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);
    g.drawText("-48", textBounds.withY(dbToY(-48.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);
    g.drawText("-72", textBounds.withY(dbToY(-72.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);

    // --- 4. 繪製 Compressor Threshold 線 ---
    bool isGlobal = (focusBandNum == -1);
    if (! isGlobal && juce::isPositiveAndBelow(focusBandNum, 4))
    {
        compBypassID = ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum);
        bool compIsEnabled = *processor.treeState.getRawParameterValue(compBypassID);
        if (compIsEnabled)
        {
            float compressorLineY = dbToY(realtimeThresholdDb);
            g.setColour(juce::Colours::yellowgreen);
            g.drawLine((float) textBounds.getX(), compressorLineY, (float) textBounds.getRight(), compressorLineY, 1.5f);
        }
    }

    // 更新子組件的參數
    vuMeterIn.setParameters(true, focusBandNum);
    vuMeterOut.setParameters(false, focusBandNum);

    // --- 5. 繪製 RMS 和 Peak 讀數 ---
    auto toDB = [](float linear)
    { return juce::Decibels::gainToDecibels(linear, -96.0f); };

    float avgInputRmsDb = toDB(vuMeterIn.getLeftChannelLevel());
    float avgInputPeakDb = toDB(vuMeterIn.getLeftChannelPeakLevel());
    float avgOutputRmsDb = toDB(vuMeterOut.getLeftChannelLevel());
    float avgOutputPeakDb = toDB(vuMeterOut.getLeftChannelPeakLevel());

    auto leftArea = getLocalBounds().withRight(vuMeterIn.getX());
    auto rightArea = getLocalBounds().withLeft(vuMeterOut.getRight());

    g.setColour(juce::Colours::yellowgreen);
    auto fontSizeBig = 14.0f * getWidth() / 150.0f;
    auto fontSizeSmall = 10.0f * getWidth() / 150.0f;

    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeBig).withStyle("Bold") });
    g.drawText(juce::String(avgInputPeakDb, 1), leftArea.withTrimmedBottom(leftArea.getHeight() / 2), juce::Justification::centredBottom);
    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeSmall).withStyle("Plain") });
    g.drawText(juce::String(avgInputRmsDb, 1), leftArea.withTrimmedTop(leftArea.getHeight() / 2), juce::Justification::centredTop);

    g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
    g.setFont(fontSizeSmall);
    g.drawFittedText("In", leftArea.removeFromBottom(getHeight() / 4).toNearestInt(), juce::Justification::centredTop, 1);

    g.setColour(juce::Colours::yellowgreen);
    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeBig).withStyle("Bold") });
    g.drawText(juce::String(avgOutputPeakDb, 1), rightArea.withTrimmedBottom(rightArea.getHeight() / 2), juce::Justification::centredBottom);
    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeSmall).withStyle("Plain") });
    g.drawText(juce::String(avgOutputRmsDb, 1), rightArea.withTrimmedTop(rightArea.getHeight() / 2), juce::Justification::centredTop);

    g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
    g.setFont(fontSizeSmall);
    g.drawFittedText("Out", rightArea.removeFromBottom(getHeight() / 4).toNearestInt(), juce::Justification::centredTop, 1);
}

void VUPanel::resized()
{
    // --- 使用局部變量來清晰地實現宏定義的佈局 ---
    const float width = (float) getWidth();
    const float height = (float) getHeight();

    const float meterWidth = width / 10.0f;
    const float meterHeight = height / 10.0f * 9.0f;
    const float meterY = height / 10.0f;
    const float meterX1 = width / 3.0f - meterWidth / 2.0f;
    const float meterX2 = width / 3.0f * 2.0f - meterWidth / 2.0f;

    vuMeterIn.setBounds(juce::Rectangle<float>(meterX1, meterY, meterWidth, meterHeight).toNearestInt());
    vuMeterOut.setBounds(juce::Rectangle<float>(meterX2, meterY, meterWidth, meterHeight).toNearestInt());
}

void VUPanel::setFocusBandNum(int num)
{
    focusBandNum = num;
}

void VUPanel::timerCallback()
{
    repaint();
}

void VUPanel::updateRealtimeThreshold(float newThresholdDb)
{
    // Store the live value. No need to repaint here, as the timerCallback already does.
    realtimeThresholdDb = newThresholdDb;
}