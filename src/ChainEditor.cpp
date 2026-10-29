#include "ChainEditor.h"

namespace
{
const juce::Colour bg(0xff10151f),panel(0xff202b3b),ink(0xffe5ebf5),muted(0xff9aabba),mint(0xff67dfc5);
juce::File settingsFile(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("DJL/Drum/plugin-folders.txt");}
}
class ChainEditor::Card final : public juce::Component
{
public:
    Card(ChainEditor& o,drum::EffectSlot s,int index):owner(o),slot(std::move(s)),position(index)
    {
        juce::PluginDescription description;if(auto xml=juce::parseXML(slot.description))description.loadFromXml(*xml);
        title=description.name;setComponentID("chain-slot-"+juce::String(position));
        for(auto* button:{&edit,&bypass,&earlier,&later,&remove,&retry})addAndMakeVisible(*button);
        edit.setComponentID("chain-edit-"+juce::String(position));bypass.setComponentID("chain-bypass-"+juce::String(position));
        remove.setComponentID("chain-remove-"+juce::String(position));
        edit.onClick=[this]{owner.effects.openEditor(slot.id);};
        bypass.setToggleState(slot.bypassed,juce::dontSendNotification);
        bypass.onClick=[this]{owner.apply(owner.effects.bypass(owner.kitId,owner.instrumentId,slot.id,!slot.bypassed));};
        earlier.onClick=[this]{owner.apply(owner.effects.move(owner.kitId,owner.instrumentId,slot.id,position-1));};
        later.onClick=[this]{owner.apply(owner.effects.move(owner.kitId,owner.instrumentId,slot.id,position+1));};
        remove.onClick=[this]{owner.apply(owner.effects.remove(owner.kitId,owner.instrumentId,slot.id));};
        retry.onClick=[this]{owner.effects.retry(slot.id);update();};
        earlier.setTooltip("Move earlier in the audio chain");later.setTooltip("Move later in the audio chain");
        update();
    }
    void update()
    {
        info=owner.effects.status(slot.id);const bool ready=owner.effects.instance(slot.id)!=nullptr;
        edit.setEnabled(ready);retry.setVisible(!ready);earlier.setEnabled(position>0);later.setEnabled(position+1<(int)owner.slots.size());repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1),7);
        g.setColour(slot.bypassed?muted:mint);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),7,1);
        g.setFont(juce::FontOptions(14,juce::Font::bold));g.drawText(juce::String(position+1)+". "+title,10,8,getWidth()-20,25,juce::Justification::centredLeft,true);
        g.setFont(juce::FontOptions(11));g.setColour(muted);g.drawFittedText(info,10,36,getWidth()-20,32,juce::Justification::topLeft,2);
        g.setFont(juce::FontOptions(10));g.drawText("Drag header to reorder",10,getHeight()-18,getWidth()-20,14,juce::Justification::centredLeft);
    }
    void resized() override
    {
        edit.setBounds(10,75,72,25);bypass.setBounds(88,75,92,25);
        earlier.setBounds(10,106,32,24);later.setBounds(47,106,32,24);remove.setBounds(88,106,92,24);retry.setBounds(10,137,170,24);
    }
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if(event.getDistanceFromDragStart()>5 && event.getMouseDownY()<70 && !owner.isDragAndDropActive())owner.startDragging("slot:"+slot.id,this);
    }
private:
    ChainEditor& owner;drum::EffectSlot slot;int position;juce::String title,info;
    juce::TextButton edit{"Open"},bypass{"Bypass"},earlier{"<"},later{">"},remove{"Remove"},retry{"Retry loading"};
};
ChainEditor::ChainEditor(drum::KitLibrary& model,drum::EffectRack& rack):library(model),effects(rack)
{
    setComponentID("plugin-chain");
    addAndMakeVisible(search);search.setTextToShowWhenEmpty("Find a VST3 effect...",muted);search.setComponentID("chain-search");
    search.setColour(juce::TextEditor::backgroundColourId,bg);search.setColour(juce::TextEditor::textColourId,ink);
    search.onTextChange=[this]{filtered.clear();for(const auto& f:files)if(f.getFileNameWithoutExtension().containsIgnoreCase(search.getText()))filtered.push_back(f);browser.updateContent();browser.repaint();};
    addAndMakeVisible(browser);browser.setModel(this);browser.setRowHeight(27);browser.setColour(juce::ListBox::backgroundColourId,bg);browser.setComponentID("chain-browser");
    for(auto* button:{&folder,&rescan,&add})addAndMakeVisible(*button);
    add.onClick=[this]{const int row=browser.getSelectedRow();if(row>=0 && row<(int)filtered.size())addFile(filtered[(size_t)row]);};
    rescan.onClick=[this]{scan();};
    folder.onClick=[this]{
        chooser=std::make_unique<juce::FileChooser>("Choose a folder containing VST3 effects");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe=juce::Component::SafePointer<ChainEditor>(this)](const juce::FileChooser& fc){
            if(!safe || fc.getResult()==juce::File())return;
            safe->roots.addIfNotAlreadyThere(fc.getResult().getFullPathName());auto saved=settingsFile();saved.getParentDirectory().createDirectory();saved.replaceWithText(safe->roots.joinIntoString("\n"));safe->scan();
        });
    };
    addAndMakeVisible(viewport);viewport.setViewedComponent(&flow,false);viewport.setScrollBarsShown(false,true);viewport.setComponentID("chain-flow");
    addAndMakeVisible(status);status.setFont(juce::FontOptions(11));status.setColour(juce::Label::textColourId,muted);
    status.setText("Drag an effect from the browser into the chain, or drop a .vst3 file/folder.",juce::dontSendNotification);
    roots.addLines(settingsFile().loadFileAsString());
   #if JUCE_WINDOWS
    auto common=juce::SystemStats::getEnvironmentVariable("COMMONPROGRAMFILES","C:\\Program Files\\Common Files");roots.addIfNotAlreadyThere(juce::File(common).getChildFile("VST3").getFullPathName());
    roots.addIfNotAlreadyThere(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("VST3").getFullPathName());
   #elif JUCE_MAC
    roots.addIfNotAlreadyThere("/Library/Audio/Plug-Ins/VST3");roots.addIfNotAlreadyThere(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/VST3").getFullPathName());
   #else
    roots.addIfNotAlreadyThere("/usr/lib/vst3");roots.addIfNotAlreadyThere(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3").getFullPathName());
   #endif
    scan();startTimerHz(5);
}
ChainEditor::~ChainEditor(){stopTimer();browser.setModel(nullptr);viewport.setViewedComponent(nullptr,false);}
void ChainEditor::scan()
{
    files.clear();juce::StringArray seen;
    std::function<void(const juce::File&,int)> visit=[&](const juce::File& directory,int depth){
        if(depth>8 || !directory.isDirectory())return;
        for(const auto& child:directory.findChildFiles(juce::File::findFilesAndDirectories,false))
        {
            if(child.hasFileExtension("vst3"))
            {
                if(child.getFileNameWithoutExtension()=="DJL Drum")continue;
                if(!seen.contains(child.getFullPathName(),true)){seen.add(child.getFullPathName());files.push_back(child);}
            }
            else if(child.isDirectory() && !child.isSymbolicLink())visit(child,depth+1);
        }
    };
    for(const auto& root:roots)if(juce::File::isAbsolutePath(root))visit(juce::File(root),0);
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.getFileName().compareIgnoreCase(b.getFileName())<0;});
    search.onTextChange();
}
void ChainEditor::select(const juce::String& kit,const juce::String& instrument)
{
    if(kitId==kit && instrumentId==instrument)return;kitId=kit;instrumentId=instrument;slots.clear();cards.clear();refresh();
}
void ChainEditor::refresh()
{
    const auto state=library.snapshot();const auto* kit=drum::activeKit(state);const auto* instrument=kit->id==kitId?drum::findInstrument(*kit,instrumentId):nullptr;
    auto next=instrument?instrument->effects:std::vector<drum::EffectSlot>{};
    // Parameter-state capture should not recreate the UI under the pointer.
    auto appearance=[](const auto& a,const auto& b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].id!=b[i].id || a[i].description!=b[i].description || a[i].bypassed!=b[i].bypassed)return false;return true;};
    if(!appearance(slots,next))
    {
        slots=std::move(next);cards.clear();
        for(size_t i=0;i<slots.size();++i){auto card=std::make_unique<Card>(*this,slots[i],(int)i);flow.addAndMakeVisible(*card);cards.push_back(std::move(card));}
        resized();repaint();
    }
    for(auto& card:cards)card->update();add.setEnabled(instrument!=nullptr && !loading && slots.size()<drum::maxEffects);
}
void ChainEditor::paint(juce::Graphics& g)
{
    g.setColour(ink);g.setFont(juce::FontOptions(13));g.drawText("VST3 browser",0,0,220,22,juce::Justification::centredLeft);
    g.drawText("Instrument audio  >  effects in order  >  output / sends",238,0,getWidth()-238,22,juce::Justification::centredLeft);
    if(cards.empty())
    {
        g.setColour(bg);g.fillRoundedRectangle(viewport.getBounds().toFloat().reduced(3),6);g.setColour(muted);
        g.drawFittedText("Drop VST3 audio effects here\n\nEmpty chain: audio passes through unchanged",viewport.getBounds().reduced(22),juce::Justification::centred,4);
    }
}
void ChainEditor::resized()
{
    search.setBounds(0,27,220,25);browser.setBounds(0,58,220,juce::jmax(40,getHeight()-128));
    folder.setBounds(0,getHeight()-62,106,25);rescan.setBounds(114,getHeight()-62,106,25);add.setBounds(0,getHeight()-31,220,25);
    viewport.setBounds(238,27,juce::jmax(1,getWidth()-238),juce::jmax(1,getHeight()-60));
    const int height=juce::jmax(184,viewport.getHeight()-16);flow.setSize(juce::jmax(viewport.getWidth(),(int)cards.size()*206+100),height);
    for(size_t i=0;i<cards.size();++i)cards[i]->setBounds((int)i*206,0,192,height);
    status.setBounds(238,getHeight()-29,getWidth()-238,27);
}
int ChainEditor::insertionAt(juce::Point<int> point) const
{
    return juce::jlimit(0,(int)slots.size(),(flow.getLocalPoint(this,point).x+100)/206);
}
bool ChainEditor::isInterestedInDragSource(const SourceDetails& source)
{
    auto text=source.description.toString();return text.startsWith("vst3:") || text.startsWith("slot:");
}
void ChainEditor::itemDropped(const SourceDetails& source)
{
    const auto text=source.description.toString();int position=insertionAt(source.localPosition);
    if(text.startsWith("vst3:"))addFile(juce::File(text.substring(5)),position);
    else if(text.startsWith("slot:"))
    {
        auto id=text.substring(5);for(int i=0;i<(int)slots.size();++i)if(slots[(size_t)i].id==id && i<position)--position;
        apply(effects.move(kitId,instrumentId,id,position));
    }
}
bool ChainEditor::isInterestedInFileDrag(const juce::StringArray& names)
{
    return !names.isEmpty() && std::all_of(names.begin(),names.end(),[](const auto& name){return juce::File::isAbsolutePath(name) && juce::File(name).hasFileExtension("vst3");});
}
void ChainEditor::filesDropped(const juce::StringArray& names,int x,int y)
{
    if(!isInterestedInFileDrag(names))return;int position=insertionAt({x,y});for(const auto& name:names)addFile(juce::File(name),position++);
}
void ChainEditor::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected)
{
    if(row<0 || row>=(int)filtered.size())return;if(selected){g.setColour(panel);g.fillAll();}
    g.setColour(selected?mint:ink);g.setFont(juce::FontOptions(12));g.drawText(filtered[(size_t)row].getFileNameWithoutExtension(),7,0,width-14,height,juce::Justification::centredLeft,true);
}
void ChainEditor::listBoxItemDoubleClicked(int row,const juce::MouseEvent&){if(row>=0 && row<(int)filtered.size())addFile(filtered[(size_t)row]);}
juce::var ChainEditor::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    if(rows.isEmpty() || rows[0]<0 || rows[0]>=(int)filtered.size())return {};return "vst3:"+filtered[(size_t)rows[0]].getFullPathName();
}
void ChainEditor::addFile(const juce::File& file,int position)
{
    if(instrumentId.isEmpty())return;
    status.setText("Loading "+file.getFileNameWithoutExtension()+"...",juce::dontSendNotification);loading=true;
    const auto kit=kitId,instrument=instrumentId;
    juce::MessageManager::callAsync([safe=juce::Component::SafePointer<ChainEditor>(this),file,position,kit,instrument]{
        if(!safe)return;auto found=safe->effects.discover(file);safe->loading=false;
        if(found.isEmpty()){safe->status.setText("No supported audio effect found. Use a VST3 effect, not a Live device or instrument.",juce::dontSendNotification);return;}
        if(found.size()==1){safe->apply(safe->effects.add(kit,instrument,*found[0],position));return;}
        std::vector<juce::PluginDescription> descriptions;juce::PopupMenu menu;
        for(int i=0;i<found.size();++i){descriptions.push_back(*found[i]);menu.addItem(i+1,found[i]->name);}
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(safe.getComponent()),[safe,descriptions,position,kit,instrument](int choice){if(safe && choice>0)safe->apply(safe->effects.add(kit,instrument,descriptions[(size_t)choice-1],position));});
    });
}
void ChainEditor::apply(const drum::Result& result)
{
    status.setText(result.ok?"Chain saved with this instrument. Drag headers to change the processing order.":result.message,juce::dontSendNotification);
    // A button may have called us; retire its card after the callback returns.
    juce::MessageManager::callAsync([safe=juce::Component::SafePointer<ChainEditor>(this)]{if(safe)safe->refresh();});
}
