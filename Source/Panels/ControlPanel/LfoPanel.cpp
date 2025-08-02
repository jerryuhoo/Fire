#include "LfoPanel.h"

//==============================================================================
// LfoEditor 实现
//==============================================================================

LfoEditor::LfoEditor()
{
    // 初始化时创建两个点和它们之间的一段直线
    points.push_back({ 0.0f, 0.5f });
    points.push_back({ 1.0f, 0.5f });
    curvatures.push_back(0.0f); // 初始弧度为0（直线）
}

LfoEditor::~LfoEditor() { }

void LfoEditor::paint(juce::Graphics& g)
{
    // (paint 函数与上一版完全相同，无需修改)
    // 1. 绘制背景和网格
    g.fillAll(juce::Colours::black.brighter(0.1f));
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1.0f);
    
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    for (int i = 1; i < gridDivisions; ++i)
    {
        float x = getWidth() * i / (float)gridDivisions;
        float y = getHeight() * i / (float)gridDivisions;
        g.drawVerticalLine(juce::roundToInt(x), 0.0f, getHeight());
        g.drawHorizontalLine(juce::roundToInt(y), 0.0f, getWidth());
    }

    // 2. 绘制LFO路径
    if (points.size() < 2) return;

    juce::Path lfoPath;
    lfoPath.startNewSubPath(fromNormalized(points.front()));

    for (size_t i = 0; i < points.size() - 1; ++i)
    {
        auto p1_screen = fromNormalized(points[i]);
        auto p2_screen = fromNormalized(points[i+1]);
        
        if (juce::approximatelyEqual(p1_screen.x, p2_screen.x) || juce::approximatelyEqual(curvatures[i], 0.0f))
        {
            lfoPath.lineTo(p2_screen);
        }
        else
        {
            const int numSegments = 30;
            
            for (int j = 1; j <= numSegments; ++j)
            {
                float tx = (float)j / (float)numSegments;
                const float absExp = std::pow(4.0f, std::abs(curvatures[i]));
                float ty;
                if (curvatures[i] >= 0.0f)
                    ty = std::pow(tx, absExp);
                else
                    ty = 1.0f - std::pow(1.0f - tx, absExp);

                float currentX = p1_screen.x + (p2_screen.x - p1_screen.x) * tx;
                float currentY = p1_screen.y + (p2_screen.y - p1_screen.y) * ty;
                lfoPath.lineTo({ currentX, currentY });
            }
        }
    }
    
    g.setColour(juce::Colours::orange);
    g.strokePath(lfoPath, juce::PathStrokeType(2.0f));

    // 3. 绘制控制点
    g.setColour(juce::Colours::yellow);
    for (const auto& point : points)
    {
        auto localPoint = fromNormalized(point);
        g.fillEllipse(localPoint.x - pointRadius, localPoint.y - pointRadius, pointRadius * 2, pointRadius * 2);
    }
}

void LfoEditor::mouseDown(const juce::MouseEvent& event)
{
    // (mouseDown 函数与上一版完全相同，无需修改)
    // 左键点击：拖动或添加点
    if (event.mods.isLeftButtonDown())
    {
        draggingPointIndex = -1;
        for (size_t i = 0; i < points.size(); ++i)
        {
            if (fromNormalized(points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
            {
                draggingPointIndex = static_cast<int>(i);
                return;
            }
        }
        
        if (points.size() < maxPoints)
        {
            addPoint(toNormalized(event.getPosition()));
            auto it = std::find(points.begin(), points.end(), toNormalized(event.getPosition()));
            if (it != points.end())
                draggingPointIndex = static_cast<int>(std::distance(points.begin(), it));
        }
    }
    // 右键点击：准备编辑曲线
    else if (event.mods.isRightButtonDown())
    {
        editingCurveIndex = -1;
        for (int i = (int)points.size() - 2; i >= 0; --i)
        {
            auto p1 = fromNormalized(points[i]);
            auto p2 = fromNormalized(points[i + 1]);

            auto segmentBounds = juce::Rectangle<float>(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p1.x - p2.x), std::abs(p1.y - p2.y));
            segmentBounds = segmentBounds.expanded(0, 15.0f);

            if (segmentBounds.contains(event.position.toFloat()))
            {
                editingCurveIndex = i;
                break;
            }
        }
    }
}

void LfoEditor::mouseDrag(const juce::MouseEvent& event)
{
    // (*** 已修改 ***) 左键拖拽逻辑
    if (event.mods.isLeftButtonDown() && draggingPointIndex != -1)
    {
        auto currentPos = event.getPosition();
        
        // 网格吸附逻辑
        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            float snappedX = std::round(currentPos.x * gridDivisions / getWidth()) * (getWidth() / (float)gridDivisions);
            float snappedY = std::round(currentPos.y * gridDivisions / getHeight()) * (getHeight() / (float)gridDivisions);
            currentPos = { (int)snappedX, (int)snappedY };
        }
        
        // (*** 新增 ***) 拖拽约束逻辑
        float minX = 0.0f;
        float maxX = (float)getWidth();

        // 如果拖动的不是第一个点，则其最小X由左邻居决定
        if (draggingPointIndex > 0)
            minX = fromNormalized(points[draggingPointIndex - 1]).x;

        // 如果拖动的不是最后一个点，则其最大X由右邻居决定
        if (draggingPointIndex < points.size() - 1)
            maxX = fromNormalized(points[draggingPointIndex + 1]).x;
        
        // 应用X轴约束
        currentPos.setX(juce::jlimit(minX, maxX, (float)currentPos.x));

        // 应用组件边界的Y轴约束
        auto constrainedPos = getLocalBounds().getConstrainedPoint(currentPos);
        
        // 更新点的位置
        points[draggingPointIndex] = toNormalized(constrainedPos);
        
        // 因为X坐标被约束，点的顺序不会改变，所以不再需要排序
        repaint();
    }
    // 右键拖拽改变弧度
    else if (event.mods.isRightButtonDown() && editingCurveIndex != -1)
    {
        // 拿到这一段的两个端点（屏幕坐标）
        auto p1_screen = fromNormalized(points[editingCurveIndex]);
        auto p2_screen = fromNormalized(points[editingCurveIndex + 1]);

        // 计算斜率
        float dx = p2_screen.x - p1_screen.x;
        float dy = p2_screen.y - p1_screen.y;
        float slope = (dx != 0.0f) ? (dy / dx) : std::numeric_limits<float>::infinity();

        // 拖动距离：向下为正，向上为负
        float dragDistY = event.getDistanceFromDragStartY();

        // sensitivity 越大形变越灵敏
        const float divisor    = 100.0f;
        const float sensitivity = 1.0f;

        // 根据斜率决定正负： slope<0 时反向，否则正常
        float rawCurv = (slope < 0.0f
                         ?  dragDistY / divisor
                         : -dragDistY / divisor)
                        * sensitivity;

        // 限制在 [-1,1]
        curvatures[editingCurveIndex] = juce::jlimit(-2.0f, 2.0f, rawCurv);

        repaint();
    }
}

// ... (mouseUp, mouseDoubleClick, addPoint, removePoint 等函数与上一版完全相同，无需修改) ...

void LfoEditor::mouseUp(const juce::MouseEvent& event)
{
    if (!points.empty())
    {
        points.front().x = 0.0f;
        points.back().x = 1.0f;
    }
    draggingPointIndex = -1;
    editingCurveIndex = -1;
    repaint();
}

void LfoEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    // 右键双击重置曲率
    if (event.mods.isRightButtonDown())
    {
        int curveToReset = -1;
        for (int i = (int)points.size() - 2; i >= 0; --i)
        {
            auto p1 = fromNormalized(points[i]);
            auto p2 = fromNormalized(points[i + 1]);
            auto segmentBounds = juce::Rectangle<float>(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p1.x - p2.x), std::abs(p1.y - p2.y));
            if (segmentBounds.expanded(0, 15.0f).contains(event.position.toFloat()))
            {
                curveToReset = i;
                break;
            }
        }
        
        if (curveToReset != -1)
        {
            curvatures[curveToReset] = 0.0f;
            repaint();
        }
        return;
    }
    
    // 左键双击删除点
    if (points.size() <= 2) return;
    for (size_t i = points.size() - 1; i > 0; --i)
    {
        if (fromNormalized(points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
        {
            removePoint(static_cast<int>(i));
            return;
        }
    }
}

void LfoEditor::addPoint(juce::Point<float> newPoint)
{
    if (points.size() >= maxPoints) return;
    
    points.push_back(newPoint);
    updateAndSortPoints();
    
    auto it = std::find(points.begin(), points.end(), newPoint);
    int insertIndex = static_cast<int>(std::distance(points.begin(), it));
    
    if (insertIndex > 0)
    {
        curvatures[insertIndex - 1] = 0.0f;
        curvatures.insert(curvatures.begin() + insertIndex, 0.0f);
    }
    repaint();
}

void LfoEditor::removePoint(int index)
{
    if (index <= 0 || index >= points.size() - 1) return;
    
    points.erase(points.begin() + index);
    
    curvatures.erase(curvatures.begin() + index - 1);
    if (index - 1 < curvatures.size())
        curvatures[index - 1] = 0.0f;

    repaint();
}

juce::Point<float> LfoEditor::toNormalized(juce::Point<int> localPoint)
{
    return { (float)localPoint.x / (float)getWidth(), (float)localPoint.y / (float)getHeight() };
}

juce::Point<float> LfoEditor::fromNormalized(juce::Point<float> normalizedPoint)
{
    return { normalizedPoint.x * (float)getWidth(), normalizedPoint.y * (float)getHeight() };
}

void LfoEditor::updateAndSortPoints()
{
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
        return a.x < b.x;
    });
}


//==============================================================================
// LfoPanel 实现 (保持不变)
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

    // rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "LFO_RATE", rateSlider);
}

LfoPanel::~LfoPanel() {}

void LfoPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawText("LFO 1", getLocalBounds().withTrimmedTop(10), juce::Justification::centredTop, true);
    g.setColour(juce::Colour(0xff303030));
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
