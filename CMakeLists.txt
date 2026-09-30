cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)

# Modern concise way to add dependencies to your project
include (CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Change me!
# This is the internal name of the project and the name of JUCE's shared code target
# Note: This cannot have spaces (it may be 2024, but you can't have it all!)
# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs display)
set(PROJECT_NAME "Fire")

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
# You can also just have it be the same thing as PROJECT_NAME
# You may want to append the major version on the end of this (and PROJECT_NAME) ala:
#   set(PROJECT_NAME "MyPlugin_v${MAJOR_VERSION}")
# Doing so enables major versions to show up in IDEs and DAWs as separate plugins
# allowing you to change parameters and behavior without breaking existing user projects
set(PRODUCT_NAME "Fire")

# Change me! Used for the MacOS bundle name and Installers
set(COMPANY_NAME "Blue Wings Music")

# Change me! Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.bluewingsmusic.Fire")

# Change me! Set the plugin formats you want built
# Valid choices: AAX Unity VST VST3 AU AUv3 Standalone
set(FORMATS AU VST3)

# For simplicity, the name of the CMake project is also the name of the target
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" PROJECT_VERSION_NUMERIC "${CURRENT_VERSION}")
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION_NUMERIC})

# JUCE is setup as a submodule in the /JUCE folder
# Locally, you must run `git submodule update --init --recursive` once
# The recorded submodule revision pins the tested JUCE release (9.0.2).
# On Github Actions, this is done as a part of actions/checkout
add_subdirectory(JUCE)

# Add CLAP format
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-overrides/ClapTail.cmake")

# Add any other modules you want modules here, before the juce_add_plugin call
# juce_add_module(modules/my_module)

# This adds the melatonin inspector module
add_subdirectory (modules/melatonin_inspector)

# See `docs/CMake API.md` in the JUCE repo for all config options
set(FIRE_INSTALL_PLUGINS_DEFAULT ON)
if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES)
    set(FIRE_INSTALL_PLUGINS_DEFAULT OFF)
endif ()
option(FIRE_INSTALL_PLUGINS "Copy compiled plug-ins to the user's plug-in folders" ${FIRE_INSTALL_PLUGINS_DEFAULT})
juce_add_plugin("${PROJECT_NAME}"
    # Icons for the standalone app
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"

    # Change me!
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"

    # On MacOS, plugin is copied to /Users/yourname/Library/Audio/Plug-Ins/
    COPY_PLUGIN_AFTER_BUILD ${FIRE_INSTALL_PLUGINS}

    # Change me!
    # A four-character plugin id
    # First character MUST be uppercase for AU formats
    PLUGIN_MANUFACTURER_CODE Wing

    # Change me!
    # A unique four-character plugin id
    # Note: this must have at least one upper-case character
    PLUGIN_CODE Fire
    FORMATS "${FORMATS}"

    # The name of your final executable
    # This is how it's listed in the DAW
    # This can be different from PROJECT_NAME and can have spaces!
    # You might want to use v${MAJOR_VERSION} here once you go to v2...
    PRODUCT_NAME "${PRODUCT_NAME}")

# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(SharedCode INTERFACE)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # The upstream CLAP helper copies the bundle before a complete macOS
    # resource signature is applied. Suppress only that CLAP copy; JUCE's
    # already-configured AU/VST3 copy steps retain their original setting.
    get_target_property(FIRE_COPY_PLUGIN_AFTER_BUILD
        "${PROJECT_NAME}" JUCE_COPY_PLUGIN_AFTER_BUILD)
    set_property(TARGET "${PROJECT_NAME}"
        PROPERTY JUCE_COPY_PLUGIN_AFTER_BUILD FALSE)
endif ()

clap_juce_extensions_plugin(TARGET "${PROJECT_NAME}"
    CLAP_ID "${BUNDLE_ID}"
    CLAP_FEATURES audio-effect)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_property(TARGET "${PROJECT_NAME}"
        PROPERTY JUCE_COPY_PLUGIN_AFTER_BUILD
        "${FIRE_COPY_PLUGIN_AFTER_BUILD}")

    # Sign complete bundles even when automatic install is disabled. The
    # linker's binary-only signature does not seal AU/CLAP resources, and JUCE
    # generates VST3 moduleinfo.json after its initial signing step. Seal the
    # final manifest here so a version change cannot invalidate the VST3 bundle.
    foreach (FireMacFormat IN ITEMS AU VST3 CLAP)
        if (TARGET "${PROJECT_NAME}_${FireMacFormat}")
            add_custom_command(TARGET "${PROJECT_NAME}_${FireMacFormat}" POST_BUILD
                COMMAND /usr/bin/codesign --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}_${FireMacFormat}>"
                COMMAND /usr/bin/codesign --verify --deep --strict
                    "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}_${FireMacFormat}>"
                COMMENT "Signing ${PROJECT_NAME}_${FireMacFormat}"
                VERBATIM)
        endif ()
    endforeach ()
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND FIRE_INSTALL_PLUGINS)
    if ("$ENV{HOME}" STREQUAL "")
        message(FATAL_ERROR "HOME must be set before installing the CLAP bundle")
    endif ()

    set(FIRE_CLAP_INSTALL_DIRECTORY
        "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
    set(FIRE_CLAP_INSTALL_BUNDLE
        "${FIRE_CLAP_INSTALL_DIRECTORY}/${PRODUCT_NAME}.clap")
    add_custom_command(TARGET "${PROJECT_NAME}_CLAP" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${FIRE_CLAP_INSTALL_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
            "${FIRE_CLAP_INSTALL_BUNDLE}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}_CLAP>"
            "${FIRE_CLAP_INSTALL_BUNDLE}"
        COMMAND /usr/bin/codesign --verify --deep --strict
            "${FIRE_CLAP_INSTALL_BUNDLE}"
        COMMENT "Signing and installing ${PRODUCT_NAME}.clap"
        VERBATIM)
endif ()

# Configure shared compiler and language defaults.
include(SharedCodeDefaults)

# The shared defaults module enables fast-math, but Fire deliberately validates
# external state and audio samples for NaN/Inf. Fast-math permits the compiler to
# assume those values never occur and remove the guards entirely. Restore IEEE
# floating-point semantics after the shared defaults while retaining the normal
# optimisations supplied by the Release configuration.
if (MSVC)
    target_compile_options(SharedCode INTERFACE
        $<$<CONFIG:Release>:/O2 /fp:precise>
        $<$<CONFIG:RelWithDebInfo>:/O2 /fp:precise>)
else ()
    target_compile_options(SharedCode INTERFACE
        $<$<CONFIG:Release>:-O3 -fno-fast-math>
        $<$<CONFIG:RelWithDebInfo>:-O2 -fno-fast-math>)
endif ()


# Manually list all .h and .cpp files for the plugin
# If you are like me, you'll use globs for your sanity.
# Just ensure you employ CONFIGURE_DEPENDS so the build system picks up changes
# If you want to appease the CMake gods and avoid globs, manually add files like so:
# set(SourceFiles Source/PluginEditor.h Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginProcessor.cpp)
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

# CMake discovers implementation files automatically, while the Projucer project
# requires each one to be listed explicitly.  Keep the two build frontends in
# lockstep so an implementation cannot compile in CI but fail to link in Xcode.
set(FireJucerManifest "${CMAKE_CURRENT_SOURCE_DIR}/Fire.jucer")
if (EXISTS "${FireJucerManifest}")
    file(READ "${FireJucerManifest}" FireJucerContents)
    set(FireJucerMissingSources)

    foreach (SourceFile IN LISTS SourceFiles)
        if (SourceFile MATCHES "\\.cpp$")
            file(RELATIVE_PATH RelativeSourceFile "${CMAKE_CURRENT_SOURCE_DIR}" "${SourceFile}")
            file(TO_CMAKE_PATH "${RelativeSourceFile}" RelativeSourceFile)
            string(FIND "${FireJucerContents}" "file=\"${RelativeSourceFile}\"" SourceFilePosition)

            if (SourceFilePosition EQUAL -1)
                list(APPEND FireJucerMissingSources "${RelativeSourceFile}")
            endif ()
        endif ()
    endforeach ()

    if (FireJucerMissingSources)
        list(JOIN FireJucerMissingSources "\n  " FireJucerMissingSourcesText)
        message(FATAL_ERROR
            "Fire.jucer is missing implementation files required by Xcode:\n"
            "  ${FireJucerMissingSourcesText}")
    endif ()
endif ()

target_sources(SharedCode INTERFACE ${SourceFiles})

# Adds a BinaryData target for embedding assets into the binary
include(Assets)

# MacOS only: Cleans up folder and target organization on Xcode.
include(XcodePrettify)

# This is where you can set preprocessor definitions for JUCE and your plugin
target_compile_definitions(SharedCode
    INTERFACE

    # JUCE_WEB_BROWSER and JUCE_USE_CURL off by default
    JUCE_WEB_BROWSER=0  # If you set this to 1, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you set this to 1, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_VST3_CAN_REPLACE_VST2=0

    # Uncomment if you are paying for a an Indie/Pro license or releasing under GPLv3
    # JUCE_DISPLAY_SPLASH_SCREEN=0

    # lets the app known if we're Debug or Release
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"

    # JucePlugin_Name is for some reason doesn't use the nicer PRODUCT_NAME
    PRODUCT_NAME_WITHOUT_VERSION="Fire"
)

# Link to any other modules you added (with juce_add_module) here!
# Usually JUCE modules must have PRIVATE visibility
# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_add_module
# However, with Pamplejuce, you'll link modules to SharedCode with INTERFACE visibility
# This allows the JUCE plugin targets and the Tests target to link against it
target_link_libraries(SharedCode
    INTERFACE
    Assets
    melatonin_inspector
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Link the JUCE plugin targets our SharedCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# IPP support, comment out to disable
include(PamplejuceIPP)

# Everything related to the tests target
include(Tests)

# A separate target for Benchmarks (keeps the Tests target fast)
include(Benchmarks)

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)
