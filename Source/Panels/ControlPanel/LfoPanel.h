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
    一个可交互的LFO波形编辑器组件 (已升级)
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
    const std::vector<float>& getCurvatures() const { return curvatures; }

private:
    // 将组件内的坐标转换为归一化的[0, 1]坐标
    juce::Point<float> toNormalized(juce::Point<int> localPoint);
    // 将归一化的[0, 1]坐标转换为组件内的浮点坐标
    juce::Point<float> fromNormalized(juce::Point<float> normalizedPoint);
    
    // 更新并排序点，确保X轴坐标递增
    void updateAndSortPoints();
    
    // (*** 新增 ***) 在添加或删除点时，同步更新弧度数组
    void addPoint(juce::Point<float> newPoint);
    void removePoint(int index);

    std::vector<juce::Point<float>> points; // 使用归一化的[0, 1]坐标存储点
    std::vector<float> curvatures;          // (*** 新增 ***) 存储每条线段的弧度, 值域[-1, 1]

    int draggingPointIndex = -1;            // 被拖动的点的索引, -1表示没有
    int editingCurveIndex = -1;             // (*** 新增 ***) 被右键编辑的曲线的索引, -1表示没有

    const int maxPoints = 16;
    const float pointRadius = 6.0f;         // 点的可视半径
    const int gridDivisions = 4;            // (*** 新增 ***) 网格的分割数
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
    FireAudioProcessor& processor;
    
    LfoEditor lfoEditor;

    juce::Slider rateSlider;
    juce::Label  rateLabel;
    
    // std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPanel)
};