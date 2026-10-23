#include "GranularAudioIO.h"
#include "../DSP/Clouds/CloudsEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <vector>
#if !JUCE_MAC
 #include <filesystem>
#endif
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#elif JUCE_MAC
 #include <stdio.h>
 #include <sys/stdio.h>
 #include <sys/stat.h>
 #include <cerrno>
#elif JUCE_LINUX
 #include <fcntl.h>
 #include <sys/syscall.h>
 #include <unistd.h>
 #include <cerrno>
#endif

namespace fire::granular_io
{
namespace
{
bool stopped(const std::atomic<bool>* flag) noexcept{return flag&&flag->load(std::memory_order_acquire);}
juce::Result cancelled(){return juce::Result::fail("The audio file operation was cancelled.");}
#if JUCE_MAC
std::string nativePath(const juce::File& file)
{return file.getFullPathName().toStdString();}
#else
std::filesystem::path nativePath(const juce::File& file)
{return std::filesystem::u8path(file.getFullPathName().toStdString());}
#endif
juce::Result validateInfo(const AudioInfo& info)
{
    if(!std::isfinite(info.sampleRate)||info.sampleRate<8000||info.sampleRate>384000)
        return juce::Result::fail("Choose audio sampled between 8 kHz and 384 kHz.");
    if(info.channels!=1&&info.channels!=2)return juce::Result::fail("Choose a mono or stereo audio file.");
    if(info.sourceFrames<=0 || !std::isfinite(info.durationSeconds) || info.durationSeconds<=0)
        return juce::Result::fail("The audio file is empty or has an invalid length.");
    return juce::Result::ok();
}
std::unique_ptr<juce::AudioFormatReader> openReader(const juce::File& file)
{juce::AudioFormatManager manager;manager.registerBasicFormats();return std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(file));}
AudioInfo readerInfo(const juce::AudioFormatReader& reader,const FileStamp& stamp)
{
    AudioInfo result;result.sampleRate=reader.sampleRate;result.channels=static_cast<int>(reader.numChannels);
    result.sourceFrames=reader.lengthInSamples;result.durationSeconds=static_cast<double>(reader.lengthInSamples)/reader.sampleRate;result.stamp=stamp;return result;
}
juce::Result validateContainerBounds(const juce::File& file,const std::atomic<bool>* cancel=nullptr)
{
    // JUCE's uncompressed WAV/AIFF readers zero-pad short physical reads and
    // may still report success. Reject truncated chunk containers explicitly
    // instead of importing missing payload bytes as a valid silent recording.
    juce::FileInputStream input(file);if(!input.openedOk())return juce::Result::fail("The audio file could not be opened.");
    char root[4]{},format[4]{};if(input.read(root,4)!=4)return juce::Result::fail("The audio file is truncated.");
    const bool aiff=std::memcmp(root,"FORM",4)==0;
    const bool rf64=std::memcmp(root,"RF64",4)==0||std::memcmp(root,"BW64",4)==0;
    if(!aiff&&!rf64&&std::memcmp(root,"RIFF",4)!=0)return juce::Result::ok();
    const auto declared=static_cast<std::uint32_t>(aiff?input.readIntBigEndian():input.readInt());
    if(input.read(format,4)!=4)return juce::Result::fail("The audio container header is truncated.");
    if(aiff?(std::memcmp(format,"AIFF",4)!=0&&std::memcmp(format,"AIFC",4)!=0):std::memcmp(format,"WAVE",4)!=0)
        return juce::Result::ok();
    const auto bytes=input.getTotalLength();
    if(bytes<12 || (!rf64&&(declared<4||static_cast<juce::uint64>(declared)>static_cast<juce::uint64>(bytes-8))))
        return juce::Result::fail("The audio container is incomplete.");
    juce::int64 end=rf64?bytes:static_cast<juce::int64>(declared)+8;
    juce::uint64 largeData=0;bool hasLargeData=false;
    for(int chunks=0;input.getPosition()<end;++chunks)
    {
        if(stopped(cancel))return cancelled();
        if(chunks>=65536 || end-input.getPosition()<8)return juce::Result::fail("The audio container has invalid chunks.");
        char tag[4]{};if(input.read(tag,4)!=4)return juce::Result::fail("The audio container is truncated.");
        juce::uint64 length=static_cast<std::uint32_t>(aiff?input.readIntBigEndian():input.readInt());
        const auto payload=input.getPosition();
        if(rf64 && std::memcmp(tag,"data",4)==0 && length==0xffffffffu)
        {if(!hasLargeData)return juce::Result::fail("The RF64 length is missing.");length=largeData;}
        if(length>static_cast<juce::uint64>(end-payload))return juce::Result::fail("An audio chunk extends beyond the file.");
        if(rf64&&std::memcmp(tag,"ds64",4)==0)
        {
            if(length<28)return juce::Result::fail("The RF64 header is truncated.");
            const auto riffSize=static_cast<juce::uint64>(input.readInt64());
            largeData=static_cast<juce::uint64>(input.readInt64());hasLargeData=true;
            if(riffSize>static_cast<juce::uint64>(bytes-8) || riffSize+8<static_cast<juce::uint64>(payload)+length)
                return juce::Result::fail("The RF64 container is incomplete.");
            end=static_cast<juce::int64>(riffSize)+8;
        }
        const auto next=payload+static_cast<juce::int64>(length);
        if(!input.setPosition(next<end?next+static_cast<juce::int64>(length&1u):next))return juce::Result::fail("The audio chunk could not be read.");
    }
    return juce::Result::ok();
}
bool sameFormat(const AudioInfo& first,const AudioInfo& second)
{return first.sampleRate==second.sampleRate&&first.channels==second.channels&&first.sourceFrames==second.sourceFrames;}
std::error_code publishAtomically(const juce::File& temporary,const juce::File& final,bool replace)
{
    const auto source=nativePath(temporary),destination=nativePath(final);
   #if JUCE_WINDOWS
    // No COPY_ALLOWED flag: a failed same-directory rename must leave the old
    // destination intact, never degrade into delete/copy across volumes.
    const DWORD flags=MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0);
    if(::MoveFileExW(source.c_str(),destination.c_str(),flags))return {};
    return {static_cast<int>(::GetLastError()),std::system_category()};
   #elif JUCE_MAC
    // Exclusive rename also works on common external volumes without hardlinks.
    if(::renamex_np(source.c_str(),destination.c_str(),replace?0:RENAME_EXCL)==0)return {};
    return {errno,std::generic_category()};
   #else
    if(replace)
    {std::error_code error;std::filesystem::rename(source,destination,error);return error;}
    #if JUCE_LINUX && defined(SYS_renameat2)
    constexpr unsigned noReplace=1u; // Linux RENAME_NOREPLACE.
    if(::syscall(SYS_renameat2,AT_FDCWD,source.c_str(),AT_FDCWD,destination.c_str(),noReplace)==0)return {};
    const int failure=errno;
    if(failure!=ENOSYS&&failure!=EINVAL&&failure!=EOPNOTSUPP)return {failure,std::generic_category()};
    #endif
    // Older kernels may lack exclusive rename. An atomic link is a safe
    // fallback where supported; failure never authorises replacing a new file.
    std::error_code error;std::filesystem::create_hard_link(source,destination,error);return error;
   #endif
}
}

juce::String supportedFilePatterns()
{juce::AudioFormatManager manager;manager.registerBasicFormats();return manager.getWildcardForAllFormats();}

juce::Result getFileStamp(const juce::File& file,FileStamp& output)
{
    FileStamp next;next.path=file.getLinkedTarget().getFullPathName();next.exists=file.exists();
    if(next.exists)
    {
        next.sizeBytes=file.getSize();next.modifiedMilliseconds=file.getLastModificationTime().toMilliseconds();
       #if JUCE_MAC
        // libc++ filesystem time/path support requires macOS 10.15. Keep the
        // existing 10.13 deployment target and retain nanosecond resolution
        // without multiplying an arbitrary filesystem timestamp into overflow.
        struct stat attributes{};
        if(::stat(nativePath(file).c_str(),&attributes)!=0)
            return juce::Result::fail("The audio file's modification time could not be read.");
        next.modifiedSeconds=static_cast<juce::int64>(attributes.st_mtimespec.tv_sec);
        next.modifiedTicks=static_cast<juce::int64>(attributes.st_mtimespec.tv_nsec);
       #else
        std::error_code error;
        const auto time=std::filesystem::last_write_time(nativePath(file),error);
        if(error)return juce::Result::fail("The audio file's modification time could not be read.");
        next.modifiedTicks=static_cast<juce::int64>(time.time_since_epoch().count());
       #endif
    }
    output=std::move(next);return juce::Result::ok();
}

juce::Result inspectFile(const juce::File& file,AudioInfo& output)
{
    FileStamp before,after;if(auto result=getFileStamp(file,before);result.failed())return result;
    if(!file.existsAsFile())return juce::Result::fail("Choose an existing audio file.");
    if(auto result=validateContainerBounds(file);result.failed())return result;
    auto reader=openReader(file);if(!reader)return juce::Result::fail("This audio format is unsupported or the file is damaged.");
    auto info=readerInfo(*reader,before);if(auto result=validateInfo(info);result.failed())return result;
    if(auto result=getFileStamp(file,after);result.failed())return result;
    if(before!=after)return juce::Result::fail("The audio file changed while it was being inspected. Select it again.");
    output=std::move(info);return juce::Result::ok();
}

juce::Result readWindow(const juce::File& file,const AudioInfo& expected,ImportOptions options,
                        ImportResult& output,const std::atomic<bool>* cancel)
{
    if(stopped(cancel))return cancelled();
    if(auto result=validateInfo(expected);result.failed())return result;
    if(options.startFrame<0 || options.startFrame>=expected.sourceFrames)
        return juce::Result::fail("The selected start is outside the audio file.");
    FileStamp before,after;if(auto result=getFileStamp(file,before);result.failed())return result;
    if(before!=expected.stamp || !before.exists)return juce::Result::fail("The audio file changed. Inspect it again before importing.");
    if(auto result=validateContainerBounds(file,cancel);result.failed())return result;
    auto reader=openReader(file);if(!reader)return juce::Result::fail("The audio file could not be decoded.");
    const auto actual=readerInfo(*reader,before);
    if(!sameFormat(actual,expected))return juce::Result::fail("The audio format or length changed. Select the file again.");
    const auto remaining=actual.sourceFrames-options.startFrame;
    const auto availableOutput=std::ceil(static_cast<long double>(remaining)*recordingSampleRate/actual.sampleRate);
    const int frames=static_cast<int>(std::clamp(availableOutput,1.0L,static_cast<long double>(effects::frozenRecordingFrames)));
    const bool unity=actual.sampleRate==recordingSampleRate;
    const int guard=unity?0:effects::frozen_audio::sourceGuardFrames(actual.sampleRate);
    if(guard<0 || guard>4096)return juce::Result::fail("The sample-rate conversion window is invalid.");
    const auto readStart=std::max<juce::int64>(0,options.startFrame-guard);
    const int prefix=static_cast<int>(options.startFrame-readStart);
    const auto wanted=static_cast<juce::int64>(std::ceil((frames-1)*actual.sampleRate/recordingSampleRate))+prefix+guard+1;
    const auto readCount64=std::min(actual.sourceFrames-readStart,wanted);
    constexpr int maximumReadFrames=effects::frozenRecordingFrames*12+8193;
    if(readCount64<=0 || readCount64>maximumReadFrames)return juce::Result::fail("The selected decode window exceeds its memory limit.");
    const int readCount=static_cast<int>(readCount64);
    juce::AudioBuffer<float> input(actual.channels,readCount);input.clear();
    for(int offset=0;offset<readCount;)
    {
        if(stopped(cancel))return cancelled();
        const int count=std::min(8192,readCount-offset);
        if(!reader->read(&input,offset,count,readStart+offset,true,actual.channels==2))
            return juce::Result::fail("The selected audio window could not be read.");
        offset+=count;
    }
    ImportResult next;next.sourceStartFrame=options.startFrame;next.outputFrames=frames;
    next.selectedSeconds=std::min(maximumDuration,static_cast<double>(remaining)/actual.sampleRate);
    next.normalised=options.normalise;
    double peak=0;
    for(int channel=0;channel<actual.channels;++channel)
        for(int frame=0;frame<readCount;++frame)
        {
            auto& sample=input.getWritePointer(channel)[frame];
            if(!std::isfinite(sample)){sample=0;++next.nonFiniteSamples;}
            peak=std::max(peak,std::abs(static_cast<double>(sample)));
        }
    const double scale=std::max(1.0,peak);
    if(scale>1)for(int channel=0;channel<actual.channels;++channel)
        for(int frame=0;frame<readCount;++frame)input.getWritePointer(channel)[frame]=static_cast<float>(input.getSample(channel,frame)/scale);
    juce::AudioBuffer<float> converted(2,frames);
    if(unity)
    {
        converted.copyFrom(0,0,input,0,prefix,frames);
        converted.copyFrom(1,0,input,actual.channels==1?0:1,prefix,frames);
    }
    else if(!effects::frozen_audio::resampleWindow(input.getReadPointer(0),actual.channels==2?input.getReadPointer(1):nullptr,
        readCount,actual.sampleRate,prefix,converted.getWritePointer(0),converted.getWritePointer(1),frames,cancel))
        return stopped(cancel)?cancelled():juce::Result::fail("Sample-rate conversion failed.");
    if(stopped(cancel))return cancelled();
    for(int channel=0;channel<2;++channel)for(int frame=0;frame<frames;++frame)
    {
        const auto sample=converted.getSample(channel,frame);
        if(!std::isfinite(sample))return juce::Result::fail("Sample-rate conversion produced invalid audio.");
        next.sourcePeak=std::max(next.sourcePeak,std::abs(static_cast<double>(sample)*scale));
    }
    const double gain=options.normalise&&next.sourcePeak>0?std::pow(10.0,-1.0/20.0)/next.sourcePeak:1.0;
    auto recording=std::make_shared<effects::FrozenRecording>();recording->validFrames=frames;
    recording->head=frames==effects::frozenRecordingFrames?0:frames;
    for(int channel=0;channel<2;++channel)for(int frame=0;frame<frames;++frame)
    {
        const double value=static_cast<double>(converted.getSample(channel,frame))*scale*gain;
        if(value<-1 || value>32767.0/32768.0)++next.clippedSamples;
        recording->samples[static_cast<size_t>(channel)][static_cast<size_t>(frame)]=static_cast<std::int16_t>(
            std::llround(std::clamp(value*32768.0,-32768.0,32767.0)));
    }
    reader.reset();
    if(auto result=getFileStamp(file,after);result.failed())return result;
    if(before!=after)return juce::Result::fail("The audio file changed during import. Select it again.");
    if(stopped(cancel))return cancelled();
    next.recording=std::move(recording);output=std::move(next);return juce::Result::ok();
}

juce::File resolveWavePath(const juce::File& path)
{return path.hasFileExtension("wav")?path:juce::File(path.getFullPathName()+".wav");}

juce::Result exportWaveAtomic(const juce::File& finalPath,const effects::FrozenRecording& recording,
                             bool overwriteConfirmed,const std::atomic<bool>* cancel,const FileStamp* expectedDestination)
{
    if(stopped(cancel))return cancelled();
    if(!recording.isValid())return juce::Result::fail("There is no valid frozen audio to export.");
    if(finalPath==juce::File() || !finalPath.hasFileExtension("wav"))return juce::Result::fail("Resolve the final WAV filename before exporting.");
    if(finalPath.isSymbolicLink() || finalPath.isDirectory() || !finalPath.getParentDirectory().isDirectory())
        return juce::Result::fail("Choose a writable WAV file location.");
    FileStamp before,after;if(auto result=getFileStamp(finalPath,before);result.failed())return result;
    if(expectedDestination && *expectedDestination!=before)return juce::Result::fail("The export destination changed after it was selected.");
    if(before.exists&&!overwriteConfirmed)return juce::Result::fail("Confirm overwriting the existing WAV file first.");
    if(before.exists&&!finalPath.hasWriteAccess())return juce::Result::fail("The destination file is read-only.");
    juce::TemporaryFile temporary(finalPath);
    std::unique_ptr<juce::OutputStream> stream=temporary.getFile().createOutputStream();
    if(!stream)return juce::Result::fail("The temporary WAV file could not be created.");
    auto* fileStream=dynamic_cast<juce::FileOutputStream*>(stream.get());
    juce::WavAudioFormat wav;
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(recordingSampleRate).withNumChannels(2).withBitsPerSample(16));
    if(!writer)return juce::Result::fail("The WAV encoder could not be created.");
    const int first=(recording.head-recording.validFrames+effects::frozenRecordingFrames)%effects::frozenRecordingFrames;
    std::vector<int> left(static_cast<size_t>(recording.validFrames)),right(left.size());
    for(int frame=0;frame<recording.validFrames;++frame)
    {
        const auto ring=static_cast<size_t>((first+frame)%effects::frozenRecordingFrames);
        left[static_cast<size_t>(frame)]=static_cast<int>(recording.samples[0][ring])*65536;
        right[static_cast<size_t>(frame)]=static_cast<int>(recording.samples[1][ring])*65536;
    }
    for(int offset=0;offset<recording.validFrames;)
    {
        if(stopped(cancel))return cancelled();
        const int count=std::min(8192,recording.validFrames-offset);
        const int* channels[]{left.data()+offset,right.data()+offset,nullptr};
        if(!writer->write(channels,count))return juce::Result::fail("The WAV audio could not be written.");
        offset+=count;
    }
    if(!writer->flush())return juce::Result::fail("The WAV file could not be finalised.");
    if(fileStream){fileStream->flush();if(fileStream->getStatus().failed())return juce::Result::fail("The WAV file could not be flushed.");}
    writer.reset();
    auto verify=openReader(temporary.getFile());
    if(!verify || verify->sampleRate!=recordingSampleRate || verify->numChannels!=2 || verify->bitsPerSample!=16
        || verify->lengthInSamples!=recording.validFrames || verify->usesFloatingPointData)
        return juce::Result::fail("The completed WAV file failed verification.");
    std::vector<int> checkLeft(left.size()),checkRight(right.size());int* channels[]{checkLeft.data(),checkRight.data()};
    if(!verify->read(channels,2,0,recording.validFrames,false)||checkLeft!=left||checkRight!=right)
        return juce::Result::fail("The exported PCM did not match the frozen recording.");
    verify.reset();
    if(auto result=getFileStamp(finalPath,after);result.failed())return result;
    if(before!=after || finalPath.isSymbolicLink())return juce::Result::fail("The export destination changed while the WAV was being written.");
    if(stopped(cancel))return cancelled();
    const auto error=publishAtomically(temporary.getFile(),finalPath,before.exists);
    if(error)return juce::Result::fail("The WAV file could not be published atomically: "+juce::String::fromUTF8(error.message().c_str()));
    return juce::Result::ok();
}
} // namespace fire::granular_io
