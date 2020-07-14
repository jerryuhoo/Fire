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
    //xml.removeAllAttributes(); // clear first

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
//int createFileIfNonExistant(const File &file)
//{
//    // 1 saved a new file
//    // 2 replaced a existing file
//    // 3 do nothing
//    if (!file.exists())
//    {
//        file.create();
//        return 1;
//    }
//    else
//    {
//        bool choice = NativeMessageBox::showOkCancelBox(AlertWindow::NoIcon,
//        "Warning", "Preset already exists, replace it?", nullptr, nullptr);
//        if (choice)
//        {
//            file.replaceFileIn(file.getFullPathName());
//            return 2;
//        }
//        else
//        {
//            return 3;
//        }
//    }
//}

void parseFileToXmlElement(const File &file, XmlElement &xml)
{
    std::unique_ptr<XmlElement> parsed{XmlDocument::parse(file)};
    if (parsed)
        xml = *parsed;
}

int writeXmlElementToFile(const XmlElement &xml, File &file, String presetName)
{
    //createFileIfNonExistant(file);
    // 1 saved a new file
    // 2 replaced a existing file
    // 3 do nothing
    if (!file.exists())
    {
        file.create();
        xml.writeTo(file);
        return 1;
    }
    else
    {
        bool choice = NativeMessageBox::showOkCancelBox(AlertWindow::WarningIcon,
        "\""+presetName + ".fire\" already exists. Do you want to replace it?", "A file or folder with the same name already exists in the folder User. Replacing it will overwrite its current contents.", nullptr, nullptr);
        if (choice)
        {
            file.replaceFileIn(file.getFullPathName());
            xml.writeTo(file);
            return 2;
        }
        else
        {
            return 3;
        }
    }
    
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
    scanAllPresets();
    //parseFileToXmlElement(presetFile, presetXml);
}

StatePresets::~StatePresets()
{

}

void StatePresets::scanAllPresets()
{
    RangedDirectoryIterator iterator(presetFile, true, "*.fire", 2);
    for(auto file : iterator)
    {
        String newPresetID = getNextAvailablePresetID(presetXml); // presetID format: "preset##"
        std::unique_ptr<XmlElement> currentState{new XmlElement{newPresetID}}; // must be pointer as parent takes ownership
        parseFileToXmlElement(file.getFile(), *currentState);
        presetXml.addChildElement(currentState.release()); // will be deleted by parent element
    }

    //  presetFile
//    DirectoryIterator iter(presetFile, true, "*.fire", 2);
//    DBG(presetFile.getFileName());
//    while( iter.next() )
//    {
//        File f(iter.getFile() );
//        DBG(f.getFileName());
//        parseFileToXmlElement(f, presetXml);
//        if( f.hasExtension(".fire" ) { menu.addItem(f.getgetFileNameWithoutExtension(), ...); }
//        else if( f.isDirectory() )
//        {
//            PopupMenu subMenu;
//            func( subMenu, f );
//            menu.addSubMenu( subMenu, ... );
//        }
//    }
}


int StatePresets::savePreset(const String &presetName, bool hasExtension)
{
    int saveResult;
    
    // save single to real file
    presetXmlSingle.removeAllAttributes(); // clear all first
    presetXmlSingle.setAttribute("presetName", presetName); // set preset name
    saveStateToXml(pluginProcessor, presetXmlSingle);
    File userPresetFile = presetFile.getChildFile("User").getChildFile(getPresetName());
    //presetFile = "~/Library/Audio/Presets/Wings/Fire/User/"+getPresetName();
    //DBG(userPresetFile.getFullPathName());
    if (hasExtension)
    {
        userPresetFile.replaceFileIn(userPresetFile.getFullPathName());
        presetXmlSingle.writeTo(userPresetFile);
        saveResult = 2;
    }
    else
    {
        saveResult = writeXmlElementToFile(presetXmlSingle, userPresetFile, presetName);
    }
    
    // save temp xml which includes all presets
    if (saveResult == 1)
    {
        String newPresetID = getNextAvailablePresetID(presetXml); // presetID format: "preset##"
        std::unique_ptr<XmlElement> currentState{new XmlElement{newPresetID}}; // must be pointer as
        saveStateToXml(pluginProcessor, *currentState);                        // parent takes ownership
        currentState->setAttribute("presetName", presetName);
        presetXml.addChildElement(currentState.release()); // will be deleted by parent element
        return getNumPresets();
    }
    else if (saveResult == 2)
    {
        int currentPresetId;
        int index = 1;
        forEachXmlChildElement (presetXml, e)
        {
            if (e->getAttributeValue(0) == presetName)
            {
                currentPresetId = index;
                saveStateToXml(pluginProcessor, *e);
            }
            index++;
        }
        return currentPresetId;
    }
    else
    {
        return -1;
    }
    
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

void StatePresets::setPresetName(String name)
{
    presetName = name + ".fire";
}

StringRef StatePresets::getPresetName()
{
    return presetName;
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

File StatePresets::getFile()
{
    return presetFile;
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
  deletePresetButton{"Delete"},
  openFolderButton{"Folder"}
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
    //addAndMakeVisible(deletePresetButton);
    //deletePresetButton.addListener(this);
    addAndMakeVisible(openFolderButton);
    openFolderButton.addListener(this);
    
    toggleABButton.setColour(TextButton::textColourOffId, COLOUR1);
    toggleABButton.setColour(TextButton::buttonColourId, COLOUR5);
    toggleABButton.setColour(ComboBox::outlineColourId, COLOUR5);
    copyABButton.setColour(TextButton::textColourOffId, COLOUR1);
    copyABButton.setColour(TextButton::buttonColourId, COLOUR5);
    copyABButton.setColour(ComboBox::outlineColourId, COLOUR5);
    savePresetButton.setColour(TextButton::textColourOffId, COLOUR1);
    savePresetButton.setColour(TextButton::buttonColourId, COLOUR5);
    savePresetButton.setColour(ComboBox::outlineColourId, COLOUR5);
    //deletePresetButton.setColour(TextButton::textColourOffId, COLOUR1);
    //deletePresetButton.setColour(TextButton::buttonColourId, COLOUR5);
    //deletePresetButton.setColour(ComboBox::outlineColourId, COLOUR5);
    openFolderButton.setColour(TextButton::textColourOffId, COLOUR1);
    openFolderButton.setColour(TextButton::buttonColourId, COLOUR5);
    openFolderButton.setColour(ComboBox::outlineColourId, COLOUR5);
    
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
    //deletePresetButton.setBounds(r.removeFromLeft(componentWidth));
    openFolderButton.setBounds(r.removeFromLeft(componentWidth));
}

void StateComponent::buttonClicked(Button *clickedButton)
{
    if (clickedButton == &toggleABButton)
        procStateAB.toggleAB();
    if (clickedButton == &copyABButton)
        procStateAB.copyAB();
    if (clickedButton == &savePresetButton)
        savePresetAlertWindow();
    //if (clickedButton == &deletePresetButton)
    //    deletePresetAndRefresh();
    if (clickedButton == &openFolderButton)
        openPresetFolder();
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
    enum choice
    {
        ok,
        cancel
    };
    
    // if preset number > 0
    if (procStatePresets.getNumPresets() > 0)
    {
        bool choice = NativeMessageBox::showOkCancelBox(AlertWindow::NoIcon,
        "Warning", "Delete preset?", nullptr, nullptr);
        if (choice)
        {
            procStatePresets.deletePreset();
            refreshPresetBox();
        }
    }
    else
    {
//            AlertWindow alert2{"No preset!", "", AlertWindow::AlertIconType::NoIcon};
//            alert2.addButton("OK", choice::ok, KeyPress(KeyPress::returnKey, 0, 0));
//            alert2.setColour(AlertWindow::textColourId, COLOUR1);
//            alert2.setColour(AlertWindow::outlineColourId, COLOUR5);
//            alert2.setColour(AlertWindow::backgroundColourId, COLOUR7);
//            alert2.setLookAndFeel(&otherLookAndFeel);
//            alert2.getLookAndFeel().setColour(TextButton::textColourOffId, COLOUR1);
//            alert2.getLookAndFeel().setColour(TextButton::textColourOnId, COLOUR1);
//            alert2.getLookAndFeel().setColour(TextButton::buttonColourId, COLOUR6);
//            alert2.getLookAndFeel().setColour(ComboBox::outlineColourId, COLOUR6);
//            alert2.getLookAndFeel().setColour(TextEditor::outlineColourId, COLOUR6);
//            alert2.getLookAndFeel().setColour(TextEditor::backgroundColourId, COLOUR6);
//            alert2.getLookAndFeel().setColour(TextEditor::textColourId, COLOUR1);
//            alert2.getLookAndFeel().setColour(TextEditor::highlightColourId, COLOUR1);
//            alert2.getLookAndFeel().setColour(TextEditor::highlightedTextColourId, COLOUR6);
//            alert2.getLookAndFeel().setColour(TextEditor::shadowColourId, COLOUR7);
//            if (alert2.runModalLoop() == choice::ok) // LEAKS when quit while open !!!
//            {
//
//            }
        NativeMessageBox::showMessageBox (AlertWindow::NoIcon, "Warning", "No preset!");
        
    }
    
    
}

void StateComponent::savePresetAlertWindow()
{
//        enum choice
//        {
//            ok,
//            cancel
//        };
//
//        AlertWindow alert{"Save preset", "", AlertWindow::AlertIconType::NoIcon};
//        alert.addTextEditor("presetEditorID", "");
//        alert.addButton("Cancel", choice::cancel, KeyPress(KeyPress::escapeKey, 0, 0));
//        alert.addButton("OK", choice::ok, KeyPress(KeyPress::returnKey, 0, 0));
//        alert.setColour(AlertWindow::textColourId, COLOUR1);
//        alert.setColour(AlertWindow::outlineColourId, COLOUR5);
//        alert.setColour(AlertWindow::backgroundColourId, COLOUR7);
//        alert.setLookAndFeel(&otherLookAndFeel);
//        alert.getLookAndFeel().setColour(TextButton::textColourOffId, COLOUR1);
//        alert.getLookAndFeel().setColour(TextButton::textColourOnId, COLOUR1);
//        alert.getLookAndFeel().setColour(TextButton::buttonColourId, COLOUR6);
//        alert.getLookAndFeel().setColour(ComboBox::outlineColourId, COLOUR6);
//        alert.getLookAndFeel().setColour(TextEditor::outlineColourId, COLOUR6);
//        alert.getLookAndFeel().setColour(TextEditor::backgroundColourId, COLOUR6);
//        alert.getLookAndFeel().setColour(TextEditor::textColourId, COLOUR1);
//        alert.getLookAndFeel().setColour(TextEditor::highlightColourId, COLOUR1);
//        alert.getLookAndFeel().setColour(TextEditor::highlightedTextColourId, COLOUR6);
//        alert.getLookAndFeel().setColour(TextEditor::shadowColourId, COLOUR7);
//
//        if (alert.runModalLoop() == choice::ok) // LEAKS when quit while open !!!
//        {
//            String presetName{alert.getTextEditorContents("presetEditorID")};
//
//            procStatePresets.savePreset(presetName);
//            refreshPresetBox();
//            presetBox.setSelectedId(procStatePresets.getNumPresets());
//        }
    //File userFile ("~/Library/Audio/Presets/Wings/Fire/User"); // need to get userfile path from sp?
    File userFile = procStatePresets.getFile().getChildFile("User");
    creatFolderIfNotExist(userFile);
    FileChooser filechooser("save preset", userFile, "*", true, false,nullptr);
    if (filechooser.browseForFileToSave(true)) {
        int numPresets;
        // this is really shit code, but I don't have any other way to solve this shit problem!!!
        File inputName = filechooser.getResult();
        bool hasExtension = false;
        if (inputName.hasFileExtension(".fire")) // for example, this input is "a.fire"
        {
            procStatePresets.setPresetName(presetName);
            hasExtension = true;    // if hasExtension is true, it will not pop-up a self-defined warning window
        }
        presetName = inputName.getFileNameWithoutExtension();
        procStatePresets.setPresetName(presetName);
        numPresets = procStatePresets.savePreset(presetName, hasExtension);
        if (numPresets > 0)
        {
            refreshPresetBox();
            presetBox.setSelectedId(numPresets); // c
        }
    }
}
void StateComponent::openPresetFolder()
{
    // open preset folder
    
    //File userFile ("~/Library/Audio/Presets/Wings/Fire");
    File userFile = procStatePresets.getFile();
    creatFolderIfNotExist(userFile);
    if (!userFile.existsAsFile())
    {
        File(userFile).startAsProcess();
    }
}

void StateComponent::creatFolderIfNotExist(File userFile)
{
    if (!userFile.exists())
    {
        userFile.createDirectory();
    }
}

String StateComponent::getPresetName()
{
    return presetName;
}

} // namespace state
