@echo off
setlocal
set "FIRE_JUCE_DIR=%~dp0..\JUCE"
set "FIRE_JUCE_PATCH=%~dp0juce-9.0.3-callback-safety.patch"
set "FIRE_JUCE_REVISION="
for /f "delims=" %%r in ('git -C "%FIRE_JUCE_DIR%" rev-parse HEAD') do set "FIRE_JUCE_REVISION=%%r"
if not "%FIRE_JUCE_REVISION%"=="be29c81492b6151c8ea8d14c840e1311963b3a83" (
    echo Fire's JUCE safety patch requires the pinned JUCE revision. 1>&2
    exit /b 1
)
git -C "%FIRE_JUCE_DIR%" apply --reverse --check "%FIRE_JUCE_PATCH%" >nul 2>&1
if not errorlevel 1 exit /b 0
git -C "%FIRE_JUCE_DIR%" apply --check "%FIRE_JUCE_PATCH%"
if errorlevel 1 exit /b 1
git -C "%FIRE_JUCE_DIR%" apply "%FIRE_JUCE_PATCH%"
exit /b %errorlevel%
