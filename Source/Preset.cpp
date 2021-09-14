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
void saveStateToXml(const juce::AudioProcessor &proc, juce::XmlElement &xml)
{
    //xml.removeAllAttributes(); // clear first
    
    for (const auto &param : proc.getParameters())
        if (auto *p = dynamic_cast<juce::AudioProcessorParameterWithID *>(param))
        {
                xml.setAttribute(p->paramID, p->getValue());
        }
}

void loadStateFromXml(const juce::XmlElement &xml, juce::AudioProcessor &proc)
{
    for (const auto &param : proc.getParameters())
        if (auto *p = dynamic_cast<juce::AudioProcessorParameterWithID *>(param))
            // if not in xml set current
            p->setValueNotifyingHost((float)xml.getDoubleAttribute(p->paramID, p->getValue()));
}

//==============================================================================
StateAB::StateAB(juce::AudioProcessor &p)
: pluginProcessor{p}
{
    copyAB();
}

void StateAB::toggleAB()
{
    juce::XmlElement temp{"Temp"};
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

void parseFileToXmlElement(const juce::File &file, juce::XmlElement &xml)
{
    std::unique_ptr<juce::XmlElement> parsed{juce::XmlDocument::parse(file)};
    if (parsed)
        xml = *parsed;
}

bool writeXmlElementToFile(const juce::XmlElement &xml, juce::File &file, juce::String presetName, bool hasExtension)
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
            bool choice = juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                                  "\"" + presetName + PRESET_EXETENSION + "\" already exists. Do you want to replace it?", "A file or folder with the same name already exists in the folder User. Replacing it will overwrite its current contents.", nullptr, nullptr);
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
    PresetNameSorter(const juce::String &attributeToSortBy, bool forwards)
    : attributeToSort(attributeToSortBy),
    direction(forwards ? 1 : -1)
    {
    }
    
    int compareElements(juce::XmlElement *first, juce::XmlElement *second) const
    {
        auto result = first->getStringAttribute(attributeToSort)
        .compareNatural(second->getStringAttribute(attributeToSort));
        
        // exchange preset tag
        if (result == -1)
        {
            juce::String tempPresetTag = "";
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
StatePresets::StatePresets(juce::AudioProcessor &proc, const juce::String &presetFileLocation)
: pluginProcessor{proc},
presetFile{juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
    .getChildFile(presetFileLocation)}
{
    scanAllPresets();
    //parseFileToXmlElement(presetFile, mPresetXml);
}

StatePresets::~StatePresets()
{
}

juce::String StatePresets::getNextAvailablePresetId()
{
    int newPresetIdNumber = getNumPresets();
    return "preset" + static_cast<juce::String>(newPresetIdNumber); // format: preset##
}

void StatePresets::recursiveFileSearch(juce::XmlElement &parentXML, juce::File dir)
{
    juce::RangedDirectoryIterator iterator(dir, false, "*", 3); // findDirectories = 1, findFiles = 2, findFilesAndDirectories = 3, ignoreHiddenFiles = 4
    for (auto file : iterator)
    {
        // is folder
        if (file.isDirectory())
        {
            juce::String folderName;
            folderName = file.getFile().getFileName();
            std::unique_ptr<juce::XmlElement> currentState{new juce::XmlElement{folderName}}; // must be pointer as parent takes ownership
            //DBG(dir.getChildFile(file.getFile().getFileName()).getFullPathName());
            recursiveFileSearch(*currentState, dir.getChildFile(file.getFile().getFileName()));
            parentXML.addChildElement(currentState.release()); // will be deleted by parent element
        }
        // is preset
        else if (file.getFile().hasFileExtension(PRESET_EXETENSION))
        {
            numPresets++;
            juce::String newPresetId = getNextAvailablePresetId();                    // presetId format: "preset##"
            std::unique_ptr<juce::XmlElement> currentState{new juce::XmlElement{newPresetId}}; // must be pointer as parent takes ownership
            parseFileToXmlElement(file.getFile(), *currentState);
            currentState->setTagName(newPresetId);
            
            // if file name differs from preset name in .fire
            juce::String newName = file.getFile().getFileNameWithoutExtension();
            if (newName != currentState->getStringAttribute("presetName"))
            {
                currentState->setAttribute("presetName", newName);
            }
            
            //mPresetXml.addChildElement(currentState.release()); // will be deleted by parent element
            parentXML.addChildElement(currentState.release()); // will be deleted by parent element
            
            // sort
            PresetNameSorter sorter("presetName", true);
            parentXML.sortChildElements(sorter);
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
    mPresetXml.deleteAllChildElements();
    //RangedDirectoryIterator iterator(presetFile, true, "*.fire", 2);
    
    recursiveFileSearch(mPresetXml, presetFile);
    
    //mPresetXml.writeTo(File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Audio/Presets/Wings/Fire/test.xml"));
}

bool StatePresets::savePreset(juce::File savePath)
{
    juce::File userPresetFile;
    bool hasExtension = false;
    juce::String presetName = savePath.getFileNameWithoutExtension();
    
    if (!savePath.hasFileExtension(PRESET_EXETENSION))
    {
        userPresetFile = savePath.getFullPathName() + PRESET_EXETENSION;
    }
    else
    {
        userPresetFile = savePath.getFullPathName();
        hasExtension = true;
    }
    
    // save single to real file
    presetXmlSingle.removeAllAttributes();                  // clear all first
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

void StatePresets::recursivePresetLoad(juce::XmlElement parentXml, juce::String presetId)
{
    int index = 0;
//    forEachXmlChildElement(parentXml, child)
//    {
//        //DBG(child->getTagName());
//        //DBG(presetId);
//        if (child->hasAttribute("presetName") && child->getTagName() == presetId)
//        {
//            juce::XmlElement loadThisChild{*child}; // (0 indexed method)
//            loadStateFromXml(loadThisChild, pluginProcessor);
//            statePresetName = child->getAttributeValue(0); //presetName index is 0
//        }
//        else
//        {
//            recursivePresetLoad(*child, presetId);
//        }
//        index++;
//    }
    
    for (auto* child : parentXml.getChildIterator())
    {
        //DBG(child->getTagName());
        //DBG(presetId);
        if (child->hasAttribute("presetName") && child->getTagName() == presetId)
        {
            juce::XmlElement loadThisChild{*child}; // (0 indexed method)
            loadStateFromXml(loadThisChild, pluginProcessor);
            statePresetName = child->getAttributeValue(0); //presetName index is 0
        }
        else
        {
            recursivePresetLoad(*child, presetId);
        }
        index++;
    }
}

void StatePresets::loadPreset(juce::String presetId)
{
    recursivePresetLoad(mPresetXml, presetId);
}

void StatePresets::deletePreset()
{
    juce::XmlElement *childToDelete{mPresetXml.getChildElement(mCurrentPresetId - 1)};
    if (childToDelete)
        mPresetXml.removeChildElement(childToDelete, true);
}

void StatePresets::setPresetName(juce::String name)
{
    statePresetName = name + PRESET_EXETENSION;
}

juce::StringRef StatePresets::getPresetName()
{
    return statePresetName;
}

void StatePresets::recursivePresetNameAdd(juce::XmlElement parentXml, juce::ComboBox &menu, int &index)
{
    
//    forEachXmlChildElement(parentXml, child) // should avoid macro?
//    {
//        if (child->hasAttribute("presetName"))
//        {
//            // is preset
//            index++;
//            juce::String n = child->getStringAttribute("presetName");
//            if (n == "")
//                n = "(Unnamed preset)";
//            menu.addItem(n, index);
//            // save new preset and rescan, this will return new preset index
//            if (statePresetName == n)
//            {
//                mCurrentPresetId = index;
//            }
//        }
//        else
//        {
//            // is folder
//            if (index != 0)
//            {
//                menu.addSeparator();
//            }
//            juce::String n = child->getTagName();
//            menu.addSectionHeading(n);
//
//            recursivePresetNameAdd(*child, menu, index);
//        }
//    }
    
    for (auto* child : parentXml.getChildIterator())
    {
        if (child->hasAttribute("presetName"))
        {
            // is preset
            index++;
            juce::String n = child->getStringAttribute("presetName");
            if (n == "")
                n = "(Unnamed preset)";
            menu.addItem(n, index);
            // save new preset and rescan, this will return new preset index
            if (statePresetName == n)
            {
                mCurrentPresetId = index;
            }
        }
        else
        {
            // is folder
            if (index != 0)
            {
                menu.addSeparator();
            }
            juce::String n = child->getTagName();
            menu.addSectionHeading(n);

            recursivePresetNameAdd(*child, menu, index);
        }
    }
}

void StatePresets::setPresetAndFolderNames(juce::ComboBox &menu)
{
    int index = 0;
    recursivePresetNameAdd(mPresetXml, menu, index);
}

int StatePresets::getNumPresets() const
{
    //return mPresetXml.getNumChildElements();
    return numPresets;
}

int StatePresets::getCurrentPresetId() const
{
    return mCurrentPresetId;
}

void StatePresets::setCurrentPresetId(int currentPresetId)
{
    mCurrentPresetId = currentPresetId;
}

juce::File StatePresets::getFile()
{
    return presetFile;
}

void StatePresets::initPreset()
{
    for (const auto &param : pluginProcessor.getParameters())
        if (auto *p = dynamic_cast<juce::AudioProcessorParameterWithID *>(param))
            // if not in xml set current
            p->setValueNotifyingHost(p->getDefaultValue());
    // set preset combobox to 0
    statePresetName = "";
}

//==============================================================================

//==============================================================================
StateComponent::StateComponent(StateAB &sab, StatePresets &sp, Multiband &m)
: procStateAB{sab},
procStatePresets{sp},
multiband(m),
toggleABButton{"A"},
copyABButton{"Copy"},
previousButton{"<"},
nextButton{">"},
savePresetButton{"Save"},
//deletePresetButton{"Delete"},
menuButton{"Menu"}
{
    addAndMakeVisible(toggleABButton);
    addAndMakeVisible(copyABButton);
    toggleABButton.addListener(this);
    copyABButton.addListener(this);
    
    addAndMakeVisible(previousButton);
    addAndMakeVisible(nextButton);
    previousButton.addListener(this);
    nextButton.addListener(this);
    
    addAndMakeVisible(presetBox);

    presetBox.setJustificationType(juce::Justification::centred);
    presetBox.setColour(juce::ComboBox::textColourId, COLOUR1);
    presetBox.setColour(juce::ComboBox::arrowColourId, COLOUR1);
    presetBox.setColour(juce::ComboBox::buttonColourId, COLOUR1);
    presetBox.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    presetBox.setColour(juce::ComboBox::focusedOutlineColourId, COLOUR1);
    presetBox.setColour(juce::ComboBox::backgroundColourId, COLOUR4);
    presetBox.setTextWhenNothingSelected("- Init -");
    
    // when selecting the same preset, this will revert back to the original preset (in case you changed something)
    presetBox.onChange = [this]
    {
        auto menu = presetBox.getRootMenu();
        auto id = presetBox.getSelectedId();
                
        juce::PopupMenu::MenuItemIterator iterator (*menu);

        while (iterator.next())
        {
            auto item = &iterator.getItem();

            if (item->itemID == id)
            {
                item->setAction ([this, id] { updatePresetBox(id); isChanged = true; });
            }
            else
            {
                item->setAction (nullptr);
            }
        }
    };
    
    refreshPresetBox();
    ifPresetActiveShowInBox();
    presetBox.addListener(this);
    
    addAndMakeVisible(savePresetButton);
    savePresetButton.addListener(this);
    //addAndMakeVisible(deletePresetButton);
    //deletePresetButton.addListener(this);
    addAndMakeVisible(menuButton);
    menuButton.addListener(this);
    
    toggleABButton.setColour(juce::TextButton::textColourOffId, COLOUR1);
    toggleABButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    toggleABButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    copyABButton.setColour(juce::TextButton::textColourOffId, COLOUR1);
    copyABButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    copyABButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    previousButton.setColour(juce::TextButton::textColourOffId, COLOUR1);
    previousButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    previousButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    nextButton.setColour(juce::TextButton::textColourOffId, COLOUR1);
    nextButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    nextButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    savePresetButton.setColour(juce::TextButton::textColourOffId, COLOUR1);
    savePresetButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    savePresetButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    //deletePresetButton.setColour(TextButton::textColourOffId, COLOUR1);
    //deletePresetButton.setColour(TextButton::buttonColourId, COLOUR5);
    //deletePresetButton.setColour(ComboBox::outlineColourId, COLOUR5);
    menuButton.setColour(juce::TextButton::textColourOffId, COLOUR1);
    menuButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    menuButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    menuButton.getLookAndFeel().setColour(juce::ComboBox::textColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(juce::ComboBox::arrowColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(juce::ComboBox::buttonColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(juce::ComboBox::outlineColourId, COLOUR7);
    menuButton.getLookAndFeel().setColour(juce::ComboBox::focusedOutlineColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(juce::ComboBox::backgroundColourId, COLOUR7);
    presetMenu.setLookAndFeel(&otherLookAndFeel);
    
    menuButton.getLookAndFeel().setColour(juce::PopupMenu::textColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(juce::PopupMenu::highlightedBackgroundColourId, COLOUR5);
    menuButton.getLookAndFeel().setColour(juce::PopupMenu::highlightedTextColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(juce::PopupMenu::headerTextColourId, COLOUR1);
    menuButton.getLookAndFeel().setColour(juce::PopupMenu::backgroundColourId, COLOUR6);
}

void StateComponent::paint(juce::Graphics & /*g*/)
{
    //g.fillAll (Colours::lightgrey);
}

void StateComponent::resized()
{
    juce::Rectangle<int> r(getLocalBounds());
    const int numComponents{6};
    const int componentWidth{getWidth() / numComponents};
    
    toggleABButton.setBounds(r.removeFromLeft(componentWidth));
    copyABButton.setBounds(r.removeFromLeft(componentWidth));
    presetBox.setBounds(r.removeFromLeft(componentWidth * 2).reduced(getHeight() / 15)); // if reduced value is getHeight() / 10, juce has a bug?
    previousButton.setBounds(r.removeFromLeft(componentWidth / 4));
    nextButton.setBounds(r.removeFromLeft(componentWidth / 4));
    savePresetButton.setBounds(r.removeFromLeft(componentWidth));
    //deletePresetButton.setBounds(r.removeFromLeft(componentWidth));
    menuButton.setBounds(r.removeFromLeft(componentWidth));
}

void StateComponent::buttonClicked(juce::Button *clickedButton)
{
//    if (clickedButton == &toggleABButton)
//    {
//        procStateAB.toggleAB();
//        isChanged = true;
//
//        if (toggleABButton.getButtonText() == "A")
//            toggleABButton.setButtonText("B");
//        else
//            toggleABButton.setButtonText("A");
//    }
    if (clickedButton == &copyABButton)
        procStateAB.copyAB();
    if (clickedButton == &previousButton)
        setPreviousPreset();
    if (clickedButton == &nextButton)
        setNextPreset();
    if (clickedButton == &savePresetButton)
        savePresetAlertWindow();
    //if (clickedButton == &deletePresetButton)
    //    deletePresetAndRefresh();
    if (clickedButton == &menuButton)
        //openPresetFolder();
        popPresetMenu();
}

void StateComponent::setPreviousPreset()
{
    int presetIndex = procStatePresets.getCurrentPresetId() - 1;
    if (presetIndex > 0)
    {
        //juce::String presetId = (juce::String)presetIndex;
        //procStatePresets.setCurrentPresetId(presetIndex);
        //procStatePresets.loadPreset(presetId);
        presetBox.setSelectedId(presetIndex);
    }
    //isChanged = true;
}

void StateComponent::setNextPreset()
{
    int presetIndex = procStatePresets.getCurrentPresetId() + 1;
    if (presetIndex <= procStatePresets.getNumPresets())
    {
        //juce::String presetId = (juce::String)presetIndex;
        //procStatePresets.setCurrentPresetId(presetIndex);
        //procStatePresets.loadPreset(presetId);
        presetBox.setSelectedId(presetIndex);
    }
    //isChanged = true;
}

void StateComponent::comboBoxChanged(juce::ComboBox *changedComboBox)
{
//    int selectedId = changedComboBox->getSelectedId();
//
//    // do this because open and close GUI will use this function, but will reset the value if the presetbox is not "init"
//    // next, previous, change combobox will change the selectedId, but currentId will change only after this.
//    // and then, it will load the preset.
//    
//    if (procStatePresets.getCurrentPresetId() != selectedId)
//    {
//        updatePresetBox(selectedId);
//    }
}

void StateComponent::updatePresetBox(int selectedId) // when preset is changed
{
    const juce::String presetId{ "preset" + (juce::String)selectedId };
    procStatePresets.setCurrentPresetId(selectedId);
    procStatePresets.loadPreset(presetId);
}

void StateComponent::refreshPresetBox() // rescan, init, save, or delete
{
    presetBox.clear();
    procStatePresets.setPresetAndFolderNames(presetBox);
}

void StateComponent::ifPresetActiveShowInBox()
{
    const int currentPresetId{procStatePresets.getCurrentPresetId()};
    const int numPresets{procStatePresets.getNumPresets()};
    if (1 <= currentPresetId && currentPresetId <= numPresets)
        presetBox.setSelectedId(currentPresetId);
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
        bool choice = juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::NoIcon,
                                                              "Warning", "Delete preset?", nullptr, nullptr);
        if (choice)
        {
            procStatePresets.deletePreset();
            refreshPresetBox();
        }
    }
    else
    {
//        juce::NativeMessageBox::showMessageBox(juce::AlertWindow::NoIcon, "Warning", "No preset!");
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::NoIcon, "Warning", "No preset!");
    }
}

void StateComponent::savePresetAlertWindow()
{
    juce::File userFile = procStatePresets.getFile().getChildFile("User");
    creatFolderIfNotExist(userFile);
    /*
    juce::FileChooser filechooser("save preset", userFile, "*", true, false, nullptr);
    if (filechooser.browseForFileToSave(true))
    {
        // this is really shit code, but I don't have any other way to solve this shit problem!!!
        juce::File inputName = filechooser.getResult();
        bool isSaved = procStatePresets.savePreset(inputName);
        if (isSaved)
        {
            refreshPresetBox();
            presetBox.setSelectedId(procStatePresets.getCurrentPresetId());
        }
    }
     */
    
    //fileChooser = std::make_unique<juce::FileChooser> ("save preset", userFile, "*", true, false, nullptr);
    fileChooser = std::make_unique<juce::FileChooser> ("save preset", userFile, "*");
    auto folderChooserFlags = juce::FileBrowserComponent::saveMode;

    fileChooser->launchAsync (folderChooserFlags, [this] (const juce::FileChooser& chooser)
    {
        // this is really shit code, but I don't have any other way to solve this shit problem!!!
        juce::File inputName = chooser.getResult();
        bool isSaved = procStatePresets.savePreset(inputName);
        if (isSaved)
        {
            refreshPresetBox();
            presetBox.setSelectedId(procStatePresets.getCurrentPresetId());
        }
    });
}

void StateComponent::openPresetFolder()
{
    // open preset folder
    juce::File userFile = procStatePresets.getFile();
    creatFolderIfNotExist(userFile);
    if (!userFile.existsAsFile())
    {
        juce::File(userFile).startAsProcess();
    }
}

void StateComponent::rescanPresetFolder()
{
    int presetId = procStatePresets.getCurrentPresetId();
    int currentPresetNum;
    int previousPresetNum = procStatePresets.getNumPresets();
    procStatePresets.scanAllPresets();
    refreshPresetBox();
    currentPresetNum = procStatePresets.getNumPresets();
    // if some presets are deleted, set presetId to 0
    if (currentPresetNum < previousPresetNum)
    {
        procStatePresets.setCurrentPresetId(presetId - 1);
    }
    else
    {
        procStatePresets.setCurrentPresetId(presetId);
    }
    presetBox.setSelectedId(procStatePresets.getCurrentPresetId());
}

void StateComponent::creatFolderIfNotExist(juce::File userFile)
{
    if (!userFile.exists())
    {
        userFile.createDirectory();
    }
}

juce::String StateComponent::getPresetName()
{
    return procStatePresets.getPresetName();
}

void StateComponent::popPresetMenu()
{
    presetMenu.clear();
    presetMenu.addItem(1, "Init", true);
    presetMenu.addItem(2, "Open Preset Folder", true);
    presetMenu.addItem(3, "Rescan Preset Folder", true);
    presetMenu.addItem(4, "Give a Star on GitHub!", true);
    presetMenu.addItem(5, "Check for New Version", true);

    float heightScale = getHeight() / 50.0f;
    float widthScale = getWidth() / 1000.0f;
    float scale = juce::jmin(heightScale, widthScale);
    otherLookAndFeel.scale = scale;
    
//    int result = presetMenu.show(0, 250 * widthScale,
//                                 10, 30 * heightScale);
    
    presetMenu.showMenuAsync(juce::PopupMenu::Options().withStandardItemHeight(30 * heightScale)
                                                       .withMinimumWidth(250 * widthScale)
                             , [this](int result)
    {
        if (result == 1)  // init
        {
            // set all parameters to default
            procStatePresets.initPreset();
            // set GUI verticle lines to default
            resetMultiband();
            presetBox.setSelectedId(0);
        }
        else if (result == 2)
        {
            openPresetFolder();
        }
        else if (result == 3)
        {
            rescanPresetFolder();
        }
        else if (result == 4)
        {
            juce::URL gitHubWebsite(GITHUB_LINK);
            gitHubWebsite.launchInDefaultBrowser();
        }
        else if (result == 5)
        {
            versionInfo = VersionInfo::fetchLatestFromUpdateServer();
            
            if (versionInfo == nullptr)
            {
    //            juce::NativeMessageBox::showMessageBox(juce::AlertWindow::WarningIcon,
    //                "Error", "No release found or disconnected from the network!", nullptr);
                juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "No release found or disconnected from the network!");
    //            DBG("No release found or other error occurred!");
            }
            else if(versionInfo->versionString != VERSION)
            {
                
                
                /** old code
                 */
                /*
                juce::String version = versionInfo->versionString;
                bool choice = juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::InfoIcon,
                    "New Version", "New version " + version + " available, do you want to download it?", nullptr, nullptr);
                if (choice)
                {
                    juce::URL gitHubWebsite("https://github.com/jerryuhoo/Fire/releases/tag/" + version);
                    gitHubWebsite.launchInDefaultBrowser();
                } */
                /** old code
                 */
                
                version = versionInfo->versionString;
                const auto callback = juce::ModalCallbackFunction::create ([this](int result) {
                    if (result == 1) // result == 1 means user clicks OK
                    {
                        juce::URL gitHubWebsite(GITHUB_TAG_LINK + version);
                        gitHubWebsite.launchInDefaultBrowser();
                    }
                });
                juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::InfoIcon,
                    "New Version", "New version " + version + " available, do you want to download it?", nullptr, callback);
            }
            else
            {
    //            juce::NativeMessageBox::showMessageBox(juce::AlertWindow::InfoIcon,
    //                "New Version", "You are up to date!", nullptr);
                juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "New Version", "You are up to date!");
            }
        }
    });
}

void StateComponent::resetMultiband()
{
    multiband.reset();
}

void StateComponent::setChangedState(bool state)
{
    isChanged = state;
}

bool StateComponent::getChangedState()
{
    return isChanged;
}

juce::ComboBox* StateComponent::getPresetBox()
{
    return &presetBox;
}

juce::Button* StateComponent::getToggleABButton()
{
    return &toggleABButton;
}

StatePresets* StateComponent::getProcStatePresets()
{
    return &procStatePresets;
}

StateAB* StateComponent::getProcStateAB()
{
    return &procStateAB;
}
//juce::TextButton& StateComponent::getNextButton()
//{
//    return nextButton;
//}
//
//juce::TextButton& StateComponent::getPreviousButton()
//{
//    return previousButton;
//}


} // namespace state
