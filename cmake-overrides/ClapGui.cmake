# Backport the JUCE 9 embedded-peer fix without moving the pinned CLAP
# dependency or changing its audio path. Work on the generated wrapper copy.
# https://github.com/free-audio/clap-juce-extensions/pull/183
set(FireClapGuiInclude "#include <juce_gui_basics/juce_gui_basics.h>")
string(FIND "${FireClapWrapper}" "${FireClapGuiInclude}" FireClapGuiIncludePosition)
if(FireClapGuiIncludePosition EQUAL -1)
    message(FATAL_ERROR "CLAP GUI includes changed; review the physical-pixel adaptation.")
endif()
# Insert after JUCE_GUI_BASICS_INCLUDE_XHEADERS is defined by the wrapper.
string(REPLACE "${FireClapGuiInclude}"
    "${FireClapGuiInclude}\n#include \"Utility/ClapGuiScaling.h\""
    FireClapWrapper "${FireClapWrapper}")

set(FireClapAttachLine "        editorWrapper->addToDesktop(0, (void *)window);")
string(REPLACE "${FireClapAttachLine}" "" FireClapWithoutAttach "${FireClapWrapper}")
string(LENGTH "${FireClapWrapper}" FireClapBeforeAttachLength)
string(LENGTH "${FireClapWithoutAttach}" FireClapAfterAttachLength)
string(LENGTH "${FireClapAttachLine}" FireClapAttachLength)
math(EXPR FireClapAttachDifference "${FireClapBeforeAttachLength} - ${FireClapAfterAttachLength}")
math(EXPR FireClapExpectedAttachDifference "2 * ${FireClapAttachLength}")
if(NOT FireClapAttachDifference EQUAL FireClapExpectedAttachDifference)
    message(FATAL_ERROR "Expected Windows and X11 CLAP attachment points; review GUI scaling adaptation.")
endif()
string(REPLACE "${FireClapAttachLine}"
    "${FireClapAttachLine}\n        fire::utility::configureClapPhysicalPixelPeer(*editorWrapper);"
    FireClapWrapper "${FireClapWrapper}")
