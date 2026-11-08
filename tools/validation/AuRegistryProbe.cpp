// Query one registered AU without enumerating or instantiating other plug-ins.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
OSType fourCC(const char* value)
{
    if (std::strlen(value) != 4) return 0;
    OSType result = 0;
    for (int i = 0; i < 4; ++i)
        result = (result << 8) | static_cast<unsigned char>(value[i]);
    return result;
}
}

int main(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cerr << "usage: AuRegistryProbe <type> <subtype> <manufacturer> <packed-version>\n";
        return 1;
    }
    AudioComponentDescription description {};
    description.componentType = fourCC(argv[1]);
    description.componentSubType = fourCC(argv[2]);
    description.componentManufacturer = fourCC(argv[3]);
    char* end = nullptr;
    errno = 0;
    const auto expected = std::strtoul(argv[4], &end, 10);
    if (!description.componentType || !description.componentSubType || !description.componentManufacturer
        || errno != 0 || end == argv[4] || *end != '\0'
        || expected > std::numeric_limits<UInt32>::max())
    {
        std::cerr << "Invalid AU identity or packed version\n";
        return 1;
    }
    const auto component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr)
    {
        std::cout << "AU not registered: " << argv[1] << '/' << argv[2] << '/' << argv[3] << '\n';
        return 2;
    }
    UInt32 version = 0;
    const auto status = AudioComponentGetVersion(component, &version);
    if (status != noErr)
    {
        std::cerr << "AudioComponentGetVersion failed: " << status << '\n';
        return 1;
    }
    std::cout << "Registered AU " << argv[1] << '/' << argv[2] << '/' << argv[3]
              << " version " << (version >> 16) << '.' << ((version >> 8) & 255)
              << '.' << (version & 255) << " (" << version << ")\n";
    if (version != expected)
    {
        std::cout << "Waiting for installed version " << expected << '\n';
        return 3;
    }
    return 0;
}
