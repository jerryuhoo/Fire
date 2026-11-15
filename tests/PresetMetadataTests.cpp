#include <PluginProcessor.h>
#include <Utility/PresetMetadata.h>
#include <Utility/FactoryPresets.h>
#include <Utility/FrozenAudioState.h>
#include <catch2/catch_test_macros.hpp>
#include <string>

namespace
{
using Metadata=state::StatePresets::Metadata;
struct NotesFolder
{
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fire-notes-"+juce::Uuid().toString());
    explicit NotesFolder(FireAudioProcessor& p){REQUIRE(path.createDirectory().wasOk());p.statePresets.setPresetDirectoryForTesting(path);}
    ~NotesFolder(){path.deleteRecursively();}
};
juce::XmlElement notesPreset(FireAudioProcessor& p)
{juce::XmlElement xml("WINGSFIRE");state::saveStateToXml(p,xml);return xml;}
juce::File notesFile(FireAudioProcessor& p,NotesFolder& folder,const juce::String& name="Notes.fire")
{
    auto xml=notesPreset(p);xml.setAttribute("presetName",name.upToLastOccurrenceOf(".fire",false,false));
    const auto file=folder.path.getChildFile(name);REQUIRE(xml.writeTo(file));p.statePresets.scanAllPresets();return file;
}
Metadata readNotes(state::StatePresets& library,const juce::String& key)
{Metadata result;REQUIRE(library.readPresetMetadata(key,result).wasOk());return result;}
void stripNotes(juce::XmlElement& xml){xml.removeAttribute("presetTags");xml.removeAttribute("presetDescription");}
state::StatePresets::BrowserEntry notesEntry(state::StatePresets& library,const juce::String& key)
{for(const auto& entry:library.getBrowserEntries(true))if(entry.key==key)return entry;FAIL("Preset entry missing");return {};}
}

TEST_CASE("Editing user notes preserves the entire sound XML and frozen recording without changing live state",
          "[preset-metadata][preset][state][frozen-audio]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;NotesFolder folder(p);
    REQUIRE(p.addInsertEffect(0,fire::effects::Type::granular)==0);
    auto xml=notesPreset(p);xml.setAttribute("presetName","Notes");xml.setAttribute("externalUnknownAttribute","preserve me");
    fire::effects::FrozenRecordings media{};auto recording=std::make_shared<fire::effects::FrozenRecording>();
    recording->head=recording->validFrames=73;recording->samples[0][3]=12345;recording->samples[1][37]=-23456;media[0][0]=recording;
    fire::effects::writeFrozenAudioState(xml,media);
    const auto file=folder.path.getChildFile("Notes.fire");REQUIRE(xml.writeTo(file));p.statePresets.scanAllPresets();
    const auto before=notesPreset(p);auto notes=readNotes(p.statePresets,"Notes.fire");
    notes.tags={juce::String::fromUTF8("  温暖 "),"Tape","tape","comma,tag","\"quote\""};
    notes.description=juce::String::fromUTF8("  两行说明\r\nTape colour.  ");
    REQUIRE(p.statePresets.writePresetMetadata("Notes.fire",notes,notes.revision).wasOk());
    const auto actual=readNotes(p.statePresets,"Notes.fire");
    CHECK((actual.tags==juce::StringArray{juce::String::fromUTF8("温暖"),"Tape","comma,tag","\"quote\""}));
    CHECK(actual.description==juce::String::fromUTF8("两行说明\nTape colour."));
    CHECK(actual.tags[0].length()==2);CHECK(actual.tags[0][0]==0x6e29u);
    CHECK(actual.revision!=notes.revision);CHECK_FALSE(actual.factory);
    auto after=juce::XmlDocument::parse(file);REQUIRE(after);stripNotes(*after);stripNotes(xml);CHECK(after->toString()==xml.toString());
    CHECK(p.isCurrentStateEquivalentToPreset(before));
    const auto entry=notesEntry(p.statePresets,"Notes.fire");CHECK(entry.tags==actual.tags);CHECK(entry.description==actual.description);
    CHECK(state::canLoadStateFromXml(*after,p));
}

TEST_CASE("Preset metadata bounds and stale revisions leave files unchanged",
          "[preset-metadata][validation][filesystem]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;NotesFolder folder(p);const auto file=notesFile(p,folder);
    const auto original=file.loadFileAsString();const auto valid=readNotes(p.statePresets,"Notes.fire");
    auto reject=[&](const Metadata& bad,const juce::String& revision){CHECK(p.statePresets.writePresetMetadata("Notes.fire",bad,revision).failed());CHECK(file.loadFileAsString()==original);};
    auto bad=valid;bad.description=juce::String::repeatedString(juce::String::fromUTF8("文"),2049);reject(bad,valid.revision);
    bad=valid;bad.tags.add(juce::String::repeatedString("t",33));reject(bad,valid.revision);
    bad=valid;for(int i=0;i<17;++i)bad.tags.add(juce::String(i));reject(bad,valid.revision);
    bad=valid;bad.tags={"split\ntag"};reject(bad,valid.revision);
    bad=valid;bad.description=juce::String::charToString(1);reject(bad,valid.revision);
    bad=valid;bad.description="Edited";reject(bad,{});reject(bad,"stale");
    Metadata unchanged;unchanged.description="sentinel";
    CHECK(p.statePresets.readPresetMetadata("../outside.fire",unchanged).failed());CHECK(unchanged.description=="sentinel");
    CHECK(p.statePresets.readPresetMetadata("@factory/missing",unchanged).failed());CHECK(unchanged.description=="sentinel");
}

TEST_CASE("Independent library instances reject stale metadata after a sound or notes edit",
          "[preset-metadata][revision][filesystem]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor a,b;NotesFolder folder(a);const auto file=notesFile(a,folder);
    b.statePresets.setPresetDirectoryForTesting(folder.path);
    const auto stale=readNotes(a.statePresets,"Notes.fire");auto update=readNotes(b.statePresets,"Notes.fire");update.description="New notes";
    REQUIRE(b.statePresets.writePresetMetadata("Notes.fire",update,update.revision).wasOk());
    auto old=stale;old.description="Overwrite";CHECK(a.statePresets.writePresetMetadata("Notes.fire",old,stale.revision).failed());
    auto current=readNotes(a.statePresets,"Notes.fire");auto changed=juce::XmlDocument::parse(file);REQUIRE(changed);changed->setAttribute("drive1",.8f);REQUIRE(changed->writeTo(file));
    current.description="Later notes";CHECK(a.statePresets.writePresetMetadata("Notes.fire",current,current.revision).failed());
    CHECK(readNotes(a.statePresets,"Notes.fire").description=="New notes");
}

TEST_CASE("Factory metadata overrides survive preference compaction hide restore and reset independently",
          "[preset-metadata][factory-presets][preferences]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;NotesFolder folder(p);p.statePresets.enableFactoryPresets();
    const auto key="@factory/"+fire::factory::definitions.front().key;const auto original=readNotes(p.statePresets,key);
    auto edited=original;edited.description="";edited.tags={juce::String::fromUTF8("收藏的声音"),"My tag"};
    REQUIRE(p.statePresets.writePresetMetadata(key,edited,edited.revision).wasOk());
    auto actual=readNotes(p.statePresets,key);CHECK(actual.factory);CHECK(actual.hasOverride);CHECK(actual.description.isEmpty());
    CHECK(actual.tags==edited.tags);CHECK(p.statePresets.getPresetXml().toString().contains(original.description));
    REQUIRE(p.statePresets.setPresetFavourite(key,true));REQUIRE(p.statePresets.setPresetFavourite(key,false));
    CHECK(readNotes(p.statePresets,key).tags==edited.tags);REQUIRE(p.statePresets.removeBrowserPreset(key));
    CHECK(notesEntry(p.statePresets,key).tags==edited.tags);REQUIRE(p.statePresets.restoreBrowserPreset(key));
    p.statePresets.setCurrentPresetKey(key);CHECK(p.statePresets.getCurrentPresetDescription().isEmpty());
    CHECK(p.statePresets.resetFactoryMetadata(key,original.revision).failed());
    REQUIRE(p.statePresets.setPresetFavourite(key,true));
    REQUIRE(p.statePresets.resetFactoryMetadata(key,actual.revision).wasOk());
    actual=readNotes(p.statePresets,key);CHECK_FALSE(actual.hasOverride);CHECK(actual.description==original.description);
    CHECK(notesEntry(p.statePresets,key).favourite);CHECK(actual.tags.isEmpty());
    FireAudioProcessor other;other.statePresets.setPresetDirectoryForTesting(folder.path);other.statePresets.enableFactoryPresets();
    CHECK(readNotes(other.statePresets,key).description==original.description);
}

TEST_CASE("Archived user files retain notes and cannot be edited until restored",
          "[preset-metadata][recycle][filesystem]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;NotesFolder folder(p);notesFile(p,folder);
    auto edited=readNotes(p.statePresets,"Notes.fire");edited.tags={"archive"};edited.description="Keep these notes";
    REQUIRE(p.statePresets.writePresetMetadata("Notes.fire",edited,edited.revision).wasOk());
    const auto current=readNotes(p.statePresets,"Notes.fire");REQUIRE(p.statePresets.removeBrowserPreset("Notes.fire"));
    const auto entry=notesEntry(p.statePresets,"Notes.fire");CHECK(entry.removed);CHECK(entry.tags==edited.tags);CHECK(entry.description==edited.description);
    CHECK(p.statePresets.writePresetMetadata("Notes.fire",edited,current.revision).failed());
    REQUIRE(p.statePresets.restoreBrowserPreset("Notes.fire"));CHECK(readNotes(p.statePresets,"Notes.fire").tags==edited.tags);
    CHECK(readNotes(p.statePresets,"Notes.fire").description==edited.description);
}

TEST_CASE("Saving changed sounds keeps existing notes and Save As inherits selected notes",
          "[preset-metadata][save][filesystem]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;NotesFolder folder(p);const auto file=notesFile(p,folder);
    auto edited=readNotes(p.statePresets,"Notes.fire");edited.tags={"keep"};edited.description="Original notes";
    REQUIRE(p.statePresets.writePresetMetadata("Notes.fire",edited,edited.revision).wasOk());
    p.treeState.getParameter("drive1")->setValueNotifyingHost(.6f);
    REQUIRE(p.statePresets.savePreset(file,true).isNotEmpty());CHECK(readNotes(p.statePresets,"Notes.fire").description==edited.description);
    p.statePresets.setCurrentPresetKey("Notes.fire");
    REQUIRE(p.statePresets.savePreset(folder.path.getChildFile("Variation.fire"),true).isNotEmpty());
    CHECK(readNotes(p.statePresets,"Variation.fire").tags==edited.tags);
}

TEST_CASE("Malformed stored tags cannot silently replace metadata or write outside the library",
          "[preset-metadata][validation][filesystem]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;NotesFolder folder(p);const auto file=notesFile(p,folder);
    const auto valid=readNotes(p.statePresets,"Notes.fire");
    for(const auto* tags:{"invalid","[42]","{}"})
    {
        auto xml=notesPreset(p);xml.setAttribute("presetTags",tags);REQUIRE(xml.writeTo(file));
        Metadata sentinel;sentinel.description="unchanged";CHECK(p.statePresets.readPresetMetadata("Notes.fire",sentinel).failed());CHECK(sentinel.description=="unchanged");
    }
    CHECK(p.statePresets.writePresetMetadata("../outside.fire",valid,valid.revision).failed());
    CHECK(p.statePresets.writePresetMetadata(file.getFullPathName(),valid,valid.revision).failed());
}

TEST_CASE("Preset notes reject invalid XML text before trimming and allow multiline descriptions",
          "[preset-metadata][validation][text]")
{
    namespace metadata=fire::preset_metadata;
    for(juce::juce_wchar c:{0u,1u,8u,11u,12u,31u,0xd800u,0xdfffu,0xfffeu,0xffffu,0x110000u})
        CHECK_FALSE(metadata::validXmlCodePoint(c));
    for(juce::juce_wchar c:{9u,10u,13u,32u,0xd7ffu,0xe000u,0xfffdu,0x10000u,0x10ffffu})
        CHECK(metadata::validXmlCodePoint(c));
    Metadata input,out;out.description="unchanged";
    input.tags={juce::String::charToString(11)+"tag"};
    CHECK(metadata::canonicalise(input,out).failed());CHECK(out.description=="unchanged");
    input.tags={"\ntag"};CHECK(metadata::canonicalise(input,out).failed());
    input.tags={juce::String::fromUTF8("合法")};input.description=juce::String::fromUTF8("首行\ttext\r\n第二行");
    REQUIRE(metadata::canonicalise(input,out).wasOk());CHECK(out.description==juce::String::fromUTF8("首行\ttext\n第二行"));
}

TEST_CASE("Real UTF8 preset metadata survives disk reading editing and fresh library loading",
          "[preset-metadata][unicode][filesystem]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;NotesFolder folder(p);
    auto xml=notesPreset(p);xml.setAttribute("presetDescription","FIRE_UTF8_DESCRIPTION_FIXTURE");
    xml.setAttribute("presetTags",juce::String::fromUTF8("[\"温暖\",\"电子管\"]"));
    // JUCE's XML writer legally escapes non-ASCII characters as entities.
    // Replace this ASCII sentinel after serialisation to exercise raw UTF-8
    // file input, independently of that writer's escaping policy.
    const auto file=folder.path.getChildFile("Unicode.fire");
    const auto encoded=xml.toString().replace("FIRE_UTF8_DESCRIPTION_FIXTURE",juce::String::fromUTF8("原始中文"));
    REQUIRE(file.replaceWithData(encoded.toRawUTF8(),encoded.getNumBytesAsUTF8()));
    juce::MemoryBlock bytes;REQUIRE(file.loadFileAsData(bytes));
    CHECK(std::string(static_cast<const char*>(bytes.getData()),bytes.getSize()).find("原始中文")!=std::string::npos);
    auto metadata=readNotes(p.statePresets,"Unicode.fire");
    CHECK(metadata.description==juce::String::fromUTF8("原始中文"));CHECK(metadata.description.length()==4);
    REQUIRE(metadata.tags.size()==2);CHECK(metadata.tags[0].length()==2);CHECK(metadata.tags[1][0]==0x7535u);
    metadata.description=juce::String::fromUTF8("修改后的说明 🎛️");metadata.tags.add(juce::String::fromUTF8("空间感"));
    REQUIRE(p.statePresets.writePresetMetadata("Unicode.fire",metadata,metadata.revision).wasOk());
    FireAudioProcessor fresh;fresh.statePresets.setPresetDirectoryForTesting(folder.path);
    const auto restored=readNotes(fresh.statePresets,"Unicode.fire");
    CHECK(restored.description==metadata.description);CHECK(restored.tags==metadata.tags);
    CHECK(std::string(restored.description.toRawUTF8())==std::string("修改后的说明 🎛️"));
    const auto saved=juce::XmlDocument::parse(file);REQUIRE(saved);
    CHECK(state::canLoadStateFromXml(*saved,fresh));
}
