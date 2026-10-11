// A small real CLAP host used by CTest on every platform. No Fire implementation
// is linked here: every operation crosses the distributed plug-in's CLAP ABI.
#include <clap/clap.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
 #define NOMINMAX
 #include <windows.h>
#else
 #include <dlfcn.h>
 #include <sys/stat.h>
#endif

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

#if defined(_WIN32)
std::wstring widePath(const std::string& path)
{
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, nullptr, 0);
    require(size > 0, "invalid UTF-8 plug-in path");
    std::wstring result(static_cast<size_t>(size), L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, result.data(), size) > 0,
            "plug-in path conversion failed");
    result.resize(result.size() - 1);
    return result;
}
#endif

std::string absolutePath(const char* path)
{
#if defined(_WIN32)
    const auto input = widePath(path);
    const auto required = GetFullPathNameW(input.c_str(), 0, nullptr, nullptr);
    require(required > 0, "could not resolve plug-in path");
    std::wstring full(static_cast<size_t>(required), L'\0');
    const auto length = GetFullPathNameW(input.c_str(), required, full.data(), nullptr);
    require(length > 0 && length < required, "could not resolve plug-in path");
    full.resize(length);
    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, full.c_str(), -1, nullptr, 0, nullptr, nullptr);
    require(utf8Size > 0, "plug-in path conversion failed");
    std::string result(static_cast<size_t>(utf8Size), '\0');
    require(WideCharToMultiByte(CP_UTF8, 0, full.c_str(), -1, result.data(), utf8Size, nullptr, nullptr) > 0,
            "plug-in path conversion failed");
    result.resize(result.size() - 1);
    return result;
#else
    std::unique_ptr<char, decltype(&std::free)> resolved(realpath(path, nullptr), &std::free);
    require(resolved != nullptr, "could not resolve plug-in path");
    return resolved.get();
#endif
}

struct Library
{
#if defined(_WIN32)
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
    const clap_plugin_entry_t* entry = nullptr;
    bool initialized = false;
    explicit Library(const std::string& path)
    {
        auto binary = path;
#if defined(__APPLE__)
        struct stat info {};
        if (stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
            binary += "/Contents/MacOS/Fire";
#endif
#if defined(_WIN32)
        handle = LoadLibraryW(widePath(binary).c_str());
        if (handle) entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(handle, "clap_entry"));
#else
        handle = dlopen(binary.c_str(), RTLD_LOCAL | RTLD_NOW);
        if (handle) entry = static_cast<const clap_plugin_entry_t*>(dlsym(handle, "clap_entry"));
        else if (const char* error = dlerror()) std::cerr << error << '\n';
#endif
    }
    ~Library()
    {
        if (initialized) entry->deinit();
#if defined(_WIN32)
        if (handle) FreeLibrary(handle);
#else
        if (handle) dlclose(handle);
#endif
    }
};

struct Host
{
    bool audioThread = false, callbackPending = false;
    clap_host_t api { CLAP_VERSION, this, "Fire CLAP smoke host", "Blue Wings Music",
        "https://www.bluewingsmusic.com/fire.html", "1.0.0", extension,
        [](const clap_host_t*) {}, [](const clap_host_t*) {},
        [](const clap_host_t* h) { self(h).callbackPending = true; } };
    static Host& self(const clap_host_t* h) { return *static_cast<Host*>(h->host_data); }
    static const void* CLAP_ABI extension(const clap_host_t*, const char* id)
    {
        static const clap_host_thread_check_t threads {
            [](const clap_host_t* h) { return !self(h).audioThread; },
            [](const clap_host_t* h) { return self(h).audioThread; } };
        if (std::strcmp(id, CLAP_EXT_THREAD_CHECK) == 0) return &threads;
        return nullptr;
    }
    void dispatch(const clap_plugin_t* plugin)
    {
        for (int count = 0; callbackPending && count < 100; ++count)
        {
            callbackPending = false;
            plugin->on_main_thread(plugin);
        }
        require(!callbackPending, "plug-in main-thread callback did not settle");
    }
};

struct Plugin
{
    Host host;
    const clap_plugin_t* api = nullptr;
    bool active = false, processing = false;
    ~Plugin()
    {
        if (!api) return;
        if (processing) { host.audioThread = true; api->stop_processing(api); host.audioThread = false; }
        if (active) api->deactivate(api);
        api->destroy(api);
    }
    void create(const clap_plugin_factory_t* factory, const char* id)
    {
        api = factory->create_plugin(factory, &host.api, id);
        require(api && api->init(api), "CLAP instance initialization failed");
        host.dispatch(api);
    }
};

struct MemoryStream
{
    std::vector<uint8_t> bytes;
    size_t offset = 0;
    clap_ostream_t output { this, [](const clap_ostream_t* s, const void* source, uint64_t count) -> int64_t
    {
        auto& state = *static_cast<MemoryStream*>(s->ctx);
        // Short writes/reads are legal; exercise the plug-in's stream loops.
        const auto size = static_cast<size_t>(std::min<uint64_t>(count, 79));
        if (state.bytes.size() + size > 128 * 1024 * 1024) return -1;
        const auto* begin = static_cast<const uint8_t*>(source);
        state.bytes.insert(state.bytes.end(), begin, begin + size);
        return static_cast<int64_t>(size);
    } };
    clap_istream_t input { this, [](const clap_istream_t* s, void* target, uint64_t count) -> int64_t
    {
        auto& state = *static_cast<MemoryStream*>(s->ctx);
        const auto size = std::min({static_cast<size_t>(count), size_t(73), state.bytes.size() - state.offset});
        std::memcpy(target, state.bytes.data() + state.offset, size);
        state.offset += size;
        return static_cast<int64_t>(size);
    } };
};

const clap_input_events_t emptyInput { nullptr,
    [](const clap_input_events_t*) -> uint32_t { return 0; },
    [](const clap_input_events_t*, uint32_t) -> const clap_event_header_t* { return nullptr; } };
const clap_output_events_t discardOutput { nullptr,
    [](const clap_output_events_t*, const clap_event_header_t*) { return true; } };

void checkAudio(Plugin& plugin)
{
    const auto* ports = static_cast<const clap_plugin_audio_ports_t*>(plugin.api->get_extension(plugin.api, CLAP_EXT_AUDIO_PORTS));
    require(ports && ports->count(plugin.api, true) == 1 && ports->count(plugin.api, false) == 1,
            "expected one main input and output bus");
    clap_audio_port_info_t inputInfo {}, outputInfo {};
    require(ports->get(plugin.api, 0, true, &inputInfo) && ports->get(plugin.api, 0, false, &outputInfo)
        && inputInfo.channel_count == 2 && outputInfo.channel_count == 2, "expected stereo main buses");
    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        for (const uint32_t block : {64u, 512u})
        {
            require(plugin.api->activate(plugin.api, rate, block, block), "CLAP activation failed");
            plugin.active = true;
            std::array<std::vector<float>, 4> buffers;
            for (auto& b : buffers) b.resize(block);
            float* inputChannels[] {buffers[0].data(), buffers[1].data()};
            float* outputChannels[] {buffers[2].data(), buffers[3].data()};
            clap_audio_buffer_t input { inputChannels, nullptr, 2, 0, 0 };
            clap_audio_buffer_t output { outputChannels, nullptr, 2, 0, 0 };
            double energy = 0;
            plugin.host.audioThread = true;
            require(plugin.api->start_processing(plugin.api), "CLAP start_processing failed");
            plugin.processing = true;
            for (int iteration = 0; iteration < 32; ++iteration)
            {
                for (uint32_t i = 0; i < block; ++i)
                {
                    const auto t = static_cast<double>(iteration * block + i) / rate;
                    buffers[0][i] = static_cast<float>(0.12 * std::sin(t * 440.0 * 6.283185307179586));
                    buffers[1][i] = static_cast<float>(0.10 * std::sin(t * 701.0 * 6.283185307179586));
                    buffers[2][i] = buffers[3][i] = 0;
                }
                clap_process_t process {};
                process.steady_time = iteration * block;
                process.frames_count = block;
                process.audio_inputs = &input; process.audio_inputs_count = 1;
                process.audio_outputs = &output; process.audio_outputs_count = 1;
                process.in_events = &emptyInput; process.out_events = &discardOutput;
                require(plugin.api->process(plugin.api, &process) != CLAP_PROCESS_ERROR, "CLAP process failed");
                for (const auto channel : {2, 3})
                    for (const auto sample : buffers[channel])
                    {
                        require(std::isfinite(sample), "non-finite output from CLAP");
                        energy += static_cast<double>(sample) * sample;
                    }
            }
            plugin.api->stop_processing(plugin.api); plugin.processing = false;
            plugin.host.audioThread = false;
            plugin.api->deactivate(plugin.api); plugin.active = false;
            plugin.host.dispatch(plugin.api);
            require(energy > 1.0e-8, "CLAP unexpectedly produced silent output");
        }
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 3, "usage: FireClapSmoke <plugin.clap> <expected-version>");
        const auto path = absolutePath(argv[1]);
        Library library(path);
        require(library.entry && clap_version_is_compatible(library.entry->clap_version), "missing or incompatible clap_entry");
        require(library.entry->init(path.c_str()), "CLAP entry initialization failed");
        library.initialized = true;
        const auto* factory = static_cast<const clap_plugin_factory_t*>(library.entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        require(factory && factory->get_plugin_count(factory) == 1, "expected one CLAP plug-in");
        const auto* descriptor = factory->get_plugin_descriptor(factory, 0);
        require(descriptor && std::strcmp(descriptor->version, argv[2]) == 0, "CLAP version differs from VERSION");
        Plugin original;
        original.create(factory, descriptor->id);
        const auto* parameters = static_cast<const clap_plugin_params_t*>(original.api->get_extension(original.api, CLAP_EXT_PARAMS));
        const auto* state = static_cast<const clap_plugin_state_t*>(original.api->get_extension(original.api, CLAP_EXT_STATE));
        require(parameters && state, "CLAP parameters/state extension missing");
        const auto count = parameters->count(original.api);
        require(count > 0, "CLAP parameter bank is empty");
        clap_param_info_t chosen {};
        bool found = false;
        for (uint32_t i = 0; i < count && !found; ++i)
        {
            require(parameters->get_info(original.api, i, &chosen), "parameter metadata unavailable");
            found = (chosen.flags & CLAP_PARAM_IS_AUTOMATABLE) != 0
                && (chosen.flags & (CLAP_PARAM_IS_READONLY | CLAP_PARAM_IS_STEPPED)) == 0
                && chosen.max_value > chosen.min_value;
        }
        require(found, "no writable continuous CLAP parameter found");
        double initial = 0;
        require(parameters->get_value(original.api, chosen.id, &initial), "initial CLAP parameter value unavailable");
        const auto fraction = initial > (chosen.min_value + chosen.max_value) * 0.5 ? 0.271 : 0.731;
        const auto target = chosen.min_value + fraction * (chosen.max_value - chosen.min_value);
        clap_event_param_value_t event {};
        event.header = {sizeof(event), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        event.param_id = chosen.id; event.cookie = chosen.cookie;
        event.note_id = -1; event.port_index = event.channel = event.key = -1; event.value = target;
        const clap_input_events_t eventList { &event,
            [](const clap_input_events_t*) -> uint32_t { return 1; },
            [](const clap_input_events_t* list, uint32_t index) -> const clap_event_header_t*
            { return index == 0 ? &static_cast<const clap_event_param_value_t*>(list->ctx)->header : nullptr; } };
        parameters->flush(original.api, &eventList, &discardOutput);
        original.host.dispatch(original.api);
        double actual = 0;
        require(parameters->get_value(original.api, chosen.id, &actual), "CLAP parameter value unavailable");
        // JUCE normalized parameters can quantize their underlying dB/Hz range
        // without advertising CLAP_PARAM_IS_STEPPED. Allow that quantization but
        // require a substantial, correctly directed change before saving it.
        require(std::abs(actual - target) < 0.01 * (chosen.max_value - chosen.min_value)
            && std::abs(actual - initial) > 0.1 * (chosen.max_value - chosen.min_value),
            "CLAP parameter event was not applied");
        MemoryStream saved;
        require(state->save(original.api, &saved.output) && !saved.bytes.empty(), "CLAP state save failed");
        Plugin restored;
        restored.create(factory, descriptor->id);
        const auto* restoreState = static_cast<const clap_plugin_state_t*>(restored.api->get_extension(restored.api, CLAP_EXT_STATE));
        const auto* restoreParams = static_cast<const clap_plugin_params_t*>(restored.api->get_extension(restored.api, CLAP_EXT_PARAMS));
        require(restoreState && restoreParams && restoreState->load(restored.api, &saved.input), "CLAP state restore failed");
        restored.host.dispatch(restored.api);
        require(restoreParams->count(restored.api) == count, "CLAP restored parameter count differs");
        for (uint32_t i = 0; i < count; ++i)
        {
            clap_param_info_t info {};
            double before = 0, after = 0;
            require(parameters->get_info(original.api, i, &info)
                && parameters->get_value(original.api, info.id, &before)
                && restoreParams->get_value(restored.api, info.id, &after), "CLAP state parameter unavailable");
            require(std::abs(before - after) <= 1e-5 * std::max(1.0, info.max_value - info.min_value),
                    "CLAP state roundtrip changed a parameter");
        }
        checkAudio(restored);
        std::cout << "CLAP " << descriptor->version << ": fresh-instance state roundtrip, " << count
                  << " parameters, finite stereo audio at 44.1/48/96 kHz and 64/512 samples passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CLAP smoke test failed: " << error.what() << '\n';
        return 1;
    }
}
