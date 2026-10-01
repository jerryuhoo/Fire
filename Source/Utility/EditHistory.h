#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace fire::state
{
// Sound snapshots include parameters, curves, routes and A/B. History belongs
// to the processor, so closing the editor does not discard an editing session.
// All capture/allocation is restricted to the message thread; audio-thread
// automation never enters history and cannot block on its storage.
class EditHistory final : private juce::AudioProcessorListener
{
public:
    using Capture = std::function<juce::MemoryBlock()>;
    using Restore = std::function<void(const juce::MemoryBlock&)>;
    EditHistory(juce::AudioProcessor& owner, Capture capture, Restore restore)
        : processor(owner), captureState(std::move(capture)), restoreState(std::move(restore)),
          gestures(static_cast<size_t>(owner.getParameters().size()), false)
    {
        reset();
        processor.addListener(this);
    }
    ~EditHistory() override { processor.removeListener(this); }

    void changed() noexcept
    {
        if (! onMessageThread() || replaying) return;
        dirty = true;
        lastChange = juce::Time::getMillisecondCounter();
    }
    void beginGroup()
    {
        if (! onMessageThread() || replaying) return;
        if (depth == 0) checkpoint();
        ++depth;
    }
    void endGroup() noexcept
    {
        if (! onMessageThread() || replaying || depth == 0) return;
        if (--depth == 0) checkpoint();
    }
    void poll() noexcept
    {
        if (! onMessageThread()) return;
        if (resetPending.exchange(false)) { reset(); return; }
        if (dirty && depth == 0
            && juce::Time::getMillisecondCounter() - lastChange >= 300u
            && ! juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
            checkpoint();
    }
    void checkpoint() noexcept
    {
        if (! onMessageThread() || replaying || depth != 0) return;
        if (resetPending.exchange(false)) { reset(); return; }
        try
        {
            auto next = captureState();
            auto key = comparisonKey(next);
            if (! entries.empty() && (! dirty || entries[cursor].key == key))
            {
                // Refresh runtime media without turning audio recording or
                // window resizing into a separate undo operation.
                bytes -= entries[cursor].data.getSize();
                entries[cursor].data = std::move(next);
                entries[cursor].key = std::move(key);
                bytes += entries[cursor].data.getSize();
                dirty = false;
                return;
            }
            while (entries.size() > cursor + 1)
            { bytes -= entries.back().data.getSize(); entries.pop_back(); }
            bytes += next.getSize();
            entries.push_back({std::move(next), std::move(key)});
            cursor = entries.size() - 1;
            while (entries.size() > 1 && (entries.size() > 101 || bytes > maximumBytes))
            { bytes -= entries.front().data.getSize(); entries.erase(entries.begin()); --cursor; }
            dirty = false;
        }
        catch (...) { dirty = false; resetPending.store(true); }
    }
    bool canUndo() const noexcept { return ! resetPending.load() && (cursor > 0 || dirty); }
    bool canRedo() const noexcept { return ! resetPending.load() && ! dirty && cursor + 1 < entries.size(); }
    bool undo() { return travel(-1); }
    bool redo() { return travel(1); }
    void ignoreNextHostDisplayChange() noexcept { skipReplayNotification = true; }
    void requestReset() noexcept
    {
        if (onMessageThread() && replaying) return;
        resetPending.store(true);
    }

private:
    struct Entry { juce::MemoryBlock data, key; };
    static constexpr size_t maximumBytes = 64u * 1024u * 1024u;
    static bool onMessageThread() noexcept
    {
        auto* messages = juce::MessageManager::getInstanceWithoutCreating();
        return messages != nullptr && messages->isThisTheMessageThread();
    }
    static juce::MemoryBlock comparisonKey(const juce::MemoryBlock& data)
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
        if (xml == nullptr) return data;
        if (auto* other = xml->getChildByName("otherState"))
        { other->removeAttribute("editorWidth"); other->removeAttribute("editorHeight"); }
        juce::MemoryBlock result;
        juce::AudioProcessor::copyXmlToBinary(*xml, result);
        return result;
    }
    void reset() noexcept
    {
        try
        {
            auto data = captureState();
            auto key = comparisonKey(data);
            entries.clear(); bytes = data.getSize(); cursor = 0; depth = 0; dirty = false;
            std::fill(gestures.begin(), gestures.end(), false);
            entries.push_back({std::move(data), std::move(key)});
        }
        catch (...) { entries.clear(); bytes = cursor = 0; depth = 0; dirty = false; }
    }
    bool travel(int direction)
    {
        if (! onMessageThread() || replaying || depth != 0) return false;
        checkpoint();
        if (entries.empty() || (direction < 0 ? cursor == 0 : cursor + 1 >= entries.size())) return false;
        const auto target = direction < 0 ? cursor - 1 : cursor + 1;
        replaying = true;
        const juce::ScopeGuard finish {[this] { replaying = false; dirty = false; }};
        restoreState(entries[target].data);
        cursor = target;
        // Host restoration canonicalises historical numbers and A/B schema.
        // Adopt that complete representation so its next capture cannot
        // masquerade as a new edit and erase the remaining redo branch.
        auto restored = captureState();
        auto key = comparisonKey(restored);
        bytes -= entries[cursor].data.getSize();
        bytes += restored.getSize();
        entries[cursor] = {std::move(restored), std::move(key)};
        return true;
    }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override { changed(); }
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override
    {
        if (!onMessageThread()) return;
        if (skipReplayNotification && details.nonParameterStateChanged)
        { skipReplayNotification = false; return; }
        if (details.nonParameterStateChanged || details.programChanged) changed();
    }
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int index) override
    {
        if (! onMessageThread() || replaying || index < 0 || static_cast<size_t>(index) >= gestures.size()) return;
        if (! gestures[static_cast<size_t>(index)])
        { beginGroup(); gestures[static_cast<size_t>(index)] = true; }
    }
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int index) override
    {
        if (! onMessageThread() || replaying || index < 0 || static_cast<size_t>(index) >= gestures.size()) return;
        if (gestures[static_cast<size_t>(index)])
        { gestures[static_cast<size_t>(index)] = false; endGroup(); }
    }
    juce::AudioProcessor& processor;
    Capture captureState;
    Restore restoreState;
    std::vector<Entry> entries;
    std::vector<bool> gestures;
    size_t cursor = 0, bytes = 0;
    int depth = 0;
    bool dirty = false, replaying = false;
    bool skipReplayNotification = false;
    std::uint32_t lastChange = 0;
    std::atomic<bool> resetPending{false};
};
}
