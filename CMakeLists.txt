# Declare minimum version for CMake
cmake_minimum_required(VERSION 3.15)

# Declare project target and version
project(JuceCIStarter VERSION 0.0.1)

# Add subdirectories for JUCE and the plugin
add_subdirectory(JUCE)
add_subdirectory(Fire)