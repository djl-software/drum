#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    auto* b=dynamic_cast<juce::TextButton*>(control(root,id)); check(b && b->isEnabled(),"Missing or disabled UI action"); b->onClick();
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
    drum::KitLibrary library;
    auto original=library.snapshot(); auto kit=original.activeKit; auto group=drum::activeKit(original)->groups.front().id;
    check(original.kits.size()==1,"Missing initial kit");
    check(!library.removeKit(kit).ok,"Last kit removed");
    check(!library.removeGroup(kit,group).ok,"Last group removed");
    check(!library.rename(group,"   ").ok,"Blank name accepted");
    check(!library.rename(group,juce::String::repeatedString("x",65)).ok,"Long name accepted");
    auto first=library.addInstrument(kit,group); good(first);
    auto second=library.addInstrument(kit,group); good(second);
    good(library.assignNote(kit,first.id,60));
    check(!library.assignNote(kit,second.id,60).ok,"Note collision accepted");
    check(!library.assignNote(kit,second.id,109).ok,"Invalid note accepted");
    auto group2=library.addGroup(kit); good(group2);
    good(library.rename(first.id,"Kick <A> & B"));
    good(library.moveInstrument(kit,first.id,group2.id));
    good(library.reorderGroup(kit,group2.id,-1));
    auto s=library.snapshot();
    check(drum::findGroup(*drum::activeKit(s),group2.id)->instruments.front().id==first.id,"Move changed instrument identity");
    check(drum::findInstrument(*drum::activeKit(s),first.id)->note==60,"Organisation changed MIDI note");
    good(library.removeInstrument(kit,second.id));
    auto duplicate=library.duplicateInstrument(kit,first.id); good(duplicate);
    s=library.snapshot(); check(duplicate.id!=first.id,"Duplicate reused instrument ID");
    check(drum::findInstrument(*drum::activeKit(s),duplicate.id)->note!=60,"Duplicate reused MIDI note");
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
    auto* keys=dynamic_cast<juce::ComboBox*>(control(editor,"instrument-note")); check(keys!=nullptr,"Missing note selector");
    keys->setSelectedId(101,juce::dontSendNotification); keys->onChange();
    auto assigned=processor.getLibrary().snapshot(); check(drum::findInstrument(*drum::activeKit(assigned),instId)->note==100,"UI note selection not applied");
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
    std::cout << "Management, persistence and UI wiring tests passed\n";
}
