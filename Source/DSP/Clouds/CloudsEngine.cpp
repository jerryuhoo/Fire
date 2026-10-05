#include "CloudsEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

// Diagnostics apply only to the unmodernised upstream headers/tables below.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#include "vendor/clouds/dsp/granular_sample_player.h"
#include "vendor/clouds/dsp/fx/diffuser.h"
#include "vendor/clouds/dsp/fx/reverb.h"
#include "vendor/stmlib/dsp/filter.h"

// Keep the generated, licensed tables in this implementation unit.
#include "vendor/clouds/resources.inc"
#include "vendor/stmlib/dsp/units.inc"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace fire::effects
{
namespace
{
constexpr double coreRate = 32000.0;
constexpr int coreBlock = 32;
constexpr double pi = 3.1415926535897932384626433832795;
using Frame = fire_clouds_vendor::FloatFrame;

float safeSample(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
}
float safeControl(float value, float fallback, float minimum = 0.0f, float maximum = 1.0f) noexcept
{
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}
CloudsParameters sanitise(const CloudsParameters& p) noexcept
{
    return {safeControl(p.position, 0.1f), safeControl(p.size, 0.5f),
            safeControl(p.pitch, 0.0f, -24.0f, 24.0f), safeControl(p.density, 0.25f),
            safeControl(p.texture, 0.5f), safeControl(p.spread, 0.5f),
            safeControl(p.feedback, 0.0f), safeControl(p.reverb, 0.0f), p.freeze, p.publicationSequence};
}
size_t powerOfTwo(size_t value)
{
    size_t result = 1;
    while (result < value) result *= 2;
    return result;
}

// Tables are shared by instances with the same rate ratio. The cache is only
// visited by prepare(); no lock or reference-count mutation occurs in process.
struct SrcKernel
{
    static constexpr int phases = 256;
    int taps = 48;
    std::vector<float> coefficients;

    SrcKernel(int length, double cutoff) : taps(length), coefficients(static_cast<size_t>((phases + 1) * length))
    {
        const int firstOffset = 1 - taps / 2;
        std::vector<double> row(static_cast<size_t>(taps));
        for (int phase = 0; phase < phases; ++phase)
        {
            const double fraction = static_cast<double>(phase) / phases;
            double sum = 0.0;
            for (int tap = 0; tap < taps; ++tap)
            {
                const double distance = firstOffset + tap - fraction;
                const double windowAngle = pi * distance / (0.5 * taps);
                const double window = std::abs(distance) >= 0.5 * taps ? 0.0
                    : 0.42 + 0.5 * std::cos(windowAngle) + 0.08 * std::cos(2.0 * windowAngle);
                const double argument = pi * cutoff * distance;
                const double value = cutoff * (std::abs(argument) < 1.0e-12 ? 1.0 : std::sin(argument) / argument) * window;
                row[static_cast<size_t>(tap)] = value;
                sum += value;
            }
            for (int tap = 0; tap < taps; ++tap)
                coefficients[static_cast<size_t>(phase * taps + tap)] = static_cast<float>(row[static_cast<size_t>(tap)] / sum);
        }
        coefficients[static_cast<size_t>(phases * taps)] = 0.0f;
        for (int tap = 1; tap < taps; ++tap)
            coefficients[static_cast<size_t>(phases * taps + tap)] = coefficients[static_cast<size_t>(tap - 1)];
    }
};
std::shared_ptr<const SrcKernel> acquireKernel(double inputRate, double outputRate)
{
    const double decimation = std::max(1.0, inputRate / outputRate);
    const int taps = 2 * static_cast<int>(std::ceil(24.0 * decimation));
    const double cutoff = 0.94 / decimation;
    using Key = std::pair<int, double>;
    static std::mutex mutex;
    static std::map<Key, std::weak_ptr<const SrcKernel>> cache;
    const std::lock_guard<std::mutex> lock(mutex);
    const Key key {taps, cutoff};
    if (const auto found = cache.find(key); found != cache.end())
        if (auto result = found->second.lock()) return result;
    for (auto it = cache.begin(); it != cache.end();)
        if (it->second.expired()) it = cache.erase(it); else ++it;
    auto result = std::make_shared<const SrcKernel>(taps, cutoff);
    cache[key] = result;
    return result;
}

class StreamingSrc
{
public:
    void prepare(double inputRate, double outputRate)
    {
        table = acquireKernel(inputRate, outputRate);
        step = inputRate / outputRate;
        ring.resize(powerOfTwo(static_cast<size_t>(table->taps + 64)));
        reset();
    }
    void reset() noexcept { written = position = 0; fraction = 0.0; }
    void push(Frame value) noexcept
    {
        ring[static_cast<size_t>(written) & (ring.size() - 1)] = value;
        ++written;
    }
    bool pop(Frame& result) noexcept
    {
        if (position + table->taps / 2 >= written) return false;
        const auto phasePosition = fraction * SrcKernel::phases;
        const int phase = std::min(SrcKernel::phases - 1, static_cast<int>(phasePosition));
        const auto blend = static_cast<float>(phasePosition - phase);
        const auto* first = table->coefficients.data() + phase * table->taps;
        const auto* second = first + table->taps;
        const auto beginning = position + 1 - table->taps / 2;
        double left = 0.0, right = 0.0;
        for (int tap = 0; tap < table->taps; ++tap)
        {
            const auto index = beginning + tap;
            if (index < 0 || index < written - static_cast<int64_t>(ring.size())) continue;
            const auto& sample = ring[static_cast<size_t>(index) & (ring.size() - 1)];
            const auto coefficient = first[tap] + blend * (second[tap] - first[tap]);
            left += static_cast<double>(sample.l) * coefficient;
            right += static_cast<double>(sample.r) * coefficient;
        }
        result = {static_cast<float>(left), static_cast<float>(right)};
        fraction += step;
        const auto increment = static_cast<int64_t>(fraction);
        position += increment;
        fraction -= static_cast<double>(increment);
        return true;
    }
private:
    std::shared_ptr<const SrcKernel> table;
    std::vector<Frame> ring;
    int64_t written = 0, position = 0;
    double fraction = 0.0, step = 1.0;
};

class NormalCore
{
public:
    void prepare()
    {
        for (size_t channel = 0; channel < recording.size(); ++channel)
        {
            recording[channel].resize(recordingSamples);
            buffers[channel].Init(recording[channel].data(), recordingSamples, tails[channel].data());
        }
        reset();
    }
    void reset(bool preserveFrozen = false) noexcept
    {
        const bool keep = preserveFrozen && actuallyFrozen && buffers[0].valid_samples() > 0;
        const int head = buffers[0].head(), valid = buffers[0].valid_samples();
        for (auto& buffer : buffers) buffer.Reset();
        player.Init(2, 32);
        diffuser.Init(diffusionMemory.data());
        reverb.Init(reverbMemory.data());
        for (auto& filter : feedbackFilter) filter.Init();
        feedback.fill({0.0f, 0.0f});
        freezeLowpass = 0.0f;
        samplesSinceSignal = 0;
        heardSignal = false;
        previousFreezeRequest = pendingCapture = false;
        actuallyFrozen = false;
        if (keep)
        {
            for (auto& buffer : buffers) buffer.RestoreRecordingState(head, valid);
            freezeLowpass = 1.0f; samplesSinceSignal = recordingSamples;
            heardSignal = previousFreezeRequest = actuallyFrozen = true;
        }
    }
    bool isFrozen() const noexcept { return actuallyFrozen && buffers[0].valid_samples() > 0; }
    void publishRecording(AtomicFrozenRecording& destination, std::uint32_t publication) const noexcept
    {
        const int valid = buffers[0].valid_samples();
        destination.publish(isFrozen() ? valid : 0, buffers[0].head(), publication,
            [this, valid](size_t channel, int frame) -> std::int16_t
            { return frame < valid ? recording[channel][static_cast<size_t>(frame)] : 0; });
    }
    void restoreRecording(const FrozenRecording& saved) noexcept
    {
        reset();
        if (!saved.isValid()) return;
        for (size_t channel = 0; channel < recording.size(); ++channel)
        {
            std::copy(saved.samples[channel].begin(), saved.samples[channel].end(), recording[channel].begin());
            buffers[channel].RestoreRecordingState(saved.head, saved.validFrames);
        }
        freezeLowpass = 1.0f; samplesSinceSignal = recordingSamples;
        heardSignal = previousFreezeRequest = actuallyFrozen = true;
    }
    void process(const std::array<Frame, coreBlock>& input,
                 std::array<Frame, coreBlock>& output,
                 const CloudsParameters& controls) noexcept
    {
        using namespace fire_clouds_vendor;
        using namespace fire_clouds_stmlib;
        if (controls.freeze && !previousFreezeRequest)
            pendingCapture = !heardSignal || buffers[0].valid_samples() == 0;
        if (!controls.freeze || samplesSinceSignal >= recordingSamples)
            pendingCapture = false;
        previousFreezeRequest = controls.freeze;
        const bool freeze = controls.freeze && !pendingCapture;
        actuallyFrozen = freeze;
        ONE_POLE(freezeLowpass, freeze ? 1.0f : 0.0f, 0.0005f)
        const auto amount = controls.feedback;
        feedbackFilter[0].set_f_q<FREQUENCY_FAST>((20.0f + 100.0f * amount * amount) / 32000.0f, 1.0f);
        feedbackFilter[1].set(feedbackFilter[0]);
        const float feedbackGain = amount * (1.0f - freezeLowpass);
        for (int sample = 0; sample < coreBlock; ++sample)
        {
            // Codec-equivalent range and quantisation, after input SRC.
            const auto toCodec = [](float value) {
                return static_cast<float>(static_cast<int16_t>(std::clamp(safeSample(value), -1.0f, 32767.0f / 32768.0f) * 32768.0f)) / 32768.0f;
            };
            const auto l = toCodec(input[static_cast<size_t>(sample)].l);
            const auto r = toCodec(input[static_cast<size_t>(sample)].r);
            const auto fbL = feedbackFilter[0].Process<FILTER_MODE_HIGH_PASS>(feedback[static_cast<size_t>(sample)].l);
            const auto fbR = feedbackFilter[1].Process<FILTER_MODE_HIGH_PASS>(feedback[static_cast<size_t>(sample)].r);
            recordInput[0][static_cast<size_t>(sample)] = l + feedbackGain * (SoftLimit(feedbackGain * 1.4f * fbL + l) - l);
            recordInput[1][static_cast<size_t>(sample)] = r + feedbackGain * (SoftLimit(feedbackGain * 1.4f * fbR + r) - r);
            if (std::abs(l) > 1.0e-5f || std::abs(r) > 1.0e-5f) heardSignal = true;
        }
        for (size_t channel = 0; channel < buffers.size(); ++channel)
            buffers[channel].WriteFade(recordInput[channel].data(), coreBlock, 1, !freeze);
        if (!freeze && heardSignal)
            samplesSinceSignal = std::min(recordingSamples, samplesSinceSignal + coreBlock);

        Parameters parameters {};
        parameters.position = controls.position;
        parameters.size = controls.size;
        parameters.pitch = controls.pitch;
        parameters.density = controls.density;
        parameters.texture = controls.texture;
        parameters.stereo_spread = controls.spread;
        parameters.freeze = freeze;
        parameters.granular.use_deterministic_seed = controls.density < 0.5f;
        parameters.granular.overlap = controls.density >= 0.53f ? (controls.density - 0.53f) * 2.12f
            : controls.density <= 0.47f ? (0.47f - controls.density) * 2.12f : 0.0f;
        parameters.granular.window_shape = controls.texture < 0.75f ? controls.texture * 1.333f : 1.0f;
        player.Play(buffers.data(), parameters, granularOutput.data(), coreBlock);
        for (int sample = 0; sample < coreBlock; ++sample)
            output[static_cast<size_t>(sample)] = {granularOutput[static_cast<size_t>(sample * 2)], granularOutput[static_cast<size_t>(sample * 2 + 1)]};
        diffuser.set_amount(controls.texture > 0.75f ? (controls.texture - 0.75f) * 4.0f : 0.0f);
        diffuser.Process(output.data(), coreBlock);
        feedback = output; // The upstream feedback excludes reverb.
        float reverbAmount = controls.reverb * 0.95f + amount * (2.0f - amount) * freezeLowpass;
        reverbAmount = std::clamp(reverbAmount, 0.0f, 1.0f);
        reverb.set_amount(reverbAmount * 0.54f);
        reverb.set_diffusion(0.7f);
        reverb.set_time(0.35f + 0.63f * reverbAmount);
        reverb.set_input_gain(0.2f);
        reverb.set_lp(0.6f + 0.37f * amount);
        reverb.Process(output.data(), coreBlock);
        for (auto& sample : output)
        {
            const auto convert = [](float value) {
                if (!std::isfinite(value)) return 0.0f;
                return static_cast<float>(SoftConvert(std::clamp(value * 1.2f, -6.0f, 6.0f))) / 32768.0f;
            };
            sample.l = convert(sample.l);
            sample.r = convert(sample.r);
        }
    }
private:
    // Same per-channel recording allocation as the hardware's stereo mode.
    static constexpr int recordingSamples = (65536 - 128) / 2;
    std::array<std::vector<int16_t>, 2> recording;
    std::array<std::array<int16_t, 256>, 2> tails {};
    std::array<fire_clouds_vendor::AudioBuffer<fire_clouds_vendor::RESOLUTION_16_BIT>, 2> buffers;
    fire_clouds_vendor::GranularSamplePlayer player;
    fire_clouds_vendor::Diffuser diffuser;
    fire_clouds_vendor::Reverb reverb;
    std::array<float, 2048> diffusionMemory {};
    std::array<uint16_t, 16384> reverbMemory {};
    std::array<fire_clouds_stmlib::Svf, 2> feedbackFilter;
    std::array<Frame, coreBlock> feedback {};
    std::array<std::array<float, coreBlock>, 2> recordInput {};
    std::array<float, coreBlock * 2> granularOutput {};
    float freezeLowpass = 0.0f;
    int samplesSinceSignal = 0;
    bool heardSignal = false;
    bool previousFreezeRequest = false, pendingCapture = false;
    bool actuallyFrozen = false;
};
}

struct CloudsEngine::RecordingExchange
{
    AtomicFrozenRecording captured, restored;
    FrozenRecording audioScratch;
    std::atomic<std::uint64_t> applied{0};
    std::mutex writerLock; // Restoring writers only; audio never enters it.
};

struct CloudsEngine::Impl
{
    explicit Impl(RecordingExchange& exchangeToUse) : exchange(exchangeToUse) {}
    void prepare(double rate)
    {
        sampleRate = std::isfinite(rate) && rate >= 8000.0 && rate <= 384000.0 ? rate : 48000.0;
        inputConverter.prepare(sampleRate, coreRate);
        outputConverter.prepare(coreRate, sampleRate);
        core.prepare();
        minimumOutput = static_cast<size_t>(std::ceil(sampleRate / coreRate * coreBlock)) + 2;
        outputQueue.resize(powerOfTwo(4 * minimumOutput + 128));
        reset();
    }
    void reset(bool preserveFrozen = false) noexcept
    {
        inputConverter.reset();
        outputConverter.reset();
        core.reset(preserveFrozen);
        // A restore is a one-shot publication, not a reset preset. Replaying
        // an already consumed mailbox would overwrite a later live capture.
        // prepare() and authoritative state loads stage a fresh version when
        // their recording needs to be installed again.
        if (!preserveFrozen) exchange.captured.publish(nullptr, 0);
        fill = 0;
        outputRead = outputWrite = outputCount = 0;
        outputPrimed = parametersPrimed = false;
        smoothed = {};
    }
    void smooth(const CloudsParameters& parameters) noexcept
    {
        const auto target = sanitise(parameters);
        if (!parametersPrimed)
        {
            smoothed = target;
            parametersPrimed = true;
            return;
        }
        // 2 ms at the fixed internal rate; the host's base/route ramps remain
        // outside this adapter. This does not depend on host callback size.
        constexpr float coefficient = 0.015503562994591547f; // 1 - exp(-1 / 64)
        const auto approach = [](float& value, float targetValue) { value += coefficient * (targetValue - value); };
        approach(smoothed.position, target.position); approach(smoothed.size, target.size);
        approach(smoothed.pitch, target.pitch); approach(smoothed.density, target.density);
        approach(smoothed.texture, target.texture); approach(smoothed.spread, target.spread);
        approach(smoothed.feedback, target.feedback); approach(smoothed.reverb, target.reverb);
        smoothed.freeze = target.freeze;
        smoothed.publicationSequence = target.publicationSequence;
    }
    void process(float& left, float& right, const CloudsParameters& parameters) noexcept
    {
        if (exchange.restored.generation() != appliedRestore)
        {
            std::uint64_t version = 0;
            if (exchange.restored.copy(exchange.audioScratch, version, parameters.publicationSequence, true))
            {
                core.restoreRecording(exchange.audioScratch);
                core.publishRecording(exchange.captured, parameters.publicationSequence);
                appliedRestore = version;
                exchange.applied.store(version, std::memory_order_release);
            }
        }
        inputConverter.push({safeSample(left), safeSample(right)});
        Frame frame;
        while (inputConverter.pop(frame))
        {
            smooth(parameters);
            input[static_cast<size_t>(fill++)] = frame;
            if (fill != coreBlock) continue;
            const bool wasFrozen = core.isFrozen();
            core.process(input, output, smoothed);
            if (wasFrozen != core.isFrozen()) core.publishRecording(exchange.captured, smoothed.publicationSequence);
            fill = 0;
            for (const auto& sample : output)
            {
                outputConverter.push(sample);
                while (outputConverter.pop(frame))
                {
                    // The fixed input/output rate ratio and one-block prefill
                    // bound this queue; capacity is reserved only in prepare.
                    if (outputCount < outputQueue.size())
                    {
                        outputQueue[outputWrite] = frame;
                        outputWrite = (outputWrite + 1) & (outputQueue.size() - 1);
                        ++outputCount;
                    }
                }
            }
        }
        if (!outputPrimed && outputCount >= minimumOutput) outputPrimed = true;
        left = right = 0.0f;
        if (outputPrimed && outputCount != 0)
        {
            const auto value = outputQueue[outputRead];
            outputRead = (outputRead + 1) & (outputQueue.size() - 1);
            --outputCount;
            left = std::isfinite(value.l) ? value.l : 0.0f;
            right = std::isfinite(value.r) ? value.r : 0.0f;
        }
    }
    double sampleRate = 48000.0;
    StreamingSrc inputConverter, outputConverter;
    NormalCore core;
    std::array<Frame, coreBlock> input {}, output {};
    std::vector<Frame> outputQueue;
    size_t minimumOutput = 0, outputRead = 0, outputWrite = 0, outputCount = 0;
    int fill = 0;
    bool outputPrimed = false, parametersPrimed = false;
    CloudsParameters smoothed;
    RecordingExchange& exchange;
    std::uint64_t appliedRestore = 0;
};

CloudsEngine::CloudsEngine() noexcept = default;
CloudsEngine::~CloudsEngine() = default;
void CloudsEngine::prepare(double sampleRate)
{
    ensureRecordingExchange();
    // The fixed-rate material remains usable after a host rate change.
    if (auto saved = copyFrozenRecording()) stageFrozenRecording(saved);
    if (!implementation) implementation = std::make_unique<Impl>(*recordings);
    implementation->prepare(sampleRate);
}
void CloudsEngine::reset(bool preserveFrozen) noexcept
{
    if (implementation) implementation->reset(preserveFrozen);
}
void CloudsEngine::process(float& left, float& right, const CloudsParameters& parameters) noexcept
{
    if (implementation) implementation->process(left, right, parameters);
    else left = right = 0.0f;
}
void CloudsEngine::ensureRecordingExchange()
{
    if (!recordings) recordings = std::make_unique<RecordingExchange>();
}
void CloudsEngine::stageFrozenRecording(const FrozenRecordingPtr& saved, std::uint32_t publication)
{
    if (!recordings && !saved) return;
    ensureRecordingExchange();
    const std::lock_guard<std::mutex> lock(recordings->writerLock);
    recordings->restored.publish(saved.get(), publication);
}
FrozenRecordingPtr CloudsEngine::copyFrozenRecording() const
{
    if (!recordings) return {};
    const auto* source = recordings->restored.generation() != recordings->applied.load(std::memory_order_acquire)
        ? &recordings->restored : &recordings->captured;
    if (source->generation() == 0) return {};
    auto saved = std::make_shared<FrozenRecording>();
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        std::uint64_t version = 0;
        if (source->copy(*saved, version)) return saved->isValid() ? saved : FrozenRecordingPtr{};
    }
    return {};
}
}
