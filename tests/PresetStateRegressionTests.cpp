#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Panels/TopPanel/Preset.h"
#include "helpers/ScopedNumericLocale.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

struct StateComponentMenuTestAccess
{
    static std::function<void(int)> createResultHandler(
        state::StateComponent& component)
    {
        return component.createPresetMenuResultHandler();
    }

    static void handleResult(state::StateComponent& component, int result)
    {
        component.handlePresetMenuResult(result);
    }

    static void rescan(state::StateComponent& component)
    {
        component.rescanPresetFolder();
    }
};

struct StateComponentManualUpdateTestAccess
{
    struct PresentedAlert
    {
        juce::String title;
        juce::String message;
        juce::StringArray buttons;
        juce::Component* associatedComponent = nullptr;
        juce::Component* parentComponent = nullptr;
        std::function<void(int)> complete;
        int closeCount = 0;
    };

    using AlertPtr = std::shared_ptr<PresentedAlert>;

    static void installDialogSeam(
        state::StateComponent& component,
        std::vector<AlertPtr>& presentedAlerts,
        std::vector<juce::String>& launchedUrls)
    {
        component.manualUpdateDialogPresenterForTesting =
            [&presentedAlerts](const juce::MessageBoxOptions& options,
                               std::function<void(int)> complete)
        {
            auto alert = std::make_shared<PresentedAlert>();
            alert->title = options.getTitle();
            alert->message = options.getMessage();
            for (int i = 0; i < options.getNumButtons(); ++i)
                alert->buttons.add(options.getButtonText(i));
            alert->associatedComponent = options.getAssociatedComponent();
            alert->parentComponent = options.getParentComponent();
            alert->complete = std::move(complete);
            presentedAlerts.push_back(alert);
            return std::function<void()> { [alert]
            {
                ++alert->closeCount;
            } };
        };
        component.manualUpdateUrlLauncherForTesting =
            [&launchedUrls](const juce::URL& url)
        {
            launchedUrls.push_back(url.toString(false));
        };
    }

    static std::uint64_t beginRequest(state::StateComponent& component)
    {
        return component.beginManualUpdateRequest();
    }

    static void publishNetworkFailure(state::StateComponent& component,
                                      std::uint64_t generation)
    {
        component.publishManualUpdateResult(nullptr, generation);
    }

    static void flushPendingResult(state::StateComponent& component)
    {
        component.handleUpdateNowIfNeeded();
    }

    static void showDownloadPrompt(state::StateComponent& component,
                                   const juce::String& version)
    {
        component.showManualUpdateAlert(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("New Version")
                .withMessage("New version " + version + " available")
                .withButton("OK")
                .withButton("Cancel"),
            version);
    }

    static bool hasActiveRequest(state::StateComponent& component)
    {
        const juce::ScopedLock lock(component.updateResultLock);
        return component.manualUpdateRequestActive;
    }

    static bool hasPendingResult(state::StateComponent& component)
    {
        const juce::ScopedLock lock(component.updateResultLock);
        return component.pendingManualUpdateRequestGeneration != 0
               || component.versionCheckReady.load(std::memory_order_acquire);
    }

    static bool hasActiveAlert(const state::StateComponent& component)
    {
        return component.manualUpdateAlertActive;
    }
};

struct StateComponentPresetBoxTestAccess
{
    static std::function<void(int)> createResultHandler(
        state::StateComponent& component)
    {
        auto& presetBox = component.presetBox;
        return presetBox.createPopupResultHandler(
            presetBox.popupContextRevision);
    }

    static bool isPopupRequestArmed(
        const state::StateComponent& component)
    {
        return component.presetBox.popupRequestArmed;
    }

    static bool isPopupSessionActive(
        const state::StateComponent& component)
    {
        return component.presetBox.popupSessionActive;
    }

    static bool hasActivePointerInteraction(
        const state::StateComponent& component)
    {
        return component.presetBox.pointerInteractionActive;
    }

    static void selectNextPreset(state::StateComponent& component)
    {
        component.setNextPreset();
    }

    static void setPopupCloser(state::StateComponent& component,
                               std::function<void()> closer)
    {
        component.presetBox.popupCloserForTesting = std::move(closer);
    }

    static void notifyParentHierarchyChanged(
        state::StateComponent& component)
    {
        component.presetBox.parentHierarchyChanged();
    }
};

struct StateComponentSaveChooserTestAccess
{
    struct Session
    {
        std::shared_ptr<juce::FileChooser> chooser;
        std::uint64_t generation = 0;
        std::function<void(const juce::File&)> deliverResult;
    };

    static Session beginSession(state::StateComponent& component)
    {
        component.invalidateSaveChooserSession();

        auto chooser = std::make_shared<juce::FileChooser>(
            "Test preset save", juce::File(), "*.fire", false, false,
            &component);
        component.fileChooser = chooser;
        component.fileChooserSessionActive = true;
        const auto generation = ++component.fileChooserSessionGeneration;
        return { chooser,
                 generation,
                 component.createSaveChooserResultHandler(chooser,
                                                           generation) };
    }

    static bool isCurrent(const state::StateComponent& component,
                          const Session& session)
    {
        return component.isSaveChooserSessionCurrent(
            session.generation, session.chooser.get());
    }

    static bool hasOwnedChooser(const state::StateComponent& component)
    {
        return component.fileChooser != nullptr;
    }
};

struct StateComponentSaveErrorAlertTestAccess
{
    struct PresentedAlert
    {
        juce::String title;
        juce::String message;
        juce::Component* associatedComponent = nullptr;
        juce::Component* parentComponent = nullptr;
        std::function<void(int)> complete;
        int closeCount = 0;
    };

    using AlertPtr = std::shared_ptr<PresentedAlert>;
    using Presenter = std::function<std::function<void()>(
        const juce::MessageBoxOptions&,
        std::function<void(int)>)>;

    static void installDialogSeam(
        state::StateComponent& component,
        std::vector<AlertPtr>& presentedAlerts)
    {
        component.saveErrorDialogPresenterForTesting =
            [&presentedAlerts](const juce::MessageBoxOptions& options,
                               std::function<void(int)> complete)
        {
            auto alert = std::make_shared<PresentedAlert>();
            alert->title = options.getTitle();
            alert->message = options.getMessage();
            alert->associatedComponent = options.getAssociatedComponent();
            alert->parentComponent = options.getParentComponent();
            alert->complete = std::move(complete);
            presentedAlerts.push_back(alert);
            return std::function<void()> { [alert]
            {
                ++alert->closeCount;
            } };
        };
    }

    static void setDialogPresenter(state::StateComponent& component,
                                   Presenter presenter)
    {
        component.saveErrorDialogPresenterForTesting =
            std::move(presenter);
    }

    static void show(state::StateComponent& component,
                     juce::String message)
    {
        component.showSaveErrorAlert(std::move(message));
    }

    static void launchSave(state::StateComponent& component)
    {
        component.savePresetAlertWindow();
    }

    static bool hasActiveAlert(const state::StateComponent& component)
    {
        return component.saveErrorAlertActive;
    }
};

struct StateComponentSessionBoundaryTestAccess
{
    static void setSettingsDialog(state::StateComponent& component,
                                  juce::DialogWindow* dialog)
    {
        component.settingsDialog = dialog;
    }

    static bool hasSettingsDialog(const state::StateComponent& component)
    {
        return component.settingsDialog != nullptr;
    }

    static void setSettingsDialogFactory(
        state::StateComponent& component,
        std::function<juce::DialogWindow*()> factory)
    {
        component.settingsDialogFactoryForTesting = std::move(factory);
    }

    static void showSettingsDialog(state::StateComponent& component)
    {
        component.showSettingsDialog();
    }
};

namespace
{
class ScopedTemporaryDirectory
{
public:
    ScopedTemporaryDirectory()
        : directory(juce::File::getCurrentWorkingDirectory()
                        .getChildFile("Builds")
                        .getChildFile("TestTemp")
                        .getChildFile("FirePresetStateTests-" + juce::Uuid().toString()))
    {
        directoryWasCreated = directory.createDirectory().wasOk();
    }

    ~ScopedTemporaryDirectory()
    {
        if (directoryWasCreated)
            directory.deleteRecursively(false);
    }

    bool wasCreated() const noexcept { return directoryWasCreated; }

    juce::File directory;

private:
    bool directoryWasCreated = false;
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(
        parameter->getNormalisableRange().convertTo0to1(plainValue));
}

float getPlainParameter(const FireAudioProcessor& processor,
                        const juce::String& parameterID)
{
    const auto* value = processor.treeState.getRawParameterValue(parameterID);
    REQUIRE(value != nullptr);
    return value->load(std::memory_order_relaxed);
}

juce::MouseEvent makePresetBoxMouseEvent(
    juce::Component& component,
    juce::ModifierKeys modifiers,
    bool wasDragged = false)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
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
             wasDragged };
}

LfoData makeLfoShape(float middleX, float middleY, float smoothness)
{
    LfoData shape;
    shape.points = { { 0.0f, 0.15f }, { middleX, middleY }, { 1.0f, 0.35f } };
    shape.curvatures = { 0.4f, -0.6f };
    shape.smoothness = smoothness;
    shape.sanitise();
    return shape;
}

const ModulationRouting* findRouting(const juce::Array<ModulationRouting>& routings,
                                     const juce::String& target)
{
    return std::find_if(routings.begin(), routings.end(), [&](const auto& routing)
                        { return routing.targetParameterID == target; });
}

class ParameterTriggeredStateCapture final : public juce::AudioProcessorListener
{
public:
    ParameterTriggeredStateCapture(FireAudioProcessor& processorToObserve,
                                   int parameterIndexToCapture)
        : processor(processorToObserve),
          parameterIndex(parameterIndexToCapture)
    {
        processor.addListener(this);
    }

    ~ParameterTriggeredStateCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int changedParameterIndex,
                                        float) override
    {
        if (captured || changedParameterIndex != parameterIndex)
            return;

        captured = true;
        processor.getStateInformation(state);
    }

    void audioProcessorChanged(juce::AudioProcessor*,
                               const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    FireAudioProcessor& processor;
    juce::MemoryBlock state;
    int parameterIndex = -1;
    bool captured = false;
};

class NonParameterStateCapture final : public juce::AudioProcessorListener
{
public:
    explicit NonParameterStateCapture(FireAudioProcessor& processorToObserve)
        : processor(processorToObserve)
    {
        processor.addListener(this);
    }

    ~NonParameterStateCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int,
                                        float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (! details.nonParameterStateChanged || captureInProgress)
            return;

        const juce::ScopedValueSetter<bool> captureGuard(captureInProgress, true);
        generations.push_back(
            processor.getMultibandTopologyGenerationForTesting());
        states.emplace_back();
        processor.getStateInformation(states.back());
    }

    FireAudioProcessor& processor;
    std::vector<juce::MemoryBlock> states;
    std::vector<std::uint32_t> generations;
    bool captureInProgress = false;
};

class ParameterTriggeredStateHistory final : public juce::AudioProcessorListener
{
public:
    ParameterTriggeredStateHistory(FireAudioProcessor& processorToObserve,
                                   int parameterIndexToCapture)
        : processor(processorToObserve),
          parameterIndex(parameterIndexToCapture)
    {
        processor.addListener(this);
    }

    ~ParameterTriggeredStateHistory() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int changedParameterIndex,
                                        float) override
    {
        if (captureInProgress || changedParameterIndex != parameterIndex)
            return;

        const juce::ScopedValueSetter<bool> captureGuard(captureInProgress, true);
        states.emplace_back();
        processor.getStateInformation(states.back());
    }

    void audioProcessorChanged(juce::AudioProcessor*,
                               const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    FireAudioProcessor& processor;
    std::vector<juce::MemoryBlock> states;
    int parameterIndex = -1;
    bool captureInProgress = false;
};

class ThrowingParameterListener final : public juce::AudioProcessorListener
{
public:
    ThrowingParameterListener(FireAudioProcessor& processorToObserve,
                              int parameterIndexToThrow)
        : processor(processorToObserve),
          parameterIndex(parameterIndexToThrow)
    {
        processor.addListener(this);
    }

    ~ThrowingParameterListener() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int changedParameterIndex,
                                        float) override
    {
        if (throwOnNextChange && changedParameterIndex == parameterIndex)
        {
            throwOnNextChange = false;
            throw std::runtime_error("test listener failure");
        }
    }

    void audioProcessorChanged(juce::AudioProcessor*,
                               const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    FireAudioProcessor& processor;
    int parameterIndex = -1;
    bool throwOnNextChange = true;
};

enum class HostDeletionTrigger
{
    parameterChange,
    nonParameterChange
};

template <typename Owner>
class DeleteUiOnHostNotification final
    : public juce::AudioProcessorListener
{
public:
    DeleteUiOnHostNotification(FireAudioProcessor& processorToObserve,
                               std::unique_ptr<Owner>& ownerToDelete,
                               HostDeletionTrigger triggerToUse)
        : processor(processorToObserve),
          owner(ownerToDelete),
          trigger(triggerToUse)
    {
        processor.addListener(this);
    }

    ~DeleteUiOnHostNotification() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
        if (trigger == HostDeletionTrigger::parameterChange)
            deleteOwnerOnce();
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (trigger == HostDeletionTrigger::nonParameterChange
            && details.nonParameterStateChanged)
            deleteOwnerOnce();
    }

    FireAudioProcessor& processor;
    std::unique_ptr<Owner>& owner;
    HostDeletionTrigger trigger;
    int callbackCount = 0;
    bool callbackCompleted = false;

private:
    void deleteOwnerOnce()
    {
        if (callbackCount != 0)
            return;

        ++callbackCount;
        owner.reset();
        callbackCompleted = true;
    }
};

template <typename ComponentType>
ComponentType* findComponentOfType(juce::Component& root)
{
    if (auto* component = dynamic_cast<ComponentType*>(&root))
        return component;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* child = root.getChildComponent(index))
            if (auto* component = findComponentOfType<ComponentType>(*child))
                return component;

    return nullptr;
}

int getParameterIndex(const FireAudioProcessor& processor,
                      const juce::String& parameterID)
{
    const auto& parameters = processor.getParameters();
    for (int index = 0; index < parameters.size(); ++index)
    {
        const auto* parameter = dynamic_cast<const juce::AudioProcessorParameterWithID*>(
            parameters[index]);
        if (parameter != nullptr && parameter->getParameterID() == parameterID)
            return index;
    }

    return -1;
}

int countRoutingsForTarget(const juce::Array<ModulationRouting>& routings,
                           const juce::String& target)
{
    return static_cast<int>(std::count_if(routings.begin(), routings.end(), [&](const auto& routing)
                                          { return routing.targetParameterID == target; }));
}

void appendRouting(juce::XmlElement& routingParent,
                   int source,
                   const juce::String& target,
                   float depth)
{
    auto* routing = routingParent.createNewChildElement("ROUTING");
    routing->setAttribute("source", source);
    routing->setAttribute("target", target);
    routing->setAttribute("depth", depth);
    routing->setAttribute("bipolar", true);
    routing->setAttribute("bypassed", false);
}

void writePresetFile(FireAudioProcessor& processor,
                     const juce::File& file,
                     const juce::String& name)
{
    juce::XmlElement xml { "WINGSFIRE" };
    xml.setAttribute("presetName", name);
    state::saveStateToXml(processor, xml);
    REQUIRE(xml.writeTo(file));
}

juce::XmlElement* findHostParameter(juce::XmlElement& stateXml,
                                    FireAudioProcessor& processor,
                                    const juce::String& parameterID)
{
    auto* parameterState = stateXml.getChildByName(
        processor.treeState.state.getType().toString());
    REQUIRE(parameterState != nullptr);

    for (auto* child : parameterState->getChildIterator())
        if (child->getStringAttribute("id") == parameterID)
            return child;

    return nullptr;
}

juce::XmlElement* findHostLfo(juce::XmlElement& stateXml, int lfoIndex)
{
    auto* lfoState = stateXml.getChildByName("LFO_STATE");
    REQUIRE(lfoState != nullptr);
    for (auto* child : lfoState->getChildIterator())
        if (child->hasTagName("LFO")
            && child->getIntAttribute("index", -1) == lfoIndex)
            return child;

    return nullptr;
}

int countValueTreeChildrenWithID(const juce::ValueTree& tree,
                                 const juce::String& parameterID)
{
    int count = 0;
    for (const auto& child : tree)
        if (child.getProperty("id").toString() == parameterID)
            ++count;

    return count;
}

juce::File findProjectRoot()
{
    auto candidate = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                         .getParentDirectory();
    while (candidate != candidate.getParentDirectory())
    {
        if (candidate.getChildFile("CMakeLists.txt").existsAsFile()
            && candidate.getChildFile("tests/Presets").isDirectory())
            return candidate;

        candidate = candidate.getParentDirectory();
    }

    return {};
}
} // namespace

TEST_CASE("Preset files round-trip parameters, multiband state, LFOs and routings headlessly",
          "[preset][state][roundtrip][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());
    FireAudioProcessor processor;
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto frequencyID = ParameterIDAndName::getIDString(FREQ_ID, 0);
    setPlainParameter(processor, NUM_BANDS_ID, 2.0f);
    setPlainParameter(processor, frequencyID, 1375.0f);
    setPlainParameter(processor, driveID, 24.0f);

    const auto savedShape = makeLfoShape(0.42f, 0.91f, 0.67f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 2),
                      savedShape.smoothness);
    processor.getLfoManager().setLfoData(2, savedShape);
    processor.assignLfoToTarget(2, driveID);
    processor.setModulationDepth(driveID, -0.37f);

    const auto savePath = temporaryDirectory.directory.getChildFile("Roundtrip.fire");
    REQUIRE(presets.savePreset(savePath) == "Roundtrip");

    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, frequencyID, 8000.0f);
    setPlainParameter(processor, driveID, 6.0f);
    processor.getLfoManager().setLfoData(2, LfoData {});
    processor.clearModulationForParameter(driveID);

    juce::ComboBox menu;
    presets.setPresetAndFolderNames(menu);
    const int presetID = presets.getCurrentPresetId();
    REQUIRE(presetID > 0);
    const auto presetTag = presets.comboBoxIdToTagNameMap[presetID];
    REQUIRE(presetTag.isNotEmpty());
    NonParameterStateCapture hostNotification(processor);
    REQUIRE(presets.loadPreset(presetTag));
    REQUIRE(hostNotification.states.size() == 1);

    FireAudioProcessor stateObservedByHost;
    stateObservedByHost.setStateInformation(
        hostNotification.states.front().getData(),
        static_cast<int>(hostNotification.states.front().getSize()));
    CHECK(getPlainParameter(stateObservedByHost, NUM_BANDS_ID)
          == Catch::Approx(2.0f));
    CHECK(getPlainParameter(stateObservedByHost, driveID)
          == Catch::Approx(24.0f));

    CHECK(getPlainParameter(processor, NUM_BANDS_ID) == Catch::Approx(2.0f));
    CHECK(getPlainParameter(processor, frequencyID) == Catch::Approx(1375.0f));
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(24.0f));

    const auto restoredShapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredShapes.size() == 4);
    REQUIRE(restoredShapes[2].points.size() == 3);
    CHECK(restoredShapes[2].points[1].x == Catch::Approx(0.42f));
    CHECK(restoredShapes[2].points[1].y == Catch::Approx(0.91f));
    CHECK(restoredShapes[2].smoothness == Catch::Approx(0.67f));

    const auto restoredRoutings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* restoredRouting = findRouting(restoredRoutings, driveID);
    REQUIRE(restoredRouting != nullptr);
    CHECK(restoredRouting->sourceLfoIndex == 2);
    CHECK(restoredRouting->depth == Catch::Approx(-0.37f));
}

TEST_CASE("Preset save failure is atomic and dotted filenames retain their identity",
          "[preset][filesystem][identity]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());
    FireAudioProcessor processor;
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };

    const auto dottedPath = temporaryDirectory.directory.getChildFile("Lead.v2.fire");
    CHECK(presets.savePreset(dottedPath) == "Lead.v2");
    CHECK(dottedPath.existsAsFile());
    CHECK(presets.getNumPresets() == 1);
    CHECK(presets.getPresetName() == "Lead.v2");

    juce::ComboBox menu;
    presets.setPresetAndFolderNames(menu);
    CHECK(menu.getNumItems() == 1);
    CHECK(menu.getItemText(0) == "Lead.v2");

    const juce::String nameBeforeFailure { presets.getPresetName() };
    const int countBeforeFailure = presets.getNumPresets();
    const auto parentBlocker = temporaryDirectory.directory.getChildFile("not-a-folder");
    REQUIRE(parentBlocker.replaceWithText("This regular file cannot contain a preset."));
    const auto unwritablePath = parentBlocker.getChildFile("CannotSave.fire");
    REQUIRE(parentBlocker.existsAsFile());
    CHECK(presets.savePreset(unwritablePath).isEmpty());
    CHECK_FALSE(unwritablePath.existsAsFile());
    CHECK(presets.getPresetName() == nameBeforeFailure);
    CHECK(presets.getNumPresets() == countBeforeFailure);
}

TEST_CASE("Preset rescans publish only complete counts to concurrent readers",
          "[preset][filesystem][concurrency][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    FireAudioProcessor presetSource;
    constexpr int expectedPresetCount = 32;
    for (int index = 0; index < expectedPresetCount; ++index)
        writePresetFile(
            presetSource,
            temporaryDirectory.directory.getChildFile(
                "Concurrent-" + juce::String(index) + ".fire"),
            "Concurrent-" + juce::String(index));

    state::StatePresets presets {
        presetSource, temporaryDirectory.directory.getFullPathName()
    };
    REQUIRE(presets.getNumPresets() == expectedPresetCount);

    std::atomic<bool> readerReady { false };
    std::atomic<bool> keepReading { true };
    std::atomic<bool> observedPartialCount { false };
    std::thread reader([&]
    {
        readerReady.store(true, std::memory_order_release);
        while (keepReading.load(std::memory_order_acquire))
            if (presets.getNumPresets() != expectedPresetCount)
                observedPartialCount.store(true, std::memory_order_relaxed);
    });

    while (! readerReady.load(std::memory_order_acquire))
        std::this_thread::yield();

    for (int scan = 0; scan < 3; ++scan)
        presets.scanAllPresets();

    keepReading.store(false, std::memory_order_release);
    reader.join();

    CHECK_FALSE(observedPartialCount.load(std::memory_order_relaxed));
    CHECK(presets.getNumPresets() == expectedPresetCount);
}

TEST_CASE("Duplicate preset display names keep their relative-path identity",
          "[preset][filesystem][identity][duplicates]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    const auto folderA = temporaryDirectory.directory.getChildFile("A");
    const auto folderB = temporaryDirectory.directory.getChildFile("B");
    REQUIRE(folderA.createDirectory().wasOk());
    REQUIRE(folderB.createDirectory().wasOk());

    FireAudioProcessor presetA;
    FireAudioProcessor presetB;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(presetA, driveID, 17.0f);
    setPlainParameter(presetB, driveID, 63.0f);
    writePresetFile(presetA, folderA.getChildFile("Twin.fire"), "Twin");
    writePresetFile(presetB, folderB.getChildFile("Twin.fire"), "Twin");

    FireAudioProcessor processor;
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };
    juce::ComboBox initialMenu;
    presets.setPresetAndFolderNames(initialMenu);
    REQUIRE(presets.getNumPresets() == 2);

    int folderAPresetID = 0;
    for (int id = 1; id <= presets.getNumPresets(); ++id)
    {
        presets.loadPreset(presets.comboBoxIdToTagNameMap[id]);
        if (presets.getCurrentPresetKey() == "A/Twin.fire")
        {
            folderAPresetID = id;
            break;
        }
    }

    REQUIRE(folderAPresetID > 0);
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(17.0f));
    presets.setCurrentPresetId(folderAPresetID);

    presets.scanAllPresets();
    juce::ComboBox rescannedMenu;
    presets.setPresetAndFolderNames(rescannedMenu);

    CHECK(presets.getCurrentPresetKey() == "A/Twin.fire");
    CHECK(presets.getCurrentPresetId() == folderAPresetID);
}

TEST_CASE("Preset identity preserves legal leading filename spaces",
          "[preset][filesystem][identity][whitespace][duplicates][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    FireAudioProcessor plainPreset;
    FireAudioProcessor spacedPreset;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(plainPreset, driveID, 19.0f);
    setPlainParameter(spacedPreset, driveID, 71.0f);
    writePresetFile(
        plainPreset,
        temporaryDirectory.directory.getChildFile("Twin.fire"),
        "Twin");
    writePresetFile(
        spacedPreset,
        temporaryDirectory.directory.getChildFile(" Twin.fire"),
        " Twin");

    FireAudioProcessor processor;
    state::StatePresets presets {
        processor, temporaryDirectory.directory.getFullPathName()
    };
    juce::ComboBox menu;
    presets.setPresetAndFolderNames(menu);
    REQUIRE(presets.getNumPresets() == 2);

    int plainPresetID = 0;
    int spacedPresetID = 0;
    for (int id = 1; id <= presets.getNumPresets(); ++id)
    {
        REQUIRE(presets.loadPreset(presets.comboBoxIdToTagNameMap[id]));
        const auto key = presets.getCurrentPresetKey();
        if (key == "Twin.fire")
        {
            plainPresetID = id;
            CHECK(getPlainParameter(processor, driveID)
                  == Catch::Approx(19.0f));
        }
        else if (key == " Twin.fire")
        {
            spacedPresetID = id;
            CHECK(getPlainParameter(processor, driveID)
                  == Catch::Approx(71.0f));
        }
    }

    REQUIRE(plainPresetID > 0);
    REQUIRE(spacedPresetID > 0);
    REQUIRE(plainPresetID != spacedPresetID);

    presets.setCurrentPresetId(spacedPresetID);
    REQUIRE(presets.loadPreset(
        presets.comboBoxIdToTagNameMap[spacedPresetID]));
    REQUIRE(presets.getCurrentPresetKey() == " Twin.fire");

    presets.scanAllPresets();
    juce::ComboBox rescannedMenu;
    presets.setPresetAndFolderNames(rescannedMenu);

    CHECK(presets.getCurrentPresetKey() == " Twin.fire");
    CHECK(presets.getCurrentPresetId() > 0);
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(71.0f));

    // Host-state restoration must retain the same exact path identity before
    // the menu has been rebuilt.
    presets.setCurrentPresetKey(" Twin.fire");
    juce::ComboBox restoredMenu;
    presets.setPresetAndFolderNames(restoredMenu);
    CHECK(presets.getCurrentPresetKey() == " Twin.fire");
    CHECK(presets.getCurrentPresetId() > 0);
}

TEST_CASE("Preset UI synchronisation reflects restored identity without reloading live state",
          "[preset][state][ui][identity][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 34.0f);
    writePresetFile(processor,
                    temporaryDirectory.directory.getChildFile("Sync.fire"),
                    "Sync");

    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };
    presets.setCurrentPresetKey("Sync.fire");
    state::StateComponent component { processor.stateAB, presets, processor.treeState };
    component.synchronisePresetSelectionFromManager();

    auto* presetBox = component.getPresetBox();
    REQUIRE(presetBox != nullptr);
    REQUIRE(presets.getCurrentPresetId() == 1);
    CHECK(presetBox->getSelectedId() == 1);
    CHECK(presetBox->getText() == "Sync");
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(34.0f));

    // Reconciliation is display-only. A differing live state must be marked
    // dirty rather than silently reloading the on-disk preset.
    setPlainParameter(processor, driveID, 61.0f);
    component.synchronisePresetSelectionFromManager();
    CHECK(presetBox->getSelectedId() == 0);
    CHECK(presetBox->getText() == "Sync*");
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(61.0f));
}

TEST_CASE("Preset rescan compares external files without replacing the live sound",
          "[preset][filesystem][ui][rescan][dirty][identity][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    const auto presetPath =
        temporaryDirectory.directory.getChildFile("Sync.fire");
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    FireAudioProcessor originalPreset;
    setPlainParameter(originalPreset, driveID, 34.0f);
    writePresetFile(originalPreset, presetPath, "Sync");

    FireAudioProcessor processor;
    setPlainParameter(processor, driveID, 34.0f);
    processor.statePresets.setPresetDirectoryForTesting(
        temporaryDirectory.directory);
    processor.statePresets.setCurrentPresetKey("Sync.fire");
    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.synchronisePresetSelectionFromManager();

    auto* presetBox = component.getPresetBox();
    REQUIRE(presetBox != nullptr);
    REQUIRE(presetBox->getSelectedId() == 1);
    REQUIRE(presetBox->getText() == "Sync");

    SECTION("a semantic no-op rewrite remains clean")
    {
        FireAudioProcessor equivalentPreset;
        setPlainParameter(equivalentPreset, driveID, 34.0f);
        writePresetFile(equivalentPreset, presetPath, "Different metadata");

        StateComponentMenuTestAccess::rescan(component);

        CHECK(presetBox->getSelectedId() == 1);
        CHECK(presetBox->getText() == "Sync");
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(34.0f));
    }

    SECTION("an external overwrite marks the unchanged live sound dirty")
    {
        FireAudioProcessor replacementPreset;
        setPlainParameter(replacementPreset, driveID, 73.0f);
        writePresetFile(replacementPreset, presetPath, "Sync");

        StateComponentMenuTestAccess::rescan(component);

        CHECK(presetBox->getSelectedId() == 0);
        CHECK(presetBox->getText() == "Sync*");
        CHECK(processor.statePresets.getCurrentPresetKey() == "Sync.fire");
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(34.0f));
    }

    SECTION("delete and recreate cannot erase an existing dirty indication")
    {
        REQUIRE(presetPath.deleteFile());
        StateComponentMenuTestAccess::rescan(component);

        REQUIRE(presetBox->getSelectedId() == 0);
        REQUIRE(presetBox->getText() == "Sync*");
        REQUIRE(processor.statePresets.getCurrentPresetKey() == "Sync.fire");
        REQUIRE(getPlainParameter(processor, driveID)
                == Catch::Approx(34.0f));

        FireAudioProcessor recreatedPreset;
        setPlainParameter(recreatedPreset, driveID, 34.0f);
        writePresetFile(recreatedPreset, presetPath, "Sync");
        StateComponentMenuTestAccess::rescan(component);

        CHECK(presetBox->getSelectedId() == 0);
        CHECK(presetBox->getText() == "Sync*");
        CHECK(processor.statePresets.getCurrentPresetKey() == "Sync.fire");
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(34.0f));
    }
}

TEST_CASE("Preset selection publishes identity and sound as one host generation",
          "[preset][state][host][identity][topology][transaction][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    const auto firstDrive = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto secondDrive = ParameterIDAndName::getIDString(DRIVE_ID, 1);

    FireAudioProcessor oldPreset;
    setPlainParameter(oldPreset, NUM_BANDS_ID, 2.0f);
    setPlainParameter(oldPreset, firstDrive, 11.0f);
    setPlainParameter(oldPreset, secondDrive, 77.0f);
    oldPreset.assignLfoToTarget(0, secondDrive);
    oldPreset.setModulationDepth(secondDrive, 0.37f);
    writePresetFile(oldPreset,
                    temporaryDirectory.directory.getChildFile("Old.fire"),
                    "Old");

    FireAudioProcessor newPreset;
    setPlainParameter(newPreset, NUM_BANDS_ID, 1.0f);
    setPlainParameter(newPreset, firstDrive, 77.0f);
    newPreset.assignLfoToTarget(0, firstDrive);
    newPreset.setModulationDepth(firstDrive, 0.37f);
    writePresetFile(newPreset,
                    temporaryDirectory.directory.getChildFile("New.fire"),
                    "New");

    FireAudioProcessor subject;
    juce::MemoryBlock oldHostState;
    oldPreset.getStateInformation(oldHostState);
    subject.setStateInformation(oldHostState.getData(),
                                static_cast<int>(oldHostState.getSize()));
    subject.statePresets.setPresetDirectoryForTesting(
        temporaryDirectory.directory);
    subject.statePresets.setCurrentPresetKey("Old.fire");

    state::StateComponent component {
        subject.stateAB, subject.statePresets, subject.treeState
    };
    component.synchronisePresetSelectionFromManager();
    auto* presetBox = component.getPresetBox();
    REQUIRE(presetBox != nullptr);

    int newPresetID = 0;
    for (int id = 1; id <= subject.statePresets.getNumPresets(); ++id)
        if (presetBox->getItemText(presetBox->indexOfItemId(id)) == "New")
            newPresetID = id;
    REQUIRE(newPresetID > 0);
    REQUIRE(subject.statePresets.getCurrentPresetKey() == "Old.fire");

    const int bandCountIndex = getParameterIndex(subject, NUM_BANDS_ID);
    REQUIRE(bandCountIndex >= 0);
    ParameterTriggeredStateCapture parameterHost(subject, bandCountIndex);
    NonParameterStateCapture committedHost(subject);
    component.updatePresetBox(newPresetID);
    REQUIRE(parameterHost.captured);
    REQUIRE(parameterHost.state.getSize() > 0);
    REQUIRE(committedHost.states.size() == 1);

    FireAudioProcessor restored;
    restored.setStateInformation(
        parameterHost.state.getData(),
        static_cast<int>(parameterHost.state.getSize()));
    const auto savedRoutings =
        restored.getLfoManager().getModulationRoutingsCopy();
    CHECK(getPlainParameter(restored, NUM_BANDS_ID) == Catch::Approx(2.0f));
    CHECK(getPlainParameter(restored, firstDrive) == Catch::Approx(11.0f));
    CHECK(countRoutingsForTarget(savedRoutings, secondDrive) == 1);
    CHECK(countRoutingsForTarget(savedRoutings, firstDrive) == 0);
    CHECK(restored.statePresets.getCurrentPresetKey() == "Old.fire");

    FireAudioProcessor committed;
    const auto& committedState = committedHost.states.front();
    committed.setStateInformation(committedState.getData(),
                                  static_cast<int>(committedState.getSize()));
    const auto committedRoutings =
        committed.getLfoManager().getModulationRoutingsCopy();
    CHECK(getPlainParameter(committed, NUM_BANDS_ID) == Catch::Approx(1.0f));
    CHECK(getPlainParameter(committed, firstDrive) == Catch::Approx(77.0f));
    CHECK(countRoutingsForTarget(committedRoutings, firstDrive) == 1);
    CHECK(countRoutingsForTarget(committedRoutings, secondDrive) == 0);
    CHECK(committed.statePresets.getCurrentPresetKey() == "New.fire");

    CHECK(getPlainParameter(subject, NUM_BANDS_ID) == Catch::Approx(1.0f));
    CHECK(subject.statePresets.getCurrentPresetKey() == "New.fire");
}

TEST_CASE("Preset selection survives synchronous editor deletion by the host",
          "[preset][ui][host][lifetime][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    FireAudioProcessor presetSource;
    setPlainParameter(presetSource, driveID, 73.0f);
    writePresetFile(presetSource,
                    temporaryDirectory.directory.getChildFile("Delete.fire"),
                    "Delete");

    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    setPlainParameter(processor, driveID, 11.0f);
    processor.statePresets.setPresetDirectoryForTesting(
        temporaryDirectory.directory);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    auto* stateComponent =
        findComponentOfType<state::StateComponent>(*editor);
    REQUIRE(stateComponent != nullptr);
    auto* presetBox = stateComponent->getPresetBox();
    REQUIRE(presetBox != nullptr);
    REQUIRE(presetBox->getNumItems() == 1);

    SECTION("parameter notification during preset load")
    {
        NonParameterStateCapture committedHost(processor);
        DeleteUiOnHostNotification<FireAudioProcessorEditor> deleteOnChange(
            processor, editor, HostDeletionTrigger::parameterChange);
        presetBox->setSelectedId(1, juce::sendNotificationSync);

        CHECK(deleteOnChange.callbackCount == 1);
        CHECK(deleteOnChange.callbackCompleted);
        CHECK(editor == nullptr);
        CHECK_FALSE(processor.isMultibandTopologyEditInProgress());
        CHECK((processor.getMultibandTopologyGenerationForTesting() & 1u)
              == 0u);
        REQUIRE(committedHost.states.size() == 1);
        CHECK(committedHost.generations.front() % 2u == 0u);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(73.0f));
    }

    SECTION("final non-parameter state notification")
    {
        NonParameterStateCapture committedHost(processor);
        DeleteUiOnHostNotification<FireAudioProcessorEditor> deleteOnChange(
            processor, editor, HostDeletionTrigger::nonParameterChange);
        presetBox->setSelectedId(1, juce::sendNotificationSync);

        CHECK(deleteOnChange.callbackCount == 1);
        CHECK(deleteOnChange.callbackCompleted);
        CHECK(editor == nullptr);
        CHECK_FALSE(processor.isMultibandTopologyEditInProgress());
        CHECK((processor.getMultibandTopologyGenerationForTesting() & 1u)
              == 0u);
        REQUIRE(committedHost.states.size() == 1);
        CHECK(committedHost.generations.front() % 2u == 0u);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(73.0f));
    }
}

TEST_CASE("Preset Init survives synchronous component deletion by the host",
          "[preset][ui][menu][init][host][lifetime][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    auto* driveParameter = processor.treeState.getParameter(driveID);
    REQUIRE(driveParameter != nullptr);
    const auto defaultDrive = driveParameter->getNormalisableRange()
                                  .convertFrom0to1(
                                      driveParameter->getDefaultValue());
    setPlainParameter(processor, driveID, 73.0f);

    auto component = std::make_unique<state::StateComponent>(
        processor.stateAB, processor.statePresets, processor.treeState);
    auto* rawComponent = component.get();

    SECTION("parameter notification during Init")
    {
        NonParameterStateCapture committedHost(processor);
        DeleteUiOnHostNotification<state::StateComponent> deleteOnChange(
            processor, component, HostDeletionTrigger::parameterChange);
        StateComponentMenuTestAccess::handleResult(*rawComponent, 1);

        CHECK(deleteOnChange.callbackCount == 1);
        CHECK(deleteOnChange.callbackCompleted);
        CHECK(component == nullptr);
        CHECK_FALSE(processor.isMultibandTopologyEditInProgress());
        CHECK((processor.getMultibandTopologyGenerationForTesting() & 1u)
              == 0u);
        REQUIRE(committedHost.states.size() == 1);
        CHECK(committedHost.generations.front() % 2u == 0u);
        CHECK(getPlainParameter(processor, driveID)
              == Catch::Approx(defaultDrive));
    }

    SECTION("final non-parameter state notification")
    {
        NonParameterStateCapture committedHost(processor);
        DeleteUiOnHostNotification<state::StateComponent> deleteOnChange(
            processor, component, HostDeletionTrigger::nonParameterChange);
        StateComponentMenuTestAccess::handleResult(*rawComponent, 1);

        CHECK(deleteOnChange.callbackCount == 1);
        CHECK(deleteOnChange.callbackCompleted);
        CHECK(component == nullptr);
        CHECK_FALSE(processor.isMultibandTopologyEditInProgress());
        CHECK((processor.getMultibandTopologyGenerationForTesting() & 1u)
              == 0u);
        REQUIRE(committedHost.states.size() == 1);
        CHECK(committedHost.generations.front() % 2u == 0u);
        CHECK(getPlainParameter(processor, driveID)
              == Catch::Approx(defaultDrive));
    }
}

TEST_CASE("Preset menu rejects hidden and superseded asynchronous results",
          "[preset][ui][menu][session][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    auto* driveParameter = processor.treeState.getParameter(driveID);
    REQUIRE(driveParameter != nullptr);
    const auto defaultDrive = driveParameter->getNormalisableRange()
                                  .convertFrom0to1(
                                      driveParameter->getDefaultValue());

    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(0);
    component.setVisible(true);
    const juce::ScopeGuard removeFromDesktop { [&]
    {
        component.removeFromDesktop();
    } };
    REQUIRE(component.isShowing());

    SECTION("a replacement menu invalidates the older result")
    {
        setPlainParameter(processor, driveID, 73.0f);
        auto staleResult =
            StateComponentMenuTestAccess::createResultHandler(component);
        auto currentResult =
            StateComponentMenuTestAccess::createResultHandler(component);

        staleResult(1);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(73.0f));

        currentResult(1);
        CHECK(getPlainParameter(processor, driveID)
              == Catch::Approx(defaultDrive));

        setPlainParameter(processor, driveID, 61.0f);
        currentResult(1);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(61.0f));
    }

    SECTION("an editor visibility boundary invalidates the pending result")
    {
        setPlainParameter(processor, driveID, 73.0f);
        auto staleResult =
            StateComponentMenuTestAccess::createResultHandler(component);

        component.dismissPointerGestures();
        staleResult(1);

        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(73.0f));
    }

    SECTION("direct hiding invalidates the pending result")
    {
        setPlainParameter(processor, driveID, 73.0f);
        auto staleResult =
            StateComponentMenuTestAccess::createResultHandler(component);

        component.setVisible(false);
        staleResult(1);

        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(73.0f));
    }

    SECTION("disabling rejects and consumes the pending result")
    {
        setPlainParameter(processor, driveID, 73.0f);
        auto staleResult =
            StateComponentMenuTestAccess::createResultHandler(component);

        component.setEnabled(false);
        component.setEnabled(true);
        staleResult(1);

        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(73.0f));

        auto replacementResult =
            StateComponentMenuTestAccess::createResultHandler(component);
        replacementResult(1);
        CHECK(getPlainParameter(processor, driveID)
              == Catch::Approx(defaultDrive));
    }
}

TEST_CASE("Manual update results stay inside one visible request session",
          "[preset][ui][update-check][session][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        component.dismissPointerGestures();
        component.removeFromDesktop();
    } };
    REQUIRE(component.isShowing());

    std::vector<StateComponentManualUpdateTestAccess::AlertPtr> alerts;
    std::vector<juce::String> launchedUrls;
    StateComponentManualUpdateTestAccess::installDialogSeam(
        component, alerts, launchedUrls);

    SECTION("a replacement request rejects the older worker result")
    {
        const auto staleGeneration =
            StateComponentManualUpdateTestAccess::beginRequest(component);
        const auto currentGeneration =
            StateComponentManualUpdateTestAccess::beginRequest(component);

        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, staleGeneration);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);
        CHECK(alerts.empty());
        CHECK(StateComponentManualUpdateTestAccess::hasActiveRequest(
            component));

        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, currentGeneration);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);
        REQUIRE(alerts.size() == 1);
        CHECK(alerts.front()->title == "Error");
        CHECK(alerts.front()->buttons.size() == 1);
        CHECK(alerts.front()->associatedComponent == &component);
        CHECK(alerts.front()->parentComponent == &component);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveRequest(
            component));
        CHECK(StateComponentManualUpdateTestAccess::hasActiveAlert(component));

        alerts.front()->complete(0);
        CHECK(alerts.front()->closeCount == 1);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));

        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, currentGeneration);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);
        CHECK(alerts.size() == 1);
        CHECK(launchedUrls.empty());
    }

    SECTION("hiding clears a result which was already queued")
    {
        const auto staleGeneration =
            StateComponentManualUpdateTestAccess::beginRequest(component);
        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, staleGeneration);
        REQUIRE(StateComponentManualUpdateTestAccess::hasPendingResult(
            component));

        component.setVisible(false);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveRequest(
            component));
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasPendingResult(
            component));
        component.setVisible(true);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);
        CHECK(alerts.empty());

        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, staleGeneration);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);
        CHECK(alerts.empty());
    }

    SECTION("an ancestor hide boundary rejects a late worker result")
    {
        const auto staleGeneration =
            StateComponentManualUpdateTestAccess::beginRequest(component);
        component.dismissPointerGestures();
        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, staleGeneration);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);

        CHECK(alerts.empty());
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveRequest(
            component));
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasPendingResult(
            component));
    }

    SECTION("disable-enable cannot revive an old worker request")
    {
        const auto staleGeneration =
            StateComponentManualUpdateTestAccess::beginRequest(component);

        component.setEnabled(false);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveRequest(
            component));
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasPendingResult(
            component));
        component.setEnabled(true);

        const auto replacementGeneration =
            StateComponentManualUpdateTestAccess::beginRequest(component);
        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, staleGeneration);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);
        CHECK(alerts.empty());
        CHECK(StateComponentManualUpdateTestAccess::hasActiveRequest(
            component));

        StateComponentManualUpdateTestAccess::publishNetworkFailure(
            component, replacementGeneration);
        StateComponentManualUpdateTestAccess::flushPendingResult(component);
        REQUIRE(alerts.size() == 1);
        CHECK(StateComponentManualUpdateTestAccess::hasActiveAlert(component));
        alerts.front()->complete(0);
    }
}

TEST_CASE("Manual update alerts close and reject stale completion callbacks",
          "[preset][ui][update-check][alert][session][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        component.dismissPointerGestures();
        component.removeFromDesktop();
    } };
    REQUIRE(component.isShowing());

    std::vector<StateComponentManualUpdateTestAccess::AlertPtr> alerts;
    std::vector<juce::String> launchedUrls;
    StateComponentManualUpdateTestAccess::installDialogSeam(
        component, alerts, launchedUrls);

    SECTION("hide closes the alert and its old OK cannot affect a replacement")
    {
        StateComponentManualUpdateTestAccess::showDownloadPrompt(
            component, "v99.0.0");
        REQUIRE(alerts.size() == 1);
        auto staleAlert = alerts.front();
        CHECK(staleAlert->buttons.size() == 2);
        CHECK(staleAlert->associatedComponent == &component);
        CHECK(staleAlert->parentComponent == &component);
        CHECK(StateComponentManualUpdateTestAccess::hasActiveAlert(component));

        component.setVisible(false);
        CHECK(staleAlert->closeCount == 1);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));

        component.setVisible(true);
        StateComponentManualUpdateTestAccess::showDownloadPrompt(
            component, "v100.0.0");
        REQUIRE(alerts.size() == 2);
        auto replacementAlert = alerts.back();
        REQUIRE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));

        staleAlert->complete(1);
        CHECK(launchedUrls.empty());
        CHECK(replacementAlert->closeCount == 0);
        CHECK(StateComponentManualUpdateTestAccess::hasActiveAlert(component));

        replacementAlert->complete(1);
        REQUIRE(launchedUrls.size() == 1);
        CHECK(launchedUrls.front().contains("v100.0.0"));
        CHECK(replacementAlert->closeCount == 1);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));
    }

    SECTION("ancestor dismissal closes the alert and rejects its OK")
    {
        StateComponentManualUpdateTestAccess::showDownloadPrompt(
            component, "v99.0.0");
        REQUIRE(alerts.size() == 1);
        auto staleAlert = alerts.front();

        component.dismissPointerGestures();
        CHECK(staleAlert->closeCount == 1);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));

        staleAlert->complete(1);
        CHECK(launchedUrls.empty());
        CHECK(staleAlert->closeCount == 1);
    }

    SECTION("disable-enable closes the old alert without harming its replacement")
    {
        StateComponentManualUpdateTestAccess::showDownloadPrompt(
            component, "v99.0.0");
        REQUIRE(alerts.size() == 1);
        auto staleAlert = alerts.front();

        component.setEnabled(false);
        CHECK(staleAlert->closeCount == 1);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));
        component.setEnabled(true);

        StateComponentManualUpdateTestAccess::showDownloadPrompt(
            component, "v100.0.0");
        REQUIRE(alerts.size() == 2);
        auto replacementAlert = alerts.back();
        REQUIRE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));

        staleAlert->complete(1);
        CHECK(launchedUrls.empty());
        CHECK(replacementAlert->closeCount == 0);
        CHECK(StateComponentManualUpdateTestAccess::hasActiveAlert(component));

        replacementAlert->complete(1);
        REQUIRE(launchedUrls.size() == 1);
        CHECK(launchedUrls.front().contains("v100.0.0"));
        CHECK(replacementAlert->closeCount == 1);
        CHECK_FALSE(StateComponentManualUpdateTestAccess::hasActiveAlert(
            component));
    }
}

TEST_CASE("Destroying StateComponent synchronously closes its manual update alert",
          "[preset][ui][update-check][alert][lifetime][destruction][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    std::vector<StateComponentManualUpdateTestAccess::AlertPtr> alerts;
    std::vector<juce::String> launchedUrls;

    auto component = std::make_unique<state::StateComponent>(
        processor.stateAB, processor.statePresets, processor.treeState);
    component->setBounds(0, 0, 800, 48);
    component->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component->setVisible(true);
    REQUIRE(component->isShowing());
    StateComponentManualUpdateTestAccess::installDialogSeam(
        *component, alerts, launchedUrls);
    StateComponentManualUpdateTestAccess::showDownloadPrompt(
        *component, "v99.0.0");
    REQUIRE(alerts.size() == 1);
    auto staleAlert = alerts.front();

    component.reset();
    CHECK(staleAlert->closeCount == 1);

    staleAlert->complete(1);
    CHECK(launchedUrls.empty());
    CHECK(staleAlert->closeCount == 1);
}

TEST_CASE("Preset save chooser results stay inside one visible owner session",
          "[preset][ui][save-chooser][session][identity][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    FireAudioProcessor processor;
    processor.statePresets.setPresetDirectoryForTesting(
        temporaryDirectory.directory);
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    const juce::ScopeGuard removeFromDesktop { [&]
    {
        component.dismissPointerGestures();
        component.removeFromDesktop();
    } };
    REQUIRE(component.isShowing());

    const auto checkSavedDrive = [&](const juce::File& presetFile,
                                     float expectedDrive)
    {
        auto presetXml = juce::XmlDocument::parse(presetFile);
        REQUIRE(presetXml != nullptr);
        FireAudioProcessor restored;
        REQUIRE(state::loadStateFromXml(*presetXml, restored));
        CHECK(getPlainParameter(restored, driveID)
              == Catch::Approx(expectedDrive));
    };

    SECTION("an ancestor hide boundary releases the owner and rejects its result")
    {
        setPlainParameter(processor, driveID, 23.0f);
        auto staleSession =
            StateComponentSaveChooserTestAccess::beginSession(component);
        REQUIRE(StateComponentSaveChooserTestAccess::isCurrent(
            component, staleSession));

        component.dismissPointerGestures();
        CHECK_FALSE(StateComponentSaveChooserTestAccess::hasOwnedChooser(
            component));

        setPlainParameter(processor, driveID, 77.0f);
        auto replacementSession =
            StateComponentSaveChooserTestAccess::beginSession(component);
        REQUIRE(StateComponentSaveChooserTestAccess::isCurrent(
            component, replacementSession));

        const auto stalePath =
            temporaryDirectory.directory.getChildFile("Stale.fire");
        staleSession.deliverResult(stalePath);
        CHECK_FALSE(stalePath.existsAsFile());
        CHECK(StateComponentSaveChooserTestAccess::isCurrent(
            component, replacementSession));

        const auto currentPath =
            temporaryDirectory.directory.getChildFile("Current.fire");
        replacementSession.deliverResult(currentPath);
        REQUIRE(currentPath.existsAsFile());
        CHECK_FALSE(StateComponentSaveChooserTestAccess::hasOwnedChooser(
            component));
        checkSavedDrive(currentPath, 77.0f);
    }

    SECTION("direct hide and restore cannot revive the old chooser")
    {
        setPlainParameter(processor, driveID, 31.0f);
        auto staleSession =
            StateComponentSaveChooserTestAccess::beginSession(component);

        component.setVisible(false);
        component.setVisible(true);
        CHECK_FALSE(StateComponentSaveChooserTestAccess::hasOwnedChooser(
            component));

        setPlainParameter(processor, driveID, 68.0f);
        auto replacementSession =
            StateComponentSaveChooserTestAccess::beginSession(component);
        const auto stalePath =
            temporaryDirectory.directory.getChildFile("Hidden.fire");
        staleSession.deliverResult(stalePath);

        CHECK_FALSE(stalePath.existsAsFile());
        CHECK(StateComponentSaveChooserTestAccess::isCurrent(
            component, replacementSession));

        const auto currentPath =
            temporaryDirectory.directory.getChildFile("Visible.fire");
        replacementSession.deliverResult(currentPath);
        REQUIRE(currentPath.existsAsFile());
        checkSavedDrive(currentPath, 68.0f);
    }

    SECTION("disable and restore cannot revive the old chooser")
    {
        setPlainParameter(processor, driveID, 31.0f);
        auto staleSession =
            StateComponentSaveChooserTestAccess::beginSession(component);

        component.setEnabled(false);
        CHECK_FALSE(StateComponentSaveChooserTestAccess::hasOwnedChooser(
            component));
        component.setEnabled(true);

        const auto stalePath =
            temporaryDirectory.directory.getChildFile("Disabled.fire");
        staleSession.deliverResult(stalePath);
        CHECK_FALSE(stalePath.existsAsFile());

        setPlainParameter(processor, driveID, 68.0f);
        auto replacementSession =
            StateComponentSaveChooserTestAccess::beginSession(component);
        const auto currentPath =
            temporaryDirectory.directory.getChildFile("Reenabled.fire");
        replacementSession.deliverResult(currentPath);
        REQUIRE(currentPath.existsAsFile());
        checkSavedDrive(currentPath, 68.0f);
    }

    SECTION("cancelling consumes only the matching chooser")
    {
        auto cancelledSession =
            StateComponentSaveChooserTestAccess::beginSession(component);
        cancelledSession.deliverResult({});
        CHECK_FALSE(StateComponentSaveChooserTestAccess::hasOwnedChooser(
            component));

        auto replacementSession =
            StateComponentSaveChooserTestAccess::beginSession(component);
        CHECK(StateComponentSaveChooserTestAccess::isCurrent(
            component, replacementSession));

        cancelledSession.deliverResult({});
        CHECK(StateComponentSaveChooserTestAccess::isCurrent(
            component, replacementSession));
    }
}

TEST_CASE("Preset save failure alerts stay inside one visible owner session",
          "[preset][ui][save-alert][session][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    FireAudioProcessor processor;
    processor.statePresets.setPresetDirectoryForTesting(
        temporaryDirectory.directory);
    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        component.dismissPointerGestures();
        component.removeFromDesktop();
    } };
    REQUIRE(component.isShowing());

    std::vector<StateComponentSaveErrorAlertTestAccess::AlertPtr> alerts;
    StateComponentSaveErrorAlertTestAccess::installDialogSeam(
        component, alerts);

    SECTION("an unusable preset root owns its folder-creation failure")
    {
        const auto blockedRoot =
            temporaryDirectory.directory.getChildFile("BlockedRoot");
        REQUIRE(blockedRoot.replaceWithText("not a directory"));
        processor.statePresets.setPresetDirectoryForTesting(blockedRoot);

        StateComponentSaveErrorAlertTestAccess::launchSave(component);

        REQUIRE(alerts.size() == 1);
        CHECK(alerts.front()->title == "Preset save failed");
        CHECK(alerts.front()->message.contains("folder"));
        CHECK(alerts.front()->associatedComponent == &component);
        CHECK(alerts.front()->parentComponent == &component);
        CHECK(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));
        alerts.front()->complete(0);
        CHECK(alerts.front()->closeCount == 1);
        CHECK_FALSE(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));
    }

    SECTION("a failed chooser destination owns its write failure")
    {
        const auto blockedParent =
            temporaryDirectory.directory.getChildFile("BlockedParent");
        REQUIRE(blockedParent.replaceWithText("not a directory"));
        auto session =
            StateComponentSaveChooserTestAccess::beginSession(component);

        session.deliverResult(
            blockedParent.getChildFile("CannotWrite.fire"));

        REQUIRE(alerts.size() == 1);
        CHECK(alerts.front()->title == "Preset save failed");
        CHECK(alerts.front()->message.contains("selected location"));
        CHECK(alerts.front()->associatedComponent == &component);
        CHECK(alerts.front()->parentComponent == &component);
        CHECK(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));
        alerts.front()->complete(0);
        CHECK(alerts.front()->closeCount == 1);
    }

    SECTION("hide closes the old alert and rejects its late completion")
    {
        StateComponentSaveErrorAlertTestAccess::show(
            component, "First failure");
        REQUIRE(alerts.size() == 1);
        auto staleAlert = alerts.front();

        component.setVisible(false);
        CHECK(staleAlert->closeCount == 1);
        CHECK_FALSE(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));
        component.setVisible(true);

        StateComponentSaveErrorAlertTestAccess::show(
            component, "Replacement failure");
        REQUIRE(alerts.size() == 2);
        auto replacementAlert = alerts.back();

        staleAlert->complete(0);
        CHECK(replacementAlert->closeCount == 0);
        CHECK(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));

        replacementAlert->complete(0);
        CHECK(replacementAlert->closeCount == 1);
        CHECK_FALSE(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));
    }

    SECTION("disable-enable cannot revive the old alert")
    {
        StateComponentSaveErrorAlertTestAccess::show(
            component, "Disabled failure");
        REQUIRE(alerts.size() == 1);
        auto staleAlert = alerts.front();

        component.setEnabled(false);
        CHECK(staleAlert->closeCount == 1);
        CHECK_FALSE(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));
        component.setEnabled(true);

        StateComponentSaveErrorAlertTestAccess::show(
            component, "Replacement failure");
        REQUIRE(alerts.size() == 2);
        auto replacementAlert = alerts.back();
        staleAlert->complete(0);
        CHECK(replacementAlert->closeCount == 0);
        CHECK(StateComponentSaveErrorAlertTestAccess::hasActiveAlert(
            component));
        replacementAlert->complete(0);
    }
}

TEST_CASE("Preset session dismissal tolerates synchronous owner deletion",
          "[preset][ui][session][dialog][lifetime][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto component = std::make_unique<state::StateComponent>(
        processor.stateAB, processor.statePresets, processor.treeState);
    component->setBounds(0, 0, 800, 48);
    component->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component->setVisible(true);

    int closeCount = 0;
    StateComponentSaveErrorAlertTestAccess::setDialogPresenter(
        *component,
        [&component, &closeCount](const juce::MessageBoxOptions&,
                                  std::function<void(int)>)
        {
            return std::function<void()> { [&component, &closeCount]
            {
                ++closeCount;
                component.reset();
            } };
        });
    StateComponentSaveErrorAlertTestAccess::show(
        *component, "Failure before deletion");
    REQUIRE(component != nullptr);

    component->dismissPointerGestures();

    CHECK(component == nullptr);
    CHECK(closeCount == 1);
}

TEST_CASE("Destroying StateComponent closes its preset save failure alert",
          "[preset][ui][save-alert][lifetime][destruction][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    std::vector<StateComponentSaveErrorAlertTestAccess::AlertPtr> alerts;
    auto component = std::make_unique<state::StateComponent>(
        processor.stateAB, processor.statePresets, processor.treeState);
    component->setBounds(0, 0, 800, 48);
    component->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component->setVisible(true);
    StateComponentSaveErrorAlertTestAccess::installDialogSeam(
        *component, alerts);
    StateComponentSaveErrorAlertTestAccess::show(
        *component, "Failure before destruction");
    REQUIRE(alerts.size() == 1);
    auto staleAlert = alerts.front();

    component.reset();

    CHECK(staleAlert->closeCount == 1);
    staleAlert->complete(0);
    CHECK(staleAlert->closeCount == 1);
}

TEST_CASE("Disabling preset controls synchronously closes settings",
          "[preset][ui][settings][dialog][session][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        component.dismissPointerGestures();
        component.removeFromDesktop();
    } };

    SECTION("an already-owned dialog closes at the disable boundary")
    {
        auto* dialog = new juce::DialogWindow(
            "Settings", fire::ui::colours::canvas, true, false);
        juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);
        StateComponentSessionBoundaryTestAccess::setSettingsDialog(
            component, dialog);
        REQUIRE(StateComponentSessionBoundaryTestAccess::hasSettingsDialog(
            component));

        component.setEnabled(false);

        CHECK(safeDialog == nullptr);
        CHECK_FALSE(
            StateComponentSessionBoundaryTestAccess::hasSettingsDialog(
                component));
        component.setEnabled(true);
    }

    SECTION("disable-enable during launch rejects the old dialog")
    {
        juce::Component::SafePointer<juce::DialogWindow> launchedDialog;
        StateComponentSessionBoundaryTestAccess::setSettingsDialogFactory(
            component,
            [&component, &launchedDialog]
            {
                auto* dialog = new juce::DialogWindow(
                    "Settings", fire::ui::colours::canvas, true, false);
                launchedDialog = dialog;
                component.setEnabled(false);
                component.setEnabled(true);
                return dialog;
            });

        StateComponentSessionBoundaryTestAccess::showSettingsDialog(
            component);

        CHECK(launchedDialog == nullptr);
        CHECK_FALSE(
            StateComponentSessionBoundaryTestAccess::hasSettingsDialog(
                component));
    }
}

TEST_CASE("Preset box hierarchy dismissal tolerates synchronous owner deletion",
          "[preset][ui][preset-box][lifecycle][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto component = std::make_unique<state::StateComponent>(
        processor.stateAB, processor.statePresets, processor.treeState);

    int closeCount = 0;
    StateComponentPresetBoxTestAccess::setPopupCloser(
        *component,
        [&component, &closeCount]
        {
            ++closeCount;
            component.reset();
        });

    StateComponentPresetBoxTestAccess::notifyParentHierarchyChanged(
        *component);

    CHECK(component == nullptr);
    CHECK(closeCount == 1);
}

TEST_CASE("Preset box accepts only deliberate input in its current lifecycle",
          "[preset][ui][preset-box][input][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    FireAudioProcessor firstPreset;
    FireAudioProcessor secondPreset;
    writePresetFile(firstPreset,
                    temporaryDirectory.directory.getChildFile("First.fire"),
                    "First");
    writePresetFile(secondPreset,
                    temporaryDirectory.directory.getChildFile("Second.fire"),
                    "Second");

    FireAudioProcessor processor;
    processor.statePresets.setPresetDirectoryForTesting(
        temporaryDirectory.directory);
    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    auto* presetBox = component.getPresetBox();
    REQUIRE(presetBox != nullptr);
    REQUIRE(component.isShowing());
    REQUIRE(presetBox->getNumItems() == 2);

    int changeCount = 0;
    presetBox->onChange = [&changeCount] { ++changeCount; };
    const juce::ScopeGuard cleanup { [&]
    {
        presetBox->onChange = nullptr;
        component.dismissPointerGestures();
        component.removeFromDesktop();
    } };
    auto& presetBoxComponent = static_cast<juce::Component&>(*presetBox);

    SECTION("auxiliary and mixed presses never arm the popup")
    {
        const std::array rejectedModifiers {
            juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                                 | juce::ModifierKeys::middleButtonModifier }
        };

        for (const auto modifiers : rejectedModifiers)
        {
            presetBoxComponent.mouseDown(
                makePresetBoxMouseEvent(presetBoxComponent, modifiers));
            presetBoxComponent.mouseDrag(
                makePresetBoxMouseEvent(
                    presetBoxComponent, modifiers, true));
            presetBoxComponent.mouseUp(
                makePresetBoxMouseEvent(presetBoxComponent, {}, true));
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            CHECK_FALSE(presetBox->isPopupActive());
            CHECK_FALSE(
                StateComponentPresetBoxTestAccess::isPopupRequestArmed(
                    component));
            CHECK_FALSE(
                StateComponentPresetBoxTestAccess::hasActivePointerInteraction(
                    component));
            CHECK(presetBox->getSelectedId() == 0);
            CHECK(changeCount == 0);
        }
    }

    SECTION("a queued primary opener cannot cross a hide-show boundary")
    {
        presetBoxComponent.mouseDown(makePresetBoxMouseEvent(
            presetBoxComponent,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
        REQUIRE(presetBox->isPopupActive());
        REQUIRE(StateComponentPresetBoxTestAccess::isPopupRequestArmed(
            component));
        REQUIRE(
            StateComponentPresetBoxTestAccess::hasActivePointerInteraction(
                component));

        component.setVisible(false);
        CHECK_FALSE(presetBox->isPopupActive());
        component.setVisible(true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        CHECK_FALSE(presetBox->isPopupActive());
        CHECK_FALSE(StateComponentPresetBoxTestAccess::isPopupRequestArmed(
            component));
        CHECK_FALSE(StateComponentPresetBoxTestAccess::isPopupSessionActive(
            component));
        CHECK(presetBox->getSelectedId() == 0);
        CHECK(changeCount == 0);

        presetBoxComponent.mouseMove(
            makePresetBoxMouseEvent(presetBoxComponent, {}));
        CHECK_FALSE(
            StateComponentPresetBoxTestAccess::hasActivePointerInteraction(
                component));
    }

    SECTION("hidden and disabled direct commands are inert")
    {
        component.setVisible(false);
        presetBox->showPopup();
        REQUIRE(presetBox->keyPressed(
            juce::KeyPress { juce::KeyPress::rightKey }));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK_FALSE(presetBox->isPopupActive());
        CHECK(presetBox->getSelectedId() == 0);
        CHECK(changeCount == 0);

        component.setVisible(true);
        component.setEnabled(false);
        presetBox->showPopup();
        REQUIRE(presetBox->keyPressed(
            juce::KeyPress { juce::KeyPress::rightKey }));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK_FALSE(presetBox->isPopupActive());
        CHECK(presetBox->getSelectedId() == 0);
        CHECK(changeCount == 0);
    }

    SECTION("direction keys commit synchronously")
    {
        REQUIRE(presetBox->keyPressed(
            juce::KeyPress { juce::KeyPress::rightKey }));
        CHECK(presetBox->getSelectedId() == 1);
        CHECK(changeCount == 1);

        REQUIRE(presetBox->keyPressed(
            juce::KeyPress { juce::KeyPress::rightKey }));
        CHECK(presetBox->getSelectedId() == 2);
        CHECK(changeCount == 2);

        REQUIRE(presetBox->keyPressed(
            juce::KeyPress { juce::KeyPress::leftKey }));
        CHECK(presetBox->getSelectedId() == 1);
        CHECK(changeCount == 3);
    }

    SECTION("header navigation commits before the preset list can change")
    {
        StateComponentPresetBoxTestAccess::selectNextPreset(component);
        CHECK(presetBox->getSelectedId() == 1);
        CHECK(changeCount == 1);
    }
}

TEST_CASE("Preset box binds asynchronous results to one preset-list revision",
          "[preset][ui][preset-box][session][identity][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    FireAudioProcessor laterPreset;
    setPlainParameter(laterPreset, driveID, 77.0f);
    writePresetFile(laterPreset,
                    temporaryDirectory.directory.getChildFile("Zed.fire"),
                    "Zed");

    FireAudioProcessor processor;
    processor.statePresets.setPresetDirectoryForTesting(
        temporaryDirectory.directory);
    state::StateComponent component(
        processor.stateAB, processor.statePresets, processor.treeState);
    component.setBounds(0, 0, 800, 48);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    auto* presetBox = component.getPresetBox();
    REQUIRE(presetBox != nullptr);
    REQUIRE(component.isShowing());
    REQUIRE(presetBox->getNumItems() == 1);

    presetBox->onChange = [&]
    {
        component.updatePresetBox(presetBox->getSelectedId());
    };
    const juce::ScopeGuard cleanup { [&]
    {
        presetBox->onChange = nullptr;
        component.dismissPointerGestures();
        component.removeFromDesktop();
    } };

    SECTION("a rescan cannot reinterpret an old numeric item ID")
    {
        auto staleResult =
            StateComponentPresetBoxTestAccess::createResultHandler(component);

        FireAudioProcessor earlierPreset;
        setPlainParameter(earlierPreset, driveID, 33.0f);
        writePresetFile(
            earlierPreset,
            temporaryDirectory.directory.getChildFile("Alpha.fire"),
            "Alpha");
        processor.statePresets.scanAllPresets();
        component.synchronisePresetSelectionFromManager();
        REQUIRE(presetBox->getNumItems() == 2);
        REQUIRE(presetBox->getItemText(presetBox->indexOfItemId(1))
                == "Alpha");

        setPlainParameter(processor, driveID, 11.0f);
        staleResult(1);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(11.0f));
        CHECK(processor.statePresets.getCurrentPresetKey().isEmpty());
        CHECK(presetBox->getSelectedId() == 0);

        auto currentResult =
            StateComponentPresetBoxTestAccess::createResultHandler(component);
        currentResult(1);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(33.0f));
        CHECK(processor.statePresets.getCurrentPresetKey() == "Alpha.fire");
        CHECK(presetBox->getSelectedId() == 1);

        setPlainParameter(processor, driveID, 19.0f);
        currentResult(2);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(19.0f));
        CHECK(presetBox->getSelectedId() == 1);
    }

    SECTION("only the newest result handler may commit")
    {
        auto staleResult =
            StateComponentPresetBoxTestAccess::createResultHandler(component);
        auto currentResult =
            StateComponentPresetBoxTestAccess::createResultHandler(component);

        setPlainParameter(processor, driveID, 11.0f);
        staleResult(1);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(11.0f));
        CHECK(presetBox->getSelectedId() == 0);

        currentResult(1);
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(77.0f));
        CHECK(processor.statePresets.getCurrentPresetKey() == "Zed.fire");
        CHECK(presetBox->getSelectedId() == 1);
    }

    SECTION("disable-enable consumes the pending result")
    {
        auto staleResult =
            StateComponentPresetBoxTestAccess::createResultHandler(component);
        setPlainParameter(processor, driveID, 11.0f);

        component.setEnabled(false);
        component.setEnabled(true);
        staleResult(1);

        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(11.0f));
        CHECK(processor.statePresets.getCurrentPresetKey().isEmpty());
        CHECK(presetBox->getSelectedId() == 0);
    }
}

TEST_CASE("Preset scan rejects malformed and foreign XML without exposing reset traps",
          "[preset][scan][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedTemporaryDirectory temporaryDirectory;
    CAPTURE(temporaryDirectory.directory.getFullPathName());
    REQUIRE(temporaryDirectory.wasCreated());
    FireAudioProcessor validPresetProcessor;
    setPlainParameter(validPresetProcessor,
                      ParameterIDAndName::getIDString(DRIVE_ID, 0),
                      19.0f);
    writePresetFile(validPresetProcessor,
                    temporaryDirectory.directory.getChildFile("Valid.fire"),
                    "Valid");

    juce::XmlElement foreignXml { "SETTINGS" };
    foreignXml.setAttribute("unrelated", "data");
    REQUIRE(foreignXml.writeTo(temporaryDirectory.directory.getChildFile("Foreign.fire")));
    juce::XmlElement emptyFirePreset { "WINGSFIRE" };
    REQUIRE(emptyFirePreset.writeTo(
        temporaryDirectory.directory.getChildFile("EmptyFire.fire")));
    juce::XmlElement sparseVersionedPreset { "WINGSFIRE" };
    sparseVersionedPreset.setAttribute("presetName", "SparseV2");
    state::saveStateToXml(validPresetProcessor, sparseVersionedPreset);
    sparseVersionedPreset.removeAttribute(
        ParameterIDAndName::getIDString(DRIVE_ID, 0));
    REQUIRE(sparseVersionedPreset.writeTo(
        temporaryDirectory.directory.getChildFile("SparseV2.fire")));
    REQUIRE(temporaryDirectory.directory.getChildFile("Malformed.fire")
                .replaceWithText("<WINGSFIRE><broken></WINGSFIRE>"));

    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 27.0f);
    state::StatePresets presets { processor, temporaryDirectory.directory.getFullPathName() };

    juce::ComboBox menu;
    presets.setPresetAndFolderNames(menu);
    REQUIRE(presets.getNumPresets() == 1);
    REQUIRE(menu.getNumItems() == 1);
    CHECK(menu.getItemText(0) == "Valid");

    // A foreign XML file must never become a selectable action that silently
    // restores every missing parameter to its default value.
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(27.0f));
}

TEST_CASE("Invalid numeric preset attributes fall back safely",
          "[preset][state][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto* mix = processor.treeState.getParameter(MIX_ID);
    REQUIRE(mix != nullptr);
    const float expectedDefault = mix->getDefaultValue();
    mix->setValueNotifyingHost(0.31f);

    juce::XmlElement malformedPreset { "WINGSFIRE" };
    malformedPreset.setAttribute(MIX_ID, "not-a-number");
    state::loadStateFromXml(malformedPreset, processor);

    CHECK(mix->getValue() == Catch::Approx(expectedDefault));
}

TEST_CASE("Preset and host state keep dot-decimal syntax under comma locales",
          "[preset][state][host][locale][roundtrip]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(source, driveID, 37.25f);
    source.assignLfoToTarget(2, driveID);
    source.setModulationDepth(driveID, -0.375f);

    juce::XmlElement preset { "WINGSFIRE" };
    state::saveStateToXml(source, preset);
    juce::MemoryBlock hostState;
    source.getStateInformation(hostState);
    REQUIRE(hostState.getSize() > 0);

    fire::test::ScopedCommaNumericLocale numericLocale;
    if (! numericLocale.activate())
        SKIP("No comma-decimal LC_NUMERIC locale is installed");

    FireAudioProcessor presetRestored;
    REQUIRE(state::loadStateFromXml(preset, presetRestored));
    CHECK(getPlainParameter(presetRestored, driveID)
          == Catch::Approx(37.25f));
    const auto presetRoutings =
        presetRestored.getLfoManager().getModulationRoutingsCopy();
    const auto* presetRouting = findRouting(
        presetRoutings, driveID);
    REQUIRE(presetRouting != nullptr);
    CHECK(presetRouting->sourceLfoIndex == 2);
    CHECK(presetRouting->depth == Catch::Approx(-0.375f));

    FireAudioProcessor hostRestored;
    hostRestored.setStateInformation(
        hostState.getData(), static_cast<int>(hostState.getSize()));
    CHECK(getPlainParameter(hostRestored, driveID)
          == Catch::Approx(37.25f));
    const auto hostRoutings =
        hostRestored.getLfoManager().getModulationRoutingsCopy();
    const auto* hostRouting = findRouting(
        hostRoutings, driveID);
    REQUIRE(hostRouting != nullptr);
    CHECK(hostRouting->sourceLfoIndex == 2);
    CHECK(hostRouting->depth == Catch::Approx(-0.375f));
}

TEST_CASE("Versioned presets reject incomplete snapshots atomically",
          "[preset][state][corrupt][versioned][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 27.0f);

    const auto expectedShape = makeLfoShape(0.41f, 0.87f, 0.58f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 2),
                      expectedShape.smoothness);
    processor.getLfoManager().setLfoData(2, expectedShape);
    processor.assignLfoToTarget(2, driveID);
    processor.setModulationDepth(driveID, -0.36f);

    juce::XmlElement completePreset { "WINGSFIRE" };
    state::saveStateToXml(processor, completePreset);
    REQUIRE(completePreset.getIntAttribute("presetFormatVersion") == 2);

    std::vector<std::unique_ptr<juce::XmlElement>> invalidPresets;

    auto missingParameter = std::make_unique<juce::XmlElement>(completePreset);
    missingParameter->removeAttribute(driveID);
    invalidPresets.push_back(std::move(missingParameter));

    auto missingLfoState = std::make_unique<juce::XmlElement>(completePreset);
    if (auto* lfoState = missingLfoState->getChildByName("LFO_STATE"))
        missingLfoState->removeChildElement(lfoState, true);
    invalidPresets.push_back(std::move(missingLfoState));

    auto truncatedLfoState = std::make_unique<juce::XmlElement>(completePreset);
    if (auto* lfoState = truncatedLfoState->getChildByName("LFO_STATE"))
        lfoState->removeChildElement(
            lfoState->getChildElement(lfoState->getNumChildElements() - 1), true);
    invalidPresets.push_back(std::move(truncatedLfoState));

    auto missingRoutingState = std::make_unique<juce::XmlElement>(completePreset);
    if (auto* routingState = missingRoutingState->getChildByName("MODULATION_STATE"))
        missingRoutingState->removeChildElement(routingState, true);
    invalidPresets.push_back(std::move(missingRoutingState));

    auto duplicateRouting = std::make_unique<juce::XmlElement>(completePreset);
    if (auto* routingState = duplicateRouting->getChildByName("MODULATION_STATE"))
        appendRouting(*routingState, 0, driveID, 0.25f);
    invalidPresets.push_back(std::move(duplicateRouting));

    auto unsupportedFutureVersion = std::make_unique<juce::XmlElement>(completePreset);
    unsupportedFutureVersion->setAttribute("presetFormatVersion", 3);
    invalidPresets.push_back(std::move(unsupportedFutureVersion));

    for (const auto& invalidPreset : invalidPresets)
    {
        REQUIRE(invalidPreset != nullptr);
        CHECK_FALSE(state::loadStateFromXml(*invalidPreset, processor));
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(27.0f));

        const auto shapes = processor.getLfoManager().getLfoDataCopy();
        REQUIRE(shapes.size() == 4);
        REQUIRE(shapes[2].points.size() == expectedShape.points.size());
        CHECK(shapes[2].points[1].x == Catch::Approx(expectedShape.points[1].x));
        CHECK(shapes[2].points[1].y == Catch::Approx(expectedShape.points[1].y));
        CHECK(shapes[2].smoothness == Catch::Approx(expectedShape.smoothness));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* routing = findRouting(routings, driveID);
        REQUIRE(routing != nullptr);
        CHECK(routing->sourceLfoIndex == 2);
        CHECK(routing->depth == Catch::Approx(-0.36f));
    }
}

TEST_CASE("Corrupt host state without a valid APVTS tree is rejected atomically",
          "[state][host][corrupt][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 22.0f);
    processor.setSavedWidth(1432);
    processor.setSavedHeight(811);
    processor.statePresets.setCurrentPresetId(7);

    const auto shape = makeLfoShape(0.38f, 0.88f, 0.57f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 1),
                      shape.smoothness);
    processor.getLfoManager().setLfoData(1, shape);
    processor.assignLfoToTarget(1, driveID);
    processor.setModulationDepth(driveID, 0.42f);

    // Keep a known-good inactive B snapshot, then make active A observably
    // different. A rejected host transaction must preserve both sides.
    processor.stateAB.copyAB();
    setPlainParameter(processor, driveID, 31.0f);

    juce::XmlElement corruptState { "state" };
    corruptState.createNewChildElement("BROKEN_PARAMETER_STATE");
    auto* otherState = corruptState.createNewChildElement("otherState");
    otherState->setAttribute("currentPresetID", 1);
    otherState->setAttribute("editorWidth", 1);
    otherState->setAttribute("editorHeight", 1);

    FireAudioProcessor hostileABPayload;
    setPlainParameter(hostileABPayload, driveID, 88.0f);
    auto* hostileABState = corruptState.createNewChildElement("AB_STATE");
    state::saveStateToXml(hostileABPayload, *hostileABState);
    hostileABState->setAttribute("currentSideIsA", false);

    juce::MemoryBlock corruptBinary;
    juce::AudioProcessor::copyXmlToBinary(corruptState, corruptBinary);
    processor.setStateInformation(corruptBinary.getData(),
                                  static_cast<int>(corruptBinary.getSize()));

    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(31.0f));
    CHECK(processor.getSavedWidth() == 1432);
    CHECK(processor.getSavedHeight() == 811);
    CHECK(processor.statePresets.getCurrentPresetId() == 7);
    CHECK(processor.stateAB.isCurrentA());

    const auto shapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes.size() == 4);
    REQUIRE(shapes[1].points.size() == 3);
    CHECK(shapes[1].points[1].x == Catch::Approx(0.38f));
    CHECK(shapes[1].points[1].y == Catch::Approx(0.88f));
    CHECK(shapes[1].smoothness == Catch::Approx(0.57f));

    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* routing = findRouting(routings, driveID);
    REQUIRE(routing != nullptr);
    CHECK(routing->sourceLfoIndex == 1);
    CHECK(routing->depth == Catch::Approx(0.42f));

    processor.stateAB.toggleAB();
    CHECK_FALSE(processor.stateAB.isCurrentA());
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(22.0f));
}

TEST_CASE("Sparse host parameter trees are rejected atomically",
          "[state][host][corrupt][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 31.0f);
    processor.setSavedWidth(1432);
    processor.setSavedHeight(811);
    processor.statePresets.setCurrentPresetId(7);

    const auto shape = makeLfoShape(0.38f, 0.88f, 0.57f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 1),
                      shape.smoothness);
    processor.getLfoManager().setLfoData(1, shape);
    processor.assignLfoToTarget(1, driveID);
    processor.setModulationDepth(driveID, 0.42f);

    FireAudioProcessor incoming;
    juce::MemoryBlock incomingState;
    incoming.getStateInformation(incomingState);
    auto sparseXml = juce::AudioProcessor::getXmlFromBinary(
        incomingState.getData(), static_cast<int>(incomingState.getSize()));
    REQUIRE(sparseXml != nullptr);

    auto* parameterState = sparseXml->getChildByName(
        incoming.treeState.state.getType().toString());
    REQUIRE(parameterState != nullptr);
    auto* retainedParameter = findHostParameter(*sparseXml, incoming, HQ_ID);
    REQUIRE(retainedParameter != nullptr);
    for (int childIndex = parameterState->getNumChildElements() - 1;
         childIndex >= 0;
         --childIndex)
    {
        auto* child = parameterState->getChildElement(childIndex);
        if (child != retainedParameter)
            parameterState->removeChildElement(child, true);
    }
    REQUIRE(parameterState->getNumChildElements() == 1);

    auto* otherState = sparseXml->getChildByName("otherState");
    REQUIRE(otherState != nullptr);
    otherState->setAttribute("currentPresetID", 1);
    otherState->setAttribute("editorWidth", 999);
    otherState->setAttribute("editorHeight", 600);

    SECTION("versioned state rejects a declared-count mismatch")
    {
        REQUIRE(sparseXml->hasAttribute("stateFormatVersion"));
        REQUIRE(sparseXml->hasAttribute("savedParameterCount"));
    }

    SECTION("unversioned state rejects an implausibly sparse legacy wrapper")
    {
        sparseXml->removeAttribute("stateFormatVersion");
        sparseXml->removeAttribute("savedParameterCount");
    }

    juce::MemoryBlock sparseState;
    juce::AudioProcessor::copyXmlToBinary(*sparseXml, sparseState);
    processor.setStateInformation(sparseState.getData(),
                                  static_cast<int>(sparseState.getSize()));

    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(31.0f));
    CHECK(processor.getSavedWidth() == 1432);
    CHECK(processor.getSavedHeight() == 811);
    CHECK(processor.statePresets.getCurrentPresetId() == 7);

    const auto shapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes.size() == 4);
    REQUIRE(shapes[1].points.size() == 3);
    CHECK(shapes[1].points[1].x == Catch::Approx(0.38f));
    CHECK(shapes[1].points[1].y == Catch::Approx(0.88f));
    CHECK(shapes[1].smoothness == Catch::Approx(0.57f));

    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* routing = findRouting(routings, driveID);
    REQUIRE(routing != nullptr);
    CHECK(routing->sourceLfoIndex == 1);
    CHECK(routing->depth == Catch::Approx(0.42f));
}

TEST_CASE("The oldest wrapped host parameter layout remains loadable",
          "[state][host][legacy][compatibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto templateState = processor.treeState.copyState();
    juce::ValueTree legacyParameters(templateState.getType());

    const std::array<std::pair<const char*, float>, 9> legacyAnchors {{
        { HQ_ID, 1.0f },
        { DOWNSAMPLE_ID, 4.0f },
        { MIX_ID, 0.37f },
        { OFF_ID, 0.0f },
        { PRE_ID, 1.0f },
        { POST_ID, 0.0f },
        { LOW_ID, 1.0f },
        { BAND_ID, 0.0f },
        { HIGH_ID, 0.0f },
    }};

    for (const auto& [parameterID, value] : legacyAnchors)
    {
        juce::ValueTree matchingChild;
        for (const auto& child : templateState)
            if (child.getProperty("id").toString() == parameterID)
            {
                matchingChild = child.createCopy();
                break;
            }

        REQUIRE(matchingChild.isValid());
        matchingChild.setProperty("value", value, nullptr);
        legacyParameters.addChild(matchingChild, -1, nullptr);
    }

    const auto parameterChildType = templateState.getChild(0).getType();
    for (int retiredIndex = 0; retiredIndex < 10; ++retiredIndex)
    {
        juce::ValueTree retiredParameter(parameterChildType);
        retiredParameter.setProperty("id",
                                     "retiredParameter" + juce::String(retiredIndex),
                                     nullptr);
        retiredParameter.setProperty("value", retiredIndex * 0.1f, nullptr);
        legacyParameters.addChild(retiredParameter, -1, nullptr);
    }
    REQUIRE(legacyParameters.getNumChildren() == 19);

    juce::XmlElement legacyState("state");
    legacyState.addChildElement(legacyParameters.createXml().release());
    juce::MemoryBlock legacyBinary;
    juce::AudioProcessor::copyXmlToBinary(legacyState, legacyBinary);

    processor.setStateInformation(legacyBinary.getData(),
                                  static_cast<int>(legacyBinary.getSize()));

    CHECK(getPlainParameter(processor, HQ_ID) == Catch::Approx(1.0f));
    CHECK(getPlainParameter(processor, DOWNSAMPLE_ID) == Catch::Approx(4.0f));
    CHECK(getPlainParameter(processor, MIX_ID) == Catch::Approx(0.37f));
    CHECK(getPlainParameter(processor, OFF_ID) == Catch::Approx(0.0f));
    CHECK(getPlainParameter(processor, PRE_ID) == Catch::Approx(1.0f));
    CHECK(getPlainParameter(processor, POST_ID) == Catch::Approx(0.0f));
    CHECK(getPlainParameter(processor, LOW_ID) == Catch::Approx(1.0f));
    CHECK(getPlainParameter(processor, BAND_ID) == Catch::Approx(0.0f));
    CHECK(getPlainParameter(processor, HIGH_ID) == Catch::Approx(0.0f));
}

TEST_CASE("Legacy multiband host state can omit the temporary global Mix",
          "[state][host][legacy][compatibility][multiband]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto templateState = processor.treeState.copyState();
    juce::ValueTree legacyParameters(templateState.getType());

    const auto appendCurrentParameter = [&](const juce::String& parameterID,
                                            float value)
    {
        juce::ValueTree matchingChild;
        for (const auto& child : templateState)
            if (child.getProperty("id").toString() == parameterID)
            {
                matchingChild = child.createCopy();
                break;
            }

        REQUIRE(matchingChild.isValid());
        matchingChild.setProperty("value", value, nullptr);
        legacyParameters.addChild(matchingChild, -1, nullptr);
    };

    const std::array<std::pair<const char*, float>, 8> stableAnchors {{
        { HQ_ID, 1.0f },
        { DOWNSAMPLE_ID, 4.0f },
        { OFF_ID, 0.0f },
        { PRE_ID, 1.0f },
        { POST_ID, 0.0f },
        { LOW_ID, 1.0f },
        { BAND_ID, 0.0f },
        { HIGH_ID, 0.0f },
    }};
    for (const auto& [parameterID, value] : stableAnchors)
        appendCurrentParameter(parameterID, value);

    for (int band = 0; band < 4; ++band)
        appendCurrentParameter(ParameterIDAndName::getIDString(MIX_ID, band),
                               0.23f + 0.1f * static_cast<float>(band));

    const auto parameterChildType = templateState.getChild(0).getType();
    for (int retiredIndex = 0; retiredIndex < 51; ++retiredIndex)
    {
        juce::ValueTree retiredParameter(parameterChildType);
        retiredParameter.setProperty("id",
                                     "retiredMultibandParameter"
                                         + juce::String(retiredIndex),
                                     nullptr);
        retiredParameter.setProperty("value", retiredIndex * 0.01f, nullptr);
        legacyParameters.addChild(retiredParameter, -1, nullptr);
    }
    REQUIRE(legacyParameters.getNumChildren() == 63);
    CHECK(countValueTreeChildrenWithID(legacyParameters, MIX_ID) == 0);

    juce::XmlElement legacyState("state");
    legacyState.addChildElement(legacyParameters.createXml().release());
    juce::MemoryBlock legacyBinary;
    juce::AudioProcessor::copyXmlToBinary(legacyState, legacyBinary);

    processor.setStateInformation(legacyBinary.getData(),
                                  static_cast<int>(legacyBinary.getSize()));

    CHECK(getPlainParameter(
              processor, ParameterIDAndName::getIDString(MIX_ID, 0))
          == Catch::Approx(0.23f));
    CHECK(getPlainParameter(
              processor, ParameterIDAndName::getIDString(MIX_ID, 3))
          == Catch::Approx(0.53f));
    CHECK(getPlainParameter(processor, MIX_ID) == Catch::Approx(1.0f));
}

TEST_CASE("Reentrant host saves never mix multiband topology generations",
          "[state][host][topology][transaction][reentrant]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor incoming;

    const auto oldTarget = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    const auto newTarget = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    REQUIRE(oldTarget != newTarget);

    setPlainParameter(subject, NUM_BANDS_ID, 2.0f);
    setPlainParameter(subject, newTarget, 11.0f);
    setPlainParameter(subject, oldTarget, 77.0f);
    subject.assignLfoToTarget(0, oldTarget);
    subject.setModulationDepth(oldTarget, 0.37f);

    // Model the completed result of deleting band zero: the surviving logical
    // band and its modulation route both move from slot one to slot zero.
    setPlainParameter(incoming, NUM_BANDS_ID, 1.0f);
    setPlainParameter(incoming, newTarget, 77.0f);
    incoming.assignLfoToTarget(0, newTarget);
    incoming.setModulationDepth(newTarget, 0.37f);
    juce::MemoryBlock incomingState;
    incoming.getStateInformation(incomingState);

    const int bandCountIndex = getParameterIndex(subject, NUM_BANDS_ID);
    REQUIRE(bandCountIndex >= 0);
    ParameterTriggeredStateCapture host(subject, bandCountIndex);

    // APVTS parameter listeners are synchronous. During the old commit path,
    // NUM_BANDS already came from the incoming state while the routing still
    // belonged to the previous generation, so a host save here was torn.
    subject.setStateInformation(incomingState.getData(),
                                static_cast<int>(incomingState.getSize()));
    REQUIRE(host.captured);
    REQUIRE(host.state.getSize() > 0);

    FireAudioProcessor restored;
    restored.setStateInformation(host.state.getData(),
                                 static_cast<int>(host.state.getSize()));

    const auto savedBandCount = getPlainParameter(restored, NUM_BANDS_ID);
    const auto savedFirstDrive = getPlainParameter(restored, newTarget);
    const auto savedRoutings = restored.getLfoManager().getModulationRoutingsCopy();
    const bool savedOldTarget = countRoutingsForTarget(savedRoutings, oldTarget) > 0;
    const bool savedNewTarget = countRoutingsForTarget(savedRoutings, newTarget) > 0;
    const bool coherentOldGeneration = savedBandCount == Catch::Approx(2.0f)
                                    && savedFirstDrive == Catch::Approx(11.0f)
                                    && savedOldTarget
                                    && ! savedNewTarget;
    const bool coherentNewGeneration = savedBandCount == Catch::Approx(1.0f)
                                    && savedFirstDrive == Catch::Approx(77.0f)
                                    && ! savedOldTarget
                                    && savedNewTarget;

    CAPTURE(savedBandCount, savedFirstDrive, savedOldTarget, savedNewTarget);
    CHECK((coherentOldGeneration || coherentNewGeneration));
    CHECK_FALSE((savedBandCount == Catch::Approx(1.0f) && savedOldTarget));
}

TEST_CASE("LFO shape writes preserve APVTS smoothness authority",
          "[state][host][lfo][authority]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0);
    constexpr float automatedSmoothness = 0.43f;
    setPlainParameter(processor, smoothnessID, automatedSmoothness);

    const int smoothnessIndex = getParameterIndex(processor, smoothnessID);
    REQUIRE(smoothnessIndex >= 0);
    ParameterTriggeredStateHistory host(processor, smoothnessIndex);

    const auto incomingShape = makeLfoShape(0.39f, 0.87f, 0.9f);
    processor.getLfoManager().setLfoData(0, incomingShape);

    CHECK(host.states.empty());
    CHECK(getPlainParameter(processor, smoothnessID)
          == Catch::Approx(automatedSmoothness));
    const auto lfoData = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(lfoData.size() == 4);
    CHECK(lfoData[0].points == incomingShape.points);
    CHECK(lfoData[0].curvatures == incomingShape.curvatures);
    CHECK(lfoData[0].smoothness == Catch::Approx(automatedSmoothness));
}

TEST_CASE("Host saves pair LFO shapes with authoritative smoothness",
          "[state][host][lfo][authority][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    const auto publishedShape = makeLfoShape(0.39f, 0.87f, 0.73f);
    constexpr float automatedSmoothness = 0.37f;
    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0);
    setPlainParameter(subject, smoothnessID, automatedSmoothness);
    juce::MemoryBlock savedState;

    auto& manager = subject.getLfoManager();
    manager.setLfoData(0, publishedShape);
    subject.getStateInformation(savedState);
    REQUIRE(savedState.getSize() > 0);

    auto savedXml = juce::AudioProcessor::getXmlFromBinary(
        savedState.getData(),
        static_cast<int>(savedState.getSize()));
    REQUIRE(savedXml != nullptr);
    auto* savedParameter = findHostParameter(*savedXml, subject, smoothnessID);
    auto* savedLfo = findHostLfo(*savedXml, 0);
    REQUIRE(savedParameter != nullptr);
    REQUIRE(savedLfo != nullptr);
    CHECK(savedParameter->getDoubleAttribute("value")
          == Catch::Approx(automatedSmoothness));
    CHECK(savedLfo->getDoubleAttribute("smoothness")
          == Catch::Approx(automatedSmoothness));

    FireAudioProcessor restored;
    restored.setStateInformation(savedState.getData(),
                                 static_cast<int>(savedState.getSize()));
    const auto restoredData = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredData.size() == 4);
    CHECK(restoredData[0].points == publishedShape.points);
    CHECK(restoredData[0].curvatures == publishedShape.curvatures);
    CHECK(restoredData[0].smoothness
          == Catch::Approx(automatedSmoothness));
}

TEST_CASE("Host loading promotes legacy LFO-only smoothness into APVTS",
          "[state][host][lfo][authority][legacy][migration]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto smoothnessID = ParameterIDAndName::getIDString(
        LFO_SMOOTH_ID, 0);
    const auto shape = makeLfoShape(0.46f, 0.89f, 0.11f);
    constexpr float legacySmoothness = 0.37f;
    source.getLfoManager().setLfoData(0, shape);

    juce::MemoryBlock legacyState;
    source.getStateInformation(legacyState);
    auto legacyXml = juce::AudioProcessor::getXmlFromBinary(
        legacyState.getData(), static_cast<int>(legacyState.getSize()));
    REQUIRE(legacyXml != nullptr);
    auto* parameterState = legacyXml->getChildByName(
        source.treeState.state.getType().toString());
    auto* smoothnessParameter = findHostParameter(
        *legacyXml, source, smoothnessID);
    auto* lfo = findHostLfo(*legacyXml, 0);
    REQUIRE(parameterState != nullptr);
    REQUIRE(smoothnessParameter != nullptr);
    REQUIRE(lfo != nullptr);

    parameterState->removeChildElement(smoothnessParameter, true);
    legacyXml->removeAttribute("stateFormatVersion");
    legacyXml->removeAttribute("savedParameterCount");
    lfo->setAttribute("smoothness", legacySmoothness);
    juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyState);

    FireAudioProcessor restored;
    restored.setStateInformation(legacyState.getData(),
                                 static_cast<int>(legacyState.getSize()));
    CHECK(getPlainParameter(restored, smoothnessID)
          == Catch::Approx(legacySmoothness));
    const auto restoredData = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredData.size() == 4);
    CHECK(restoredData[0].points == shape.points);
    CHECK(restoredData[0].curvatures == shape.curvatures);
    CHECK(restoredData[0].smoothness == Catch::Approx(legacySmoothness));

    juce::MemoryBlock upgradedState;
    restored.getStateInformation(upgradedState);
    auto upgradedXml = juce::AudioProcessor::getXmlFromBinary(
        upgradedState.getData(),
        static_cast<int>(upgradedState.getSize()));
    REQUIRE(upgradedXml != nullptr);
    auto* upgradedParameter = findHostParameter(
        *upgradedXml, restored, smoothnessID);
    auto* upgradedLfo = findHostLfo(*upgradedXml, 0);
    REQUIRE(upgradedParameter != nullptr);
    REQUIRE(upgradedLfo != nullptr);
    CHECK(upgradedParameter->getDoubleAttribute("value")
          == Catch::Approx(legacySmoothness));
    CHECK(upgradedLfo->getDoubleAttribute("smoothness")
          == Catch::Approx(legacySmoothness));
}

TEST_CASE("Completed LFO automation remains authoritative after shape edits",
          "[state][host][lfo][authority][automation][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    auto& manager = subject.getLfoManager();
    const auto baselineShape = makeLfoShape(0.28f, 0.76f, 0.2f);
    const auto publishedShape = makeLfoShape(0.64f, 0.31f, 0.8f);
    constexpr float automatedSmoothness = 0.43f;
    manager.setLfoData(0, baselineShape);

    juce::MemoryBlock savedAfterAutomation;
    manager.setLfoData(0, publishedShape);
    setPlainParameter(subject,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0),
                      automatedSmoothness);
    subject.getStateInformation(savedAfterAutomation);
    REQUIRE(savedAfterAutomation.getSize() > 0);

    auto savedXml = juce::AudioProcessor::getXmlFromBinary(
        savedAfterAutomation.getData(),
        static_cast<int>(savedAfterAutomation.getSize()));
    REQUIRE(savedXml != nullptr);
    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0);
    auto* savedParameter = findHostParameter(*savedXml, subject, smoothnessID);
    auto* savedLfo = findHostLfo(*savedXml, 0);
    REQUIRE(savedParameter != nullptr);
    REQUIRE(savedLfo != nullptr);
    CHECK(savedParameter->getDoubleAttribute("value")
          == Catch::Approx(automatedSmoothness));
    CHECK(savedLfo->getDoubleAttribute("smoothness")
          == Catch::Approx(automatedSmoothness));

    FireAudioProcessor restored;
    restored.setStateInformation(savedAfterAutomation.getData(),
                                 static_cast<int>(savedAfterAutomation.getSize()));
    const auto restoredData = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredData.size() == 4);
    CHECK(restoredData[0].points == publishedShape.points);
    CHECK(restoredData[0].curvatures == publishedShape.curvatures);
    CHECK(restoredData[0].smoothness
          == Catch::Approx(automatedSmoothness));

    const auto finalData = manager.getLfoDataCopy();
    REQUIRE(finalData.size() == 4);
    CHECK(finalData[0].points == publishedShape.points);
    CHECK(finalData[0].curvatures == publishedShape.curvatures);
    CHECK(finalData[0].smoothness
          == Catch::Approx(automatedSmoothness));
}

TEST_CASE("LFO snapshots use the parameter-quantised smoothness",
          "[state][host][lfo][authority][quantisation][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0);
    auto* smoothnessParameter = subject.treeState.getParameter(smoothnessID);
    REQUIRE(smoothnessParameter != nullptr);
    const int smoothnessIndex = getParameterIndex(subject, smoothnessID);
    REQUIRE(smoothnessIndex >= 0);

    auto publishedShape = makeLfoShape(0.58f, 0.82f, 0.735f);
    publishedShape.smoothness = 0.735f;
    const float expectedSmoothness = smoothnessParameter->getNormalisableRange()
                                         .snapToLegalValue(
                                             publishedShape.smoothness);
    CHECK(expectedSmoothness == Catch::Approx(0.74f));

    subject.getLfoManager().setLfoData(0, publishedShape);
    ParameterTriggeredStateCapture host(subject, smoothnessIndex);
    setPlainParameter(subject, smoothnessID, publishedShape.smoothness);
    REQUIRE(host.captured);
    REQUIRE(host.state.getSize() > 0);

    auto savedXml = juce::AudioProcessor::getXmlFromBinary(
        host.state.getData(), static_cast<int>(host.state.getSize()));
    REQUIRE(savedXml != nullptr);
    auto* savedParameter = findHostParameter(*savedXml, subject, smoothnessID);
    auto* savedLfo = findHostLfo(*savedXml, 0);
    REQUIRE(savedParameter != nullptr);
    REQUIRE(savedLfo != nullptr);
    CHECK(savedParameter->getDoubleAttribute("value")
          == Catch::Approx(expectedSmoothness));
    CHECK(savedLfo->getDoubleAttribute("smoothness")
          == Catch::Approx(expectedSmoothness));

    FireAudioProcessor restored;
    restored.setStateInformation(host.state.getData(),
                                 static_cast<int>(host.state.getSize()));
    const auto restoredData = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredData.size() == 4);
    CHECK(restoredData[0].points == publishedShape.points);
    CHECK(restoredData[0].curvatures == publishedShape.curvatures);
    CHECK(restoredData[0].smoothness
          == Catch::Approx(expectedSmoothness));
}

TEST_CASE("Shape edits cannot overwrite later smoothness automation",
          "[state][host][lfo][authority][automation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto earlierShape = makeLfoShape(0.31f, 0.84f, 0.2f);
    const auto laterShape = makeLfoShape(0.67f, 0.18f, 0.8f);
    constexpr float automatedSmoothness = 0.55f;
    const auto smoothnessID = ParameterIDAndName::getIDString(
        LFO_SMOOTH_ID, 0);

    auto& manager = processor.getLfoManager();
    manager.setLfoData(0, earlierShape);
    setPlainParameter(processor, smoothnessID, automatedSmoothness);
    manager.setLfoData(0, laterShape);

    const auto finalData = manager.getLfoDataCopy();
    REQUIRE(finalData.size() == 4);
    const auto* finalSmoothness = processor.treeState.getRawParameterValue(smoothnessID);
    REQUIRE(finalSmoothness != nullptr);
    CHECK(finalData[0].points == laterShape.points);
    CHECK(finalData[0].curvatures == laterShape.curvatures);
    CHECK(finalData[0].smoothness == Catch::Approx(automatedSmoothness));
    CHECK(finalSmoothness->load(std::memory_order_relaxed)
          == Catch::Approx(automatedSmoothness));
}

TEST_CASE("Host and preset snapshots take LFO smoothness from APVTS authority",
          "[state][host][preset][lfo][authority][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();
    const auto smoothnessID = ParameterIDAndName::getIDString(
        LFO_SMOOTH_ID, 0);
    constexpr float authoritativeSmoothness = 0.2f;
    const auto shapeWithStaleSmoothness = makeLfoShape(0.71f, 0.16f, 0.8f);

    setPlainParameter(processor, smoothnessID, authoritativeSmoothness);
    manager.setLfoData(0, shapeWithStaleSmoothness);

    const auto liveData = manager.getLfoDataCopy();
    REQUIRE(liveData.size() == 4);
    CHECK(liveData[0].points == shapeWithStaleSmoothness.points);
    CHECK(liveData[0].curvatures == shapeWithStaleSmoothness.curvatures);
    CHECK(liveData[0].smoothness == Catch::Approx(authoritativeSmoothness));
    CHECK(getPlainParameter(processor, smoothnessID)
          == Catch::Approx(authoritativeSmoothness));

    juce::MemoryBlock hostState;
    processor.getStateInformation(hostState);
    REQUIRE(hostState.getSize() > 0);

    auto hostXml = juce::AudioProcessor::getXmlFromBinary(
        hostState.getData(), static_cast<int>(hostState.getSize()));
    REQUIRE(hostXml != nullptr);
    auto* hostParameter = findHostParameter(*hostXml, processor, smoothnessID);
    auto* hostLfo = findHostLfo(*hostXml, 0);
    REQUIRE(hostParameter != nullptr);
    REQUIRE(hostLfo != nullptr);
    CHECK(hostParameter->getDoubleAttribute("value")
          == Catch::Approx(authoritativeSmoothness));
    CHECK(hostLfo->getDoubleAttribute("smoothness")
          == Catch::Approx(authoritativeSmoothness));

    juce::XmlElement presetState { "WINGSFIRE" };
    state::saveStateToXml(processor, presetState);
    auto* presetLfo = findHostLfo(presetState, 0);
    REQUIRE(presetLfo != nullptr);
    CHECK(presetState.getDoubleAttribute(smoothnessID)
          == Catch::Approx(authoritativeSmoothness));
    CHECK(presetLfo->getDoubleAttribute("smoothness")
          == Catch::Approx(authoritativeSmoothness));
    const auto presetShape = LfoData::readFromXml(*presetLfo);
    CHECK(presetShape.points == shapeWithStaleSmoothness.points);
    CHECK(presetShape.curvatures == shapeWithStaleSmoothness.curvatures);

    FireAudioProcessor restored;
    restored.setStateInformation(hostState.getData(),
                                 static_cast<int>(hostState.getSize()));
    const auto restoredData = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredData.size() == 4);
    CHECK(restoredData[0].points == shapeWithStaleSmoothness.points);
    CHECK(restoredData[0].curvatures == shapeWithStaleSmoothness.curvatures);
    CHECK(restoredData[0].smoothness
          == Catch::Approx(authoritativeSmoothness));
    CHECK(getPlainParameter(restored, smoothnessID)
          == Catch::Approx(authoritativeSmoothness));
}

TEST_CASE("Cross-thread host saves do not wait for an active topology writer",
          "[state][host][topology][transaction][locking]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    const auto oldTarget = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    const auto stagedTarget = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(subject, NUM_BANDS_ID, 2.0f);
    subject.assignLfoToTarget(0, oldTarget);
    subject.statePresets.setCurrentPresetKey("Factory/Old.fire");
    subject.setSavedEditorSize(1200, 600);

    subject.beginMultibandTopologyEdit();
    setPlainParameter(subject, NUM_BANDS_ID, 1.0f);
    subject.clearModulationForParameter(oldTarget);
    subject.assignLfoToTarget(1, stagedTarget);
    subject.statePresets.setCurrentPresetKey("Factory/Staged.fire");
    subject.setSavedEditorSize(1600, 800);

    juce::WaitableEvent workerStarted;
    juce::WaitableEvent saveCompleted;
    juce::MemoryBlock savedState;
    std::thread hostSave([&]
    {
        workerStarted.signal();
        subject.getStateInformation(savedState);
        saveCompleted.signal();
    });

    const bool didStart = workerStarted.wait(2000);
    const bool completedWhileEditWasActive = didStart && saveCompleted.wait(2000);
    subject.requestMultibandTopologyReset();
    hostSave.join();

    REQUIRE(didStart);
    REQUIRE(completedWhileEditWasActive);
    REQUIRE(savedState.getSize() > 0);
    FireAudioProcessor restored;
    restored.setStateInformation(savedState.getData(),
                                 static_cast<int>(savedState.getSize()));
    const auto routings = restored.getLfoManager().getModulationRoutingsCopy();
    CHECK(getPlainParameter(restored, NUM_BANDS_ID) == Catch::Approx(2.0f));
    CHECK(countRoutingsForTarget(routings, oldTarget) == 1);
    CHECK(countRoutingsForTarget(routings, stagedTarget) == 0);
    CHECK(restored.statePresets.getCurrentPresetKey() == "Factory/Old.fire");
    CHECK(restored.getSavedWidth() == 1200);
    CHECK(restored.getSavedHeight() == 600);
}

TEST_CASE("Topology writers drain readers which observed the prior generation",
          "[state][host][topology][transaction][reader-handshake]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    setPlainParameter(subject, NUM_BANDS_ID, 2.0f);

    juce::WaitableEvent writerReturnedFromBegin;
    juce::WaitableEvent allowWriterToFinish;
    std::thread writer;
    bool writerPublishedOdd = false;
    bool writerReturnedWhileReaderWasRegistered = false;
    unsigned int readersSeenByHook = 0;

    subject.setSerializableStateReaderHookForTesting([&]
    {
        writer = std::thread([&]
        {
            subject.beginMultibandTopologyEdit();
            writerReturnedFromBegin.signal();
            allowWriterToFinish.wait(2000);
            subject.requestMultibandTopologyReset();
        });

        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 2000.0;
        while ((subject.getMultibandTopologyGenerationForTesting() & 1u) == 0u
               && juce::Time::getMillisecondCounterHiRes() < deadline)
            juce::Thread::yield();

        writerPublishedOdd =
            (subject.getMultibandTopologyGenerationForTesting() & 1u) != 0u;
        readersSeenByHook =
            subject.getActiveSerializableStateReadersForTesting();
        writerReturnedWhileReaderWasRegistered =
            writerReturnedFromBegin.wait(75);
    });

    juce::MemoryBlock savedState;
    subject.getStateInformation(savedState);
    const bool writerReturnedAfterReaderUnregistered =
        writerReturnedFromBegin.wait(2000);
    allowWriterToFinish.signal();
    if (writer.joinable())
        writer.join();

    REQUIRE(writerPublishedOdd);
    CHECK(readersSeenByHook == 1u);
    CHECK_FALSE(writerReturnedWhileReaderWasRegistered);
    REQUIRE(writerReturnedAfterReaderUnregistered);
    REQUIRE(savedState.getSize() > 0);
    CHECK(subject.getActiveSerializableStateReadersForTesting() == 0u);
    CHECK((subject.getMultibandTopologyGenerationForTesting() & 1u) == 0u);

    FireAudioProcessor restored;
    restored.setStateInformation(savedState.getData(),
                                 static_cast<int>(savedState.getSize()));
    CHECK(getPlainParameter(restored, NUM_BANDS_ID) == Catch::Approx(2.0f));
}

TEST_CASE("Listener exceptions cannot strand a topology transaction",
          "[state][host][topology][transaction][exception]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor incoming;
    setPlainParameter(subject, NUM_BANDS_ID, 2.0f);
    setPlainParameter(incoming, NUM_BANDS_ID, 1.0f);
    juce::MemoryBlock incomingState;
    incoming.getStateInformation(incomingState);

    const int bandCountIndex = getParameterIndex(subject, NUM_BANDS_ID);
    REQUIRE(bandCountIndex >= 0);
    bool listenerExceptionEscaped = false;
    {
        ThrowingParameterListener listener(subject, bandCountIndex);
        try
        {
            subject.setStateInformation(
                incomingState.getData(),
                static_cast<int>(incomingState.getSize()));
        }
        catch (const std::runtime_error&)
        {
            listenerExceptionEscaped = true;
        }
    }

    juce::WaitableEvent probeEntered;
    std::thread topologyProbe([&]
    {
        subject.beginMultibandTopologyEdit();
        probeEntered.signal();
        subject.requestMultibandTopologyReset();
    });

    const bool transactionWasBalanced = probeEntered.wait(2000);
    if (! transactionWasBalanced)
    {
        // Cleanup for the deliberately old-red implementation, which retained
        // this thread's recursive writer-lock level after the exception.
        subject.requestMultibandTopologyReset();
    }
    topologyProbe.join();

    REQUIRE(listenerExceptionEscaped);
    CHECK(transactionWasBalanced);
    CHECK((subject.getMultibandTopologyGenerationForTesting() & 1u) == 0u);
    juce::MemoryBlock recoveredState;
    subject.getStateInformation(recoveredState);
    CHECK(recoveredState.getSize() > 0);
}

TEST_CASE("Nested topology edits retain one complete host snapshot",
          "[state][host][topology][transaction][nested]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    const auto oldTarget = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    const auto intermediateTarget = ParameterIDAndName::getIDString(DRIVE_ID, 2);
    const auto finalTarget = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    setPlainParameter(subject, NUM_BANDS_ID, 2.0f);
    subject.assignLfoToTarget(0, oldTarget);

    struct SavedSignature
    {
        float bandCount = 0.0f;
        bool hasOldTarget = false;
        bool hasIntermediateTarget = false;
        bool hasFinalTarget = false;
    };

    const auto captureSignature = [&] ()
    {
        juce::MemoryBlock state;
        subject.getStateInformation(state);
        FireAudioProcessor restored;
        restored.setStateInformation(state.getData(),
                                     static_cast<int>(state.getSize()));
        const auto routings = restored.getLfoManager().getModulationRoutingsCopy();
        return SavedSignature {
            getPlainParameter(restored, NUM_BANDS_ID),
            countRoutingsForTarget(routings, oldTarget) > 0,
            countRoutingsForTarget(routings, intermediateTarget) > 0,
            countRoutingsForTarget(routings, finalTarget) > 0
        };
    };

    subject.beginMultibandTopologyEdit();
    setPlainParameter(subject, NUM_BANDS_ID, 3.0f);
    subject.clearModulationForParameter(oldTarget);
    subject.assignLfoToTarget(1, intermediateTarget);

    subject.beginMultibandTopologyEdit();
    setPlainParameter(subject, NUM_BANDS_ID, 1.0f);
    subject.clearModulationForParameter(intermediateTarget);
    subject.assignLfoToTarget(2, finalTarget);

    const auto duringInnerEdit = captureSignature();
    subject.requestMultibandTopologyReset();
    const auto afterInnerPublish = captureSignature();
    subject.requestMultibandTopologyReset();
    const auto afterOuterPublish = captureSignature();

    for (const auto& staged : { duringInnerEdit, afterInnerPublish })
    {
        CHECK(staged.bandCount == Catch::Approx(2.0f));
        CHECK(staged.hasOldTarget);
        CHECK_FALSE(staged.hasIntermediateTarget);
        CHECK_FALSE(staged.hasFinalTarget);
    }

    CHECK(afterOuterPublish.bandCount == Catch::Approx(1.0f));
    CHECK_FALSE(afterOuterPublish.hasOldTarget);
    CHECK_FALSE(afterOuterPublish.hasIntermediateTarget);
    CHECK(afterOuterPublish.hasFinalTarget);
}

TEST_CASE("Invalid known host parameters reject the complete state transaction",
          "[state][host][corrupt][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 26.0f);
    processor.setSavedWidth(1410);
    processor.setSavedHeight(805);
    processor.statePresets.setCurrentPresetId(4);

    const auto shape = makeLfoShape(0.36f, 0.86f, 0.54f);
    processor.getLfoManager().setLfoData(1, shape);
    processor.assignLfoToTarget(1, driveID);
    processor.setModulationDepth(driveID, 0.41f);

    juce::MemoryBlock validState;
    processor.getStateInformation(validState);
    auto baselineXml = juce::AudioProcessor::getXmlFromBinary(
        validState.getData(), static_cast<int>(validState.getSize()));
    REQUIRE(baselineXml != nullptr);

    for (const juce::String invalidValue : { "nan", "oops", "1e4294967296" })
    {
        CAPTURE(invalidValue);
        auto invalidXml = std::make_unique<juce::XmlElement>(*baselineXml);
        auto* drive = findHostParameter(*invalidXml, processor, driveID);
        REQUIRE(drive != nullptr);
        drive->setAttribute("value", invalidValue);

        auto* otherState = invalidXml->getChildByName("otherState");
        REQUIRE(otherState != nullptr);
        otherState->setAttribute("currentPresetID", 0);
        otherState->setAttribute("editorWidth", 999);
        otherState->setAttribute("editorHeight", 600);

        if (auto* lfoState = invalidXml->getChildByName("LFO_STATE"))
            invalidXml->removeChildElement(lfoState, true);
        if (auto* routingState = invalidXml->getChildByName("MODULATION_STATE"))
            routingState->deleteAllChildElements();

        juce::MemoryBlock invalidState;
        juce::AudioProcessor::copyXmlToBinary(*invalidXml, invalidState);
        processor.setStateInformation(invalidState.getData(),
                                      static_cast<int>(invalidState.getSize()));

        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(26.0f));
        CHECK(processor.getSavedWidth() == 1410);
        CHECK(processor.getSavedHeight() == 805);
        CHECK(processor.statePresets.getCurrentPresetId() == 4);

        const auto shapes = processor.getLfoManager().getLfoDataCopy();
        REQUIRE(shapes.size() == 4);
        REQUIRE(shapes[1].points.size() == 3);
        CHECK(shapes[1].points[1].x == Catch::Approx(0.36f));
        CHECK(shapes[1].points[1].y == Catch::Approx(0.86f));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* routing = findRouting(routings, driveID);
        REQUIRE(routing != nullptr);
        CHECK(routing->sourceLfoIndex == 1);
        CHECK(routing->depth == Catch::Approx(0.41f));
    }
}

TEST_CASE("Legacy preset equivalence compares effective LFO and routing defaults",
          "[preset][state][equivalence][legacy][lfo]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    setPlainParameter(processor, driveID, 34.0f);
    juce::XmlElement legacyPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, legacyPreset);
    legacyPreset.removeAttribute("presetFormatVersion");
    if (auto* lfoState = legacyPreset.getChildByName("LFO_STATE"))
        legacyPreset.removeChildElement(lfoState, true);
    if (auto* routingState = legacyPreset.getChildByName("MODULATION_STATE"))
        legacyPreset.removeChildElement(routingState, true);

    setPlainParameter(processor, driveID, 61.0f);
    processor.getLfoManager().setLfoData(0, makeLfoShape(0.4f, 0.9f, 0.3f));
    processor.assignLfoToTarget(0, driveID);
    state::loadStateFromXml(legacyPreset, processor);
    CHECK(processor.isCurrentStateEquivalentToPreset(legacyPreset));

    processor.getLfoManager().setLfoData(0, makeLfoShape(0.62f, 0.18f, 0.3f));
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(legacyPreset));

    state::loadStateFromXml(legacyPreset, processor);
    REQUIRE(processor.isCurrentStateEquivalentToPreset(legacyPreset));
    processor.assignLfoToTarget(2, driveID);
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(legacyPreset));

    state::loadStateFromXml(legacyPreset, processor);
    auto malformedFallbackPreset = std::make_unique<juce::XmlElement>(legacyPreset);
    malformedFallbackPreset->setAttribute(driveID, "0.5oops");
    state::loadStateFromXml(*malformedFallbackPreset, processor);
    CHECK(processor.isCurrentStateEquivalentToPreset(*malformedFallbackPreset));

    // Smoothness originally lived only in LFO XML. The loader promotes it to
    // APVTS, so a missing lfoSmooth parameter attribute must still compare
    // clean against the promoted value.
    juce::XmlElement legacySmoothnessPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, legacySmoothnessPreset);
    legacySmoothnessPreset.removeAttribute("presetFormatVersion");
    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0);
    legacySmoothnessPreset.removeAttribute(smoothnessID);
    auto* legacyLfoState = legacySmoothnessPreset.getChildByName("LFO_STATE");
    REQUIRE(legacyLfoState != nullptr);
    auto* legacyLfo = legacyLfoState->getChildByAttribute("index", "0");
    REQUIRE(legacyLfo != nullptr);
    legacyLfo->setAttribute("smoothness", "0.37");
    state::loadStateFromXml(legacySmoothnessPreset, processor);
    CHECK(getPlainParameter(processor, smoothnessID) == Catch::Approx(0.37f));
    CHECK(processor.isCurrentStateEquivalentToPreset(legacySmoothnessPreset));
}

TEST_CASE("Bundled legacy LFO preset is clean after semantic model loading",
          "[preset][state][equivalence][legacy][lfo][fixture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto fixture = findProjectRoot().getChildFile("tests/Presets/lfo_sawup.fire");
    REQUIRE(fixture.existsAsFile());
    auto preset = juce::XmlDocument::parse(fixture);
    REQUIRE(preset != nullptr);

    FireAudioProcessor processor;
    state::loadStateFromXml(*preset, processor);

    // This fixture predates LFO smoothness and routing bypass persistence. Its
    // short decimal strings and omitted default attributes must compare as the
    // exact model produced by the legacy loader.
    CHECK(processor.isCurrentStateEquivalentToPreset(*preset));

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    processor.getLfoManager().toggleBypassForRouting(driveID);
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(*preset));

    state::loadStateFromXml(*preset, processor);
    auto changedShape = processor.getLfoManager().getLfoDataCopy()[0];
    REQUIRE(changedShape.points.size() > 1);
    changedShape.points[1].y = juce::jlimit(0.0f, 1.0f, changedShape.points[1].y + 0.01f);
    processor.getLfoManager().setLfoData(0, changedShape);
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(*preset));
}

TEST_CASE("Host parameter migration clamps ranges and rejects corrupt manifests",
          "[state][host][migration][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("out-of-range denormalised values are clamped")
    {
        FireAudioProcessor source;
        juce::MemoryBlock stateBlock;
        source.getStateInformation(stateBlock);
        auto xml = juce::AudioProcessor::getXmlFromBinary(
            stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        REQUIRE(xml != nullptr);

        const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        auto* drive = findHostParameter(*xml, source, driveID);
        auto* numBands = findHostParameter(*xml, source, NUM_BANDS_ID);
        REQUIRE(drive != nullptr);
        REQUIRE(numBands != nullptr);
        drive->setAttribute("value", "100000");
        numBands->setAttribute("value", "-999");
        juce::AudioProcessor::copyXmlToBinary(*xml, stateBlock);

        FireAudioProcessor restored;
        restored.setStateInformation(stateBlock.getData(),
                                     static_cast<int>(stateBlock.getSize()));
        const auto* driveParameter = restored.treeState.getParameter(driveID);
        const auto* numBandsParameter = restored.treeState.getParameter(NUM_BANDS_ID);
        REQUIRE(driveParameter != nullptr);
        REQUIRE(numBandsParameter != nullptr);
        CHECK(getPlainParameter(restored, driveID)
              == Catch::Approx(driveParameter->getNormalisableRange().end));
        CHECK(getPlainParameter(restored, NUM_BANDS_ID)
              == Catch::Approx(numBandsParameter->getNormalisableRange().start));
    }

    SECTION("duplicate IDs and an invalid declared count reject the transaction")
    {
        FireAudioProcessor source;
        const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        setPlainParameter(source, driveID, 28.0f);
        juce::MemoryBlock stateBlock;
        source.getStateInformation(stateBlock);
        auto xml = juce::AudioProcessor::getXmlFromBinary(
            stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        REQUIRE(xml != nullptr);
        auto* parameters = xml->getChildByName(source.treeState.state.getType().toString());
        auto* originalDrive = findHostParameter(*xml, source, driveID);
        REQUIRE(parameters != nullptr);
        REQUIRE(originalDrive != nullptr);

        auto duplicateDrive = std::make_unique<juce::XmlElement>(*originalDrive);
        duplicateDrive->setAttribute("value", 73.0);
        parameters->addChildElement(duplicateDrive.release());
        auto* unknown = parameters->createNewChildElement(originalDrive->getTagName());
        unknown->setAttribute("id", "futureUnknownParameter");
        unknown->setAttribute("value", 9.0);
        juce::AudioProcessor::copyXmlToBinary(*xml, stateBlock);

        FireAudioProcessor restored;
        setPlainParameter(restored, driveID, 13.0f);
        restored.setStateInformation(stateBlock.getData(),
                                     static_cast<int>(stateBlock.getSize()));
        CHECK(getPlainParameter(restored, driveID) == Catch::Approx(13.0f));

        const auto canonicalState = restored.treeState.copyState();
        CHECK(countValueTreeChildrenWithID(canonicalState, driveID) == 1);
        CHECK(countValueTreeChildrenWithID(canonicalState, "futureUnknownParameter") == 0);
    }

    SECTION("unique future IDs are ignored when the declared count is coherent")
    {
        FireAudioProcessor source;
        const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        setPlainParameter(source, driveID, 28.0f);
        juce::MemoryBlock stateBlock;
        source.getStateInformation(stateBlock);
        auto xml = juce::AudioProcessor::getXmlFromBinary(
            stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        REQUIRE(xml != nullptr);
        auto* parameters = xml->getChildByName(
            source.treeState.state.getType().toString());
        REQUIRE(parameters != nullptr);
        auto* knownParameter = findHostParameter(*xml, source, driveID);
        REQUIRE(knownParameter != nullptr);

        auto* unknown = parameters->createNewChildElement(
            knownParameter->getTagName());
        unknown->setAttribute("id", "futureUnknownParameter");
        unknown->setAttribute("value", 9.0);
        xml->setAttribute("savedParameterCount",
                          parameters->getNumChildElements());
        juce::AudioProcessor::copyXmlToBinary(*xml, stateBlock);

        FireAudioProcessor restored;
        restored.setStateInformation(stateBlock.getData(),
                                     static_cast<int>(stateBlock.getSize()));
        CHECK(getPlainParameter(restored, driveID) == Catch::Approx(28.0f));

        const auto canonicalState = restored.treeState.copyState();
        CHECK(countValueTreeChildrenWithID(canonicalState,
                                           "futureUnknownParameter") == 0);
    }
}

TEST_CASE("Host state round-trip preserves stable preset path identity",
          "[state][host][preset][identity][roundtrip]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    source.statePresets.setCurrentPresetKey("Factory/Lead.v2.fire");

    juce::MemoryBlock stateBlock;
    source.getStateInformation(stateBlock);
    FireAudioProcessor restored;
    restored.setStateInformation(stateBlock.getData(),
                                 static_cast<int>(stateBlock.getSize()));

    CHECK(restored.statePresets.getCurrentPresetKey() == "Factory/Lead.v2.fire");
}

TEST_CASE("State loaders enforce one modulation routing per target",
          "[state][preset][host][lfo][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto targetID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    SECTION("preset XML")
    {
        FireAudioProcessor processor;
        juce::XmlElement preset { "WINGSFIRE" };
        state::saveStateToXml(processor, preset);
        preset.removeAttribute("presetFormatVersion");
        auto* routingState = preset.getChildByName("MODULATION_STATE");
        REQUIRE(routingState != nullptr);
        appendRouting(*routingState, 1, targetID, 0.25f);
        appendRouting(*routingState, 3, targetID, -0.75f);

        state::loadStateFromXml(preset, processor);
        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        CHECK(countRoutingsForTarget(routings, targetID) == 1);
    }

    SECTION("versioned host state is rejected atomically")
    {
        FireAudioProcessor source;
        juce::MemoryBlock stateBlock;
        source.getStateInformation(stateBlock);
        auto xml = juce::AudioProcessor::getXmlFromBinary(
            stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        REQUIRE(xml != nullptr);
        auto* routingState = xml->getChildByName("MODULATION_STATE");
        REQUIRE(routingState != nullptr);
        appendRouting(*routingState, 0, targetID, 0.2f);
        appendRouting(*routingState, 2, targetID, -0.4f);
        juce::AudioProcessor::copyXmlToBinary(*xml, stateBlock);

        FireAudioProcessor restored;
        restored.setStateInformation(stateBlock.getData(),
                                     static_cast<int>(stateBlock.getSize()));
        const auto routings = restored.getLfoManager().getModulationRoutingsCopy();
        CHECK(countRoutingsForTarget(routings, targetID) == 0);
    }
}

TEST_CASE("A-B swapping preserves multiband, LFO and modulation state",
          "[state][ab][roundtrip][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto frequencyID = ParameterIDAndName::getIDString(FREQ_ID, 0);

    const auto shapeA = makeLfoShape(0.31f, 0.83f, 0.22f);
    setPlainParameter(processor, NUM_BANDS_ID, 2.0f);
    setPlainParameter(processor, frequencyID, 1180.0f);
    setPlainParameter(processor, driveID, 21.0f);
    processor.getLfoManager().setLfoData(0, shapeA);
    processor.assignLfoToTarget(0, driveID);
    processor.setModulationDepth(driveID, 0.3f);
    processor.stateAB.copyAB();

    const auto shapeB = makeLfoShape(0.73f, 0.24f, 0.79f);
    setPlainParameter(processor, NUM_BANDS_ID, 3.0f);
    setPlainParameter(processor, frequencyID, 4200.0f);
    setPlainParameter(processor, driveID, 3.0f);
    processor.getLfoManager().setLfoData(0, shapeB);
    processor.clearModulationForParameter(driveID);
    processor.assignLfoToTarget(3, driveID);
    processor.setModulationDepth(driveID, -0.65f);

    processor.stateAB.toggleAB();
    CHECK(getPlainParameter(processor, NUM_BANDS_ID) == Catch::Approx(2.0f));
    CHECK(getPlainParameter(processor, frequencyID) == Catch::Approx(1180.0f));
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(21.0f));
    auto shapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes[0].points.size() == 3);
    CHECK(shapes[0].points[1].x == Catch::Approx(0.31f));
    auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    auto* routing = findRouting(routings, driveID);
    REQUIRE(routing != nullptr);
    CHECK(routing->sourceLfoIndex == 0);
    CHECK(routing->depth == Catch::Approx(0.3f));

    processor.stateAB.toggleAB();
    CHECK(getPlainParameter(processor, NUM_BANDS_ID) == Catch::Approx(3.0f));
    CHECK(getPlainParameter(processor, frequencyID) == Catch::Approx(4200.0f));
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(3.0f));
    shapes = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes[0].points.size() == 3);
    CHECK(shapes[0].points[1].x == Catch::Approx(0.73f));
    routings = processor.getLfoManager().getModulationRoutingsCopy();
    routing = findRouting(routings, driveID);
    REQUIRE(routing != nullptr);
    CHECK(routing->sourceLfoIndex == 3);
    CHECK(routing->depth == Catch::Approx(-0.65f));
}

TEST_CASE("Host sessions preserve the inactive A-B snapshot and active side",
          "[state][host][ab][roundtrip][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    const auto shapeB = makeLfoShape(0.24f, 0.81f, 0.18f);
    setPlainParameter(source, driveID, 14.0f);
    source.getLfoManager().setLfoData(0, shapeB);
    source.assignLfoToTarget(0, driveID);
    source.setModulationDepth(driveID, 0.23f);
    source.stateAB.copyAB();

    const auto shapeA = makeLfoShape(0.68f, 0.29f, 0.76f);
    setPlainParameter(source, driveID, 72.0f);
    source.getLfoManager().setLfoData(0, shapeA);
    source.clearModulationForParameter(driveID);
    source.assignLfoToTarget(2, driveID);
    source.setModulationDepth(driveID, -0.61f);
    REQUIRE(source.stateAB.isCurrentA());

    const auto checkLiveState = [&](FireAudioProcessor& processor,
                                    float expectedDrive,
                                    float expectedMiddleX,
                                    int expectedSource,
                                    float expectedDepth)
    {
        CHECK(getPlainParameter(processor, driveID) == Catch::Approx(expectedDrive));
        const auto shapes = processor.getLfoManager().getLfoDataCopy();
        REQUIRE(shapes.size() == 4);
        REQUIRE(shapes[0].points.size() == 3);
        CHECK(shapes[0].points[1].x == Catch::Approx(expectedMiddleX));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* routing = findRouting(routings, driveID);
        REQUIRE(routing != nullptr);
        CHECK(routing->sourceLfoIndex == expectedSource);
        CHECK(routing->depth == Catch::Approx(expectedDepth));
    };

    SECTION("session saved while A is active")
    {
        juce::MemoryBlock hostState;
        source.getStateInformation(hostState);
        FireAudioProcessor restored;
        restored.setStateInformation(hostState.getData(),
                                     static_cast<int>(hostState.getSize()));

        REQUIRE(restored.stateAB.isCurrentA());
        checkLiveState(restored, 72.0f, 0.68f, 2, -0.61f);

        restored.stateAB.toggleAB();
        CHECK_FALSE(restored.stateAB.isCurrentA());
        checkLiveState(restored, 14.0f, 0.24f, 0, 0.23f);
    }

    SECTION("session saved while B is active")
    {
        source.stateAB.toggleAB();
        REQUIRE_FALSE(source.stateAB.isCurrentA());
        checkLiveState(source, 14.0f, 0.24f, 0, 0.23f);

        juce::MemoryBlock hostState;
        source.getStateInformation(hostState);
        FireAudioProcessor restored;
        restored.setStateInformation(hostState.getData(),
                                     static_cast<int>(hostState.getSize()));

        REQUIRE_FALSE(restored.stateAB.isCurrentA());
        checkLiveState(restored, 14.0f, 0.24f, 0, 0.23f);

        restored.stateAB.toggleAB();
        CHECK(restored.stateAB.isCurrentA());
        checkLiveState(restored, 72.0f, 0.68f, 2, -0.61f);
    }
}

TEST_CASE("A-B state replacement acquires its state lock inside the topology transaction",
          "[state][host][ab][topology][transaction][locking]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;

    const auto requireTopologyBeforeStateLock = [&](const auto& operation)
    {
        bool hookWasCalled = false;
        std::uint32_t generationWhenStateLockWasAcquired = 0;
        processor.stateAB.setMutationLockAcquiredHookForTesting([&]
        {
            hookWasCalled = true;
            generationWhenStateLockWasAcquired =
                processor.getMultibandTopologyGenerationForTesting();
        });

        operation();

        REQUIRE(hookWasCalled);
        CHECK((generationWhenStateLockWasAcquired & 1u) != 0u);
        CHECK((processor.getMultibandTopologyGenerationForTesting() & 1u)
              == 0u);
    };

    SECTION("toggle")
    {
        NonParameterStateCapture host(processor);
        requireTopologyBeforeStateLock([&] { processor.stateAB.toggleAB(); });
        REQUIRE(host.states.size() == 1);
        REQUIRE(host.generations.size() == 1);
        CHECK((host.generations.front() & 1u) == 0u);
    }

    SECTION("host restore")
    {
        juce::MemoryBlock hostState;
        processor.getStateInformation(hostState);
        REQUIRE(hostState.getSize() > 0);
        requireTopologyBeforeStateLock([&]
        {
            processor.setStateInformation(hostState.getData(),
                                          static_cast<int>(hostState.getSize()));
        });
    }
}

TEST_CASE("Copying A-B state serializes without publishing an audio topology change",
          "[state][ab][topology][copy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(processor, driveID, 63.0f);
    const auto generationBeforeCopy =
        processor.getMultibandTopologyGenerationForTesting();
    bool anotherThreadCouldAcquireWriterLock = true;
    processor.stateAB.setMutationLockAcquiredHookForTesting([&]
    {
        std::thread writerProbe([&]
        {
            anotherThreadCouldAcquireWriterLock =
                processor.tryAcquireMultibandTopologyWriterLockForTesting();
        });
        writerProbe.join();
    });

    processor.stateAB.copyAB(false);

    CHECK_FALSE(anotherThreadCouldAcquireWriterLock);
    CHECK(processor.getMultibandTopologyGenerationForTesting()
          == generationBeforeCopy);
    setPlainParameter(processor, driveID, 17.0f);
    processor.stateAB.toggleAB();
    CHECK(getPlainParameter(processor, driveID) == Catch::Approx(63.0f));
}

TEST_CASE("Host saves keep active and inactive A-B states in one generation",
          "[state][host][ab][topology][transaction][reentrant]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);

    setPlainParameter(source, driveID, 14.0f);
    source.stateAB.copyAB(false);
    setPlainParameter(source, driveID, 72.0f);
    REQUIRE(source.stateAB.isCurrentA());

    bool hookWasCalled = false;
    source.setHostStateMainCaptureHookForTesting([&]
    {
        hookWasCalled = true;
        source.stateAB.toggleAB();
    });

    juce::MemoryBlock savedState;
    source.getStateInformation(savedState);
    REQUIRE(hookWasCalled);
    REQUIRE(savedState.getSize() > 0);
    CHECK_FALSE(source.stateAB.isCurrentA());
    CHECK(getPlainParameter(source, driveID) == Catch::Approx(14.0f));

    FireAudioProcessor restored;
    restored.setStateInformation(savedState.getData(),
                                 static_cast<int>(savedState.getSize()));
    CHECK(restored.stateAB.isCurrentA());
    CHECK(getPlainParameter(restored, driveID) == Catch::Approx(72.0f));

    restored.stateAB.toggleAB();
    CHECK_FALSE(restored.stateAB.isCurrentA());
    CHECK(getPlainParameter(restored, driveID) == Catch::Approx(14.0f));
}

TEST_CASE("Truncated A-B snapshots fall back to the restored live state",
          "[state][host][ab][corrupt][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    setPlainParameter(source, driveID, 64.0f);

    juce::MemoryBlock hostState;
    source.getStateInformation(hostState);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        hostState.getData(), static_cast<int>(hostState.getSize()));
    REQUIRE(xml != nullptr);

    auto* abState = xml->getChildByName("AB_STATE");
    REQUIRE(abState != nullptr);
    abState->removeAllAttributes();
    abState->deleteAllChildElements();
    abState->setAttribute("currentSideIsA", false);
    abState->setAttribute(driveID, 0.12);

    juce::MemoryBlock truncatedState;
    juce::AudioProcessor::copyXmlToBinary(*xml, truncatedState);

    FireAudioProcessor restored;
    restored.setStateInformation(truncatedState.getData(),
                                 static_cast<int>(truncatedState.getSize()));

    CHECK(restored.stateAB.isCurrentA());
    CHECK(getPlainParameter(restored, driveID) == Catch::Approx(64.0f));

    restored.stateAB.toggleAB();
    CHECK_FALSE(restored.stateAB.isCurrentA());
    CHECK(getPlainParameter(restored, driveID) == Catch::Approx(64.0f));
}
