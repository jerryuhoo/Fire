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

#include <JuceHeader.h>
#include "GUI/LookAndFeel.h"
#include "Multiband/Multiband.h"
#include "Utility/VersionInfo.h"

namespace state
{

//==============================================================================
void saveStateToXml(const juce::AudioProcessor &processor, juce::XmlElement &xml);
void loadStateFromXml(const juce::XmlElement &xml, juce::AudioProcessor &processor);

//==============================================================================
/** Handler for AB state toggling and copying in plugin.                        // improve descriptions
Create public instance in processor and call .toggleAB() and .copyAB()
methods from button callback in editor.
*/
class StateAB
{
public:
    explicit StateAB(juce::AudioProcessor &p);

    void toggleAB();
    void copyAB();

private:
    juce::AudioProcessor &pluginProcessor;
    juce::XmlElement ab{"AB"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateAB);
};

//==============================================================================
//int createFileIfNonExistant(const File &file);
void parseFileToXmlElement(const juce::File &file, juce::XmlElement &xml);
bool writeXmlElementToFile(const juce::XmlElement &xml, juce::File &file, juce::String presetName, bool hasExtension);

//==============================================================================
/** Create StatePresets object with XML file saved relative to user
data directory.
e.g. StatePresets my_sps {"JohnFlynnPlugins/ThisPlugin/presets.xml"}
Full path Mac  = ~/Library/JohnFlynnPlugins/ThisPlugin/presets.xml
*/
class StatePresets
{
public:
    StatePresets(juce::AudioProcessor &proc, const juce::String &presetFileLocation);
    ~StatePresets();
    
    bool savePreset(juce::File savePath);
    void loadPreset(juce::String selectedName);
    void deletePreset();

    void setPresetAndFolderNames(juce::ComboBox &menu);
    int getNumPresets() const;
    juce::String getNextAvailablePresetID();
    int getCurrentPresetId() const;
    void setCurrentPresetId(int currentPresetID);
    void setPresetName(juce::String name);
    juce::StringRef getPresetName();
    void scanAllPresets();
    juce::File getFile();
    void initPreset();
    void recursiveFileSearch(juce::XmlElement &parentXML, juce::File dir);
    void recursivePresetLoad(juce::XmlElement parentXml, juce::String presetID);
    void recursivePresetNameAdd(juce::XmlElement parentXml ,juce::ComboBox &menu, int &index);
    
private:
    juce::AudioProcessor &pluginProcessor;
    juce::XmlElement mPresetXml{"WINGSFIRE"}; // in-plugin representation mutiple presets in one xml
    juce::XmlElement presetXmlSingle{"WINGSFIRE"}; // single preset for save file
    juce::File presetFile; // on-disk representation
    juce::String statePresetName{""};
    int mCurrentPresetID{0};
    int numPresets = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatePresets);
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
public juce::ComboBox::Listener
{
public:
    StateComponent(StateAB &sab, StatePresets &sp, Multiband &m);

    void paint(juce::Graphics &) override;
    void resized() override;
    
    juce::String getPresetName();
    /*juce::TextButton& getNextButton();
    juce::TextButton& getPreviousButton();*/
    
    void setInitState(bool state);
    bool getInitState();
    
private:
    StateAB &procStateAB;
    StatePresets &procStatePresets;
    Multiband &multiband;
    //FireAudioProcessorEditor& editor;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<VersionInfo> versionInfo;
    juce::String version;
    
    OtherLookAndFeel otherLookAndFeel;
    
    juce::TextButton toggleABButton;
    juce::TextButton copyABButton;
    juce::ComboBox presetBox;
    juce::TextButton previousButton;
    juce::TextButton nextButton;
    juce::TextButton savePresetButton;
    //TextButton deletePresetButton;
    juce::TextButton menuButton;
    juce::PopupMenu presetMenu;
    
    bool isInit = false;

    void buttonClicked(juce::Button *clickedButton) override;
    void comboBoxChanged(juce::ComboBox *changedComboBox) override;

    void refreshPresetBox();
    void ifPresetActiveShowInBox();
    void deletePresetAndRefresh();
    void savePresetAlertWindow();
    void openPresetFolder();
    void rescanPresetFolder();
    void creatFolderIfNotExist(juce::File userFile);
    void popPresetMenu();
    void setPreviousPreset();
    void setNextPreset();
    
    void resetMultiband();

    //juce::String presetName;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateComponent);
};

//==============================================================================
// JF_DECLARE_UNIT_TEST_WITH_STATIC_INSTANCE (StateTests)

//==============================================================================
} // namespace state

#endif // STATE_H_INCLUDED
