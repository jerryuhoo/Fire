/*
 ==============================================================================
 
 Preset.cpp
 Created: 12 Jul 2020 9:06:49pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#include "Preset.h"
#include "../../PluginProcessor.h"

namespace state
{

    //==============================================================================
    void saveStateToXml(const juce::AudioProcessor& proc, juce::XmlElement& xml)
    {
        //xml.removeAllAttributes(); // clear first
        auto& fireProc = static_cast<const FireAudioProcessor&>(proc);

        for (const auto& param : fireProc.getParameters())
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                xml.setAttribute(p->paramID, p->getValue());
            }

        // 1. Save LFO Shapes
        auto* lfoState = xml.createNewChildElement("LFO_STATE");

        auto& mutableProc = const_cast<FireAudioProcessor&>(fireProc);
        const auto& lfoDataToSave = mutableProc.getLfoData();

        for (int i = 0; i < lfoDataToSave.size(); ++i)
        {
            auto* lfoXml = new juce::XmlElement("LFO");
            lfoXml->setAttribute("index", i);
            lfoDataToSave[i].writeToXml(*lfoXml);
            lfoState->addChildElement(lfoXml);
        }

        // 2. Save Modulation Matrix Routings
        auto* modMatrixState = xml.createNewChildElement("MODULATION_STATE");
        for (const auto& routing : fireProc.getLfoManager().getModulationRoutings())
        {
            // Only save routings that are actually in use
            if (! routing.targetParameterID.isEmpty())
            {
                auto* routingXml = new juce::XmlElement("ROUTING");
                routing.writeToXml(*routingXml);
                modMatrixState->addChildElement(routingXml);
            }
        }
    }

    void loadStateFromXml(const juce::XmlElement& xml, juce::AudioProcessor& proc)
    {
        auto& fireProc = static_cast<FireAudioProcessor&>(proc);

        // This loop now handles ALL parameters directly, restoring the correct, stable logic.
        for (const auto& param : proc.getParameters())
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                // Check if the preset XML has an attribute with this parameter's ID.
                if (xml.hasAttribute(p->paramID))
                {
                    // If it exists, load the value from the XML.
                    // Note: We use getDoubleAttribute as it safely covers both float and int parameters.
                    p->setValueNotifyingHost((float) xml.getDoubleAttribute(p->paramID, p->getValue()));
                }
                else
                {
                    // If the parameter is not in the preset (e.g., an older preset),
                    // set it to its default value to ensure a predictable state.
                    p->setValueNotifyingHost(p->getDefaultValue());
                }
            }
        }

        // --- Load LFO and Matrix data ---

        // Cast to non-const to modify the processor's state
        auto& mutableFireProc = const_cast<FireAudioProcessor&>(fireProc);

        // Get mutable references to the data containers
        auto& lfoDataToLoad = mutableFireProc.getLfoData();
        auto& modRoutingsToLoad = mutableFireProc.getLfoManager().getModulationRoutings();

        // Clear the existing data using the new references
        for (auto& lfo : lfoDataToLoad)
        {
            lfo = LfoData();
        }
        modRoutingsToLoad.clear();

        // 1. Load LFO Shapes
        if (auto* lfoState = xml.getChildByName("LFO_STATE"))
        {
            for (auto* lfoXml : lfoState->getChildIterator())
            {
                const int index = lfoXml->getIntAttribute("index", -1);
                if (juce::isPositiveAndBelow(index, (int) lfoDataToLoad.size()))
                {
                    // Load data into the LfoManager via the reference
                    lfoDataToLoad[index] = LfoData::readFromXml(*lfoXml);
                }
            }
        }

        // 2. Load Modulation Matrix Routings
        if (auto* modMatrixState = xml.getChildByName("MODULATION_STATE"))
        {
            fireProc.getLfoManager().getModulationRoutings().clear();
            for (auto* routingXml : modMatrixState->getChildIterator())
            {
                fireProc.getLfoManager().getModulationRoutings().add(ModulationRouting::readFromXml(*routingXml));
            }
        }

        // 3. IMPORTANT: Notify the editor to update its display
        if (auto* editor = fireProc.getActiveEditor())
        {
            editor->repaint();
        }

        fireProc.sendChangeMessage();
        fireProc.getLfoManager().onLfoShapeChanged(-1);
    }

    //==============================================================================
    StateAB::StateAB(juce::AudioProcessor& p)
        : pluginProcessor { p }
    {
        copyAB();
    }

    void StateAB::toggleAB()
    {
        juce::XmlElement temp { "Temp" };
        saveStateToXml(pluginProcessor, temp); // current to temp
        loadStateFromXml(ab, pluginProcessor); // ab to current
        ab = temp; // temp to ab
    }

    void StateAB::copyAB()
    {
        ab.removeAllAttributes();
        ab.deleteAllChildElements();
        saveStateToXml(pluginProcessor, ab);
    }

    void StateAB::reset()
    {
        pluginProcessor.reset();
    }

    //==============================================================================

    void parseFileToXmlElement(const juce::File& file, juce::XmlElement& xml)
    {
        std::unique_ptr<juce::XmlElement> parsed { juce::XmlDocument::parse(file) };
        if (parsed)
            xml = *parsed;
    }

    bool writeXmlElementToFile(const juce::XmlElement& xml, juce::File& file, juce::String presetName, bool hasExtension)
    {
        //createFileIfNonExistant(file);
        // 1 saved a new file
        // 2 replaced a existing file
        // 3 do nothing
        if (! file.exists())
        {
            file.create();
            xml.writeTo(file);
            return true;
        }
        else
        {
            if (! hasExtension) // pop up alert window
            {
                bool choice = juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                                      "\"" + presetName + PRESET_EXETENSION + "\" already exists. Do you want to replace it?",
                                                                      "A file or folder with the same name already exists in the folder User. Replacing it will overwrite its current contents.",
                                                                      nullptr,
                                                                      nullptr);
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
        int compareElements(juce::XmlElement* first, juce::XmlElement* second) const
        {
            // Check if the elements are presets (i.e., they have a "presetName" attribute).
            bool firstIsPreset = first->hasAttribute("presetName");
            bool secondIsPreset = second->hasAttribute("presetName");

            // If both are presets, sort them by presetName.
            if (firstIsPreset && secondIsPreset)
            {
                return first->getStringAttribute("presetName")
                    .compareNatural(second->getStringAttribute("presetName"));
            }
            // If both are folders, sort them by folder name (TagName).
            else if (! firstIsPreset && ! secondIsPreset)
            {
                return first->getTagName().compareNatural(second->getTagName());
            }
            // If one is a preset and the other is a folder, put the folder first.
            else if (firstIsPreset && ! secondIsPreset)
            {
                return 1; // first (preset) > second (folder)
            }
            else // !firstIsPreset && secondIsPreset
            {
                return -1; // first (folder) < second (preset)
            }
        }

    private:
        juce::String attributeToSort;
        int direction;
    };

    //==============================================================================
    StatePresets::StatePresets(juce::AudioProcessor& proc, const juce::String& presetFileLocation)
        : pluginProcessor { proc },
          presetFile { juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile(presetFileLocation) }
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

    void StatePresets::recursiveFileSearch(juce::XmlElement& parentXML, juce::File dir)
    {
        juce::RangedDirectoryIterator iterator(dir, false, "*", 3); // findDirectories = 1, findFiles = 2, findFilesAndDirectories = 3, ignoreHiddenFiles = 4
        for (auto file : iterator)
        {
            // is folder
            if (file.isDirectory())
            {
                juce::String folderName;
                folderName = file.getFile().getFileName();
                std::unique_ptr<juce::XmlElement> currentState { new juce::XmlElement { folderName } }; // must be pointer as parent takes ownership
                //DBG(dir.getChildFile(file.getFile().getFileName()).getFullPathName());
                recursiveFileSearch(*currentState, dir.getChildFile(file.getFile().getFileName()));
                parentXML.addChildElement(currentState.release()); // will be deleted by parent element
            }
            // is preset
            else if (file.getFile().hasFileExtension(PRESET_EXETENSION))
            {
                numPresets++;
                juce::String newPresetId = getNextAvailablePresetId(); // presetId format: "preset##"
                std::unique_ptr<juce::XmlElement> currentState { new juce::XmlElement { newPresetId } }; // must be pointer as parent takes ownership
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
            }
            else
            {
                // not a fire preset, maybe popup alert window?
                //jassertfalse;
            }
        }
    }

    void StatePresets::recursiveSort(juce::XmlElement* parent)
    {
        PresetNameSorter sorter;
        parent->sortChildElements(sorter);

        for (auto* child : parent->getChildIterator())
        {
            // If the child element is a folder (i.e., it doesn't have a "presetName" attribute), sort it recursively.
            if (! child->hasAttribute("presetName"))
            {
                recursiveSort(child);
            }
        }
    }

    void StatePresets::scanAllPresets()
    {
        numPresets = 0;
        mPresetXml.deleteAllChildElements();
        //RangedDirectoryIterator iterator(presetFile, true, "*.fire", 2);

        recursiveFileSearch(mPresetXml, presetFile);

        recursiveSort(&mPresetXml);

        //mPresetXml.writeTo(File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Audio/Presets/Wings/Fire/test.xml"));
    }

    juce::String StatePresets::savePreset(juce::File savePath)
    {
        juce::File userPresetFile;
        bool hasExtension = false;
        juce::String presetName = savePath.getFileNameWithoutExtension();

        // If the filename is empty (e.g., user cancelled the dialog), return an empty string.
        if (presetName.isEmpty())
            return juce::String();

        if (! savePath.hasFileExtension(PRESET_EXETENSION))
        {
            userPresetFile = savePath.getFullPathName() + PRESET_EXETENSION;
        }
        else
        {
            userPresetFile = savePath.getFullPathName();
            hasExtension = true;
        }

        // Save the single preset to a real file.
        presetXmlSingle.removeAllAttributes(); // Clear all first.
        presetXmlSingle.setAttribute("presetName", presetName); // Set preset name.
        saveStateToXml(pluginProcessor, presetXmlSingle);

        bool isSaved = writeXmlElementToFile(presetXmlSingle, userPresetFile, presetName, hasExtension);

        if (isSaved)
        {
            statePresetName = presetName; // Also update the internal state.
            scanAllPresets(); // Rescan and sort all presets.

            // Key change: Return the preset name on success.
            return presetName;
        }
        else
        {
            // Key change: Return an empty string on failure or cancellation.
            return juce::String();
        }
    }

    void StatePresets::recursivePresetLoad(juce::XmlElement parentXml, juce::String presetId)
    {
        //    int index = 0;

        for (auto* child : parentXml.getChildIterator())
        {
            if (child->hasAttribute("presetName") && child->getTagName() == presetId)
            {
                juce::XmlElement loadThisChild { *child }; // (0 indexed method)
                loadStateFromXml(loadThisChild, pluginProcessor);
                statePresetName = child->getAttributeValue(0); //presetName index is 0
                break;
            }
            else
            {
                recursivePresetLoad(*child, presetId);
            }
            //        index++;
        }
    }

    void StatePresets::loadPreset(juce::String presetId)
    {
        recursivePresetLoad(mPresetXml, presetId);
    }

    void StatePresets::deletePreset()
    {
        juce::XmlElement* childToDelete { mPresetXml.getChildElement(mCurrentPresetId - 1) };
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

    void StatePresets::recursivePresetNameAdd(juce::XmlElement parentXml, juce::ComboBox& menu, int& index)
    {
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
                comboBoxIdToTagNameMap.set(index, child->getTagName());

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

    void StatePresets::setPresetAndFolderNames(juce::ComboBox& menu)
    {
        comboBoxIdToTagNameMap.clear();
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

    const juce::XmlElement& StatePresets::getPresetXml() const
    {
        return mPresetXml;
    }

    void StatePresets::initPreset()
    {
        for (const auto& param : pluginProcessor.getParameters())
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                // if not in xml set current
                p->setValueNotifyingHost(p->getDefaultValue());
        // set preset combobox to 0
        statePresetName = "";
        mCurrentPresetId = 0;

        auto& fireProc = static_cast<FireAudioProcessor&>(pluginProcessor);
        for (auto& lfo : fireProc.getLfoManager().getLfoData())
        {
            lfo.resetToDefault();
        }

        auto& routings = fireProc.getLfoManager().getModulationRoutings();
        routings.clear();

        fireProc.getLfoManager().onLfoShapeChanged(-1);

        // Notify the editor to update its display
        if (auto* editor = fireProc.getActiveEditor())
        {
            editor->repaint();
        }
    }

    //==============================================================================

    //==============================================================================
    StateComponent::StateComponent(StateAB& sab, StatePresets& sp, juce::AudioProcessorValueTreeState& vts)
        : procStateAB { sab },
          procStatePresets { sp },
          valueTreeState { vts },
          toggleABButton { "A" },
          copyABButton { "Copy" },
          previousButton { "" },
          nextButton { "" },
          savePresetButton { "Save" },
          //deletePresetButton{"Delete"},
          menuButton { "Menu" }
    {
        auto& params = procStatePresets.getProcessor().getParameters();
        for (auto param : params)
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                valueTreeState.addParameterListener(p->paramID, this);
            }
        }
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
        { updatePresetBox(presetBox.getSelectedId()); };

        refreshPresetBox();

        // 1. 获取宿主保存的预设ID和名称
        const int currentPresetId = procStatePresets.getCurrentPresetId();
        juce::String presetNameFromHost = procStatePresets.getPresetName();
        const int numPresets = procStatePresets.getNumPresets();

        if (currentPresetId > 0 && currentPresetId <= numPresets)
        {
            juce::String presetNameFromHost = presetBox.getItemText(presetBox.indexOfItemId(currentPresetId));
            juce::XmlElement* presetXml = nullptr;

            // 关键修复：将 lambda 参数改为 const juce::XmlElement&
            std::function<juce::XmlElement*(const juce::XmlElement&, const juce::String&)> findPresetInXml =
                [&](const juce::XmlElement& parentXml, const juce::String& nameToFind) -> juce::XmlElement*
            {
                for (auto* child : parentXml.getChildIterator())
                {
                    if (child->hasAttribute("presetName") && child->getStringAttribute("presetName") == nameToFind)
                    {
                        return const_cast<juce::XmlElement*>(child);
                    }

                    if (child->getNumChildElements() > 0)
                    {
                        if (auto* foundChild = findPresetInXml(*child, nameToFind))
                        {
                            return foundChild;
                        }
                    }
                }
                return nullptr;
            };

            presetXml = findPresetInXml(procStatePresets.getPresetXml(), presetNameFromHost);

            if (presetXml != nullptr)
            {
                auto& fireProc = static_cast<FireAudioProcessor&>(procStatePresets.getProcessor());
                bool areParametersEqual = fireProc.isCurrentStateEquivalentToPreset(*presetXml);

                if (! areParametersEqual)
                {
                    isChanged = true;
                    const juce::String nameToDisplay = presetNameFromHost + "*";
                    presetBox.setText(nameToDisplay, juce::dontSendNotification);
                    presetBox.setSelectedId(currentPresetId, juce::dontSendNotification);
                    markAsDirty();
                }
                else
                {
                    presetBox.setSelectedId(currentPresetId, juce::dontSendNotification);
                    presetBox.setText(presetNameFromHost, juce::dontSendNotification);
                }
            }
        }

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
        previousButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
        previousButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
        previousButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
        previousButton.setComponentID("left_arrow");
        nextButton.setColour(juce::TextButton::textColourOffId, COLOUR1);
        nextButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
        nextButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
        nextButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
        nextButton.setComponentID("right_arrow");
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
        presetMenu.setLookAndFeel(&fireLookAndFeel);

        menuButton.getLookAndFeel().setColour(juce::PopupMenu::textColourId, COLOUR1);
        menuButton.getLookAndFeel().setColour(juce::PopupMenu::highlightedBackgroundColourId, COLOUR5);
        menuButton.getLookAndFeel().setColour(juce::PopupMenu::highlightedTextColourId, COLOUR1);
        menuButton.getLookAndFeel().setColour(juce::PopupMenu::headerTextColourId, COLOUR1);
        menuButton.getLookAndFeel().setColour(juce::PopupMenu::backgroundColourId, COLOUR6);
    }

    StateComponent::~StateComponent()
    {
        auto& params = procStatePresets.getProcessor().getParameters();
        for (auto param : params)
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                valueTreeState.removeParameterListener(p->paramID, this);
            }
        }
    }

    void StateComponent::parameterChanged(const juce::String& parameterID, float newValue)
    {
        // This function is called whenever any parameter changes, including:
        // a) User interaction with the UI controls
        // b) Host sending automation data
        // c) We load a preset ourselves

        // If the change is caused by a programmatic preset load, we should not mark it as "dirty"
        // So we use the isProgrammaticChange flag to ignore these changes
        if (isProgrammaticChange)
            return;

        // Otherwise, the change is user-initiated, so we call markAsDirty
        markAsDirty();
    }

    void StateComponent::paint(juce::Graphics& /*g*/)
    {
        //g.fillAll (Colours::lightgrey);
    }

    void StateComponent::resized()
    {
        juce::Rectangle<int> r(getLocalBounds());
        const int numComponents { 6 };
        const int componentWidth { getWidth() / numComponents };

        toggleABButton.setBounds(r.removeFromLeft(componentWidth));
        copyABButton.setBounds(r.removeFromLeft(componentWidth));
        presetBox.setBounds(r.removeFromLeft(componentWidth * 2).reduced(getHeight() / 15)); // if reduced value is getHeight() / 10, juce has a bug?
        previousButton.setBounds(r.removeFromLeft(componentWidth / 4));
        nextButton.setBounds(r.removeFromLeft(componentWidth / 4));
        savePresetButton.setBounds(r.removeFromLeft(componentWidth));
        //deletePresetButton.setBounds(r.removeFromLeft(componentWidth));
        menuButton.setBounds(r.removeFromLeft(componentWidth));
    }

    void StateComponent::buttonClicked(juce::Button* clickedButton)
    {
        //    if (clickedButton == &toggleABButton)
        //    {
        //        the code is moved to Editor
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
            popPresetMenu();
    }

    void StateComponent::markAsDirty()
    {
        // Only perform the operation if a "clean" preset is currently selected (ID > 0 and does not have a *).
        if (presetBox.getSelectedId() > 0 && ! presetBox.getText().endsWith("*"))
        {
            const auto currentText = presetBox.getText();

            // 1. First, set the selectedId to 0 in the background.
            //    This makes the ComboBox's internal state believe "nothing is selected," preparing it to fire onChange
            //    for the next step when we select the same item.
            presetBox.setSelectedId(0, juce::dontSendNotification);

            // 2. Then, immediately overwrite the default text ("- Init -") that might appear due to ID=0
            //    with our desired "dirty" text.
            presetBox.setText(currentText + "*", juce::dontSendNotification);
        }
    }

    void StateComponent::setPreviousPreset()
    {
        int presetIndex = procStatePresets.getCurrentPresetId() - 1;
        if (presetIndex > 0)
        {
            presetBox.setSelectedId(presetIndex);
        }
    }

    void StateComponent::setNextPreset()
    {
        int presetIndex = procStatePresets.getCurrentPresetId() + 1;
        if (presetIndex <= procStatePresets.getNumPresets())
        {
            presetBox.setSelectedId(presetIndex);
        }
    }

    void StateComponent::comboBoxChanged(juce::ComboBox* changedComboBox)
    {
        // use presetBox.onChange instead!
    }

    void StateComponent::updatePresetBox(int selectedId) // when preset is changed
    {
        isChanged = true; // do it first

        if (selectedId > 0)
        {
            // Get preset name
            juce::String presetName = presetBox.getItemText(presetBox.indexOfItemId(selectedId));

            // If the name ends with '*', it means the user is trying to restore a preset
            if (presetName.endsWith("*"))
            {
                // Remove '*' to get the actual preset name
                presetName = presetName.dropLastCharacters(1);
            }

            auto* presetManager = &procStatePresets;

            const juce::String internalIdToLoad = presetManager->comboBoxIdToTagNameMap[selectedId];

            if (internalIdToLoad.isNotEmpty())
            {
                isProgrammaticChange = true;
                presetManager->setCurrentPresetId(selectedId);
                presetManager->loadPreset(internalIdToLoad);

                juce::MessageManager::callAsync([this]()
                                                { isProgrammaticChange = false; });

                juce::String presetName = presetBox.getItemText(presetBox.indexOfItemId(selectedId));

                presetBox.setText(presetName, juce::dontSendNotification);

                presetBox.setSelectedId(selectedId, juce::dontSendNotification);
            }
            else
            {
                jassertfalse;
            }
        }
    }

    void StateComponent::refreshPresetBox() // rescan, init, save, or delete
    {
        presetBox.clear();
        procStatePresets.setPresetAndFolderNames(presetBox);
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
            const auto callback = juce::ModalCallbackFunction::create([this](int choice)
                                                                      {
            if (choice)
            {
                procStatePresets.deletePreset();
                refreshPresetBox();
            } });
            juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::NoIcon,
                                                    "Warning",
                                                    "Delete preset?",
                                                    nullptr,
                                                    callback);
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::NoIcon, "Warning", "No preset!");
        }
    }

    void StateComponent::savePresetAlertWindow()
    {
        juce::File userFile = procStatePresets.getFile().getChildFile("User");
        creatFolderIfNotExist(userFile);

        fileChooser = std::make_unique<juce::FileChooser>("save preset", userFile, "*");
        auto folderChooserFlags = juce::FileBrowserComponent::saveMode;

        fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
                                 {
            juce::File inputName = chooser.getResult();

            // 1. Call the modified savePreset and get the returned name.
            juce::String savedPresetName = procStatePresets.savePreset(inputName);

            // 2. Check if the name is valid (i.e., save was successful and not cancelled).
            if (savedPresetName.isNotEmpty())
            {
                // 3. Refresh the UI to load the new preset list.
                refreshPresetBox();

                // 4. Iterate to find the newly saved preset, using the same logic as in rescanPresetFolder.
                int newPresetIdToSelect = 0;
                for (int i = 0; i < presetBox.getNumItems(); ++i)
                {
                    if (presetBox.getItemId(i) > 0 && presetBox.getItemText(i) == savedPresetName)
                    {
                        newPresetIdToSelect = presetBox.getItemId(i);
                        break;
                    }
                }
                
                // 5. Once found, select it using its new ID.
                //    We want to trigger the onChange callback here to ensure all states are updated correctly,
                //    so a direct call to setSelectedId (which sends a notification by default) is what we need.
                if (newPresetIdToSelect > 0)
                {
                    presetBox.setSelectedId(newPresetIdToSelect);
                }
            } });
    }

    void StateComponent::openPresetFolder()
    {
        // open preset folder
        juce::File userFile = procStatePresets.getFile();
        creatFolderIfNotExist(userFile);
        if (! userFile.existsAsFile())
        {
            juce::File(userFile).startAsProcess();
        }
    }

    void StateComponent::rescanPresetFolder()
    {
        juce::String previouslySelectedName;
        if (presetBox.getSelectedId() > 0)
        {
            previouslySelectedName = presetBox.getText();
            if (previouslySelectedName.endsWith("*"))
                previouslySelectedName = previouslySelectedName.dropLastCharacters(1);
        }

        procStatePresets.scanAllPresets();
        refreshPresetBox();

        int newPresetIdToSelect = 0;

        if (previouslySelectedName.isNotEmpty())
        {
            for (int i = 0; i < presetBox.getNumItems(); ++i)
            {
                if (presetBox.getItemId(i) > 0 && presetBox.getItemText(i) == previouslySelectedName)
                {
                    newPresetIdToSelect = presetBox.getItemId(i);
                    break;
                }
            }
        }

        procStatePresets.setCurrentPresetId(newPresetIdToSelect);
        presetBox.setSelectedId(newPresetIdToSelect, juce::dontSendNotification);
    }

    void StateComponent::creatFolderIfNotExist(juce::File userFile)
    {
        if (! userFile.exists())
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
        presetMenu.addItem(6, "Settings", true);

        float heightScale = getHeight() / 50.0f;
        float widthScale = getWidth() / 1000.0f;
        float scale = juce::jmin(heightScale, widthScale);
        fireLookAndFeel.scale = scale;

        presetMenu.showMenuAsync(juce::PopupMenu::Options().withStandardItemHeight(30 * heightScale).withMinimumWidth(250 * widthScale), [this](int result)
                                 {
        if (result == 1)  // init
        {
            isChanged = true;
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
                juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "No release found or disconnected from the network!");
            }
            else if(!versionInfo->versionString.equalsIgnoreCase(juce::String("v") + juce::String(VERSION)))
            {
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
                juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "New Version", "You are up to date!");
            }
        }
        else if (result == 6) // The "Settings..." menu item
        {
            // ======== THE FINAL, SAFEST METHOD USING A MODAL DIALOG WINDOW ========

            // 1. Get a reference to the processor
            auto& ourProcessor = static_cast<FireAudioProcessor&>(procStatePresets.getProcessor());

            // 2. Create the settings panel on the heap
            auto* settingsPanel = new SettingsComponent(ourProcessor.getAppSettings());

            // 3. Configure DialogWindow launch options
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(settingsPanel);
            options.content->setSize(400, 300);
            options.dialogTitle = "Settings";
            options.dialogBackgroundColour = COLOUR6;
            options.escapeKeyTriggersCloseButton = true;
            options.useNativeTitleBar = true;
            options.resizable = true;
            options.componentToCentreAround = this;
            // 4. Launch dialog asynchronously
            options.launchAsync();
        } });
    }

    void StateComponent::resetMultiband()
    {
        procStateAB.reset();
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

    juce::TextButton* StateComponent::getCopyABButton()
    {
        return &copyABButton;
    }

    juce::TextButton* StateComponent::getPreviousButton()
    {
        return &previousButton;
    }

    juce::TextButton* StateComponent::getNextButton()
    {
        return &nextButton;
    }

} // namespace state
