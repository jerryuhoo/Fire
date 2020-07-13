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

#include "../JuceLibraryCode/JuceHeader.h"
#include "LookAndFeel.h"

namespace state
{

    //==============================================================================
    void saveStateToXml(const AudioProcessor &processor, XmlElement &xml);
    void loadStateFromXml(const XmlElement &xml, AudioProcessor &processor);

    //==============================================================================
    /** Handler for AB state toggling and copying in plugin.                        // improve descriptions
    Create public instance in processor and call .toggleAB() and .copyAB()
    methods from button callback in editor.
*/
    class StateAB
    {
    public:
        explicit StateAB(AudioProcessor &p);

        void toggleAB();
        void copyAB();

    private:
        AudioProcessor &pluginProcessor;
        XmlElement ab{"AB"};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateAB);
    };

    //==============================================================================
    void createFileIfNonExistant(const File &file);
    void parseFileToXmlElement(const File &file, XmlElement &xml);
    void writeXmlElementToFile(const XmlElement &xml, File &file);
    String getNextAvailablePresetID(const XmlElement &presetXml);

    //==============================================================================
    /** Create StatePresets object with XML file saved relative to user
    data directory.
    e.g. StatePresets my_sps {"JohnFlynnPlugins/ThisPlugin/presets.xml"}
    Full path Mac  = ~/Library/JohnFlynnPlugins/ThisPlugin/presets.xml
*/
    class StatePresets
    {
    public:
        StatePresets(AudioProcessor &proc, const String &presetFileLocation);
        ~StatePresets();

        void savePreset(const String &presetName); // preset already exists? confirm overwrite
        void loadPreset(int presetID);
        void deletePreset();

        StringArray getPresetNames() const;
        int getNumPresets() const;
        int getCurrentPresetId() const;

    private:
        AudioProcessor &pluginProcessor;
        XmlElement presetXml{"PRESETS"}; // local, in-plugin representation
        File presetFile;                 // on-disk representation
        int currentPresetID{0};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatePresets);
    };

    //==============================================================================
    /** GUI-side component for the State objects. Handles GUI visual layout and
    logic of the state handlers.
    Make private member of the PluginEditor. Initialise with the StateAB
    and StatePresets objects (these should be public members of the
    PluginProcessor).
*/
    class StateComponent : public Component,
                           public Button::Listener,
                           public ComboBox::Listener
    {
    public:
        StateComponent(StateAB &sab, StatePresets &sp);

        void paint(Graphics &) override;
        void resized() override;

    private:
        StateAB &procStateAB;
        StatePresets &procStatePresets;

        TextButton toggleABButton;
        TextButton copyABButton;
        ComboBox presetBox;
        TextButton savePresetButton;
        TextButton deletePresetButton;

        void buttonClicked(Button *clickedButton) override;
        void comboBoxChanged(ComboBox *changedComboBox) override;

        void refreshPresetBox();
        void ifPresetActiveShowInBox();
        void deletePresetAndRefresh();
        void savePresetAlertWindow();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateComponent);
    };

    //==============================================================================
    // JF_DECLARE_UNIT_TEST_WITH_STATIC_INSTANCE (StateTests)

    //==============================================================================
} // namespace state

#endif // STATE_H_INCLUDED
