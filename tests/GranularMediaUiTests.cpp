#include <GUI/GranularMediaControls.h>
#include <GUI/InsertEffectControls.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <chrono>
#include <cstdlib>
#include <thread>

struct GranularMediaUiTestAccess
{
    static void manual(fire::ui::GranularMediaControls& c){c.stopTimer();c.suppressDialogsForTesting=true;}
    static void tick(fire::ui::GranularMediaControls& c){c.poll();}
    static void inspect(fire::ui::GranularMediaControls& c,const juce::File& file){c.inspect(file);}
    static bool ready(const fire::ui::GranularMediaControls& c){return bool(c.previewResult.recording) && !c.pending && c.decodeDeadline==0;}
    static bool idle(const fire::ui::GranularMediaControls& c){return !c.pending && !c.awaitingOverwrite && !c.chooserActive && c.decodeDeadline==0;}
    static auto recording(const fire::ui::GranularMediaControls& c){return c.previewResult.recording;}
    static auto result(const fire::ui::GranularMediaControls& c){return c.previewResult;}
    static auto& start(fire::ui::GranularMediaControls& c){return c.start;}
    static auto& normalise(fire::ui::GranularMediaControls& c){return c.normalise;}
    static auto& importButton(fire::ui::GranularMediaControls& c){return c.apply;}
    static auto& exportButton(fire::ui::GranularMediaControls& c){return c.exportAudio;}
    static void decode(fire::ui::GranularMediaControls& c){c.decode();}
    static void apply(fire::ui::GranularMediaControls& c){c.applyImport();}
    static juce::String status(const fire::ui::GranularMediaControls& c){return c.status.getText();}
    static juce::String selection(const fire::ui::GranularMediaControls& c){return c.selectionLabel.getText();}
    static auto target(const fire::ui::GranularMediaControls& c){return c.target;}
    static auto importSelection(fire::ui::GranularMediaControls& c){return c.importSelectionHandler();}
    static auto exportSelection(fire::ui::GranularMediaControls& c,fire::effects::FrozenRecordingPtr r){return c.exportSelectionHandler(std::move(r));}
    static void chooseImport(fire::ui::GranularMediaControls& c,const juce::File& file){c.sourceFile=file;c.chooseImport();}
    static void chooseExport(fire::ui::GranularMediaControls& c){c.chooseExport();}
    static std::weak_ptr<juce::FileChooser> chooser(const fire::ui::GranularMediaControls& c){return c.chooser;}
    static void destination(fire::ui::GranularMediaControls& c,const juce::File& file,fire::effects::FrozenRecordingPtr r){c.prepareDestination(file,std::move(r));}
    static bool confirming(const fire::ui::GranularMediaControls& c){return bool(c.awaitingOverwrite);}
    static juce::File confirmedPath(const fire::ui::GranularMediaControls& c){return c.awaitingOverwrite?c.awaitingOverwrite->file:juce::File{};}
    static void confirm(fire::ui::GranularMediaControls& c,bool accepted){c.confirmOverwrite(accepted,c.generation);}
    static auto fakeJob(fire::ui::GranularMediaControls& c,fire::ui::GranularMediaJob::Kind kind,fire::ui::GranularMediaWorkers::Task task)
    {
        c.cancelActive();
        if(kind==fire::ui::GranularMediaJob::Kind::preview && !c.info)
        {fire::granular_io::AudioInfo info;info.sampleRate=32000;info.channels=2;info.sourceFrames=512;info.durationSeconds=512./32000;c.info=info;}
        auto state=std::make_shared<fire::ui::GranularMediaJob>(kind,c.generation);c.startJob(state,std::move(task));return state;
    }
    static void open(InsertEffectControls& c){c.showMedia();}
    static void close(InsertEffectControls& c){c.closeMedia();}
    static auto popover(InsertEffectControls& c){return c.mediaPopover;}
    static auto* body(InsertEffectControls& c){return c.mediaSession?c.mediaSession->content.get():nullptr;}
    static auto& sample(InsertEffectControls& c){return c.sampleButton;}
    static auto& freeze(InsertEffectControls& c){return c.freezeButton;}
};
namespace
{
using Access=GranularMediaUiTestAccess;using Controls=fire::ui::GranularMediaControls;namespace io=fire::granular_io;
using Recording=fire::effects::FrozenRecording;using RecordingPtr=fire::effects::FrozenRecordingPtr;
struct Folder
{juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fire-media-ui-"+juce::Uuid().toString());
 Folder(){REQUIRE(path.createDirectory().wasOk());}~Folder(){path.deleteRecursively();}};
void show(juce::Component& c){c.addToDesktop(juce::ComponentPeer::windowIsTemporary);c.setVisible(true);}
RecordingPtr recording(int value=7000,int frames=512)
{auto result=std::make_shared<Recording>();result->validFrames=frames;result->head=frames;for(auto& c:result->samples)for(int i=0;i<frames;++i)c[static_cast<size_t>(i)]=static_cast<std::int16_t>(value);return result;}
void sourceFile(const juce::File& file)
{
    juce::AudioBuffer<float> audio(2,96000);
    for(int i=0;i<audio.getNumSamples();++i)
    {const float sample=(i<32000?.15f:-.3f)*std::cos(juce::MathConstants<float>::twoPi*200.f*i/32000.f);audio.setSample(0,i,sample);audio.setSample(1,i,sample*.5f);}
    std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();REQUIRE(stream);
    auto writer=juce::WavAudioFormat{}.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(32000).withNumChannels(2).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    REQUIRE(writer);REQUIRE(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()));REQUIRE(writer->flush());
}
void wait(const std::function<bool()>& done,const std::function<void()>& poll={})
{
    const auto begin=juce::Time::getMillisecondCounter();
    while(!done() && juce::Time::getMillisecondCounter()-begin<5000)
    {juce::MessageManager::getInstance()->runDispatchLoopUntil(5);if(poll)poll();}
    REQUIRE(done());
}
struct Fixture
{
    FireAudioProcessor processor;int slot;std::unique_ptr<Controls> control;
    Fixture(){slot=processor.addInsertEffect(1,fire::effects::Type::granular);REQUIRE(slot>=0);auto target=processor.getGranularAudioTarget(1,slot);REQUIRE(target);control=std::make_unique<Controls>(processor,*target,[]{return true;});show(*control);Access::manual(*control);}
    void load(const juce::File& file){Access::inspect(*control,file);wait([&]{return Access::ready(*control);},[&]{Access::tick(*control);});}
};
struct Gate
{
    std::atomic<bool> entered{false},release{false},ranOnMessageThread{false};
    void block(fire::ui::GranularMediaJob& job)
    {ranOnMessageThread=juce::MessageManager::getInstance()->isThisTheMessageThread();entered=true;
     while(!release.load() && !job.cancelled.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));}
};
void preview(juce::Component& c,const juce::String& name)
{if(const auto* root=std::getenv("FIRE_GRANULAR_MEDIA_PREVIEW_DIR")){auto file=juce::File(root).getChildFile(name+".png");REQUIRE(file.getParentDirectory().createDirectory().wasOk());auto stream=file.createOutputStream();REQUIRE(stream);stream->setPosition(0);stream->truncate();REQUIRE(juce::PNGImageFormat{}.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*stream));}}
template<class T> T* findChild(juce::Component& parent)
{for(auto* child:parent.getChildren()){if(auto* typed=dynamic_cast<T*>(child))return typed;if(auto* nested=findChild<T>(*child))return nested;}return nullptr;}
}

TEST_CASE("Granular Sample imports the reviewed nonzero window with normalization off and enables Freeze", "[granular-media-ui][ui][granular-io]")
{
    Folder folder;const auto file=folder.path.getChildFile("long-source.wav");sourceFile(file);Fixture f;
    REQUIRE_FALSE(Access::normalise(*f.control).getToggleState());f.load(file);
    Access::start(*f.control).setValue(1.5,juce::sendNotificationSync);Access::decode(*f.control);
    wait([&]{return Access::ready(*f.control);},[&]{Access::tick(*f.control);});
    const auto result=Access::result(*f.control);CHECK(result.sourceStartFrame==48000);CHECK(result.outputFrames==fire::effects::frozenRecordingFrames);
    CHECK(result.recording->samples[0][0]<0);CHECK_FALSE(result.normalised);CHECK(Access::selection(*f.control).contains("1.02175"));
    const auto before=f.processor.getChainEditRevision();Access::apply(*f.control);
    CHECK(f.processor.getChainEditRevision()==before+2);CHECK(Access::status(*f.control).contains("preview snapshot"));
    auto target=f.processor.getGranularAudioTarget(1,f.slot);REQUIRE(target);RecordingPtr frozen;
    REQUIRE(f.processor.captureGranularAudioForExport(*target,frozen).wasOk());REQUIRE(frozen);CHECK(frozen->samples==result.recording->samples);
    REQUIRE(f.processor.undoEdit());
    CHECK(f.processor.treeState.getRawParameterValue(fire::clouds_params::parameterID(1,f.slot,fire::clouds_params::freezeField))->load()==0);
}

TEST_CASE("Granular file selection callbacks cannot start work after close deletion or a recreated slot", "[granular-media-ui][ui][lifecycle]")
{
    Folder folder;const auto file=folder.path.getChildFile("audio.wav");sourceFile(file);
    for(bool exporting:{false,true})for(int boundary:{0,1,2})
    {
        CAPTURE(exporting,boundary);
        Fixture f;auto callback=exporting?Access::exportSelection(*f.control,recording()):Access::importSelection(*f.control);
        if(boundary==0)f.control->setVisible(false);
        else if(boundary==1)f.control.reset();
        else{f.processor.removeInsertEffect(1,f.slot);REQUIRE(f.processor.addInsertEffect(1,fire::effects::Type::granular)==f.slot);}
        callback(file);
        if(f.control){CHECK(Access::idle(*f.control));CHECK_FALSE(Access::ready(*f.control));}
        CHECK(f.processor.treeState.getRawParameterValue(fire::clouds_params::parameterID(1,f.slot,fire::clouds_params::freezeField))->load()==0);
    }
}

TEST_CASE("Late granular workers are cancelled off the message thread and cannot commit after their owner closes", "[granular-media-ui][ui][worker][lifecycle]")
{
    for(int boundary:{0,1,2})
    {
        CAPTURE(boundary);
        Fixture f;auto gate=std::make_shared<Gate>();const juce::ScopeGuard release{[gate]{gate->release=true;}};
        const auto media=recording();
        auto job=Access::fakeJob(*f.control,fire::ui::GranularMediaJob::Kind::preview,[gate,media](auto& state)
        {gate->block(state);state.imported.recording=media;state.status=juce::Result::ok();});
        wait([&]{return gate->entered.load();});CHECK_FALSE(gate->ranOnMessageThread.load());
        if(boundary==1)f.control.reset();
        else if(boundary==0)f.control->setVisible(false);
        else{f.processor.removeInsertEffect(1,f.slot);REQUIRE(f.processor.addInsertEffect(1,fire::effects::Type::granular)==f.slot);Access::tick(*f.control);}
        CHECK(job->cancelled.load());gate->release=true;wait([&]{return job->done.load();});
        if(f.control){Access::tick(*f.control);CHECK_FALSE(Access::ready(*f.control));}
        auto target=f.processor.getGranularAudioTarget(1,f.slot);REQUIRE(target);RecordingPtr frozen;
        CHECK(f.processor.captureGranularAudioForExport(*target,frozen).failed());
    }
}

TEST_CASE("Frozen export resolves wav before one overwrite decision and rejects destination changes", "[granular-media-ui][ui][export][files]")
{
    Fixture f;Folder folder;const auto media=recording();
    const auto selected=folder.path.getChildFile("take.v2");const auto final=io::resolveWavePath(selected);REQUIRE(final.getFileName().endsWith(".wav"));
    REQUIRE(final.replaceWithText("keep this file"));
    Access::destination(*f.control,selected,media);wait([&]{return Access::confirming(*f.control);},[&]{Access::tick(*f.control);});
    CHECK(Access::confirmedPath(*f.control)==final);Access::confirm(*f.control,false);CHECK(final.loadFileAsString()=="keep this file");CHECK_FALSE(selected.existsAsFile());
    Access::destination(*f.control,selected,media);wait([&]{return Access::confirming(*f.control);},[&]{Access::tick(*f.control);});
    REQUIRE(final.replaceWithText("a concurrent edit must survive"));Access::confirm(*f.control,true);
    wait([&]{return Access::idle(*f.control);},[&]{Access::tick(*f.control);});
    CHECK(final.loadFileAsString()=="a concurrent edit must survive");CHECK(Access::status(*f.control).isNotEmpty());
    Access::confirm(*f.control,true);CHECK(Access::idle(*f.control));
}

TEST_CASE("Started granular export keeps its immutable snapshot across sound edits while explicit close cancels writes", "[granular-media-ui][ui][worker][export]")
{
    for(bool cancel:{false,true})
    {
        Fixture f;Folder folder;auto media=recording(-5432);const auto file=folder.path.getChildFile("captured.wav");
        REQUIRE(file.replaceWithText("previous data"));io::FileStamp stamp;REQUIRE(io::getFileStamp(file,stamp).wasOk());
        auto gate=std::make_shared<Gate>();const juce::ScopeGuard release{[gate]{gate->release=true;}};
        auto job=Access::fakeJob(*f.control,fire::ui::GranularMediaJob::Kind::write,[gate,file,media,stamp](auto& state)
        {gate->block(state);state.file=file;state.status=io::exportWaveAtomic(file,*media,true,&state.cancelled,&stamp);});
        wait([&]{return gate->entered.load();});
        REQUIRE(f.processor.setChainRoutingMode(1,fire::parallel_chain::parallel));Access::tick(*f.control);
        CHECK_FALSE(job->cancelled.load());
        if(cancel)f.control->setVisible(false);
        gate->release=true;wait([&]{return job->done.load();});
        if(cancel){CHECK(job->cancelled.load());CHECK(file.loadFileAsString()=="previous data");}
        else
        {
            Access::tick(*f.control);CHECK(job->status.wasOk());
            io::AudioInfo info;REQUIRE(io::inspectFile(file,info).wasOk());io::ImportResult read;
            REQUIRE(io::readWindow(file,info,{},read).wasOk());CHECK(read.recording->samples[0][0]==-5432);
            CHECK(Access::status(*f.control).contains("Exported"));
        }
    }
}

TEST_CASE("Sample popover remains within a 1000x500 host and cancels on Granular rebinding in all skins", "[granular-media-ui][ui][layout][skin][lifecycle]")
{
    Folder folder;const auto source=folder.path.getChildFile("preview.wav");sourceFile(source);
    FireAudioProcessor p;const int first=p.addInsertEffect(1,fire::effects::Type::granular),second=p.addInsertEffect(1,fire::effects::Type::granular);REQUIRE(first>=0);REQUIRE(second>=0);
    FireLookAndFeel look;juce::Component host;host.setLookAndFeel(&look);host.setSize(1000,500);
    InsertEffectControls controls(p);host.addAndMakeVisible(controls);controls.setBounds(20,220,700,240);controls.bind(1,first);controls.setActive(true);show(host);
    const auto freezeBounds=Access::freeze(controls).getBounds();CHECK_FALSE(freezeBounds.intersects(Access::sample(controls).getBounds()));
    for(auto skin:fire::ui::skins)
    {
        fire::ui::setSkin(host,skin);look.setSkin(skin);host.sendLookAndFeelChange();Access::open(controls);auto callout=Access::popover(controls);REQUIRE(callout);
        CHECK(host.getLocalBounds().contains(callout->getBounds()));auto* body=Access::body(controls);REQUIRE(body);Access::manual(*body);
        Access::inspect(*body,source);wait([&]{return Access::ready(*body);},[&]{Access::tick(*body);});
        REQUIRE(Access::importButton(*body).isEnabled());
        REQUIRE(Access::normalise(*body).isEnabled());
        REQUIRE(Access::start(*body).isEnabled());
        REQUIRE_FALSE(Access::exportButton(*body).isEnabled()); // No material has been frozen yet.
        CHECK(Access::selection(*body).contains("1.02175"));for(auto* child:body->getChildren())if(child->isVisible())CHECK(body->getLocalBounds().contains(child->getBounds()));
        // Worker readiness updates logical enablement immediately; the child
        // Primary controls still need their UI timer ticks to finish fading
        // from the disabled appearance before the visual review snapshot.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(450);
        REQUIRE(body->isShowing());
        CHECK(Access::importButton(*body).isEnabled());CHECK(Access::normalise(*body).isEnabled());CHECK(Access::start(*body).isEnabled());
        CHECK_FALSE(Access::exportButton(*body).isEnabled());
        preview(*body,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-granular-sample");
        auto gate=std::make_shared<Gate>();const juce::ScopeGuard release{[gate]{gate->release=true;}};
        auto job=Access::fakeJob(*body,fire::ui::GranularMediaJob::Kind::inspect,[gate](auto& state){gate->block(state);state.status=juce::Result::fail("Cancelled fixture");});
        wait([&]{return gate->entered.load();});controls.bind(1,second);CHECK(Access::popover(controls)==nullptr);CHECK(callout==nullptr);CHECK(job->cancelled.load());
        gate->release=true;wait([&]{return job->done.load();});controls.bind(1,first);
    }
    controls.setActive(false);host.setLookAndFeel(nullptr);
}

TEST_CASE("Import does not adopt a different preset loaded by a synchronous host notification", "[granular-media-ui][ui][host][lifecycle]")
{
    Fixture f;Folder folder;const auto file=folder.path.getChildFile("source.wav");sourceFile(file);f.load(file);
    juce::MemoryBlock state;f.processor.getStateInformation(state);
    struct Restore:juce::AudioProcessorListener
    {FireAudioProcessor& p;const juce::MemoryBlock& state;bool armed=true;
     Restore(FireAudioProcessor& owner,const juce::MemoryBlock& saved):p(owner),state(saved){}
     void audioProcessorParameterChanged(juce::AudioProcessor*,int,float)override{}
     void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details)override
     {if(armed && details.nonParameterStateChanged){armed=false;p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));}}
    } restore(f.processor,state);
    const auto oldTarget=Access::target(*f.control);f.processor.addListener(&restore);Access::apply(*f.control);f.processor.removeListener(&restore);
    CHECK_FALSE(restore.armed);CHECK(Access::status(*f.control).containsIgnoreCase("changed"));
    CHECK(Access::target(*f.control).revision==oldTarget.revision);
    CHECK(f.processor.treeState.getRawParameterValue(fire::clouds_params::parameterID(1,f.slot,fire::clouds_params::freezeField))->load()==0);
}

TEST_CASE("Embedded granular file choosers fit the editor and close with Sample before late modal callbacks", "[granular-media-ui][ui][chooser][layout][skin][lifecycle]")
{
    Folder folder;const auto source=folder.path.getChildFile("temporary-source.wav");sourceFile(source);
    FireAudioProcessor p;const auto slot=p.addInsertEffect(1,fire::effects::Type::granular);REQUIRE(slot>=0);
    const auto target=p.getGranularAudioTarget(1,slot);REQUIRE(target);REQUIRE(p.applyImportedGranularAudio(*target,recording()).wasOk());
    FireLookAndFeel look;juce::Component host;host.setLookAndFeel(&look);host.setSize(1000,500);
    InsertEffectControls controls(p);host.addAndMakeVisible(controls);controls.setBounds(20,220,700,240);controls.bind(1,slot);controls.setActive(true);show(host);
    for(const auto skin:{fire::ui::Skin::paper,fire::ui::Skin::ink})for(const bool exporting:{false,true})for(const bool queuedClose:{false,true})
    {
        CAPTURE(static_cast<int>(skin),exporting,queuedClose);
        fire::ui::setSkin(host,skin);look.setSkin(skin);host.sendLookAndFeelChange();Access::open(controls);
        auto* body=Access::body(controls);REQUIRE(body);Access::manual(*body);
        const auto revision=p.getChainEditRevision();
        if(exporting)Access::chooseExport(*body);else Access::chooseImport(*body,source);
        juce::Component::SafePointer<juce::FileChooserDialogBox> dialog=findChild<juce::FileChooserDialogBox>(host);REQUIRE(dialog);
        CHECK(dialog->getParentComponent()==&host);CHECK(host.getLocalBounds().contains(dialog->getBounds()));
        CHECK(dialog->getWidth()>=580);CHECK(dialog->getHeight()>=460);
        auto* browser=findChild<juce::FileBrowserComponent>(*dialog);REQUIRE(browser);
        // The real embedded browser only browses a generated temporary folder;
        // neither path chooses or overwrites a file from the user's documents.
        browser->setRoot(folder.path);browser->setFileName(exporting?"captured.wav":source.getFileName());
        const auto& palette=fire::ui::skinPalette(skin);
        CHECK(dialog->getBackgroundColour()==palette.canvas);
        CHECK(dialog->getLookAndFeel().findColour(juce::FileChooserDialogBox::titleTextColourId)==palette.textPrimary);
        CHECK(browser->findColour(juce::FileBrowserComponent::filenameBoxTextColourId)==palette.textPrimary);
        CHECK(browser->findColour(juce::FileBrowserComponent::filenameBoxBackgroundColourId)==palette.surface0);
        auto* up=findChild<juce::DrawableButton>(*browser);REQUIRE(up);CHECK(up->getName()=="up");
        REQUIRE(up->getNormalImage());
        const auto* upArrow=dynamic_cast<const juce::DrawablePath*>(&up->getNormalImage()->getDrawable());REQUIRE(upArrow);
        CHECK(upArrow->getFill().colour==palette.textPrimary);
        auto weakChooser=Access::chooser(*body);REQUIRE_FALSE(weakChooser.expired());
        if(!queuedClose)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(450);REQUIRE(dialog);REQUIRE(body->isShowing());
            preview(host,juce::String(fire::ui::skinName(skin)).toLowerCase()+(exporting?"-granular-export-chooser":"-granular-import-chooser"));
        }
        // Exercise both destruction while modal and completion queued just
        // before the owning Sample popup is destroyed.
        if(queuedClose)dialog->setVisible(false);
        Access::close(controls);CHECK(dialog==nullptr);CHECK(weakChooser.expired());CHECK(Access::body(controls)==nullptr);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        CHECK(findChild<juce::FileChooserDialogBox>(host)==nullptr);CHECK(p.getChainEditRevision()==revision);
        CHECK_FALSE(folder.path.getChildFile("captured.wav").exists());
    }
    controls.setActive(false);host.setLookAndFeel(nullptr);
}
