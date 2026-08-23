/*
 ==============================================================================
 
 Preset.cpp
 Created: 12 Jul 2020 9:06:49pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#include "Preset.h"
#include "../../PluginProcessor.h"
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace
{
constexpr juce::int64 maximumPresetFileBytes = 4 * 1024 * 1024;
constexpr int maximumPresetFolderDepth = 16;
constexpr int maximumPresetCount = 4096;
constexpr int maximumPresetRoutings = 128;

bool parseStrictDouble(const juce::String& text, double& result) noexcept
{
    const auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(utf8, &end);
    if (end == utf8 || end == nullptr || *end != '\0' || errno == ERANGE || ! std::isfinite(parsed))
        return false;

    result = parsed;
    return true;
}

float readNormalisedAttribute(const juce::XmlElement& xml,
                              const juce::String& name,
                              float fallback) noexcept
{
    double parsed = 0.0;
    if (! parseStrictDouble(xml.getStringAttribute(name), parsed))
        return fallback;

    return juce::jlimit(0.0f, 1.0f, static_cast<float>(parsed));
}

bool readStrictIntegerAttribute(const juce::XmlElement& xml,
                                const juce::String& name,
                                int& result) noexcept
{
    double parsed = 0.0;
    if (! xml.hasAttribute(name)
        || ! parseStrictDouble(xml.getStringAttribute(name), parsed)
        || parsed < static_cast<double>(std::numeric_limits<int>::min())
        || parsed > static_cast<double>(std::numeric_limits<int>::max())
        || std::floor(parsed) != parsed)
        return false;

    result = static_cast<int>(parsed);
    return true;
}

bool isStrictNumberInRange(const juce::XmlElement& xml,
                           const juce::String& name,
                           double minimum,
                           double maximum) noexcept
{
    double parsed = 0.0;
    return xml.hasAttribute(name)
           && parseStrictDouble(xml.getStringAttribute(name), parsed)
           && parsed >= minimum
           && parsed <= maximum;
}

bool isValidLfoState(const juce::XmlElement& lfoState) noexcept
{
    if (! lfoState.hasTagName("LFO_STATE")
        || lfoState.getNumChildElements() != 4)
        return false;

    std::array<bool, 4> seenIndices {};
    for (auto* lfo : lfoState.getChildIterator())
    {
        int index = -1;
        if (! lfo->hasTagName("LFO")
            || ! readStrictIntegerAttribute(*lfo, "index", index)
            || ! juce::isPositiveAndBelow(index, 4)
            || seenIndices[static_cast<size_t>(index)]
            || lfo->getNumChildElements() > 2)
            return false;

        seenIndices[static_cast<size_t>(index)] = true;
        if (lfo->hasAttribute("smoothness")
            && ! isStrictNumberInRange(*lfo, "smoothness", 0.0, 1.0))
            return false;

        const juce::XmlElement* points = nullptr;
        const juce::XmlElement* curvatures = nullptr;
        for (auto* payload : lfo->getChildIterator())
        {
            if (payload->hasTagName("POINTS") && points == nullptr)
                points = payload;
            else if (payload->hasTagName("CURVATURES") && curvatures == nullptr)
                curvatures = payload;
            else
                return false;
        }

        if (points == nullptr || curvatures == nullptr
            || points->getNumChildElements() < 2
            || points->getNumChildElements() > static_cast<int>(LfoData::maximumNumberOfPoints)
            || curvatures->getNumChildElements() != points->getNumChildElements() - 1)
            return false;

        for (auto* point : points->getChildIterator())
            if (! point->hasTagName("P")
                || ! isStrictNumberInRange(*point, "x", 0.0, 1.0)
                || ! isStrictNumberInRange(*point, "y", 0.0, 1.0))
                return false;

        for (auto* curvature : curvatures->getChildIterator())
            if (! curvature->hasTagName("C")
                || ! isStrictNumberInRange(*curvature, "v", -2.0, 2.0))
                return false;
    }

    return true;
}

bool isValidRoutingState(const juce::XmlElement& routingState,
                         const juce::AudioProcessor& processor) noexcept
{
    if (! routingState.hasTagName("MODULATION_STATE")
        || routingState.getNumChildElements() > maximumPresetRoutings)
        return false;

    juce::StringArray seenTargets;
    for (auto* routing : routingState.getChildIterator())
    {
        int source = -1;
        if (! routing->hasTagName("ROUTING")
            || ! readStrictIntegerAttribute(*routing, "source", source)
            || ! juce::isPositiveAndBelow(source, 4)
            || ! isStrictNumberInRange(*routing, "depth", -1.0, 1.0)
            || routing->getNumChildElements() != 0)
            return false;

        const auto target = routing->getStringAttribute("target");
        if (target.isEmpty())
            continue; // Empty preallocated routing slots are part of Init.

        bool isKnownTarget = false;
        for (const auto* parameter : processor.getParameters())
            if (const auto* parameterWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
                parameterWithID != nullptr && parameterWithID->paramID == target)
            {
                isKnownTarget = true;
                break;
            }

        if (! isKnownTarget || seenTargets.contains(target))
            return false;
        seenTargets.add(target);
    }

    return true;
}

bool isValidABSnapshot(const juce::XmlElement& snapshot,
                       const juce::AudioProcessor& processor) noexcept
{
    if (! snapshot.hasTagName("AB_STATE") || snapshot.getNumChildElements() > 2)
        return false;

    int expectedParameterCount = 0;
    for (const auto* parameter : processor.getParameters())
    {
        const auto* parameterWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
        if (parameterWithID == nullptr)
            continue;

        ++expectedParameterCount;
        if (! snapshot.hasAttribute(parameterWithID->paramID))
            return false;

        if (! isStrictNumberInRange(snapshot, parameterWithID->paramID, 0.0, 1.0))
            return false;
    }

    if (expectedParameterCount == 0 || snapshot.getNumChildElements() != 2)
        return false;

    bool hasLfoState = false;
    bool hasRoutingState = false;
    for (auto* child : snapshot.getChildIterator())
    {
        if (child->hasTagName("LFO_STATE") && ! hasLfoState)
        {
            hasLfoState = true;
            if (! isValidLfoState(*child))
                return false;
        }
        else if (child->hasTagName("MODULATION_STATE") && ! hasRoutingState)
        {
            hasRoutingState = true;
            if (! isValidRoutingState(*child, processor))
                return false;
        }
        else
        {
            return false;
        }
    }

    return hasLfoState && hasRoutingState;
}

bool isSupportedPresetDocument(const juce::XmlElement& xml) noexcept
{
    return xml.hasTagName("WINGSFIRE");
}

void writeSerializablePresetSnapshotToXml(
    const FireAudioProcessor& processor,
    const FireAudioProcessor::SerializablePresetStateSnapshot& snapshot,
    juce::XmlElement& xml)
{
    xml.deleteAllChildElements();
    xml.setAttribute("presetFormatVersion", 2);
    xml.setAttribute("pluginVersion", VERSION);

    for (const auto& param : processor.getParameters())
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            float normalisedValue = p->getDefaultValue();
            for (const auto& child : snapshot.parameterState)
            {
                if (child.getProperty("id").toString() != p->paramID
                    || ! child.hasProperty("value"))
                    continue;

                if (auto* ranged = processor.treeState.getParameter(p->paramID))
                {
                    const float plainValue = static_cast<float>(
                        child.getProperty("value"));
                    if (std::isfinite(plainValue))
                    {
                        const auto& range = ranged->getNormalisableRange();
                        normalisedValue = ranged->convertTo0to1(
                            range.snapToLegalValue(juce::jlimit(
                                range.start, range.end, plainValue)));
                    }
                }
                break;
            }
            xml.setAttribute(p->paramID, normalisedValue);
        }

    auto* lfoState = xml.createNewChildElement("LFO_STATE");
    for (int i = 0; i < static_cast<int>(snapshot.lfoData.size()); ++i)
    {
        auto lfoXml = std::make_unique<juce::XmlElement>("LFO");
        lfoXml->setAttribute("index", i);
        snapshot.lfoData[static_cast<size_t>(i)].writeToXml(*lfoXml);
        lfoState->addChildElement(lfoXml.release());
    }

    auto* modMatrixState = xml.createNewChildElement("MODULATION_STATE");
    for (const auto& routing : snapshot.routings)
    {
        if (! routing.targetParameterID.isEmpty())
        {
            auto routingXml = std::make_unique<juce::XmlElement>("ROUTING");
            routing.writeToXml(*routingXml);
            modMatrixState->addChildElement(routingXml.release());
        }
    }
}
} // namespace

namespace state
{
    //==============================================================================
    void saveStateToXml(const juce::AudioProcessor& proc, juce::XmlElement& xml)
    {
        auto& fireProc = static_cast<const FireAudioProcessor&>(proc);
        auto snapshot = fireProc.captureSerializablePresetStateSnapshot();
        writeSerializablePresetSnapshotToXml(fireProc, snapshot, xml);
    }

    void loadStateFromXml(const juce::XmlElement& xml, juce::AudioProcessor& proc)
    {
        auto& fireProc = static_cast<FireAudioProcessor&>(proc);
        {
            fireProc.beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [&fireProc]
            {
                fireProc.requestMultibandTopologyReset();
            } };

            for (const auto& param : proc.getParameters())
            {
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                {
                    float valueToLoad = p->getDefaultValue();
                    if (xml.hasAttribute(p->paramID))
                    {
                        // XmlElement's numeric helpers accept a valid numeric prefix
                        // (for example "0.5oops"). Presets are an external file
                        // format, so only a complete, finite normalised number is
                        // allowed to override the parameter default.
                        valueToLoad = readNormalisedAttribute(xml, p->paramID, p->getDefaultValue());
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
        std::array<bool, 4> loadedLfoIndices {};
        for (int i = 0; i < static_cast<int>(loadedSmoothnessParameter.size()); ++i)
            loadedSmoothnessParameter[static_cast<size_t>(i)] = xml.hasAttribute(
                ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i));

        if (auto* lfoState = xml.getChildByName("LFO_STATE"))
        {
            for (auto* lfoXml : lfoState->getChildIterator())
            {
                if (! lfoXml->hasTagName("LFO"))
                    continue;

                const int index = lfoXml->getIntAttribute("index", -1);
                if (juce::isPositiveAndBelow(index, static_cast<int>(lfoDataToLoad.size()))
                    && ! loadedLfoIndices[static_cast<size_t>(index)])
                {
                    lfoDataToLoad[static_cast<size_t>(index)] = LfoData::readFromXml(*lfoXml);
                    loadedLfoSmoothness[static_cast<size_t>(index)] = lfoXml->hasAttribute("smoothness");
                    loadedLfoIndices[static_cast<size_t>(index)] = true;
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
            {
                const auto parameterID = ParameterIDAndName::getIDString(
                    LFO_SMOOTH_ID, i);
                if (auto* parameter = fireProc.treeState.getParameter(parameterID))
                {
                    const auto& range = parameter->getNormalisableRange();
                    const float smoothness = range.snapToLegalValue(
                        juce::jlimit(range.start,
                                     range.end,
                                     lfoDataToLoad[index].smoothness));
                    lfoDataToLoad[index].smoothness = smoothness;
                    parameter->setValueNotifyingHost(
                        parameter->convertTo0to1(smoothness));
                }
                continue;
            }

            if (const auto* smoothness = fireProc.treeState.getRawParameterValue(
                    ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i)))
                lfoDataToLoad[index].smoothness = smoothness->load(std::memory_order_relaxed);
        }

        juce::Array<ModulationRouting> routingsToLoad;
        if (auto* modMatrixState = xml.getChildByName("MODULATION_STATE"))
        {
            for (auto* routingXml : modMatrixState->getChildIterator())
            {
                if (! routingXml->hasTagName("ROUTING")
                    || routingsToLoad.size() >= maximumPresetRoutings)
                    continue;

                auto routing = ModulationRouting::readFromXml(*routingXml);
                routing.sourceLfoIndex = juce::jlimit(0, 3, routing.sourceLfoIndex);
                routing.depth = std::isfinite(routing.depth) ? juce::jlimit(-1.0f, 1.0f, routing.depth) : 0.5f;

                bool targetAlreadyUsed = false;
                for (const auto& existing : routingsToLoad)
                    if (existing.targetParameterID == routing.targetParameterID)
                    {
                        targetAlreadyUsed = true;
                        break;
                    }

                if (! targetAlreadyUsed
                    && routing.targetParameterID.isNotEmpty()
                    && fireProc.treeState.getParameter(routing.targetParameterID) != nullptr)
                    routingsToLoad.add(std::move(routing));
            }
        }

            fireProc.getLfoManager().replaceLfoDataAndRoutings(
                lfoDataToLoad, std::move(routingsToLoad));

            // A preset/A-B swap may replace every logical band while retaining
            // the same NUM_BANDS value. The scope guard publishes the complete
            // migration, including when a foreign listener throws.
        }
        fireProc.sendChangeMessage();
    }

    //==============================================================================
    StateAB::StateAB(juce::AudioProcessor& p)
        : pluginProcessor { p }
    {
        copyAB(false);
    }

    void StateAB::toggleAB()
    {
        {
            const juce::ScopedLock lock(stateLock);
            juce::XmlElement temp { "Temp" };
            saveStateToXml(pluginProcessor, temp); // current to temp
            loadStateFromXml(ab, pluginProcessor); // ab to current
            ab = temp; // temp to ab
            currentSideIsA.store(! currentSideIsA.load(std::memory_order_relaxed), std::memory_order_release);
        }

        pluginProcessor.updateHostDisplay(
            juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
    }

    void StateAB::copyAB(bool notifyHost)
    {
        {
            const juce::ScopedLock lock(stateLock);
            ab.removeAllAttributes();
            ab.deleteAllChildElements();
            saveStateToXml(pluginProcessor, ab);
        }

        if (notifyHost)
        {
            pluginProcessor.updateHostDisplay(
                juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
        }
    }

    void StateAB::reset()
    {
        pluginProcessor.reset();
    }

    void StateAB::writeToXml(juce::XmlElement& parent) const
    {
        const juce::ScopedLock lock(stateLock);
        parent.deleteAllChildElementsWithTagName("AB_STATE");
        auto snapshot = std::make_unique<juce::XmlElement>(ab);
        snapshot->setTagName("AB_STATE");
        snapshot->setAttribute("currentSideIsA", currentSideIsA.load(std::memory_order_relaxed));
        parent.addChildElement(snapshot.release());
    }

    void StateAB::readFromXml(const juce::XmlElement* state)
    {
        const juce::ScopedLock lock(stateLock);
        if (state == nullptr || ! isValidABSnapshot(*state, pluginProcessor))
        {
            // Old host states never persisted A/B, and a damaged alternate
            // must never turn the next toggle into an implicit Init. Preserve
            // the historical default: A is current and B mirrors the live
            // state that has just been restored.
            ab.removeAllAttributes();
            ab.deleteAllChildElements();
            auto& fireProc = static_cast<const FireAudioProcessor&>(
                pluginProcessor);
            auto fallback = fireProc
                                .captureCurrentSerializablePresetStateSnapshotForABFallback();
            writeSerializablePresetSnapshotToXml(fireProc, fallback, ab);
            currentSideIsA.store(true, std::memory_order_release);
            return;
        }

        ab = *state;
        ab.setTagName("AB");
        ab.removeAttribute("currentSideIsA");
        currentSideIsA.store(state->getBoolAttribute("currentSideIsA", true), std::memory_order_release);
    }

    //==============================================================================

    bool parseFileToXmlElement(const juce::File& file, juce::XmlElement& xml)
    {
        if (! file.existsAsFile()
            || file.getSize() <= 0
            || file.getSize() > maximumPresetFileBytes)
            return false;

        auto parsed = juce::XmlDocument::parse(file);
        if (parsed == nullptr || ! isSupportedPresetDocument(*parsed))
            return false;

        xml = *parsed;
        return true;
    }

    bool writeXmlElementToFile(const juce::XmlElement& xml,
                               juce::File& file,
                               const juce::String& presetName,
                               bool confirmOverwrite)
    {
        if (! file.exists())
            return xml.writeTo(file);

        if (file.isDirectory())
            return false;

        if (confirmOverwrite)
        {
            const bool choice = juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                                        "Replace preset?",
                                                                        "\"" + presetName + PRESET_EXETENSION
                                                                            + "\" already exists. Replacing it will overwrite its current contents.",
                                                                        nullptr,
                                                                        nullptr);
            if (! choice)
                return false;
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

    juce::String StatePresets::normalisePresetKey(juce::String key)
    {
        key = key.trim().replaceCharacter('\\', '/');
        while (key.startsWith("./"))
            key = key.substring(2);
        while (key.contains("//"))
            key = key.replace("//", "/");

        // A key is always relative to the configured preset root. Reject
        // traversal and absolute paths rather than allowing identity to escape
        // that root when it arrives from a host state.
        if (key.isEmpty() || juce::File::isAbsolutePath(key))
            return {};

        const auto segments = juce::StringArray::fromTokens(key, "/", {});
        for (const auto& segment : segments)
            if (segment.isEmpty() || segment == "." || segment == "..")
                return {};

        return key;
    }

    void StatePresets::recursiveFileSearch(juce::XmlElement& parentXML,
                                           const juce::File& dir,
                                           int depth)
    {
        if (depth > maximumPresetFolderDepth || numPresets >= maximumPresetCount)
            return;

        juce::RangedDirectoryIterator iterator(dir,
                                                false,
                                                "*",
                                                juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles,
                                                juce::File::FollowSymlinks::no);
        for (auto file : iterator)
        {
            if (numPresets >= maximumPresetCount)
                break;

            if (file.isDirectory())
            {
                auto currentState = std::make_unique<juce::XmlElement>("FOLDER");
                currentState->setAttribute("folderName", file.getFile().getFileName());
                recursiveFileSearch(*currentState, file.getFile(), depth + 1);
                if (currentState->getNumChildElements() > 0)
                    parentXML.addChildElement(currentState.release());
            }
            else if (file.getFile().hasFileExtension(PRESET_EXETENSION))
            {
                auto currentState = std::make_unique<juce::XmlElement>("PRESET");
                if (! parseFileToXmlElement(file.getFile(), *currentState))
                    continue;

                bool hasKnownParameter = false;
                for (const auto* parameter : pluginProcessor.getParameters())
                    if (const auto* parameterWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
                        parameterWithID != nullptr && currentState->hasAttribute(parameterWithID->paramID))
                    {
                        hasKnownParameter = true;
                        break;
                    }

                // A bare <WINGSFIRE/> document is syntactically valid XML but
                // not a preset. Exposing it would create a menu action that
                // silently resets every parameter to its default.
                if (! hasKnownParameter)
                    continue;

                const auto presetKey = normalisePresetKey(file.getFile().getRelativePathFrom(presetFile));
                if (presetKey.isEmpty())
                    continue;

                ++numPresets;
                const juce::String newPresetId = getNextAvailablePresetId();
                currentState->setTagName(newPresetId);

                const juce::String newName = file.getFile().getFileNameWithoutExtension();
                if (newName != currentState->getStringAttribute("presetName"))
                    currentState->setAttribute("presetName", newName);
                currentState->setAttribute("presetKey", presetKey);

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
            if (child->hasTagName("FOLDER"))
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

        recursiveFileSearch(mPresetXml, presetFile, 0);

        recursiveSort(&mPresetXml);

        //mPresetXml.writeTo(File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Audio/Presets/Wings/Fire/test.xml"));
    }

    juce::String StatePresets::savePreset(juce::File savePath, bool overwriteAlreadyConfirmed)
    {
        if (savePath == juce::File())
            return juce::String();

        juce::File userPresetFile = savePath.hasFileExtension(PRESET_EXETENSION)
                                        ? savePath
                                        : juce::File(savePath.getFullPathName() + PRESET_EXETENSION);
        const auto presetName = userPresetFile.getFileNameWithoutExtension();
        if (presetName.trim().isEmpty())
            return juce::String();

        const auto parentDirectory = userPresetFile.getParentDirectory();
        if ((! parentDirectory.isDirectory() && parentDirectory.createDirectory().failed())
            || ! parentDirectory.isDirectory())
            return juce::String();

        // Save the single preset to a real file.
        presetXmlSingle.removeAllAttributes(); // Clear all first.
        presetXmlSingle.deleteAllChildElements();
        presetXmlSingle.setAttribute("presetName", presetName); // Set preset name.
        presetXmlSingle.setAttribute("presetFormatVersion", 2);
        presetXmlSingle.setAttribute("pluginVersion", VERSION);
        saveStateToXml(pluginProcessor, presetXmlSingle);

        const bool isSaved = writeXmlElementToFile(presetXmlSingle,
                                                   userPresetFile,
                                                   presetName,
                                                   ! overwriteAlreadyConfirmed);

        if (isSaved)
        {
            const auto savedPresetKey = normalisePresetKey(userPresetFile.getRelativePathFrom(presetFile));
            {
                const juce::ScopedLock lock(identityLock);
                currentPresetKey = savedPresetKey;
                if (savedPresetKey.isNotEmpty())
                {
                    statePresetName = presetName;
                }
                else
                {
                    // Exporting outside the configured preset root is valid,
                    // but that file cannot be represented by a stable library
                    // key and must not accidentally select a same-named preset.
                    statePresetName.clear();
                    mCurrentPresetId.store(0, std::memory_order_relaxed);
                }
            }
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
                {
                    auto& fireProc = static_cast<FireAudioProcessor&>(
                        pluginProcessor);
                    fireProc.beginMultibandTopologyEdit();
                    const juce::ScopeGuard finishTopologyEdit { [&fireProc]
                    {
                        fireProc.requestMultibandTopologyReset();
                    } };

                    loadStateFromXml(*child, pluginProcessor);
                    {
                        const juce::ScopedLock lock(identityLock);
                        statePresetName = child->getStringAttribute("presetName");
                        currentPresetKey = normalisePresetKey(
                            child->getStringAttribute("presetKey"));
                    }
                }
                return true;
            }

            if (child->hasTagName("FOLDER") && recursivePresetLoad(*child, presetId))
                return true;
        }

        return false;
    }

    bool StatePresets::loadPreset(const juce::String& presetId,
                                  bool notifyHost)
    {
        const bool presetWasLoaded = recursivePresetLoad(mPresetXml, presetId);
        if (presetWasLoaded && notifyHost)
        {
            // recursivePresetLoad commits its topology transaction before
            // telling the host to request a new state snapshot.
            pluginProcessor.updateHostDisplay(
                juce::AudioProcessorListener::ChangeDetails {}
                    .withNonParameterStateChanged(true));
        }

        return presetWasLoaded;
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
            const juce::ScopedLock lock(identityLock);
            mCurrentPresetId.store(0, std::memory_order_relaxed);
            statePresetName.clear();
            currentPresetKey.clear();
        }
    }

    void StatePresets::setPresetName(juce::String name)
    {
        const juce::ScopedLock lock(identityLock);
        statePresetName = std::move(name);
    }

    juce::String StatePresets::getPresetName() const
    {
        const juce::ScopedLock lock(identityLock);
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
                const auto childKey = normalisePresetKey(child->getStringAttribute("presetKey"));
                comboBoxIdToPresetKeyMap.set(index, childKey);

                // Stable relative-path identity wins. Name matching is retained
                // only for legacy in-memory state created before preset keys.
                if ((currentPresetKey.isNotEmpty() && currentPresetKey == childKey)
                    || (currentPresetKey.isEmpty()
                        && mCurrentPresetId.load(std::memory_order_relaxed) == 0
                        && statePresetName == n))
                {
                    mCurrentPresetId.store(index);
                    currentPresetKey = childKey;
                    statePresetName = n;
                }
            }
            else if (child->hasTagName("FOLDER"))
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
        const juce::ScopedLock lock(identityLock);
        const int pendingLegacyId = mCurrentPresetId.load(std::memory_order_relaxed);
        const bool hasStableIdentity = currentPresetKey.isNotEmpty() || statePresetName.isNotEmpty();
        comboBoxIdToTagNameMap.clear();
        comboBoxIdToPresetKeyMap.clear();
        if (hasStableIdentity)
            mCurrentPresetId.store(0, std::memory_order_relaxed);
        int index = 0;
        recursivePresetNameAdd(mPresetXml, menu, index);

        // Host states created before preset keys only contain the sorted menu
        // ID. Keep that best-effort fallback until the first menu exists, then
        // bind it to the discovered stable key for future saves.
        if (! hasStableIdentity)
        {
            const int safeLegacyId = juce::isPositiveAndBelow(pendingLegacyId, index + 1)
                                         ? pendingLegacyId
                                         : 0;
            mCurrentPresetId.store(safeLegacyId, std::memory_order_relaxed);
            currentPresetKey = normalisePresetKey(comboBoxIdToPresetKeyMap[safeLegacyId]);
            if (safeLegacyId > 0)
                statePresetName = menu.getItemText(menu.indexOfItemId(safeLegacyId));
        }
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

    StatePresets::PresetIdentitySnapshot
    StatePresets::getCurrentPresetIdentity() const
    {
        const juce::ScopedLock lock(identityLock);
        return { mCurrentPresetId.load(std::memory_order_relaxed),
                 currentPresetKey };
    }

    void StatePresets::setCurrentPresetId(int currentPresetId)
    {
        const juce::ScopedLock lock(identityLock);
        const int safeId = juce::jmax(0, currentPresetId);
        mCurrentPresetId.store(safeId);
        currentPresetKey = normalisePresetKey(comboBoxIdToPresetKeyMap[safeId]);
        if (safeId == 0 || currentPresetKey.isEmpty())
            statePresetName.clear();
    }

    juce::String StatePresets::getCurrentPresetKey() const
    {
        const juce::ScopedLock lock(identityLock);
        return currentPresetKey;
    }

    void StatePresets::setCurrentPresetKey(juce::String key)
    {
        const juce::ScopedLock lock(identityLock);
        currentPresetKey = normalisePresetKey(std::move(key));
        statePresetName.clear();
        mCurrentPresetId.store(0, std::memory_order_relaxed);
    }

    juce::File StatePresets::getFile()
    {
        return presetFile;
    }

    const juce::XmlElement& StatePresets::getPresetXml() const
    {
        return mPresetXml;
    }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    void StatePresets::setPresetDirectoryForTesting(juce::File directory)
    {
        presetFile = std::move(directory);
        scanAllPresets();
    }
#endif

    void StatePresets::initPreset()
    {
        auto& fireProc = static_cast<FireAudioProcessor&>(pluginProcessor);
        {
            fireProc.beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [&fireProc]
            {
                fireProc.requestMultibandTopologyReset();
            } };
            for (const auto& param : pluginProcessor.getParameters())
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                    // if not in xml set current
                    p->setValueNotifyingHost(p->getDefaultValue());
            // set preset combobox to 0
            {
                const juce::ScopedLock lock(identityLock);
                statePresetName.clear();
                currentPresetKey.clear();
                mCurrentPresetId.store(0);
            }

            fireProc.getLfoManager().replaceLfoDataAndRoutings(
                std::array<LfoData, 4> {}, {});
        }
        fireProc.sendChangeMessage();
        pluginProcessor.updateHostDisplay(
            juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
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

        synchronisePresetSelectionFromManager();

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
        toggleABButton.setButtonText(procStateAB.isCurrentA() ? "A" : "B");
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

        if (programmaticChangeDepth.load(std::memory_order_acquire) > 0)
            return;

        dirtyUpdatePending.store(true, std::memory_order_release);
    }

    void StateComponent::timerCallback()
    {
        if (dirtyUpdatePending.exchange(false, std::memory_order_acq_rel))
            markAsDirty();
    }

    void StateComponent::beginProgrammaticChange()
    {
        dirtyUpdatePending.store(false, std::memory_order_release);
        programmaticChangeDepth.fetch_add(1, std::memory_order_acq_rel);
    }

    void StateComponent::endProgrammaticChange()
    {
        dirtyUpdatePending.store(false, std::memory_order_release);
        const int previousDepth = programmaticChangeDepth.fetch_sub(1, std::memory_order_acq_rel);
        jassert(previousDepth > 0);
        if (previousDepth <= 0)
            programmaticChangeDepth.store(0, std::memory_order_release);
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
                beginProgrammaticChange();
                const juce::ScopeGuard finishProgrammaticLoad { [this]
                {
                    endProgrammaticChange();
                } };
                bool presetWasLoaded = false;
                {
                    auto& fireProc = static_cast<FireAudioProcessor&>(
                        presetManager->getProcessor());
                    fireProc.beginMultibandTopologyEdit();
                    const juce::ScopeGuard finishTopologyEdit { [&fireProc]
                    {
                        fireProc.requestMultibandTopologyReset();
                    } };

                    // Identity is part of the same serializable generation as
                    // the preset's parameters, shapes and routings.
                    presetManager->setCurrentPresetId(selectedId);
                    presetWasLoaded = presetManager->loadPreset(
                        internalIdToLoad, false);
                    if (! presetWasLoaded)
                        presetManager->setCurrentPresetId(0);
                }

                if (! presetWasLoaded)
                    return;

                // The UI owns an outer transaction which also includes the
                // selected preset ID. Notify only after that transaction has
                // published the complete sound and identity generation.
                presetManager->getProcessor().updateHostDisplay(
                    juce::AudioProcessorListener::ChangeDetails {}
                        .withNonParameterStateChanged(true));

                const juce::String loadedPresetName = presetBox.getItemText(presetBox.indexOfItemId(selectedId));

                presetBox.setText(loadedPresetName, juce::dontSendNotification);

                presetBox.setSelectedId(selectedId, juce::dontSendNotification);
            }
            else
            {
                presetManager->setCurrentPresetId(0);
                jassertfalse;
            }
        }
    }

    void StateComponent::refreshPresetBox() // rescan, init, save, or delete
    {
        presetBox.clear(juce::dontSendNotification);
        procStatePresets.setPresetAndFolderNames(presetBox);
    }

    void StateComponent::synchronisePresetSelectionFromManager()
    {
        // A processor state restore changes the live state first and then
        // publishes its stable preset key. Rebuild the menu mapping and only
        // reflect that state in the ComboBox; this path must never load a file.
        dirtyUpdatePending.store(false, std::memory_order_release);
        refreshPresetBox();

        const auto currentPresetKey = procStatePresets.getCurrentPresetKey();
        const int currentPresetId = procStatePresets.getCurrentPresetId();
        if (currentPresetKey.isEmpty()
            || ! juce::isPositiveAndBelow(currentPresetId, procStatePresets.getNumPresets() + 1))
        {
            presetBox.setSelectedId(0, juce::dontSendNotification);
            presetBox.setText({}, juce::dontSendNotification);
            return;
        }

        std::function<const juce::XmlElement*(const juce::XmlElement&)> findPresetByKey =
            [&](const juce::XmlElement& parentXml) -> const juce::XmlElement*
        {
            for (auto* child : parentXml.getChildIterator())
            {
                if (child->hasAttribute("presetName")
                    && child->getStringAttribute("presetKey") == currentPresetKey)
                    return child;

                if (child->hasTagName("FOLDER"))
                    if (auto* found = findPresetByKey(*child))
                        return found;
            }

            return nullptr;
        };

        const auto* presetXml = findPresetByKey(procStatePresets.getPresetXml());
        if (presetXml == nullptr)
        {
            presetBox.setSelectedId(0, juce::dontSendNotification);
            presetBox.setText({}, juce::dontSendNotification);
            return;
        }

        const int itemIndex = presetBox.indexOfItemId(currentPresetId);
        const auto presetName = itemIndex >= 0
                                    ? presetBox.getItemText(itemIndex)
                                    : presetXml->getStringAttribute("presetName");
        presetBox.setSelectedId(currentPresetId, juce::dontSendNotification);
        presetBox.setText(presetName, juce::dontSendNotification);

        auto& fireProc = static_cast<FireAudioProcessor&>(procStatePresets.getProcessor());
        if (! fireProc.isCurrentStateEquivalentToPreset(*presetXml))
        {
            isChanged = true;
            markAsDirty();
        }

        // Discard parameter callbacks already queued by the host restore so a
        // just-reconciled clean preset is not dirtied on the next timer tick.
        dirtyUpdatePending.store(false, std::memory_order_release);
    }

    void StateComponent::synchroniseABButtonFromManager()
    {
        toggleABButton.setButtonText(procStateAB.isCurrentA() ? "A" : "B");
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
        if (fileChooser != nullptr)
            return;

        juce::File userFile = procStatePresets.getFile().getChildFile("User");
        creatFolderIfNotExist(userFile);
        if (! userFile.isDirectory())
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                        "Preset save failed",
                                                        "The preset folder could not be created.",
                                                        this);
            return;
        }

        fileChooser = std::make_unique<juce::FileChooser>("Save preset", userFile, "*.fire", true, false, this);
        const auto folderChooserFlags = juce::FileBrowserComponent::saveMode
                                        | juce::FileBrowserComponent::canSelectFiles;
        juce::Component::SafePointer<StateComponent> safeThis(this);

        fileChooser->launchAsync(folderChooserFlags, [safeThis](const juce::FileChooser& chooser)
                                 {
            if (safeThis == nullptr)
                return;

            const juce::File inputName = chooser.getResult();
            if (inputName == juce::File())
            {
                juce::MessageManager::callAsync([safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->fileChooser.reset();
                });
                return;
            }

            // Confirm against the final path after appending .fire. Native
            // choosers do not consistently apply the extension before their
            // overwrite check, which could otherwise silently replace an
            // existing preset when the user omitted the suffix.
            const auto finalPath = inputName.hasFileExtension(PRESET_EXETENSION)
                                       ? inputName
                                       : juce::File(inputName.getFullPathName() + PRESET_EXETENSION);
            const bool mayOverwrite = ! finalPath.existsAsFile()
                                      || juce::NativeMessageBox::showOkCancelBox(
                                          juce::AlertWindow::WarningIcon,
                                          "Replace preset?",
                                          "\"" + finalPath.getFileName()
                                              + "\" already exists. Replacing it will overwrite its current contents.",
                                          safeThis.getComponent(),
                                          nullptr);

            // A synchronous native dialog runs a nested event loop; the host
            // may close the editor while it is open.
            if (safeThis == nullptr)
                return;

            if (! mayOverwrite)
            {
                juce::MessageManager::callAsync([safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->fileChooser.reset();
                });
                return;
            }

            const juce::String savedPresetName = safeThis->procStatePresets.savePreset(finalPath, true);

            if (savedPresetName.isNotEmpty())
            {
                safeThis->dirtyUpdatePending.store(false, std::memory_order_release);
                safeThis->refreshPresetBox();

                const int newPresetIdToSelect = safeThis->procStatePresets.getCurrentPresetId();
                if (newPresetIdToSelect > 0)
                {
                    safeThis->presetBox.setSelectedId(newPresetIdToSelect, juce::dontSendNotification);
                    safeThis->presetBox.setText(savedPresetName, juce::dontSendNotification);
                }
            }
            else
            {
                juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                            "Preset save failed",
                                                            "The preset could not be written to the selected location.",
                                                            safeThis.getComponent());
            }

            // FileChooser invokes this callback from its own finished() stack.
            // Defer destruction until that stack has unwound.
            juce::MessageManager::callAsync([safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->fileChooser.reset();
            }); });
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
        const auto previouslySelectedKey = procStatePresets.getCurrentPresetKey();
        const auto previousText = presetBox.getText();
        const bool wasDirty = previousText.endsWith("*");

        procStatePresets.scanAllPresets();
        procStatePresets.setCurrentPresetKey(previouslySelectedKey);
        refreshPresetBox();

        const int newPresetIdToSelect = procStatePresets.getCurrentPresetId();
        if (newPresetIdToSelect > 0)
        {
            presetBox.setSelectedId(newPresetIdToSelect, juce::dontSendNotification);
            if (wasDirty)
                markAsDirty();
        }
        else if (wasDirty)
        {
            // Keep an honest dirty display even if the backing file was
            // removed externally during the rescan.
            presetBox.setSelectedId(0, juce::dontSendNotification);
            presetBox.setText(previousText, juce::dontSendNotification);
        }
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
                                         safeThis->beginProgrammaticChange();
                                         const juce::ScopeGuard finishInit { [component = safeThis.getComponent()]
                                         {
                                             if (component != nullptr)
                                                 component->endProgrammaticChange();
                                         } };
                                         safeThis->procStatePresets.initPreset();
                                         safeThis->resetMultiband();
                                         safeThis->presetBox.setSelectedId(0, juce::dontSendNotification);
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
