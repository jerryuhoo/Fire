#include "LfoPanel.h"

//==============================================================================
// LfoEditor 实现
//==============================================================================

LfoEditor::LfoEditor()
{
    // 初始化时创建两个默认点，构成一条直线
    points.push_back({ 0.0f, 0.5f });
    points.push_back({ 1.0f, 0.5f });
}

LfoEditor::~LfoEditor()
{
}

void LfoEditor::paint(juce::Graphics& g)
{
    // 1. 绘制背景
    g.fillAll(juce::Colours::black.brighter(0.1f));
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1.0f);
    
    // 2. 绘制网格线 (可选)
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    for (int i = 1; i < 4; ++i)
    {
        float x = getWidth() * i / 4.0f;
        float y = getHeight() * i / 4.0f;
        g.drawVerticalLine(juce::roundToInt(x), 0.0f, getHeight());
        g.drawHorizontalLine(juce::roundToInt(y), 0.0f, getWidth());
    }

    // 3. 绘制LFO路径
    if (points.empty()) return;

    juce::Path lfoPath;
    lfoPath.startNewSubPath(fromNormalized(points.front()));

    for (size_t i = 1; i < points.size(); ++i)
    {
        lfoPath.lineTo(fromNormalized(points[i]));
    }
    
    // 描绘路径
    g.setColour(juce::Colours::orange);
    g.strokePath(lfoPath, juce::PathStrokeType(2.0f));

    // 4. 绘制控制点
    g.setColour(juce::Colours::yellow);
    for (const auto& point : points)
    {
        auto localPoint = fromNormalized(point);
        g.fillEllipse(localPoint.x - pointRadius, localPoint.y - pointRadius, pointRadius * 2, pointRadius * 2);
    }
}

void LfoEditor::mouseDown(const juce::MouseEvent& event)
{
    draggingPointIndex = -1;
    for (int i = 0; i < points.size(); ++i)
    {
        if (fromNormalized(points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
        {
            draggingPointIndex = i;
            return;
        }
    }
    
    if (points.size() < maxPoints)
    {
        auto newPointNormalized = toNormalized(event.getPosition());
        points.push_back(newPointNormalized);
        updateAndSortPoints();

        auto it = std::find_if(points.begin(), points.end(),
            [&](const juce::Point<float>& p) {
                return juce::approximatelyEqual(p.x, newPointNormalized.x) && juce::approximatelyEqual(p.y, newPointNormalized.y);
            });
            
        if (it != points.end()) {
            // (*** 已修正 ***) 使用 static_cast 解决 'long' to 'int' 的警告
            draggingPointIndex = static_cast<int>(std::distance(points.begin(), it));
        }
        repaint();
    }
}

void LfoEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingPointIndex != -1)
    {
        // (*** 已修正 ***) 使用 getLocalBounds().constrainPoint() 代替 getConstrainedPosition()
        auto constrainedPos = getLocalBounds().getConstrainedPoint(event.getPosition());
        
        // 更新点的位置
        points[draggingPointIndex] = toNormalized(constrainedPos);
        
        // 重新排序，以防拖拽时点的x坐标超过了邻居
        updateAndSortPoints();

        // 拖拽过程中，被拖拽点的索引可能会因为排序而改变，需要重新定位它
        auto currentNormalizedPos = toNormalized(constrainedPos);
        auto it = std::find_if(points.begin(), points.end(),
            [&](const juce::Point<float>& p){
                // (*** 已修正 ***) 使用 juce::approximatelyEqual 进行浮点数比较
                return juce::approximatelyEqual(p.x, currentNormalizedPos.x) && juce::approximatelyEqual(p.y, currentNormalizedPos.y);
            });

        if (it != points.end()) {
            // (*** 已修正 ***) 使用 static_cast 解决 'long' to 'int' 的警告
            draggingPointIndex = static_cast<int>(std::distance(points.begin(), it));
        } else {
            draggingPointIndex = -1;
        }

        repaint();
    }
}

void LfoEditor::mouseUp(const juce::MouseEvent& event)
{
    // 确保首尾点固定在左右边缘
    if (!points.empty())
    {
        points.front().x = 0.0f;
        points.back().x = 1.0f;
    }
    draggingPointIndex = -1;
    repaint();
}

void LfoEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    // (*** 已修正 ***) 将循环变量 'i' 的类型从 int 改为 size_t (或 auto) 来解决警告
    // 从后往前遍历，这样删除时不会影响后续元素的索引
    // 我们只检查非首尾的点，所以循环范围是从 size() - 2 到 1
    if (points.size() <= 2) return; // 如果只有2个或更少的点，则不执行任何操作
    // 从后往前遍历，这样删除时不会影响后续元素的索引
    for (auto i = points.size() - 2; i >= 1; --i)
    {
        if (fromNormalized(points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
        {
            points.erase(points.begin() + i);
            repaint();
            return; // 找到并删除一个点后就退出
        }
    }
}

juce::Point<float> LfoEditor::toNormalized(juce::Point<int> localPoint)
{
    // 使用getWidth()和getHeight()来确保即使组件大小变化也能正确转换
    return { (float)localPoint.x / (float)getWidth(), (float)localPoint.y / (float)getHeight() };
}

juce::Point<float> LfoEditor::fromNormalized(juce::Point<float> normalizedPoint)
{
    return { normalizedPoint.x * (float)getWidth(), normalizedPoint.y * (float)getHeight() };
}

void LfoEditor::updateAndSortPoints()
{
    // 根据x坐标对点进行排序，防止线交叉
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
        return a.x < b.x;
    });
}

//==============================================================================
// LfoPanel 实现
//==============================================================================

LfoPanel::LfoPanel(FireAudioProcessor& p) : processor(p)
{
    addAndMakeVisible(lfoEditor);
    
    addAndMakeVisible(rateSlider);
    rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    
    addAndMakeVisible(rateLabel);
    rateLabel.setText("Rate", juce::dontSendNotification);
    rateLabel.attachToComponent(&rateSlider, false);
    rateLabel.setJustificationType(juce::Justification::centred);

    // (*** 已修正 ***) 使用你代码中的变量名 processor.treeState
//    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "LFO_RATE", rateSlider);
}

LfoPanel::~LfoPanel()
{
}

void LfoPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawText("LFO 1", getLocalBounds().withTrimmedTop(10), juce::Justification::centredTop, true);
    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds());
}

void LfoPanel::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(10, 10);
    
    bounds.removeFromTop(20);

    auto controlsBounds = bounds.removeFromRight(100);
    
    lfoEditor.setBounds(bounds);
    
    controlsBounds.removeFromTop(40);
    rateSlider.setBounds(controlsBounds.removeFromTop(80));
}
