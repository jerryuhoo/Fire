/*
  ==============================================================================

    Preset.cpp
    Created: 12 Jul 2020 9:06:49pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "Preset.h"

namespace state
{

    //==============================================================================
    void saveStateToXml(const AudioProcessor &proc, XmlElement &xml)
    {
        xml.removeAllAttributes(); // clear first

        for (const auto &param : proc.getParameters())
            if (auto *p = dynamic_cast<AudioProcessorParameterWithID *>(param))
                xml.setAttribute(p->paramID, p->getValue()); // 0to1
    }

    void loadStateFromXml(const XmlElement &xml, AudioProcessor &proc)
    {
        for (const auto &param : proc.getParameters())
            if (auto *p = dynamic_cast<AudioProcessorParameterWithID *>(param))
                // if not in xml set current
                p->setValueNotifyingHost((float)xml.getDoubleAttribute(p->paramID, p->getValue()));
    }

    //==============================================================================
    StateAB::StateAB(AudioProcessor &p)
        : pluginProcessor{p}
    {
        copyAB();
    }

    void StateAB::toggleAB()
    {
        XmlElement temp{"Temp"};
        saveStateToXml(pluginProcessor, temp); // current to temp
        loadStateFromXml(ab, pluginProcessor); // ab to current
        ab = temp;                             // temp to ab
    }

    void StateAB::copyAB()
    {
        saveStateToXml(pluginProcessor, ab);
    }

    //==============================================================================
    void createFileIfNonExistant(const File &file)
    {
        if (!file.exists())
            file.create();
        jassert(file.exists());
    }

    void parseFileToXmlElement(const File &file, XmlElement &xml) // what could go wrong here?
    {
        std::unique_ptr<XmlElement> parsed{XmlDocument::parse(file)};
        if (parsed)
            xml = *parsed;
    }

    void writeXmlElementToFile(const XmlElement &xml, File &file)
    {
        createFileIfNonExistant(file);
        xml.writeTo(file); // "" is DTD (unused)
    }

    String getNextAvailablePresetID(const XmlElement &presetXml)
    {
        int newPresetIDNumber = presetXml.getNumChildElements() + 1; // 1 indexed to match ComboBox
        return "preset" + static_cast<String>(newPresetIDNumber);    // format: preset##
    }

    //==============================================================================
    StatePresets::StatePresets(AudioProcessor &proc, const String &presetFileLocation)
        : pluginProcessor{proc},
          presetFile{File::getSpecialLocation(File::userApplicationDataDirectory)
                         .getChildFile(presetFileLocation)}
    {
        parseFileToXmlElement(presetFile, presetXml);
    }

    StatePresets::~StatePresets()
    {
        writeXmlElementToFile(presetXml, presetFile);
    }

    void StatePresets::savePreset(const String &presetName)
    {
        String newPresetID = getNextAvailablePresetID(presetXml); // presetID format: "preset##"

        std::unique_ptr<XmlElement> currentState{new XmlElement{newPresetID}}; // must be pointer as
        saveStateToXml(pluginProcessor, *currentState);                        // parent takes ownership
        currentState->setAttribute("presetName", presetName);

        presetXml.addChildElement(currentState.release()); // will be deleted by parent element
    }

    void StatePresets::loadPreset(int presetID)
    {
        if (1 <= presetID && presetID <= presetXml.getNumChildElements()) // 1 indexed to match ComboBox
        {
            XmlElement loadThisChild{*presetXml.getChildElement(presetID - 1)}; // (0 indexed method)
            loadStateFromXml(loadThisChild, pluginProcessor);
        }
        currentPresetID = presetID; // allow 0 for 'no preset selected' (?)
    }

    void StatePresets::deletePreset()
    {
        XmlElement *childToDelete{presetXml.getChildElement(currentPresetID - 1)};
        if (childToDelete)
            presetXml.removeChildElement(childToDelete, true);
    }

    StringArray StatePresets::getPresetNames() const
    {
        StringArray names;

        forEachXmlChildElement(presetXml, child) // should avoid macro?
        {
            String n = child->getStringAttribute("presetName");
            if (n == "")
                n = "(Unnamed preset)";
            names.add(n);
        }
        return names; // hopefully moves
    }

    int StatePresets::getNumPresets() const
    {
        return presetXml.getNumChildElements();
    }

    int StatePresets::getCurrentPresetId() const
    {
        return currentPresetID;
    }

    //==============================================================================
    void populateComboBox(ComboBox &comboBox, const StringArray &listItems)
    {
        for (int i = 0; i < listItems.size(); ++i)
            comboBox.addItem(listItems[i], i + 1); // 1-indexed ID for ComboBox
    }

    //==============================================================================
    StateComponent::StateComponent(StateAB &sab, StatePresets &sp)
        : procStateAB{sab},
          procStatePresets{sp},
          toggleABButton{"A-B"},
          copyABButton{"Copy"},
          savePresetButton{"Save"},
          deletePresetButton{"Delete"}
    {
        addAndMakeVisible(toggleABButton);
        addAndMakeVisible(copyABButton);
        toggleABButton.addListener(this);
        copyABButton.addListener(this);

        addAndMakeVisible(presetBox);
        presetBox.setJustificationType(Justification::centred);
        presetBox.setColour(ComboBox::textColourId, COLOUR1);
        presetBox.setColour(ComboBox::arrowColourId, COLOUR1);
        presetBox.setColour(ComboBox::buttonColourId, COLOUR1);
        presetBox.setColour(ComboBox::outlineColourId, COLOUR5);
        presetBox.setColour(ComboBox::focusedOutlineColourId, COLOUR1);
        presetBox.setColour(ComboBox::backgroundColourId, COLOUR5);
        presetBox.setTextWhenNothingSelected("- Init -");
        refreshPresetBox();
        ifPresetActiveShowInBox();
        presetBox.addListener(this);

        addAndMakeVisible(savePresetButton);
        savePresetButton.addListener(this);
        addAndMakeVisible(deletePresetButton);
        deletePresetButton.addListener(this);

        toggleABButton.setColour(TextButton::textColourOffId, COLOUR1);
        toggleABButton.setColour(TextButton::buttonColourId, COLOUR5);
        toggleABButton.setColour(ComboBox::outlineColourId, COLOUR5);
        copyABButton.setColour(TextButton::textColourOffId, COLOUR1);
        copyABButton.setColour(TextButton::buttonColourId, COLOUR5);
        copyABButton.setColour(ComboBox::outlineColourId, COLOUR5);
        savePresetButton.setColour(TextButton::textColourOffId, COLOUR1);
        savePresetButton.setColour(TextButton::buttonColourId, COLOUR5);
        savePresetButton.setColour(ComboBox::outlineColourId, COLOUR5);
        deletePresetButton.setColour(TextButton::textColourOffId, COLOUR1);
        deletePresetButton.setColour(TextButton::buttonColourId, COLOUR5);
        deletePresetButton.setColour(ComboBox::outlineColourId, COLOUR5);
    }

    void StateComponent::paint(Graphics & /*g*/)
    {
        //g.fillAll (Colours::lightgrey);
    }

    void StateComponent::resized()
    {
        Rectangle<int> r(getLocalBounds());

        const int numComponents{6};
        const int componentWidth{getWidth() / numComponents};

        toggleABButton.setBounds(r.removeFromLeft(componentWidth));
        copyABButton.setBounds(r.removeFromLeft(componentWidth));
        presetBox.setBounds(r.removeFromLeft(componentWidth * 2));
        savePresetButton.setBounds(r.removeFromLeft(componentWidth));
        deletePresetButton.setBounds(r.removeFromLeft(componentWidth));
    }

    void StateComponent::buttonClicked(Button *clickedButton)
    {
        if (clickedButton == &toggleABButton)
            procStateAB.toggleAB();
        if (clickedButton == &copyABButton)
            procStateAB.copyAB();
        if (clickedButton == &savePresetButton)
            savePresetAlertWindow();
        if (clickedButton == &deletePresetButton)
            deletePresetAndRefresh();
    }

    void StateComponent::comboBoxChanged(ComboBox *changedComboBox)
    {
        const int selectedId{changedComboBox->getSelectedId()};
        procStatePresets.loadPreset(selectedId);
    }

    void StateComponent::refreshPresetBox()
    {
        presetBox.clear();
        StringArray presetNames{procStatePresets.getPresetNames()};

        populateComboBox(presetBox, presetNames);
    }

    void StateComponent::ifPresetActiveShowInBox()
    {
        const int currentPreset{procStatePresets.getCurrentPresetId()};
        const int numPresets{procStatePresets.getNumPresets()};
        if (1 <= currentPreset && currentPreset <= numPresets)
            presetBox.setSelectedId(currentPreset);
    }

    void StateComponent::deletePresetAndRefresh()
    {
        procStatePresets.deletePreset();
        refreshPresetBox();
    }

    void StateComponent::savePresetAlertWindow()
    {
        enum choice
        {
            ok,
            cancel
        };

        AlertWindow alert{"Save preset...", "", AlertWindow::AlertIconType::NoIcon};
        alert.addTextEditor("presetEditorID", "Enter preset name");
        alert.addButton("OK", choice::ok, KeyPress(KeyPress::returnKey, 0, 0));
        alert.addButton("Cancel", choice::cancel, KeyPress(KeyPress::escapeKey, 0, 0));

        if (alert.runModalLoop() == choice::ok) // LEAKS when quit while open !!!
        {
            String presetName{alert.getTextEditorContents("presetEditorID")};

            procStatePresets.savePreset(presetName);
            refreshPresetBox();
            presetBox.setSelectedId(procStatePresets.getNumPresets());
        }
    }

} // namespace state
