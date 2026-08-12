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
        auto& fireProc = static_cast<const FireAudioProcessor&>(proc);
        xml.deleteAllChildElements();

        for (const auto& param : fireProc.getParameters())
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                xml.setAttribute(p->paramID, p->getValue());

        const auto lfoDataToSave = fireProc.getLfoManager().getLfoDataCopy();
        const auto routingsToSave = fireProc.getLfoManager().getModulationRoutingsCopy();

        // 1. Save LFO Shapes
        auto* lfoState = xml.createNewChildElement("LFO_STATE");

        for (int i = 0; i < static_cast<int>(lfoDataToSave.size()); ++i)
        {
            auto lfoXml = std::make_unique<juce::XmlElement>("LFO");
            lfoXml->setAttribute("index", i);
            lfoDataToSave[static_cast<size_t>(i)].writeToXml(*lfoXml);
            lfoState->addChildElement(lfoXml.release());
        }

        // 2. Save Modulation Matrix Routings
        auto* modMatrixState = xml.createNewChildElement("MODULATION_STATE");
        for (const auto& routing : routingsToSave)
        {
            if (! routing.targetParameterID.isEmpty())
            {
                auto routingXml = std::make_unique<juce::XmlElement>("ROUTING");
                routing.writeToXml(*routingXml);
                modMatrixState->addChildElement(routingXml.release());
            }
        }
    }

    void loadStateFromXml(const juce::XmlElement& xml, juce::AudioProcessor& proc)
    {
        auto& fireProc = static_cast<FireAudioProcessor&>(proc);

        for (const auto& param : proc.getParameters())
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                float valueToLoad = p->getDefaultValue();
                if (xml.hasAttribute(p->paramID))
                {
                    const auto valueFromXml = static_cast<float>(xml.getDoubleAttribute(p->paramID, p->getValue()));
                    valueToLoad = std::isfinite(valueFromXml) ? juce::jlimit(0.0f, 1.0f, valueFromXml) : p->getDefaultValue();
                }
                else if (p->paramID.startsWith(SHAPE_BYPASS_ID))
                {
                    valueToLoad = 1.0f;
                }

                p->setValueNotifyingHost(valueToLoad);
            }
        }

        std::array<LfoData, 4> lfoDataToLoad;
        std::array<bool, 4> loadedLfoSmoothness {};
        std::array<bool, 4> loadedSmoothnessParameter {};
        for (int i = 0; i < static_cast<int>(loadedSmoothnessParameter.size()); ++i)
            loadedSmoothnessParameter[static_cast<size_t>(i)] = xml.hasAttribute(
                ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i));

        if (auto* lfoState = xml.getChildByName("LFO_STATE"))
        {
            for (auto* lfoXml : lfoState->getChildIterator())
            {
                const int index = lfoXml->getIntAttribute("index", -1);
                if (juce::isPositiveAndBelow(index, (int) lfoDataToLoad.size()))
                {
                    lfoDataToLoad[static_cast<size_t>(index)] = LfoData::readFromXml(*lfoXml);
                    loadedLfoSmoothness[static_cast<size_t>(index)] = lfoXml->hasAttribute("smoothness");
                }
            }
        }

        // Old preset files may contain APVTS smooth parameters but no matching
        // LFO_STATE smoothness attribute. Preserve that parameter value instead
        // of replacing it with LfoData's default during the shape reset below.
        for (int i = 0; i < static_cast<int>(lfoDataToLoad.size()); ++i)
        {
            const auto index = static_cast<size_t>(i);
            // The parameter attribute is authoritative when present. The LFO
            // XML value is retained only for presets old enough to lack it.
            if (! loadedSmoothnessParameter[index] && loadedLfoSmoothness[index])
                continue;

            if (const auto* smoothness = fireProc.treeState.getRawParameterValue(
                    ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i)))
                lfoDataToLoad[index].smoothness = smoothness->load(std::memory_order_relaxed);
        }

        juce::Array<ModulationRouting> routingsToLoad;
        if (auto* modMatrixState = xml.getChildByName("MODULATION_STATE"))
        {
            for (auto* routingXml : modMatrixState->getChildIterator())
            {
                auto routing = ModulationRouting::readFromXml(*routingXml);
                routing.sourceLfoIndex = juce::jlimit(0, 3, routing.sourceLfoIndex);
                routing.depth = std::isfinite(routing.depth) ? juce::jlimit(-1.0f, 1.0f, routing.depth) : 0.5f;

                if (routing.targetParameterID.isNotEmpty() && fireProc.treeState.getParameter(routing.targetParameterID) != nullptr)
                    routingsToLoad.add(std::move(routing));
            }
        }

        auto& manager = fireProc.getLfoManager();
        {
            const juce::ScopedLock lock(manager.getLfoDataLock());
            manager.clearAllLfoData();
            for (int i = 0; i < static_cast<int>(lfoDataToLoad.size()); ++i)
                manager.setLfoData(i, lfoDataToLoad[static_cast<size_t>(i)]);
            manager.getModulationRoutings() = std::move(routingsToLoad);
        }

        // A preset/A-B swap may replace every logical band while retaining the
        // same NUM_BANDS value. Publish the completed migration explicitly so
        // the audio thread cannot reuse DSP history from the previous slots.
        fireProc.requestMultibandTopologyReset();
        fireProc.sendChangeMessage();
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

    bool parseFileToXmlElement(const juce::File& file, juce::XmlElement& xml)
    {
        auto parsed = juce::XmlDocument::parse(file);
        if (parsed == nullptr)
            return false;

        xml = *parsed;
        return true;
    }

    bool writeXmlElementToFile(const juce::XmlElement& xml,
                               juce::File& file,
                               const juce::String& presetName,
                               bool hasExtension)
    {
        if (! file.exists())
            return xml.writeTo(file);

        if (! hasExtension)
        {
            const bool choice = juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                                        "\"" + presetName + PRESET_EXETENSION + "\" already exists. Do you want to replace it?",
                                                                        "A file or folder with the same name already exists in the folder User. Replacing it will overwrite its current contents.",
                                                                        nullptr,
                                                                        nullptr);
            if (! choice)
                return false;

            return xml.writeTo(file);
        }

        return xml.writeTo(file);
    }

    static juce::String getFolderDisplayName(const juce::XmlElement& folder)
    {
        return folder.getStringAttribute("folderName", folder.getTagName());
    }

    //==============================================================================
    //sorter
    class PresetNameSorter
    {
    public:
        int compareElements(juce::XmlElement* first, juce::XmlElement* second) const
        {
            const bool firstIsPreset = first->hasAttribute("presetName");
            const bool secondIsPreset = second->hasAttribute("presetName");

            if (firstIsPreset && secondIsPreset)
            {
                return first->getStringAttribute("presetName")
                    .compareNatural(second->getStringAttribute("presetName"));
            }

            if (! firstIsPreset && ! secondIsPreset)
            {
                return getFolderDisplayName(*first).compareNatural(getFolderDisplayName(*second));
            }

            return firstIsPreset ? 1 : -1;
        }
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

    void StatePresets::recursiveFileSearch(juce::XmlElement& parentXML, const juce::File& dir)
    {
        juce::RangedDirectoryIterator iterator(dir,
                                                false,
                                                "*",
                                                juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles);
        for (auto file : iterator)
        {
            if (file.isDirectory())
            {
                auto currentState = std::make_unique<juce::XmlElement>("FOLDER");
                currentState->setAttribute("folderName", file.getFile().getFileName());
                recursiveFileSearch(*currentState, file.getFile());
                parentXML.addChildElement(currentState.release());
            }
            else if (file.getFile().hasFileExtension(PRESET_EXETENSION))
            {
                auto currentState = std::make_unique<juce::XmlElement>("PRESET");
                if (! parseFileToXmlElement(file.getFile(), *currentState))
                    continue;

                ++numPresets;
                const juce::String newPresetId = getNextAvailablePresetId();
                currentState->setTagName(newPresetId);

                const juce::String newName = file.getFile().getFileNameWithoutExtension();
                if (newName != currentState->getStringAttribute("presetName"))
                    currentState->setAttribute("presetName", newName);

                parentXML.addChildElement(currentState.release());
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
        presetXmlSingle.deleteAllChildElements();
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

    bool StatePresets::recursivePresetLoad(const juce::XmlElement& parentXml, const juce::String& presetId)
    {
        for (auto* child : parentXml.getChildIterator())
        {
            if (child->hasAttribute("presetName") && child->getTagName() == presetId)
            {
                loadStateFromXml(*child, pluginProcessor);
                statePresetName = child->getStringAttribute("presetName");
                return true;
            }

            if (recursivePresetLoad(*child, presetId))
                return true;
        }

        return false;
    }

    void StatePresets::loadPreset(juce::String presetId)
    {
        recursivePresetLoad(mPresetXml, presetId);
    }

    void StatePresets::deletePreset()
    {
        const int currentId = mCurrentPresetId.load(std::memory_order_relaxed);
        const auto presetTag = comboBoxIdToTagNameMap[currentId];
        if (currentId <= 0 || presetTag.isEmpty())
            return;

        std::function<bool(juce::XmlElement&)> removeByTag = [&](juce::XmlElement& parent)
        {
            for (auto* child : parent.getChildIterator())
            {
                if (child->getTagName() == presetTag && child->hasAttribute("presetName"))
                {
                    parent.removeChildElement(child, true);
                    return true;
                }

                if (removeByTag(*child))
                    return true;
            }
            return false;
        };

        if (removeByTag(mPresetXml))
        {
            numPresets = juce::jmax(0, numPresets - 1);
            mCurrentPresetId.store(0, std::memory_order_relaxed);
            statePresetName.clear();
        }
    }

    void StatePresets::setPresetName(juce::String name)
    {
        statePresetName = std::move(name);
    }

    juce::StringRef StatePresets::getPresetName()
    {
        return statePresetName;
    }

    void StatePresets::recursivePresetNameAdd(const juce::XmlElement& parentXml, juce::ComboBox& menu, int& index)
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
                    mCurrentPresetId.store(index);
                }
            }
            else
            {
                // is folder
                if (index != 0)
                {
                    menu.addSeparator();
                }
                juce::String n = getFolderDisplayName(*child);
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
        return mCurrentPresetId.load();
    }

    void StatePresets::setCurrentPresetId(int currentPresetId)
    {
        mCurrentPresetId.store(juce::jmax(0, currentPresetId));
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
        mCurrentPresetId.store(0);

        auto& fireProc = static_cast<FireAudioProcessor&>(pluginProcessor);
        auto& manager = fireProc.getLfoManager();
        {
            const juce::ScopedLock lock(manager.getLfoDataLock());
            manager.clearAllLfoData();
            manager.getModulationRoutings().clear();
        }
        fireProc.requestMultibandTopologyReset();
        fireProc.sendChangeMessage();
    }

    //==============================================================================

    //==============================================================================
    StateComponent::ManualUpdateCheckThread::ManualUpdateCheckThread(StateComponent& ownerToUse)
        : juce::Thread("Fire manual update check"), owner(ownerToUse)
    {
    }

    void StateComponent::ManualUpdateCheckThread::run()
    {
        auto result = fetchOperation.fetchLatest();
        if (! threadShouldExit())
            owner.publishManualUpdateResult(std::move(result));
    }

    void StateComponent::ManualUpdateCheckThread::stop()
    {
        signalThreadShouldExit();
        fetchOperation.cancel();
        stopThread(-1);
    }

    StateComponent::StateComponent(StateAB& sab, StatePresets& sp, juce::AudioProcessorValueTreeState& vts)
        : procStateAB { sab },
          procStatePresets { sp },
          valueTreeState { vts },
          manualUpdateCheckThread { *this },
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

        presetBox.setComponentID("header_preset");
        presetBox.setJustificationType(juce::Justification::centred);
        presetBox.setColour(juce::ComboBox::textColourId, fire::ui::colours::textPrimary);
        presetBox.setColour(juce::ComboBox::arrowColourId, fire::ui::colours::flame);
        presetBox.setColour(juce::ComboBox::buttonColourId, fire::ui::colours::flame);
        presetBox.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
        presetBox.setColour(juce::ComboBox::focusedOutlineColourId, fire::ui::colours::ember);
        presetBox.setColour(juce::ComboBox::backgroundColourId, fire::ui::colours::surface0);
        presetBox.setTextWhenNothingSelected("- Init -");

        // when selecting the same preset, this will revert back to the original preset (in case you changed something)
        presetBox.onChange = [this]
        { updatePresetBox(presetBox.getSelectedId()); };

        refreshPresetBox();

        const int currentPresetId = procStatePresets.getCurrentPresetId();
        const int numPresets = procStatePresets.getNumPresets();

        if (currentPresetId > 0 && currentPresetId <= numPresets)
        {
            const juce::String presetNameFromHost = presetBox.getItemText(presetBox.indexOfItemId(currentPresetId));
            const juce::XmlElement* presetXml = nullptr;

            std::function<const juce::XmlElement*(const juce::XmlElement&, const juce::String&)> findPresetInXml =
                [&](const juce::XmlElement& parentXml, const juce::String& nameToFind) -> const juce::XmlElement*
            {
                for (auto* child : parentXml.getChildIterator())
                {
                    if (child->hasAttribute("presetName") && child->getStringAttribute("presetName") == nameToFind)
                        return child;

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

        auto styleHeaderButton = [](juce::TextButton& button, juce::Colour accent)
        {
            button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
            button.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
            button.setColour(juce::TextButton::textColourOnId, accent);
            button.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        };

        styleHeaderButton(toggleABButton, fire::ui::colours::gold);
        toggleABButton.setComponentID("header_ab");
        toggleABButton.setColour(juce::TextButton::textColourOffId,
                                 fire::ui::colours::gold.withAlpha(0.84f));
        styleHeaderButton(copyABButton, fire::ui::colours::flame);
        copyABButton.setComponentID("header_action");
        styleHeaderButton(previousButton, fire::ui::colours::flame);
        previousButton.setComponentID("header_previous");
        previousButton.setTooltip("Previous preset");
        previousButton.setTitle("Previous preset");
        styleHeaderButton(nextButton, fire::ui::colours::flame);
        nextButton.setComponentID("header_next");
        nextButton.setTooltip("Next preset");
        nextButton.setTitle("Next preset");
        styleHeaderButton(savePresetButton, fire::ui::colours::positive);
        savePresetButton.setComponentID("header_action");
        savePresetButton.setColour(juce::TextButton::textColourOffId,
                                   fire::ui::colours::positive.withAlpha(0.86f));
        //deletePresetButton.setColour(TextButton::textColourOffId, COLOUR1);
        //deletePresetButton.setColour(TextButton::buttonColourId, COLOUR5);
        //deletePresetButton.setColour(ComboBox::outlineColourId, COLOUR5);
        styleHeaderButton(menuButton, fire::ui::colours::flame);
        menuButton.setComponentID("header_menu");
        menuButton.setTooltip("Preset and application menu");
        menuButton.setTitle("Preset and application menu");
        presetMenu.setLookAndFeel(&fireLookAndFeel);
        startTimerHz(30);
    }

    StateComponent::~StateComponent()
    {
        stopTimer();
        juce::PopupMenu::dismissAllActiveMenus();
        presetMenu.setLookAndFeel(nullptr);

        if (settingsDialog != nullptr)
        {
            settingsDialog->setVisible(false);
            settingsDialog->exitModalState(0);
            settingsDialog = nullptr;
        }

        manualUpdateCheckThread.stop();
        cancelPendingUpdate();
        fileChooser.reset();
        presetBox.onChange = nullptr;

        // Remove listeners from all buttons that had them added in the constructor
        toggleABButton.removeListener(this);
        copyABButton.removeListener(this);
        previousButton.removeListener(this);
        nextButton.removeListener(this);
        savePresetButton.removeListener(this);
        menuButton.removeListener(this);

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
        juce::ignoreUnused(parameterID, newValue);

        if (isProgrammaticChange.load(std::memory_order_acquire))
            return;

        dirtyUpdatePending.store(true, std::memory_order_release);
    }

    void StateComponent::timerCallback()
    {
        if (dirtyUpdatePending.exchange(false, std::memory_order_acq_rel))
            markAsDirty();
    }

    void StateComponent::handleAsyncUpdate()
    {
        if (dirtyUpdatePending.exchange(false, std::memory_order_acq_rel))
            markAsDirty();

        if (versionCheckReady.exchange(false, std::memory_order_acq_rel))
            showManualUpdateResult();
    }

    void StateComponent::publishManualUpdateResult(std::unique_ptr<VersionInfo> result)
    {
        {
            const juce::ScopedLock lock(updateResultLock);
            pendingVersionInfo = std::move(result);
        }
        versionCheckReady.store(true, std::memory_order_release);
        triggerAsyncUpdate();
    }

    void StateComponent::showManualUpdateResult()
    {
        std::unique_ptr<VersionInfo> result;
        {
            const juce::ScopedLock lock(updateResultLock);
            result = std::move(pendingVersionInfo);
        }

        if (result == nullptr)
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                        "Error",
                                                        "No release found or disconnected from the network!");
            return;
        }

        if (result->isNewerVersionThanCurrent())
        {
            const auto versionToDownload = result->versionString;
            const auto callback = juce::ModalCallbackFunction::create([versionToDownload](int choice)
                                                                      {
                                                                          if (choice == 1)
                                                                              juce::URL(GITHUB_TAG_LINK + versionToDownload).launchInDefaultBrowser();
                                                                      });
            juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::InfoIcon,
                                                    "New Version",
                                                    "New version " + versionToDownload + " available, do you want to download it?",
                                                    nullptr,
                                                    callback);
            return;
        }

        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                    "New Version",
                                                    "You are up to date!");
    }

    void StateComponent::paint(juce::Graphics& /*g*/)
    {
        //g.fillAll (Colours::lightgrey);
    }

    void StateComponent::resized()
    {
        const auto uiScale = juce::jmax(0.5f, static_cast<float>(getHeight()) / 48.0f);
        const auto gap = juce::jmax(3, juce::roundToInt(6.0f * uiScale));
        const auto controlHeight = juce::jmin(getHeight(),
                                              juce::roundToInt(32.0f * uiScale));
        const auto compactWidth = juce::jmax(28, juce::roundToInt(32.0f * uiScale));
        const auto actionWidth = juce::jmax(42, juce::roundToInt(46.0f * uiScale));
        auto r = getLocalBounds().withSizeKeepingCentre(getWidth(), controlHeight);

        auto placeLeft = [&](juce::Component& component, int width)
        {
            component.setBounds(r.removeFromLeft(width));
            r.removeFromLeft(gap);
        };
        auto placeRight = [&](juce::Component& component, int width)
        {
            component.setBounds(r.removeFromRight(width));
            r.removeFromRight(gap);
        };

        placeLeft(toggleABButton, compactWidth);
        placeLeft(copyABButton, actionWidth);
        placeRight(menuButton, actionWidth);
        placeRight(savePresetButton, actionWidth);
        placeRight(nextButton, compactWidth);
        placeRight(previousButton, compactWidth);
        presetBox.setBounds(r.reduced(0, juce::jmax(1, getHeight() / 12)));
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
        juce::ignoreUnused(changedComboBox);
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
                cancelPendingUpdate();
                isProgrammaticChange.store(true, std::memory_order_release);
                presetManager->setCurrentPresetId(selectedId);
                presetManager->loadPreset(internalIdToLoad);
                isProgrammaticChange.store(false, std::memory_order_release);

                const juce::String loadedPresetName = presetBox.getItemText(presetBox.indexOfItemId(selectedId));

                presetBox.setText(loadedPresetName, juce::dontSendNotification);

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
        if (procStatePresets.getNumPresets() > 0)
        {
            juce::Component::SafePointer<StateComponent> safeThis(this);
            const auto callback = juce::ModalCallbackFunction::create([safeThis](int choice)
                                                                      {
                                                                          if (choice != 0 && safeThis != nullptr)
                                                                          {
                                                                              safeThis->procStatePresets.deletePreset();
                                                                              safeThis->refreshPresetBox();
                                                                          }
                                                                      });
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
        const auto folderChooserFlags = juce::FileBrowserComponent::saveMode;
        juce::Component::SafePointer<StateComponent> safeThis(this);

        fileChooser->launchAsync(folderChooserFlags, [safeThis](const juce::FileChooser& chooser)
                                 {
            if (safeThis == nullptr)
                return;

            juce::File inputName = chooser.getResult();

            const juce::String savedPresetName = safeThis->procStatePresets.savePreset(inputName);

            if (savedPresetName.isNotEmpty())
            {
                safeThis->refreshPresetBox();

                int newPresetIdToSelect = 0;
                for (int i = 0; i < safeThis->presetBox.getNumItems(); ++i)
                {
                    if (safeThis->presetBox.getItemId(i) > 0 && safeThis->presetBox.getItemText(i) == savedPresetName)
                    {
                        newPresetIdToSelect = safeThis->presetBox.getItemId(i);
                        break;
                    }
                }

                if (newPresetIdToSelect > 0)
                    safeThis->presetBox.setSelectedId(newPresetIdToSelect);
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

        juce::Component::SafePointer<StateComponent> safeThis(this);
        presetMenu.showMenuAsync(juce::PopupMenu::Options()
                                     .withStandardItemHeight(juce::roundToInt(30.0f * heightScale))
                                     .withMinimumWidth(juce::roundToInt(250.0f * widthScale)),
                                 [safeThis](int result)
                                 {
                                     if (safeThis == nullptr)
                                         return;

                                     if (result == 1)
                                     {
                                         safeThis->isChanged = true;
                                         safeThis->procStatePresets.initPreset();
                                         safeThis->resetMultiband();
                                         safeThis->presetBox.setSelectedId(0);
                                     }
                                     else if (result == 2)
                                     {
                                         safeThis->openPresetFolder();
                                     }
                                     else if (result == 3)
                                     {
                                         safeThis->rescanPresetFolder();
                                     }
                                     else if (result == 4)
                                     {
                                         juce::URL(GITHUB_LINK).launchInDefaultBrowser();
                                     }
                                     else if (result == 5)
                                     {
                                         if (! safeThis->manualUpdateCheckThread.isThreadRunning())
                                         {
                                             safeThis->manualUpdateCheckThread.prepareForStart();
                                             safeThis->manualUpdateCheckThread.startThread();
                                         }
                                     }
                                     else if (result == 6)
                                     {
                                         if (safeThis->settingsDialog != nullptr)
                                         {
                                             safeThis->settingsDialog->toFront(true);
                                             return;
                                         }

                                         auto& processor = static_cast<FireAudioProcessor&>(safeThis->procStatePresets.getProcessor());
                                         auto settingsPanel = std::make_unique<SettingsComponent>(processor.getAppSettings());

                                         juce::DialogWindow::LaunchOptions options;
                                         options.content.setOwned(settingsPanel.release());
                                         options.content->setSize(400, 300);
                                         options.dialogTitle = "Settings";
                                         options.dialogBackgroundColour = COLOUR6;
                                         options.escapeKeyTriggersCloseButton = true;
                                         options.useNativeTitleBar = true;
                                         options.resizable = true;
                                         options.componentToCentreAround = safeThis.getComponent();
                                         safeThis->settingsDialog = options.launchAsync();
                                     }
                                 });
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
