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

bool writeXmlElementToFile(const XmlElement &xml, File &file, String presetName, bool hasExtension)
{
    //createFileIfNonExistant(file);
    // 1 saved a new file
    // 2 replaced a existing file
    // 3 do nothing
    if (!file.exists())
    {
        file.create();
        xml.writeTo(file);
        return true;
    }
    else
    {
        if (!hasExtension) // pop up alert window
        {
            bool choice = NativeMessageBox::showOkCancelBox(AlertWindow::WarningIcon,
            "\""+presetName + ".fire\" already exists. Do you want to replace it?", "A file or folder with the same name already exists in the folder User. Replacing it will overwrite its current contents.", nullptr, nullptr);
            if (choice)
            {
                file.replaceFileIn(file.getFullPathName());
                xml.writeTo(file);
                return true;
            }
            else
            {
                return false;
            }
        }
        else // no alert window
        {
            // replace existing file and return 2
            file.replaceFileIn(file.getFullPathName());
            xml.writeTo(file);
            return true;
        }
        
    }
    
}

//==============================================================================
//sorter
class PresetNameSorter
{
public:
    PresetNameSorter (const juce::String& attributeToSortBy, bool forwards)
        : attributeToSort (attributeToSortBy),
          direction (forwards ? 1 : -1)
    {}

    int compareElements (juce::XmlElement* first, juce::XmlElement* second) const
    {
        auto result = first->getStringAttribute (attributeToSort)
                            .compareNatural (second->getStringAttribute (attributeToSort));
        
        // exchange preset tag
        if (result == -1)
        {
            String tempPresetTag = "";
            tempPresetTag = first->getTagName();
            first->setTagName(second->getTagName());
            second->setTagName(tempPresetTag);
        }
        return direction * result;
    }

private:
    juce::String attributeToSort;
    int direction;
};

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

String StatePresets::getNextAvailablePresetID(const XmlElement &presetXml)
{
    int newPresetIDNumber = getNumPresets();
    return "preset" + static_cast<String>(newPresetIDNumber);    // format: preset##
}

void StatePresets::recursiveFileSearch(XmlElement &parentXML, File dir)
{
    RangedDirectoryIterator iterator(dir, false, "*", 3); // findDirectories = 1, findFiles = 2, findFilesAndDirectories = 3, ignoreHiddenFiles = 4
    for(auto file : iterator)
    {
        // is folder
        if (file.isDirectory())
        {
            String folderName;
            folderName = file.getFile().getFileName();
            std::unique_ptr<XmlElement> currentState{new XmlElement{folderName}}; // must be pointer as parent takes ownership
            //DBG(dir.getChildFile(file.getFile().getFileName()).getFullPathName());
            recursiveFileSearch(*currentState, dir.getChildFile(file.getFile().getFileName()));
            parentXML.addChildElement(currentState.release()); // will be deleted by parent element
        }
        // is preset
        else if (file.getFile().hasFileExtension(".fire"))
        {
            numPresets++;
            String newPresetID = getNextAvailablePresetID(presetXml); // presetID format: "preset##"
            std::unique_ptr<XmlElement> currentState{new XmlElement{newPresetID}}; // must be pointer as parent takes ownership
            parseFileToXmlElement(file.getFile(), *currentState);
            currentState->setTagName(newPresetID);
            
            // if file name differs from preset name in .fire
            String newName = file.getFile().getFileNameWithoutExtension();
            if (newName != currentState->getStringAttribute("presetName")) {
                currentState->setAttribute("presetName", newName);
            }
            
            //presetXml.addChildElement(currentState.release()); // will be deleted by parent element
            parentXML.addChildElement(currentState.release()); // will be deleted by parent element
            
            // sort
            PresetNameSorter sorter ("presetName", true);
            parentXML.sortChildElements (sorter);
        }
        else
        {
            // not a fire preset, maybe popup alert window?
            //jassertfalse;
        }
        
    }
}

void StatePresets::scanAllPresets()
{
    numPresets = 0;
    presetXml.deleteAllChildElements();
    //RangedDirectoryIterator iterator(presetFile, true, "*.fire", 2);
    
    recursiveFileSearch(presetXml, presetFile);
    
    presetXml.writeTo(File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Audio/Presets/Wings/Fire/test.xml"));
}


bool StatePresets::savePreset(File savePath)
{
    File userPresetFile;
    bool hasExtension = false;
    String presetName = savePath.getFileNameWithoutExtension();

    if (!savePath.hasFileExtension(".fire"))
    {
        userPresetFile = savePath.getFullPathName() + ".fire";
        
    }
    else
    {
        userPresetFile = savePath.getFullPathName();
        hasExtension = true;
    }
    
    // save single to real file
    presetXmlSingle.removeAllAttributes(); // clear all first
    presetXmlSingle.setAttribute("presetName", presetName); // set preset name
    saveStateToXml(pluginProcessor, presetXmlSingle);
    //presetXmlSingle.writeTo(File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Audio/Presets/Wings/Fire/test2.xml"));
    //File userPresetFile = presetFile.getChildFile("User").getChildFile(getPresetName());
    
    statePresetName = presetName;
    bool isSaved = writeXmlElementToFile(presetXmlSingle, userPresetFile, presetName, hasExtension);
    
    if (isSaved)
    {
        scanAllPresets();
        return true;
    }
    else
    {
        return false;
    }
}

void StatePresets::recursivePresetLoad(XmlElement parentXml, String presetID)
{
    
    int index = 0;
    forEachXmlChildElement(parentXml, child)
    {
        //DBG(child->getTagName());
        //DBG(presetID);
        if (child->hasAttribute("presetName") && child->getTagName() == presetID)
        {
            XmlElement loadThisChild{*child}; // (0 indexed method)
            loadStateFromXml(loadThisChild, pluginProcessor);
        }
        else
        {
            recursivePresetLoad(*child, presetID);
        }
        index++;
    }
}

void StatePresets::loadPreset(String presetID)
{
    recursivePresetLoad(presetXml, presetID);
}


void StatePresets::deletePreset()
{
    XmlElement *childToDelete{presetXml.getChildElement(mCurrentPresetID - 1)};
    if (childToDelete)
        presetXml.removeChildElement(childToDelete, true);
}

void StatePresets::setPresetName(String name)
{
    statePresetName = name + ".fire";
}

StringRef StatePresets::getPresetName()
{
    return statePresetName;
}

void StatePresets::recursivePresetNameAdd(XmlElement parentXml ,ComboBox &menu, int &index)
{
    forEachXmlChildElement(parentXml, child) // should avoid macro?
    {
        if (child->hasAttribute("presetName"))
        {
            // is preset
            index++;
            String n = child->getStringAttribute("presetName");
            if (n == "")
                n = "(Unnamed preset)";
            menu.addItem(n, index);
            // save new preset and rescan, this will return new preset index
            if (statePresetName == n)
            {
                mCurrentPresetID = index;
            }
        }
        else
        {
            // is folder
            if (index != 0) {
                menu.addSeparator();
            }
            String n = child->getTagName();
            menu.addSectionHeading(n);

            recursivePresetNameAdd(*child, menu, index);
        }
    }
}

void StatePresets::setPresetAndFolderNames(ComboBox &menu)
{
    int index = 0;
    recursivePresetNameAdd(presetXml, menu, index);
}

int StatePresets::getNumPresets() const
{
    //return presetXml.getNumChildElements();
    return numPresets;
}

int StatePresets::getCurrentPresetId() const
{
    return mCurrentPresetID;
}

void StatePresets::setCurrentPresetId(int currentPresetID)
{
    mCurrentPresetID = currentPresetID;
}

File StatePresets::getFile()
{
    return presetFile;
}

void StatePresets::initPreset()
{
    for (const auto &param : pluginProcessor.getParameters())
        if (auto *p = dynamic_cast<AudioProcessorParameterWithID *>(param))
            // if not in xml set current
            p->setValueNotifyingHost(p->getDefaultValue());
    //set preset combobox to 0
}

//==============================================================================


//==============================================================================
StateComponent::StateComponent(StateAB &sab, StatePresets &sp)
: procStateAB{sab},
  procStatePresets{sp},
  toggleABButton{"A-B"},
  copyABButton{"Copy"},
  savePresetButton{"Save"},
  deletePresetButton{"Delete"},
  menuButton{"Menu"}
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
    addAndMakeVisible(menuButton);
    menuButton.addListener(this);
    
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
    menuButton.setColour(TextButton::textColourOffId, COLOUR1);
    menuButton.setColour(TextButton::buttonColourId, COLOUR5);
    menuButton.setColour(ComboBox::outlineColourId, COLOUR5);
    menuButton.getLookAndFeel().setColour(ComboBox::textColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(ComboBox::arrowColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(ComboBox::buttonColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(ComboBox::outlineColourId, COLOUR7);
    menuButton.getLookAndFeel().setColour(ComboBox::focusedOutlineColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(ComboBox::backgroundColourId, COLOUR7);
    menuButton.getLookAndFeel().setColour(PopupMenu::textColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, COLOUR5);
    menuButton.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(PopupMenu::headerTextColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(PopupMenu::backgroundColourId, COLOUR6);
    
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
    menuButton.setBounds(r.removeFromLeft(componentWidth));
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
    if (clickedButton == &menuButton)
        //openPresetFolder();
        popPresetMenu();
}

void StateComponent::comboBoxChanged(ComboBox *changedComboBox)
{
    
    const String presetID{"preset" + (String)changedComboBox->getSelectedId()};
    procStatePresets.setCurrentPresetId(changedComboBox->getSelectedId());
    //DBG(procStatePresets.getCurrentPresetId());
    
    procStatePresets.loadPreset(presetID);
}

void StateComponent::refreshPresetBox()
{
    presetBox.clear();
    procStatePresets.setPresetAndFolderNames(presetBox);
}

void StateComponent::ifPresetActiveShowInBox()
{
    const int currentPresetID{procStatePresets.getCurrentPresetId()};
    const int numPresets{procStatePresets.getNumPresets()};
    if (1 <= currentPresetID && currentPresetID <= numPresets)
        presetBox.setSelectedId(currentPresetID);
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

    File userFile = procStatePresets.getFile().getChildFile("User");
    creatFolderIfNotExist(userFile);
    FileChooser filechooser("save preset", userFile, "*", true, false,nullptr);
    if (filechooser.browseForFileToSave(true)) {
        
        // this is really shit code, but I don't have any other way to solve this shit problem!!!
        File inputName = filechooser.getResult();
        bool isSaved = procStatePresets.savePreset(inputName);
        if (isSaved)
        {
            refreshPresetBox();
            presetBox.setSelectedId(procStatePresets.getCurrentPresetId());
        }
    }
}
void StateComponent::openPresetFolder()
{
    // open preset folder
    File userFile = procStatePresets.getFile();
    creatFolderIfNotExist(userFile);
    if (!userFile.existsAsFile())
    {
        File(userFile).startAsProcess();
    }
}

void StateComponent::rescanPresetFolder()
{
    int presetID = procStatePresets.getCurrentPresetId();
    int currentPresetNum;
    int previousPresetNum = procStatePresets.getNumPresets();
    procStatePresets.scanAllPresets();
    refreshPresetBox();
    currentPresetNum = procStatePresets.getNumPresets();
    // if some presets are deleted, set presetID to 0
    if (currentPresetNum < previousPresetNum)
    {
        procStatePresets.setCurrentPresetId(presetID-1);
    }
    else
    {
        procStatePresets.setCurrentPresetId(presetID);
    }
    presetBox.setSelectedId(procStatePresets.getCurrentPresetId());
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

void StateComponent::popPresetMenu()
{
    
    presetMenu.clear();
    presetMenu.addItem(1, "Init", true);
    presetMenu.addItem(2, "Open Preset Folder", true);
    presetMenu.addItem(3, "Rescan Preset Folder", true);
    
    int result = presetMenu.show();
    if (result == 1)
    {
        procStatePresets.initPreset();
        presetBox.setSelectedId(0);
    }
    if (result == 2)
    {
        openPresetFolder();
    }
    if (result == 3)
    {
        rescanPresetFolder();
    }
}




} // namespace state


