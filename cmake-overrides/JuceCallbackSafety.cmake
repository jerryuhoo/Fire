# Backport callback lifetime guards to the pinned JUCE revision. Never update
# the submodule revision or discard other dependency edits implicitly.
function(fire_apply_juce_callback_safety)
    get_filename_component(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(juce_dir "${root}/JUCE")
    set(patch "${root}/cmake-overrides/juce-9.0.3-callback-safety.patch")
    set(expected "be29c81492b6151c8ea8d14c840e1311963b3a83")
    find_package(Git REQUIRED QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_dir}" rev-parse HEAD
        OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE result ERROR_QUIET)
    if (NOT result EQUAL 0 OR NOT revision STREQUAL expected)
        message(FATAL_ERROR "Fire's JUCE safety patch requires pinned revision ${expected}; initialise submodules or review the patch before upgrading JUCE.")
    endif ()
    file(MAKE_DIRECTORY "${root}/Builds")
    file(LOCK "${root}/Builds/.juce-callback-safety.lock" GUARD FUNCTION TIMEOUT 30)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_dir}" apply --reverse --check "${patch}"
        RESULT_VARIABLE applied OUTPUT_QUIET ERROR_QUIET)
    if (applied EQUAL 0)
        return()
    endif ()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_dir}" apply --check "${patch}"
        RESULT_VARIABLE clean ERROR_VARIABLE details)
    if (NOT clean EQUAL 0)
        message(FATAL_ERROR "JUCE safety patch conflicts with local dependency edits; no files were reset.\n${details}")
    endif ()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_dir}" apply "${patch}"
        RESULT_VARIABLE result ERROR_VARIABLE details)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Could not apply JUCE callback safety patch: ${details}")
    endif ()
    message(STATUS "Applied Fire's JUCE callback lifetime guards")
endfunction()
fire_apply_juce_callback_safety()
