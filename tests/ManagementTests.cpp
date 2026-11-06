#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BuiltinEffects.h"
#include <set>
#include <stdexcept>
#include <iostream>

namespace
{
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void good(const drum::Result& result) { if(!result.ok) throw std::runtime_error(result.message.toStdString()); }
juce::Component* control(juce::Component& root, const juce::String& id)
{
    if(root.getComponentID()==id) return &root;
    for(auto* child:root.getChildren()) if(auto* found=control(*child,id)) return found;
    return nullptr;
}
void click(juce::Component& root, const char* id)
{
    auto* b=dynamic_cast<juce::TextButton*>(control(root,id)); if(!b || !b->isEnabled())throw std::runtime_error(std::string("Missing or disabled UI action: ")+id); b->onClick();
}
void screenshot(juce::Component& editor, const char* name)
{
    auto image=editor.createComponentSnapshot(editor.getLocalBounds(),true);
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);
    file.deleteFile(); juce::FileOutputStream output(file); check(output.openedOk(),"Unable to create layout preview");
    check(juce::PNGImageFormat().writeImageToStream(image,output),"Unable to save layout preview");
}
}
void runManagementTests()
{
    {
        drum::KitLibrary layout;auto state=layout.snapshot();auto k=state.activeKit,g=state.kits[0].groups[0].id;
        auto a=layout.addInstrument(k,g).id;auto empty=layout.addGroup(k).id;auto h=layout.addGroup(k).id;auto b=layout.addInstrument(k,h).id;
        auto note=[&](const juce::String& id){auto s=layout.snapshot();return drum::findInstrument(s.kits[0],id)->note;};
        good(layout.setGroupBoundary(k,6));check(note(a)==21 && note(b)==24,"Half octave must align next nonempty group to C, ignoring empty groups");
        for(int n=0;n<4;++n)good(layout.addInstrument(k,g));check(note(b)==30,"Half octave must advance to F# after crossing C");
        good(layout.setGroupBoundary(k,12));check(note(b)==36,"Full octave must advance to next C");
        auto xml=layout.toXml();drum::KitLibrary restored;check(restored.restore(*xml) && restored.snapshot().kits[0].groupBoundary==12,"Boundary lost on recall");
        for(auto* kit:xml->getChildIterator())kit->deleteAllChildElementsWithTagName("Return");xml->setAttribute("schemaVersion",15);check(restored.restore(*xml) && restored.snapshot().kits[0].groupBoundary==0,"Old documents must retain continuous mapping");
        good(layout.setGroupBoundary(k,0));check(note(b)==26,"Continuous mapping did not close gaps");
        good(layout.setGroupBoundary(k,12));for(int n=0;n<6;++n){auto group=layout.addGroup(k).id;good(layout.addInstrument(k,group));}
        auto overflowGroup=layout.addGroup(k).id;auto before=layout.snapshot();check(!layout.addInstrument(k,overflowGroup).ok,"Out-of-range boundary accepted");check(layout.snapshot().revision==before.revision,"Rejected boundary mutated state");
    }
    struct AlertMode {bool previous=juce::LookAndFeel::getDefaultLookAndFeel().isUsingNativeAlertWindows();AlertMode(){juce::LookAndFeel::getDefaultLookAndFeel().setUsingNativeAlertWindows(false);}~AlertMode(){juce::LookAndFeel::getDefaultLookAndFeel().setUsingNativeAlertWindows(previous);}} alertMode;
    {
        DrumAudioProcessor blank;blank.initialiseLibraryDocuments();check(blank.librariesEmpty(),"Default Kit 1 must not appear as an open library");
        good(blank.rememberLibraryWorkspace());DrumAudioProcessor next(true);good(next.openSharedLibrary());check(next.libraryEditorProcessor()->librariesEmpty(),"Empty Library workspace was not remembered");
        auto& browser=*next.libraryEditorProcessor();auto created=browser.newLibraryDocument();good(created);check(!browser.librariesEmpty() && browser.libraryDocumentDirty(created.id),"New did not open an unsaved library from the empty screen");
        auto group=browser.getLibrary().snapshot().kits[0].groups[0].id;auto sound=browser.getLibrary().addInstrument(created.id,group);good(sound);
        {
            DrumAudioProcessorEditor editor(next);click(editor,"library-menu");juce::Component* screen=nullptr;for(auto* child:editor.getChildren())if(child->isVisible() && control(*child,"use-library-sound"))screen=child;check(screen!=nullptr,"Library screen missing for add flow");
            click(*screen,"use-library-sound");juce::MessageManager::getInstance()->runDispatchLoopUntil(30);auto* prompt=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());check(prompt && !next.hasOpenKit(),"Add without a kit did not ask New/Load/Close");prompt->exitModalState(0);juce::MessageManager::getInstance()->runDispatchLoopUntil(30);check(!next.hasOpenKit(),"Closing add prompt created a kit");
        }
        browser.librarySelectedGroup=group;browser.librarySelectedInstrument=sound.id;next.closeSharedLibrary();
        DrumAudioProcessor reopened(true);good(reopened.openSharedLibrary());auto& remembered=*reopened.libraryEditorProcessor();check(remembered.getLibrary().snapshot().activeKit==created.id && remembered.librarySelectedGroup==group && remembered.librarySelectedInstrument==sound.id && remembered.libraryDocumentDirty(created.id),"Remembered tabs, selection or unsaved draft lost");
        juce::MemoryBlock saved;reopened.getStateInformation(saved);good(remembered.closeLibraryDocument(created.id));reopened.closeSharedLibrary();
        DrumAudioProcessor recalled(true);recalled.setStateInformation(saved.getData(),(int)saved.getSize());good(recalled.openSharedLibrary());check(!recalled.libraryEditorProcessor()->librariesEmpty(),"Remembered empty preferences overrode saved Live Set workspace");
        DrumAudioProcessor fresh(true);good(fresh.openSharedLibrary());check(fresh.libraryEditorProcessor()->librariesEmpty(),"Closing all tabs did not persist");
    }
    {
        DrumAudioProcessor documents;
        auto id=documents.getLibrary().snapshot().activeKit;
        auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("library-document-tests").getChildFile(juce::Uuid().toString());
        auto first=folder.getChildFile("My drums.drumlib"),second=folder.getChildFile("New name.drumlib");
        check(documents.libraryDocumentDirty(id),"New library should be unsaved");
        good(documents.saveDocument(first));check(!documents.libraryDocumentDirty(id) && documents.libraryDocumentFile(id)==first,"Save failed to set tab baseline and path");
        auto state=documents.getLibrary().snapshot();good(documents.getLibrary().addInstrument(id,state.kits[0].groups[0].id));check(documents.libraryDocumentDirty(id),"Instrument edit missing dirty marker");
        good(documents.saveDocument(second));check(documents.getLibrary().snapshot().kits.size()==1 && documents.getLibrary().snapshot().kits[0].name=="New name","Save as opened another library or failed to rename tab");
        check(first.existsAsFile(),"Save as removed original file");
        auto copy=documents.duplicateLibraryDocument(id);good(copy);check(documents.getLibrary().snapshot().activeKit==copy.id && documents.libraryDocumentDirty(copy.id) && documents.libraryDocumentFile(copy.id)==juce::File(),"Duplicate did not open an unsaved independent copy");
        check(!documents.saveDocument(second).ok,"Duplicate overwrote another open tab's file");
        auto session=documents.libraryWorkspaceState();DrumAudioProcessor restored;check(restored.restoreLibraryWorkspace(*session),"Library workspace did not restore");check(restored.libraryDocumentDirty(copy.id) && !restored.libraryDocumentDirty(id) && restored.libraryDocumentFile(id)==second,"Workspace lost dirty state or filename");
        // Older saved baselines lack defaults introduced by later releases.
        auto legacy=documents.libraryWorkspaceState();
        for(auto* doc:legacy->getChildIterator())if(doc->hasTagName("Document"))if(auto* base=doc->getFirstChildElement())base->removeAttribute("groupBoundary");
        DrumAudioProcessor migrated;check(migrated.restoreLibraryWorkspace(*legacy),"Old baseline migration failed");
        check(!migrated.libraryDocumentDirty(id) && migrated.libraryDocumentDirty(copy.id),"Migration mistook defaults for edits or lost unsaved copy");
        good(migrated.getLibrary().rename(id,"Actual edit"));check(migrated.libraryDocumentDirty(id),"Migration suppressed a genuine edit");
        auto edited=migrated.libraryWorkspaceState();DrumAudioProcessor recalled;check(recalled.restoreLibraryWorkspace(*edited) && recalled.libraryDocumentDirty(id),"Genuine edit lost on recall");
        restored.discardLibraryDocuments();check(restored.getLibrary().snapshot().kits.size()==1,"Discard retained unsaved duplicate");
        good(restored.closeLibraryDocument(id));check(restored.librariesEmpty() && second.existsAsFile(),"Close deleted a file or kept last tab open");good(restored.openDocument(second));check(!restored.librariesEmpty() && !restored.libraryDocumentDirty(id),"Reopening last closed library failed");
        for(int n=1;n<8;++n)good(restored.duplicateLibraryDocument(id));check(!restored.duplicateLibraryDocument(id).ok,"More than eight libraries opened");
        auto eight=restored.getLibrary().snapshot();for(const auto& item:eight.kits)good(restored.closeLibraryDocument(item.id));check(restored.librariesEmpty(),"Closing all tabs failed");good(restored.newLibraryDocument());check(!restored.librariesEmpty() && restored.libraryDocumentsDirty(),"New after Close all failed");
    }
    {
        DrumAudioProcessor documents;
        auto state=documents.getLibrary().snapshot();auto id=state.activeKit;
        auto sound=documents.getLibrary().addInstrument(id,state.kits[0].groups[0].id);good(sound);
        drum::BuiltinEffect clipper(drum::BuiltinKind::clipper);juce::MemoryBlock bytes;
        clipper.getStateInformation(bytes);
        auto legacy=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),(int)bytes.getSize());
        legacy->setAttribute("version",2);juce::AudioProcessor::copyXmlToBinary(*legacy,bytes);
        drum::EffectSlot slot{juce::Uuid().toString(),drum::builtinDescription(drum::BuiltinKind::clipper).createXml()->toString(),bytes.toBase64Encoding(),false};
        good(documents.getLibrary().setEffects(id,sound.id,{slot}));
        auto file=juce::File::getCurrentWorkingDirectory().getChildFile("library-version-"+juce::Uuid().toString()+"/Saved.drumlib");
        good(documents.saveDocument(file));check(!documents.libraryDocumentDirty(id),"Saving a legacy effect library left it dirty");
        documents.getEffects().sync();documents.getEffects().capture();
        check(!documents.libraryDocumentDirty(id),"Effect state version upgrade was mistaken for an edit");
        auto workspace=documents.libraryWorkspaceState();DrumAudioProcessor recalled;
        check(recalled.restoreLibraryWorkspace(*workspace) && !recalled.libraryDocumentDirty(id),"Unchanged migrated library became dirty after recall");
        clipper.controls[1]->setValueNotifyingHost(clipper.controls[1]->convertTo0to1(-6.f));clipper.getStateInformation(bytes);slot.state=bytes.toBase64Encoding();
        good(recalled.getLibrary().setEffects(id,sound.id,{slot}));check(recalled.libraryDocumentDirty(id),"Effect migration comparison suppressed an actual parameter edit");
        good(recalled.saveDocument(file));recalled.getEffects().sync();recalled.getEffects().capture();
        check(!recalled.libraryDocumentDirty(id),"Saved effect changes did not clear dirty state");
    }
    {
        struct GestureProbe final : juce::AudioProcessorParameter::Listener {
            int values=0, starts=0, ends=0;
            void parameterValueChanged(int,float) override {++values;}
            void parameterGestureChanged(int,bool starting) override {starting?++starts:++ends;}
        } probe;
        DrumAudioProcessor track(true);
        juce::AudioProcessorParameter* link=nullptr;
        for(auto* parameter:track.getParameters())if(parameter->getName(64).startsWith("DJL Link "))link=parameter;
        check(link!=nullptr,"Missing Live connection parameter");
        link->addListener(&probe);track.useLiveKit();track.useLiveKit();link->removeListener(&probe);
        check(probe.values==2 && probe.starts==2 && probe.ends==2,"Update does not expose a complete Configure gesture");
        for(int n=0;n<drum::numMacros;++n)check(track.macro(n).get()==.5f,"Connection repair modified musical macros");
        check(track.autoRemapLiveClips(),"New instances must default to automatic clip remapping");
        track.setAutoRemapLiveClips(false);juce::MemoryBlock saved;track.getStateInformation(saved);DrumAudioProcessor recalled(true);recalled.setStateInformation(saved.getData(),(int)saved.getSize());check(!recalled.autoRemapLiveClips(),"Explicit Off choice must survive recall");
    }
    {
        drum::KitLibrary ordered;auto initial=ordered.snapshot();auto k=initial.activeKit,g=drum::activeKit(initial)->groups[0].id;
        auto h=ordered.addGroup(k).id,a=ordered.addInstrument(k,g).id,b=ordered.addInstrument(k,h).id,c=ordered.addInstrument(k,h).id;
        auto item=[&](const juce::String& id){auto state=ordered.snapshot();return *drum::findInstrument(*drum::activeKit(state),id);};
        auto contiguous=[&]{auto state=ordered.snapshot();int expected=drum::firstNote;for(const auto& group:drum::activeKit(state)->groups)for(const auto& instrument:group.instruments)check(instrument.note==expected++,"Keyboard has a gap or differs from kit order");};
        auto description=drum::builtinDescription(drum::BuiltinKind::utility);
        drum::EffectSlot effect{juce::Uuid().toString(),description.createXml()->toString(),{},false};effect.sidechainNote=item(b).note;effect.sidechainMute=true;
        good(ordered.setEffects(k,c,{effect}));auto x=ordered.addInstrument(k,g).id;contiguous();
        check(item(x).note==22 && item(b).note==23 && item(c).effects[0].sidechainNote==23,"Insert did not shift later group or sidechain");
        auto epoch=ordered.activeKitEpoch();good(ordered.reorderInstrument(k,c,-1));contiguous();check(ordered.activeKitEpoch()>epoch && item(c).effects[0].sidechainNote==item(b).note,"Reorder failed to invalidate old keys or preserve sidechain identity");
        good(ordered.moveInstrument(k,x,h));contiguous();check(item(x).note==24,"Group move did not map to destination position");
        auto copy=ordered.duplicateInstrument(k,a);good(copy);contiguous();check(item(copy.id).note==22,"Copy not placed at its group position");
        good(ordered.removeInstrument(k,a));contiguous();good(ordered.reorderGroup(k,h,-1));contiguous();check(item(c).note==21 && item(copy.id).note==24,"Group reorder did not move keyboard block");
        good(ordered.removeInstrument(k,b));contiguous();check(item(c).effects[0].sidechainNote==-1 && !item(c).effects[0].sidechainMute,"Deleted sidechain source retargeted another sound");
        auto sample=ordered.addSampleInstrument(k,h,juce::File::getCurrentWorkingDirectory().getChildFile("ordered-sample.wav").getFullPathName());good(sample);contiguous();check(item(sample.id).note==23 && item(copy.id).note==24,"Sample insertion did not shift later group");
        good(ordered.sliceInstrument(k,sample.id,2));contiguous();good(ordered.duplicateGroup(k,g));contiguous();
        effect.sidechainNote=item(x).note;good(ordered.setEffects(k,c,{effect}));auto legacy=ordered.toXml();for(auto* kit:legacy->getChildIterator())kit->deleteAllChildElementsWithTagName("Return");legacy->setAttribute("schemaVersion",11);
        for(auto* kit:legacy->getChildIterator())for(auto* group:kit->getChildIterator())for(auto* instrument:group->getChildIterator()){
            instrument->setAttribute("note",instrument->getIntAttribute("note")+30);
            for(auto* slot:instrument->getChildIterator())if(slot->hasTagName("Effect") && slot->getIntAttribute("sidechainNote",-1)>=0)slot->setAttribute("sidechainNote",slot->getIntAttribute("sidechainNote")+30);
        }
        check(ordered.restore(*legacy),"Legacy keyboard order migration failed");contiguous();check(item(c).effects[0].sidechainNote==item(x).note,"Migration changed sidechain source identity");
        auto saved=ordered.toXml()->toString();auto invalid=ordered.toXml();auto* group=invalid->getFirstChildElement()->getFirstChildElement();group->getChildElement(1)->setAttribute("note",group->getChildElement(0)->getIntAttribute("note"));check(!ordered.restore(*invalid) && ordered.toXml()->toString()==saved,"Duplicate note input was silently repaired or partially restored");
        good(ordered.removeGroup(k,h));contiguous();
    }

    drum::KitLibrary library;
    auto original=library.snapshot(); auto kit=original.activeKit; auto group=drum::activeKit(original)->groups.front().id;
    check(original.kits.size()==1,"Missing initial kit");
    check(!library.removeKit(kit).ok,"Last kit removed");
    check(!library.removeGroup(kit,group).ok,"Last group removed");
    check(!library.rename(group,"   ").ok,"Blank name accepted");
    check(!library.rename(group,juce::String::repeatedString("x",65)).ok,"Long name accepted");
    auto first=library.addInstrument(kit,group); good(first);
    auto second=library.addInstrument(kit,group); good(second);
    check(!library.assignNote(kit,first.id,60).ok,"Manual note broke contiguous mapping");
    check(!library.assignNote(kit,second.id,60).ok,"Note collision accepted");
    check(!library.assignNote(kit,second.id,109).ok,"Invalid note accepted");
    auto group2=library.addGroup(kit); good(group2);
    good(library.rename(first.id,"Kick <A> & B"));
    good(library.moveInstrument(kit,first.id,group2.id));
    good(library.reorderGroup(kit,group2.id,-1));
    auto s=library.snapshot();
    check(drum::findGroup(*drum::activeKit(s),group2.id)->instruments.front().id==first.id,"Move changed instrument identity");
    check(drum::findInstrument(*drum::activeKit(s),first.id)->note==21,"Organisation did not update MIDI note");
    good(library.removeInstrument(kit,second.id));
    auto duplicate=library.duplicateInstrument(kit,first.id); good(duplicate);
    s=library.snapshot(); check(duplicate.id!=first.id,"Duplicate reused instrument ID");
    check(drum::findInstrument(*drum::activeKit(s),duplicate.id)->note==22,"Duplicate not assigned next MIDI note");
    auto copyGroup=library.duplicateGroup(kit,group2.id); good(copyGroup);
    auto copyKit=library.duplicateKit(kit); good(copyKit);
    auto xml=library.toXml(); drum::KitLibrary restored;
    check(restored.restore(*xml),"Valid library rejected");
    check(restored.toXml()->toString()==xml->toString(),"Library changed on recall");
    check(restored.snapshot().activeKit==copyKit.id,"Active kit was not recalled");
    auto invalid=std::make_unique<juce::XmlElement>(*xml);
    invalid->setAttribute("activeKit","missing");
    check(!restored.restore(*invalid),"Missing active kit accepted");
    check(restored.toXml()->toString()==xml->toString(),"Invalid restore destroyed current library");
    invalid=std::make_unique<juce::XmlElement>(*xml);
    invalid->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument")->setAttribute("note","60garbage");
    check(!restored.restore(*invalid),"Malformed note accepted");
    invalid=std::make_unique<juce::XmlElement>(*xml);
    auto* child=invalid->getChildByName("Kit")->getChildByName("Group");
    child->getChildElement(1)->setAttribute("id",child->getChildElement(0)->getStringAttribute("id"));
    check(!restored.restore(*invalid),"Duplicate ID accepted");
    invalid=std::make_unique<juce::XmlElement>(*xml); invalid->setAttribute("schemaVersion",99);
    check(!restored.restore(*invalid),"Unknown state version accepted");
    while(library.snapshot().kits.size()<8) good(library.addKit());
    check(!library.addKit().ok && !library.duplicateKit(kit).ok,"Kit limit exceeded");
    good(library.activateKit(kit));
    while(drum::activeKit(library.snapshot())->groups.size()<16) good(library.addGroup(kit));
    check(!library.addGroup(kit).ok,"Group limit exceeded");
    while(drum::instrumentCount(*drum::activeKit(library.snapshot()))<88) good(library.addInstrument(kit,group));
    auto full=library.toXml()->toString();
    check(!library.addInstrument(kit,group).ok && !library.duplicateInstrument(kit,first.id).ok,"Instrument limit exceeded");
    check(full==library.toXml()->toString(),"Rejected edit changed state");
    s=library.snapshot(); std::set<int> notes;
    for(auto& g:drum::activeKit(s)->groups) for(auto& i:g.instruments) notes.insert(i.note);
    check(notes.size()==88 && *notes.begin()==21 && *notes.rbegin()==108,"88-note allocation incomplete");
    good(library.removeGroup(kit,group2.id));
    s=library.snapshot(); check(!drum::findInstrument(*drum::activeKit(s),first.id),"Group deletion left orphaned instrument");
    good(library.removeKit(kit));
    check(library.snapshot().activeKit!=kit,"Deleting active kit left invalid selection");
    juce::XmlElement legacy("DrumState"); legacy.setAttribute("schemaVersion",1);
    check(restored.restore(legacy) && restored.snapshot().kits.size()==1,"Legacy scaffold state did not migrate");

    DrumAudioProcessor processor;
    good(processor.getLibrary().addKit(true));
    juce::MemoryBlock state; processor.getStateInformation(state);
    DrumAudioProcessor recalled; recalled.setStateInformation(state.getData(),(int)state.getSize());
    check(processor.getLibrary().toXml()->toString()==recalled.getLibrary().toXml()->toString(),"DAW state round trip lost management data");
    recalled.setStateInformation("invalid",7);
    check(processor.getLibrary().toXml()->toString()==recalled.getLibrary().toXml()->toString(),"Malformed DAW state changed library");

    DrumAudioProcessorEditor editor(processor);
    screenshot(editor,"drum-layout.png");
    editor.setSize(1080,860); screenshot(editor,"drum-layout-minimum.png");
    editor.setSize(800,600); screenshot(editor,"drum-layout-compact.png");
    check(editor.getChildComponent(0)->getBoundsInParent().getWidth()<=801 && editor.getChildComponent(0)->getBoundsInParent().getHeight()<=601,"Compact editor exceeds window");
    editor.setSize(1080,860);
    click(editor,"add-group");
    auto* name=dynamic_cast<juce::TextEditor*>(control(editor,"group-name")); check(name!=nullptr,"Missing group name editor");
    name->setText("replace me"); name->setHighlightedRegion({0,10});
    for (auto character : juce::String("UI test group"))
        name->keyPressed(juce::KeyPress((int)character, juce::ModifierKeys(), character));
    check(name->getText()=="UI test group","Name editor rejected typed characters");
    name->onReturnKey();
    click(editor,"add-instrument");
    auto before=processor.getLibrary().snapshot(); auto* active=drum::activeKit(before);
    check(active->groups.back().name=="UI test group" && active->groups.back().instruments.size()==1,"Group/instrument UI actions not wired");
    auto instId=active->groups.back().instruments.front().id;
    auto* keys=dynamic_cast<juce::Label*>(control(editor,"instrument-note"));check(keys && keys->getText().contains("MIDI 33"),"Computed keyboard key missing from instrument details");
    auto* dest=dynamic_cast<juce::ComboBox*>(control(editor,"instrument-group")); check(dest!=nullptr,"Missing group selector");
    dest->setSelectedId(1,juce::dontSendNotification); dest->onChange();
    auto moved=processor.getLibrary().snapshot(); check(drum::findGroup(*drum::activeKit(moved),active->groups.front().id)->instruments.back().id==instId,"UI group move not applied");
    click(editor,"copy-instrument");
    click(editor,"copy-kit");
    check(processor.getLibrary().snapshot().kits.size()==3,"Kit copy button not wired");
    // Dense libraries must remain scrollable at the minimum editor size.
    auto dense=processor.getLibrary().snapshot(); auto denseKit=dense.activeKit; auto denseGroup=drum::activeKit(dense)->groups.front().id;
    while(drum::instrumentCount(*drum::activeKit(processor.getLibrary().snapshot()))<88) good(processor.getLibrary().addInstrument(denseKit,denseGroup));
    while(drum::activeKit(processor.getLibrary().snapshot())->groups.size()<16) good(processor.getLibrary().addGroup(denseKit));
    while(processor.getLibrary().snapshot().kits.size()<8) good(processor.getLibrary().addKit());
    good(processor.getLibrary().activateKit(denseKit));
    DrumAudioProcessorEditor denseEditor(processor); denseEditor.setSize(1080,860); screenshot(denseEditor,"drum-layout-dense.png");
    DrumAudioProcessor empty; DrumAudioProcessorEditor emptyEditor(empty); screenshot(emptyEditor,"drum-layout-empty.png");
    // The top-level editor swaps sibling screens and keeps the current kit intact.
    {
        DrumAudioProcessor track(true);good(track.openSharedLibrary());if(track.libraryEditorProcessor()->librariesEmpty())good(track.libraryEditorProcessor()->newLibraryDocument());track.newKit();auto before=track.getLibrary().snapshot();auto kitId=before.activeKit;
        good(track.getLibrary().rename(kitId,"Toolbar save test"));
        auto first=track.getLibrary().addInstrument(kitId,before.kits[0].groups[0].id).id;
        auto second=track.getLibrary().addInstrument(kitId,before.kits[0].groups[0].id).id;
        auto third=track.getLibrary().addInstrument(kitId,before.kits[0].groups[0].id).id;
        DrumAudioProcessorEditor main(track);main.setSize(1280,900);main.setVisible(true);
        check(!control(main,"kit-name")->isVisible() && control(main,"kit-0")->isVisible(),"Kit name must be a non-editable tab");
        check(control(main,"kit-menu")->isVisible() && control(main,"library-menu")->isVisible(),"File menus not visible");
        check(!control(main,"open-patterns")->isVisible(),"MIDI toolbar button still visible");
        auto* update=dynamic_cast<juce::TextButton*>(control(main,"use-kit-live"));check(update && !update->isVisible(),"Update should be hidden in favour of Setup reconnect");
        check(control(main,"live-setup")->getX()>control(main,"library-menu")->getRight(),"Setup must follow Library");check(control(main,"sound-editor")->getY()>=control(main,"library-menu")->getBottom(),"Sound editor overlaps toolbar");check(!control(main,"instrument-tuning")->isVisible(),"Redundant tuning remains visible");check(control(main,"play-sample")->getY()==control(main,"library-menu")->getY(),"Transport is not on toolbar row");auto* tempo=dynamic_cast<juce::TextEditor*>(control(main,"audition-tempo-entry"));check(tempo && tempo->getRight()==main.getWidth()-24,"Tempo is not a right-aligned text box");tempo->setText("137.5");tempo->onReturnKey();check(std::abs(track.getSampleEngine().auditionTempo()-137.5)<.01,"Typed tempo did not apply");
        click(main,"save-document");check(track.documentFile().getFileName()=="Toolbar save test.drumkit" && track.documentFile().existsAsFile(),"Save did not use kit name automatically");
        auto savedKit=track.documentFile();auto savedBytes=savedKit.loadFileAsString();
        good(track.getLibrary().rename(first,"Unsaved change"));click(main,"save-document");juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        check(juce::Component::getCurrentlyModalComponent()==nullptr && savedKit.loadFileAsString()!=savedBytes,"Save should update current kit without a warning");
        auto* pads=control(main,"instrument-grid");int side=(pads->getWidth()-60)/6;auto now=juce::Time::getCurrentTime();juce::Point<float> start{float(side/2),float(side/2)},finish{float(2*(side+12)+side/2),float(side/2)};
        auto event=[&](juce::Point<float> pos){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pos,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,pads,pads,now,start,now,1,false);};
        pads->mouseDown(event(start));pads->mouseDrag(event(finish));pads->mouseUp(event(finish));
        auto moved=track.getLibrary().snapshot().kits[0].groups[0].instruments;
        check(moved[0].id==second && moved[1].id==third && moved[2].id==first,"Pad drag did not insert and shift intervening pads");
        check(moved[0].note==21 && moved[1].note==22 && moved[2].note==23,"Pad drag left gaps in MIDI assignments");
        pads->mouseDoubleClick(event(start));check(!control(main,"sound-editor")->isVisible(),"Double-click opened Edit for a kit tile");click(main,"open-sound-editor");check(dynamic_cast<juce::TextButton*>(control(main,"close-sound-editor"))->getButtonText()=="Exit","Edit close button must say Exit");
        check(dynamic_cast<juce::TextButton*>(control(main,"open-sound-editor"))->getButtonText()=="Edit","Edit button still says Sound");click(main,"close-sound-editor");
        screenshot(main,"drum-toolbar-kit.png");click(main,"library-menu");
        juce::TextButton* back=nullptr;std::function<void(juce::Component&)> findBack=[&](juce::Component& c){if(auto* b=dynamic_cast<juce::TextButton*>(&c);b && b->getComponentID()=="kit-menu" && b->getButtonText()=="Close" && b->isVisible())back=b;for(auto* child:c.getChildren())findBack(*child);};findBack(main);
        check(back!=nullptr,"Library is not a screen in the main editor");
        auto* libraryScreen=back->getParentComponent();check(!control(*libraryScreen,"group-name")->isVisible() && !control(*libraryScreen,"copy-group")->isVisible() && !control(*libraryScreen,"delete-group")->isVisible(),"Old Library group controls remain visible");check(dynamic_cast<juce::TextButton*>(control(*libraryScreen,"add-group"))->getButtonText()=="+","Library group add is not a plus");auto countBefore=drum::instrumentCount(track.getLibrary().snapshot().kits[0]);
        click(*libraryScreen,"add-instrument");click(*libraryScreen,"use-library-sound");
        check(drum::instrumentCount(track.getLibrary().snapshot().kits[0])==countBefore+1,"Library screen could not add its instrument to the kit");
        auto* libraryPads=control(*libraryScreen,"instrument-grid");int librarySide=(libraryPads->getWidth()-60)/6;
        juce::Point<float> plus{float(librarySide-18),18};auto plusEvent=juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),plus,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,libraryPads,libraryPads,now,plus,now,1,false);
        libraryPads->mouseDown(plusEvent);libraryPads->mouseUp(plusEvent);check(drum::instrumentCount(track.getLibrary().snapshot().kits[0])==countBefore+2,"Tile plus did not add to the one active kit");
        auto tileEvent=plusEvent.withNewPosition(juce::Point<float>{float(librarySide/2),60.0f});libraryPads->mouseDoubleClick(tileEvent);check(!control(*libraryScreen,"sound-editor")->isVisible(),"Library tile double-click opened Edit");
        check(libraryPads->keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)),"Selected Library instrument did not handle Delete");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);auto* removal=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());check(removal!=nullptr,"Delete did not request confirmation");removal->exitModalState(0);juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        check(track.libraryEditorProcessor()->getLibrary().snapshot().kits[0].groups[0].instruments.size()==1,"Cancelling Delete removed the library sound");
        auto sample=juce::File::getCurrentWorkingDirectory().getChildFile("library-drop-"+juce::Uuid().toString()+".wav");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(sample.createOutputStream().release(),48000,1,16,{},0));check(writer!=nullptr,"Could not create drop fixture");juce::AudioBuffer<float> silence(1,32);silence.clear();writer->writeFromAudioSampleBuffer(silence,0,32);writer.reset();
        dynamic_cast<juce::FileDragAndDropTarget*>(libraryPads)->filesDropped({sample.getFullPathName()},librarySide/2,librarySide/2);check(track.libraryEditorProcessor()->getSampleEngine().waitForLoading(10000),"Library drop timed out");juce::MessageManager::getInstance()->runDispatchLoopUntil(40);juce::Timer::callPendingTimersSynchronously();auto dropped=track.libraryEditorProcessor()->getLibrary().snapshot().kits[0].groups[0].instruments;check(dropped.size()==2 && dropped[0].samplePath.isEmpty() && dropped[1].samplePath==sample.getFullPathName(),"Drop over a Library pad replaced it instead of adding");
        screenshot(main,"drum-toolbar-library.png");track.libraryEditorProcessor()->discardLibraryDocuments();back->onClick();
        check(track.getLibrary().snapshot().activeKit==kitId && control(main,"kit-menu")->isVisible(),"Library navigation lost the current kit");
    }
    {
        DrumAudioProcessor track(true);track.newKit();auto source=track.getLibrary().snapshot();auto kit=source.activeKit;auto group=source.kits[0].groups[0].id;
        good(track.getLibrary().rename(kit,"Original"));good(track.getLibrary().addInstrument(kit,group));
        auto result=track.duplicateActiveKit();good(result);auto duplicate=track.getLibrary().snapshot();
        check(duplicate.kits.size()==1 && duplicate.activeKit!=kit && duplicate.kits[0].name=="Original copy" && drum::instrumentCount(duplicate.kits[0])==1 && track.documentFile()==juce::File(),"Duplicate kit did not become an independent single active kit");
    }
    {
        DrumAudioProcessor source(true);source.newKit();auto state=source.getLibrary().snapshot();
        good(source.getLibrary().addInstrument(state.activeKit,state.kits[0].groups[0].id));
        auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("kit-load-"+juce::Uuid().toString());auto file=folder.getChildFile("Chosen name.drumkit");
        good(source.saveDocument(file));check(source.getLibrary().snapshot().kits[0].name=="Chosen name","Save As filename not applied to kit name");
        auto legacy=juce::parseXML(file);legacy->getChildByName("DrumState")->getFirstChildElement()->setAttribute("name","Kit 1 copy");file.replaceWithText(legacy->toString());
        DrumAudioProcessor loaded(true);loaded.newKit();good(loaded.openDocument(file));auto result=loaded.getLibrary().snapshot();
        check(result.kits[0].name=="Chosen name" && drum::instrumentCount(result.kits[0])==1 && loaded.documentFile()==file,"Load did not replace kit data and filename-derived name");
        good(loaded.renameActiveKit("New name"));check(loaded.documentFile()==juce::File() && file.existsAsFile(),"Rename did not detach from original file");
    }
    if(auto path=juce::SystemStats::getEnvironmentVariable("DJL_DRUM_WORKSPACE_FIXTURE",{});path.isNotEmpty()){
        auto xml=juce::parseXML(juce::File(path));DrumAudioProcessor fixture;check(xml && fixture.restoreLibraryWorkspace(*xml),"Workspace fixture restore failed");check(!fixture.libraryDocumentsDirty(),"Unedited real workspace reported dirty");fixture.getEffects().sync();fixture.getEffects().capture();check(!fixture.libraryDocumentsDirty(),"Loading unchanged effects marked real workspace dirty");DrumAudioProcessorEditor fixtureEditor(fixture);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);juce::Timer::callPendingTimersSynchronously();fixture.getEffects().capture();check(!fixture.libraryDocumentsDirty(),"Opening Library UI marked real workspace dirty");
    }
    {
        DrumAudioProcessor source(true),peer(true);source.newKit();auto initial=source.getLibrary().snapshot();auto id=initial.activeKit;
        auto instrument=source.getLibrary().addInstrument(id,initial.kits[0].groups[0].id);good(instrument);
        auto file=juce::File::getCurrentWorkingDirectory().getChildFile("linked-"+juce::Uuid().toString()+"/Shared.drumkit");good(source.saveDocument(file));good(peer.openDocument(file));
        good(source.getLibrary().rename(instrument.id,"Linked kick"));source.syncLinkedKit();peer.syncLinkedKit();
        check(peer.getLibrary().snapshot().kits[0].groups[0].instruments[0].name=="Linked kick","Same-file kit edit did not propagate");
        auto controls=peer.getLibrary().snapshot().kits[0].groups[0].instruments[0].controls;controls.pan=-.5;good(peer.getLibrary().setControls(id,instrument.id,controls));peer.syncLinkedKit();source.syncLinkedKit();
        check(source.getLibrary().snapshot().kits[0].groups[0].instruments[0].controls.pan==-.5,"Linked controls did not propagate in reverse");
        source.macro(0).setValueNotifyingHost(.8f);source.syncLinkedKit();peer.syncLinkedKit();check(std::abs(peer.macro(0).get()-.8f)<.001f,"Linked macro did not propagate");
        auto effects=drum::factoryChain(1);good(source.getLibrary().setEffects(id,instrument.id,effects));source.syncLinkedKit();peer.syncLinkedKit();
        check(peer.getLibrary().snapshot().kits[0].groups[0].instruments[0].effects.size()==effects.size(),"Linked effect chain did not propagate");
        auto group=source.getLibrary().addGroup(id);good(group);good(source.getLibrary().moveInstrument(id,instrument.id,group.id));source.syncLinkedKit();peer.syncLinkedKit();
        check(peer.getLibrary().snapshot().kits[0].groups.back().instruments[0].id==instrument.id,"Linked group move lost instrument identity");
        good(peer.duplicateActiveKit());good(source.getLibrary().rename(instrument.id,"Original only"));source.syncLinkedKit();peer.syncLinkedKit();
        check(peer.getLibrary().snapshot().kits[0].groups.back().instruments[0].name=="Linked kick","Duplicate remained linked to original");
    }
    {
        DrumAudioProcessor library;auto state=library.getLibrary().snapshot();auto kit=state.activeKit,group=state.kits[0].groups[0].id;
        auto file=juce::File::getCurrentWorkingDirectory().getChildFile("aiff-"+juce::Uuid().toString()+".aif");juce::AiffAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(file.createOutputStream().release(),48000,1,16,{},0));check(writer!=nullptr,"AIFF writer failed");
        juce::AudioBuffer<float> audio(1,128);audio.clear();audio.setSample(0,32,.5f);writer->writeFromAudioSampleBuffer(audio,0,128);writer.reset();
        library.getSampleEngine().requestImport(kit,group,file);check(library.getSampleEngine().waitForLoading(10000),"AIFF import timed out");check(drum::instrumentCount(library.getLibrary().snapshot().kits[0])==1,"Standard AIFF failed to import");
        auto unsupported=file.getSiblingFile("content-"+file.getFileName());juce::MemoryOutputStream bytes;bytes.write("FORM",4);bytes.writeIntBigEndian(34);bytes.write("AIFC",4);bytes.write("COMM",4);bytes.writeIntBigEndian(22);for(int n=0;n<18;++n)bytes.writeByte(0);bytes.write("able",4);unsupported.replaceWithData(bytes.getData(),bytes.getDataSize());
        library.getSampleEngine().requestImport(kit,group,unsupported);check(library.getSampleEngine().waitForLoading(10000),"Unsupported AIFF timed out");check(library.getSampleEngine().statusFor(group).message.startsWith("Ableton Content"),"Ableton Content did not produce clear import guidance");
    }
    {
        DrumAudioProcessor track(true);track.newKit();auto state=track.getLibrary().snapshot();auto id=state.activeKit;auto instrument=track.getLibrary().addInstrument(id,state.kits[0].groups[0].id);good(instrument);auto destination=track.getLibrary().addGroup(id);good(destination);
        DrumAudioProcessorEditor editor(track);editor.setSize(1280,900);auto* pads=control(editor,"instrument-grid");auto* groups=dynamic_cast<juce::ListBox*>(control(editor,"groups"));
        auto start=juce::Point<float>(30,50);auto end=pads->getLocalPoint(groups,groups->getRowPosition(1,true).getCentre()).toFloat();auto time=juce::Time::getCurrentTime();
        auto event=[&](juce::Point<float> pos){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pos,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,pads,pads,time,start,time,1,false);};
        pads->mouseDown(event(start));pads->mouseDrag(event(end));pads->mouseUp(event(end));
        auto after=track.getLibrary().snapshot();check(after.kits[0].groups[0].instruments.empty() && after.kits[0].groups[1].instruments[0].id==instrument.id,"Dragging a tile onto a group failed");
        auto* pan=dynamic_cast<juce::Slider*>(control(editor,"instrument-pan"));check(pan->getTextFromValue(0)=="C" && pan->getTextFromValue(-50).contains("L") && pan->getTextFromValue(50).contains("R"),"Pan direction labels missing");
    }
    std::cout << "Management, persistence and UI wiring tests passed\n";
}
