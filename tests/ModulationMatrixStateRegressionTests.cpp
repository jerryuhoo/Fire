#include "../Source/Panels/ControlPanel/ModulationMatrixPanel.h"
#include "../Source/Panels/TopPanel/Preset.h"
#include "../Source/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

struct ModulationMatrixRoutingComboBoxTestAccess
{
    static std::function<void(int)> createPopupResultHandler(
        ModulationMatrixRoutingComboBox& comboBox)
    {
        return comboBox.createPopupResultHandler();
    }

    static bool hasActivePointerInteraction(
        const ModulationMatrixRoutingComboBox& comboBox)
    {
        return comboBox.pointerInteractionActive;
    }

    static std::uint64_t getPopupSessionRevision(
        const ModulationMatrixRoutingComboBox& comboBox)
    {
        return comboBox.popupSessionRevision;
    }

    static void setLifecycleReentrancyHook(
        ModulationMatrixRoutingComboBox& comboBox,
        std::function<void()> hook)
    {
        comboBox.lifecycleReentrancyHookForTesting = std::move(hook);
    }

    static void dispatchParentHierarchyChanged(
        ModulationMatrixRoutingComboBox& comboBox)
    {
        comboBox.parentHierarchyChanged();
    }
};

struct ModulationMatrixRowTestAccess
{
    static void setAmountPointerSource(
        ModulationMatrixRow& row,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        row.amountSlider.pointerSourceType = type;
        row.amountSlider.pointerSourceIndex = index;
    }

    static bool hasActiveAmountGesture(
        const ModulationMatrixRow& row) noexcept
    {
        return row.amountSlider.primaryGestureInProgress;
    }

    static bool isAmountPointerDispatchInProgress(
        const ModulationMatrixRow& row) noexcept
    {
        return row.amountSlider.pointerDispatchInProgress;
    }

    static void setAmountAnimationTargets(ModulationMatrixRow& row,
                                          float hover,
                                          float press,
                                          float focus,
                                          float disabled) noexcept
    {
        row.amountSlider.hoverAnimation.setTarget(hover);
        row.amountSlider.pressAnimation.setTarget(press);
        row.amountSlider.focusAnimation.setTarget(focus);
        row.amountSlider.disabledAnimation.setTarget(disabled);
    }

    static bool advanceAmountAnimation(ModulationMatrixRow& row,
                                       float deltaSeconds) noexcept
    {
        return row.amountSlider.advanceAnimation(deltaSeconds);
    }

    static void updateAmountAnimationTargets(ModulationMatrixRow& row) noexcept
    {
        row.amountSlider.updateAnimationTargets();
    }

    static const ModulationMatrixRoutingComboBox& getSourceMenu(
        const ModulationMatrixRow& row) noexcept
    {
        return row.sourceMenu;
    }

    static const juce::Slider& getAmountSlider(
        const ModulationMatrixRow& row) noexcept
    {
        return row.amountSlider;
    }

    static juce::Slider& getAmountSlider(ModulationMatrixRow& row) noexcept
    {
        return row.amountSlider;
    }

    static const juce::TextButton& getBipolarButton(
        const ModulationMatrixRow& row) noexcept
    {
        return row.bipolarButton;
    }

    static const ModulationMatrixRoutingComboBox& getDestinationMenu(
        const ModulationMatrixRow& row) noexcept
    {
        return row.destinationMenu;
    }

    static const juce::TextButton& getBypassButton(
        const ModulationMatrixRow& row) noexcept
    {
        return row.bypassButton;
    }

    static const juce::TextButton& getRemoveButton(
        const ModulationMatrixRow& row) noexcept
    {
        return row.removeButton;
    }
};

namespace
{
void replacePresetRoutings(juce::XmlElement& preset,
                           std::initializer_list<ModulationRouting> routings)
{
    auto* routingState = preset.getChildByName("MODULATION_STATE");
    if (routingState == nullptr)
        routingState = preset.createNewChildElement("MODULATION_STATE");

    routingState->deleteAllChildElements();
    for (const auto& routing : routings)
    {
        auto* routingXml = routingState->createNewChildElement("ROUTING");
        routing.writeToXml(*routingXml);
    }
}

void collectMatrixRows(juce::Component& component,
                       std::vector<ModulationMatrixRow*>& rows)
{
    if (auto* row = dynamic_cast<ModulationMatrixRow*>(&component))
        rows.push_back(row);

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            collectMatrixRows(*child, rows);
}

juce::Viewport* findViewport(juce::Component& component)
{
    if (auto* viewport = dynamic_cast<juce::Viewport*>(&component))
        return viewport;

    for (int childIndex = 0; childIndex < component.getNumChildComponents();
         ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            if (auto* viewport = findViewport(*child))
                return viewport;

    return nullptr;
}

std::shared_ptr<ModulationRoutingEditSession> makeRoutingEditSession(
    FireAudioProcessor& processor)
{
    auto session = std::make_shared<ModulationRoutingEditSession>();
    session->revision =
        processor.getLfoManager().getModulationRoutingRevision();
    return session;
}

juce::Slider* findAmountSlider(juce::Component& component)
{
    if (auto* slider = dynamic_cast<juce::Slider*>(&component))
        return slider;

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            if (auto* slider = findAmountSlider(*child))
                return slider;

    return nullptr;
}

juce::Image renderSlider(juce::Slider& slider)
{
    juce::Image image(juce::Image::ARGB,
                      juce::jmax(1, slider.getWidth()),
                      juce::jmax(1, slider.getHeight()),
                      true);
    juce::Graphics graphics(image);
    slider.paintEntireComponent(graphics, true);
    return image;
}

std::uint64_t imageFingerprint(const juce::Image& image)
{
    std::uint64_t fingerprint = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            fingerprint ^= image.getPixelAt(x, y).getARGB();
            fingerprint *= 1099511628211ull;
        }

    return fingerprint;
}

int countPixelsNearColour(const juce::Image& image,
                          juce::Colour expected)
{
    const auto channelDistance = [](juce::uint8 first,
                                    juce::uint8 second)
    {
        return first >= second ? first - second : second - first;
    };

    int matchingPixels = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto pixel = image.getPixelAt(x, y);
            const auto distance =
                channelDistance(pixel.getRed(), expected.getRed())
                + channelDistance(pixel.getGreen(), expected.getGreen())
                + channelDistance(pixel.getBlue(), expected.getBlue());
            if (pixel.getAlpha() >= 96 && distance <= 36)
                ++matchingPixels;
        }

    return matchingPixels;
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers,
                                juce::Point<float> mouseDownPosition,
                                bool mouseWasDragged)
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
             mouseDownPosition,
             time,
             1,
             mouseWasDragged };
}

bool containsComboBoxText(juce::Component& component,
                          const juce::String& expectedText)
{
    if (auto* menu = dynamic_cast<juce::ComboBox*>(&component);
        menu != nullptr && menu->getText() == expectedText)
        return true;

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            if (containsComboBoxText(*child, expectedText))
                return true;

    return false;
}

juce::TextButton* findTextButton(juce::Component& component,
                                 const juce::String& buttonText)
{
    if (auto* button = dynamic_cast<juce::TextButton*>(&component);
        button != nullptr && button->getButtonText() == buttonText)
        return button;

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            if (auto* button = findTextButton(*child, buttonText))
                return button;

    return nullptr;
}

class NonParameterChangeCapture final : public juce::AudioProcessorListener
{
public:
    explicit NonParameterChangeCapture(FireAudioProcessor& processorToObserve)
        : processor(processorToObserve)
    {
        processor.addListener(this);
    }

    ~NonParameterChangeCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (details.nonParameterStateChanged)
            ++notificationCount;
    }

    FireAudioProcessor& processor;
    int notificationCount = 0;
};

class DeleteMatrixPanelOnHostNotification final
    : public juce::AudioProcessorListener
{
public:
    DeleteMatrixPanelOnHostNotification(
        FireAudioProcessor& processorToObserve,
        std::unique_ptr<ModulationMatrixPanel>& panelToDelete,
        const ModulationMatrixRow* amountRowToObserve = nullptr)
        : processor(processorToObserve),
          panel(panelToDelete),
          amountRow(amountRowToObserve)
    {
        processor.addListener(this);
    }

    ~DeleteMatrixPanelOnHostNotification() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (! details.nonParameterStateChanged || notificationCount != 0)
            return;

        ++notificationCount;
        if (amountRow != nullptr)
            notificationArrivedAfterPointerDispatch =
                ! ModulationMatrixRowTestAccess::
                    isAmountPointerDispatchInProgress(*amountRow);
        panel.reset();
        callbackCompleted = true;
    }

    FireAudioProcessor& processor;
    std::unique_ptr<ModulationMatrixPanel>& panel;
    const ModulationMatrixRow* amountRow = nullptr;
    int notificationCount = 0;
    bool callbackCompleted = false;
    bool notificationArrivedAfterPointerDispatch = false;
};

class EditSliderOnHostNotification final
    : public juce::AudioProcessorListener
{
public:
    EditSliderOnHostNotification(
        FireAudioProcessor& processorToObserve,
        juce::Slider& sliderToEdit)
        : processor(processorToObserve), slider(sliderToEdit)
    {
        processor.addListener(this);
    }

    ~EditSliderOnHostNotification() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (! details.nonParameterStateChanged)
            return;

        ++notificationCount;
        if (attemptedEdit)
            return;

        attemptedEdit = true;
        slider.setValue(0.91, juce::sendNotificationSync);
        callbackCompleted = true;
    }

    FireAudioProcessor& processor;
    juce::Slider& slider;
    int notificationCount = 0;
    bool attemptedEdit = false;
    bool callbackCompleted = false;
};

void collectComboBoxes(juce::Component& component,
                       std::vector<juce::ComboBox*>& comboBoxes)
{
    if (auto* comboBox = dynamic_cast<juce::ComboBox*>(&component))
        comboBoxes.push_back(comboBox);

    for (int childIndex = 0;
         childIndex < component.getNumChildComponents();
         ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            collectComboBoxes(*child, comboBoxes);
}

ModulationMatrixRoutingComboBox* findRoutingComboBox(
    juce::Component& component,
    bool sourceMenu)
{
    std::vector<juce::ComboBox*> comboBoxes;
    collectComboBoxes(component, comboBoxes);
    const auto match = std::find_if(
        comboBoxes.begin(), comboBoxes.end(), [sourceMenu](const auto* comboBox)
        {
            const auto isSource = comboBox->getNumItems() > 0
                                  && comboBox->getItemText(0) == "LFO 1";
            return isSource == sourceMenu;
        });

    return match != comboBoxes.end()
               ? dynamic_cast<ModulationMatrixRoutingComboBox*>(*match)
               : nullptr;
}

class SliderInteractionCapture final : public juce::Slider::Listener
{
public:
    void sliderValueChanged(juce::Slider*) override { ++valueChangeCount; }
    void sliderDragStarted(juce::Slider*) override { ++dragStartCount; }
    void sliderDragEnded(juce::Slider*) override { ++dragEndCount; }

    int valueChangeCount = 0;
    int dragStartCount = 0;
    int dragEndCount = 0;
};

class EditAmountOnDragEnd final : public juce::Slider::Listener
{
public:
    void sliderValueChanged(juce::Slider*) override {}
    void sliderDragStarted(juce::Slider*) override {}

    void sliderDragEnded(juce::Slider* slider) override
    {
        ++dragEndCount;
        slider->setValue(juce::jlimit(-1.0,
                                     1.0,
                                     slider->getValue() + 0.17),
                         juce::sendNotificationSync);
    }

    int dragEndCount = 0;
};

class DeleteMatrixPanelOnAmountDragEnd final : public juce::Slider::Listener
{
public:
    DeleteMatrixPanelOnAmountDragEnd(
        juce::Slider& sliderToObserve,
        std::unique_ptr<ModulationMatrixPanel>& panelToDelete)
        : slider(&sliderToObserve), panel(panelToDelete)
    {
        sliderToObserve.addListener(this);
    }

    ~DeleteMatrixPanelOnAmountDragEnd() override
    {
        if (slider != nullptr)
            slider->removeListener(this);
    }

    void sliderValueChanged(juce::Slider*) override {}
    void sliderDragStarted(juce::Slider*) override {}

    void sliderDragEnded(juce::Slider*) override
    {
        ++dragEndCount;
        if (deletionStarted)
            return;

        deletionStarted = true;
        panel.reset();
        callbackCompleted = true;
    }

    juce::Component::SafePointer<juce::Slider> slider;
    std::unique_ptr<ModulationMatrixPanel>& panel;
    int dragEndCount = 0;
    bool deletionStarted = false;
    bool callbackCompleted = false;
};

class ButtonClickCapture final : private juce::Button::Listener
{
public:
    explicit ButtonClickCapture(juce::Button& buttonToObserve)
        : button(buttonToObserve)
    {
        button.addListener(this);
    }

    ~ButtonClickCapture() override
    {
        button.removeListener(this);
    }

    int getClickCount() const noexcept { return clickCount; }

private:
    void buttonClicked(juce::Button*) override { ++clickCount; }

    juce::Button& button;
    int clickCount = 0;
};

std::vector<juce::ModifierKeys> getRejectedButtonModifiers()
{
    std::vector<juce::ModifierKeys> modifiers {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier }
    };

#if JUCE_MAC
    modifiers.emplace_back(juce::ModifierKeys::leftButtonModifier
                           | juce::ModifierKeys::ctrlModifier);
#endif

    return modifiers;
}

void exerciseButtonPointerGesture(juce::Button& button,
                                  juce::ModifierKeys downModifiers)
{
    const auto position = button.getLocalBounds().toFloat().getCentre();
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(button,
                                       position,
                                       downModifiers,
                                       position,
                                       false));
    component.mouseUp(makeMouseEvent(button,
                                     position,
                                     {},
                                     position,
                                     false));
}

void endButtonPointerGesture(juce::Button& button)
{
    const auto position = button.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(button).mouseUp(
        makeMouseEvent(button, position, {}, position, false));
}

void beginButtonPointerGesture(juce::Button& button,
                               juce::ModifierKeys downModifiers)
{
    const auto position = button.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(button).mouseDown(
        makeMouseEvent(button,
                       position,
                       downModifiers,
                       position,
                       false));
}
} // namespace

TEST_CASE("Modulation matrix viewport stays width-stable at first overflow",
          "[ui][modulation-matrix][viewport][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();

    const auto setRoutingCount = [&manager](int count)
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        for (int index = 0; index < count; ++index)
            routings.add({});
        manager.advanceModulationRoutingRevisionLocked();
    };

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    auto* viewport = findViewport(panel);
    REQUIRE(viewport != nullptr);
    auto* content = viewport->getViewedComponent();
    REQUIRE(content != nullptr);

    const auto rowsThatFit = viewport->getHeight() / 44;
    REQUIRE(rowsThatFit > 0);
    setRoutingCount(rowsThatFit);
    panel.buildUiFromProcessorState();

    CHECK_FALSE(viewport->getVerticalScrollBar().isVisible());
    CHECK_FALSE(viewport->isHorizontalScrollBarShown());
    CHECK_FALSE(viewport->getHorizontalScrollBar().isVisible());
    CHECK(content->getWidth() == viewport->getMaximumVisibleWidth());

    setRoutingCount(rowsThatFit + 1);
    panel.buildUiFromProcessorState();

    CHECK(viewport->getVerticalScrollBar().isVisible());
    CHECK_FALSE(viewport->isHorizontalScrollBarShown());
    CHECK_FALSE(viewport->getHorizontalScrollBar().isVisible());
    CHECK(content->getWidth() == viewport->getMaximumVisibleWidth());
    CHECK(viewport->getMaximumVisibleWidth()
          == viewport->getWidth() - viewport->getScrollBarThickness());
}

TEST_CASE("Modulation commands publish only when routing state changes",
          "[modulation-matrix][state][notification][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();
    NonParameterChangeCapture host(processor);
    const juce::String target { "notification_target" };

    const auto initialRevision = manager.getModulationRoutingRevision();
    CHECK_FALSE(processor.clearModulationForParameter(target));
    CHECK_FALSE(processor.invertModulationDepthForParameter(target));
    CHECK_FALSE(processor.toggleModulationBypassForParameter(target));
    CHECK_FALSE(processor.clearModulationForParameter({}));
    CHECK_FALSE(processor.invertModulationDepthForParameter({}));
    CHECK_FALSE(processor.toggleModulationBypassForParameter({}));
    CHECK(manager.getModulationRoutingRevision() == initialRevision);
    CHECK(host.notificationCount == 0);

    REQUIRE(processor.assignLfoToTarget(1, target)
            == LfoManager::AssignmentResult::changed);
    REQUIRE(host.notificationCount == 1);
    host.notificationCount = 0;

    processor.setModulationDepth(target, 0.0f);
    REQUIRE(host.notificationCount == 1);
    host.notificationCount = 0;
    const auto zeroDepthRevision = manager.getModulationRoutingRevision();

    CHECK_FALSE(processor.invertModulationDepthForParameter(target));
    CHECK(manager.getModulationRoutingRevision() == zeroDepthRevision);
    CHECK(host.notificationCount == 0);

    REQUIRE(processor.toggleModulationBypassForParameter(target));
    CHECK(manager.getModulationRoutingRevision() == zeroDepthRevision + 1);
    CHECK(host.notificationCount == 1);
    {
        const auto routings = manager.getModulationRoutingsCopy();
        const auto routing = std::find_if(
            routings.begin(), routings.end(),
            [&target](const auto& candidate)
            {
                return candidate.targetParameterID == target;
            });
        REQUIRE(routing != routings.end());
        CHECK(routing->isBypassed);
    }

    host.notificationCount = 0;
    processor.setModulationDepth(target, 0.4f);
    REQUIRE(host.notificationCount == 1);
    host.notificationCount = 0;
    const auto positiveDepthRevision = manager.getModulationRoutingRevision();

    REQUIRE(processor.invertModulationDepthForParameter(target));
    CHECK(manager.getModulationRoutingRevision()
          == positiveDepthRevision + 1);
    CHECK(host.notificationCount == 1);
    {
        const auto routings = manager.getModulationRoutingsCopy();
        const auto routing = std::find_if(
            routings.begin(), routings.end(),
            [&target](const auto& candidate)
            {
                return candidate.targetParameterID == target;
            });
        REQUIRE(routing != routings.end());
        CHECK(routing->depth == Catch::Approx(-0.4f));
    }

    host.notificationCount = 0;
    const auto invertedDepthRevision = manager.getModulationRoutingRevision();
    REQUIRE(processor.clearModulationForParameter(target));
    CHECK(manager.getModulationRoutingRevision()
          == invertedDepthRevision + 1);
    CHECK(host.notificationCount == 1);

    host.notificationCount = 0;
    const auto clearedRevision = manager.getModulationRoutingRevision();
    CHECK_FALSE(processor.clearModulationForParameter(target));
    CHECK_FALSE(processor.invertModulationDepthForParameter(target));
    CHECK_FALSE(processor.toggleModulationBypassForParameter(target));
    CHECK(manager.getModulationRoutingRevision() == clearedRevision);
    CHECK(host.notificationCount == 0);
}

TEST_CASE("Modulation routing edits stop at the shared capacity",
          "[modulation-matrix][state][capacity][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();

    SECTION("revision-aware add and the matrix button stop at capacity")
    {
        auto state = manager.getModulationRoutingStateSnapshot();
        while (state.routings.size()
               < LfoManager::maximumModulationRoutings)
        {
            const auto result =
                manager.addEmptyModulationRoutingIfRevisionMatches(
                    state.revision);
            REQUIRE(result.accepted);
            REQUIRE(result.changed);
            state = manager.getModulationRoutingStateSnapshot();
        }

        REQUIRE(state.routings.size()
                == LfoManager::maximumModulationRoutings);
        const auto fullRevision = state.revision;
        const auto rejectedAdd =
            manager.addEmptyModulationRoutingIfRevisionMatches(fullRevision);
        CHECK(rejectedAdd.accepted);
        CHECK_FALSE(rejectedAdd.changed);
        CHECK(rejectedAdd.revision == fullRevision);
        CHECK(manager.getModulationRoutingsCopy().size()
              == LfoManager::maximumModulationRoutings);

        ModulationMatrixPanel panel { processor };
        panel.setBounds(0, 0, 760, 420);
        auto* addButton = findTextButton(panel, "+ ADD ROUTE");
        REQUIRE(addButton != nullptr);
        CHECK_FALSE(addButton->isEnabled());

        addButton->triggerClick();
        CHECK(manager.getModulationRoutingsCopy().size()
              == LfoManager::maximumModulationRoutings);
        CHECK(manager.getModulationRoutingRevision() == fullRevision);
    }

    SECTION("assign reuses existing capacity and never appends past it")
    {
        juce::Array<ModulationRouting> fullRoutings;
        fullRoutings.ensureStorageAllocated(
            LfoManager::maximumModulationRoutings);
        for (int i = 0; i < LfoManager::maximumModulationRoutings; ++i)
        {
            ModulationRouting routing;
            routing.sourceLfoIndex = i % 4;
            routing.targetParameterID = "capacity_target_" + juce::String(i);
            fullRoutings.add(std::move(routing));
        }
        fullRoutings.getReference(fullRoutings.size() - 1)
            .targetParameterID.clear();

        REQUIRE(manager.replaceLfoDataAndRoutings(
            std::array<LfoData, 4> {}, fullRoutings));
        const auto initialRevision = manager.getModulationRoutingRevision();

        manager.assignLfoToTarget(2, "capacity_reused_target");
        auto state = manager.getModulationRoutingStateSnapshot();
        REQUIRE(state.routings.size()
                == LfoManager::maximumModulationRoutings);
        CHECK(state.routings.getLast().targetParameterID
              == "capacity_reused_target");
        CHECK(state.routings.getLast().sourceLfoIndex == 2);
        CHECK(state.revision == initialRevision + 1);

        manager.assignLfoToTarget(3, fullRoutings[0].targetParameterID);
        state = manager.getModulationRoutingStateSnapshot();
        REQUIRE(state.routings.size()
                == LfoManager::maximumModulationRoutings);
        CHECK(state.routings[0].sourceLfoIndex == 3);
        CHECK(state.revision == initialRevision + 2);

        manager.assignLfoToTarget(2, "capacity_overflow_target");
        state = manager.getModulationRoutingStateSnapshot();
        CHECK(state.routings.size()
              == LfoManager::maximumModulationRoutings);
        CHECK(state.revision == initialRevision + 2);
        CHECK(std::none_of(
            state.routings.begin(),
            state.routings.end(),
            [](const auto& routing)
            {
                return routing.targetParameterID
                       == "capacity_overflow_target";
            }));
    }

    SECTION("oversized replacement is rejected as one transaction")
    {
        const auto before = manager.getModulationRoutingStateSnapshot();
        const auto shapesBefore = manager.getLfoDataCopy();
        REQUIRE(shapesBefore.size() == 4);

        juce::Array<ModulationRouting> oversized;
        for (int i = 0;
             i <= LfoManager::maximumModulationRoutings;
             ++i)
            oversized.add({});

        std::array<LfoData, 4> replacementShapes;
        replacementShapes[0].points[0].y = 0.75f;
        CHECK_FALSE(manager.replaceLfoDataAndRoutings(
            replacementShapes, std::move(oversized)));

        const auto after = manager.getModulationRoutingStateSnapshot();
        CHECK(after.revision == before.revision);
        CHECK(after.routings.size() == before.routings.size());
        const auto shapesAfter = manager.getLfoDataCopy();
        REQUIRE(shapesAfter.size() == shapesBefore.size());
        CHECK(shapesAfter[0].points == shapesBefore[0].points);
    }
}

TEST_CASE("Host state at the routing capacity remains loadable",
          "[modulation-matrix][state][host][capacity][regression]")
{
    FireAudioProcessor source;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const auto target = targets.front().parameterID;
    source.assignLfoToTarget(2, target);
    source.setModulationDepth(target, -0.375f);

    auto& sourceManager = source.getLfoManager();
    auto routingState = sourceManager.getModulationRoutingStateSnapshot();
    while (routingState.routings.size()
           < LfoManager::maximumModulationRoutings)
    {
        const auto result =
            sourceManager.addEmptyModulationRoutingIfRevisionMatches(
                routingState.revision);
        REQUIRE(result.accepted);
        REQUIRE(result.changed);
        routingState = sourceManager.getModulationRoutingStateSnapshot();
    }

    juce::MemoryBlock stateBlock;
    source.getStateInformation(stateBlock);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
    REQUIRE(xml != nullptr);
    const auto* savedRoutings = xml->getChildByName("MODULATION_STATE");
    REQUIRE(savedRoutings != nullptr);
    REQUIRE(savedRoutings->getNumChildElements()
            == LfoManager::maximumModulationRoutings);

    FireAudioProcessor restored;
    restored.setStateInformation(
        stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
    const auto restoredRoutings =
        restored.getLfoManager().getModulationRoutingsCopy();
    const auto restoredRouting = std::find_if(
        restoredRoutings.begin(),
        restoredRoutings.end(),
        [&target](const auto& routing)
        {
            return routing.targetParameterID == target;
        });
    REQUIRE(restoredRouting != restoredRoutings.end());
    CHECK(restoredRouting->sourceLfoIndex == 2);
    CHECK(restoredRouting->depth == Catch::Approx(-0.375f));
}

TEST_CASE("Preset routing validation uses the shared capacity boundary",
          "[modulation-matrix][preset][state][capacity][regression]")
{
    FireAudioProcessor source;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE(targets.size() >= 2);
    const auto restoredTarget = targets[0].parameterID;
    const auto baselineTarget = targets[1].parameterID;
    source.assignLfoToTarget(1, restoredTarget);
    source.setModulationDepth(restoredTarget, 0.625f);

    juce::XmlElement preset { "WINGSFIRE" };
    state::saveStateToXml(source, preset);
    auto* routingState = preset.getChildByName("MODULATION_STATE");
    REQUIRE(routingState != nullptr);
    REQUIRE(routingState->getNumChildElements() == 1);
    while (routingState->getNumChildElements()
           < LfoManager::maximumModulationRoutings)
    {
        auto* emptyRouting =
            routingState->createNewChildElement("ROUTING");
        ModulationRouting{}.writeToXml(*emptyRouting);
    }

    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(preset, restored));
    const auto exactBoundaryRoutings =
        restored.getLfoManager().getModulationRoutingsCopy();
    const auto restoredRouting = std::find_if(
        exactBoundaryRoutings.begin(),
        exactBoundaryRoutings.end(),
        [&restoredTarget](const auto& routing)
        {
            return routing.targetParameterID == restoredTarget;
        });
    REQUIRE(restoredRouting != exactBoundaryRoutings.end());
    CHECK(restoredRouting->sourceLfoIndex == 1);
    CHECK(restoredRouting->depth == Catch::Approx(0.625f));

    auto oversizedPreset = std::make_unique<juce::XmlElement>(preset);
    REQUIRE(oversizedPreset != nullptr);
    auto* oversizedRoutingState =
        oversizedPreset->getChildByName("MODULATION_STATE");
    REQUIRE(oversizedRoutingState != nullptr);
    auto* overflowRouting =
        oversizedRoutingState->createNewChildElement("ROUTING");
    ModulationRouting{}.writeToXml(*overflowRouting);
    REQUIRE(oversizedRoutingState->getNumChildElements()
            == LfoManager::maximumModulationRoutings + 1);

    restored.clearModulationForParameter(restoredTarget);
    restored.assignLfoToTarget(3, baselineTarget);
    const auto baselineRevision =
        restored.getLfoManager().getModulationRoutingRevision();
    CHECK_FALSE(state::loadStateFromXml(*oversizedPreset, restored));
    const auto rejectedRoutings =
        restored.getLfoManager().getModulationRoutingsCopy();
    CHECK(restored.getLfoManager().getModulationRoutingRevision()
          == baselineRevision);
    CHECK(std::any_of(
        rejectedRoutings.begin(),
        rejectedRoutings.end(),
        [&baselineTarget](const auto& routing)
        {
            return routing.targetParameterID == baselineTarget;
        }));
    CHECK(std::none_of(
        rejectedRoutings.begin(),
        rejectedRoutings.end(),
        [&restoredTarget](const auto& routing)
        {
            return routing.targetParameterID == restoredTarget;
        }));
}

TEST_CASE("Modulation matrix host notifications may synchronously delete the panel",
          "[ui][modulation-matrix][lifetime][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    auto panel = std::make_unique<ModulationMatrixPanel>(processor);
    panel->setBounds(0, 0, 760, 420);

    SECTION("add route")
    {
        auto* addButton = findTextButton(*panel, "+ ADD ROUTE");
        REQUIRE(addButton != nullptr);
        DeleteMatrixPanelOnHostNotification host(processor, panel);

        exerciseButtonPointerGesture(
            *addButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(host.callbackCompleted);
        CHECK(host.notificationCount == 1);
        CHECK(panel == nullptr);
        CHECK(manager.getModulationRoutingsCopy().size() == 2);
    }

    SECTION("add route while an amount gesture is active")
    {
        std::vector<ModulationMatrixRow*> rows;
        collectMatrixRows(*panel, rows);
        REQUIRE(rows.size() == 1);
        auto* amountSlider = findAmountSlider(*rows.front());
        auto* addButton = findTextButton(*panel, "+ ADD ROUTE");
        REQUIRE(amountSlider != nullptr);
        REQUIRE(addButton != nullptr);

        NonParameterChangeCapture host(processor);
        DeleteMatrixPanelOnAmountDragEnd deleteOnDragEnd(
            *amountSlider, panel);
        const auto downPosition =
            amountSlider->getLocalBounds().toFloat().getCentre();
        amountSlider->mouseDown(makeMouseEvent(
            *amountSlider,
            downPosition,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
            downPosition,
            false));
        REQUIRE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(
            *rows.front()));
        const auto notificationsBeforeAdd = host.notificationCount;

        // Add invalidates the old rows and closes their pointer gestures before
        // notifying the host. The drag-end listener deletes the complete panel
        // from inside requestUiRebuild(), so buttonClicked() must use only the
        // processor pointer captured before that lifecycle boundary.
        addButton->triggerClick();

        CHECK(deleteOnDragEnd.callbackCompleted);
        CHECK(deleteOnDragEnd.dragEndCount == 1);
        CHECK(panel == nullptr);
        CHECK(manager.getModulationRoutingsCopy().size() == 2);
        CHECK(host.notificationCount == notificationsBeforeAdd + 1);
    }

    SECTION("change source")
    {
        std::vector<ModulationMatrixRow*> rows;
        collectMatrixRows(*panel, rows);
        REQUIRE(rows.size() == 1);
        std::vector<juce::ComboBox*> comboBoxes;
        collectComboBoxes(*rows.front(), comboBoxes);
        const auto sourceMenu = std::find_if(
            comboBoxes.begin(), comboBoxes.end(), [](const auto* comboBox)
            {
                return comboBox->getNumItems() == 4;
            });
        REQUIRE(sourceMenu != comboBoxes.end());
        DeleteMatrixPanelOnHostNotification host(processor, panel);

        (*sourceMenu)->setSelectedId(2, juce::sendNotificationSync);

        CHECK(host.callbackCompleted);
        CHECK(host.notificationCount == 1);
        CHECK(panel == nullptr);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
    }

    SECTION("drag amount")
    {
        std::vector<ModulationMatrixRow*> rows;
        collectMatrixRows(*panel, rows);
        REQUIRE(rows.size() == 1);
        auto* amountSlider = findAmountSlider(*rows.front());
        REQUIRE(amountSlider != nullptr);
        REQUIRE(amountSlider->getWidth() > 80);

        const auto downPosition = juce::Point<float> {
            4.0f, amountSlider->getLocalBounds().toFloat().getCentreY()
        };
        const auto dragPosition = juce::Point<float> {
            static_cast<float>(amountSlider->getWidth() - 70), downPosition.y
        };
        const auto primary = juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier };
        amountSlider->mouseDown(makeMouseEvent(*amountSlider,
                                               downPosition,
                                               primary,
                                               downPosition,
                                               false));
        const auto depthAfterDown =
            manager.getModulationRoutingsCopy().getReference(0).depth;
        DeleteMatrixPanelOnHostNotification host(
            processor, panel, rows.front());

        // The host deletes the complete panel from lfoDataHasChanged(). The
        // notification must run only after JUCE's Slider::mouseDrag has
        // finished accessing its Pimpl.
        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               primary,
                                               downPosition,
                                               true));

        CHECK(host.callbackCompleted);
        CHECK(host.notificationCount == 1);
        CHECK(host.notificationArrivedAfterPointerDispatch);
        CHECK(panel == nullptr);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].depth != Catch::Approx(depthAfterDown));
    }

    SECTION("remove route")
    {
        std::vector<ModulationMatrixRow*> rows;
        collectMatrixRows(*panel, rows);
        REQUIRE(rows.size() == 1);
        auto* removeButton = dynamic_cast<juce::TextButton*>(
            rows.front()->findChildWithID("remove_button"));
        REQUIRE(removeButton != nullptr);
        DeleteMatrixPanelOnHostNotification host(processor, panel);

        exerciseButtonPointerGesture(
            *removeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(host.callbackCompleted);
        CHECK(host.notificationCount == 1);
        CHECK(panel == nullptr);
        CHECK(manager.getModulationRoutingsCopy().isEmpty());
    }
}

TEST_CASE("Modulation matrix invalidates shifted rows before host notification",
          "[ui][modulation-matrix][state][identity][reentrancy][aba][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add({});
        routings.add({});
        routings.add({});
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 3);
    auto* staleSecondAmount = findAmountSlider(*rows[1]);
    auto* firstRemoveButton = dynamic_cast<juce::TextButton*>(
        rows[0]->findChildWithID("remove_button"));
    REQUIRE(staleSecondAmount != nullptr);
    REQUIRE(firstRemoveButton != nullptr);

    const auto initialRevision = manager.getModulationRoutingRevision();
    EditSliderOnHostNotification host(processor, *staleSecondAmount);
    exerciseButtonPointerGesture(
        *firstRemoveButton,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

    CHECK(host.callbackCompleted);
    CHECK(host.notificationCount == 1);
    CHECK(panel.isUiRebuildPending());
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 1);
    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 2);
    CHECK(liveRoutings[0].depth == Catch::Approx(0.5f));
    CHECK(liveRoutings[1].depth == Catch::Approx(0.5f));
}

TEST_CASE("Modulation matrix rebuild entry points survive deletion during dismissal",
          "[ui][modulation-matrix][lifetime][dismissal][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    auto panel = std::make_unique<ModulationMatrixPanel>(processor);
    panel->setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(*panel, rows);
    REQUIRE(rows.size() == 1);
    auto* amountSlider = findAmountSlider(*rows.front());
    REQUIRE(amountSlider != nullptr);
    const auto position = amountSlider->getLocalBounds().toFloat().getCentre();
    amountSlider->mouseDown(makeMouseEvent(
        *amountSlider,
        position,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
        position,
        false));
    REQUIRE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(*rows.front()));

    EditAmountOnDragEnd editOnDismissal;
    amountSlider->addListener(&editOnDismissal);
    DeleteMatrixPanelOnHostNotification host(
        processor, panel, rows.front());

    SECTION("queued rebuild")
    {
        panel->requestUiRebuild();
    }

    SECTION("immediate rebuild")
    {
        panel->buildUiFromProcessorState();
    }

    CHECK(editOnDismissal.dragEndCount == 1);
    CHECK(host.callbackCompleted);
    CHECK(host.notificationCount == 1);
    CHECK(host.notificationArrivedAfterPointerDispatch);
    CHECK(panel == nullptr);
}

TEST_CASE("Modulation matrix routing menus reject auxiliary and mixed pointer input",
          "[ui][modulation-matrix][input][combo-box][pointer][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    juce::Component desktopHost;
    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    desktopHost.setBounds(0, 0, 760, 80);
    row.setBounds(0, 0, 760, 40);
    desktopHost.addAndMakeVisible(row);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);

    auto* sourceMenu = findRoutingComboBox(row, true);
    REQUIRE(sourceMenu != nullptr);
    REQUIRE(sourceMenu->isShowing());
    const auto selectedId = sourceMenu->getSelectedId();
    const auto sessionRevision =
        ModulationMatrixRoutingComboBoxTestAccess::getPopupSessionRevision(
            *sourceMenu);
    std::vector<juce::ModifierKeys> rejectedModifiers {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::middleButtonModifier }
    };
#if JUCE_MAC
    rejectedModifiers.emplace_back(juce::ModifierKeys::leftButtonModifier
                                   | juce::ModifierKeys::ctrlModifier);
#endif

    for (const auto modifiers : rejectedModifiers)
    {
        const auto position = sourceMenu->getLocalBounds().toFloat().getCentre();
        auto& component = static_cast<juce::Component&>(*sourceMenu);
        component.mouseDown(makeMouseEvent(*sourceMenu,
                                           position,
                                           modifiers,
                                           position,
                                           false));
        component.mouseUp(makeMouseEvent(*sourceMenu,
                                         position,
                                         {},
                                         position,
                                         false));

        CHECK_FALSE(sourceMenu->isPopupActive());
        CHECK_FALSE(ModulationMatrixRoutingComboBoxTestAccess::
                        hasActivePointerInteraction(*sourceMenu));
        CHECK(sourceMenu->getSelectedId() == selectedId);
        CHECK(ModulationMatrixRoutingComboBoxTestAccess::
                  getPopupSessionRevision(*sourceMenu)
              == sessionRevision);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == routing.sourceLfoIndex);
    }

    sourceMenu->setScrollWheelEnabled(true);
    juce::MouseWheelDetails wheel;
    wheel.deltaY = -0.5f;
    static_cast<juce::Component&>(*sourceMenu).mouseWheelMove(
        makeMouseEvent(*sourceMenu,
                       sourceMenu->getLocalBounds().toFloat().getCentre(),
                       {},
                       {},
                       false),
        wheel);
    CHECK(sourceMenu->getSelectedId() == selectedId);
    CHECK(manager.getModulationRoutingsCopy()[0].sourceLfoIndex
          == routing.sourceLfoIndex);
}

TEST_CASE("Modulation matrix popup results remain bound to their routing session",
          "[ui][modulation-matrix][combo-box][session][identity][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE(targets.size() >= 2);

    const ModulationRouting routing {
        0, targets[0].parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* sourceMenu = findRoutingComboBox(*rows.front(), true);
    auto* destinationMenu = findRoutingComboBox(*rows.front(), false);
    auto* amountSlider = findAmountSlider(*rows.front());
    REQUIRE(sourceMenu != nullptr);
    REQUIRE(destinationMenu != nullptr);
    REQUIRE(amountSlider != nullptr);
    REQUIRE(sourceMenu->isShowing());
    REQUIRE(destinationMenu->isShowing());

    const auto checkInitialAssignment = [&]
    {
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == routing.sourceLfoIndex);
        CHECK(routings[0].targetParameterID == routing.targetParameterID);
    };

    SECTION("source result cannot borrow a newer row revision")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        amountSlider->setValue(0.40, juce::sendNotificationSync);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());

        auto currentResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        currentResult(2);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
        CHECK(routings[0].depth == Catch::Approx(0.40f));
        CHECK(panel.isUiRebuildPending());
    }

    SECTION("destination result preserves its captured complete routing")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*destinationMenu);
        amountSlider->setValue(-0.35, juce::sendNotificationSync);
        staleResult(3);

        checkInitialAssignment();
        CHECK(destinationMenu->getSelectedId() == 2);
        CHECK_FALSE(panel.isUiRebuildPending());

        auto currentResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*destinationMenu);
        currentResult(3);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].targetParameterID == targets[1].parameterID);
        CHECK(routings[0].depth == Catch::Approx(-0.35f));
        CHECK(panel.isUiRebuildPending());
    }

    SECTION("a replacement popup owns the only consumable result")
    {
        auto supersededResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        auto currentResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);

        supersededResult(2);
        checkInitialAssignment();
        CHECK_FALSE(panel.isUiRebuildPending());

        currentResult(2);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
        CHECK(panel.isUiRebuildPending());
    }

    SECTION("row hide and restore invalidates the old popup")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        rows.front()->setVisible(false);
        rows.front()->setVisible(true);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());
    }

    SECTION("disable and restore invalidates the old popup")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        rows.front()->setEnabled(false);
        rows.front()->setEnabled(true);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());
    }

    SECTION("panel hide and restore invalidates the old popup")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        panel.setVisible(false);
        panel.setVisible(true);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());
    }

    SECTION("keyboard direction commits synchronously")
    {
        CHECK(static_cast<juce::Component&>(*sourceMenu).keyPressed(
            juce::KeyPress { juce::KeyPress::downKey }));
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
        CHECK(panel.isUiRebuildPending());
    }
}

TEST_CASE("Modulation matrix rebuilt rows reject late popup results",
          "[ui][modulation-matrix][combo-box][session][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* sourceMenu = findRoutingComboBox(*rows.front(), true);
    REQUIRE(sourceMenu != nullptr);
    auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
        createPopupResultHandler(*sourceMenu);

    panel.buildUiFromProcessorState();
    staleResult(2);

    const auto routings = manager.getModulationRoutingsCopy();
    REQUIRE(routings.size() == 1);
    CHECK(routings[0].sourceLfoIndex == routing.sourceLfoIndex);
    CHECK(routings[0].targetParameterID == routing.targetParameterID);
    CHECK_FALSE(panel.isUiRebuildPending());
}

TEST_CASE("Modulation matrix popup commit survives synchronous panel deletion",
          "[ui][modulation-matrix][combo-box][session][reentrancy][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    auto panel = std::make_unique<ModulationMatrixPanel>(processor);
    panel->setBounds(0, 0, 760, 420);
    panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel->setVisible(true);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(*panel, rows);
    REQUIRE(rows.size() == 1);
    auto* sourceMenu = findRoutingComboBox(*rows.front(), true);
    REQUIRE(sourceMenu != nullptr);
    auto popupResult = ModulationMatrixRoutingComboBoxTestAccess::
        createPopupResultHandler(*sourceMenu);
    DeleteMatrixPanelOnHostNotification host(processor, panel);

    popupResult(2);

    CHECK(host.callbackCompleted);
    CHECK(host.notificationCount == 1);
    CHECK(panel == nullptr);
    const auto routings = manager.getModulationRoutingsCopy();
    REQUIRE(routings.size() == 1);
    CHECK(routings[0].sourceLfoIndex == 1);
}

TEST_CASE("Modulation matrix routing combo enablement callback survives synchronous deletion",
          "[ui][modulation-matrix][combo-box][lifecycle][enablement][reentrancy][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Component owner;
    auto comboBox =
        std::make_unique<ModulationMatrixRoutingComboBox>();
    owner.addAndMakeVisible(*comboBox);

    bool deletionCallbackCompleted = false;
    auto* comboBoxToDelete = comboBox.get();
    ModulationMatrixRoutingComboBoxTestAccess::setLifecycleReentrancyHook(
        *comboBoxToDelete,
        [&]
        {
            ModulationMatrixRoutingComboBoxTestAccess::
                setLifecycleReentrancyHook(*comboBoxToDelete, {});
            comboBox.reset();
            deletionCallbackCompleted = true;
        });

    // Disabling the surviving owner propagates the lifecycle callback to its
    // child while keeping JUCE's outer setEnabled() call itself alive.
    owner.setEnabled(false);

    CHECK(deletionCallbackCompleted);
    CHECK(comboBox == nullptr);
    CHECK_FALSE(owner.isEnabled());
}

TEST_CASE("Modulation matrix routing combo reparent callback survives synchronous panel deletion",
          "[ui][modulation-matrix][combo-box][lifecycle][reparent][reentrancy][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    juce::Component owner;
    auto panel = std::make_unique<ModulationMatrixPanel>(processor);
    panel->setBounds(0, 0, 760, 420);
    owner.addAndMakeVisible(*panel);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(*panel, rows);
    REQUIRE(rows.size() == 1);
    auto* sourceMenu = findRoutingComboBox(*rows.front(), true);
    REQUIRE(sourceMenu != nullptr);

    // Complete the outer reparent operation before exercising deletion from
    // the ComboBox callback itself. JUCE explicitly disallows deleting a
    // parent while Component::internalHierarchyChanged() is still walking it.
    owner.removeChildComponent(panel.get());
    REQUIRE(owner.getNumChildComponents() == 0);

    bool deletionCallbackCompleted = false;
    ModulationMatrixRoutingComboBoxTestAccess::setLifecycleReentrancyHook(
        *sourceMenu,
        [&]
        {
            ModulationMatrixRoutingComboBoxTestAccess::
                setLifecycleReentrancyHook(*sourceMenu, {});
            panel.reset();
            deletionCallbackCompleted = true;
        });

    ModulationMatrixRoutingComboBoxTestAccess::
        dispatchParentHierarchyChanged(*sourceMenu);

    CHECK(deletionCallbackCompleted);
    CHECK(panel == nullptr);
    CHECK(owner.getNumChildComponents() == 0);
}

TEST_CASE("Modulation matrix amount accepts only primary-button drags",
          "[ui][modulation-matrix][input][amount-slider]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    constexpr float initialDepth = 0.25f;
    const ModulationRouting routing {
        0, targets.front().parameterID, initialDepth, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        REQUIRE_FALSE(routings.isEmpty());
        routings.set(0, routing);
    }

    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    row.setBounds(0, 0, 760, 40);
    auto* amountSlider = findAmountSlider(row);
    REQUIRE(amountSlider != nullptr);
    REQUIRE(amountSlider->getWidth() > 80);

    SliderInteractionCapture sliderCapture;
    NonParameterChangeCapture hostCapture(processor);
    amountSlider->addListener(&sliderCapture);

    const auto downPosition = juce::Point<float> {
        4.0f, amountSlider->getLocalBounds().toFloat().getCentreY()
    };
    const auto dragPosition = juce::Point<float> {
        static_cast<float>(amountSlider->getWidth() - 70), downPosition.y
    };

    const auto exercisePointerGesture = [&](juce::ModifierKeys modifiers)
    {
        amountSlider->mouseDown(makeMouseEvent(*amountSlider,
                                               downPosition,
                                               modifiers,
                                               downPosition,
                                               false));
        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               modifiers,
                                               downPosition,
                                               true));
        amountSlider->mouseUp(makeMouseEvent(*amountSlider,
                                             dragPosition,
                                             modifiers,
                                             downPosition,
                                             true));
    };

    const auto checkRejectedGesture = [&](juce::ModifierKeys modifiers)
    {
        exercisePointerGesture(modifiers);

        CHECK(amountSlider->getValue() == Catch::Approx(initialDepth));
        CHECK(amountSlider->getThumbBeingDragged() == -1);
        CHECK(sliderCapture.valueChangeCount == 0);
        CHECK(sliderCapture.dragStartCount == 0);
        CHECK(sliderCapture.dragEndCount == 0);
        CHECK(hostCapture.notificationCount == 0);

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(routings.isEmpty());
        CHECK(routings[0].depth == Catch::Approx(initialDepth));
    };

    SECTION("physical right click")
    {
        checkRejectedGesture(juce::ModifierKeys {
            juce::ModifierKeys::rightButtonModifier });
    }

    SECTION("physical middle click")
    {
        checkRejectedGesture(juce::ModifierKeys {
            juce::ModifierKeys::middleButtonModifier });
    }

#if JUCE_MAC
    SECTION("macOS Control-click")
    {
        checkRejectedGesture(juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier
            | juce::ModifierKeys::ctrlModifier });
    }
#endif

    SECTION("left-button drag")
    {
        exercisePointerGesture(juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier });

        CHECK(amountSlider->getValue() != Catch::Approx(initialDepth));
        CHECK(amountSlider->getThumbBeingDragged() == -1);
        CHECK(sliderCapture.valueChangeCount > 0);
        CHECK(sliderCapture.dragStartCount == 1);
        CHECK(sliderCapture.dragEndCount == 1);
        CHECK(hostCapture.notificationCount > 0);

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(routings.isEmpty());
        CHECK(routings[0].depth
              == Catch::Approx(static_cast<float>(amountSlider->getValue())));
    }

    SECTION("a move without the owning button recovers a missing mouseUp")
    {
        const auto primary = juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier };
        amountSlider->mouseDown(makeMouseEvent(*amountSlider,
                                               downPosition,
                                               primary,
                                               downPosition,
                                               false));
        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               primary,
                                               downPosition,
                                               true));
        REQUIRE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        REQUIRE(sliderCapture.dragEndCount == 0);

        amountSlider->mouseMove(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               {},
                                               downPosition,
                                               true));

        CHECK_FALSE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        CHECK(sliderCapture.dragEndCount == 1);
        CHECK(amountSlider->getThumbBeingDragged() == -1);
    }

    SECTION("a foreign MouseInputSource cannot drag or release the owner gesture")
    {
        const auto primary = juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier };
        const auto ownerDown = makeMouseEvent(*amountSlider,
                                              downPosition,
                                              primary,
                                              downPosition,
                                              false);
        amountSlider->mouseDown(ownerDown);
        REQUIRE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        CHECK(sliderCapture.dragStartCount == 1);
        const auto valueAfterOwnerDown = amountSlider->getValue();

        const auto ownerType = ownerDown.source.getType();
        const auto ownerIndex = ownerDown.source.getIndex();
        const auto foreignType = ownerType == juce::MouseInputSource::mouse
                                     ? juce::MouseInputSource::touch
                                     : juce::MouseInputSource::mouse;
        ModulationMatrixRowTestAccess::setAmountPointerSource(
            row, foreignType, ownerIndex + 17);

        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               primary,
                                               downPosition,
                                               true));
        amountSlider->mouseUp(makeMouseEvent(*amountSlider,
                                             dragPosition,
                                             {},
                                             downPosition,
                                             true));
        CHECK(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        CHECK(sliderCapture.dragEndCount == 0);
        CHECK(amountSlider->getValue() == Catch::Approx(valueAfterOwnerDown));

        ModulationMatrixRowTestAccess::setAmountPointerSource(
            row, ownerType, ownerIndex);
        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               primary,
                                               downPosition,
                                               true));
        amountSlider->mouseUp(makeMouseEvent(*amountSlider,
                                             dragPosition,
                                             {},
                                             downPosition,
                                             true));
        CHECK_FALSE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        CHECK(sliderCapture.dragEndCount == 1);
        CHECK(amountSlider->getValue() != Catch::Approx(initialDepth));
    }

    amountSlider->removeListener(&sliderCapture);
}

TEST_CASE("Modulation matrix amount uses continuous shared slider feedback",
          "[ui][modulation-matrix][amount-slider][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };

    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    auto* amountSlider = findAmountSlider(row);
    REQUIRE(amountSlider != nullptr);
    auto* animation =
        dynamic_cast<PrimarySliderAnimationState*>(amountSlider);
    REQUIRE(animation != nullptr);

    ModulationMatrixRowTestAccess::setAmountAnimationTargets(
        row, 1.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(ModulationMatrixRowTestAccess::advanceAmountAnimation(
        row, 1.0f / 60.0f));
    CHECK(animation->getHoverAnimation() > 0.0f);
    CHECK(animation->getHoverAnimation() < 1.0f);
    CHECK(animation->getPressAnimation() > animation->getHoverAnimation());
    CHECK(animation->getFocusAnimation() > 0.0f);
    CHECK(animation->getDisabledAnimation() > 0.0f);

    // A row hidden directly or through an ancestor must not retain animated
    // hover/focus state or an off-screen timer.
    ModulationMatrixRowTestAccess::updateAmountAnimationTargets(row);
    CHECK(animation->getHoverAnimation() == Catch::Approx(0.0f));
    CHECK(animation->getPressAnimation() == Catch::Approx(0.0f));
    CHECK(animation->getFocusAnimation() == Catch::Approx(0.0f));
    CHECK(animation->getDisabledAnimation() == Catch::Approx(0.0f));

    amountSlider->setEnabled(false);
    CHECK(animation->getDisabledAnimation() == Catch::Approx(1.0f));
}

TEST_CASE("Modulation matrix row dismissal closes amount and button gestures",
          "[ui][modulation-matrix][input][dismissal]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        manager.getModulationRoutings().set(0, routing);
    }

    ModulationMatrixRow row(
        processor, 0, routing, makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    juce::Component visibleParent;
    visibleParent.setBounds(0, 0, 760, 40);
    visibleParent.addAndMakeVisible(row);
    row.setBounds(0, 0, 760, 40);
    auto* amountSlider = findAmountSlider(row);
    auto* polarityButton = findTextButton(row, "Bi");
    REQUIRE(amountSlider != nullptr);
    REQUIRE(polarityButton != nullptr);
    SliderInteractionCapture sliderCapture;
    amountSlider->addListener(&sliderCapture);

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier };
    const auto sliderPosition = amountSlider->getLocalBounds().toFloat().getCentre();
    amountSlider->mouseDown(makeMouseEvent(*amountSlider,
                                          sliderPosition,
                                          primary,
                                          sliderPosition,
                                          false));
    static_cast<juce::Component&>(*polarityButton).mouseDown(
        makeMouseEvent(*polarityButton,
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       primary,
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       false));
    REQUIRE(sliderCapture.dragStartCount == 1);

    row.setVisible(false);
    CHECK_FALSE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
    CHECK(sliderCapture.dragEndCount == 1);

    const auto polarityBeforeRelease = routing.isBipolar;
    static_cast<juce::Component&>(*polarityButton).mouseUp(
        makeMouseEvent(*polarityButton,
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       {},
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       false));
    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE_FALSE(liveRoutings.isEmpty());
    CHECK(liveRoutings[0].isBipolar == polarityBeforeRelease);

    amountSlider->removeListener(&sliderCapture);
}

TEST_CASE("Modulation matrix buttons accept only complete primary-button clicks",
          "[ui][modulation-matrix][input][buttons]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.5f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    int deleteCount = 0;
    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [&deleteCount](std::uint64_t, ModulationRouting)
        {
            ++deleteCount;
        });
    row.setBounds(0, 0, 760, 40);
    auto* polarityButton = findTextButton(row, "Bi");
    auto* bypassButton = findTextButton(row, "Off");
    auto* removeButton = dynamic_cast<juce::TextButton*>(
        row.findChildWithID("remove_button"));
    REQUIRE(polarityButton != nullptr);
    REQUIRE(bypassButton != nullptr);
    REQUIRE(removeButton != nullptr);

    ModulationMatrixPanel panel(processor);
    panel.setBounds(0, 0, 760, 420);
    auto* addButton = findTextButton(panel, "+ ADD ROUTE");
    auto* closeButton = findTextButton(panel, "Close");
    REQUIRE(addButton != nullptr);
    REQUIRE(closeButton != nullptr);

    NonParameterChangeCapture host(processor);

    const auto exerciseRejectedGestures = [](juce::Button& button,
                                             const auto& checkInvariant)
    {
        for (const auto modifiers : getRejectedButtonModifiers())
        {
            exerciseButtonPointerGesture(button, modifiers);
            checkInvariant();
        }
    };

    SECTION("polarity")
    {
        ButtonClickCapture clicks(*polarityButton);
        const auto checkUnchanged = [&]
        {
            const auto routings = manager.getModulationRoutingsCopy();
            REQUIRE(routings.size() == 1);
            CHECK(routings[0].isBipolar);
            CHECK(polarityButton->getToggleState());
            CHECK(polarityButton->getButtonText() == "Bi");
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*polarityButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *polarityButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK_FALSE(routings[0].isBipolar);
        CHECK_FALSE(polarityButton->getToggleState());
        CHECK(polarityButton->getButtonText() == "Uni");
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 1);
    }

    SECTION("bypass")
    {
        ButtonClickCapture clicks(*bypassButton);
        const auto checkUnchanged = [&]
        {
            const auto routings = manager.getModulationRoutingsCopy();
            REQUIRE(routings.size() == 1);
            CHECK_FALSE(routings[0].isBypassed);
            CHECK_FALSE(bypassButton->getToggleState());
            CHECK(bypassButton->getButtonText() == "Off");
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*bypassButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *bypassButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].isBypassed);
        CHECK(bypassButton->getToggleState());
        CHECK(bypassButton->getButtonText() == "On");
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 1);
    }

    SECTION("remove")
    {
        ButtonClickCapture clicks(*removeButton);
        const auto checkUnchanged = [&]
        {
            CHECK(manager.getModulationRoutingsCopy().size() == 1);
            CHECK(deleteCount == 0);
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*removeButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *removeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(manager.getModulationRoutingsCopy().size() == 1);
        CHECK(deleteCount == 1);
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 0);
    }

    SECTION("add")
    {
        ButtonClickCapture clicks(*addButton);
        const auto checkUnchanged = [&]
        {
            CHECK(manager.getModulationRoutingsCopy().size() == 1);
            CHECK_FALSE(panel.isUiRebuildPending());
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*addButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *addButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(manager.getModulationRoutingsCopy().size() == 2);
        CHECK(panel.isUiRebuildPending());
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 1);
    }

    SECTION("close")
    {
        ButtonClickCapture clicks(*closeButton);
        const auto checkUnchanged = [&]
        {
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*closeButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 0);
    }

    SECTION("a new primary down replaces stale rejected ownership")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier });

        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }

    SECTION("a new primary down safely replaces stale primary ownership")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        REQUIRE(closeButton->isDown());

        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }

    SECTION("hiding cancels a primary gesture without clicking")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        REQUIRE(closeButton->isDown());

        closeButton->setVisible(false);
        CHECK(clicks.getClickCount() == 0);
        CHECK_FALSE(closeButton->isDown());

        closeButton->setVisible(true);
        endButtonPointerGesture(*closeButton);
        CHECK(clicks.getClickCount() == 0);
        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }

    SECTION("disabling cancels a primary gesture without clicking")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        REQUIRE(closeButton->isDown());

        closeButton->setEnabled(false);
        CHECK(clicks.getClickCount() == 0);
        CHECK_FALSE(closeButton->isDown());

        closeButton->setEnabled(true);
        endButtonPointerGesture(*closeButton);
        CHECK(clicks.getClickCount() == 0);
        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }
}

TEST_CASE("Modulation matrix primary buttons preserve non-pointer activation",
          "[ui][modulation-matrix][input][buttons][keyboard]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.5f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    int deleteCount = 0;
    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [&deleteCount](std::uint64_t, ModulationRouting)
        {
            ++deleteCount;
        });
    row.setBounds(0, 0, 760, 40);
    auto* polarityButton = findTextButton(row, "Bi");
    auto* bypassButton = findTextButton(row, "Off");
    auto* removeButton = dynamic_cast<juce::TextButton*>(
        row.findChildWithID("remove_button"));
    REQUIRE(polarityButton != nullptr);
    REQUIRE(bypassButton != nullptr);
    REQUIRE(removeButton != nullptr);

    ModulationMatrixPanel panel(processor);
    panel.setBounds(0, 0, 760, 420);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    auto* addButton = findTextButton(panel, "+ ADD ROUTE");
    auto* closeButton = findTextButton(panel, "Close");
    REQUIRE(addButton != nullptr);
    REQUIRE(closeButton != nullptr);
    REQUIRE(closeButton->isShowing());

    SECTION("polarity triggerClick")
    {
        polarityButton->triggerClick();
        auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK_FALSE(routings[0].isBipolar);

        row.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK_FALSE(routings[0].isBipolar);
    }

    SECTION("bypass triggerClick")
    {
        bypassButton->triggerClick();
        auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].isBypassed);

        row.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].isBypassed);
    }

    SECTION("remove triggerClick")
    {
        removeButton->triggerClick();
        CHECK(deleteCount == 1);

        row.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(deleteCount == 1);
    }

    SECTION("add triggerClick")
    {
        addButton->triggerClick();
        CHECK(manager.getModulationRoutingsCopy().size() == 2);

        panel.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(manager.getModulationRoutingsCopy().size() == 2);
    }

    SECTION("close Return key")
    {
        ButtonClickCapture clicks(*closeButton);
        CHECK(static_cast<juce::Component&>(*closeButton).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        CHECK(clicks.getClickCount() == 1);
    }

    SECTION("close Space key")
    {
        ButtonClickCapture clicks(*closeButton);
        CHECK(static_cast<juce::Component&>(*closeButton).keyPressed(
            juce::KeyPress { juce::KeyPress::spaceKey }));
        CHECK(clicks.getClickCount() == 1);
    }
}

TEST_CASE("Modulation matrix toggle buttons publish only real model changes",
          "[ui][modulation-matrix][state][button]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.5f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        REQUIRE_FALSE(routings.isEmpty());
        routings.set(0, routing);
    }

    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    row.setBounds(0, 0, 760, 40);
    auto* polarityButton = findTextButton(row, "Bi");
    auto* bypassButton = findTextButton(row, "Off");
    REQUIRE(polarityButton != nullptr);
    REQUIRE(bypassButton != nullptr);
    NonParameterChangeCapture host(processor);

    SECTION("polarity")
    {
        polarityButton->setState(juce::Button::buttonOver);
        polarityButton->setState(juce::Button::buttonDown);
        polarityButton->setState(juce::Button::buttonOver);
        polarityButton->setState(juce::Button::buttonNormal);

        auto liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK(liveRoutings[0].isBipolar);
        CHECK(host.notificationCount == 0);

        polarityButton->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);

        liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK_FALSE(liveRoutings[0].isBipolar);
        CHECK(polarityButton->getButtonText() == "Uni");
        CHECK(host.notificationCount == 1);
    }

    SECTION("bypass")
    {
        bypassButton->setState(juce::Button::buttonOver);
        bypassButton->setState(juce::Button::buttonDown);
        bypassButton->setState(juce::Button::buttonOver);
        bypassButton->setState(juce::Button::buttonNormal);

        auto liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK_FALSE(liveRoutings[0].isBypassed);
        CHECK(host.notificationCount == 0);

        bypassButton->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);

        liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK(liveRoutings[0].isBypassed);
        CHECK(bypassButton->getButtonText() == "On");
        CHECK(host.notificationCount == 1);
    }
}

TEST_CASE("Modulation matrix follows externally recalled routings without stale-row writes",
          "[ui][modulation-matrix][state][recall]")
{
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE(targets.size() >= 3);

    const ModulationRouting initialRouting {
        0, targets[0].parameterID, 0.10f, true, false
    };
    const ModulationRouting recalledFirstRouting {
        2, targets[1].parameterID, 0.25f, false, false
    };
    const ModulationRouting recalledSecondRouting {
        3, targets[2].parameterID, -0.45f, true, true
    };

    juce::XmlElement initialPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, initialPreset);
    replacePresetRoutings(initialPreset, { initialRouting });
    state::loadStateFromXml(initialPreset, processor);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);

    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    CHECK(containsComboBoxText(*rows.front(), targets[0].displayText));

    juce::XmlElement recalledPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, recalledPreset);
    replacePresetRoutings(recalledPreset,
                          { recalledFirstRouting, recalledSecondRouting });
    state::loadStateFromXml(recalledPreset, processor);

    // ChangeBroadcaster delivery and the panel rebuild are asynchronous. Until
    // they run, the visible row still represents the previous target and must
    // not use its cached array index to edit the newly recalled first route.
    auto* staleAmountSlider = findAmountSlider(*rows.front());
    REQUIRE(staleAmountSlider != nullptr);
    staleAmountSlider->setValue(0.91, juce::sendNotificationSync);

    auto liveRoutings = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 2);
    CHECK(liveRoutings[0].targetParameterID == recalledFirstRouting.targetParameterID);
    CHECK(liveRoutings[0].depth == Catch::Approx(recalledFirstRouting.depth));

    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    rows.clear();
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 2);

    const auto rowsDisplaying = [&](const juce::String& targetText)
    {
        return static_cast<int>(std::count_if(rows.begin(), rows.end(), [&](auto* row)
                                              { return containsComboBoxText(*row, targetText); }));
    };

    CHECK(rowsDisplaying(targets[1].displayText) == 1);
    CHECK(rowsDisplaying(targets[2].displayText) == 1);
    CHECK(rowsDisplaying(targets[0].displayText) == 0);
}

TEST_CASE("Modulation matrix rejects stale rows after same-target state recall",
          "[ui][modulation-matrix][state][recall][identity][aba][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting initialRouting {
        0, targets.front().parameterID, 0.10f, true, false
    };
    const ModulationRouting recalledRouting {
        2, targets.front().parameterID, -0.45f, true, false
    };

    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(initialRouting);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* staleRow = rows.front();

    juce::XmlElement recalledPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, recalledPreset);
    replacePresetRoutings(recalledPreset, { recalledRouting });
    REQUIRE(state::loadStateFromXml(recalledPreset, processor));
    REQUIRE_FALSE(panel.isUiRebuildPending());
    const auto revisionAfterRecall =
        manager.getModulationRoutingRevision();
    NonParameterChangeCapture host(processor);

    const auto checkRecalledAuthority = [&]
    {
        const auto liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE(liveRoutings.size() == 1);
        CHECK(liveRoutings[0].sourceLfoIndex
              == recalledRouting.sourceLfoIndex);
        CHECK(liveRoutings[0].targetParameterID
              == recalledRouting.targetParameterID);
        CHECK(liveRoutings[0].depth
              == Catch::Approx(recalledRouting.depth));
        CHECK(liveRoutings[0].isBipolar
              == recalledRouting.isBipolar);
        CHECK(liveRoutings[0].isBypassed
              == recalledRouting.isBypassed);
        CHECK(host.notificationCount == 0);
        CHECK(panel.isUiRebuildPending());
        CHECK(manager.getModulationRoutingRevision()
              == revisionAfterRecall);
    };

    SECTION("amount")
    {
        auto* amountSlider = findAmountSlider(*staleRow);
        REQUIRE(amountSlider != nullptr);
        amountSlider->setValue(0.91, juce::sendNotificationSync);
        checkRecalledAuthority();
    }

    SECTION("polarity")
    {
        auto* polarityButton = findTextButton(*staleRow, "Bi");
        REQUIRE(polarityButton != nullptr);
        exerciseButtonPointerGesture(
            *polarityButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        checkRecalledAuthority();
    }

    SECTION("source")
    {
        std::vector<juce::ComboBox*> comboBoxes;
        collectComboBoxes(*staleRow, comboBoxes);
        const auto sourceMenu = std::find_if(
            comboBoxes.begin(), comboBoxes.end(), [](const auto* comboBox)
            {
                return comboBox->getNumItems() == 4;
            });
        REQUIRE(sourceMenu != comboBoxes.end());
        (*sourceMenu)->setSelectedId(2, juce::sendNotificationSync);
        checkRecalledAuthority();
    }

    SECTION("remove")
    {
        auto* removeButton = dynamic_cast<juce::TextButton*>(
            staleRow->findChildWithID("remove_button"));
        REQUIRE(removeButton != nullptr);
        exerciseButtonPointerGesture(
            *removeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        checkRecalledAuthority();
    }
}

TEST_CASE("Modulation matrix invalidates rows after identical state recall",
          "[ui][modulation-matrix][state][recall][identity][aba][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        1, targets.front().parameterID, 0.25f, false, true
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* staleAmountSlider = findAmountSlider(*rows.front());
    REQUIRE(staleAmountSlider != nullptr);

    juce::XmlElement identicalPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, identicalPreset);
    const auto revisionBeforeRecall =
        manager.getModulationRoutingRevision();
    REQUIRE(state::loadStateFromXml(identicalPreset, processor));
    CHECK(manager.getModulationRoutingRevision() != revisionBeforeRecall);
    REQUIRE_FALSE(panel.isUiRebuildPending());
    const auto revisionAfterRecall =
        manager.getModulationRoutingRevision();

    NonParameterChangeCapture host(processor);
    staleAmountSlider->setValue(0.73, juce::sendNotificationSync);

    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 1);
    CHECK(liveRoutings[0].sourceLfoIndex == routing.sourceLfoIndex);
    CHECK(liveRoutings[0].targetParameterID == routing.targetParameterID);
    CHECK(liveRoutings[0].depth == Catch::Approx(routing.depth));
    CHECK(liveRoutings[0].isBipolar == routing.isBipolar);
    CHECK(liveRoutings[0].isBypassed == routing.isBypassed);
    CHECK(host.notificationCount == 0);
    CHECK(panel.isUiRebuildPending());
    CHECK(manager.getModulationRoutingRevision() == revisionAfterRecall);
}

TEST_CASE("Modulation matrix keeps its edit session current after local edits",
          "[ui][modulation-matrix][state][identity][session][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE(targets.size() >= 2);

    const ModulationRouting firstRouting {
        0, targets[0].parameterID, 0.10f, true, false
    };
    const ModulationRouting secondRouting {
        1, targets[1].parameterID, -0.10f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(firstRouting);
        routings.add(secondRouting);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 2);
    auto* firstAmount = findAmountSlider(*rows[0]);
    auto* secondAmount = findAmountSlider(*rows[1]);
    REQUIRE(firstAmount != nullptr);
    REQUIRE(secondAmount != nullptr);
    NonParameterChangeCapture host(processor);
    const auto initialRevision = manager.getModulationRoutingRevision();

    firstAmount->setValue(0.20, juce::sendNotificationSync);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 1);
    CHECK(host.notificationCount == 1);
    CHECK_FALSE(panel.isUiRebuildPending());

    firstAmount->setValue(0.30, juce::sendNotificationSync);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 2);
    CHECK(host.notificationCount == 2);
    CHECK_FALSE(panel.isUiRebuildPending());

    secondAmount->setValue(-0.40, juce::sendNotificationSync);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 3);
    CHECK(host.notificationCount == 3);
    CHECK_FALSE(panel.isUiRebuildPending());

    auto* firstPolarity = findTextButton(*rows[0], "Bi");
    REQUIRE(firstPolarity != nullptr);
    exerciseButtonPointerGesture(
        *firstPolarity,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 4);
    CHECK(host.notificationCount == 4);
    CHECK_FALSE(panel.isUiRebuildPending());

    auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 2);
    CHECK(liveRoutings[0].depth == Catch::Approx(0.30f));
    CHECK_FALSE(liveRoutings[0].isBipolar);
    CHECK(liveRoutings[1].depth == Catch::Approx(-0.40f));

    auto* removeButton = dynamic_cast<juce::TextButton*>(
        rows[0]->findChildWithID("remove_button"));
    REQUIRE(removeButton != nullptr);
    exerciseButtonPointerGesture(
        *removeButton,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

    liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 1);
    CHECK(liveRoutings[0].targetParameterID
          == secondRouting.targetParameterID);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 5);
    CHECK(host.notificationCount == 5);
    CHECK(panel.isUiRebuildPending());
}

TEST_CASE("Modulation matrix ignores empty rows while a structural rebuild is pending",
          "[ui][modulation-matrix][state][identity]")
{
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        REQUIRE(routings.size() >= 3);
        routings.getReference(0).depth = 0.10f;
        routings.getReference(1).depth = 0.20f;
        routings.getReference(2).depth = 0.30f;
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);

    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() >= 3);
    auto* staleSecondAmount = findAmountSlider(*rows[1]);
    REQUIRE(staleSecondAmount != nullptr);
    NonParameterChangeCapture host(processor);

    // Removing the first empty slot shifts every later empty row while their
    // target IDs remain indistinguishable. The revision must make the old row
    // reject the shifted slot without relying on a pre-existing rebuild flag.
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        manager.getModulationRoutings().remove(0);
        manager.advanceModulationRoutingRevisionLocked();
    }
    REQUIRE_FALSE(panel.isUiRebuildPending());
    staleSecondAmount->setValue(0.91, juce::sendNotificationSync);

    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() >= 2);
    CHECK(liveRoutings[1].depth == Catch::Approx(0.30f));
    CHECK(host.notificationCount == 0);
    CHECK(panel.isUiRebuildPending());

    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    rows.clear();
    collectMatrixRows(panel, rows);
    CHECK(rows.size() == static_cast<size_t>(liveRoutings.size()));
}

TEST_CASE("Modulation matrix source affordances follow the LFO bank palette",
          "[ui][modulation-matrix][theme][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    std::array<std::uint64_t, fire::ui::lfoBankCount> fingerprints {};

    for (int sourceIndex = 0;
         sourceIndex < fire::ui::lfoBankCount;
         ++sourceIndex)
    {
        CAPTURE(sourceIndex);
        const ModulationRouting routing {
            sourceIndex, {}, 0.72f, true, false
        };
        ModulationMatrixRow row(
            processor,
            0,
            routing,
            makeRoutingEditSession(processor),
            [](std::uint64_t, ModulationRouting) {});
        row.setBounds(0, 0, 760, 44);
        const auto sourceColour = fire::ui::lfoBankColour(sourceIndex);
        const auto renderedSourceColour =
            fire::ui::colours::surface2.overlaidWith(
                sourceColour.withMultipliedAlpha(0.9f));

        const auto& sourceMenu =
            ModulationMatrixRowTestAccess::getSourceMenu(row);
        CHECK(sourceMenu.getSelectedId() == sourceIndex + 1);
        CHECK(sourceMenu.getText() == "LFO " + juce::String(sourceIndex + 1));
        CHECK(sourceMenu.findColour(juce::ComboBox::textColourId)
              == sourceColour);
        CHECK(sourceMenu.findColour(juce::ComboBox::outlineColourId)
              == sourceColour.withAlpha(0.45f));

        const auto* sourceItems = sourceMenu.getRootMenu();
        REQUIRE(sourceItems != nullptr);
        CHECK(sourceItems->getNumItems() == fire::ui::lfoBankCount);
        int sourceItemIndex = 0;
        for (juce::PopupMenu::MenuItemIterator iterator(*sourceItems);
             iterator.next();)
        {
            const auto& item = iterator.getItem();
            CAPTURE(sourceItemIndex);
            REQUIRE(sourceItemIndex < fire::ui::lfoBankCount);
            CHECK(item.itemID == sourceItemIndex + 1);
            CHECK(item.text == "LFO " + juce::String(sourceItemIndex + 1));
            CHECK(item.colour
                  == fire::ui::lfoBankColour(sourceItemIndex));
            ++sourceItemIndex;
        }
        CHECK(sourceItemIndex == fire::ui::lfoBankCount);

        auto& amountSlider =
            ModulationMatrixRowTestAccess::getAmountSlider(row);
        CHECK(amountSlider.findColour(juce::Slider::trackColourId)
              == sourceColour);

        const auto positiveImage = renderSlider(amountSlider);
        const auto positiveMatchingPixels =
            countPixelsNearColour(positiveImage, renderedSourceColour);
        CAPTURE(positiveMatchingPixels);
        CHECK(positiveMatchingPixels > 24);
        fingerprints[static_cast<size_t>(sourceIndex)] =
            imageFingerprint(positiveImage);

        amountSlider.setValue(-0.72, juce::dontSendNotification);
        const auto negativeImage = renderSlider(amountSlider);
        const auto negativeMatchingPixels =
            countPixelsNearColour(negativeImage, renderedSourceColour);
        CAPTURE(negativeMatchingPixels);
        CHECK(negativeMatchingPixels > 24);
        CHECK(imageFingerprint(negativeImage)
              != fingerprints[static_cast<size_t>(sourceIndex)]);

        const auto& bipolarButton =
            ModulationMatrixRowTestAccess::getBipolarButton(row);
        CHECK(bipolarButton.getToggleState());
        CHECK(bipolarButton.findColour(juce::TextButton::textColourOnId)
              == sourceColour);

        const auto& destinationMenu =
            ModulationMatrixRowTestAccess::getDestinationMenu(row);
        CHECK(destinationMenu.findColour(juce::ComboBox::textColourId)
              == fire::ui::colours::textPrimary);
        CHECK(destinationMenu.findColour(juce::ComboBox::outlineColourId)
              == fire::ui::colours::hairline);

        const auto& bypassButton =
            ModulationMatrixRowTestAccess::getBypassButton(row);
        CHECK(bypassButton.findColour(juce::TextButton::textColourOnId)
              == fire::ui::colours::danger);
        CHECK(bypassButton.findColour(juce::TextButton::textColourOffId)
              == fire::ui::colours::positive);

        const auto& removeButton =
            ModulationMatrixRowTestAccess::getRemoveButton(row);
        CHECK(removeButton.findColour(juce::TextButton::textColourOnId)
              == fire::ui::colours::danger);
        CHECK(removeButton.findColour(juce::TextButton::textColourOffId)
              == fire::ui::colours::danger);
    }

    for (size_t first = 0; first < fingerprints.size(); ++first)
        for (size_t second = first + 1; second < fingerprints.size(); ++second)
            CHECK(fingerprints[first] != fingerprints[second]);

    const ModulationRouting fallbackRouting { 0, {}, 0.72f, true, false };
    ModulationMatrixRow fallbackRow(
        processor,
        0,
        fallbackRouting,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    fallbackRow.setBounds(0, 0, 760, 44);
    auto& fallbackSlider =
        ModulationMatrixRowTestAccess::getAmountSlider(fallbackRow);
    fallbackSlider.setColour(juce::Slider::trackColourId,
                             juce::Colours::transparentBlack);
    const auto renderedFallbackColour =
        fire::ui::colours::surface2.overlaidWith(
            fire::ui::colours::ember.withMultipliedAlpha(0.9f));
    CHECK(countPixelsNearColour(renderSlider(fallbackSlider),
                                renderedFallbackColour)
          > 24);

    ModulationMatrixPanel panel { processor };
    auto* addButton = findTextButton(panel, "+ ADD ROUTE");
    REQUIRE(addButton != nullptr);
    CHECK(addButton->findColour(juce::TextButton::textColourOnId)
          == fire::ui::colours::modulation);
    CHECK(addButton->findColour(juce::TextButton::textColourOffId)
          == fire::ui::colours::modulation);
}

TEST_CASE("Modulation matrix row controls expose distinct accessibility semantics",
          "[ui][modulation-matrix][accessibility][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    juce::Component desktopHost;
    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    desktopHost.setBounds(0, 0, 760, 80);
    row.setBounds(0, 0, 760, 40);
    desktopHost.addAndMakeVisible(row);

    auto* sourceMenu = findRoutingComboBox(row, true);
    auto* destinationMenu = findRoutingComboBox(row, false);
    auto* amountSlider = findAmountSlider(row);
    auto* polarityButton = findTextButton(row, "Bi");
    auto* bypassButton = findTextButton(row, "Off");
    auto* removeButton = dynamic_cast<juce::TextButton*>(
        row.findChildWithID("remove_button"));
    REQUIRE(sourceMenu != nullptr);
    REQUIRE(destinationMenu != nullptr);
    REQUIRE(amountSlider != nullptr);
    REQUIRE(polarityButton != nullptr);
    REQUIRE(bypassButton != nullptr);
    REQUIRE(removeButton != nullptr);

    const std::array<juce::Component*, 6> controls {
        sourceMenu,
        destinationMenu,
        amountSlider,
        polarityButton,
        bypassButton,
        removeButton
    };
    for (auto* control : controls)
    {
        REQUIRE(control != nullptr);
        CHECK(control->getAccessibilityHandler() == nullptr);
    }

    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);

    const std::array<juce::String, 6> expectedTitles {
        "Modulation routing 1 source",
        "Modulation routing 1 destination",
        "Modulation routing 1 amount",
        "Modulation routing 1 polarity",
        "Modulation routing 1 bypass",
        "Remove modulation routing 1"
    };
    const std::array<juce::String, 6> expectedHelp {
        "Select the LFO source for modulation routing 1",
        "Select the destination for modulation routing 1",
        "Set the modulation depth for modulation routing 1",
        "Switch modulation routing 1 between bipolar and unipolar",
        "Turn bypass on or off for modulation routing 1",
        "Remove modulation routing 1"
    };
    const std::array<juce::AccessibilityRole, 6> expectedRoles {
        juce::AccessibilityRole::comboBox,
        juce::AccessibilityRole::comboBox,
        juce::AccessibilityRole::slider,
        juce::AccessibilityRole::button,
        juce::AccessibilityRole::button,
        juce::AccessibilityRole::button
    };

    for (size_t controlIndex = 0; controlIndex < controls.size(); ++controlIndex)
    {
        CAPTURE(controlIndex);
        auto& control = *controls[controlIndex];
        CHECK(control.getTitle() == expectedTitles[controlIndex]);
        auto* tooltipClient = dynamic_cast<juce::TooltipClient*>(&control);
        REQUIRE(tooltipClient != nullptr);
        CHECK(tooltipClient->getTooltip() == expectedHelp[controlIndex]);

        auto* accessibility = control.getAccessibilityHandler();
        REQUIRE(accessibility != nullptr);
        CHECK(accessibility->getRole() == expectedRoles[controlIndex]);
        CHECK(accessibility->getTitle() == expectedTitles[controlIndex]);
        CHECK(accessibility->getHelp() == expectedHelp[controlIndex]);
    }

    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 1);
    CHECK(liveRoutings[0].sourceLfoIndex == routing.sourceLfoIndex);
    CHECK(liveRoutings[0].targetParameterID == routing.targetParameterID);
    CHECK(liveRoutings[0].depth == Catch::Approx(routing.depth));
    CHECK(liveRoutings[0].isBipolar == routing.isBipolar);
    CHECK(liveRoutings[0].isBypassed == routing.isBypassed);

    desktopHost.removeFromDesktop();
    for (auto* control : controls)
        CHECK(control->getAccessibilityHandler() == nullptr);
}

TEST_CASE("Cached modulation amount accessibility rejects stale value writes",
          "[ui][modulation-matrix][accessibility][lifecycle][stale][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (int boundaryIndex = 0; boundaryIndex < 3; ++boundaryIndex)
    {
        const auto* boundaryName = boundaryIndex == 0 ? "hidden"
                                 : boundaryIndex == 1 ? "disabled"
                                                      : "peer detached";
        DYNAMIC_SECTION(boundaryName)
        {
            FireAudioProcessor processor;
            const auto targets = ParameterIDAndName::getAllModulatableTargets();
            REQUIRE_FALSE(targets.empty());
            const ModulationRouting routing {
                0, targets.front().parameterID, 0.25f, true, false
            };
            auto& manager = processor.getLfoManager();
            {
                const juce::ScopedLock lock(manager.getLfoDataLock());
                auto& routings = manager.getModulationRoutings();
                routings.clear();
                routings.add(routing);
            }

            juce::Component desktopHost;
            ModulationMatrixRow row(
                processor,
                0,
                routing,
                makeRoutingEditSession(processor),
                [](std::uint64_t, ModulationRouting) {});
            desktopHost.setBounds(0, 0, 760, 80);
            row.setBounds(0, 0, 760, 40);
            desktopHost.addAndMakeVisible(row);
            desktopHost.addToDesktop(
                juce::ComponentPeer::windowIsTemporary);
            desktopHost.setVisible(true);
            REQUIRE(row.isShowing());

            auto* amountSlider = findAmountSlider(row);
            REQUIRE(amountSlider != nullptr);
            REQUIRE(amountSlider->isShowing());
            SliderInteractionCapture capture;
            amountSlider->addListener(&capture);

            auto* handler = amountSlider->getAccessibilityHandler();
            REQUIRE(handler != nullptr);
            CHECK(handler->getRole() == juce::AccessibilityRole::slider);
            CHECK(handler->getTitle() == "Modulation routing 1 amount");
            CHECK(handler->getHelp() == amountSlider->getTooltip());
            auto* value = handler->getValueInterface();
            REQUIRE(value != nullptr);
            CHECK_FALSE(value->isReadOnly());
            CHECK(value->getCurrentValue() == Catch::Approx(0.25));
            CHECK(value->getRange().getMinimumValue()
                  == Catch::Approx(-1.0));
            CHECK(value->getRange().getMaximumValue()
                  == Catch::Approx(1.0));
            CHECK(value->getRange().getInterval()
                  == Catch::Approx(0.01));

            if (boundaryIndex == 0)
                amountSlider->setVisible(false);
            else if (boundaryIndex == 1)
                amountSlider->setEnabled(false);
            else
                desktopHost.removeFromDesktop();

            if (boundaryIndex == 1)
            {
                REQUIRE_FALSE(amountSlider->isEnabled());
                REQUIRE(amountSlider->isShowing());
            }
            else
            {
                REQUIRE_FALSE(amountSlider->isShowing());
            }

            value->setValue(0.8);
            value->setValueAsString("-0.5");

            CHECK(amountSlider->getValue() == Catch::Approx(0.25));
            CHECK(capture.valueChangeCount == 0);
            CHECK(capture.dragStartCount == 0);
            CHECK(capture.dragEndCount == 0);
            const auto liveRoutings = manager.getModulationRoutingsCopy();
            REQUIRE(liveRoutings.size() == 1);
            CHECK(liveRoutings[0].depth == Catch::Approx(0.25f));

            amountSlider->removeListener(&capture);
            desktopHost.removeFromDesktop();
        }
    }
}
