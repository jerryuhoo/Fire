#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

struct EditorUpdateCheckLifecycleTestAccess final
{
    using AlertCompletion = std::function<void(int)>;
    using AlertFactory = std::function<juce::ScopedMessageBox(
        const juce::MessageBoxOptions&,
        AlertCompletion)>;

    static std::uint64_t captureSession(FireAudioProcessorEditor& editor)
    {
        return editor.captureUpdateCheckSession();
    }

    static bool publish(FireAudioProcessorEditor& editor,
                        const juce::String& version,
                        std::uint64_t sessionGeneration)
    {
        return editor.publishAvailableUpdate(version, sessionGeneration);
    }

    static juce::String consume(FireAudioProcessorEditor& editor)
    {
        return editor.takeAvailableUpdate();
    }

    static void flushAsyncUpdate(FireAudioProcessorEditor& editor)
    {
        editor.handleUpdateNowIfNeeded();
    }

    static void setAlertFactory(
        FireAudioProcessorEditor& editor,
        AlertFactory factory)
    {
        editor.availableUpdateAlertFactoryForTesting = std::move(factory);
    }

    static void setUrlLauncher(
        FireAudioProcessorEditor& editor,
        std::function<void(const juce::String&)> launcher)
    {
        editor.availableUpdateUrlLauncherForTesting = std::move(launcher);
    }

    static bool isAlertActive(const FireAudioProcessorEditor& editor)
    {
        return editor.availableUpdateAlertActive;
    }

    static std::uint64_t alertGeneration(
        const FireAudioProcessorEditor& editor)
    {
        return editor.availableUpdateAlertGeneration;
    }

    static std::uint64_t alertDismissalCount(
        const FireAudioProcessorEditor& editor)
    {
        return editor.availableUpdateAlertDismissalCountForTesting;
    }
};

namespace
{
void showEditor(FireAudioProcessorEditor& editor)
{
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    REQUIRE(editor.isShowing());
}
} // namespace

TEST_CASE("Editor update result survives only the initial no-peer construction",
          "[editor][update-check][ui][session][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);

    std::vector<EditorUpdateCheckLifecycleTestAccess::AlertCompletion>
        alertCompletions;
    EditorUpdateCheckLifecycleTestAccess::setAlertFactory(
        editor,
        [&](const juce::MessageBoxOptions&,
            EditorUpdateCheckLifecycleTestAccess::AlertCompletion completion)
        {
            alertCompletions.push_back(std::move(completion));
            return juce::ScopedMessageBox {};
        });

    const auto initialSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(editor);
    REQUIRE(initialSession != 0);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v99.0.0", initialSession));

    // AudioProcessorEditor is normally constructed before its host peer. A
    // result reaching this provisional state must wait for the first window,
    // not be mistaken for a stale hidden-editor result.
    EditorUpdateCheckLifecycleTestAccess::flushAsyncUpdate(editor);
    CHECK(alertCompletions.empty());

    showEditor(editor);
    CHECK(EditorUpdateCheckLifecycleTestAccess::captureSession(editor)
          == initialSession);
    EditorUpdateCheckLifecycleTestAccess::flushAsyncUpdate(editor);
    REQUIRE(alertCompletions.size() == 1);
    CHECK(EditorUpdateCheckLifecycleTestAccess::isAlertActive(editor));
    alertCompletions.front()(0);
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(editor));

    editor.removeFromDesktop();
}

TEST_CASE("Editor hide invalidates pending and late update-check results",
          "[editor][update-check][ui][session][hidden][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    showEditor(editor);

    const auto oldSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(editor);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v98.0.0", oldSession));

    editor.setVisible(false);
    editor.setVisible(true);
    REQUIRE(editor.isShowing());

    CHECK(EditorUpdateCheckLifecycleTestAccess::consume(editor).isEmpty());
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v98.1.0", oldSession));

    const auto replacementSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(editor);
    REQUIRE(replacementSession != oldSession);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v99.1.0", replacementSession));
    CHECK(EditorUpdateCheckLifecycleTestAccess::consume(editor)
          == "v99.1.0");

    editor.removeFromDesktop();
}

TEST_CASE("Editor disable invalidates pending results and active update alerts",
          "[editor][update-check][ui][session][disabled][alert][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    showEditor(editor);

    std::vector<EditorUpdateCheckLifecycleTestAccess::AlertCompletion>
        alertCompletions;
    juce::StringArray launchedUrls;
    EditorUpdateCheckLifecycleTestAccess::setAlertFactory(
        editor,
        [&](const juce::MessageBoxOptions&,
            EditorUpdateCheckLifecycleTestAccess::AlertCompletion completion)
        {
            alertCompletions.push_back(std::move(completion));
            return juce::ScopedMessageBox {};
        });
    EditorUpdateCheckLifecycleTestAccess::setUrlLauncher(
        editor,
        [&](const juce::String& url)
        {
            launchedUrls.add(url);
        });

    const auto pendingSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(editor);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v99.5.0", pendingSession));

    editor.setEnabled(false);
    CHECK(EditorUpdateCheckLifecycleTestAccess::captureSession(editor) == 0);
    editor.setEnabled(true);
    EditorUpdateCheckLifecycleTestAccess::flushAsyncUpdate(editor);

    CHECK(alertCompletions.empty());
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v99.5.1", pendingSession));

    const auto alertSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(editor);
    REQUIRE(alertSession != pendingSession);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v99.6.0", alertSession));
    EditorUpdateCheckLifecycleTestAccess::flushAsyncUpdate(editor);
    REQUIRE(alertCompletions.size() == 1);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(editor));

    const auto alertGeneration =
        EditorUpdateCheckLifecycleTestAccess::alertGeneration(editor);
    editor.setEnabled(false);
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(editor));
    CHECK(EditorUpdateCheckLifecycleTestAccess::alertGeneration(editor)
          != alertGeneration);
    CHECK(EditorUpdateCheckLifecycleTestAccess::alertDismissalCount(editor)
          == 1);
    editor.setEnabled(true);

    // Re-enabling the same editor must not revive either the worker session or
    // the OK callback belonging to the disabled alert.
    alertCompletions.front()(1);
    CHECK(launchedUrls.isEmpty());
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v99.6.1", alertSession));

    const auto replacementSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(editor);
    REQUIRE(replacementSession != alertSession);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        editor, "v99.7.0", replacementSession));
    EditorUpdateCheckLifecycleTestAccess::flushAsyncUpdate(editor);
    REQUIRE(alertCompletions.size() == 2);
    alertCompletions[1](0);
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(editor));

    editor.removeFromDesktop();
}

TEST_CASE("Editor update alert is scoped to its visible editor session",
          "[editor][update-check][ui][session][async][alert][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    showEditor(*editor);

    std::vector<EditorUpdateCheckLifecycleTestAccess::AlertCompletion>
        alertCompletions;
    std::vector<juce::MessageBoxOptions> alertOptions;
    juce::StringArray launchedUrls;
    EditorUpdateCheckLifecycleTestAccess::setAlertFactory(
        *editor,
        [&](const juce::MessageBoxOptions& options,
            EditorUpdateCheckLifecycleTestAccess::AlertCompletion completion)
        {
            alertOptions.push_back(options);
            alertCompletions.push_back(std::move(completion));
            return juce::ScopedMessageBox {};
        });
    EditorUpdateCheckLifecycleTestAccess::setUrlLauncher(
        *editor,
        [&](const juce::String& url)
        {
            launchedUrls.add(url);
        });

    const auto waitForAlerts = [&](size_t count)
    {
        // Native paints and timers may occupy the first dispatch slice on a
        // busy CI worker. Wait for the actual async result, with a deadline.
        const auto started = juce::Time::getMillisecondCounter();
        while (alertCompletions.size() < count
               && juce::Time::getMillisecondCounter() - started < 1000u)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    };
    const auto oldSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(*editor);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        *editor, "v97.0.0", oldSession));

    // Hide before AsyncUpdater consumes the result, then reuse the same editor.
    // The old notification may still run, but it must find no presentable data.
    editor->setVisible(false);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    CHECK(alertCompletions.empty());
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(
        *editor));

    const auto currentSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(*editor);
    REQUIRE(currentSession != oldSession);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        *editor, "v99.2.0", currentSession));
    waitForAlerts(1);
    REQUIRE(alertCompletions.size() == 1);
    REQUIRE(alertOptions.size() == 1);
    CHECK(alertOptions.front().getAssociatedComponent() == editor.get());
    CHECK(alertOptions.front().getParentComponent() == editor.get());
    CHECK(alertOptions.front().getTitle() == "New Version");
    CHECK(alertOptions.front().getNumButtons() == 2);
    CHECK(alertOptions.front().getButtonText(0) == "Download");
    CHECK(alertOptions.front().getButtonText(1) == "Cancel");
    CHECK(alertOptions.front().getMessage().contains("v99.2.0"));
    CHECK(EditorUpdateCheckLifecycleTestAccess::isAlertActive(*editor));

    const auto firstAlertGeneration =
        EditorUpdateCheckLifecycleTestAccess::alertGeneration(*editor);
    editor->setVisible(false);
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(
        *editor));
    CHECK(EditorUpdateCheckLifecycleTestAccess::alertGeneration(*editor)
          != firstAlertGeneration);
    CHECK(EditorUpdateCheckLifecycleTestAccess::alertDismissalCount(*editor)
          == 1);

    // Closing the editor invalidates the alert before ScopedMessageBox::close
    // can deliver its late completion. Even an OK result has no side effect.
    alertCompletions.front()(1);
    CHECK(launchedUrls.isEmpty());

    editor->setVisible(true);
    REQUIRE(editor->isShowing());
    const auto replacementSession =
        EditorUpdateCheckLifecycleTestAccess::captureSession(*editor);
    REQUIRE(replacementSession != currentSession);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        *editor, "v99.3.0", replacementSession));
    waitForAlerts(2);
    REQUIRE(alertCompletions.size() == 2);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(*editor));

    const auto replacementAlertGeneration =
        EditorUpdateCheckLifecycleTestAccess::alertGeneration(*editor);

    // A callback from the closed first alert must not clear the newer member
    // handle, even though the editor is visible again when it finally arrives.
    alertCompletions.front()(1);
    CHECK(launchedUrls.isEmpty());
    CHECK(EditorUpdateCheckLifecycleTestAccess::isAlertActive(*editor));
    CHECK(EditorUpdateCheckLifecycleTestAccess::alertGeneration(*editor)
          == replacementAlertGeneration);
    CHECK(EditorUpdateCheckLifecycleTestAccess::alertDismissalCount(*editor)
          == 1);

    alertCompletions[1](1);
    REQUIRE(launchedUrls.size() == 1);
    CHECK(launchedUrls[0].endsWith("v99.3.0"));
    CHECK_FALSE(EditorUpdateCheckLifecycleTestAccess::isAlertActive(
        *editor));
    CHECK(EditorUpdateCheckLifecycleTestAccess::alertDismissalCount(*editor)
          == 2);

    // A displayed replacement is also closed during editor destruction. Its
    // SafePointer-bound completion cannot access the deleted editor or launch.
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        *editor, "v99.4.0", replacementSession));
    waitForAlerts(3);
    REQUIRE(alertCompletions.size() == 3);
    auto completionAfterDestruction = alertCompletions[2];
    editor.reset();
    completionAfterDestruction(1);
    CHECK(launchedUrls.size() == 1);
}

TEST_CASE("Editor update alert tolerates synchronous completion deleting its owner",
          "[editor][update-check][ui][alert][reentrant][destruction][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    showEditor(*editor);

    int launchCount = 0;
    juce::String launchedUrl;
    EditorUpdateCheckLifecycleTestAccess::setUrlLauncher(
        *editor,
        [&](const juce::String& url)
        {
            ++launchCount;
            launchedUrl = url;
            editor.reset();
        });
    EditorUpdateCheckLifecycleTestAccess::setAlertFactory(
        *editor,
        [](const juce::MessageBoxOptions&,
           EditorUpdateCheckLifecycleTestAccess::AlertCompletion completion)
        {
            completion(1);
            return juce::ScopedMessageBox {};
        });

    const auto session =
        EditorUpdateCheckLifecycleTestAccess::captureSession(*editor);
    REQUIRE(EditorUpdateCheckLifecycleTestAccess::publish(
        *editor, "v101.0.0", session));

    auto* const rawEditor = editor.get();
    EditorUpdateCheckLifecycleTestAccess::flushAsyncUpdate(*rawEditor);

    CHECK(editor == nullptr);
    CHECK(launchCount == 1);
    CHECK(launchedUrl.endsWith("v101.0.0"));
}
