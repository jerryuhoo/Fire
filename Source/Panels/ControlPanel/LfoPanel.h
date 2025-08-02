/*
  ==============================================================================

    LfoPanel.h
    Created: 2 Aug 2025 9:24:01pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../PluginProcessor.h"

//==============================================================================
/*
    一个可交互的LFO波形编辑器组件
*/
class LfoEditor : public juce::Component
{
public:
    LfoEditor();
    ~LfoEditor() override;

    void paint(juce::Graphics& g) override;

    // --- 鼠标交互 ---
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    
    // --- 获取LFO点数据，用于DSP处理 ---
    const std::vector<juce::Point<float>>& getPoints() const { return points; }

private:
    // 将组件内的坐标转换为归一化的[0, 1]坐标
    juce::Point<float> toNormalized(juce::Point<int> localPoint);
    
    // (*** 已修正 ***) 将归一化的[0, 1]坐标转换为组件内的浮点坐标
    juce::Point<float> fromNormalized(juce::Point<float> normalizedPoint);
    
    // 更新并排序点，确保X轴坐标递增
    void updateAndSortPoints();

    std::vector<juce::Point<float>> points; // 使用归一化的[0, 1]坐标存储点
    int draggingPointIndex = -1; // -1表示没有点被拖动
    const int maxPoints = 16;
    const float pointRadius = 6.0f; // 点的可视半径
};


//==============================================================================
/*
    包含LFO编辑器和相关控制旋钮的主面板
*/
class LfoPanel : public juce::Component
{
public:
    LfoPanel(FireAudioProcessor& p); // 传入Processor引用
    ~LfoPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // 获取LfoEditor的引用，以便Processor可以获取点数据
    LfoEditor& getLfoEditor() { return lfoEditor; }

private:
    // JUCE会自动处理这个处理器的引用，无需手动删除
    FireAudioProcessor& processor;
    
    LfoEditor lfoEditor;

    juce::Slider rateSlider;
    juce::Label  rateLabel;
    
    // 使用Attachment可以自动将Slider和Processor中的参数同步
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPanel)
};
