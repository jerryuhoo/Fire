#pragma once
#include <algorithm>
#include "LookAndFeel.h"
#include "PrimarySlider.h"
#include "GranularMediaWorker.h"
#include "../PluginProcessor.h"

struct GranularMediaUiTestAccess;
namespace fire::ui
{
class GranularMediaControls final : public juce::Component,private juce::Timer
{
public:
    GranularMediaControls(FireAudioProcessor& p,FireAudioProcessor::GranularAudioTarget destination,std::function<bool()> ownerValid)
        :processor(p),target(destination),validOwner(std::move(ownerValid))
    {
        setLookAndFeel(&look);
        const juce::Component::SafePointer<GranularMediaControls> safe(this);
        choose.setComponentID("granular_choose_audio");choose.setTitle("Choose audio to import");choose.onClick=[safe]{if(safe)safe->chooseImport();};
        apply.setComponentID("granular_import_freeze");apply.setTitle("Import selected audio window and freeze");apply.onClick=[safe]{if(safe)safe->applyImport();};
        exportAudio.setComponentID("granular_export_audio");exportAudio.setTitle("Export captured frozen audio as WAV");exportAudio.onClick=[safe]{if(safe)safe->chooseExport();};
        cancel.setComponentID("granular_cancel_io");cancel.setTitle("Cancel audio file operation");cancel.onClick=[safe]{if(safe){safe->cancelActive();safe->status.setText("Cancelled",juce::dontSendNotification);safe->updateButtons();}};
        normalise.setButtonText("Normalize to -1 dBFS");normalise.setToggleState(false,juce::dontSendNotification);
        normalise.setComponentID("granular_normalise");normalise.setTooltip("Optional stereo-linked peak normalization after conversion. Off preserves the source amplitude.");
        normalise.onClick=[safe]{if(safe)safe->optionsChanged();};
        start.setSliderStyle(juce::Slider::LinearHorizontal);start.setTextBoxStyle(juce::Slider::TextBoxRight,false,106,28);
        start.setComponentID("granular_start_seconds");start.setTitle("Audio window start in seconds");
        start.setTooltip("Select the start of the window to import. Fire holds at most 1.02175 seconds, independent of the host sample rate.");
        start.textFromValueFunction=[](double value){return juce::String(value,5)+" s";};start.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue();};
        start.onValueChange=[safe]{if(safe && !safe->updatingStart)safe->optionsChanged();};
        for(auto* component:std::array<juce::Component*,9>{&choose,&apply,&exportAudio,&cancel,&normalise,&start,&infoLabel,&selectionLabel,&status})addAndMakeVisible(component);
        infoLabel.setFont(bodyFont(12));selectionLabel.setFont(bodyFont(11));status.setFont(bodyFont(11));
        for(auto* label:{&infoLabel,&selectionLabel,&status})label->setJustificationType(juce::Justification::topLeft);
        infoLabel.setText("No file selected. Import uses the reviewed preview snapshot.",juce::dontSendNotification);
        selectionLabel.setText("Capacity 1.02175 s / 32 kHz stereo, 16-bit.\nChoose a start window before importing.",juce::dontSendNotification);
        status.setText("Choose a mono or stereo file, review the window, then Import and Freeze.",juce::dontSendNotification);
        setSize(630,420);refreshExportAvailability();updateButtons();lookAndFeelChanged();
    }
    ~GranularMediaControls()override{tearingDown=true;stopTimer();cancelActive();setLookAndFeel(nullptr);}
    void dismiss(){cancelActive();}
    bool hasValidOwner()const{return ownerCurrent();}
    void visibilityChanged()override{if(isShowing())startTimerHz(30);else{stopTimer();cancelActive();}}
    void enablementChanged()override{if(!isEnabled())cancelActive();}
    void styleCallout(juce::CallOutBox& box){setSkin(box,skinFor(*this));box.setLookAndFeel(&calloutLook);}
    void lookAndFeelChanged()override
    {
        if(tearingDown)return;look.setSkin(skinFor(*this));const auto& p=paletteFor(*this);
        for(auto* label:{&infoLabel,&selectionLabel,&status})label->setColour(juce::Label::textColourId,p.textSecondary);
        start.setColour(juce::Slider::trackColourId,p.accent);start.setColour(juce::Slider::thumbColourId,p.textPrimary);
        start.setColour(juce::Slider::textBoxTextColourId,p.textPrimary);start.setColour(juce::Slider::textBoxBackgroundColourId,p.surface0);
        start.setColour(juce::Slider::textBoxOutlineColourId,p.hairline);repaint();
    }
    void resized()override
    {
        auto a=getLocalBounds().reduced(18);auto heading=a.removeFromTop(30);choose.setBounds(heading.removeFromRight(116));
        a.removeFromTop(6);infoLabel.setBounds(a.removeFromTop(34));waveArea=a.removeFromTop(juce::jlimit(52,104,getHeight()/4));
        a.removeFromTop(10);auto options=a.removeFromTop(30);normalise.setBounds(options.removeFromRight(190));options.removeFromRight(10);start.setBounds(options);
        a.removeFromTop(5);selectionLabel.setBounds(a.removeFromTop(38));
        auto actions=a.removeFromBottom(32);apply.setBounds(actions.removeFromRight(158));actions.removeFromRight(9);exportAudio.setBounds(actions.removeFromRight(144));cancel.setBounds(actions.removeFromLeft(76));
        a.removeFromBottom(6);status.setBounds(a);
    }
    void paint(juce::Graphics& g)override
    {
        const auto& p=paletteFor(*this);drawCanvas(g,getLocalBounds().toFloat(),skinFor(*this));
        g.setColour(p.textPrimary);g.setFont(displayFont(18));g.drawText("Granular / Sample",getLocalBounds().reduced(18).withHeight(30).withTrimmedRight(125),juce::Justification::centredLeft);
        auto area=waveArea.toFloat();g.setColour(p.surface0);g.fillRoundedRectangle(area,5);g.setColour(p.hairline);g.drawRoundedRectangle(area,5,1);
        area.reduce(9,8);
        if(hasWave)
            for(int channel=0;channel<2;++channel)
            {
                const float centre=area.getY()+area.getHeight()*(channel==0?.25f:.75f),height=area.getHeight()*.22f;
                g.setColour(p.hairline);g.drawHorizontalLine(juce::roundToInt(centre),area.getX(),area.getRight());
                g.setColour(isLineSkin(*this)?p.accent:colours::granular);
                for(int point=0;point<GranularWavePreview::points;++point)
                {
                    const float x=area.getX()+area.getWidth()*point/(GranularWavePreview::points-1);
                    g.drawVerticalLine(juce::roundToInt(x),centre-wave.high[static_cast<size_t>(channel)][static_cast<size_t>(point)]*height,
                                        centre-wave.low[static_cast<size_t>(channel)][static_cast<size_t>(point)]*height+.5f);
                }
            }
        else{g.setFont(bodyFont(12));g.setColour(p.textMuted);g.drawText("Audio window preview",area,juce::Justification::centred);}
    }
private:
    friend struct ::GranularMediaUiTestAccess;
    class CalloutLook final:public juce::LookAndFeel_V4
    {void drawCallOutBoxBackground(juce::CallOutBox& box,juce::Graphics& g,const juce::Path& path,juce::Image&)override
     {const auto& p=paletteFor(box);juce::DropShadow(juce::Colours::black.withAlpha(.18f),7,{0,2}).drawForPath(g,path);g.setColour(p.canvas);g.fillPath(path);g.setColour(p.hairline);g.strokePath(path,juce::PathStrokeType(1));}};
    bool ownerCurrent()const{return !tearingDown && validOwner && validOwner();}
    bool targetCurrent()const{return ownerCurrent() && processor.isGranularAudioTargetCurrent(target);}
    bool requestCurrent(std::uint64_t request)const{return request==generation && isShowing() && isEnabled() && targetCurrent();}
    void cancelActive()
    {
        ++generation;decodeDeadline=0;
        if(pending)pending->cancelled.store(true,std::memory_order_release);pending.reset();
        awaitingOverwrite.reset();overwriteAlert.close();chooser.reset();chooserActive=false;
    }
    void updateButtons()
    {
        if(tearingDown)return;
        const bool valid=targetCurrent(),busy=bool(pending)||chooserActive||bool(awaitingOverwrite);
        choose.setEnabled(valid);normalise.setEnabled(valid && info.has_value());start.setEnabled(valid && info && maximumStartFrame>0);
        apply.setEnabled(valid && previewResult.recording && !previewCommitted && !busy && decodeDeadline==0);
        exportAudio.setEnabled(valid && exportAvailable && !busy);cancel.setEnabled(busy || decodeDeadline>0);
    }
    void refreshExportAvailability()
    {
        if(!targetCurrent()){exportAvailable=false;return;}
        effects::FrozenRecordingPtr recording;
        const auto result=processor.captureGranularAudioForExport(target,recording);
        exportAvailable=result.wasOk() && recording;
        exportAudio.setTooltip(exportAvailable?"Export the captured frozen recording as 32 kHz stereo 16-bit WAV. The export keeps that immutable snapshot.":result.getErrorMessage());
        if(exportAvailable && !info){wave=GranularWavePreview::from(recording);hasWave=true;infoLabel.setText("Frozen capture / "+juce::String(recording->validFrames/granular_io::recordingSampleRate,5)+" s",juce::dontSendNotification);repaint(waveArea);}
    }
    void startJob(const std::shared_ptr<GranularMediaJob>& job,GranularMediaWorkers::Task task)
    {pending=job;GranularMediaWorkers::submit(job,std::move(task));updateButtons();}
    std::function<void(const juce::File&)> importSelectionHandler()
    {
        cancelActive();const auto request=generation;const juce::Component::SafePointer<GranularMediaControls> safe(this);
        return [safe,request](const juce::File& file)
        {auto owner=safe;if(!owner || !owner->requestCurrent(request))return;owner->chooserActive=false;owner->chooser.reset();
         if(file==juce::File()){owner->updateButtons();return;}owner->inspect(file);};
    }
    void chooseImport()
    {
        if(!targetCurrent())return;
        auto selected=importSelectionHandler();
        chooser=std::make_shared<juce::FileChooser>("Import granular audio",sourceFile,granular_io::supportedFilePatterns(),false,false,getTopLevelComponent());
        chooserActive=true;const std::weak_ptr<juce::FileChooser> weak=chooser;
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [weak,selected=std::move(selected)](const juce::FileChooser& dialog)
            {auto keep=weak.lock();if(keep)selected(dialog.getResult());});styleFileChooser();updateButtons();
    }
    void styleFileChooser()
    {
        // The browser is a sibling of Sample, so its standard 600 x 500
        // window fits even when the Sample callout is only 630 x 420.
        auto* parent=getTopLevelComponent();
        auto* dialog=dynamic_cast<juce::FileChooserDialogBox*>(juce::Component::getCurrentlyModalComponent());
        if(!chooserActive || dialog==nullptr || dialog->getParentComponent()!=parent)return;
        const auto& p=paletteFor(*this);
        // This LookAndFeel belongs to this Sample session, never the editor or
        // JUCE's global default. The constructor chose colours before parenting.
        look.setColour(juce::FileChooserDialogBox::titleTextColourId,p.textPrimary);
        look.setColour(juce::FileBrowserComponent::currentPathBoxBackgroundColourId,p.surface0);
        look.setColour(juce::FileBrowserComponent::currentPathBoxTextColourId,p.textPrimary);
        look.setColour(juce::FileBrowserComponent::currentPathBoxArrowColourId,p.textSecondary);
        look.setColour(juce::FileBrowserComponent::filenameBoxBackgroundColourId,p.surface0);
        look.setColour(juce::FileBrowserComponent::filenameBoxTextColourId,p.textPrimary);
        look.setColour(juce::ListBox::backgroundColourId,p.surface0);
        look.setColour(juce::DirectoryContentsDisplayComponent::textColourId,p.textPrimary);
        look.setColour(juce::DirectoryContentsDisplayComponent::highlightColourId,p.raised);
        look.setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId,p.textPrimary);
        dialog->setLookAndFeel(&look);dialog->setBackgroundColour(p.canvas);
        dialog->setBounds(dialog->getBounds().constrainedWithin(parent->getLocalBounds().reduced(8)));
        // JUCE creates this drawable before attaching the button to its parent,
        // so its baked default white fill does not follow a Paper theme.
        const auto colourUpButton=[&](auto&& visit,juce::Component& component)->void
        {
            for(auto* child:component.getChildren())
            {
                if(auto* up=dynamic_cast<juce::DrawableButton*>(child);up && up->getName()=="up")
                {
                    juce::Path path;path.addArrow({50.f,100.f,50.f,0.f},40.f,100.f,50.f);
                    juce::DrawablePath arrow;arrow.setPath(path);arrow.setFill(p.textPrimary);up->setImages(&arrow);
                }
                visit(visit,*child);
            }
        };
        colourUpButton(colourUpButton,*dialog);
    }
    void inspect(const juce::File& file)
    {
        if(!targetCurrent())return;cancelActive();lastExportResult.clear();sourceFile=file;info.reset();previewResult={};previewCommitted=false;hasWave=false;
        auto job=std::make_shared<GranularMediaJob>(GranularMediaJob::Kind::inspect,generation);job->file=file;
        status.setText("Reading audio information...",juce::dontSendNotification);repaint(waveArea);
        startJob(job,[](auto& state){state.status=granular_io::inspectFile(state.file,state.info);});
    }
    void optionsChanged()
    {
        if(!info || !targetCurrent())return;
        cancelActive();previewResult={};previewCommitted=false;hasWave=false;decodeDeadline=juce::Time::getMillisecondCounterHiRes()+100;
        status.setText("Preparing selected window...",juce::dontSendNotification);updateButtons();repaint(waveArea);
    }
    juce::int64 selectedFrame()const
    {
        if(!info)return 0;
        const auto frame=static_cast<long double>(start.getValue())*info->sampleRate+.5L;
        return static_cast<juce::int64>(juce::jlimit(0.L,static_cast<long double>(maximumStartFrame),frame));
    }
    void decode()
    {
        if(!info || !targetCurrent())return;
        decodeDeadline=0;auto job=std::make_shared<GranularMediaJob>(GranularMediaJob::Kind::preview,generation);
        job->file=sourceFile;job->info=*info;
        const granular_io::ImportOptions options{selectedFrame(),normalise.getToggleState()};
        selectionLabel.setText("Capacity 1.02175 s / 32 kHz stereo, 16-bit.\nSelected start "+juce::String(options.startFrame/info->sampleRate,5)+" s.",juce::dontSendNotification);
        status.setText("Decoding the selected window...",juce::dontSendNotification);
        startJob(job,[options](auto& state)
        {state.status=granular_io::readWindow(state.file,state.info,options,state.imported,&state.cancelled);
         if(state.status.wasOk() && !state.cancelled.load(std::memory_order_acquire))state.wave=GranularWavePreview::from(state.imported.recording);});
    }
    void applyImport()
    {
        if(!isShowing() || !targetCurrent() || !previewResult.recording || previewCommitted || pending || decodeDeadline>0)return;
        const juce::Component::SafePointer<GranularMediaControls> safe(this);const auto recording=previewResult.recording;
        const auto requestTarget=target;
        const auto result=processor.applyImportedGranularAudio(requestTarget,recording);
        if(!safe)return;
        if(result.failed()){status.setText(result.getErrorMessage(),juce::dontSendNotification);updateButtons();return;}
        const auto next=processor.getGranularAudioTarget(requestTarget.scope,requestTarget.slot);
        if(!next || !ownerCurrent() || next->slotGeneration!=requestTarget.slotGeneration
            || next->revision!=static_cast<std::uint32_t>(requestTarget.revision+2))
        {cancelActive();status.setText("The target changed during import. Reopen Sample to continue.",juce::dontSendNotification);updateButtons();return;}
        target=*next;previewCommitted=true;status.setText("Imported the preview snapshot and froze it. Undo restores the previous frozen recording or live mode.",juce::dontSendNotification);
        refreshExportAvailability();updateButtons();
    }
    std::function<void(const juce::File&)> exportSelectionHandler(effects::FrozenRecordingPtr snapshot)
    {
        cancelActive();const auto request=generation;const juce::Component::SafePointer<GranularMediaControls> safe(this);
        return [safe,request,snapshot=std::move(snapshot)](const juce::File& file)
        {auto owner=safe;if(!owner || !owner->requestCurrent(request))return;owner->chooserActive=false;owner->chooser.reset();
         if(file==juce::File()){owner->updateButtons();return;}owner->prepareDestination(file,snapshot);};
    }
    void chooseExport()
    {
        if(!targetCurrent())return;effects::FrozenRecordingPtr captured;
        const auto result=processor.captureGranularAudioForExport(target,captured);
        if(result.failed()){status.setText(result.getErrorMessage(),juce::dontSendNotification);refreshExportAvailability();updateButtons();return;}
        auto selected=exportSelectionHandler(captured);
        const auto suggested=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Fire Frozen.wav");
        chooser=std::make_shared<juce::FileChooser>("Export captured frozen audio",suggested,"*.wav",false,false,getTopLevelComponent());
        chooserActive=true;const std::weak_ptr<juce::FileChooser> weak=chooser;
        // The final suffix and destination stamp are resolved before our single
        // explicit confirmation. Do not enable the chooser's overwrite prompt.
        chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,
            [weak,selected=std::move(selected)](const juce::FileChooser& dialog){auto keep=weak.lock();if(keep)selected(dialog.getResult());});styleFileChooser();updateButtons();
    }
    void prepareDestination(const juce::File& selected,effects::FrozenRecordingPtr snapshot)
    {
        if(!targetCurrent() || !snapshot)return;cancelActive();lastExportResult.clear();
        auto job=std::make_shared<GranularMediaJob>(GranularMediaJob::Kind::destination,generation);
        job->file=granular_io::resolveWavePath(selected);job->snapshot=std::move(snapshot);
        status.setText("Checking export destination...",juce::dontSendNotification);
        startJob(job,[](auto& state){state.status=granular_io::getFileStamp(state.file,state.destination);});
    }
    void confirmOverwrite(bool accepted,std::uint64_t request)
    {
        if(!awaitingOverwrite || request!=generation || !requestCurrent(request))return;
        auto prepared=std::move(awaitingOverwrite);overwriteAlert.close();
        if(!accepted){status.setText("Export cancelled; the existing file was kept.",juce::dontSendNotification);updateButtons();return;}
        write(prepared,true);
    }
    void write(const std::shared_ptr<GranularMediaJob>& prepared,bool overwrite)
    {
        if(!targetCurrent() || !prepared->snapshot || prepared->generation!=generation)return;
        auto job=std::make_shared<GranularMediaJob>(GranularMediaJob::Kind::write,generation);
        job->file=prepared->file;job->snapshot=prepared->snapshot;job->destination=prepared->destination;
        status.setText("Exporting captured frozen audio...",juce::dontSendNotification);
        startJob(job,[overwrite](auto& state){state.status=granular_io::exportWaveAtomic(state.file,*state.snapshot,overwrite,&state.cancelled,&state.destination);});
    }
    void poll()
    {
        if(!ownerCurrent()){cancelActive();status.setText("The target changed. Reopen Sample to continue.",juce::dontSendNotification);updateButtons();stopTimer();return;}
        const bool exporting=pending && pending->kind==GranularMediaJob::Kind::write;
        // A started export intentionally owns an immutable snapshot. Subsequent
        // sound edits do not change or cancel that write; close/Cancel still do.
        if(!exporting && !targetCurrent())
        {cancelActive();status.setText(lastExportResult.isNotEmpty()?lastExportResult+". The target changed; reopen Sample to continue.":"The granular target changed. Reopen Sample to continue.",juce::dontSendNotification);updateButtons();stopTimer();return;}
        if(decodeDeadline>0 && juce::Time::getMillisecondCounterHiRes()>=decodeDeadline)decode();
        if(pending && pending->done.load(std::memory_order_acquire))
        {
            auto finished=std::move(pending);
            if(finished->generation!=generation || finished->cancelled.load(std::memory_order_acquire)){updateButtons();return;}
            if(finished->status.failed()){status.setText(finished->status.getErrorMessage(),juce::dontSendNotification);updateButtons();return;}
            if(finished->kind==GranularMediaJob::Kind::inspect)
            {
                info=finished->info;
                // Keep this scalar: JUCE's explicit jmax template also considers
                // an unsupported SIMDRegister<long long> overload on Linux x86.
                maximumStartFrame=std::max(juce::int64{0},info->sourceFrames-static_cast<juce::int64>(std::ceil(granular_io::maximumDuration*info->sampleRate)));
                updatingStart=true;start.setRange(0,juce::jmax(1./info->sampleRate,maximumStartFrame/info->sampleRate),1./info->sampleRate);start.setValue(0,juce::dontSendNotification);updatingStart=false;
                infoLabel.setText(sourceFile.getFileName()+" / "+juce::String(info->durationSeconds,3)+" s / "+juce::String(info->sampleRate/1000.,1)+" kHz / "+(info->channels==1?"mono -> stereo":"stereo"),juce::dontSendNotification);
                decode();
            }
            else if(finished->kind==GranularMediaJob::Kind::preview)
            {
                previewResult=std::move(finished->imported);wave=std::move(finished->wave);hasWave=true;
                selectionLabel.setText("Selected "+juce::String(previewResult.sourceStartFrame/info->sampleRate,5)+" s + "+juce::String(previewResult.outputFrames/granular_io::recordingSampleRate,5)+" s. Capacity 1.02175 s.\nImport converts to 32 kHz stereo, 16-bit.",juce::dontSendNotification);
                juce::String message=previewResult.normalised?"Preview ready / normalized to -1 dBFS.":"Preview ready / original amplitude preserved.";
                if(previewResult.clippedSamples>0)message+=" Clipped samples: "+juce::String(previewResult.clippedSamples)+".";
                if(previewResult.nonFiniteSamples>0)message+=" Invalid samples replaced: "+juce::String(previewResult.nonFiniteSamples)+".";
                status.setText(message,juce::dontSendNotification);repaint(waveArea);
            }
            else if(finished->kind==GranularMediaJob::Kind::destination)
            {
                if(finished->destination.exists)
                {
                    awaitingOverwrite=finished;const auto request=generation;
                    status.setText("Confirm replacement of "+finished->file.getFullPathName(),juce::dontSendNotification);
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
                    if(!suppressDialogsForTesting)
#endif
                    {
                        const juce::Component::SafePointer<GranularMediaControls> safe(this);
                        const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                            .withTitle("Replace frozen audio WAV?").withMessage("Replace this file?\n"+finished->file.getFullPathName())
                            .withButton("Replace").withButton("Cancel").withAssociatedComponent(this);
                        auto alert=juce::NativeMessageBox::showScopedAsync(options,[safe,request](int result){if(safe)safe->confirmOverwrite(result==1,request);});
                        if(safe && safe->generation==request)safe->overwriteAlert=std::move(alert);
                        if(!safe)return;
                    }
                }
                else write(finished,false);
            }
            else {lastExportResult="Exported captured frozen audio to "+finished->file.getFileName();status.setText(lastExportResult,juce::dontSendNotification);}
            updateButtons();
        }
        if(!pending && !chooserActive && !awaitingOverwrite && juce::Time::getMillisecondCounterHiRes()-lastExportCheck>500)
        {lastExportCheck=juce::Time::getMillisecondCounterHiRes();refreshExportAvailability();updateButtons();}
    }
    void timerCallback()override{poll();}
    FireAudioProcessor& processor;FireAudioProcessor::GranularAudioTarget target;std::function<bool()> validOwner;
    FireLookAndFeel look;CalloutLook calloutLook;
    PrimaryTextButton choose{"Choose audio..."},apply{"Import and Freeze"},exportAudio{"Export frozen WAV"},cancel{"Cancel"};
    PrimaryToggleButton normalise;PrimarySlider start;juce::Label infoLabel,selectionLabel,status;juce::Rectangle<int> waveArea;
    juce::File sourceFile;juce::String lastExportResult;std::optional<granular_io::AudioInfo> info;granular_io::ImportResult previewResult;GranularWavePreview wave;
    juce::int64 maximumStartFrame=0;std::uint64_t generation=0;double decodeDeadline=0,lastExportCheck=0;
    std::shared_ptr<GranularMediaJob> pending,awaitingOverwrite;std::shared_ptr<juce::FileChooser> chooser;juce::ScopedMessageBox overwriteAlert;
    bool updatingStart=false,hasWave=false,exportAvailable=false,chooserActive=false,tearingDown=false,previewCommitted=false;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    bool suppressDialogsForTesting=false;
#endif
};
}
