#include "SoundEditor.h"

namespace
{
const juce::Colour bg(0xff10151f),panel(0xff19212e),ink(0xffe5ebf5),muted(0xff8d9bb0),mint(0xff67dfc5);
void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> area,float size,juce::Colour colour=ink)
{
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawFittedText(value,area,juce::Justification::centredLeft,1);
}
}
struct SoundEditor::Field final : juce::Component
{
    const drum::SoundParameter& parameter;
    juce::Slider slider;
    juce::ComboBox combo;
    explicit Field(const drum::SoundParameter& p,SoundEditor& editor) : parameter(p)
    {
        const auto id=juce::String("edit-")+p.key;
        if(juce::String(p.choices).isNotEmpty())
        {
            addAndMakeVisible(combo); combo.addItemList(juce::StringArray::fromTokens(p.choices,"|",""),1);
            combo.setComponentID(id); combo.setName(p.label);
            combo.onChange=[this,&editor] { editor.change(parameter,combo.getSelectedId()-1); };
        }
        else
        {
            addAndMakeVisible(slider); slider.setComponentID(id); slider.setName(p.label);
            slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,92,26);
            slider.setRange(p.minimum*p.scale,p.maximum*p.scale,p.step*p.scale);
            slider.setTextValueSuffix(p.unit); slider.setDoubleClickReturnValue(true,p.initial*p.scale);
            if(p.maximum>=1000 && p.scale==1) slider.setSkewFactorFromMidPoint(p.minimum==20?1000:juce::jmin(500.0,p.maximum*0.1));
            slider.setColour(juce::Slider::trackColourId,mint); slider.setColour(juce::Slider::thumbColourId,mint);
            slider.setColour(juce::Slider::textBoxTextColourId,ink); slider.setColour(juce::Slider::textBoxBackgroundColourId,bg);
            slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff2b3648));
            slider.onValueChange=[this,&editor] { editor.change(parameter,slider.getValue()/parameter.scale); };
        }
    }
    void paint(juce::Graphics& g) override { text(g,parameter.label,{0,0,getWidth(),19},12,muted); }
    void resized() override { slider.setBounds(0,23,getWidth(),28); combo.setBounds(0,23,getWidth(),28); }
    void update(double v,bool newSelection)
    {
        if(newSelection) slider.hideTextBox(true);
        if(newSelection || (!slider.isMouseButtonDown() && !slider.hasKeyboardFocus(true))) slider.setValue(v*parameter.scale,juce::dontSendNotification);
        combo.setSelectedId((int)v+1,juce::dontSendNotification);
    }
};
struct SoundEditor::MacroField final : juce::Component
{
    SoundEditor& editor;
    int index;
    juce::Slider value,minimum,maximum;
    juce::ComboBox target;
    explicit MacroField(SoundEditor& e,int n):editor(e),index(n)
    {
        addAndMakeVisible(target); target.setComponentID("macro-target-"+juce::String(n)); target.setName("Macro "+juce::String(n+1)+" mapping");
        for(int t=0;t<(int)std::size(drum::macroTargets);++t) target.addItem(drum::macroTargets[(size_t)t].name,t+1);
        for(auto* slider:{&value,&minimum,&maximum})
        {
            addAndMakeVisible(*slider); slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,74,23);
            slider->setColour(juce::Slider::trackColourId,mint);slider->setColour(juce::Slider::thumbColourId,mint);slider->setColour(juce::Slider::textBoxTextColourId,ink);slider->setColour(juce::Slider::textBoxBackgroundColourId,bg);
        }
        value.setRange(0,100,0.1);value.setTextValueSuffix(" %");value.setName("Macro "+juce::String(n+1));value.setComponentID("macro-value-"+juce::String(n));
        minimum.setName("Mapping minimum");maximum.setName("Mapping maximum");minimum.setComponentID("macro-min-"+juce::String(n));maximum.setComponentID("macro-max-"+juce::String(n));
        value.onDragStart=[this]{editor.processor.macro(index).beginChangeGesture();};
        value.onDragEnd=[this]{editor.processor.macro(index).endChangeGesture();};
        value.onValueChange=[this]{if(!editor.updating)editor.processor.macro(index).setValueNotifyingHost((float)value.getValue()/100);};
        target.onChange=[this]{save(true);};minimum.onValueChange=[this]{save(false);};maximum.onValueChange=[this]{save(false);};
    }
    void save(bool newTarget)
    {
        if(editor.updating) return;
        auto state=editor.processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,editor.instrumentId);
        if(!i || kit->id!=editor.kitId) return;
        auto c=i->controls;auto& m=c.macros[(size_t)index];
        if(newTarget)
        {
            m.target=target.getSelectedId()-1;const auto& t=drum::macroTargets[(size_t)m.target];m.minimum=t.minimum;m.maximum=t.maximum;
        }
        else {m.minimum=minimum.getValue();m.maximum=maximum.getValue();}
        auto result=editor.processor.getLibrary().setControls(editor.kitId,editor.instrumentId,c);
        if(result.ok) editor.processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        editor.message=result.ok?"Mapping saved for this instrument. One macro can control sounds across the active kit.":result.message;editor.refresh();
    }
    void update(const drum::MacroMapping& m,bool changed)
    {
        target.setSelectedId(m.target+1,juce::dontSendNotification); const auto& t=drum::macroTargets[(size_t)m.target];
        for(auto* slider:{&minimum,&maximum})
        {
            if(changed) slider->hideTextBox(true);
            slider->setRange(t.minimum,t.maximum,0.001);slider->setEnabled(m.target!=0);
        }
        if(changed || (!minimum.isMouseButtonDown() && !minimum.hasKeyboardFocus(true))) minimum.setValue(m.minimum,juce::dontSendNotification);
        if(changed || (!maximum.isMouseButtonDown() && !maximum.hasKeyboardFocus(true))) maximum.setValue(m.maximum,juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override
    {
        text(g,"MACRO "+juce::String(index+1),{0,0,getWidth(),22},12,mint);
        text(g,"Min",{0,82,30,24},11,muted);text(g,"Max",{getWidth()/2+4,82,30,24},11,muted);
    }
    void resized() override
    {
        value.setBounds(90,0,getWidth()-90,24);target.setBounds(0,34,getWidth(),30);
        minimum.setBounds(32,82,getWidth()/2-40,25);maximum.setBounds(getWidth()/2+36,82,getWidth()/2-36,25);
    }
};
class SoundEditor::Waveform final : public juce::Component,public juce::SettableTooltipClient
{
public:
    explicit Waveform(SoundEditor& e):owner(e)
    { setComponentID("sample-waveform"); setName("Sample waveform: drag start and end markers"); setTooltip("Drag the green Start and End markers to trim. Ctrl + mouse wheel zooms; Shift + wheel scrolls."); }
    std::array<float,512> peaks {};
    double zoom=1,offset=0;
    int marker=0;
    void paint(juce::Graphics& g) override
    {
        g.fillAll(bg); auto area=getLocalBounds().reduced(10,24); const float mid=(float)area.getCentreY();
        auto x=[&](double position){return (float)(area.getX()+(position-offset)*zoom*area.getWidth());};
        g.saveState(); g.reduceClipRegion(area);
        g.setColour(mint.withAlpha(0.08f)); g.fillRect(juce::Rectangle<float>(x(owner.sound.start),(float)area.getY(),x(owner.sound.end)-x(owner.sound.start),(float)area.getHeight()));
        float peak=0.01f; for(auto p:peaks) peak=juce::jmax(peak,p);
        for(int n=0;n<512;++n)
        {
            const auto fraction=n/512.0; const float xx=x(fraction),height=peaks[(size_t)n]/peak*area.getHeight()*0.45f;
            g.setColour(fraction>=owner.sound.start && fraction<owner.sound.end?mint:muted.darker());
            g.fillRect(xx,mid-height,juce::jmax(1.0f,area.getWidth()*(float)zoom/512),juce::jmax(1.0f,2*height));
        }
        if(owner.sound.loop!=0 && owner.sound.mode==2)
        {
            double length=owner.sound.end-owner.sound.start;
            g.setColour(juce::Colour(0xffe9ad70));
            for(auto position:{owner.sound.start+length*owner.sound.loopStart,owner.sound.start+length*owner.sound.loopEnd}) g.drawVerticalLine((int)x(position),(float)area.getY(),(float)area.getBottom());
        }
        g.setColour(mint);
        g.drawVerticalLine((int)x(owner.sound.start),(float)area.getY(),(float)area.getBottom());
        g.drawVerticalLine((int)x(owner.sound.end)-1,(float)area.getY(),(float)area.getBottom());
        g.restoreState();
        text(g,"START  "+juce::String(owner.sound.start*100,2)+"%",{10,2,180,20},11,mint);
        text(g,"END  "+juce::String(owner.sound.end*100,2)+"%",{getWidth()-190,2,180,20},11,mint);
        text(g,owner.path.isEmpty()?"Drop an audio sample here":juce::File(owner.path).getFileName(),{10,getHeight()-22,getWidth()-130,20},11,muted);
        text(g,juce::String(zoom,1)+"x zoom",{getWidth()-110,getHeight()-22,100,20},11,muted);
    }
    double position(float x) const {return juce::jlimit(0.0,1.0,offset+(x-10)/juce::jmax(1.0,(getWidth()-20)*zoom));}
    void mouseDown(const juce::MouseEvent& e) override
    {
        double p=position(e.position.x); marker=std::abs(p-owner.sound.start)<std::abs(p-owner.sound.end)?0:1; mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        double p=position(e.position.x);
        p=marker==0?juce::jmin(p,owner.sound.end-0.00001):juce::jmax(p,owner.sound.start+0.00001);
        owner.change(drum::soundParameters[(size_t)marker],p);
    }
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override
    {
        if(e.mods.isCtrlDown())
        {
            const double anchor=position(e.position.x); zoom=juce::jlimit(1.0,32.0,zoom*std::pow(1.4,wheel.deltaY*5));
            offset=juce::jlimit(0.0,1-1/zoom,anchor-(e.position.x-10)/((getWidth()-20)*zoom));
        }
        else if(e.mods.isShiftDown()) offset=juce::jlimit(0.0,1-1/zoom,offset-wheel.deltaY/zoom);
        repaint();
    }
private:
    SoundEditor& owner;
};
SoundEditor::SoundEditor(DrumAudioProcessor& p):processor(p),waveform(std::make_unique<Waveform>(*this))
{
    setComponentID("sound-editor"); setOpaque(true);
    auto button=[this](juce::TextButton& b,const char* title,const char* id) {addAndMakeVisible(b); b.setButtonText(title); b.setComponentID(id);};
    button(back,"Back to kit","close-sound-editor"); back.onClick=[this]{if(close)close();};
    button(play,"Play","edit-play"); play.onClick=[this]{processor.getSampleEngine().audition(note);};
    button(stop,"Stop","edit-stop"); stop.onClick=[this]{processor.getSampleEngine().stop();};
    button(reset,"Reset page","reset-edit-page"); reset.onClick=[this]{
        auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);
        if(!i || kit->id!=kitId) return;
        auto c=i->controls; for(auto& parameter:drum::soundParameters) if(parameter.page==page) c.sound.*(parameter.member)=parameter.initial;
        auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
        if(result.ok) processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        message=result.ok?"Page reset. The source file is unchanged.":result.message; refresh();
    };
    const char* names[]{"Sample","Envelopes & pitch","Filter","LFO","Outputs & sends","Macros"};
    for(int n=0;n<6;++n) {addAndMakeVisible(tabs[(size_t)n]); tabs[(size_t)n].setButtonText(names[n]); tabs[(size_t)n].setComponentID("edit-tab-"+juce::String(n)); tabs[(size_t)n].onClick=[this,n]{setPage(n);};}
    addAndMakeVisible(viewport); viewport.setViewedComponent(&content,false); viewport.setScrollBarsShown(true,false); viewport.setScrollBarThickness(8);
    content.addAndMakeVisible(*waveform);
    for(auto& parameter:drum::soundParameters) {auto field=std::make_unique<Field>(parameter,*this); content.addAndMakeVisible(*field); fields.push_back(std::move(field));}
    for(int n=0;n<drum::numMacros;++n) {auto field=std::make_unique<MacroField>(*this,n);content.addAndMakeVisible(*field);macroFields.push_back(std::move(field));}
    content.addAndMakeVisible(sliceCount);sliceCount.setComponentID("slice-count");sliceCount.setName("Number of equal slices");
    for(int count:{2,4,8,16,32,64}) sliceCount.addItem(juce::String(count)+" equal slices",count);sliceCount.setSelectedId(8);
    content.addAndMakeVisible(slice);slice.setButtonText("Slice to new group");slice.setComponentID("slice-to-group");
    slice.onClick=[this]{
        auto result=processor.getLibrary().sliceInstrument(kitId,instrumentId,sliceCount.getSelectedId());
        if(result.ok) processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        message=result.ok?"Created a new group of slices on free keys. The original instrument is preserved.":result.message;refresh();
    };
    setPage(0);startTimerHz(15);
}
SoundEditor::~SoundEditor() {stopTimer();viewport.setViewedComponent(nullptr,false);}
void SoundEditor::select(const juce::String& kit,const drum::Instrument* i)
{
    const bool changed=kitId!=kit || instrumentId!=(i?i->id:juce::String());
    updating=true; kitId=kit; instrumentId=i?i->id:juce::String(); name=i?i->name:"No instrument selected"; path=i?i->samplePath:juce::String(); note=i?i->note:21; sound=i?i->controls.sound:drum::SoundParameters{};
    if(changed) {waveform->zoom=1;waveform->offset=0;message.clear();}
    for(auto& field:fields) {field->update(sound.*(field->parameter.member),changed); field->setEnabled(i!=nullptr);}
    for(int n=0;n<drum::numMacros;++n)
    {
        auto& field=*macroFields[(size_t)n];field.update(i?i->controls.macros[(size_t)n]:drum::MacroMapping{},changed);field.setEnabled(i!=nullptr);
        if(!field.value.isMouseButtonDown() && !field.value.hasKeyboardFocus(true)) field.value.setValue(processor.macro(n).get()*100,juce::dontSendNotification);
    }
    waveform->peaks=processor.getSampleEngine().waveformFor(instrumentId); waveform->repaint();
    play.setEnabled(i && processor.getSampleEngine().statusFor(i->id).ready); reset.setEnabled(i!=nullptr);
    updating=false; repaint();
}
void SoundEditor::refresh()
{
    auto state=processor.getLibrary().snapshot(); auto* kit=drum::activeKit(state);
    select(kit->id,drum::findInstrument(*kit,instrumentId));
}
void SoundEditor::change(const drum::SoundParameter& parameter,double value)
{
    if(updating) return;
    auto state=processor.getLibrary().snapshot(); auto* kit=drum::activeKit(state); auto* i=drum::findInstrument(*kit,instrumentId);
    if(!i || kit->id!=kitId) return;
    auto c=i->controls; c.sound.*(parameter.member)=value;
    auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
    if(result.ok) processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
    message=result.ok?"Saved with this kit. Markers, direction, playback mode and loop on/off apply on the next hit.":"Start must be before End; loop start must be before loop end.";
    refresh();
}
void SoundEditor::setPage(int value)
{
    page=value; viewport.setViewPosition(0,0);
    for(int n=0;n<6;++n) tabs[(size_t)n].setToggleState(n==page,juce::dontSendNotification);
    for(auto& field:fields) field->setVisible(field->parameter.page==page);
    for(auto& field:macroFields) field->setVisible(page==5);
    slice.setVisible(page==0);sliceCount.setVisible(page==0);reset.setVisible(page!=5);
    waveform->setVisible(page==0); resized(); repaint();
}
void SoundEditor::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g,name,{18,12,getWidth()-410,30},22);
    text(g,"MIDI "+juce::String(note)+"  /  "+drum::noteName(note)+"  /  SOUND EDITOR",{18,46,getWidth()-36,22},11,mint);
    static const char* help[]{
        "Drag waveform markers to trim. Loop positions are relative to the trimmed region. Loop works in Classic mode.",
        "Amplitude ADSR shapes Classic mode. Trigger and Gate use Fade in/out. Pitch envelope works in all modes.",
        "Clean resonant filters with optional drive. Positive envelope amounts raise cutoff; negative amounts lower it.",
        "Per-voice modulation. Host tempo follows your DAW; standalone uses 120 BPM. Depths of zero leave the sound unchanged.",
        "Enable the desired plug-in outputs in your DAW. Sends are post-fader stereo outputs; add effects on receiving tracks.",
        "Automate Macro 1-16 in your DAW. Mappings belong to each instrument; Min/Max allow inverted ranges. Later mappings take priority."};
    text(g,help[page],{18,116,getWidth()-36,24},12,muted);
    text(g,message.isEmpty()?"Non-destructive edits. The original audio file is preserved.":message,{18,getHeight()-29,getWidth()-36,23},11,muted);
}
void SoundEditor::resized()
{
    back.setBounds(getWidth()-380,15,125,32); play.setBounds(getWidth()-245,15,65,32); stop.setBounds(getWidth()-170,15,65,32); reset.setBounds(getWidth()-95,15,80,32);
    const int tabWidth=(getWidth()-36-40)/6;
    for(int n=0;n<6;++n) tabs[(size_t)n].setBounds(18+n*(tabWidth+8),78,tabWidth,30);
    viewport.setBounds(18,150,juce::jmax(1,getWidth()-36),juce::jmax(1,getHeight()-187));
    const int width=juce::jmax(1,viewport.getWidth()-12),columns=3,gap=24,cell=(width-gap*2)/columns;
    const int top=page==0?160:6; waveform->setBounds(0,0,width,144);
    int n=0; for(auto& field:fields) if(field->parameter.page==page)
    {field->setBounds((n%columns)*(cell+gap),top+(n/columns)*65,cell,56); ++n;}
    int bottom=top+((n+2)/3)*65+6;
    if(page==0) {sliceCount.setBounds(0,bottom,160,28);slice.setBounds(172,bottom,180,28);bottom+=40;}
    if(page==5)
    {
        const int cellWidth=(width-30)/2;
        for(int index=0;index<drum::numMacros;++index) macroFields[(size_t)index]->setBounds((index%2)*(cellWidth+30),(index/2)*135,cellWidth,118);
        bottom=(drum::numMacros/2)*135;
    }
    content.setSize(width,juce::jmax(viewport.getHeight(),bottom));
}
void SoundEditor::timerCallback()
{
    if(!isShowing()) return;
    for(int n=0;n<drum::numMacros;++n)
    {
        auto& slider=macroFields[(size_t)n]->value;
        if(!slider.isMouseButtonDown() && !slider.hasKeyboardFocus(true)) slider.setValue(processor.macro(n).get()*100,juce::dontSendNotification);
    }
}
void SoundEditor::filesDropped(const juce::StringArray& files,int,int)
{
    if(files.size()!=1 || instrumentId.isEmpty() || !juce::File::isAbsolutePath(files[0])) return;
    processor.getSampleEngine().requestLoad(kitId,instrumentId,juce::File(files[0]));
    message="Loading replacement audio..."; repaint();
}
