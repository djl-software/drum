#include "ChainEditor.h"

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
        for(int row=0;row<2;++row)
        {
            addAndMakeVisible(parameters[row]);addAndMakeVisible(values[row]);
            values[row].setComponentID("chain-value-"+juce::String(position)+"-"+juce::String(row));
            values[row].setSliderStyle(juce::Slider::LinearHorizontal);values[row].setTextBoxStyle(juce::Slider::TextBoxRight,false,70,22);
            values[row].setRange(0,1);parameters[row].onChange=[this]{update();};
            values[row].textFromValueFunction=[this,row](double v){auto* p=parameter(row);return p?p->getText((float)v,32)+" "+p->getLabel():juce::String{};};
            values[row].valueFromTextFunction=[this,row](const juce::String& text){auto* p=parameter(row);return p?(double)p->getValueForText(text):0.0;};
            values[row].onValueChange=[this,row]{if(auto* p=parameter(row)){p->beginChangeGesture();p->setValueNotifyingHost((float)values[row].getValue());p->endChangeGesture();}};
        }
        update();
    }
    void update()
    {
        info=owner.effects.status(slot.id);const bool ready=owner.effects.instance(slot.id)!=nullptr;
        auto* plugin=owner.effects.instance(slot.id);const int count=plugin?plugin->getParameters().size():0;
        if(count!=parameterCount)
        {
            parameterCount=count;
            for(int row=0;row<2;++row)
            {
                parameters[row].clear(juce::dontSendNotification);
                for(int n=0;n<count;++n)parameters[row].addItem(plugin->getParameters()[n]->getName(64),n+1);
                parameters[row].setSelectedId(count>row?row+1:count,juce::dontSendNotification);
            }
        }
        for(int row=0;row<2;++row)
        {
            auto* p=parameter(row);parameters[row].setEnabled(p!=nullptr);values[row].setEnabled(p!=nullptr);
            if(p && !values[row].isMouseButtonDown())
            {
                const int steps=p->getNumSteps();values[row].setRange(0,1,steps>1 && steps<10000?1.0/(steps-1):0);
                values[row].setDoubleClickReturnValue(true,p->getDefaultValue());
                values[row].setValue(p->getValue(),juce::dontSendNotification);values[row].updateText();
            }
        }
        edit.setTooltip(info);bypass.setTooltip(info);
        edit.setEnabled(ready);retry.setVisible(!ready);earlier.setEnabled(position>0);later.setEnabled(position+1<(int)owner.slots.size());repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1),7);
        g.setColour(slot.bypassed?muted:mint);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),7,1);
        g.setFont(juce::FontOptions(14,juce::Font::bold));g.drawText(juce::String(position+1)+". "+title,10,8,getWidth()-20,25,juce::Justification::centredLeft,true);
    }
    void resized() override
    {
        edit.setBounds(10,34,72,23);bypass.setBounds(88,34,92,23);
        for(int row=0;row<2;++row){parameters[row].setBounds(10,63+row*49,170,23);values[row].setBounds(10,87+row*49,170,23);}
        earlier.setBounds(10,166,32,24);later.setBounds(47,166,32,24);remove.setBounds(88,166,92,24);retry.setBounds(10,63,170,25);
    }
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if(event.getDistanceFromDragStart()>5 && event.getMouseDownY()<33 && !owner.isDragAndDropActive())owner.startDragging("slot:"+slot.id,this);
    }
private:
    juce::AudioProcessorParameter* parameter(int row) const
    {
        auto* plugin=owner.effects.instance(slot.id);const int index=parameters[row].getSelectedId()-1;
        return plugin && index>=0 && index<plugin->getParameters().size()?plugin->getParameters()[index]:nullptr;
    }
    juce::ComboBox parameters[2];juce::Slider values[2];int parameterCount=-1;
    ChainEditor& owner;drum::EffectSlot slot;int position;juce::String title,info;
    juce::TextButton edit{"Open"},bypass{"Bypass"},earlier{"<"},later{">"},remove{"Remove"},retry{"Retry loading"};
};
ChainEditor::ChainEditor(drum::KitLibrary& model,drum::EffectRack& rack):library(model),effects(rack)
{
    setComponentID("plugin-chain");
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
    for(auto& card:cards)card->update();load.setEnabled(instrument!=nullptr && slots.size()<drum::maxEffects);
}
void ChainEditor::paint(juce::Graphics& g)
{
    g.setColour(ink);g.setFont(juce::FontOptions(13));
    g.drawText("Instrument audio  >  effects in order  >  output / sends",0,0,getWidth()-140,22,juce::Justification::centredLeft);
    if(cards.empty())
    {
        g.setColour(bg);g.fillRoundedRectangle(viewport.getBounds().toFloat().reduced(3),6);g.setColour(muted);
        g.drawFittedText("Drop VST3 audio effects here\n\nEmpty chain: audio passes through unchanged",viewport.getBounds().reduced(22),juce::Justification::centred,4);
    }
}
void ChainEditor::resized()
{
    load.setBounds(getWidth()-126,0,126,25);
    viewport.setBounds(0,33,juce::jmax(1,getWidth()),juce::jmax(1,getHeight()-66));
    const int height=juce::jmax(200,viewport.getHeight()-16);flow.setSize(juce::jmax(viewport.getWidth()-16,(int)cards.size()*206+100),height);
    for(size_t i=0;i<cards.size();++i)cards[i]->setBounds((int)i*206,0,192,height);
    status.setBounds(0,getHeight()-29,getWidth(),27);
}
int ChainEditor::insertionAt(juce::Point<int> point) const
{
    return juce::jlimit(0,(int)slots.size(),(flow.getLocalPoint(this,point).x+100)/206);
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
    browserBridge->poll(kitId+"/"+instrumentId,viewport.getBounds());
}
void ChainEditor::browserDropped(const juce::var& value)
{
    const auto* matches=value["matches"].getArray();if(!matches || matches->isEmpty())return;
    std::vector<juce::File> files;juce::PopupMenu menu;
    for(const auto& match:*matches)
    {
        auto path=match["path"].toString();if(!juce::File::isAbsolutePath(path))continue;
        juce::File file(path);if(!file.hasFileExtension("vst3") || !file.exists())continue;
        if(!match["id"].toString().startsWith("device:vst3:audiofx:"))continue;
        files.push_back(file);menu.addItem((int)files.size(),match["vendor"].toString()+" "+match["name"].toString()+" — "+path);
    }
    if(files.empty())return;
    if(files.size()==1){addFile(files.front());return;}
    status.setText("Multiple installed VST3 matches. Choose the effect to add.",juce::dontSendNotification);
    const auto kit=kitId,instrument=instrumentId;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safe=juce::Component::SafePointer<ChainEditor>(this),files,kit,instrument](int choice){
            if(safe && safe->kitId==kit && safe->instrumentId==instrument && choice>0 && choice<=(int)files.size())safe->addFile(files[(size_t)choice-1]);
        });
}
