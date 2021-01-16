# Declare minimum version for CMake
cmake_minimum_required(VERSION 3.15)

# Declare project target and version
project(Fire VERSION 0.82.1)

# Add subdirectories for JUCE and the plugin
add_subdirectory(JUCE)
add_subdirectory(Fire)

# Declare plugin target with configuration
juce_add_plugin(Fire
    VERSION 0.82.1
    PLUGIN_MANUFACTURER_CODE Code
    PLUGIN_CODE Cod3
    FORMATS AU VST3
    PRODUCT_NAME "Fire")

# Generate the JUCE header file for our source code
juce_generate_juce_header(Fire)

# Add source files to target
target_sources(Fire PRIVATE
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp)

# Provide preprocessor definitions for JUCE
target_compile_definitions(Fire PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE audio utils library to plugin target
target_link_libraries(Fire PRIVATE
    juce::juce_audio_utils)