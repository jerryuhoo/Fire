# pluginval's allocation self-tests intentionally allocate unused vectors.
# Recent optimisers can remove those allocations and invalidate the checks.
# Keep that instrumentation/test translation unit unoptimised; the validator
# and the Fire binaries retain their normal Release settings. No tests change.
if (CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Source/TestUtilities.cpp"
        PROPERTIES COMPILE_OPTIONS "-O0")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Source/TestUtilities.cpp"
        PROPERTIES COMPILE_OPTIONS "/Od")
endif ()
