# The pinned CLAP adapter sends getTailLengthSeconds() through JUCE's fast
# integer rounding routine. Infinity becomes zero there, cutting frozen Clouds
# voices. Patch a build-directory copy, keeping the upstream submodule intact.
get_target_property(FireClapSourceRoot clap_juce_sources CLAP_JUCE_SOURCE_DIR)
set(FireClapOriginalCpp "${FireClapSourceRoot}/src/wrapper/clap-juce-wrapper.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FireClapOriginalCpp}")
file(READ "${FireClapOriginalCpp}" FireClapWrapper)
string(REPLACE "\r\n" "\n" FireClapWrapper "${FireClapWrapper}")
# JUCE 8.0.11 moved the legacy parameter adapter into the headless module;
# current releases expose its inline implementation as a header.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/modules/juce_audio_processors_headless/format_types/juce_LegacyAudioParameter.h")
    string(REPLACE "#include <juce_audio_processors/format_types/juce_LegacyAudioParameter.cpp>"
        "#include <juce_audio_processors_headless/format_types/juce_LegacyAudioParameter.h>"
        FireClapWrapper "${FireClapWrapper}")
endif()
string(REPLACE "juceTrackInfo.colour = clapColourToJUCEColour(clapTrackInfo.color);"
    "#if JUCE_VERSION >= 0x080009\n                juceTrackInfo.colourARGB = clapColourToJUCEColour(clapTrackInfo.color).getARGB();\n#else\n                juceTrackInfo.colour = clapColourToJUCEColour(clapTrackInfo.color);\n#endif"
    FireClapWrapper "${FireClapWrapper}")
string(REPLACE "juce::detail::PluginUtilities::getDesktopFlags(editorWrapper->editor.get());"
    "#if JUCE_VERSION >= 0x090000\n            juce::detail::PluginUtilities::getDesktopFlagsAndWindowsMultiTouchMode(editorWrapper->editor.get()).desktopFlags;\n#else\n            juce::detail::PluginUtilities::getDesktopFlags(editorWrapper->editor.get());\n#endif"
    FireClapWrapper "${FireClapWrapper}")
set(FireClapOldReturn [=[return uint32_t(
            juce::roundToIntAccurate((double)sampleRate() * processor->getTailLengthSeconds()));]=])
string(FIND "${FireClapWrapper}" "${FireClapOldReturn}" FireClapTailPosition)
if(FireClapTailPosition EQUAL -1)
    message(FATAL_ERROR "CLAP wrapper tail code changed; review the Fire infinite-tail adaptation.")
endif()
string(REPLACE "${FireClapOldReturn}" "" FireClapWithoutTail "${FireClapWrapper}")
string(LENGTH "${FireClapWrapper}" FireClapOriginalLength)
string(LENGTH "${FireClapWithoutTail}" FireClapStrippedLength)
string(LENGTH "${FireClapOldReturn}" FireClapExpectedDifference)
math(EXPR FireClapActualDifference "${FireClapOriginalLength} - ${FireClapStrippedLength}")
if(NOT FireClapActualDifference EQUAL FireClapExpectedDifference)
    message(FATAL_ERROR "Expected exactly one CLAP tail conversion to adapt.")
endif()
string(REPLACE "${FireClapOldReturn}"
    "return fire::utility::tailSecondsToClapSamples(processor->getTailLengthSeconds(), static_cast<double>(sampleRate()));"
    FireClapWrapper "${FireClapWrapper}")

# tail.changed is an audio-thread callback. Notify after processing automation,
# so a host caching the tail notices both Freeze and its release during playback.
set(FireClapTailMember "    juce::MidiBuffer midiBuffer;")
set(FireClapTailNotifyPoint [=[        // process any leftover events
        while (currentEvent < numEvents)
            processEvent(numSamples);

        return CLAP_PROCESS_CONTINUE;]=])
string(FIND "${FireClapWrapper}" "${FireClapTailMember}" FireClapMemberPosition)
string(FIND "${FireClapWrapper}" "${FireClapTailNotifyPoint}" FireClapNotifyPosition)
if(FireClapMemberPosition EQUAL -1 OR FireClapNotifyPosition EQUAL -1)
    message(FATAL_ERROR "CLAP processing code changed; review the Fire tail notification adaptation.")
endif()
string(REPLACE "${FireClapTailMember}"
    "${FireClapTailMember}\n    uint32_t fireLastNotifiedTail = 0;"
    FireClapWrapper "${FireClapWrapper}")
set(FireClapTailNotification [=[        // process any leftover events
        while (currentEvent < numEvents)
            processEvent(numSamples);

        if (_host.canUseTail())
        {
            const auto currentTail = tailGet();
            if (currentTail != fireLastNotifiedTail)
            {
                fireLastNotifiedTail = currentTail;
                _host.tailChanged();
            }
        }
        return CLAP_PROCESS_CONTINUE;]=])
string(REPLACE "${FireClapTailNotifyPoint}" "${FireClapTailNotification}"
    FireClapWrapper "${FireClapWrapper}")
set(FireClapGeneratedDir "${CMAKE_CURRENT_BINARY_DIR}/generated/fire-clap")
file(CONFIGURE OUTPUT "${FireClapGeneratedDir}/clap-juce-wrapper.cpp"
    CONTENT "#include \"Utility/ClapTail.h\"\n${FireClapWrapper}" @ONLY)
if(APPLE)
    set(FireClapWrapperName clap-juce-mac.mm)
    configure_file("${FireClapSourceRoot}/src/wrapper/${FireClapWrapperName}"
                   "${FireClapGeneratedDir}/${FireClapWrapperName}" COPYONLY)
else()
    set(FireClapWrapperName clap-juce-wrapper.cpp)
endif()
get_target_property(FireClapWrapperSources clap_juce_sources INTERFACE_SOURCES)
string(REPLACE "${FireClapSourceRoot}/src/wrapper/${FireClapWrapperName}"
               "${FireClapGeneratedDir}/${FireClapWrapperName}"
               FireClapWrapperSources "${FireClapWrapperSources}")
set_property(TARGET clap_juce_sources PROPERTY INTERFACE_SOURCES "${FireClapWrapperSources}")
target_include_directories(clap_juce_sources INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
