/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   analog_tube_off_png;
    const int            analog_tube_off_pngSize = 985501;

    extern const char*   analog_tube_on_png;
    const int            analog_tube_on_pngSize = 940849;

    extern const char*   analog_tape_reel_png;
    const int            analog_tape_reel_pngSize = 2032806;

    extern const char*   vintage_walnut_png;
    const int            vintage_walnut_pngSize = 2600048;

    extern const char*   fire_anime_png;
    const int            fire_anime_pngSize = 713474;

    extern const char*   firelogo_png;
    const int            firelogo_pngSize = 9722;

    extern const char*   firewingslogo_png;
    const int            firewingslogo_pngSize = 11396;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 7;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
