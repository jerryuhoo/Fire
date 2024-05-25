#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
namespace melatonin
{
    // We have to avoid using ApplicationProperties in a SharedResourcePointer
    // as we don't want to screw up the app's properties file
    struct InspectorSettings
    {
        InspectorSettings()
        {
            juce::PropertiesFile::Options opts;

            opts.applicationName = "melatonin_inspector";
            opts.filenameSuffix = ".xml";
            opts.folderName = "sudara";
            opts.osxLibrarySubFolder = "Application Support";
            opts.commonToAllUsers = false;
            opts.ignoreCaseOfKeyNames = false;
            opts.doNotSave = false;
            opts.millisecondsBeforeSaving = 1;
            opts.storageFormat = juce::PropertiesFile::storeAsXML;
            props = std::make_unique<juce::PropertiesFile> (opts);
        }

        void saveIfNeeded() const
        {
            props->saveIfNeeded();
        }

        // this is a unique_ptr because our object must be default constructable
        std::unique_ptr<juce::PropertiesFile> props;
    };
}
