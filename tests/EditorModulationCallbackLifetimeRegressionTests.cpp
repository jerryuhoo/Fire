#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <new>

namespace
{
template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;

    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
        if (auto* child = root.getChildComponent(childIndex))
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;

    return nullptr;
}

class QuarantinedEditor final : public FireAudioProcessorEditor
{
public:
    explicit QuarantinedEditor(FireAudioProcessor& processorToEdit)
        : FireAudioProcessorEditor(processorToEdit)
    {
    }

    static void operator delete(void* storage) noexcept;

    static bool tombstoneIsIntact()
    {
        if (destroyedStorage == nullptr)
            return false;

        const auto* first =
            static_cast<const unsigned char*>(destroyedStorage);
        return std::all_of(first,
                           first + sizeof(QuarantinedEditor),
                           [](unsigned char byte)
                           {
                               return byte == tombstoneByte;
                           });
    }

    static void releaseDestroyedStorage() noexcept
    {
        ::operator delete(destroyedStorage);
        destroyedStorage = nullptr;
    }

private:
    static inline void* destroyedStorage = nullptr;
    static constexpr unsigned char tombstoneByte = 0xa5;
};

void QuarantinedEditor::operator delete(void* storage) noexcept
{
    destroyedStorage = storage;
    std::memset(destroyedStorage,
                tombstoneByte,
                sizeof(QuarantinedEditor));
}

struct QuarantinedStorageGuard
{
    ~QuarantinedStorageGuard()
    {
        QuarantinedEditor::releaseDestroyedStorage();
    }
};

class DestroyEditorOnHostNotification final
    : public juce::AudioProcessorListener
{
public:
    DestroyEditorOnHostNotification(
        FireAudioProcessor& processorToObserve,
        std::unique_ptr<FireAudioProcessorEditor>& editorToDestroy)
        : processor(processorToObserve), editor(editorToDestroy)
    {
        processor.addListener(this);
    }

    ~DestroyEditorOnHostNotification() override
    {
        processor.removeListener(this);
        editor.reset();
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
        editor.reset();
        callbackCompleted = true;
    }

    bool tombstoneIsIntact() const
    {
        return QuarantinedEditor::tombstoneIsIntact();
    }

    FireAudioProcessor& processor;
    std::unique_ptr<FireAudioProcessorEditor>& editor;
    int notificationCount = 0;
    bool callbackCompleted = false;
};
} // namespace

TEST_CASE("Editor modulation callbacks stop after synchronous host deletion",
          "[ui][editor][modulation][lifecycle][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    const auto targetParameterID =
        ParameterIDAndName::getIDString(DRIVE_ID, 0);
    processor.assignLfoToTarget(0, targetParameterID);
    processor.setModulationDepth(targetParameterID, -0.37f);

    [[maybe_unused]] QuarantinedStorageGuard releaseQuarantinedStorage;
    std::unique_ptr<FireAudioProcessorEditor> editor =
        std::make_unique<QuarantinedEditor>(processor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    REQUIRE(bandPanel != nullptr);
    auto* slider = bandPanel->getDriveKnob();
    REQUIRE(slider != nullptr);
    REQUIRE(slider->getParamID() == targetParameterID);

    std::function<void()> invokeCallback;

    SECTION("value-entry popup confirmation")
    {
        const auto requestCallback = slider->onSetValueRequested;
        REQUIRE(static_cast<bool>(requestCallback));
        requestCallback(slider, targetParameterID);

        auto* popup = findDescendant<ValueEntryPopup>(*editor);
        REQUIRE(popup != nullptr);
        const auto callback = popup->onOk;
        REQUIRE(static_cast<bool>(callback));
        auto* parameter = processor.treeState.getParameter(targetParameterID);
        REQUIRE(parameter != nullptr);
        const auto endpoint = parameter->convertFrom0to1(1.0f);
        invokeCallback = [callback, endpoint] { callback(endpoint); };
    }

    SECTION("one-shot Assign slider click")
    {
        auto* lfoPanel = findDescendant<LfoPanel>(*editor);
        REQUIRE(lfoPanel != nullptr);
        const auto enterAssignMode = lfoPanel->onAssignButtonClicked;
        REQUIRE(static_cast<bool>(enterAssignMode));
        enterAssignMode(2);

        const auto callback = slider->onClickInAssignMode;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = [callback, targetParameterID]
        {
            callback(targetParameterID);
        };
    }

    SECTION("modulation bypass")
    {
        const auto callback = slider->onBypassToggled;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = [callback, targetParameterID]
        {
            callback(targetParameterID);
        };
    }

    SECTION("typed modulation endpoint")
    {
        const auto callback = slider->onModAmountSetValue;
        REQUIRE(static_cast<bool>(callback));
        auto* parameter = processor.treeState.getParameter(targetParameterID);
        REQUIRE(parameter != nullptr);
        const auto endpoint = parameter->convertFrom0to1(1.0f);
        invokeCallback = [callback, endpoint] { callback(endpoint); };
    }

    SECTION("context-menu LFO assignment")
    {
        const auto callback = slider->onLfoAssignmentRequested;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = [callback, targetParameterID]
        {
            callback(3, targetParameterID);
        };
    }

    SECTION("clear modulation")
    {
        const auto callback = slider->onModulationCleared;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = [callback, targetParameterID]
        {
            callback(targetParameterID);
        };
    }

    SECTION("invert modulation")
    {
        const auto callback = slider->onModulationInverted;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = [callback, targetParameterID]
        {
            callback(targetParameterID);
        };
    }

    SECTION("toggle bipolar mode")
    {
        const auto callback = slider->onBipolarModeToggled;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = [callback, targetParameterID]
        {
            callback(targetParameterID);
        };
    }

    SECTION("drag modulation depth")
    {
        const auto callback = slider->onModAmountChanged;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = [callback] { callback(0.81f); };
    }

    SECTION("reset modulation")
    {
        const auto callback = slider->onModulationReset;
        REQUIRE(static_cast<bool>(callback));
        invokeCallback = callback;
    }

    REQUIRE(static_cast<bool>(invokeCallback));

    DestroyEditorOnHostNotification deleteOnChange(processor, editor);
    invokeCallback();

    CHECK(deleteOnChange.notificationCount == 1);
    CHECK(deleteOnChange.callbackCompleted);
    CHECK(editor == nullptr);
    CHECK(deleteOnChange.tombstoneIsIntact());
}
