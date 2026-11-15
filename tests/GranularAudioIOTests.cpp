#include <Utility/GranularAudioIO.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
namespace io=fire::granular_io;
using Recording=fire::effects::FrozenRecording;
constexpr int capacity=fire::effects::frozenRecordingFrames;
struct AudioFolder
{
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fire-audio-io-"+juce::Uuid().toString());
    AudioFolder(){REQUIRE(path.createDirectory().wasOk());}
    ~AudioFolder(){path.deleteRecursively();}
};
void writeFloats(const juce::File& file,double rate,const juce::AudioBuffer<float>& samples)
{
    std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();REQUIRE(stream);
    juce::WavAudioFormat format;
    auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(rate)
        .withNumChannels(samples.getNumChannels()).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    REQUIRE(writer);REQUIRE(writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()));REQUIRE(writer->flush());
}
io::AudioInfo audioInfo(const juce::File& file)
{io::AudioInfo info;REQUIRE(io::inspectFile(file,info).wasOk());return info;}
io::ImportResult importAudio(const juce::File& file,juce::int64 start=0,bool normalise=false)
{io::ImportResult result;REQUIRE(io::readWindow(file,audioInfo(file),{start,normalise},result).wasOk());REQUIRE(result.recording);return result;}
Recording makeRecording(int frames,int head)
{
    Recording result;result.validFrames=frames;result.head=head;
    for(int channel=0;channel<2;++channel)for(int i=0;i<capacity;++i)
        result.samples[static_cast<size_t>(channel)][static_cast<size_t>(i)]=static_cast<std::int16_t>((i*173+channel*1307)%65536-32768);
    result.samples[0][0]=-32768;result.samples[1][0]=32767;return result;
}
}

TEST_CASE("Frozen WAV export unwraps full and partial rings and exact 32k PCM reimports unchanged",
          "[granular-io][wav][roundtrip]")
{
    AudioFolder folder;
    for(const auto [frames,head]:{std::pair{1,1},std::pair{73,73},std::pair{capacity,0},std::pair{capacity,237}})
    {
        CAPTURE(frames,head);const auto original=makeRecording(frames,head);
        const auto file=folder.path.getChildFile("ring-"+juce::String(frames)+"-"+juce::String(head)+".wav");
        REQUIRE(io::exportWaveAtomic(file,original,false).wasOk());
        const auto info=audioInfo(file);CHECK(info.sampleRate==32000);CHECK(info.channels==2);CHECK(info.sourceFrames==frames);
        const auto imported=importAudio(file);CHECK(imported.outputFrames==frames);CHECK(imported.clippedSamples==0);CHECK(imported.nonFiniteSamples==0);
        CHECK(imported.recording->head==(frames==capacity?0:frames));
        const int first=(head-frames+capacity)%capacity;
        for(int channel=0;channel<2;++channel)for(int i=0;i<frames;++i)
            CHECK(imported.recording->samples[static_cast<size_t>(channel)][static_cast<size_t>(i)]
                  ==original.samples[static_cast<size_t>(channel)][static_cast<size_t>((first+i)%capacity)]);
        for(int channel=0;channel<2;++channel)for(int i=frames;i<capacity;++i)
            CHECK(imported.recording->samples[static_cast<size_t>(channel)][static_cast<size_t>(i)]==0);
    }
}

TEST_CASE("Mono PCM import duplicates channels and normalisation preserves stereo balance",
          "[granular-io][mono][normalise]")
{
    AudioFolder folder;const auto mono=folder.path.getChildFile("mono.wav");
    {
        std::unique_ptr<juce::OutputStream> stream=mono.createOutputStream();REQUIRE(stream);
        auto writer=juce::WavAudioFormat{}.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(32000).withNumChannels(1).withBitsPerSample(16));REQUIRE(writer);
        const std::array<int,5> pcm{-32768*65536,-100*65536,0,1*65536,32767*65536};const int* channels[]{pcm.data(),nullptr};
        REQUIRE(writer->write(channels,5));REQUIRE(writer->flush());
    }
    const auto imported=importAudio(mono);REQUIRE(imported.outputFrames==5);
    const std::array<int,5> expected{-32768,-100,0,1,32767};
    for(int i=0;i<5;++i){CHECK(imported.recording->samples[0][static_cast<size_t>(i)]==expected[static_cast<size_t>(i)]);CHECK(imported.recording->samples[1][static_cast<size_t>(i)]==expected[static_cast<size_t>(i)]);}
    juce::AudioBuffer<float> stereo(2,17);for(int i=0;i<17;++i){stereo.setSample(0,i,.5f);stereo.setSample(1,i,.125f);}
    const auto file=folder.path.getChildFile("stereo.wav");writeFloats(file,32000,stereo);
    const auto normalised=importAudio(file,0,true);CHECK(normalised.normalised);CHECK(normalised.clippedSamples==0);
    const auto peak=static_cast<int>(std::llround(std::pow(10.0,-1.0/20.0)*32768));
    CHECK(normalised.recording->samples[0][0]==peak);CHECK(std::abs(normalised.recording->samples[1][0]*4-peak)<=2);
}

TEST_CASE("Audio import sanitises nonfinite samples and bounds extreme float input without clipping normalised audio",
          "[granular-io][normalise][non-finite]")
{
    AudioFolder folder;juce::AudioBuffer<float> samples(1,8);samples.clear();
    samples.setSample(0,0,2);samples.setSample(0,1,-2);samples.setSample(0,2,std::numeric_limits<float>::quiet_NaN());
    samples.setSample(0,3,std::numeric_limits<float>::infinity());samples.setSample(0,4,-std::numeric_limits<float>::infinity());samples.setSample(0,5,std::numeric_limits<float>::max());
    const auto file=folder.path.getChildFile("hot.wav");writeFloats(file,32000,samples);
    const auto raw=importAudio(file);CHECK(raw.nonFiniteSamples==3);CHECK(raw.clippedSamples==6);
    CHECK(raw.recording->samples[0][0]==32767);CHECK(raw.recording->samples[0][1]==-32768);CHECK(raw.recording->samples[0][5]==32767);
    for(int i=2;i<=4;++i)CHECK(raw.recording->samples[0][static_cast<size_t>(i)]==0);
    const auto normalised=importAudio(file,0,true);CHECK(normalised.clippedSamples==0);CHECK(std::isfinite(normalised.sourcePeak));
    CHECK(normalised.recording->samples[0][5]==static_cast<int>(std::llround(std::pow(10.0,-1.0/20.0)*32768)));
    samples.clear();const auto silence=folder.path.getChildFile("silence.wav");writeFloats(silence,32000,samples);
    const auto quiet=importAudio(silence,0,true);CHECK(quiet.sourcePeak==0);CHECK(quiet.clippedSamples==0);CHECK(quiet.recording->samples[0][0]==0);
}

TEST_CASE("Selected windows resample common and extreme rates with sample aligned starts and finite lengths",
          "[granular-io][src][window]")
{
    AudioFolder folder;
    for(const double rate:{8000.0,44100.0,48000.0,96000.0,192000.0,384000.0})
    {
        CAPTURE(rate);const int length=static_cast<int>(rate*1.5);juce::AudioBuffer<float> samples(1,length);
        for(int i=0;i<length;++i)samples.setSample(0,i,static_cast<float>(.25*std::sin(juce::MathConstants<double>::twoPi*997*i/rate)));
        const auto file=folder.path.getChildFile(juce::String(static_cast<int>(rate))+".wav");writeFloats(file,rate,samples);
        const auto start=static_cast<juce::int64>(rate*.23)+7;const auto imported=importAudio(file,start);
        REQUIRE(imported.outputFrames==capacity);CHECK(imported.sourceStartFrame==start);CHECK(imported.selectedSeconds==Catch::Approx(io::maximumDuration));
        for(int i:{0,1,37,1000,16000,32695})
        {
            const auto expected=.25*std::sin(juce::MathConstants<double>::twoPi*997*(static_cast<double>(start)/rate+i/32000.0));
            CHECK(imported.recording->samples[0][static_cast<size_t>(i)]/32768.0==Catch::Approx(expected).margin(.003));
            CHECK(imported.recording->samples[1][static_cast<size_t>(i)]==imported.recording->samples[0][static_cast<size_t>(i)]);
        }
        const auto shortWindow=importAudio(file,length-1);CHECK(shortWindow.outputFrames==static_cast<int>(std::ceil(32000.0/rate)));
        CHECK(shortWindow.recording->isValid());
    }
}

TEST_CASE("Downsampling rejects ultrasonic energy instead of aliasing it into the frozen recording",
          "[granular-io][src][antialias]")
{
    AudioFolder folder;
    for(const double rate:{96000.0,192000.0,384000.0})
    {
        CAPTURE(rate);juce::AudioBuffer<float> input(1,static_cast<int>(rate*.2));
        for(int i=0;i<input.getNumSamples();++i)input.setSample(0,i,static_cast<float>(.6*std::sin(juce::MathConstants<double>::twoPi*30000*i/rate)));
        const auto file=folder.path.getChildFile("ultrasonic-"+juce::String(static_cast<int>(rate))+".wav");writeFloats(file,rate,input);
        const auto result=importAudio(file);double energy=0;int count=0;
        for(int i=128;i<result.outputFrames-128;++i){const auto value=result.recording->samples[0][static_cast<size_t>(i)]/32768.0;energy+=value*value;++count;}
        REQUIRE(count>0);CHECK(std::sqrt(energy/count)<.002);
    }
}

TEST_CASE("Inspect and window failures leave caller state unchanged including cancellation and changed files",
          "[granular-io][validation][cancel][file-stamp]")
{
    AudioFolder folder;juce::AudioBuffer<float> samples(1,128);samples.clear();const auto file=folder.path.getChildFile("source.wav");writeFloats(file,32000,samples);
    const auto info=audioInfo(file);io::ImportResult sentinel;sentinel.outputFrames=777;
    for(const auto start:{juce::int64{-1},info.sourceFrames,std::numeric_limits<juce::int64>::max()})
    {CHECK(io::readWindow(file,info,{start,false},sentinel).failed());CHECK(sentinel.outputFrames==777);}
    std::atomic<bool> cancel{true};CHECK(io::readWindow(file,info,{},sentinel,&cancel).failed());CHECK(sentinel.outputFrames==777);
    REQUIRE(file.replaceWithText("changed"));CHECK(io::readWindow(file,info,{},sentinel).failed());CHECK(sentinel.outputFrames==777);
    io::AudioInfo untouched;untouched.channels=77;CHECK(io::inspectFile(file,untouched).failed());CHECK(untouched.channels==77);
    const auto unsupported=folder.path.getChildFile("surround.wav");juce::AudioBuffer<float> surround(3,16);surround.clear();writeFloats(unsupported,32000,surround);
    CHECK(io::inspectFile(unsupported,untouched).failed());CHECK(untouched.channels==77);
    const auto lowRate=folder.path.getChildFile("low-rate.wav");writeFloats(lowRate,4000,samples);CHECK(io::inspectFile(lowRate,untouched).failed());
    const auto empty=folder.path.getChildFile("empty.wav");juce::AudioBuffer<float> emptySamples(1,0);writeFloats(empty,32000,emptySamples);
    CHECK(io::inspectFile(empty,untouched).failed());CHECK(untouched.channels==77);
    const auto truncated=folder.path.getChildFile("truncated.wav");writeFloats(truncated,32000,samples);
    {auto stream=truncated.createOutputStream();REQUIRE(stream);REQUIRE(stream->setPosition(truncated.getSize()-3));REQUIRE(stream->truncate().wasOk());}
    CHECK(io::inspectFile(truncated,untouched).failed());CHECK(untouched.channels==77);
    CHECK(io::supportedFilePatterns().containsIgnoreCase("*.wav"));
}

TEST_CASE("WAV export resolves suffixes before confirmation and preserves existing destinations on failure",
          "[granular-io][wav][atomic][cancel]")
{
    AudioFolder folder;const auto path=io::resolveWavePath(folder.path.getChildFile("colour.v1"));CHECK(path.getFileName()=="colour.v1.wav");CHECK(io::resolveWavePath(path)==path);
    const auto recording=makeRecording(17,17);REQUIRE(path.replaceWithText("existing destination"));
    io::FileStamp selected;REQUIRE(io::getFileStamp(path,selected).wasOk());
    CHECK(io::exportWaveAtomic(path,recording,false).failed());CHECK(path.loadFileAsString()=="existing destination");
    std::atomic<bool> cancel{true};CHECK(io::exportWaveAtomic(path,recording,true,&cancel,&selected).failed());CHECK(path.loadFileAsString()=="existing destination");
    Recording invalid;CHECK(io::exportWaveAtomic(path,invalid,true).failed());CHECK(path.loadFileAsString()=="existing destination");
    REQUIRE(path.replaceWithText("another editor changed the destination"));CHECK(io::exportWaveAtomic(path,recording,true,nullptr,&selected).failed());
    CHECK(path.loadFileAsString()=="another editor changed the destination");
    REQUIRE(io::getFileStamp(path,selected).wasOk());REQUIRE(io::exportWaveAtomic(path,recording,true,nullptr,&selected).wasOk());CHECK(audioInfo(path).sourceFrames==17);
    CHECK(io::exportWaveAtomic(folder.path.getChildFile("missing/output.wav"),recording,false).failed());
    CHECK(io::exportWaveAtomic(folder.path.getChildFile("ambiguous"),recording,false).failed());
    CHECK(folder.path.findChildFiles(juce::File::findFiles,false,"*").size()==1);
}

#if JUCE_MAC || JUCE_LINUX
TEST_CASE("A very long sparse source imports only the selected bounded final window",
          "[granular-io][window][bounded-file]")
{
    AudioFolder folder;const auto file=folder.path.getChildFile("long.wav");
    constexpr int frames=1<<28;constexpr int dataBytes=frames*2;
    {
        auto stream=file.createOutputStream();REQUIRE(stream);
        stream->write("RIFF",4);stream->writeInt(dataBytes+36);stream->write("WAVEfmt ",8);stream->writeInt(16);
        stream->writeShort(1);stream->writeShort(1);stream->writeInt(32000);stream->writeInt(64000);stream->writeShort(2);stream->writeShort(16);
        stream->write("data",4);stream->writeInt(dataBytes);REQUIRE(stream->setPosition(44LL+dataBytes-2));REQUIRE(stream->writeShort(1234));stream->flush();REQUIRE(stream->getStatus().wasOk());
    }
    const auto info=audioInfo(file);REQUIRE(info.sourceFrames==frames);
    const auto result=importAudio(file,info.sourceFrames-capacity);REQUIRE(result.outputFrames==capacity);
    CHECK(result.recording->samples[0][capacity-1]==1234);CHECK(result.recording->samples[1][capacity-1]==1234);
    CHECK(result.recording->samples[0][0]==0);
}
#endif
