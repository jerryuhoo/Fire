#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace fire::effects
{
inline constexpr int frozenRecordingFrames = (65536 - 128) / 2 - 8;
struct FrozenRecording
{
    std::array<std::array<std::int16_t, frozenRecordingFrames>, 2> samples{};
    int head = 0, validFrames = 0;
    bool isValid() const noexcept
    {
        return validFrames > 0 && validFrames <= frozenRecordingFrames && head >= 0
            && head < frozenRecordingFrames && (validFrames == frozenRecordingFrames || head == validFrames);
    }
};
using FrozenRecordingPtr = std::shared_ptr<const FrozenRecording>;
using FrozenRecordings = std::array<std::array<FrozenRecordingPtr, 8>, 5>;

// One writer per mailbox. Atomics make failed seqlock reads legal C++, including
// while a fresh capture is being published. Readers never hold a lock needed by
// audio, and an audio reader simply retains its previous state on contention.
class AtomicFrozenRecording
{
    static_assert(std::atomic<std::int16_t>::is_always_lock_free);
public:
    AtomicFrozenRecording() noexcept
    {
        for (auto& channel : samples) for (auto& value : channel) value.store(0, std::memory_order_relaxed);
    }
    std::uint64_t generation() const noexcept { return sequence.load(std::memory_order_seq_cst); }
    template<class ReadSample>
    void publish(int frames, int writeHead, std::uint32_t publication, ReadSample read) noexcept
    {
        sequence.fetch_add(1, std::memory_order_seq_cst);
        valid.store(frames, std::memory_order_relaxed);
        head.store(writeHead, std::memory_order_relaxed);
        requiredPublication.store(publication, std::memory_order_relaxed);
        if (frames > 0)
            for (size_t channel = 0; channel < samples.size(); ++channel)
                for (int i = 0; i < frozenRecordingFrames; ++i)
                    samples[channel][static_cast<size_t>(i)].store(read(channel, i), std::memory_order_relaxed);
        sequence.fetch_add(1, std::memory_order_seq_cst);
    }
    void publish(const FrozenRecording* recording, std::uint32_t publication) noexcept
    {
        publish(recording ? recording->validFrames : 0, recording ? recording->head : 0, publication,
            [recording](size_t channel, int frame) { return recording->samples[channel][static_cast<size_t>(frame)]; });
    }
    bool copy(FrozenRecording& destination, std::uint64_t& version,
              std::uint32_t acceptedPublication = 0, bool respectPublication = false) const noexcept
    {
        const auto before = sequence.load(std::memory_order_seq_cst);
        if (before == 0 || (before & 1u) != 0u) return false;
        const auto publication = requiredPublication.load(std::memory_order_relaxed);
        if (respectPublication && static_cast<std::int32_t>(acceptedPublication - publication) < 0) return false;
        destination.head = head.load(std::memory_order_relaxed);
        destination.validFrames = valid.load(std::memory_order_relaxed);
        if (destination.validFrames > 0)
            for (size_t channel = 0; channel < samples.size(); ++channel)
                for (int i = 0; i < frozenRecordingFrames; ++i)
                    destination.samples[channel][static_cast<size_t>(i)] = samples[channel][static_cast<size_t>(i)].load(std::memory_order_relaxed);
        if (before != sequence.load(std::memory_order_seq_cst)) return false;
        version = before;
        return true;
    }
private:
    std::array<std::array<std::atomic<std::int16_t>, frozenRecordingFrames>, 2> samples;
    std::atomic<int> head{0}, valid{0};
    std::atomic<std::uint32_t> requiredPublication{0};
    std::atomic<std::uint64_t> sequence{0};
};
}
