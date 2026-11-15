#pragma once
#include "../Utility/GranularAudioIO.h"
#include <functional>
#include <memory>

namespace fire::ui
{
struct GranularWavePreview
{
    static constexpr int points=256;
    std::array<std::array<float,points>,2> low{},high{};
    static GranularWavePreview from(const effects::FrozenRecordingPtr& recording)
    {
        GranularWavePreview result;
        if(!recording || recording->validFrames<=0)return result;
        const int count=recording->validFrames;
        const int first=(recording->head-count+effects::frozenRecordingFrames)%effects::frozenRecordingFrames;
        for(int channel=0;channel<2;++channel)
            for(int point=0;point<points;++point)
            {
                float low=1,high=-1;
                const int begin=point*count/points,end=juce::jmin(count,juce::jmax(begin+1,(point+1)*count/points));
                for(int index=begin;index<end;++index)
                {
                    const float sample=recording->samples[static_cast<size_t>(channel)][static_cast<size_t>((first+index)%effects::frozenRecordingFrames)]/32768.f;
                    low=juce::jmin(low,sample);high=juce::jmax(high,sample);
                }
                result.low[static_cast<size_t>(channel)][static_cast<size_t>(point)]=low;
                result.high[static_cast<size_t>(channel)][static_cast<size_t>(point)]=high;
            }
        return result;
    }
};

// Workers own only immutable media/path inputs and their result. No UI or
// processor pointer crosses threads; message-thread polling consumes results.
struct GranularMediaJob
{
    enum class Kind {inspect,preview,destination,write};
    explicit GranularMediaJob(Kind type,std::uint64_t request):kind(type),generation(request){}
    Kind kind;std::uint64_t generation;
    std::atomic<bool> cancelled{false},done{false};
    juce::Result status=juce::Result::fail("Pending");
    granular_io::AudioInfo info;
    granular_io::ImportResult imported;
    granular_io::FileStamp destination;
    juce::File file;
    effects::FrozenRecordingPtr snapshot;
    GranularWavePreview wave;
};
class GranularMediaWorkers
{
public:
    using Task=std::function<void(GranularMediaJob&)>;
    static void submit(std::shared_ptr<GranularMediaJob> state,Task task)
    {pool().addJob(new Job(std::move(state),std::move(task)),true);}
private:
    class Job final:public juce::ThreadPoolJob
    {
    public:
        Job(std::shared_ptr<GranularMediaJob> state,Task task):juce::ThreadPoolJob("Fire granular audio"),state(std::move(state)),task(std::move(task)){}
        JobStatus runJob()override
        {
            const juce::ScopeGuard complete{[this]{state->done.store(true,std::memory_order_release);}};
            if(state->cancelled.load(std::memory_order_acquire))return jobHasFinished;
            try{task(*state);}catch(...){state->status=juce::Result::fail("The audio file operation failed.");}
            return jobHasFinished;
        }
    private:std::shared_ptr<GranularMediaJob> state;Task task;
    };
    static juce::ThreadPool& pool(){static juce::ThreadPool workers(2);return workers;}
};
}
