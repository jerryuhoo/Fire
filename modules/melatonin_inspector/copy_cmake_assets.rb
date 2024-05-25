#!/usr/bin/env ruby

# Note, this needs to be run from your PROJECT directory, not the module's directory
# e.g.
# modules/melatonin_inspector/copy_cmake_assets.rb
#
# You can run it over and over without worry...

require 'fileutils'

# you might need to change if you're not Sudara....
build_dir = "cmake-build-debug"
module_dir = "modules/melatonin_inspector/"

# you shouldn't have to change anyhting below. File an issue if you do.
source_build_path = File.join module_dir, "juce_binarydata_MelatoninInspectorAssets/JUCELibraryCode"
source_dir = File.join(build_dir, source_build_path)
target_cpp_dir = File.join module_dir, "LatestCompiledAssets"
mela_cpp_file = File.join module_dir, "melatonin_inspector.cpp"
target_h_file = File.join target_cpp_dir, "InspectorBinaryData.h"

if !File.directory? source_dir
  puts "source_dir not found, check the build_dir: " + source_dir
elsif !File.directory? target_cpp_dir
  puts "target_cpp_dir not found, check it: " + target_cpp_dir
else

  puts "Removing existing *.cpp and InspectorBinaryData.h"
  FileUtils.rm_rf Dir.glob "#{target_cpp_dir}/*.cpp"
  FileUtils.rm_f target_h_file

  puts "Copying over the newly generated assets"
  FileUtils.cp Dir["#{source_dir}/*.cpp"], "#{target_cpp_dir}/"

  puts "Coping over InspectorBinaryData.h"
  FileUtils.cp "#{source_dir}/InspectorBinaryData.h", target_h_file

  puts "Rewriting #{mela_cpp_file} with new includes"
  cpp_file = File.open mela_cpp_file, 'w'
  cpp_file.write "// WARNING! This file is automatically written by copy_cmake_assets.rb\n\n"
  cpp_file.write "// As recommended by the JUCE MODULE API, these cpp files are included by the main module cpp\n"
  cpp_file.write "// See https://github.com/juce-framework/JUCE/blob/master/docs/JUCE%20Module%20Format.md#module-cpp-files\n\n"
  cpp_file.write "#include <juce_core/juce_core.h>\n"
  cpp_file.write "JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE(\"-Wredundant-decls\")\n"
  cpp_file.write "// NOLINTBEGIN(bugprone-suspicious-include)\n"

  Dir["#{target_cpp_dir}/*.cpp"].each do |file|
    cpp_file.write "#include \"LatestCompiledAssets/#{File.basename(file)}\"\n"
  end
  cpp_file.write "// NOLINTEND(bugprone-suspicious-include)\n"
    cpp_file.write "JUCE_END_IGNORE_WARNINGS_GCC_LIKE\n"
  cpp_file.close
end

puts "All done."
