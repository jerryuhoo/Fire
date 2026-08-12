/*
  ==============================================================================

    Preset.h
    Created: 12 Jul 2020 9:06:49pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once
#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

#include "../../GUI/InterfaceDefines.h"
#include "../../GUI/LookAndFeel.h"
#include "../../GUI/SettingsComponent.h"
#include "../../Utility/VersionInfo.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace state
{

    //==============================================================================
    void saveStateToXml(const juce::AudioProcessor& processor, juce::XmlElement& xml);
    void loadStateFromXml(const juce::XmlElement& xml, juce::AudioProcessor& processor);

    //==============================================================================
    /** Handler for AB state toggling and copying in plugin.                        // improve descriptions
Create public instance in processor and call .toggleAB() and .copyAB()
methods from button callback in editor.
*/
    class StateAB
    {
    public:
        explicit StateAB(juce::AudioProcessor& p);

        void toggleAB();
        void copyAB(bool notifyHost = true);
        void reset();
        bool isCurrentA() const noexcept { return currentSideIsA.load(std::memory_order_acquire); }
        void writeToXml(juce::XmlElement& parent) const;
        void readFromXml(const juce::XmlElement* state);

    private:
        juce::AudioProcessor& pluginProcessor;
        juce::XmlElement ab { "AB" };
        mutable juce::CriticalSection stateLock;
        std::atomic<bool> currentSideIsA { true };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateAB)
    };

    //==============================================================================
    //int createFileIfNonExistant(const File &file);
    bool parseFileToXmlElement(const juce::File& file, juce::XmlElement& xml);
    bool writeXmlElementToFile(const juce::XmlElement& xml,
                               juce::File& file,
                               const juce::String& presetName,
                               bool confirmOverwrite);

    //==============================================================================
    /** Create StatePresets object with XML file saved relative to user
data directory.
e.g. StatePresets my_sps {"JohnFlynnPlugins/ThisPlugin/presets.xml"}
Full path Mac  = ~/Library/JohnFlynnPlugins/ThisPlugin/presets.xml
*/
    class StatePresets
    {
    public:
        StatePresets(juce::AudioProcessor& proc, const juce::String& presetFileLocation);
        ~StatePresets();

        juce::HashMap<int, juce::String> comboBoxIdToTagNameMap;

        juce::String savePreset(juce::File savePath, bool overwriteAlreadyConfirmed = false);
        bool loadPreset(const juce::String& selectedName);
        void deletePreset();
        juce::AudioProcessor& getProcessor() { return pluginProcessor; }

        void setPresetAndFolderNames(juce::ComboBox& menu);
        int getNumPresets() const;
        juce::String getNextAvailablePresetId();
        int getCurrentPresetId() const;
        void setCurrentPresetId(int currentPresetId);
        juce::String getCurrentPresetKey() const;
        void setCurrentPresetKey(juce::String key);
        void setPresetName(juce::String name);
        juce::String getPresetName() const;
        void scanAllPresets();
        juce::File getFile();
        void initPreset();
        void recursiveFileSearch(juce::XmlElement& parentXML,
                                 const juce::File& dir,
                                 int depth = 0);
        bool recursivePresetLoad(const juce::XmlElement& parentXml, const juce::String& presetId);
        void recursivePresetNameAdd(const juce::XmlElement& parentXml, juce::ComboBox& menu, int& index);
        const juce::XmlElement& getPresetXml() const;

    private:
        juce::AudioProcessor& pluginProcessor;
        juce::XmlElement mPresetXml { "WINGSFIRE" }; // in-plugin representation mutiple presets in one xml
        juce::XmlElement presetXmlSingle { "WINGSFIRE" }; // single preset for save file
        juce::File presetFile; // on-disk representation
        juce::String statePresetName { "" };
        juce::String currentPresetKey;
        juce::HashMap<int, juce::String> comboBoxIdToPresetKeyMap;
        mutable juce::CriticalSection identityLock;
        void recursiveSort(juce::XmlElement* parent);
        static juce::String normalisePresetKey(juce::String key);
        std::atomic<int> mCurrentPresetId { 0 };
        int numPresets = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatePresets)
    };

    //==============================================================================
    /** GUI-side component for the State objects. Handles GUI visual layout and
logic of the state handlers.
Make private member of the PluginEditor. Initialise with the StateAB
and StatePresets objects (these should be public members of the
PluginProcessor).
*/
    class StateComponent : public juce::Component,
                           public juce::Button::Listener,
                           public juce::ComboBox::Listener,
                           public juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater,
                           private juce::Timer
    {
    public:
        StateComponent(StateAB& sab, StatePresets& sp, juce::AudioProcessorValueTreeState& vts);
        ~StateComponent() override;

        void paint(juce::Graphics&) override;
        void resized() override;
        void markAsDirty();
        void parameterChanged(const juce::String& parameterID, float newValue) override;

        juce::String getPresetName();
        /*juce::TextButton& getNextButton();
    juce::TextButton& getPreviousButton();*/

        void setChangedState(bool state);
        bool getChangedState();
        juce::ComboBox* getPresetBox();
        juce::Button* getToggleABButton();
        void updatePresetBox(int selectedId);
        void synchronisePresetSelectionFromManager();
        void synchroniseABButtonFromManager();
        StatePresets* getProcStatePresets();
        StateAB* getProcStateAB();
        juce::TextButton* getCopyABButton();
        juce::TextButton* getPreviousButton();
        juce::TextButton* getNextButton();

    private:
        class ManualUpdateCheckThread final : public juce::Thread
        {
        public:
            explicit ManualUpdateCheckThread(StateComponent& ownerToUse);
            void run() override;
            void stop();
            void prepareForStart() { fetchOperation.reset(); }

        private:
            StateComponent& owner;
            VersionInfo::FetchOperation fetchOperation;
        };

        StateAB& procStateAB;
        StatePresets& procStatePresets;

        juce::AudioProcessorValueTreeState& valueTreeState;

        std::atomic<int> programmaticChangeDepth { 0 };
        //Multiband multiband{};
        //FireAudioProcessorEditor& editor;

        std::unique_ptr<juce::FileChooser> fileChooser;
        ManualUpdateCheckThread manualUpdateCheckThread;
        juce::CriticalSection updateResultLock;
        std::unique_ptr<VersionInfo> pendingVersionInfo;
        std::atomic<bool> dirtyUpdatePending { false };
        std::atomic<bool> versionCheckReady { false };
        juce::Component::SafePointer<juce::DialogWindow> settingsDialog;

        FireLookAndFeel fireLookAndFeel;

        juce::TextButton toggleABButton;
        juce::TextButton copyABButton;
        juce::ComboBox presetBox;
        juce::TextButton previousButton;
        juce::TextButton nextButton;
        juce::TextButton savePresetButton;
        //TextButton deletePresetButton;
        juce::TextButton menuButton;
        juce::PopupMenu presetMenu;

        bool isChanged = false;

        void buttonClicked(juce::Button* clickedButton) override;
        void comboBoxChanged(juce::ComboBox* changedComboBox) override;
        void handleAsyncUpdate() override;
        void timerCallback() override;

        void refreshPresetBox();
        void deletePresetAndRefresh();
        void savePresetAlertWindow();
        void openPresetFolder();
        void rescanPresetFolder();
        void creatFolderIfNotExist(juce::File userFile);
        void popPresetMenu();
        void setPreviousPreset();
        void setNextPreset();
        void beginProgrammaticChange();
        void endProgrammaticChange();

        void resetMultiband();
        void publishManualUpdateResult(std::unique_ptr<VersionInfo> result);
        void showManualUpdateResult();

        //juce::String presetName;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateComponent)
    };

    //==============================================================================
    // JF_DECLARE_UNIT_TEST_WITH_STATIC_INSTANCE (StateTests)

    //==============================================================================
} // namespace state

#endif // STATE_H_INCLUDED
