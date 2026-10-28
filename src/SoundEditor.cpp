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
    {
        setComponentID("sample-waveform"); setName("Sample waveform: drag start and end markers");
        setTooltip("Drag the green Start and End markers to trim. Ctrl + wheel zooms at the pointer; Shift + wheel scrolls. Fit shows the whole sample.");
        auto button=[this](juce::TextButton& b,const char* label,const char* id,const char* tip)
        { addAndMakeVisible(b); b.setButtonText(label); b.setComponentID(id); b.setTooltip(tip); };
        button(zoomOut,"-","waveform-zoom-out","Zoom out around the centre");
        button(zoomIn,"+","waveform-zoom-in","Zoom in around the centre, down to individual samples");
        button(fit,"Fit","waveform-fit","Show the entire sample");
        zoomOut.onClick=[this]{setZoom(zoom/2,0.5);};
        zoomIn.onClick=[this]{setZoom(zoom*2,0.5);};
        fit.onClick=[this]{zoom=1;offset=0;repaint();};
    }
    void setSample(std::shared_ptr<const drum::Sample> value,bool reset)
    {
        if(source!=value || reset) {zoom=1;offset=0;cachedWidth=0;}
        source=std::move(value);
        for(auto* button:{&zoomOut,&zoomIn,&fit}) button->setEnabled(source!=nullptr);
        repaint();
    }
    double zoom=1,offset=0;
    int marker=0;
    juce::Rectangle<int> plot() const {return {10,26,juce::jmax(1,getWidth()-20),juce::jmax(1,getHeight()-58)};}
    void resized() override
    {
        zoomOut.setBounds(getWidth()-128,getHeight()-26,30,22);
        zoomIn.setBounds(getWidth()-92,getHeight()-26,30,22);
        fit.setBounds(getWidth()-56,getHeight()-26,46,22);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(bg); auto area=plot();
        auto x=[&](double position){return (float)(area.getX()+(position-offset)*zoom*area.getWidth());};
        g.saveState(); g.reduceClipRegion(area);
        g.setColour(mint.withAlpha(0.08f)); g.fillRect(juce::Rectangle<float>(x(owner.sound.start),(float)area.getY(),x(owner.sound.end)-x(owner.sound.start),(float)area.getHeight()));
        if(source)
        {
            const auto scale=juce::jmax(1.0f,g.getInternalContext().getPhysicalPixelScaleFactor());
            const int pixels=juce::jlimit(1,16384,(int)std::ceil(area.getWidth()*scale));
            rebuildPaths(area,pixels);
            const int channels=source->audio.getNumChannels();
            for(int channel=0;channel<channels;++channel)
            {
                const float middle=area.getY()+area.getHeight()*(channel+0.5f)/channels;
                g.setColour(muted.withAlpha(0.22f));g.drawHorizontalLine((int)middle,(float)area.getX(),(float)area.getRight());
            }
            auto draw=[&](juce::Colour colour)
            {
                for(auto& path:paths)
                {
                    if(!sampleTrace) {g.setColour(colour.withAlpha(0.75f));g.fillPath(path);}
                    g.setColour(colour);g.strokePath(path,juce::PathStrokeType(sampleTrace?1.25f:0.7f/scale));
                }
            };
            draw(muted.darker(0.35f));
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<float>(x(owner.sound.start),(float)area.getY(),x(owner.sound.end)-x(owner.sound.start),(float)area.getHeight()).getSmallestIntegerContainer());
            draw(mint);g.restoreState();
            if(channels==2)
            {
                text(g,"L",{area.getX()+4,area.getY()+2,20,16},10,muted);
                text(g,"R",{area.getX()+4,area.getCentreY()+2,20,16},10,muted);
            }
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
        text(g,owner.path.isEmpty()?"Drop an audio sample here":juce::File(owner.path).getFileName(),{10,getHeight()-25,getWidth()-410,20},11,muted);
        auto detail=juce::String(zoom,1)+"x";
        if(source) detail+="  /  "+juce::String(source->audio.getNumSamples()/source->rate/zoom*1000,2)+" ms";
        text(g,detail,{getWidth()-395,getHeight()-25,255,20},11,muted);
    }
    double position(float x) const {return juce::jlimit(0.0,1.0,offset+(x-plot().getX())/(plot().getWidth()*zoom));}
    void mouseDown(const juce::MouseEvent& e) override
    {
        marker=-1;
        if(!source || !plot().contains(e.position.toInt())) return;
        double p=position(e.position.x); marker=std::abs(p-owner.sound.start)<std::abs(p-owner.sound.end)?0:1; mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(marker<0) return;
        double p=position(e.position.x);
        p=marker==0?juce::jmin(p,owner.sound.end-0.00001):juce::jmax(p,owner.sound.start+0.00001);
        owner.change(drum::soundParameters[(size_t)marker],p);
    }
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override
    {
        if(e.mods.isCtrlDown())
        {
            setZoom(zoom*std::pow(1.4,wheel.deltaY*5),juce::jlimit(0.0,1.0,(e.position.x-plot().getX())/(double)plot().getWidth()));
        }
        else if(e.mods.isShiftDown()) offset=juce::jlimit(0.0,1-1/zoom,offset-wheel.deltaY/zoom);
        repaint();
    }
private:
    void setZoom(double value,double fraction)
    {
        const auto anchor=offset+fraction/zoom;
        const double maximum=source?juce::jmax(1.0,source->audio.getNumSamples()/32.0):1.0;
        zoom=juce::jlimit(1.0,maximum,value);
        offset=juce::jlimit(0.0,1-1/zoom,anchor-fraction/zoom);
        repaint();
    }
    void rebuildPaths(juce::Rectangle<int> area,int pixels)
    {
        if(cachedWidth==area.getWidth() && cachedHeight==area.getHeight() && cachedPixels==pixels && cachedZoom==zoom && cachedOffset==offset) return;
        cachedWidth=area.getWidth();cachedHeight=area.getHeight();cachedPixels=pixels;cachedZoom=zoom;cachedOffset=offset;
        const int frames=source->audio.getNumSamples(),channels=source->audio.getNumChannels();
        const double begin=offset*frames,length=frames/zoom;
        sampleTrace=length<=pixels;
        paths.clear();paths.resize((size_t)channels);
        for(int channel=0;channel<channels;++channel)
        {
            auto& path=paths[(size_t)channel];
            const float middle=area.getY()+area.getHeight()*(channel+0.5f)/channels;
            const float amplitude=area.getHeight()*0.44f/channels/juce::jmax(0.01f,source->peak);
            if(sampleTrace)
            {
                const int first=juce::jlimit(0,frames-1,(int)std::floor(begin));
                const int last=juce::jlimit(first,frames,(int)std::ceil(begin+length));
                for(int n=first;n<=last;++n)
                {
                    const float xx=(float)(area.getX()+(n-begin)/length*area.getWidth());
                    const float yy=middle-source->audio.getSample(channel,juce::jmin(n,frames-1))*amplitude;
                    if(n==first)path.startNewSubPath(xx,yy);else path.lineTo(xx,yy);
                }
            }
            else
            {
                std::vector<drum::WaveformData::Range> columns((size_t)pixels);
                for(int n=0;n<pixels;++n)
                {
                    const int from=juce::jlimit(0,frames-1,(int)std::floor(begin+length*n/pixels));
                    const int to=juce::jlimit(from+1,frames,(int)std::ceil(begin+length*(n+1)/pixels));
                    columns[(size_t)n]=source->waveform.range(source->audio,channel,from,to);
                    const float xx=(float)(area.getX()+(n+0.5)*area.getWidth()/pixels);
                    const float yy=middle-columns[(size_t)n].maximum*amplitude;
                    if(n==0)path.startNewSubPath((float)area.getX(),yy);
                    path.lineTo(xx,yy);
                }
                path.lineTo((float)area.getRight(),middle-columns.back().maximum*amplitude);
                path.lineTo((float)area.getRight(),middle-columns.back().minimum*amplitude);
                for(int n=pixels-1;n>=0;--n)
                    path.lineTo((float)(area.getX()+(n+0.5)*area.getWidth()/pixels),middle-columns[(size_t)n].minimum*amplitude);
                path.lineTo((float)area.getX(),middle-columns.front().minimum*amplitude);path.closeSubPath();
            }
        }
    }
    SoundEditor& owner;
    std::shared_ptr<const drum::Sample> source;
    juce::TextButton zoomOut,zoomIn,fit;
    std::vector<juce::Path> paths;
    int cachedWidth=0,cachedHeight=0,cachedPixels=0;
    double cachedZoom=0,cachedOffset=0;
    bool sampleTrace=false;
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
    waveform->setSample(processor.getSampleEngine().waveformFor(instrumentId),changed);
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
        "Drag Start/End to trim. Ctrl + wheel zooms; Shift + wheel scrolls. Loops use the trimmed region in Classic mode.",
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
    const int top=page==0?224:6; waveform->setBounds(0,0,width,208);
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
