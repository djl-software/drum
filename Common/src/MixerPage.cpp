#include "PluginProcessor.h"
#if JUCE_MAC
#include "../../Mac/src/GroupEditor.h"
#endif
#include "platform/HostFloat.h"
#include "MixerPage.h"
#include "MixerSendBar.h"
#include "MixerReadouts.h"
#include <cmath>

namespace {
const juce::Colour background(0xff10151f),panel(0xff192430),text(0xffe5ebf5),muted(0xff8d9bb0),accent(0xff67dfc5);
juce::Colour colour(int index){static const juce::Colour colours[]{juce::Colour(0xffe6af72),juce::Colour(0xffbba3e6),juce::Colour(0xff79cbd6),juce::Colour(0xffdf94b6),juce::Colour(0xff8eade6),juce::Colour(0xffddce80),juce::Colour(0xffb4d591),juce::Colour(0xffbd9b84)};return colours[index%8];}
juce::String dbText(double db){return db<=-70?juce::String::fromUTF8("\xe2\x88\x92\xe2\x88\x9e"):juce::String(db,1);}
}
class DrumMixerPage::Strip final : public juce::Component
{
public:
    Strip(DrumMixerPage& page,juce::String target,int group,int ret):owner(page),id(std::move(target)),groupIndex(group),returnIndex(ret)
    {
        for(auto* c:std::initializer_list<juce::Component*>{&name,&fader,&pan,&mute,&solo,&output,&peakReading,&levelReading})addAndMakeVisible(c);
        if(isGroup()){
#if JUCE_MAC
        addAndMakeVisible(processing);processing.setButtonText("Chain & sends");processing.setComponentID("group-processing-"+id);processing.onClick=[this]{owner.openGroupProcessing(id);};
#endif
        }
        name.setComponentID("mixer-name-"+id);name.onClick=[this]{if(isGroup()){
#if JUCE_MAC
if(owner.selected==id)owner.openGroupProcessing(id);else owner.chooseGroup(id);
#else
owner.chooseGroup(id);
#endif
}else if(isInstrument() && owner.onOpenInstrument)owner.onOpenInstrument(id);};
        fader.getProperties().set("mixer-fader",true);fader.setSliderStyle(juce::Slider::LinearVertical);fader.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);fader.setRange(isInstrument()?-60:-70,12,.1);fader.setDoubleClickReturnValue(true,0);fader.setSkewFactor(1);fader.setName("Level "+id);fader.setComponentID("mixer-level-"+id);fader.textFromValueFunction=[this](double v){return v<=(isInstrument()?-60:-70)?juce::String::fromUTF8("\xe2\x88\x92\xe2\x88\x9e"):dbText(v);};fader.valueFromTextFunction=[](const juce::String& v){return (v.containsIgnoreCase("inf") || v.contains(juce::String::charToString(0x221e)))?-70:v.getDoubleValue();};fader.onValueChange=[this]{edit([&](drum::MixerControls& c){c.volumeDb=fader.getValue();});levelReading.setText(fader.getTextFromValue(fader.getValue()),juce::dontSendNotification);};
        levelReading.setComponentID("mixer-level-readout-"+id);levelReading.setTooltip("Fader level in dB. Click to type; double-click resets to 0 dB.");levelReading.reset=[this]{fader.setValue(0,juce::sendNotificationSync);};levelReading.onTextChange=[this]{fader.setValue(fader.getValueFromText(levelReading.getText()),juce::sendNotificationSync);};
        peakReading.setComponentID("mixer-peak-readout-"+id);peakReading.setTooltip("Maximum peak in dB since reset. Click to reset.");peakReading.onClick=[this]{if(isGroup())owner.processor.getSampleEngine().resetGroupMaximum(meterGroup);else owner.processor.getEffects().resetMixerMaximum(isInstrument()?note-drum::firstNote:drum::maxInstruments+returnIndex);meter();};
        pan.getProperties().set("mixer-pan",true);pan.setTooltip("Stereo balance. Drag to adjust; double-click to centre.");pan.setRange(-1,1,.001);pan.setSkewFactor(1);pan.setDoubleClickReturnValue(true,0);pan.setName("Pan "+id);pan.setComponentID("mixer-pan-"+id);pan.textFromValueFunction=[](double v){return std::abs(v)<.005?juce::String("C"):juce::String(juce::roundToInt(std::abs(v)*100))+(v<0?" L":" R");};pan.valueFromTextFunction=[](const juce::String& value){auto v=value.trim().toUpperCase();if(v=="C")return 0.0;double n=v.getDoubleValue();if(v.contains("L"))n=-std::abs(n);if(v.contains("R"))n=std::abs(n);return juce::jlimit(-1.0,1.0,n*.01);};pan.onValueChange=[this]{edit([&](drum::MixerControls& c){c.pan=pan.getValue();});};
        mute.setClickingTogglesState(true);solo.setClickingTogglesState(true);mute.setComponentID("mixer-mute-"+id);solo.setComponentID("mixer-solo-"+id);mute.onClick=[this]{edit([&](drum::MixerControls& c){c.mute=mute.getToggleState();});};solo.onClick=[this]{edit([&](drum::MixerControls& c){c.solo=solo.getToggleState();});};
        output.addItem(isInstrument()?"To group":"Main",1);for(int n=1;n<16;++n)output.addItem("Out "+juce::String(n+1),n+1);if(returnIndex>=0)output.addItem("No dry output",17);output.setComponentID("mixer-output-"+id);output.setTooltip("Receive Out 2-16 on an Ableton audio track to use that track's sends. Disabled host outputs fall back to Main.");output.onChange=[this]{if(returnIndex>=0){auto state=owner.processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);if(!kit)return;owner.processor.getLibrary().setReturnMain(kit->id,returnIndex,output.getSelectedId()!=17);if(output.getSelectedId()==17){changed();return;}}edit([&](drum::MixerControls& c){c.output=output.getSelectedId()-1;});};
        if(isInstrument() || isGroup())for(int n=0;n<6;++n){auto& knob=sends[(size_t)n];addAndMakeVisible(knob);knob.setComponentID("mixer-send-"+id+"-"+juce::String(n));knob.onValueChange=[this,n]{auto state=owner.processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);if(kit && isGroup()){owner.processor.getLibrary().setGroupSend(kit->id,id,n,sends[(size_t)n].getValue());changed();return;}auto* instrument=kit?drum::findInstrument(*kit,id):nullptr;if(!instrument)return;auto c=instrument->controls;double* levels[]{&c.sound.sendA,&c.sound.sendB,&c.sound.sendC,&c.sound.sendD,&c.sound.sendE,&c.sound.sendF};*levels[n]=sends[(size_t)n].getValue();owner.processor.getLibrary().setControls(kit->id,id,c);changed();};}
    }
    bool isInstrument() const{return groupIndex>=0;}
    bool isGroup() const{return !isInstrument() && returnIndex<0;}
    void changed(){owner.processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
    template<class F> void edit(F fn){auto state=owner.processor.getLibrary().snapshot();auto* k=drum::activeKit(state);if(!k)return;
        if(isInstrument()){auto* i=drum::findInstrument(*k,id);if(!i)return;drum::MixerControls c{i->controls.volumeDb,i->controls.pan,i->controls.mute,i->controls.solo,(int)i->controls.sound.output};fn(c);auto values=i->controls;values.volumeDb=c.volumeDb;values.pan=c.pan;values.mute=c.mute;values.solo=c.solo;values.sound.output=c.output;owner.processor.getLibrary().setControls(k->id,id,values);}
        else if(isGroup()){auto* g=drum::findGroup(*k,id);if(!g)return;auto c=g->mixer;fn(c);owner.processor.getLibrary().setGroupMixer(k->id,id,c);}
        else {auto c=k->returnRouting[(size_t)returnIndex].mixer;fn(c);owner.processor.getLibrary().setReturnMixer(k->id,returnIndex,c);}changed();}
    void refresh(const drum::Kit& kit){drum::MixerControls c;juce::String title;int groupColour=0;
        if(isInstrument()){auto* i=drum::findInstrument(kit,id);if(!i)return;c={i->controls.volumeDb,i->controls.pan,i->controls.mute,i->controls.solo,(int)i->controls.sound.output};title=i->name;note=i->note;groupColour=kit.groups[(size_t)groupIndex].colour;
            const double levels[]{i->controls.sound.sendA,i->controls.sound.sendB,i->controls.sound.sendC,i->controls.sound.sendD,i->controls.sound.sendE,i->controls.sound.sendF};for(int n=0;n<6;++n){sends[(size_t)n].setValue(levels[n],juce::dontSendNotification);sends[(size_t)n].setName("Local "+drum::sendLabel(kit,n)+" for "+i->name);sends[(size_t)n].setTooltip("Drum local "+drum::sendLabel(kit,n)+": "+dbText(levels[n])+"\nDrag vertically; Shift for fine. Type/Enter to edit. Double-click/Delete for off. Range: -infinity to +6 dB.");}}
        else if(isGroup()){auto* g=drum::findGroup(kit,id);if(!g)return;c=g->mixer;title=g->name;for(int n=0;n<6;++n)sends[(size_t)n].setValue(g->sends[(size_t)n],juce::dontSendNotification);groupColour=g->colour;meterGroup=(int)(g-kit.groups.data());}
        else {c=kit.returnRouting[(size_t)returnIndex].mixer;title=drum::sendLabel(kit,returnIndex);groupColour=1;}
        tint=isGroup() || isInstrument()?colour(groupColour):accent;name.setButtonText(title);name.setTooltip(title+(isGroup()?(owner.selected==id?" - open group chain and sends":" - open this group"):isInstrument()?" - click to edit instrument":""));fader.setValue(c.volumeDb,juce::dontSendNotification);if(!levelReading.isBeingEdited())levelReading.setText(fader.getTextFromValue(c.volumeDb),juce::dontSendNotification);pan.setValue(c.pan,juce::dontSendNotification);fader.setName("Level "+title);pan.setName("Pan "+title);mute.setToggleState(c.mute,juce::dontSendNotification);solo.setToggleState(c.solo,juce::dontSendNotification);output.setSelectedId(returnIndex>=0 && !kit.returnRouting[(size_t)returnIndex].main?17:c.output+1,juce::dontSendNotification);fader.setColour(juce::Slider::thumbColourId,tint);fader.setColour(juce::Slider::trackColourId,tint);mute.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffd97181));solo.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffd4b660));
        int actual=c.output;if(isInstrument() && actual==0)actual=kit.groups[(size_t)groupIndex].mixer.output;auto* bus=owner.processor.getBus(false,drum::outputBusIndex(actual));inactive=actual>0 && (!bus || !bus->isEnabled());repaint();}
    void meter(){std::array<float,2> maxima{},holds{};
        if(isGroup()){auto& engine=owner.processor.getSampleEngine();peaks=engine.groupPeaks(meterGroup);maxima=engine.groupMaximum(meterGroup);holds=engine.groupHold(meterGroup);}
        else {int index=isInstrument()?note-drum::firstNote:drum::maxInstruments+returnIndex;auto& effects=owner.processor.getEffects();peaks=effects.mixerPeaks(index);maxima=effects.mixerMaximum(index);holds=effects.mixerHold(index);}
        fader.getProperties().set("peak-l",peaks[0]);fader.getProperties().set("peak-r",peaks[1]);fader.getProperties().set("hold-l",holds[0]);fader.getProperties().set("hold-r",holds[1]);fader.repaint();float maximum=juce::jmax(maxima[0],maxima[1]);peakReading.clipped=maximum>=1;peakReading.setButtonText(dbText(juce::Decibels::gainToDecibels(maximum,-100.f)));peakReading.repaint();}
    void resized() override {int h=getHeight();name.setBounds(6,14,116,38);output.setBounds(8,h-39,112,24);mute.setBounds(8,h-66,53,21);solo.setBounds(67,h-66,53,21);pan.setBounds(12,72,104,22);
        if(isInstrument() || isGroup())for(int n=0;n<6;++n)sends[(size_t)n].setBounds(8+(n%3)*38,(isGroup() && drum::EffectRack::processingLanes>=drum::effectLanes?146:121)+(n/3)*34,36,22);
        processing.setBounds(8,98,112,22);int top=isGroup() && drum::EffectRack::processingLanes>=drum::effectLanes?221:returnIndex<0?196:126;peakReading.setBounds(8,top-22,66,18);levelReading.setBounds(74,top-22,46,18);fader.setBounds(8,top,112,juce::jmax(80,h-top-74));}
    void paint(juce::Graphics& g) override {g.setColour(isGroup()?juce::Colour(0xff2a3448):panel);g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2),9);if(isGroup()){g.setColour(tint.withAlpha(.12f));g.fillRoundedRectangle(juce::Rectangle<float>(6,190,(float)getWidth()-12,(float)juce::jmax(0,getHeight()-264)),6);}g.setColour(tint);g.fillRect(10,4,108,3);g.setColour(muted);g.setFont(11);g.drawText(isInstrument()?"INSTRUMENT":isGroup()?"GROUP":"LOCAL RETURN",10,52,108,13,juce::Justification::centred);
        if(isInstrument() || isGroup())for(int n=0;n<6;++n)g.drawText(juce::String::charToString((juce::juce_wchar)('A'+n)),8+(n%3)*38,(isGroup() && drum::EffectRack::processingLanes>=drum::effectLanes?133:108)+(n/3)*34,36,12,juce::Justification::centred);
    }
private:
    DrumMixerPage& owner;juce::String id;int groupIndex=-1,returnIndex=-1,note=drum::firstNote,meterGroup=0;juce::Colour tint=accent;bool inactive=false;std::array<float,2> peaks{};
    juce::TextButton processing,name,mute{"M"},solo{"S"};juce::Slider fader;MixerSendBar pan;MixerPeakReading peakReading;MixerLevelReading levelReading;juce::ComboBox output;std::array<MixerSendBar,6> sends;
};
DrumMixerPage::DrumMixerPage(DrumAudioProcessor& p):processor(p){setComponentID("drum-mixer");look.setColour(juce::ResizableWindow::backgroundColourId,background);look.setColour(juce::Label::textColourId,text);look.setColour(juce::Slider::textBoxTextColourId,text);look.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff293848));look.setColour(juce::TextButton::textColourOffId,text);look.setColour(juce::ComboBox::backgroundColourId,panel);look.setColour(juce::ComboBox::textColourId,text);setLookAndFeel(&look);for(auto* c:std::initializer_list<juce::Component*>{&view,&back,&close,&help,&viewport})addAndMakeVisible(c);viewport.setViewedComponent(&canvas,false);viewport.setScrollBarsShown(false,true);view.setComponentID("mixer-view");view.onChange=[this]{auto state=processor.getLibrary().snapshot();auto* k=drum::activeKit(state);if(!k)return;int n=view.getSelectedId();if(n!=1 && n!=2 && (n<100 || n-100>=(int)k->groups.size()))return;chooseGroup(n==1?juce::String():n==2?juce::String("returns"):k->groups[(size_t)(n-100)].id);};back.setComponentID("mixer-back");close.setComponentID("mixer-exit");back.onClick=[this]{const auto boundary=viewSessions.empty()?0:viewSessions.back();if(viewHistory.size()>boundary){selected=viewHistory.back();viewHistory.pop_back();topology.clear();refresh();return;}if(!viewSessions.empty())viewSessions.pop_back();if(viewSessions.empty())viewHistory.clear();if(onBack)onBack();else setVisible(false);};close.onClick=[this]{viewSessions.clear();viewHistory.clear();if(onExit)onExit();else setVisible(false);};help.onClick=[this]{juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Use Ableton's sends","1. Choose Out 2-16 on an instrument or group here.\n2. In Live, create an audio track. Set Audio From to the track hosting Drum, then select that Drum output in the lower chooser. Set Monitor to In.\n3. Use the new track's send knobs for Live's return effects. Its normal output carries the dry sound.\n\nAn instrument set to 'To group' follows its group's output. A separate instrument output overrides that destination. Do not also monitor the same output twice.\n\nDrum's A-F knobs feed only its own local effects. The local return faders control their processed output, including feeds to other local returns. Final overall volume stays on the Live track.");};startTimerHz(25);}
DrumMixerPage::~DrumMixerPage(){stopTimer();viewport.setViewedComponent(nullptr,false);strips.clear();setLookAndFeel(nullptr);}
void DrumMixerPage::open(){viewSessions.push_back(viewHistory.size());setVisible(true);toFront(false);revision=UINT64_MAX;refresh();}
void DrumMixerPage::chooseGroup(const juce::String& id){if(selected!=id)viewHistory.push_back(selected);selected=id;topology.clear();refresh();}
void DrumMixerPage::refresh(){auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);if(!kit)return;juce::String key=kit->id+selected;for(const auto& g:kit->groups){key+=g.id;for(const auto& i:g.instruments)key+=i.id;}if(key!=topology){topology=key;strips.clear();view.clear(juce::dontSendNotification);view.addItem("All groups + local returns",1);view.addItem("Local returns",2);bool exists=selected.isEmpty() || selected=="returns";for(int n=0;n<(int)kit->groups.size();++n){const auto& g=kit->groups[(size_t)n];view.addItem(g.name,100+n);if(selected==g.id)exists=true;}if(!exists)selected.clear();view.setSelectedId(selected.isEmpty()?1:selected=="returns"?2:100,juce::dontSendNotification);
        auto add=[&](juce::String id,int group,int ret){auto strip=std::make_unique<Strip>(*this,id,group,ret);canvas.addAndMakeVisible(*strip);strips.push_back(std::move(strip));};
        for(int n=0;n<(int)kit->groups.size();++n){const auto& g=kit->groups[(size_t)n];if(selected.isEmpty())add(g.id,-1,-1);else if(selected==g.id){view.setSelectedId(100+n,juce::dontSendNotification);for(const auto& i:g.instruments)add(i.id,n,-1);add(g.id,-1,-1);}}
        if(selected.isEmpty() || selected=="returns")for(int n=0;n<6;++n)add("return-"+juce::String(n),-1,n);resized();}
    for(int n=0;n<(int)kit->groups.size();++n)view.changeItemText(100+n,kit->groups[(size_t)n].name);
    for(auto& strip:strips)strip->refresh(*kit);revision=state.revision;repaint();}
void DrumMixerPage::timerCallback(){if(!isShowing())return;if(revision!=processor.getLibrary().currentRevision())refresh();for(auto& strip:strips)strip->meter();}
void DrumMixerPage::resized(){view.setBounds(20,58,310,32);help.setBounds(getWidth()-380,18,145,32);back.setBounds(getWidth()-225,18,95,32);close.setBounds(getWidth()-120,18,100,32);viewport.setBounds(14,106,getWidth()-28,getHeight()-118);int width=juce::jmax(viewport.getWidth()-4,(int)strips.size()*136+8);canvas.setSize(width,juce::jmax(350,viewport.getHeight()-18));for(int n=0;n<(int)strips.size();++n)strips[(size_t)n]->setBounds(4+n*136,0,128,canvas.getHeight());}
void DrumMixerPage::paint(juce::Graphics& g){g.fillAll(background);g.setColour(text);g.setFont(24);g.drawText("MIXER",20,15,200,34,juce::Justification::left);}

void DrumMixerPage::openGroupProcessing(const juce::String& id){
#if JUCE_MAC
auto state=processor.getLibrary().snapshot();const auto* kit=drum::activeKit(state);const auto* group=kit?drum::findGroup(*kit,id):nullptr;if(!group)return;
    class Window final:public juce::DocumentWindow{public:Window(juce::String title):DocumentWindow(title,juce::Colour(0xff10151f),closeButton){setUsingNativeTitleBar(true);}void closeButtonPressed()override{setVisible(false);}};
    auto editor=std::make_unique<GroupEditor>(processor,kit->id,id);editor->setSize(1100,820);groupWindow=std::make_unique<Window>("Drum group: "+group->name);editor->onBack=[this]{if(groupWindow)groupWindow->setVisible(false);toFront(true);};editor->onExit=editor->onBack;groupWindow->setContentOwned(editor.release(),true);groupWindow->setResizable(true,false);groupWindow->centreWithSize(1120,850);djl::HostFloat::showAuxiliaryWindow(*groupWindow,this);
#else
    juce::ignoreUnused(id);
#endif
}
