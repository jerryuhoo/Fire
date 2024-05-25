#include <melatonin_inspector/melatonin_inspector.h>

//==============================================================================
class DummyApp : public juce::JUCEApplication
{
public:
    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow (getApplicationName()));
        // on CI, we start this as a background process
        juce::Process::makeForegroundProcess();
        mainWindow->toFront(true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    const juce::String getApplicationName() override { return "dummy"; }
    const juce::String getApplicationVersion() override { return "v1"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String&) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name) : DocumentWindow (name,
            juce::Colours::lightgrey,
            DocumentWindow::allButtons)
        {
            centreWithSize (300, 200);
            setVisible (true);
            inspector.setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
        melatonin::Inspector inspector { *this, true };

    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (DummyApp)
