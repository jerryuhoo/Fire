#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <Panels/SpectrogramPanel/Multiband.h>
#include <Utility/Parameters.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>

struct MultibandTopologyAnimationTestAccess
{
    static int lineCount(const Multiband& multiband) noexcept
    {
        return multiband.lineNum;
    }

    static FreqDividerGroup& divider(Multiband& multiband, int index)
    {
        return *multiband.freqDividerGroup[static_cast<size_t>(index)];
    }

    static size_t retiringDividerCount(const Multiband& multiband) noexcept
    {
        return multiband.retiringDividerVisuals.size();
    }

    static float retiringDividerOpacity(const Multiband& multiband,
                                         size_t index)
    {
        return multiband.retiringDividerVisuals[index].opacity.current;
    }

    static float retiringDividerX(const Multiband& multiband, size_t index)
    {
        return multiband.retiringDividerVisuals[index].xPercent;
    }

    static void deleteBand(Multiband& multiband, int bandIndex)
    {
        multiband.buttonClicked(
            multiband.bandUIs[static_cast<size_t>(bandIndex)]
                .closeButton.get());
    }
};

namespace
{
void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void initialiseBandLayout(FireAudioProcessor& processor,
                          int numBands,
                          const std::array<float, 3>& frequencies = {
                              1000.0f, 3000.0f, 7000.0f })
{
    setPlainParameter(processor, NUM_BANDS_ID,
                      static_cast<float>(numBands));
    for (int divider = 0; divider < 3; ++divider)
    {
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, divider),
                          frequencies[static_cast<size_t>(divider)]);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(LINE_STATE_ID,
                                                          divider),
                          divider < numBands - 1 ? 1.0f : 0.0f);
    }
}

template<typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents();
         ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (auto* match = dynamic_cast<ComponentType*>(child))
            return match;

        if (child != nullptr)
            if (auto* nested = findDescendant<ComponentType>(*child))
                return nested;
    }
    return nullptr;
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers = {})
{
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             modifiers,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             time,
             position,
             time,
             1,
             false };
}

void clickToAddDivider(Multiband& multiband, float xPercent)
{
    const auto position = juce::Point<float> {
        static_cast<float>(multiband.getWidth()) * xPercent,
        static_cast<float>(multiband.getHeight()) * 0.10f
    };
    static_cast<juce::Component&>(multiband).mouseDown(
        makeMouseEvent(multiband,
                       position,
                       juce::ModifierKeys::leftButtonModifier));
    static_cast<juce::Component&>(multiband).mouseUp(
        makeMouseEvent(multiband, position));
}

int getPublishedBandCount(const FireAudioProcessor& processor)
{
    const auto* value = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    REQUIRE(value != nullptr);
    return juce::roundToInt(value->load(std::memory_order_relaxed));
}

class DeleteEditorWhenComponentHides final : public juce::ComponentListener
{
public:
    explicit DeleteEditorWhenComponentHides(
        std::unique_ptr<FireAudioProcessorEditor>& editorToDelete)
        : editor(editorToDelete)
    {
    }

    void componentVisibilityChanged(juce::Component& component) override
    {
        if (! component.isVisible() && editor != nullptr)
        {
            didDelete = true;
            editor.reset();
        }
    }

    std::unique_ptr<FireAudioProcessorEditor>& editor;
    bool didDelete = false;
};
} // namespace

TEST_CASE("Divider add and delete visuals reverse without delaying topology",
          "[multiband][ui][topology][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 1);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(multiband->isShowing());

    constexpr float insertionX = 0.35f;
    clickToAddDivider(*multiband, insertionX);

    // The processor and hit-test topology are authoritative immediately; only
    // the child opacity is interpolated by the shared UI clock.
    REQUIRE(getPublishedBandCount(processor) == 2);
    REQUIRE(MultibandTopologyAnimationTestAccess::lineCount(*multiband) == 1);
    auto& divider = MultibandTopologyAnimationTestAccess::divider(*multiband,
                                                                  0);
    REQUIRE(divider.isVisible());
    const auto dividerX = divider.getVerticalLine().getXPercent();
    CHECK(divider.getAlpha() == Catch::Approx(0.0f).margin(0.001f));
    CHECK(MultibandTopologyAnimationTestAccess::retiringDividerCount(
              *multiband)
          == 0);

    multiband->animationTick(1.0f / 60.0f);
    const auto partiallyRevealed = divider.getAlpha();
    REQUIRE(partiallyRevealed > 0.0f);
    REQUIRE(partiallyRevealed < 1.0f);

    MultibandTopologyAnimationTestAccess::deleteBand(*multiband, 0);
    REQUIRE(getPublishedBandCount(processor) == 1);
    REQUIRE(MultibandTopologyAnimationTestAccess::lineCount(*multiband) == 0);
    CHECK_FALSE(divider.isVisible());
    REQUIRE(MultibandTopologyAnimationTestAccess::retiringDividerCount(
                *multiband)
            == 1);
    CHECK(MultibandTopologyAnimationTestAccess::retiringDividerX(
              *multiband, 0)
          == Catch::Approx(dividerX).margin(0.0001f));
    CHECK(MultibandTopologyAnimationTestAccess::retiringDividerOpacity(
              *multiband, 0)
          == Catch::Approx(partiallyRevealed).margin(0.005f));

    for (int frame = 0; frame < 4; ++frame)
        multiband->animationTick(1.0f / 60.0f);
    const auto partiallyRetired =
        MultibandTopologyAnimationTestAccess::retiringDividerOpacity(
            *multiband, 0);
    REQUIRE(partiallyRetired > 0.0f);
    REQUIRE(partiallyRetired < partiallyRevealed);

    // Re-adding at the same frequency consumes the retiring copy and resumes
    // from its current opacity rather than flashing or leaving two rails.
    clickToAddDivider(*multiband, insertionX);
    REQUIRE(getPublishedBandCount(processor) == 2);
    REQUIRE(MultibandTopologyAnimationTestAccess::lineCount(*multiband) == 1);
    CHECK(MultibandTopologyAnimationTestAccess::retiringDividerCount(
              *multiband)
          == 0);
    CHECK(divider.getAlpha()
          == Catch::Approx(partiallyRetired).margin(0.005f));

    for (int frame = 0; frame < 120; ++frame)
        multiband->animationTick(1.0f / 60.0f);
    CHECK(divider.getAlpha() == Catch::Approx(1.0f).margin(0.001f));

    MultibandTopologyAnimationTestAccess::deleteBand(*multiband, 0);
    REQUIRE(MultibandTopologyAnimationTestAccess::retiringDividerCount(
                *multiband)
            == 1);
    CHECK(MultibandTopologyAnimationTestAccess::retiringDividerOpacity(
              *multiband, 0)
          == Catch::Approx(1.0f));
    multiband->dismissTransientUi();
    CHECK(MultibandTopologyAnimationTestAccess::retiringDividerCount(
              *multiband)
          == 0);
    CHECK(divider.getAlpha() == Catch::Approx(1.0f));
}

TEST_CASE("Rapid divider removals own independent value-only fade copies",
          "[multiband][ui][topology][animation][rapid][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 4);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(multiband->isShowing());

    MultibandTopologyAnimationTestAccess::deleteBand(*multiband, 1);
    REQUIRE(getPublishedBandCount(processor) == 3);
    MultibandTopologyAnimationTestAccess::deleteBand(*multiband, 0);
    REQUIRE(getPublishedBandCount(processor) == 2);
    REQUIRE(MultibandTopologyAnimationTestAccess::lineCount(*multiband) == 1);
    REQUIRE(MultibandTopologyAnimationTestAccess::retiringDividerCount(
                *multiband)
            == 2);

    const auto firstRetiringX =
        MultibandTopologyAnimationTestAccess::retiringDividerX(*multiband, 0);
    const auto secondRetiringX =
        MultibandTopologyAnimationTestAccess::retiringDividerX(*multiband, 1);
    CHECK(std::abs(firstRetiringX - secondRetiringX) > 0.01f);

    for (int frame = 0; frame < 120; ++frame)
        multiband->animationTick(1.0f / 60.0f);
    CHECK(MultibandTopologyAnimationTestAccess::retiringDividerCount(
              *multiband)
          == 0);
    CHECK(getPublishedBandCount(processor) == 2);
    CHECK(MultibandTopologyAnimationTestAccess::lineCount(*multiband) == 1);
}

TEST_CASE("The shared divider animation clock survives synchronous UI teardown",
          "[multiband][ui][animation][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    auto* frequencyLabel = findDescendant<FreqTextLabel>(*multiband);
    REQUIRE(frequencyLabel != nullptr);

    frequencyLabel->setFade(true, true);
    for (int frame = 0; frame < 120; ++frame)
        multiband->animationTick(1.0f / 60.0f);
    REQUIRE(frequencyLabel->isVisible());

    DeleteEditorWhenComponentHides deleteOnHide(editor);
    frequencyLabel->addComponentListener(&deleteOnHide);
    frequencyLabel->setFade(true, false);
    auto* const rawMultiband = multiband;
    for (int frame = 0; frame < 120 && editor != nullptr; ++frame)
        rawMultiband->animationTick(1.0f / 60.0f);

    CHECK(deleteOnHide.didDelete);
    CHECK(editor == nullptr);
}
