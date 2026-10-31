#include "PatternEditor.h"
#include "PluginEditor.h"
#include <stdexcept>
namespace {
juce::Component* control(juce::Component& root,const juce::String& id){if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* found=control(*child,id))return found;return nullptr;}
juce::MouseEvent mouse(juce::Component& c,juce::Point<float> point,juce::Point<float> anchor){auto now=juce::Time::getCurrentTime();return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&c,&c,now,anchor,now,1,false};}
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}void good(const drum::Result& result){if(!result.ok)throw std::runtime_error(result.message.toStdString());}}
void runPatternTests(){
    drum::KitLibrary model;auto state=model.snapshot();auto kitId=state.activeKit,group=state.kits[0].groups[0].id;
    auto kick=model.addInstrument(kitId,group).id,snare=model.addInstrument(kitId,group).id;auto patternId=model.addPattern(kitId).id;
    auto kit=model.snapshot().kits[0];auto pattern=kit.patterns[0];pattern.hits={{kick,0,110},{snare,2,65}};good(model.setPattern(kitId,pattern));
    good(model.reorderInstrument(kitId,snare,-1));kit=model.snapshot().kits[0];check(kit.patterns[0].hits==pattern.hits,"Reorder changed instrument identities in a pattern");
    // The Live map labels the current assignments without copying audio devices.
    {
        auto mapKit=kit;mapKit.name="Kit & <test>";mapKit.groups[0].instruments[0].name="Snare & <rim>";
        auto mapFile=juce::File::getCurrentWorkingDirectory().getChildFile("drum-live-note-map-test.adg");
        good(drum::LiveNoteMap::write(mapKit,mapFile));juce::FileInputStream packed(mapFile);
        juce::GZIPDecompressorInputStream unpacked(&packed,false,juce::GZIPDecompressorInputStream::gzipFormat);
        auto xml=juce::parseXML(unpacked.readEntireStreamAsString());check(xml!=nullptr,"Live map is not gzip XML");
        auto* preset=xml->getChildByName("GroupDevicePreset");auto* rack=preset->getChildByName("Device")->getChildByName("MidiEffectGroupDevice");
        check(rack->getChildByName("ShowsZonesInsteadOfNoteNames")->getBoolAttribute("Value"),"Live note names disabled");
        auto* branches=preset->getChildByName("BranchPresets");check(branches->getNumChildElements()==2,"Wrong number of named key zones");
        int n=0;for(auto* branch:branches->getChildIterator()){
            const auto& instrument=mapKit.groups[0].instruments[(size_t)n++];
            check(branch->getChildByName("Name")->getStringAttribute("Value")==instrument.name,"Live map loses escaped instrument name");
            check(branch->getChildByName("DevicePresets")->getNumChildElements()==0,"Live map introduces MIDI processing");
            auto* range=branch->getChildByName("ZoneSettings")->getChildByName("KeyRange");
            for(auto tag:{"Min","Max","CrossfadeMin","CrossfadeMax"})check(range->getChildByName(tag)->getIntAttribute("Value")==instrument.note,"Live map changes note assignment");
        }
        auto linked=drum::LiveNoteMap::create(drum::LiveNoteMap::linkedKit(mapKit));
        auto* linkedBranches=linked->getChildByName("GroupDevicePreset")->getChildByName("BranchPresets");
        check(linkedBranches->getNumChildElements()==88,"Linked rack must reserve every kit key");
        for(int key=21;key<=108;++key)check(linkedBranches->getChildElement(key-21)->getChildByName("ZoneSettings")->getChildByName("KeyRange")->getChildByName("Min")->getIntAttribute("Value")==key,"Linked rack key order changed");
        good(drum::LiveNoteMap::writeLinked(mapKit,juce::File::getCurrentWorkingDirectory().getChildFile("DJL Drum Link.adg")));
        mapKit.groups[0].instruments[1].note=mapKit.groups[0].instruments[0].note;
        check(!drum::LiveNoteMap::write(mapKit,mapFile).ok,"Live map permits duplicated MIDI routes");
        mapKit.groups.clear();check(!drum::LiveNoteMap::write(mapKit,mapFile).ok,"Empty Live map should not be exported");
    }
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("pattern-midi-test.mid");good(drum::PatternPlayer::exportMidi(kit,kit.patterns[0],120,file));juce::FileInputStream stream(file);juce::MidiFile midi;check(midi.readFrom(stream) && midi.getTimeFormat()==960 && midi.getNumTracks()==1,"Invalid exported MIDI");
    auto* sequence=midi.getTrack(0);int hits=0;for(int n=0;n<sequence->getNumEvents();++n){auto message=sequence->getEventPointer(n)->message;if(!message.isNoteOn())continue;++hits;if(message.getTimeStamp()==0)check(message.getNoteNumber()==22 && message.getVelocity()==110,"Kick export did not follow its new key");else check(message.getTimeStamp()==480 && message.getNoteNumber()==21 && message.getVelocity()==65,"Snare timing or velocity changed");}check(hits==2 && sequence->getEndTime()==3840,"Pattern length or hit count changed");
    drum::PatternPlayer player(model);player.prepare(1000);player.start(patternId);juce::MidiBuffer events;events.ensureSize(100000);player.process(events,2048,120);int starts=0;for(const auto event:events)if(event.getMessage().isNoteOn()){check(event.samplePosition==0 || event.samplePosition==250 || event.samplePosition==2000,"Pattern playback timing wrong");++starts;}check(starts==3,"Pattern looping missed or duplicated a hit");player.stop();events.clear();player.process(events,16,120);check(!events.isEmpty() && (*events.begin()).getMessage().isNoteOff(),"Pattern stop left a held MIDI note");
    drum::KitLibrary restored;check(restored.restore(*model.toXml()) && restored.snapshot().kits[0].patterns[0].hits==pattern.hits,"Pattern recall changed hit identities");
    auto copy=model.duplicateKit(kitId);good(copy);auto copied=model.snapshot();auto* duplicate=drum::activeKit(copied);check(duplicate->patterns[0].id!=patternId && duplicate->patterns[0].hits[0].instrumentId!=kick && drum::findInstrument(*duplicate,duplicate->patterns[0].hits[0].instrumentId),"Copied pattern points back to original kit");
    good(model.removeInstrument(kitId,kick));auto after=model.snapshot();const drum::Kit* original=nullptr;for(const auto& k:after.kits)if(k.id==kitId)original=&k;check(original && original->patterns[0].hits.size()==1 && original->patterns[0].hits[0].instrumentId==snare,"Deleted instrument pattern hit was reassigned");
    // Schema 13 files had no note IDs. Repeated reads must give stable IDs for shared-library merge.
    auto legacy=model.toXml();legacy->setAttribute("schemaVersion",13);drum::KitLibrary migratedA,migratedB;check(migratedA.restore(*legacy) && migratedB.restore(*legacy) && migratedA.toXml()->toString()==migratedB.toXml()->toString(),"Legacy MIDI IDs change on every load");
    // Off-grid timing, duration, probability, release velocity, quantise strength and transformations.
    auto flexible=pattern;flexible.hits[0].start=.13;flexible.hits[0].duration=.71;flexible.hits[0].chance=.4;flexible.hits[0].deviation=20;flexible.hits[0].releaseVelocity=32;
    drum::MidiSelection selection{flexible.hits[0].id};drum::MidiEditing::apply(flexible,kit,selection,1,.25,.5);check(std::abs(flexible.hits[0].beat()-.19)<1.e-8 && flexible.hits[0].duration==.71,"Quantise amount or free note duration failed");
    drum::MidiEditing::apply(flexible,kit,selection,8,.25);check(flexible.hits.size()==3 && flexible.hits.back().id!=flexible.hits.front().id && flexible.hits.back().chance==.4,"Duplicate lost note identity or expression");
    auto invalid=pattern;invalid.hits[0].duration=-1;check(!restored.setPattern(kitId,invalid).ok,"Invalid note duration accepted");
    // Probability groups choose one note per cycle, not one note per event time.
    auto grouped=pattern;for(auto& h:grouped.hits){h.chanceGroup="test-group";h.groupMode=2;h.chance=.5;}good(restored.setPattern(kitId,grouped));drum::PatternPlayer groupedPlayer(restored);groupedPlayer.prepare(1000);groupedPlayer.start(grouped.id);events.clear();groupedPlayer.process(events,2000,120);int chosen=0;for(const auto event:events)if(event.getMessage().isNoteOn())++chosen;check(chosen==1,"Play-one probability group did not choose exactly one hit");
    for(auto& h:grouped.hits){h.groupMode=0;h.chance=0;}good(restored.setPattern(kitId,grouped));groupedPlayer.start(grouped.id);events.clear();groupedPlayer.process(events,2000,120);for(const auto event:events)check(!event.getMessage().isNoteOn(),"Zero-chance note played");
    // The main screen must create a library even when no kit is open.
    {DrumAudioProcessor root(true);good(root.openSharedLibrary());auto count=root.libraryEditorProcessor()->getLibrary().snapshot().kits.size();DrumAudioProcessorEditor main(root);auto* create=dynamic_cast<juce::TextButton*>(control(main,"new-library"));check(create && create->isVisible() && create->isEnabled(),"New library control unavailable without a kit");create->onClick();check(!root.hasOpenKit() && root.libraryEditorProcessor()->getLibrary().snapshot().kits.size()==count+1,"New library action did not create a collection");}
    { // Publication belongs to the processor, so no editor needs to exist.
        juce::File publication;
        {
            DrumAudioProcessor first(true),second(true);first.newKit();second.newKit();
            auto firstName=first.getParameters().getLast()->getName(128),secondName=second.getParameters().getLast()->getName(128);
            check(first.getParameters().getLast()->isAutomatable(),"Live link identifier hidden from host scripting");
            check(firstName.startsWith("DJL Link ") && firstName!=secondName,"Live instances share a link token");
            auto initial=first.getLibrary().snapshot();good(first.getLibrary().addInstrument(initial.activeKit,initial.kits[0].groups[0].id));
            first.setAutoRemapLiveClips(true);juce::MemoryBlock savedLink;first.getStateInformation(savedLink);second.setStateInformation(savedLink.getData(),(int)savedLink.getSize());
            check(second.autoRemapLiveClips(),"Live auto-remap option was not saved/restored");
            first.publishLiveKit();publication=drum::LiveKitLink::folder().getChildFile(firstName.substring(9)+".json");
            auto payload=juce::JSON::parse(publication);check((bool)payload["autoRemap"],"Live remap option not published");check(payload["notes"].size()==1,"Kit was not published without an editor");
            check((int)payload["notes"][0]["note"]==21,"Published wrong kit note");
        }
        check(!publication.existsAsFile(),"Link file survives processor destruction");
    }
    DrumAudioProcessor processor(true);processor.newKit();auto initial=processor.getLibrary().snapshot();good(processor.getLibrary().addInstrument(initial.activeKit,initial.kits[0].groups[0].id));DrumPatternEditor editor(processor);editor.setSize(1000,640);editor.open();auto* roll=control(editor,"midi-piano-roll");check(roll,"Piano roll is missing");roll->mouseDoubleClick(mouse(*roll,{50,13},{50,13}));auto edited=[&]{return processor.getLibrary().snapshot().kits[0].patterns[0];};check(edited().hits.size()==1 && edited().hits[0].beat()==.5,"Double click did not create a MIDI note");
    roll->mouseDown(mouse(*roll,{60,13},{60,13}));roll->mouseDrag(mouse(*roll,{160,13},{60,13}));roll->mouseUp(mouse(*roll,{160,13},{60,13}));check(edited().hits[0].beat()==1.5,"Dragging did not move the MIDI note");
    roll->mouseDown(mouse(*roll,{174,13},{174,13}));roll->mouseDrag(mouse(*roll,{224,13},{174,13}));roll->mouseUp(mouse(*roll,{224,13},{174,13}));check(edited().hits[0].duration==.75,"Dragging note edge did not resize it");
    editor.keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),'z'));check(edited().hits[0].duration==.25,"MIDI undo failed");editor.keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),'z'));check(edited().hits[0].duration==.75,"MIDI redo failed");
    // New notes follow the chosen grid, even after selecting/resizing a longer note.
    auto* grid=dynamic_cast<juce::ComboBox*>(control(editor,"midi-grid"));check(grid,"Grid selector missing");
    grid->setSelectedId(5,juce::sendNotificationSync);roll->mouseDoubleClick(mouse(*roll,{275,13},{275,13}));check(edited().hits.back().duration==.125,"1/32 grid created a note of the wrong length");
    grid->setSelectedId(6,juce::sendNotificationSync);roll->mouseDoubleClick(mouse(*roll,{325,13},{325,13}));check(edited().hits.back().duration==.0625,"1/64 grid created a note of the wrong length");
    editor.keyPressed(juce::KeyPress('3',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),'3'));roll->mouseDoubleClick(mouse(*roll,{375,13},{375,13}));check(std::abs(edited().hits.back().duration-1.0/24)<1.e-8,"Triplet grid note length incorrect");
    auto* lanes=control(editor,"midi-expression-lanes");auto* laneSelector=control(editor,"midi-lane-selector");check(lanes && laneSelector,"Expression lane controls missing");
    check(laneSelector->getY()==editor.getHeight()-62,"Lane selector is not inline with bottom values");int oneHeight=lanes->getHeight();editor.setExpressionLaneVisible(1,true);check(lanes->getHeight()>oneHeight,"Multiple expression lanes not displayed");
    auto beforeChance=edited().hits[0].chance;lanes->mouseDown(mouse(*lanes,{320,135},{320,135}));lanes->mouseUp(mouse(*lanes,{320,135},{320,135}));check(edited().hits[0].chance<beforeChance && edited().hits[0].velocity==100,"Chance lane edited the wrong property");
    auto beforeHeight=lanes->getHeight();lanes->mouseDown(mouse(*lanes,{250,2},{250,2}));lanes->mouseDrag(mouse(*lanes,{250,-18},{250,2}));lanes->mouseUp(mouse(*lanes,{250,-18},{250,2}));check(lanes->getHeight()>beforeHeight,"Expression lane resize failed");
    editor.setExpressionLaneVisible(0,false);editor.setExpressionLaneVisible(1,false);check(!lanes->isVisible() && lanes->getHeight()==0,"Hide all lanes did not collapse them");editor.setExpressionLaneVisible(0,true);editor.setExpressionLaneVisible(1,true);
    auto* number=dynamic_cast<juce::Slider*>(control(editor,"midi-value-Velocity"));check(number && number->getSliderStyle()!=juce::Slider::IncDecButtons,"Number field still uses +/- buttons");
    int oldVelocity=edited().hits[0].velocity;number->mouseDown(mouse(*number,{30,20},{30,20}));number->mouseDrag(mouse(*number,{30,0},{30,20}));number->mouseDrag(mouse(*number,{30,-20},{30,20}));number->mouseUp(mouse(*number,{30,-20},{30,20}));check(edited().hits[0].velocity==oldVelocity+10,"Number dragging did not change velocity");
    editor.keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),'z'));check(edited().hits[0].velocity==oldVelocity,"Numeric drag did not undo as one gesture");juce::MouseWheelDetails wheel{};wheel.deltaY=.1f;number->mouseWheelMove(mouse(*number,{30,10},{30,10}),wheel);check(edited().hits[0].velocity==oldVelocity+1,"Number wheel step incorrect");
    number->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));juce::TextEditor* textEntry=nullptr;for(auto* child:number->getChildren())if(auto* entry=dynamic_cast<juce::TextEditor*>(child);entry && entry->isVisible())textEntry=entry;check(textEntry,"Number field text entry missing");textEntry->setText("72",false);textEntry->onReturnKey();check(edited().hits[0].velocity==72,"Typed numeric entry failed");
    auto* randomButton=dynamic_cast<juce::TextButton*>(control(editor,"midi-randomize"));auto* randomRange=dynamic_cast<juce::Slider*>(control(editor,"midi-random-amount"));check(randomButton && randomRange && randomButton->getX()>laneSelector->getRight(),"Randomize is not to the right of View");
    lanes->mouseDown(mouse(*lanes,{20,130},{20,130}));lanes->mouseUp(mouse(*lanes,{20,130},{20,130}));check(randomButton->getTooltip().contains("Chance"),"Clicking lane header did not select it");auto beforeRandom=edited();randomRange->setValue(10,juce::sendNotificationSync);randomButton->onClick();auto afterRandom=edited();check(std::abs(afterRandom.hits[0].chance-beforeRandom.hits[0].chance)<=.100001 && afterRandom.hits[0].velocity==72,"Randomize changed the wrong lane or exceeded its range");for(size_t n=1;n<afterRandom.hits.size();++n)check(afterRandom.hits[n]==beforeRandom.hits[n],"Randomize changed unselected notes");
    check(laneSelector->getX()>=250 && randomRange->getX()>randomButton->getRight(),"Expression controls not grouped beneath chart");check(editor.noteExpressionText(edited().hits[0])==juce::String((int)std::round(edited().hits[0].chance*100))+"%","Selected chance not shown in note");
    lanes->mouseDown(mouse(*lanes,{20,130},{20,130}));lanes->mouseUp(mouse(*lanes,{20,130},{20,130}));check(editor.noteExpressionText(edited().hits[0]).isEmpty() && !randomButton->isEnabled(),"Second lane click did not clear note value");
    lanes->mouseDown(mouse(*lanes,{20,30},{20,30}));lanes->mouseUp(mouse(*lanes,{20,30},{20,30}));check(editor.noteExpressionText(edited().hits[0])=="72","Velocity lane did not display velocity in note");
    auto png=juce::File::getCurrentWorkingDirectory().getChildFile("drum-pattern-editor.png");png.deleteFile();{juce::FileOutputStream output(png);juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),output);}
}
