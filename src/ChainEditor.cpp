#include "ChainEditor.h"
#include "ResetSlider.h"
#include "BuiltinEffects.h"

namespace
{
const juce::Colour bg(0xff10151f),panel(0xff202b3b),ink(0xffe5ebf5),muted(0xff9aabba),mint(0xff67dfc5);

}
class ChainEditor::Card final : public juce::Component
{
public:
    Card(ChainEditor& o,drum::EffectSlot s,int index):owner(o),slot(std::move(s)),position(index)
    {
        juce::PluginDescription description;if(auto xml=juce::parseXML(slot.description))description.loadFromXml(*xml);
        title=description.name;setComponentID("chain-slot-"+juce::String(position));
        for(auto* button:{&edit,&bypass,&earlier,&later,&remove,&retry,&mute})addAndMakeVisible(*button);
        edit.setComponentID("chain-edit-"+juce::String(position));bypass.setComponentID("chain-bypass-"+juce::String(position));remove.setComponentID("chain-remove-"+juce::String(position));
        edit.onClick=[this]{owner.effects.openEditor(slot.id);};
        bypass.setToggleState(slot.bypassed,juce::dontSendNotification);
        bypass.onClick=[this]{owner.apply(owner.effects.bypass(owner.kitId,owner.instrumentId,slot.id,!slot.bypassed));};
        earlier.onClick=[this]{owner.apply(owner.effects.move(owner.kitId,owner.instrumentId,slot.id,position-1));};
        later.onClick=[this]{owner.apply(owner.effects.move(owner.kitId,owner.instrumentId,slot.id,position+1));};
        remove.onClick=[this]{owner.apply(owner.effects.remove(owner.kitId,owner.instrumentId,slot.id));};
        retry.onClick=[this]{owner.effects.retry(slot.id);update();};
        addAndMakeVisible(source);source.addItem("No Input",1);
        auto state=owner.library.snapshot();const auto* kit=drum::activeKit(state);
        for(const auto& group:kit->groups)for(const auto& i:group.instruments)source.addItem(i.name+" ("+drum::noteName(i.note)+")",i.note+2);
        source.setSelectedId(slot.sidechainNote<0?1:slot.sidechainNote+2,juce::dontSendNotification);
        source.setTooltip("Sidechain source: another instrument, before its effects. No Input supplies silence to the external sidechain.");
        for(auto* slider:{&gain,&mix}){addAndMakeVisible(*slider);useValueBar(*slider);slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,64,20);}
        gain.setRange(-60,24,0.1);gain.setTextValueSuffix(" dB");gain.setValue(slot.sidechainGain);gain.setDoubleClickReturnValue(true,0);
        mix.setRange(0,100,0.1);mix.setTextValueSuffix(" %");mix.setValue(slot.sidechainMix*100);mix.setDoubleClickReturnValue(true,100);
        gain.setComponentID("chain-sidechain-gain-"+juce::String(position));mix.setComponentID("chain-sidechain-mix-"+juce::String(position));
        mute.setToggleState(slot.sidechainMute,juce::dontSendNotification);mute.setClickingTogglesState(true);
        mute.setTooltip("Mute the selected source in the kit output while it still feeds this sidechain. Does not mute this effect's own instrument.");
        auto save=[this]{slot.sidechainNote=source.getSelectedId()==1?-1:source.getSelectedId()-2;slot.sidechainGain=gain.getValue();slot.sidechainMix=mix.getValue()/100;slot.sidechainMute=mute.getToggleState();owner.apply(owner.effects.sidechain(owner.kitId,owner.instrumentId,slot.id,slot.sidechainNote,slot.sidechainGain,slot.sidechainMix,slot.sidechainMute));};
        source.onChange=save;gain.onValueChange=save;mix.onValueChange=save;mute.onClick=save;update();
    }
    void update()
    {
        if(position<(int)owner.slots.size())slot=owner.slots[(size_t)position];
        source.setSelectedId(slot.sidechainNote<0?1:slot.sidechainNote+2,juce::dontSendNotification);
        if(!gain.isMouseButtonDown())gain.setValue(slot.sidechainGain,juce::dontSendNotification);
        if(!mix.isMouseButtonDown())mix.setValue(slot.sidechainMix*100,juce::dontSendNotification);
        mute.setToggleState(slot.sidechainMute,juce::dontSendNotification);
        info=owner.effects.status(slot.id);const bool ready=owner.effects.instance(slot.id)!=nullptr;
        const bool sidechain=owner.effects.supportsSidechain(slot.id);
        source.setVisible(sidechain);gain.setVisible(sidechain);mix.setVisible(sidechain);mute.setVisible(sidechain);
        edit.setEnabled(ready);retry.setVisible(!ready);earlier.setEnabled(position>0);later.setEnabled(position+1<(int)owner.slots.size());repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1),7);
        g.setColour(slot.bypassed?muted:mint);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),7,1);
        g.setFont(juce::FontOptions(12,juce::Font::bold));g.drawText(juce::String(position+1)+". "+title,8,3,getWidth()-22,23,juce::Justification::centredLeft,true);
        g.setFont(juce::FontOptions(11));g.setColour(muted);
        if(source.isVisible()){drawControlLabel(g,"Gain",{8,80,38,26});drawControlLabel(g,"Mix",{8,110,38,26});}
        else g.drawFittedText(info+"\nUse Open for plugin controls",8,63,144,80,juce::Justification::centred,3);
        const auto peaks=owner.effects.peaks(slot.id);
        for(int c=0;c<2;++c){
            juce::Rectangle<float> bar((float)getWidth()-14+c*5,31,3,127);
            float db=juce::Decibels::gainToDecibels(peaks[(size_t)c],-60.f);
            auto y=[&](float value){return bar.getBottom()-(value+60)/66*bar.getHeight();};
            const float limits[]={-60,-12,-3,6};const juce::Colour colours[]={juce::Colour(0xff53d58b),juce::Colour(0xffffcf5c),juce::Colour(0xffff636f)};
            for(int zone=0;zone<3;++zone){float low=limits[zone],high=limits[zone+1];g.setColour(colours[zone].withAlpha(.14f));g.fillRect(bar.getX(),y(high),bar.getWidth(),y(low)-y(high));float lit=juce::jlimit(low,high,db);g.setColour(colours[zone]);g.fillRect(bar.getX(),y(lit),bar.getWidth(),y(low)-y(lit));}
        }
    }
    void resized() override
    {
        edit.setBounds(8,29,64,21);bypass.setBounds(78,29,76,21);source.setBounds(8,53,146,21);
        gain.setBounds(52,80,102,26);mix.setBounds(52,110,102,26);mute.setBounds(8,140,146,20);
        earlier.setBounds(8,164,24,20);later.setBounds(36,164,24,20);remove.setBounds(66,164,88,20);retry.setBounds(8,54,146,22);
    }
    void mouseDrag(const juce::MouseEvent& e) override {if(e.getDistanceFromDragStart()>5 && e.getMouseDownY()<33 && !owner.isDragAndDropActive())owner.startDragging("slot:"+slot.id,this);}
private:
    ChainEditor& owner;drum::EffectSlot slot;int position;juce::String title,info;
    juce::ComboBox source;juce::Slider gain,mix;
    juce::TextButton edit{"Open"},bypass{"Bypass"},earlier{"<"},later{">"},remove{"Remove"},retry{"Retry loading"},mute{"Mute source"};
};
ChainEditor::ChainEditor(drum::KitLibrary& model,drum::EffectRack& rack):library(model),effects(rack)
{
    setComponentID("plugin-chain");
    addAndMakeVisible(builtin);builtin.setComponentID("chain-add-builtin");
    builtin.onClick=[this]{juce::PopupMenu menu;for(int n=0;n<9;++n)menu.addItem(n+1,drum::builtinName((drum::BuiltinKind)n));menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&builtin),[safe=juce::Component::SafePointer<ChainEditor>(this)](int choice){if(safe && choice>0 && choice<=9)safe->apply(safe->effects.add(safe->kitId,safe->instrumentId,drum::builtinDescription((drum::BuiltinKind)(choice-1))));});};
    addAndMakeVisible(presets);presets.setComponentID("chain-presets");presets.onClick=[this]{showChainMenu();};
    addAndMakeVisible(load);load.setComponentID("chain-load");
    load.onClick=[this]{
        chooser=std::make_unique<juce::FileChooser>("Choose a VST3 audio effect",juce::File{},"*.vst3");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [safe=juce::Component::SafePointer<ChainEditor>(this)](const juce::FileChooser& fc){
                if(safe && fc.getResult()!=juce::File())safe->addFile(fc.getResult());
            });
    };
    addAndMakeVisible(viewport);viewport.setViewedComponent(&flow,false);viewport.setScrollBarsShown(true,true);viewport.setComponentID("chain-flow");
    addAndMakeVisible(status);status.setFont(juce::FontOptions(11));status.setColour(juce::Label::textColourId,muted);
    status.setText("Drop a VST3 effect into the chain, or use Load VST3.",juce::dontSendNotification);
    browserBridge=std::make_unique<LiveBrowserBridge>(*this);
    browserBridge->onDrop=[this](const juce::var& value){browserDropped(value);};
    startTimerHz(10);
}
ChainEditor::~ChainEditor(){stopTimer();browserBridge.reset();viewport.setViewedComponent(nullptr,false);}
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
    else slots=std::move(next);
    for(auto& card:cards)card->update();load.setEnabled(instrument!=nullptr && slots.size()<drum::maxEffects);builtin.setEnabled(load.isEnabled());presets.setEnabled(instrument!=nullptr);
}
void ChainEditor::paint(juce::Graphics& g)
{
    g.setColour(ink);g.setFont(juce::FontOptions(13));
    g.drawText("Instrument chain: Drum effects + VST3, processed left to right",0,0,getWidth(),22,juce::Justification::centredLeft);
    if(cards.empty())
    {
        g.setColour(bg);g.fillRoundedRectangle(viewport.getBounds().toFloat().reduced(3),6);g.setColour(muted);
        g.drawFittedText("Add effect\n\nEmpty chain: audio passes through unchanged",viewport.getBounds().reduced(22),juce::Justification::centred,4);
    }
}
void ChainEditor::resized()
{
    builtin.setBounds(0,24,112,25);load.setBounds(120,24,112,25);presets.setBounds(240,24,96,25);
    viewport.setBounds(0,58,juce::jmax(1,getWidth()),juce::jmax(1,getHeight()-91));
    const int height=190;flow.setSize(juce::jmax(viewport.getWidth()-16,(int)cards.size()*188+40),height);
    for(size_t i=0;i<cards.size();++i)cards[i]->setBounds((int)i*188,0,176,height);
    status.setBounds(0,getHeight()-29,getWidth(),27);
}
int ChainEditor::insertionAt(juce::Point<int> point) const
{
    return juce::jlimit(0,(int)slots.size(),(flow.getLocalPoint(this,point).x+90)/188);
}
bool ChainEditor::isInterestedInDragSource(const SourceDetails& source)
{
    auto text=source.description.toString();return text.startsWith("slot:");
}
void ChainEditor::itemDropped(const SourceDetails& source)
{
    const auto text=source.description.toString();int position=insertionAt(source.localPosition);
    if(text.startsWith("slot:"))
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
void ChainEditor::addFile(const juce::File& file,int position)
{
    if(instrumentId.isEmpty())return;
    status.setText("Loading "+file.getFileNameWithoutExtension()+"...",juce::dontSendNotification);
    const auto kit=kitId,instrument=instrumentId;
    juce::MessageManager::callAsync([safe=juce::Component::SafePointer<ChainEditor>(this),file,position,kit,instrument]{
        if(!safe)return;auto found=safe->effects.discover(file);
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

void ChainEditor::timerCallback()
{
    if(isShowing())refresh();
    browserBridge->poll(kitId+"/"+instrumentId,{0,54,getWidth(),26});
}
void ChainEditor::browserDropped(const juce::var& value)
{
    if(value["error"].toString().isNotEmpty()){status.setText(value["error"].toString(),juce::dontSendNotification);return;}
    const auto* matches=value["matches"].getArray();if(!matches || matches->isEmpty())return;
    std::vector<juce::File> files;juce::PopupMenu menu;
    menu.addSectionHeader(bool(value["nativeNameClash"])?"Choose VST3 - Live also uses this name":"Choose VST3 effect");
    for(const auto& match:*matches)
    {
        auto path=match["path"].toString();if(!juce::File::isAbsolutePath(path))continue;
        juce::File file(path);if(!file.hasFileExtension("vst3") || !file.exists())continue;
        if(!match["id"].toString().startsWith("device:vst3:audiofx:"))continue;
        files.push_back(file);menu.addItem((int)files.size(),match["vendor"].toString()+" "+match["name"].toString()+" - "+path);
    }
    if(files.empty())return;
    if(files.size()==1 && !bool(value["requiresConfirmation"])){addFile(files.front());return;}
    status.setText("Choose which VST3 effect to add.",juce::dontSendNotification);
    const auto kit=kitId,instrument=instrumentId;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safe=juce::Component::SafePointer<ChainEditor>(this),files,kit,instrument](int choice){
            if(safe && safe->kitId==kit && safe->instrumentId==instrument && choice>0 && choice<=(int)files.size())safe->addFile(files[(size_t)choice-1]);
        });
}

void ChainEditor::showChainMenu()
{
    juce::PopupMenu menu;menu.addItem(1,"Default: empty chain");menu.addItem(2,"Kick punch");menu.addItem(3,"Snare snap");menu.addSeparator();menu.addItem(4,"Copy chain");menu.addItem(5,"Paste chain (replace)",clipboard.has_value());menu.addItem(6,"Save as default for new instruments");menu.addItem(7,"Apply saved default (replace)");menu.addItem(8,"Empty chain");menu.addSeparator();menu.addItem(9,"Save preset file...");menu.addItem(10,"Load preset file (replace)...");
    const auto kit=kitId,instrument=instrumentId;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presets),[safe=juce::Component::SafePointer<ChainEditor>(this),kit,instrument](int choice){
        if(!safe || safe->kitId!=kit || safe->instrumentId!=instrument || !choice)return;
        auto& self=*safe;
        if(choice<=3)self.apply(self.effects.replaceChain(kit,instrument,drum::factoryChain(choice-1)));
        else if(choice==4){clipboard=self.effects.copyChain(kit,instrument);self.status.setText("Chain copied, including effect settings.",juce::dontSendNotification);}
        else if(choice==5 && clipboard)self.apply(self.effects.replaceChain(kit,instrument,*clipboard));
        else if(choice==6){bool ok=drum::saveChain(drum::defaultChainFile(),self.effects.copyChain(kit,instrument));self.status.setText(ok?"Default saved for new instruments. Existing instruments unchanged; sidechain sources reset.":"Could not save default chain.",juce::dontSendNotification);}
        else if(choice==7)self.apply(self.effects.replaceChain(kit,instrument,drum::defaultChain()));
        else if(choice==8)self.apply(self.effects.replaceChain(kit,instrument,{}));
        else if(choice==9 || choice==10){bool save=choice==9;self.chooser=std::make_unique<juce::FileChooser>(save?"Save Drum chain preset":"Load Drum chain preset",juce::File{},"*.drumchain");self.chooser->launchAsync((save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,[safe,kit,instrument,save](const juce::FileChooser& fc){if(!safe || fc.getResult()==juce::File{})return;if(save){bool ok=drum::saveChain(fc.getResult().withFileExtension("drumchain"),safe->effects.copyChain(kit,instrument));safe->status.setText(ok?"Preset saved.":"Could not save preset.",juce::dontSendNotification);}else if(auto slots=drum::loadChain(fc.getResult()))safe->apply(safe->effects.replaceChain(kit,instrument,*slots));else safe->status.setText("Invalid Drum chain preset.",juce::dontSendNotification);});}
    });
}
