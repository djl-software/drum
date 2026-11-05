#include "SoundEditor.h"

namespace
{
const juce::Colour waveColour(0xfff1b95b), spectrumColour(0xff6cb5ff), playheadColour(0xffff76bd);
const juce::Colour bg(0xff10151f),panel(0xff19212e),ink(0xffe5ebf5),muted(0xff8d9bb0),mint(0xff67dfc5);
using SoundMember=double drum::SoundParameters::*;
SoundMember oppositeMarker(SoundMember member)
{
    using S=drum::SoundParameters;
    if(member==&S::start)return &S::end;if(member==&S::end)return &S::start;
    if(member==&S::loopStart)return &S::loopEnd;if(member==&S::loopEnd)return &S::loopStart;
    return member;
}
double markerValue(const drum::SoundParameters& sound,SoundMember member)
{
    const auto opposite=oppositeMarker(member);
    return sound.reverse!=0 && opposite!=member?1-sound.*opposite:sound.*member;
}
constexpr std::array<std::array<SoundMember,4>,3> envelopeMembers{{
    {&drum::SoundParameters::ampAttack,&drum::SoundParameters::ampDecay,&drum::SoundParameters::ampSustain,&drum::SoundParameters::ampRelease},
    {&drum::SoundParameters::pitchAttack,&drum::SoundParameters::pitchDecay,&drum::SoundParameters::pitchSustain,&drum::SoundParameters::pitchRelease},
    {&drum::SoundParameters::filterAttack,&drum::SoundParameters::filterDecay,&drum::SoundParameters::filterSustain,&drum::SoundParameters::filterRelease}}};
void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> area,float size,juce::Colour colour=ink)
{
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawFittedText(value,area,juce::Justification::centredLeft,1);
}
}
struct SoundEditor::Field final : juce::Component
{
    const drum::SoundParameter& parameter;
    ResetSlider slider;
    juce::ComboBox combo;
    juce::ToggleButton tick;
    bool binary=false;juce::String sendCaption;
    explicit Field(const drum::SoundParameter& p,SoundEditor& editor) : parameter(p)
    {
        const auto id=juce::String("edit-")+p.key;
        binary=juce::String(p.choices)=="Off|On" || juce::String(p.choices)=="On|Off";
        if(binary){addAndMakeVisible(tick);tick.setComponentID(id);tick.setName(p.label);tick.setButtonText(p.label);tick.setColour(juce::ToggleButton::textColourId,ink);tick.onClick=[this,&editor]{editor.change(parameter,juce::String(parameter.choices)=="On|Off"?!tick.getToggleState():tick.getToggleState());};}
        else if(juce::String(p.choices).isNotEmpty())
        {
            addAndMakeVisible(combo); combo.addItemList(juce::StringArray::fromTokens(p.choices,"|",""),1);
            combo.setComponentID(id); combo.setName(p.label);
            combo.onChange=[this,&editor] { editor.change(parameter,combo.getSelectedId()-1); };
        }
        else
        {
            addAndMakeVisible(slider); slider.setComponentID(id); slider.setName(p.label);
            useValueBar(slider);
            slider.setRange(p.minimum*p.scale,p.maximum*p.scale,p.step*p.scale);
            slider.setTextValueSuffix(p.unit);
            if(juce::String(p.key).startsWith("send")){slider.textFromValueFunction=[](double value){return value<=drum::sendFloor?juce::String::fromUTF8("\xe2\x88\x92\xe2\x88\x9e"):juce::String(value,1);};slider.valueFromTextFunction=[](const juce::String& value){return value.contains(juce::String::fromUTF8("\xe2\x88\x9e")) || value.containsIgnoreCase("inf")?drum::sendFloor:juce::jlimit(drum::sendFloor,0.0,value.getDoubleValue());};slider.setTooltip("Post-fader send to Drum's shared internal return. Minimum is silence; maximum is 0 dB. Double-click resets to silence.");} slider.setDoubleClickReturnValue(true,p.initial*p.scale);
            if(p.maximum>=1000 && p.scale==1) slider.setSkewFactorFromMidPoint(p.minimum==20?1000:juce::jmin(500.0,p.maximum*0.1));
            slider.setColour(juce::Slider::trackColourId,mint.withAlpha(.3f)); slider.setColour(juce::Slider::thumbColourId,mint);
            slider.setColour(juce::Slider::textBoxTextColourId,ink); slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff2b3648));
            if(p.member==&drum::SoundParameters::attackLevel)
                slider.setTooltip("0 dB = full-scale peak for the generated burst before EQ, velocity, LFO and mixer; -6 dB is about half amplitude. -60 dB is silent.");
            useValueBar(slider);
            slider.onValueChange=[this,&editor] { editor.change(parameter,slider.getValue()/parameter.scale); };
            for(int n=0;n<3;++n)if(std::find(envelopeMembers[n].begin(),envelopeMembers[n].end(),p.member)!=envelopeMembers[n].end())
            {slider.onDragStart=[&editor,n]{editor.beginEnvelopeGesture(n);};slider.onDragEnd=[&editor,n]{editor.endEnvelopeGesture(n);};}
        }
    }
    juce::String label() const
    {
        if(sendCaption.isNotEmpty())return sendCaption;
        const juce::String key(parameter.key);
        if(key=="fadeOut")return "Fade out";if(key=="snap")return "Zero snap";
        if(key=="keyOverride")return "Key label";if(key=="mono")return "Channels";
        return parameter.label;
    }
    void paint(juce::Graphics& g) override {if(!binary){g.setColour(ink.withMultipliedAlpha(isEnabled()?1.f:.35f));g.setFont(juce::FontOptions(12));g.drawFittedText(label(),{0,0,getWidth(),22},juce::Justification::centred,1);}}
    int labelWidth() const {return juce::jlimit(80,145,(int)(getWidth()*.43));}
    void resized() override {slider.setBounds((getWidth()-104)/2,22,104,88);combo.setBounds(4,34,getWidth()-8,44);tick.setBounds(4,32,getWidth()-8,44);}

    void update(double v,bool newSelection)
    {
        if(newSelection) slider.hideTextBox(true);
        if(newSelection || (!slider.isMouseButtonDown() && !slider.hasKeyboardFocus(true))) slider.setValue(v*parameter.scale,juce::dontSendNotification);
        combo.setSelectedId((int)v+1,juce::dontSendNotification);tick.setToggleState(juce::String(parameter.choices)=="On|Off"?v==0:v!=0,juce::dontSendNotification);
    }
};
struct SoundEditor::MacroField final : juce::Component
{
    SoundEditor& editor;
    int index;
    ResetSlider value,minimum,maximum;
    juce::ComboBox target;
    juce::TextEditor name;
    explicit MacroField(SoundEditor& e,int n):editor(e),index(n)
    {
        addAndMakeVisible(target); target.setComponentID("macro-target-"+juce::String(n)); target.setName("Macro "+juce::String(n+1)+" mapping");
        for(int t=0;t<(int)std::size(drum::macroTargets);++t) target.addItem(drum::macroTargets[(size_t)t].name,t+1);
        addAndMakeVisible(name);name.setComponentID("macro-name-"+juce::String(n));name.setInputRestrictions(64);name.setTextToShowWhenEmpty("Macro "+juce::String(n+1),muted);name.setTooltip("Rename this kit's shared macro. Live receives the same name; its automation ID stays unchanged.");
        auto rename=[this]{if(editor.updating)return;auto result=editor.processor.renameMacro(index,name.getText());if(!result.ok)editor.message=result.message;};name.onReturnKey=[this,rename]{rename();name.giveAwayKeyboardFocus();};name.onFocusLost=rename;
        for(auto* slider:{&value,&minimum,&maximum})
        {
            addAndMakeVisible(*slider); useValueBar(*slider);
            slider->setColour(juce::Slider::trackColourId,mint.withAlpha(.3f));slider->setColour(juce::Slider::thumbColourId,mint);slider->setColour(juce::Slider::textBoxTextColourId,ink);slider->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        }
        value.setDoubleClickReturnValue(true,50);value.setRange(0,100,0.1);value.setTextValueSuffix(" %");value.setName("Macro "+juce::String(n+1));value.setComponentID("macro-value-"+juce::String(n));
        minimum.setName("Mapping minimum");maximum.setName("Mapping maximum");minimum.setComponentID("macro-min-"+juce::String(n));maximum.setComponentID("macro-max-"+juce::String(n));
        value.onDragStart=[this]{editor.processor.macro(index).beginChangeGesture();};
        value.onDragEnd=[this]{editor.processor.macro(index).endChangeGesture();};
        value.onValueChange=[this]{if(!editor.updating){editor.processor.macro(index).setValueNotifyingHost((float)value.getValue()/100);editor.refreshPreview();}};
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
        if(changed || !name.hasKeyboardFocus(true))name.setText(editor.processor.macro(index).getName(64),false);
        minimum.setDoubleClickReturnValue(true,t.minimum);maximum.setDoubleClickReturnValue(true,t.maximum);
        if(changed || (!minimum.isMouseButtonDown() && !minimum.hasKeyboardFocus(true))) minimum.setValue(m.minimum,juce::dontSendNotification);
        if(changed || (!maximum.isMouseButtonDown() && !maximum.hasKeyboardFocus(true))) maximum.setValue(m.maximum,juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff202d3c));g.fillRoundedRectangle(getLocalBounds().toFloat(),7);g.setColour(mint.withAlpha(.35f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),7,1);
        const char* captions[]{"Value","Min","Max"};for(int n=0;n<3;++n){g.setColour(ink);g.setFont(juce::FontOptions(12));g.drawText(captions[n],n*getWidth()/3,72,getWidth()/3,20,juce::Justification::centred);}
    }
    void resized() override
    {
        name.setBounds(8,6,getWidth()-16,26);target.setBounds(8,38,getWidth()-16,30);
        juce::Slider* dials[]{&value,&minimum,&maximum};for(int n=0;n<3;++n)dials[n]->setBounds(n*getWidth()/3+(getWidth()/3-102)/2,94,102,94);
    }

};
class SoundEditor::Waveform final : public juce::Component,public juce::SettableTooltipClient
{
public:
    explicit Waveform(SoundEditor& e):owner(e)
    {
        setComponentID("sample-waveform"); setName("Sample waveform display");
        setTooltip("Wheel pans left/right; Ctrl + wheel zooms in time; Shift + wheel zooms vertically. Fit X shows the whole sample, Fit Y fits its visible height. Source allows marker dragging.");
        auto button=[this](juce::TextButton& b,const char* label,const char* id,const char* tip)
        { addAndMakeVisible(b); b.setButtonText(label); b.setComponentID(id); b.setTooltip(tip); };
        button(zoomOut,"-","waveform-zoom-out","Zoom out around the centre");
        button(zoomIn,"+","waveform-zoom-in","Zoom in around the centre, down to individual samples");
        button(fit,"Fit X","waveform-fit","Show the entire sample in time; vertical zoom stays unchanged");
        zoomOut.onClick=[this]{setZoom(zoom/2,0.5);};
        zoomIn.onClick=[this]{setZoom(zoom*2,0.5);};
        fit.onClick=[this]{zoom=1;offset=0;repaint();};
        addAndMakeVisible(view);view.setComponentID("waveform-view");view.setName("Waveform source or edited signal");
        view.addItem("Source",1);view.addItem("Edited",2);
        for(int n=0;n<6;++n)view.addItem("Send "+juce::String::charToString((juce::juce_wchar)('A'+n)),n+3);
        view.setSelectedId(2,juce::dontSendNotification);
        view.setTooltip("Edited shows one hit after Drum's controls. Source shows unprocessed audio in the playback direction with draggable trim markers. Sends show the signal before the shared return effects.");
        view.onChange=[this]{zoom=1;offset=0;rendered={};setDisplay(sourceMode()?original:nullptr);owner.refreshPreview();};
        addAndMakeVisible(status);status.setComponentID("waveform-status");status.setFont(juce::FontOptions(11));status.setColour(juce::Label::textColourId,muted);
        addAndMakeVisible(verticalZoom);verticalZoom.setComponentID("waveform-vertical-zoom");verticalZoom.setName("Waveform vertical display zoom");
        for(int n=0;n<10;++n)verticalZoom.addItem("Y "+juce::String(std::pow(2.0,n-2),n<2?2:0)+"x",n+1);
        verticalZoom.setSelectedId(3,juce::dontSendNotification);
        verticalZoom.setTooltip("Vertical display magnification, 0.25x to 128x. This changes the picture only, not audio gain. Kept across edits so level changes remain visible.");
        verticalZoom.onChange=[this]{verticalScale=std::pow(2.0,verticalZoom.getSelectedId()-3);cachedWidth=0;repaint();};
        button(fitVertical,"Fit Y","waveform-fit-vertical","Fit the height of the visible signal once. Later control changes keep this scale.");
        fitVertical.onClick=[this]{fitHeight();};
        addAndMakeVisible(channelView);channelView.setComponentID("waveform-channels");channelView.setName("Waveform channel display");
        channelView.addItem("Auto channels",1);channelView.addItem("Stereo L / R",2);channelView.addItem("Left only",3);channelView.addItem("Right only",4);
        channelView.setSelectedId(1,juce::dontSendNotification);
        channelView.setTooltip("Display only. Auto shows mono or identical L/R signals in one lane. Stereo keeps both lanes; Left or Right uses the full height for that channel. Audio is unchanged.");
        channelView.onChange=[this]{cachedWidth=0;repaint();};
    }
    void setSendNames(const drum::Kit& kit){for(int n=0;n<6;++n)view.changeItemText(n+3,drum::sendLabel(kit,n));}
    bool sourceMode() const {return view.getSelectedId()==1;}
    bool reversed() const {return sourceMode() && owner.sound.reverse!=0;}
    double trimStart() const {return markerValue(owner.sound,&drum::SoundParameters::start);}
    double trimEnd() const {return markerValue(owner.sound,&drum::SoundParameters::end);}
    void showSource(){view.setSelectedId(1,juce::sendNotificationSync);}
    void showEdited() {if(view.getSelectedId()!=2)view.setSelectedId(2,juce::sendNotificationSync);}
    int send() const {return view.getSelectedId()-3;}
    void setSample(std::shared_ptr<const drum::Sample> value,bool reset)
    {
        if(original!=value || reset) {zoom=1;offset=0;rendered={};setDisplay(nullptr);}
        original=std::move(value);
        if(sourceMode())setDisplay(original);
        repaint();
    }
    void setPreview(drum::PreviewResult value,bool busy)
    {
        if(value.sample || value.error.isNotEmpty())rendered=std::move(value);
        if(!sourceMode())setDisplay(rendered.sample);
        auto caption=sourceMode()?juce::String(reversed()?"Source reversed / Start ":"Source / Start ")+juce::String(trimStart()*100,2)+"% / End "+juce::String(trimEnd()*100,2)+"% - drag markers to trim":!original?juce::String("Load a sample to preview"):busy?juce::String("Updating preview..."):rendered.error;
        if(!sourceMode() && original && !busy && caption.isEmpty())
        {
            caption="One hit / velocity 80% / 1 s gate";
            if(owner.chainPresent)caption+=" / before Chain";
            if(source)caption+=" / peak "+juce::String(juce::Decibels::gainToDecibels(source->peak,-100.0f),1)+" dBFS";
            if(rendered.truncated)caption+=" / first "+juce::String(source->audio.getNumSamples()/source->rate,1)+" s only";
        }
        status.setText(caption,juce::dontSendNotification);
        status.setTooltip(caption);
        status.setColour(juce::Label::backgroundColourId,!busy && source && !sourceMode() && source->peak>1?juce::Colour(0xff8f2638):juce::Colours::transparentBlack);
        repaint();
    }
    double timelineMs() const {return source?source->audio.getNumSamples()/source->rate*1000:1000;}
    double zoom=1,offset=0;
    int marker=0;
    juce::Rectangle<int> plot() const {return {10,26,juce::jmax(1,getWidth()-20),juce::jmax(1,getHeight()-58)};}
    void resized() override
    {
        zoomOut.setBounds(getWidth()-128,getHeight()-26,30,22);
        zoomIn.setBounds(getWidth()-92,getHeight()-26,30,22);
        fit.setBounds(getWidth()-56,getHeight()-26,46,22);
        view.setBounds(10,2,112,22);status.setBounds(132,1,juce::jmax(1,getWidth()-308),23);
        channelView.setBounds(getWidth()-166,2,156,22);
        verticalZoom.setBounds(getWidth()-600,getHeight()-26,104,22);
        fitVertical.setBounds(getWidth()-490,getHeight()-26,54,22);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(bg); auto area=plot();
        auto x=[&](double position){return (float)(area.getX()+(position-offset)*zoom*area.getWidth());};
        g.saveState(); g.reduceClipRegion(area);
        g.setColour(mint.withAlpha(0.08f));
        if(sourceMode())g.fillRect(juce::Rectangle<float>(x(trimStart()),(float)area.getY(),x(trimEnd())-x(trimStart()),(float)area.getHeight()));
        else g.fillRect(area);
        if(source)
        {
            const auto scale=juce::jmax(1.0f,g.getInternalContext().getPhysicalPixelScaleFactor());
            const int pixels=juce::jlimit(1,16384,(int)std::ceil(area.getWidth()*scale));
            rebuildPaths(area,pixels);
            const int channels=displayChannels();
            const double duration=1000.0*source->audio.getNumSamples()/source->rate,visible=duration/zoom;
            const double rough=visible/juce::jmax(2,area.getWidth()/100),base=std::pow(10.0,std::floor(std::log10(juce::jmax(0.000001,rough))));
            const double step=base*(rough/base<=1?1:rough/base<=2?2:rough/base<=5?5:10),first=std::ceil(offset*duration/step)*step;
            for(double ms=first;ms<offset*duration+visible;ms+=step)
            {
                const int gx=(int)x(ms/duration);g.setColour(muted.withAlpha(0.28f));g.drawVerticalLine(gx,(float)area.getY(),(float)area.getBottom());
                const auto stamp=step>=1000?juce::String(ms/1000,step<10000?1:0)+" s":juce::String(ms,step<1?2:step<10?1:0)+" ms";
                text(g,stamp,{gx+4,area.getBottom()-17,84,16},10,ink);
            }
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
            if(sourceMode())g.reduceClipRegion(juce::Rectangle<float>(x(trimStart()),(float)area.getY(),x(trimEnd())-x(trimStart()),(float)area.getHeight()).getSmallestIntegerContainer());
            draw(owner.page==1?waveColour.withAlpha(.45f):waveColour);g.restoreState();
            if(channels==2)
            {
                text(g,"L",{area.getX()+4,area.getY()+2,20,16},10,muted);
                text(g,"R",{area.getX()+4,area.getCentreY()+2,20,16},10,muted);
            }
            else text(g,source->audio.getNumChannels()==1 || (channelView.getSelectedId()==1 && source->waveform.channelsAreIdentical())?"MONO":channelView.getSelectedId()==4?"R":"L",
                      {area.getX()+4,area.getY()+2,42,16},10,muted);
        }
        if(sourceMode() && owner.sound.loop!=0 && owner.sound.mode==2)
        {
            double length=owner.sound.end-owner.sound.start;
            g.setColour(juce::Colour(0xffe9ad70));
            for(auto position:{trimStart()+length*markerValue(owner.sound,&drum::SoundParameters::loopStart),trimStart()+length*markerValue(owner.sound,&drum::SoundParameters::loopEnd)})g.drawVerticalLine((int)x(position),(float)area.getY(),(float)area.getBottom());
        }
        if(sourceMode())
        {
            g.setColour(juce::Colour(0xff70c9ff));
            g.drawVerticalLine((int)x(trimStart()),(float)area.getY(),(float)area.getBottom());
            g.drawVerticalLine((int)x(trimEnd())-1,(float)area.getY(),(float)area.getBottom());
            const float left=x(trimStart()),right=x(trimEnd())-1,top=(float)area.getY();
            juce::Path handles;handles.addTriangle(left,top,left+12,top,left,top+14);handles.addTriangle(right,top,right-12,top,right,top+14);g.fillPath(handles);
            text(g,"START "+juce::String(trimStart()*100,2)+"%",{(int)left+5,area.getY()+18,100,18},11,juce::Colour(0xff70c9ff));
            text(g,"END "+juce::String(trimEnd()*100,2)+"%",{(int)right-100,area.getY()+18,98,18},11,juce::Colour(0xff70c9ff));
        }
        if(source){const auto head=owner.processor.getSampleEngine().playbackPosition(owner.note);
            if(head.active){const double position=sourceMode()?(reversed()?1-head.source:head.source):head.seconds/(source->audio.getNumSamples()/source->rate);
                if(position>=offset && position<=offset+1/zoom){const float px=x(position);g.setColour(playheadColour);g.fillRect(px-1,(float)area.getY(),2.0f,(float)area.getHeight());juce::Path flag;flag.addTriangle(px-5,(float)area.getY(),px+5,(float)area.getY(),px,(float)area.getY()+7);g.fillPath(flag);}}
        }
        g.restoreState();
        text(g,owner.path.isEmpty()?"Drop an audio sample here":juce::File(owner.path).getFileName(),{10,getHeight()-25,getWidth()-620,20},11,muted);
        auto detail=juce::String(zoom,1)+"x";
        if(source) detail+="  /  "+juce::String(source->audio.getNumSamples()/source->rate/zoom*1000,2)+" ms";
        text(g,detail,{getWidth()-420,getHeight()-25,280,20},11,muted);
    }
    double position(float x) const {return juce::jlimit(0.0,1.0,offset+(x-plot().getX())/(plot().getWidth()*zoom));}
    void mouseDown(const juce::MouseEvent& e) override
    {
        marker=-1;
        if(!sourceMode() || !source || !plot().contains(e.position.toInt())) return;
        double p=position(e.position.x); marker=std::abs(p-trimStart())<std::abs(p-trimEnd())?0:1; mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(marker<0) return;
        double p=position(e.position.x);
        p=marker==0?juce::jmin(p,trimEnd()-0.00001):juce::jmax(p,trimStart()+0.00001);
        owner.change(drum::soundParameters[(size_t)marker],p);
    }
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override
    {
        if(e.mods.isShiftDown())
        {
            const int direction=wheel.deltaY>0?1:wheel.deltaY<0?-1:0;
            verticalZoom.setSelectedId(juce::jlimit(1,10,verticalZoom.getSelectedId()+direction),juce::sendNotificationSync);
        }
        else if(e.mods.isCtrlDown())
        {
            setZoom(zoom*std::pow(1.4,wheel.deltaY*5),juce::jlimit(0.0,1.0,(e.position.x-plot().getX())/(double)plot().getWidth()));
        }
        else offset=juce::jlimit(0.0,1-1/zoom,offset-(std::abs(wheel.deltaX)>std::abs(wheel.deltaY)?wheel.deltaX:wheel.deltaY)/zoom);
        repaint();
    }
private:
    int displayChannels() const
    {
        if(!source)return 0;
        return source->audio.getNumChannels()==1 || channelView.getSelectedId()>=3
            || (channelView.getSelectedId()==1 && source->waveform.channelsAreIdentical())?1:2;
    }
    int audioChannel(int lane) const
    {
        return source->audio.getNumChannels()==1?0:channelView.getSelectedId()==4?1:channelView.getSelectedId()==3?0:lane;
    }
    void fitHeight()
    {
        if(!source)return;
        const int frames=source->audio.getNumSamples();
        const int begin=juce::jlimit(0,frames-1,(int)std::floor(offset*frames));
        const int end=juce::jlimit(begin+1,frames,(int)std::ceil((offset+1/zoom)*frames));
        float peak=0;
        for(int lane=0;lane<displayChannels();++lane)
        {
            auto range=source->waveform.range(source->audio,audioChannel(lane),reversed()?frames-end:begin,reversed()?frames-begin:end);
            peak=juce::jmax(peak,std::abs(range.minimum),std::abs(range.maximum));
        }
        const double reference=sourceMode()?juce::jmax(0.01f,source->peak):1.0f;
        const int power=peak>0?(int)std::floor(std::log2(reference/peak)):0;
        verticalZoom.setSelectedId(juce::jlimit(1,10,power+3),juce::sendNotificationSync);
    }
    void setDisplay(std::shared_ptr<const drum::Sample> value)
    {
        if(source!=value)cachedWidth=0;
        source=std::move(value);
        if(source)zoom=juce::jlimit(1.0,juce::jmax(1.0,source->audio.getNumSamples()/32.0),zoom);
        offset=juce::jlimit(0.0,1-1/zoom,offset);
        for(auto* button:{&zoomOut,&zoomIn,&fit})button->setEnabled(source!=nullptr);
        fitVertical.setEnabled(source!=nullptr);
    }
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
        if(cachedWidth==area.getWidth() && cachedHeight==area.getHeight() && cachedPixels==pixels && cachedZoom==zoom && cachedOffset==offset && cachedReverse==reversed()) return;
        cachedWidth=area.getWidth();cachedHeight=area.getHeight();cachedPixels=pixels;cachedZoom=zoom;cachedOffset=offset;cachedReverse=reversed();
        const int frames=source->audio.getNumSamples(),channels=displayChannels();
        const double begin=offset*frames,length=frames/zoom;
        sampleTrace=length<=pixels;
        paths.clear();paths.resize((size_t)channels);
        for(int channel=0;channel<channels;++channel)
        {
            auto& path=paths[(size_t)channel];
            const float middle=area.getY()+area.getHeight()*(channel+0.5f)/channels;
            const int sourceChannel=audioChannel(channel);
            const float amplitude=area.getHeight()*0.44f/channels;
            const float multiplier=(float)verticalScale/(sourceMode()?juce::jmax(0.01f,source->peak):1.0f);
            auto y=[&](float value){return middle-juce::jlimit(-1.0f,1.0f,value*multiplier)*amplitude;};
            if(sampleTrace)
            {
                const int first=juce::jlimit(0,frames-1,(int)std::floor(begin));
                const int last=juce::jlimit(first,frames,(int)std::ceil(begin+length));
                for(int n=first;n<=last;++n)
                {
                    const float xx=(float)(area.getX()+(n-begin)/length*area.getWidth());
                    const int index=juce::jmin(n,frames-1);
                    const float yy=y(source->audio.getSample(sourceChannel,reversed()?frames-1-index:index));
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
                    columns[(size_t)n]=source->waveform.range(source->audio,sourceChannel,reversed()?frames-to:from,reversed()?frames-from:to);
                    const float xx=(float)(area.getX()+(n+0.5)*area.getWidth()/pixels);
                    const float yy=y(columns[(size_t)n].maximum);
                    if(n==0)path.startNewSubPath((float)area.getX(),yy);
                    path.lineTo(xx,yy);
                }
                path.lineTo((float)area.getRight(),y(columns.back().maximum));
                path.lineTo((float)area.getRight(),y(columns.back().minimum));
                for(int n=pixels-1;n>=0;--n)
                    path.lineTo((float)(area.getX()+(n+0.5)*area.getWidth()/pixels),y(columns[(size_t)n].minimum));
                path.lineTo((float)area.getX(),y(columns.front().minimum));path.closeSubPath();
            }
        }
    }
    SoundEditor& owner;
    std::shared_ptr<const drum::Sample> source,original;
    drum::PreviewResult rendered;
    juce::ComboBox view,verticalZoom,channelView;
    juce::Label status;
    juce::TextButton zoomOut,zoomIn,fit,fitVertical;
    std::vector<juce::Path> paths;
    int cachedWidth=0,cachedHeight=0,cachedPixels=0;
    double cachedZoom=0,cachedOffset=0;
    double verticalScale=1;
    bool sampleTrace=false,cachedReverse=false;
};
class SoundEditor::WaveformDivider final : public juce::Component,public juce::SettableTooltipClient
{
public:
    explicit WaveformDivider(SoundEditor& e):owner(e)
    {
        setComponentID("waveform-divider");setName("Waveform height divider");
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setTooltip("Drag to resize the waveform. Double-click to restore its default height.");
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(muted.withAlpha(isMouseOverOrDragging()?0.8f:0.4f));
        g.fillRoundedRectangle(getWidth()*0.5f-32,getHeight()*0.5f-1,64,2,1);
    }
    void mouseDown(const juce::MouseEvent&) override {initialHeight=owner.waveform->getHeight();}
    void mouseDrag(const juce::MouseEvent& e) override
    {
        owner.waveformHeight=juce::jlimit(184,800,initialHeight+e.getDistanceFromDragStartY());
        owner.resized();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override {owner.waveformHeight=340;owner.resized();}
private:
    SoundEditor& owner;
    int initialHeight=340;
};
class SoundEditor::KeySpectrum final : public juce::Component {
public:
    drum::TonalAnalysis analysis;bool busy=false,hasSample=false;
    void update(const drum::TonalAnalysis& next,bool working,bool loaded){analysis=next;busy=working;hasSample=loaded;repaint();}
    void paint(juce::Graphics& g) override {
        g.setColour(bg);g.fillRoundedRectangle(getLocalBounds().toFloat(),8);
        text(g,"NOTE ENERGY / EDITED SOUND / BEFORE CHAIN",{14,8,getWidth()-28,22},12,muted);
        auto plot=getLocalBounds().reduced(44,0).withTrimmedTop(40).withTrimmedBottom(32);if(plot.getHeight()<20)return;
        const float width=plot.getWidth()/12.0f;const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for(int n=0;n<12;++n){auto column=juce::Rectangle<float>(plot.getX()+n*width,(float)plot.getY(),width,(float)plot.getHeight());g.setColour((n==1||n==3||n==6||n==8||n==10)?juce::Colour(0xff111e2b):juce::Colour(0xff1b2b38));g.fillRect(column);g.setColour(juce::Colour(0xff314151));g.drawVerticalLine((int)column.getX(),column.getY(),column.getBottom());
            if(!busy){const float power=analysis.noteEnergy[(size_t)n];const float level=power>0?juce::jlimit(0.0f,1.0f,(48+10*std::log10(power))/48):0;auto bar=column.reduced(6,0).withTop(column.getBottom()-level*column.getHeight());g.setGradientFill(juce::ColourGradient(spectrumColour,bar.getCentreX(),bar.getY(),juce::Colour(0xff8764d5).withAlpha(.2f),bar.getCentreX(),bar.getBottom(),false));g.fillRect(bar);}
            g.setColour(ink);g.setFont(juce::FontOptions(14));g.drawText(names[n],juce::Rectangle<int>((int)column.getX(),plot.getBottom()+5,(int)width,24),juce::Justification::centred);
        }
        for(int db=0;db>=-48;db-=12){int y=plot.getY()+juce::roundToInt(-db/48.0f*plot.getHeight());g.setColour(juce::Colour(0xff3b4d5b));g.drawHorizontalLine(y,(float)plot.getX(),(float)plot.getRight());text(g,juce::String(db),{4,y-9,36,18},11,muted);}
        if(!hasSample || busy)text(g,busy?"Analysing edited audio...":"Load a sample to analyse its notes.",plot.reduced(15),16);
    }
};
class SoundEditor::RoutingView final : public juce::Component {
public:
    drum::SoundParameters settings;juce::String instrument;bool hasChain=false,audible=true;std::array<bool,22> enabled{};
    std::array<bool,6> returnActive{};std::array<drum::ReturnRouting,6> returnRoutes{};std::array<juce::String,6> descriptions;
    std::array<juce::TextButton,6> edit;std::array<juce::Label,6> names;std::array<juce::String,6> rawNames;std::function<void(int)> openReturn;std::function<void(int,const juce::String&)> renameReturn;
    void setNames(const drum::Kit& kit){for(int n=0;n<6;++n){rawNames[(size_t)n]=kit.returns[(size_t)n].name;if(!names[(size_t)n].isBeingEdited())names[(size_t)n].setText(drum::sendLabel(kit,n),juce::dontSendNotification);}}
    RoutingView(){for(int n=0;n<6;++n){addAndMakeVisible(edit[(size_t)n]);edit[(size_t)n].setButtonText("Edit "+juce::String::charToString((juce::juce_wchar)('A'+n)));edit[(size_t)n].setComponentID("edit-return-"+juce::String(n));edit[(size_t)n].onClick=[this,n]{if(openReturn)openReturn(n);};auto& name=names[(size_t)n];addAndMakeVisible(name);name.setComponentID("send-name-"+juce::String(n));name.setEditable(true,true,false);name.setFont(juce::FontOptions(12));name.setColour(juce::Label::textColourId,ink);name.setTooltip("Click to rename this shared send. Clear the name to restore Send "+juce::String::charToString((juce::juce_wchar)('A'+n))+".");name.onEditorShow=[this,n]{names[(size_t)n].getCurrentTextEditor()->setText(rawNames[(size_t)n],false);names[(size_t)n].getCurrentTextEditor()->selectAll();};name.onTextChange=[this,n]{if(renameReturn)renameReturn(n,names[(size_t)n].getText());};}}
    void resized() override {const float row=(getHeight()-52)/7.f;for(int n=0;n<6;++n){edit[(size_t)n].setBounds((int)(getWidth()*.66f),(int)(24+row*(n+1)+2),juce::jmin(64,(int)(getWidth()*.08f)-4),juce::jmax(18,(int)row-6));names[(size_t)n].setBounds((int)(getWidth()*.47f)+4,(int)(24+row*(n+1)),(int)(getWidth()*.18f)-8,juce::jmax(14,(int)(row*.5f)));}}
    void paint(juce::Graphics& g) override {
        g.setColour(bg);g.fillRoundedRectangle(getLocalBounds().toFloat(),8);
        const float w=(float)getWidth(),h=(float)getHeight(),row=(h-52)/7,split=w*.40f,ret=w*.47f,main=w*.82f,centre=24+row*3.5f;
        auto box=[&](juce::Rectangle<float> r,const juce::String& title,juce::Colour c){g.setColour(c.withAlpha(.12f));g.fillRoundedRectangle(r,5);g.setColour(c);g.drawRoundedRectangle(r,5,1);g.setFont(juce::FontOptions(12));g.drawFittedText(title,r.reduced(5,1).toNearestInt(),juce::Justification::centred,2);};
        auto wire=[&](float x1,float y1,float x2,float y2,bool active,juce::Colour colour){g.setColour(active?colour:muted.withAlpha(.22f));g.drawLine(x1,y1,x2,y2,active?2.5f:1.f);if(x2>x1){juce::Path arrow;arrow.addTriangle(x2,y2,x2-5,y2-3,x2-5,y2+3);g.fillPath(arrow);}};
        box({12,centre-24,w*.14f,48},instrument+"\nEdited sound",audible?waveColour:muted);
        wire(12+w*.14f,centre,w*.19f,centre,audible,mint);box({w*.19f,centre-24,w*.17f,48},hasChain?"Instrument chain":"Chain: pass-through",audible?mint:muted);
        wire(w*.36f,centre,split,centre,audible,mint);
        const int destination=(int)settings.output;const bool mainDry=destination==0 || !enabled[(size_t)destination];
        wire(split,24+row*.5f,split,24+row*6.5f,audible,mint);
        const float dryY=24+row*.5f;wire(split,dryY,ret,dryY,audible,mint);
        box({ret,24,w*.27f,row-4},mainDry?(destination?"Dry: Main (fallback)":"Dry: Main"):"Dry: Out "+juce::String(destination+1),audible?mint:muted);
        if(mainDry)wire(w*.74f,dryY,main,dryY,audible,mint);else {wire(w*.74f,dryY,w-12,dryY,audible,mint);text(g,"DAW",{(int)(w*.90f),3,65,18},11,muted);}
        const double sends[]{settings.sendA,settings.sendB,settings.sendC,settings.sendD,settings.sendE,settings.sendF};
        for(int n=0;n<6;++n){const bool on=audible && sends[n]>drum::sendFloor;const float y=24+row*(n+1),mid=y+row*.5f;wire(split,mid,ret,mid,on,spectrumColour);
            const auto level=sends[n]<=drum::sendFloor?juce::String::fromUTF8("\xe2\x88\x92\xe2\x88\x9e"):juce::String(sends[n],1)+" dB";
            box({ret,y,w*.27f,row-4},{},returnActive[(size_t)n]?spectrumColour:muted);
            juce::String destinations=returnRoutes[(size_t)n].main?"M":"";for(int target=0;target<6;++target)if(returnRoutes[(size_t)n].sends[(size_t)target]>drum::sendFloor)destinations+=juce::String::charToString((juce::juce_wchar)('A'+target));if(destinations.isEmpty())destinations="None";
            text(g,level+" | "+descriptions[(size_t)n]+" | "+juce::String::charToString(0x2192)+destinations,{(int)ret+6,(int)(y+row*.5f),(int)(w*.18f)-10,(int)(row*.5f)-3},12,returnActive[(size_t)n]?spectrumColour:muted);
            wire(w*.74f,mid,main,mid,returnActive[(size_t)n] && returnRoutes[(size_t)n].main,spectrumColour);
        }
        static const juce::uint32 routeColours[]{0xff68b6ff,0xffffbd69,0xff8cda83,0xffed91d1,0xffb49bff,0xff59d9cd};
        for(int source=0;source<6;++source)for(int target=0;target<6;++target)if(returnRoutes[(size_t)source].sends[(size_t)target]>drum::sendFloor){const float x=w*.74f,lane=w*(.754f+source*.009f),from=24+row*(source+1.5f)+4,to=24+row*(target+1.5f)-4;const bool active=returnActive[(size_t)source];g.setColour(active?juce::Colour(routeColours[source]):muted.withAlpha(.25f));juce::Path route;route.startNewSubPath(x,from);route.lineTo(lane,from);route.lineTo(lane,to);route.lineTo(x,to);g.strokePath(route,juce::PathStrokeType(active?2.f:1.f));juce::Path arrow;arrow.addTriangle(x,to,x+5,to-3,x+5,to+3);g.fillPath(arrow);}
        bool anyMain=mainDry && audible;for(int n=0;n<6;++n)anyMain|=returnActive[(size_t)n] && returnRoutes[(size_t)n].main;
        wire(main,dryY,main,24+row*6.5f,anyMain,mint);
        box({main+8,centre-28,w-main-20,56},"Main sum\nTo DAW",mint);wire(main,centre,main+8,centre,anyMain,mint);
        text(g,"Selected instrument levels; lit shared routes. Click names to rename; Edit for destinations.",{12,getHeight()-23,getWidth()-24,20},11,muted);
    }
};
namespace { void neutralEnvelope(drum::SoundParameters&,int,double); }
SoundEditor::SoundEditor(DrumAudioProcessor& p):processor(p),chain(p.getLibrary(),p.getEffects()),waveform(std::make_unique<Waveform>(*this)),waveformDivider(std::make_unique<WaveformDivider>(*this))
{
    setComponentID("sound-editor"); setOpaque(true);addAndMakeVisible(chain);
    addAndMakeVisible(noteIndicator);noteIndicator.setComponentID("note-indicator");noteIndicator.setFont(juce::FontOptions(11));noteIndicator.setColour(juce::Label::textColourId,mint);
    noteIndicator.setTooltip("MIDI trigger is the pad assignment. Preview pitch/key is calculated from processed audio, before external effects. A single drum note cannot establish major/minor key. Manual key labels are saved per instrument.");
    auto button=[this](juce::TextButton& b,const char* title,const char* id) {addAndMakeVisible(b); b.setButtonText(title); b.setComponentID(id);};
    button(soloButton,"Solo","editor-instrument-solo");soloButton.setClickingTogglesState(true);
    soloButton.setTooltip("Solo this instrument. Linked to the main instrument Solo; mute takes priority.");
    soloButton.onClick=[this]{
        auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);if(!kit || kit->id!=kitId)return;
        auto* instrument=drum::findInstrument(*kit,instrumentId);if(!instrument)return;
        auto controls=instrument->controls;controls.solo=soloButton.getToggleState();
        if(processor.getLibrary().setControls(kitId,instrumentId,controls).ok)processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        refresh();if(controlsChanged)controlsChanged();
    };
    button(notesButton,"Notes","editor-instrument-notes");notesButton.onClick=[this]{if(showNotes)showNotes();};
    button(exportButton,"Export...","editor-instrument-export");exportButton.onClick=[this]{if(exportInstrument)exportInstrument();};
    button(back,"Exit","close-sound-editor"); back.onClick=[this]{if(close)close();};
    button(reset,"Reset page","reset-edit-page"); reset.onClick=[this]{
        auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);
        if(!i || kit->id!=kitId) return;
        auto c=i->controls; for(auto& parameter:drum::soundParameters) if((page==1?(parameter.page==1 || parameter.page==2):page!=2 && parameter.page==page) && !(page==1 && parameter.member==&drum::SoundParameters::detune)) c.sound.*(parameter.member)=parameter.initial;
        if(page==3)c.sound.envelopes[3]={};
        if(page==8){c.tuning=0;c.sound.detune=0;}
        if(page==1){for(int n=0;n<3;++n){neutralEnvelope(c.sound,n,envelopeLimit());constrainEnvelope(c.sound,n);}}else if(page==2)c.sound.equalizer={};
        auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
        if(result.ok){for(int n=0;n<3;++n)if(page==1)recordEnvelope(n,i->controls,c);processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
        message=result.ok?"Page reset. The source file is unchanged.":result.message; refresh();
    };
    const char* names[]{"Sample","Envelopes","Filter","LFO","Outputs & Sends","Macros","Attack","Chain","Tune"};
    lfoGraph=std::make_unique<EnvelopeEditor>("lfo-cycle","LFO cycle",mint);addAndMakeVisible(*lfoGraph);lfoGraph->setCycleMode();lfoGraph->setTimeLimit(1000);
    lfoGraph->onShape=[this](const auto& shape,bool){changeLfo(shape);};lfoGraph->onReset=[this]{drum::EnvelopeShape flat;flat.count=2;flat.hold=0;flat.points[0]={0,.5};flat.points[1]={1000,.5};changeLfo(flat,true);};
    lfoGraph->onBeginGesture=[this]{lfoGesture=true;lfoRecorded=false;};lfoGraph->onEndGesture=[this]{lfoGesture=lfoRecorded=false;};lfoGraph->onUndo=[this]{undoLfo(false);};lfoGraph->onRedo=[this]{undoLfo(true);};
    routing=std::make_unique<RoutingView>();addAndMakeVisible(*routing);routing->setComponentID("output-routing");
    routing->renameReturn=[this](int n,const juce::String& name){auto result=processor.getLibrary().setReturnName(kitId,n,name);if(result.ok)processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));else message=result.message;refresh();};
    routing->openReturn=[this](int n){returnWindowKit=kitId;returnWindowIndex=n;auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);if(!kit)return;class ReturnWindow final:public juce::DocumentWindow{public:ReturnWindow(juce::String title):DocumentWindow(title,juce::Colour(0xff19212e),closeButton){setUsingNativeTitleBar(true);}void closeButtonPressed() override{setVisible(false);}};auto editor=std::make_unique<ChainEditor>(processor.getLibrary(),processor.getEffects());editor->select(kitId,"return-"+juce::String(n));editor->setSize(1000,500);returnWindow=std::make_unique<ReturnWindow>("Drum shared "+drum::sendLabel(*kit,n));returnWindow->setContentOwned(editor.release(),true);returnWindow->setResizable(true,false);returnWindow->centreWithSize(1020,540);returnWindow->setVisible(true);};
    addAndMakeVisible(attackOnly);attackOnly.setComponentID("attack-waveform-only");attackOnly.setTooltip("Show only the generated attack layer in the waveform. Playback still includes the sample.");attackOnly.onClick=[this]{waveform->showEdited();refreshPreview();};
    keySpectrum=std::make_unique<KeySpectrum>();addAndMakeVisible(*keySpectrum);keySpectrum->setComponentID("key-spectrum");
    auto tuningControl=[this](ResetSlider& slider,juce::Label& label,const char* title,const char* id,double low,double high,double step,const char* suffix){addAndMakeVisible(slider);addAndMakeVisible(label);slider.setComponentID(id);useValueBar(slider);slider.setRange(low,high,step);slider.setTextValueSuffix(suffix);slider.setDoubleClickReturnValue(true,0);slider.setColour(juce::Slider::trackColourId,mint.withAlpha(.3f));slider.setColour(juce::Slider::thumbColourId,mint);label.setText(title,juce::dontSendNotification);label.setColour(juce::Label::textColourId,juce::Colours::white);label.setFont(juce::FontOptions(14));label.setJustificationType(juce::Justification::centredRight);slider.onValueChange=[this]{changeKeyTuning();};};
    tuningControl(keyCoarse,keyCoarseLabel,"Coarse","key-coarse",-48,48,1," st");tuningControl(keyFine,keyFineLabel,"Fine","key-fine",-100,100,.1," cents");
    keyCoarse.setTooltip("Transpose this instrument in semitones. Uses the same tuning as its main controls; playback speed changes.");keyFine.setTooltip("Fine tuning in cents: 100 cents is one semitone. Applies to the instrument across all tabs.");addAndMakeVisible(keyStatus);keyStatus.setComponentID("tune-key");keyStatus.setColour(juce::Label::textColourId,ink);keyStatus.setFont(juce::FontOptions(20));
    for(int n=0;n<(int)tabs.size();++n) {addAndMakeVisible(tabs[(size_t)n]); tabs[(size_t)n].setButtonText(names[n]); tabs[(size_t)n].setComponentID("edit-tab-"+juce::String(n)); tabs[(size_t)n].onClick=[this,n]{setPage(n);};}
    addAndMakeVisible(viewport); viewport.setComponentID("sound-controls");viewport.setViewedComponent(&content,false); viewport.setScrollBarsShown(true,false); viewport.setScrollBarThickness(8);
    addAndMakeVisible(*waveform);
    addAndMakeVisible(*waveformDivider);
    addAndMakeVisible(adsrValues);adsrValues.setComponentID("envelope-numeric-values");adsrValues.onClick=[this]{setPage(page);};
    addAndMakeVisible(eqTab);addAndMakeVisible(filterTab);eqTab.setComponentID("filter-eq-tab");filterTab.setComponentID("filter-envelope-tab");
    eqTab.onClick=[this]{setPage(2);};filterTab.onClick=[this]{selectedEnvelope=2;waveform->showEdited();setPage(1);};
    addAndMakeVisible(equalizer);
    equalizer.onChange=[this](const drum::Equalizer& eq){
        auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);
        if(!i || kit->id!=kitId)return;auto c=i->controls;c.sound.equalizer=eq;
        auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
        if(result.ok)processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        waveform->showEdited();message=result.ok?"16-band stereo EQ saved. Filter envelope shapes the separate tone filter.":result.message;refresh();
    };
    envelopes[0]=std::make_unique<EnvelopeEditor>("envelope-amp","Amplitude",waveColour);
    envelopes[1]=std::make_unique<EnvelopeEditor>("envelope-pitch","Pitch (st)",juce::Colour(0xff9bbdff));
    envelopes[2]=std::make_unique<EnvelopeEditor>("envelope-filter","Filter (oct)",juce::Colour(0xffbd97ff));
    for(int n=0;n<3;++n)
    {
        addAndMakeVisible(*envelopes[(size_t)n]);
        envelopes[(size_t)n]->onShape=[this,n](const drum::EnvelopeShape& s,bool edit){changeEnvelope(n,s,edit);};
        envelopes[(size_t)n]->onUseAdsr=[this,n]{changeEnvelope(n,{},false);};
        envelopes[(size_t)n]->onReset=[this,n]{resetEnvelope(n);};
        envelopes[(size_t)n]->onUndo=[this,n]{undoEnvelope(n,false);};
        envelopes[(size_t)n]->onRedo=[this,n]{undoEnvelope(n,true);};
        envelopes[(size_t)n]->onBeginGesture=[this,n]{beginEnvelopeGesture(n);};
        envelopes[(size_t)n]->onEndGesture=[this,n]{endEnvelopeGesture(n);};
        envelopes[(size_t)n]->onActivate=[this,n]{activateEnvelope(n);};
        if(n>0)envelopes[(size_t)n]->onAmount=[this,n](double value){waveform->showEdited();for(auto& p:drum::soundParameters)if(p.member==(n==1?&drum::SoundParameters::pitchAmount:&drum::SoundParameters::filterAmount))change(p,value);};
    }
    addAndMakeVisible(ampOverlay);addAndMakeVisible(pitchOverlay);ampOverlay.setComponentID("overlay-amplitude");pitchOverlay.setComponentID("overlay-pitch");
    ampOverlay.onClick=[this]{selectedEnvelope=0;waveform->showEdited();setPage(1);};pitchOverlay.onClick=[this]{selectedEnvelope=1;waveform->showEdited();setPage(1);};
    for(int n=0;n<3;++n)envelopes[(size_t)n]->onOverlayPan=[this](double ms){waveform->offset=juce::jlimit(0.0,1-1/waveform->zoom,waveform->offset+ms/waveform->timelineMs());waveform->repaint();syncEnvelopeOverlay();};
    for(int n=0;n<3;++n)envelopes[(size_t)n]->onOverlayWheel=[this](const juce::MouseEvent& e,const juce::MouseWheelDetails& w){waveform->mouseWheelMove(e.getEventRelativeTo(waveform.get()),w);syncEnvelopeOverlay();};
    for(auto& parameter:drum::soundParameters) {if(parameter.member==&drum::SoundParameters::keyOverride)continue;auto field=std::make_unique<Field>(parameter,*this); content.addAndMakeVisible(*field); fields.push_back(std::move(field));}
    for(int n=0;n<drum::numMacros;++n) {auto field=std::make_unique<MacroField>(*this,n);content.addAndMakeVisible(*field);macroFields.push_back(std::move(field));}
    content.addAndMakeVisible(sliceCount);sliceCount.setComponentID("slice-count");sliceCount.setName("Number of equal slices");
    for(int count:{2,4,8,16,32,64}) sliceCount.addItem(juce::String(count)+" equal slices",count);sliceCount.setSelectedId(8);
    content.addAndMakeVisible(slice);slice.setButtonText("Slice to new group");slice.setComponentID("slice-to-group");
    slice.onClick=[this]{
        auto result=processor.getLibrary().sliceInstrument(kitId,instrumentId,sliceCount.getSelectedId());
        if(result.ok) processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        message=result.ok?"Created a new group of slices on free keys. The original instrument is preserved.":result.message;refresh();
    };
    setPage(0);startTimerHz(30);
}
SoundEditor::~SoundEditor() {stopTimer();viewport.setViewedComponent(nullptr,false);}
void SoundEditor::select(const juce::String& kit,const drum::Instrument* i)
{
    processor.refreshMacroNames();
    const bool changed=kitId!=kit || instrumentId!=(i?i->id:juce::String());
    processor.getSampleEngine().analyserNote.store(i?i->note:-1);
    equalizer.setSpectrum(&processor.getSampleEngine().eqSpectrum,"Instrument pre-chain",changed);
    if(changed){lfoUndo.clear();lfoRedo.clear();lfoGesture=lfoRecorded=false;}
    if(changed || path!=(i?i->samplePath:juce::String()))envelopeHistory={};
    updating=true; kitId=kit; instrumentId=i?i->id:juce::String(); name=i?i->name:"No instrument selected"; path=i?i->samplePath:juce::String(); note=i?i->note:21; sound=i?i->controls.sound:drum::SoundParameters{};
    soloButton.setEnabled(i!=nullptr);soloButton.setToggleState(i && i->controls.solo,juce::dontSendNotification);
    chainPresent=i && !i->effects.empty();chain.select(kitId,instrumentId);
    if(changed) {waveform->zoom=1;waveform->offset=0;message.clear();}
    for(auto& field:fields)
    {
        field->update(markerValue(sound,field->parameter.member),changed);bool custom=false;
        for(int n=0;n<3;++n)for(auto member:envelopeMembers[(size_t)n])if(field->parameter.member==member)custom=sound.envelopes[(size_t)n].count!=0;
        if(oppositeMarker(field->parameter.member)!=field->parameter.member)
            field->slider.setTooltip(sound.reverse!=0?"Position measured from the reversed beginning. Start is where reverse playback begins; the original file is unchanged.":"Position measured from the forward beginning of the source. Loop positions are relative to the trimmed region.");
        bool available=true;const auto member=field->parameter.member;
        if(field->parameter.page==6 && member!=&drum::SoundParameters::attackOn)available=sound.attackOn!=0;
        if(field->parameter.page==3 && member!=&drum::SoundParameters::lfoOn)available=sound.lfoOn!=0;
        if(field->parameter.page==2 && member!=&drum::SoundParameters::filterOn)available=sound.filterOn!=0;
        if(member==&drum::SoundParameters::lfoRate)available&=sound.lfoSync==0;
        if(member==&drum::SoundParameters::lfoDivision)available&=sound.lfoSync!=0;
        if(member==&drum::SoundParameters::loop)available=sound.mode==2;
        if(member==&drum::SoundParameters::loopStart || member==&drum::SoundParameters::loopEnd || member==&drum::SoundParameters::loopFade)available=sound.loop!=0 && sound.mode==2;
        if(member==&drum::SoundParameters::fadeIn)available=sound.mode!=2;
        field->setEnabled(i!=nullptr && !custom && available);
        if(field->parameter.member==&drum::SoundParameters::fadeIn)field->slider.setTooltip(sound.mode==2?"Envelope playback uses the amplitude graph for its attack. Fade in applies in Trigger/Gate.":"Fade the beginning of a Trigger/Gate hit. View Edited to see the result.");
    }
    for(int n=0;n<drum::numMacros;++n)
    {
        auto& field=*macroFields[(size_t)n];field.update(i?i->controls.macros[(size_t)n]:drum::MacroMapping{},changed);field.setEnabled(i!=nullptr);
        if(!field.value.isMouseButtonDown() && !field.value.hasKeyboardFocus(true)) field.value.setValue(processor.macro(n).get()*100,juce::dontSendNotification);
    }
    const auto nextSample=processor.getSampleEngine().waveformFor(instrumentId);
    if(selectedSample && nextSample!=selectedSample)envelopeHistory={};
    selectedSample=nextSample;
    selectedControls=i?i->controls:drum::InstrumentControls{};
    if(!keyCoarse.isMouseButtonDown())keyCoarse.setValue(selectedControls.tuning,juce::dontSendNotification);
    if(!keyFine.isMouseButtonDown())keyFine.setValue(sound.detune,juce::dontSendNotification);
    bool mappedCoarse=false,mappedFine=false;for(const auto& m:selectedControls.macros){mappedCoarse|=m.target==3;if(m.target>3)mappedFine|=drum::macroTargets[(size_t)m.target].member==&drum::SoundParameters::detune;}
    keyCoarse.setEnabled(i && !mappedCoarse);keyFine.setEnabled(i && !mappedFine);
    equalizer.setEnabled(i!=nullptr);equalizer.update(sound.equalizer,processor.getSampleEngine().previewSampleRate());
    auto libraryState=processor.getLibrary().snapshot();if(const auto* kit=drum::activeKit(libraryState)){routing->setNames(*kit);routing->returnRoutes=kit->returnRouting;waveform->setSendNames(*kit);for(auto& macro:macroFields)for(int t=0;t<(int)std::size(drum::macroTargets);++t){juce::String title=drum::macroTargets[(size_t)t].name;if(title.startsWith("Send "))macro->target.changeItemText(t+1,drum::sendLabel(*kit,title.getLastCharacter()-'A'));}for(auto& field:fields){auto key=juce::String(field->parameter.key);if(key.startsWith("send")){int n=key.getLastCharacter()-'A';field->sendCaption=drum::sendLabel(*kit,n);field->slider.setName(field->sendCaption);field->repaint();}}if(returnWindow && returnWindowKit==kit->id && returnWindowIndex>=0)returnWindow->setName("Drum shared "+drum::sendLabel(*kit,returnWindowIndex));}

    if(i && !i->controls.solo)for(auto& group:drum::activeKit(libraryState)->groups)for(auto& other:group.instruments)
        if(other.controls.solo)selectedControls.mute=true;
    waveform->setSample(selectedSample,changed);
    reset.setEnabled(i!=nullptr);
    updating=false; refreshPreview();refreshEnvelopes(changed);repaint();
}
void SoundEditor::refreshEnvelopes(bool newSelection)
{
    auto effective=sound;
    auto mapped=[this](SoundMember member) {
        for(auto& m:selectedControls.macros)if(m.target>3 && drum::macroTargets[(size_t)m.target].member==member)return true;
        return false;
    };
    for(int n=0;n<drum::numMacros;++n)
    {
        const auto& m=selectedControls.macros[(size_t)n];
        if(m.target>3)effective.*(drum::macroTargets[(size_t)m.target].member)=m.minimum+(m.maximum-m.minimum)*processor.macro(n).get();
    }
    drum::resolveEnvelopeMacros(effective,selectedControls.macros);
    for(int n=0;n<3;++n)
    {
        const auto& members=envelopeMembers[(size_t)n];std::array<bool,4> bindings;
        for(int h=0;h<4;++h)bindings[(size_t)h]=mapped(members[(size_t)h]);
        const double amount=n==0?1:n==1?effective.pitchAmount:effective.filterAmount;
        const bool active=n==0?sound.mode==2:n==1?amount!=0:sound.filterOn!=0 && amount!=0;
        juce::String status=n==0?(active?"Envelope / S holds until note-off; release follows":"Drag to enable amplitude shaping (Envelope playback)")
            :n==1?(amount==0?"Zero amount: drag a point to enable +12 st":"Pitch shape / hold at S until note-off")
            :sound.filterOn==0 || amount==0?"Drag to enable tone filter and envelope":"Tone filter shape / S holds until note-off";
        juce::String controlled;
        for(int h=0;h<4;++h)if(bindings[(size_t)h])controlled+=juce::String::charToString("ADSR"[h]);
        if(controlled.isNotEmpty())status="Macro controls timing: "+controlled+" / Unlink timing keeps this shape and frees editing";
        const bool amountMapped=n>0 && mapped(n==1?&drum::SoundParameters::pitchAmount:&drum::SoundParameters::filterAmount);
        if(amountMapped)status=amount==0?"Inactive: macro sets amount to zero":"Amount controlled by macro";
        auto& graph=*envelopes[(size_t)n];graph.setEnabled(instrumentId.isNotEmpty());
        auto& history=envelopeHistory[(size_t)n];const auto actual=envelopeState(selectedControls,n);
        if(history.known && history.expected!=actual)history={};
        history.known=true;history.expected=actual;
        graph.setTimeLimit(envelopeLimit());
        graph.update({effective.*members[0],effective.*members[1],effective.*members[2],effective.*members[3]},effective.envelopes[(size_t)n],controlled.isNotEmpty(),amount,n==0?"%":n==1?"st":"oct",active,status,newSelection,amountMapped,n!=2 || sound.filterOn!=0);
        graph.setHistory(!history.undo.empty(),!history.redo.empty());
        graph.setActivation(controlled.isNotEmpty()?"Unlink timing":n==0 && !active?"Use Envelope":"");
    }
}
void SoundEditor::activateEnvelope(int envelope)
{
    auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* instrument=drum::findInstrument(*kit,instrumentId);
    if(!instrument || kit->id!=kitId)return;
    auto c=instrument->controls;bool unlinked=false;
    const auto& members=envelopeMembers[(size_t)envelope];
    for(int n=0;n<drum::numMacros;++n)
    {
        const auto& m=c.macros[(size_t)n];if(m.target<=3)continue;
        const auto member=drum::macroTargets[(size_t)m.target].member;
        if(std::find(members.begin(),members.end(),member)!=members.end())
        {c.sound.*member=m.minimum+(m.maximum-m.minimum)*processor.macro(n).get();unlinked=true;}
    }
    // Bake the currently displayed envelope before removing its timing mappings.
    // Other macro destinations and the host's macro values remain unchanged.
    drum::resolveEnvelopeMacros(c.sound,c.macros);
    if(unlinked)
    {
        for(auto& m:c.macros)if(m.target>3 && std::find(members.begin(),members.end(),drum::macroTargets[(size_t)m.target].member)!=members.end())m={};
    }
    else if(envelope==0)c.sound.mode=2;else if(envelope==2)c.sound.filterOn=1;
    const auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
    if(result.ok){recordEnvelope(envelope,instrument->controls,c);processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
    message=result.ok?(unlinked?"Timing macros unlinked for this envelope; current timing preserved. Points and lines can now be edited.":"Envelope enabled."):result.message;
    refresh();
}
double SoundEditor::envelopeLimit() const
{return selectedSample && selectedSample->rate>0?std::clamp(1000.0*selectedSample->audio.getNumSamples()/selectedSample->rate,0.001,80000.0):80000.0;}
void SoundEditor::constrainEnvelope(drum::SoundParameters& s,int n,SoundMember changed) const
{
    const double limit=envelopeLimit();auto& shape=s.envelopes[(size_t)n];
    for(int k=0;k<shape.count;++k)shape.points[(size_t)k].ms=std::min(shape.points[(size_t)k].ms,limit);
    const auto& m=envelopeMembers[(size_t)n];
    double total=s.*m[0]+s.*m[1]+s.*m[3];
    if(total<=limit)return;
    // Honour the edited stage (including a numeric default reset), fitting the
    // other time stages into the remainder of the original sample duration.
    if(changed==m[0] || changed==m[1] || changed==m[3])
    {
        s.*changed=std::min(s.*changed,limit);
        double remaining=0;for(int k:{0,1,3})if(m[(size_t)k]!=changed)remaining+=s.*m[(size_t)k];
        if(remaining>0)for(int k:{0,1,3})if(m[(size_t)k]!=changed)s.*m[(size_t)k]*=(limit-s.*changed)/remaining;
    }
    total=s.*m[0]+s.*m[1]+s.*m[3];
    if(total>limit)for(int k:{0,1,3})s.*m[(size_t)k]*=limit/total;
}
namespace { void neutralEnvelope(drum::SoundParameters& sound,int n,double length){auto& shape=sound.envelopes[(size_t)n];shape={};shape.count=2;shape.hold=0;const double level=n==0?1.0:0.0;shape.points[0]={0,level};shape.points[1]={length,level};if(n==1)sound.pitchAmount=0;if(n==2)sound.filterAmount=0;} }
SoundEditor::EnvelopeState SoundEditor::envelopeState(const drum::InstrumentControls& c,int n)
{
    EnvelopeState value;value.shape=c.sound.envelopes[(size_t)n];value.macros=c.macros;
    for(int k=0;k<4;++k)value.values[(size_t)k]=c.sound.*envelopeMembers[(size_t)n][(size_t)k];
    value.amount=n==1?c.sound.pitchAmount:n==2?c.sound.filterAmount:0;
    value.enabled=n==0?c.sound.mode:n==2?c.sound.filterOn:0;return value;
}
void SoundEditor::beginEnvelopeGesture(int n) {auto& h=envelopeHistory[(size_t)n];h.gesture=true;h.recorded=false;}
void SoundEditor::endEnvelopeGesture(int n) {auto& h=envelopeHistory[(size_t)n];h.gesture=false;h.recorded=false;}
void SoundEditor::recordEnvelope(int n,const drum::InstrumentControls& before,const drum::InstrumentControls& after)
{
    auto& h=envelopeHistory[(size_t)n];const auto a=envelopeState(before,n),b=envelopeState(after,n);
    if(a==b)return;
    if(!h.gesture || !h.recorded){if(h.undo.size()==64)h.undo.erase(h.undo.begin());h.undo.push_back(a);h.recorded=true;}
    h.redo.clear();h.expected=b;h.known=true;
}
void SoundEditor::resetEnvelope(int n)
{
    auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);
    if(!i || kit->id!=kitId)return;
    auto c=i->controls;const drum::SoundParameters defaults;
    for(auto member:envelopeMembers[(size_t)n])c.sound.*member=defaults.*member;
    neutralEnvelope(c.sound,n,envelopeLimit());
    constrainEnvelope(c.sound,n);
    const auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
    if(result.ok){recordEnvelope(n,i->controls,c);processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
    waveform->showEdited();message=result.ok?"Envelope reset to neutral. Undo restores the previous settings.":result.message;refresh();
}
void SoundEditor::undoEnvelope(int n,bool redo)
{
    auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);
    if(!i || kit->id!=kitId)return;
    auto& h=envelopeHistory[(size_t)n];const auto current=envelopeState(i->controls,n);
    if(!h.known || h.expected!=current){h={};refresh();return;}
    auto& from=redo?h.redo:h.undo;auto& to=redo?h.undo:h.redo;if(from.empty())return;
    const auto value=from.back();auto c=i->controls;c.sound.envelopes[(size_t)n]=value.shape;c.macros=value.macros;
    for(int k=0;k<4;++k)c.sound.*envelopeMembers[(size_t)n][(size_t)k]=value.values[(size_t)k];
    if(n==0)c.sound.mode=value.enabled;
    else if(n==1)c.sound.pitchAmount=value.amount;
    else {c.sound.filterAmount=value.amount;c.sound.filterOn=value.enabled;}
    const auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
    if(result.ok){to.push_back(current);from.pop_back();h.expected=value;h.gesture=h.recorded=false;processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
    waveform->showEdited();message=result.ok?(redo?"Envelope edit redone.":"Envelope edit undone."):result.message;refresh();
}
void SoundEditor::changeEnvelope(int envelope,const drum::EnvelopeShape& shape,bool audibleEdit)
{
    if(updating || instrumentId.isEmpty())return;
    auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);
    if(!i || kit->id!=kitId)return;
    auto controls=i->controls;const auto& members=envelopeMembers[(size_t)envelope];
    for(auto& m:controls.macros)if(m.target>3)for(auto member:members)if(drum::macroTargets[(size_t)m.target].member==member)return;
    controls.sound.envelopes[(size_t)envelope]=shape;constrainEnvelope(controls.sound,envelope);
    if(envelope==0 && audibleEdit)controls.sound.mode=2;
    bool mappedAmount=false;for(auto& m:controls.macros)mappedAmount|=m.target==11;
    if(envelope==1 && audibleEdit && controls.sound.pitchAmount==0 && !mappedAmount)controls.sound.pitchAmount=12;
    if(envelope==2 && audibleEdit)
    {
        controls.sound.filterOn=1;bool mapped=false;for(auto& m:controls.macros)mapped|=m.target==10;
        if(controls.sound.filterAmount==0 && !mapped)controls.sound.filterAmount=-2;
    }
    if(audibleEdit)waveform->showEdited();
    auto result=processor.getLibrary().setControls(kitId,instrumentId,controls);
    if(result.ok){recordEnvelope(envelope,i->controls,controls);processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
    message=result.ok?"Shape saved. Click a line to add a point; drag a point to move it; drag a line to curve it. S holds until note-off; preview gate is 1 s.":result.message;
    refresh();
}
void SoundEditor::refreshPreview()
{
    drum::PreviewInput input;
    if(isVisible())input.source=selectedSample;
    input.controls=selectedControls;input.send=page==8?-1:waveform->send();input.attackOnly=page==6 && attackOnly.getToggleState();
    for(int n=0;n<drum::numMacros;++n)input.macros[(size_t)n]=processor.macro(n).get();
    input.bpm=processor.getSampleEngine().previewTempo();input.rate=processor.getSampleEngine().previewSampleRate();
    preview.request(input);const auto result=preview.latest();const bool busy=preview.pending();waveform->setPreview(result,busy);
    for(auto& field:fields)if(field->parameter.member==&drum::SoundParameters::sampleGain)
    {
        const bool clips=!busy && result.sample && result.sample->peak>1.0f;
        field->slider.setColour(juce::Slider::textBoxBackgroundColourId,clips?juce::Colour(0xff8f2638):juce::Colours::transparentBlack);
        field->slider.setTooltip("Red only when the processed preview exceeds 0 dBFS (80% velocity, 1 s gate). Gain and display zoom alone do not indicate clipping.");
    }
    juce::String description="Trigger: "+drum::noteName(note)+" / MIDI "+juce::String(note)+"     Preview: ";
    const auto& analysis=result.analysis;
    if(!selectedSample)description+="load a sample";
    else if(busy)description+="analysing...";
    else if(analysis.midi<0)description+="no reliable pitch";
    else if(analysis.varying)description+=drum::musicalNote(analysis.lowMidi)+" to "+drum::musicalNote(analysis.highMidi)+" (varies)";
    else description+=drum::musicalNote(analysis.midi)+" / "+juce::String(440*std::pow(2.0,(analysis.midi-69)/12),1)+" Hz";
    description+=busy?"     Key: analysing...":"     Key: "+drum::spectrumKey(analysis);
    noteIndicator.setText(description,juce::dontSendNotification);
    keySpectrum->update(analysis,busy,selectedSample!=nullptr);
    if(!busy && input.send==-1 && !input.attackOnly)processor.getSampleEngine().rememberKey(instrumentId,selectedSample,selectedControls,input.macros,input.bpm,input.rate,analysis);
    const auto [keyAnalysis,keyTranspose]=processor.getSampleEngine().displayKey(instrumentId,selectedControls,input.macros);
    const int keyNote=drum::strongestNote(keyAnalysis,keyTranspose);
    juce::String detail=!selectedSample?"Key: load a sample":keyNote<0?"Key: no signal":
        "Key: "+drum::spectrumKey(keyAnalysis,keyTranspose)+"     Camelot: "+drum::camelotCode(keyNote)+" (major) / "+drum::camelotCode(keyNote+12)+" (minor)";
    keyStatus.setText(detail,juce::dontSendNotification);
    drum::EnvelopeShape cycle=sound.envelopes[3];if(sound.lfoShape!=6 || cycle.count<2){cycle={};cycle.count=sound.lfoShape==6?2:9;cycle.hold=0;for(int n=0;n<cycle.count;++n){const double phase=(double)n/(cycle.count-1);double level=.5;switch((int)sound.lfoShape){case 0:level=.5+.5*std::sin(phase*juce::MathConstants<double>::twoPi);break;case 1:level=1-2*std::abs(phase-.5);break;case 2:level=phase<.5?1:0;break;case 3:level=phase;break;case 4:level=1-phase;break;case 5:level=std::fmod(std::sin(std::floor(phase*8)+1)*43758.5453,1.0);if(level<0)level+=1;break;}cycle.points[(size_t)n]={phase*1000,level};}if(sound.lfoShape==0)for(int n=1;n<cycle.count;++n){const double phase=(n-.5)/(cycle.count-1);drum::bendEnvelopeSegment(cycle.points[(size_t)n-1].level,cycle.points[(size_t)n],.5,.5+.5*std::sin(phase*juce::MathConstants<double>::twoPi));}}
    lfoGraph->update({},cycle,false,1,"%",true,"One cycle / click to add / drag line to curve / Shift: fine",false);lfoGraph->setHistory(!lfoUndo.empty(),!lfoRedo.empty());
    routing->settings=selectedControls.sound;for(int n=0;n<drum::numMacros;++n){const auto& mapping=selectedControls.macros[(size_t)n];if(mapping.target>3)routing->settings.*(drum::macroTargets[(size_t)mapping.target].member)=mapping.minimum+(mapping.maximum-mapping.minimum)*processor.macro(n).get();}
    routing->returnActive.fill(false);auto routingState=processor.getLibrary().snapshot();if(const auto* kit=drum::activeKit(routingState)){bool solo=false;for(const auto& g:kit->groups)for(const auto& i:g.instruments)solo|=i.controls.solo;for(const auto& g:kit->groups)for(const auto& i:g.instruments){if(i.controls.mute || (solo && !i.controls.solo))continue;auto sound=i.controls.sound;for(int m=0;m<drum::numMacros;++m){const auto& map=i.controls.macros[(size_t)m];if(map.target>3)sound.*(drum::macroTargets[(size_t)map.target].member)=map.minimum+(map.maximum-map.minimum)*processor.macro(m).get();}const double levels[]{sound.sendA,sound.sendB,sound.sendC,sound.sendD,sound.sendE,sound.sendF};for(int n=0;n<6;++n)routing->returnActive[(size_t)n]|=levels[n]>drum::sendFloor;}for(int step=0;step<6;++step)for(int a=0;a<6;++a)if(routing->returnActive[(size_t)a])for(int b=0;b<6;++b)if(kit->returnRouting[(size_t)a].sends[(size_t)b]>drum::sendFloor)routing->returnActive[(size_t)b]=true;routing->audible=!selectedControls.mute && (!solo || selectedControls.solo);for(int n=0;n<6;++n){const auto& slots=kit->returns[(size_t)n].effects;int active=0;for(const auto& slot:slots)if(!slot.bypassed)++active;routing->descriptions[(size_t)n]=active?juce::String(active)+" FX":"Pass-through";}}
    routing->instrument=name;routing->hasChain=chainPresent;for(int n=0;n<22;++n){auto* bus=processor.getBus(false,drum::outputBusIndex(n));routing->enabled[(size_t)n]=bus && bus->isEnabled();}if(page==4)routing->repaint();
    syncEnvelopeOverlay();
}
void SoundEditor::syncEnvelopeOverlay()
{
    if(!waveform || !envelopes[0])return;
    for(int n=0;n<3;++n){auto& e=*envelopes[(size_t)n];e.setOverlay(waveform->plot().toFloat(),waveform->offset*waveform->timelineMs(),waveform->timelineMs()/waveform->zoom);e.setVisible(page==1 && selectedEnvelope==n && !waveform->sourceMode());if(e.isVisible())e.toFront(false);}
    ampOverlay.setToggleState(selectedEnvelope==0,juce::dontSendNotification);pitchOverlay.setToggleState(selectedEnvelope==1,juce::dontSendNotification);filterTab.setToggleState(selectedEnvelope==2,juce::dontSendNotification);
}
void SoundEditor::visibilityChanged() {if(waveform)refreshPreview();equalizer.visibilityChanged();}
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
    auto c=i->controls;
    const auto opposite=oppositeMarker(parameter.member);
    if(c.sound.reverse!=0 && opposite!=parameter.member)c.sound.*opposite=1-value;
    else c.sound.*(parameter.member)=value;
    int envelope=-1;
    for(int n=0;n<3;++n)if(std::find(envelopeMembers[n].begin(),envelopeMembers[n].end(),parameter.member)!=envelopeMembers[n].end()
        || (n==1 && parameter.member==&drum::SoundParameters::pitchAmount) || (n==2 && parameter.member==&drum::SoundParameters::filterAmount))envelope=n;
    if(envelope>=0)constrainEnvelope(c.sound,envelope,parameter.member);
    auto result=processor.getLibrary().setControls(kitId,instrumentId,c);
    if(result.ok && (parameter.member==&drum::SoundParameters::fadeIn || parameter.member==&drum::SoundParameters::fadeOut))waveform->showEdited();
    if(result.ok){if(envelope>=0)recordEnvelope(envelope,i->controls,c);processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
    message=result.ok?"Saved with this kit. Markers, direction, playback mode and loop on/off apply on the next hit.":"Start must be before End; loop start must be before loop end.";
    refresh();
}
void SoundEditor::changeLfo(drum::EnvelopeShape shape,bool reset)
{
    auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);if(!i)return;
    if(!lfoGesture || !lfoRecorded){lfoUndo.push_back({i->controls.sound.envelopes[3],i->controls.sound.lfoShape,i->controls.sound.lfoOn});if(lfoUndo.size()>100)lfoUndo.erase(lfoUndo.begin());lfoRedo.clear();lfoRecorded=true;}
    shape.points[0].ms=0;shape.points[(size_t)shape.count-1].ms=1000;
    auto c=i->controls;c.sound.envelopes[3]=shape;c.sound.lfoShape=6;c.sound.lfoOn=reset?0:1;
    if(processor.getLibrary().setControls(kitId,instrumentId,c).ok)processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));refresh();
}
void SoundEditor::undoLfo(bool redo)
{
    auto& from=redo?lfoRedo:lfoUndo;auto& to=redo?lfoUndo:lfoRedo;if(from.empty())return;
    auto state=processor.getLibrary().snapshot();auto* i=drum::findInstrument(*drum::activeKit(state),instrumentId);if(!i)return;auto c=i->controls;to.push_back({c.sound.envelopes[3],c.sound.lfoShape,c.sound.lfoOn});const auto previous=from.back();from.pop_back();c.sound.envelopes[3]=previous.shape;c.sound.lfoShape=previous.choice;c.sound.lfoOn=previous.on;
    if(processor.getLibrary().setControls(kitId,instrumentId,c).ok)processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));refresh();
}
void SoundEditor::changeKeyTuning()
{
    if(updating)return;auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* i=drum::findInstrument(*kit,instrumentId);if(!i || kit->id!=kitId)return;
    auto controls=i->controls;controls.tuning=keyCoarse.getValue();controls.sound.detune=keyFine.getValue();auto result=processor.getLibrary().setControls(kitId,instrumentId,controls);
    if(result.ok)processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));waveform->showEdited();message=result.ok?"Tuning saved; waveform and note spectrum update from the edited audio.":result.message;refresh();
}
void SoundEditor::setPage(int value)
{
    if(value==0)waveform->showSource();else if(page==0)waveform->showEdited();
    page=value; viewport.setViewPosition(0,0);
    for(int n=0;n<(int)tabs.size();++n) tabs[(size_t)n].setToggleState(n==page,juce::dontSendNotification);
    for(auto& field:fields)
    {
        bool numeric=false;for(auto& members:envelopeMembers)for(auto m:members)numeric|=field->parameter.member==m;
        const bool amount=field->parameter.member==&drum::SoundParameters::pitchAmount || field->parameter.member==&drum::SoundParameters::filterAmount;
        const auto member=field->parameter.member;
        const bool ampControl=member==&drum::SoundParameters::velocityVolume || std::find(envelopeMembers[0].begin(),envelopeMembers[0].end(),member)!=envelopeMembers[0].end();
        const bool pitchControl=member==&drum::SoundParameters::velocityPitch || member==&drum::SoundParameters::bendRange || std::find(envelopeMembers[1].begin(),envelopeMembers[1].end(),member)!=envelopeMembers[1].end();
        const bool selectedMode=page!=1 || member==&drum::SoundParameters::randomPan || (selectedEnvelope==0?ampControl:selectedEnvelope==1?pitchControl:field->parameter.page==2);
        field->setVisible(selectedMode && (field->parameter.page==page || (page==1 && field->parameter.page==2)) && field->parameter.member!=&drum::SoundParameters::start && field->parameter.member!=&drum::SoundParameters::end && (!numeric || adsrValues.getToggleState()) && !amount && page!=2);
    }
    for(auto& field:macroFields) field->setVisible(page==5);
    for(int n=0;n<3;++n)envelopes[(size_t)n]->setVisible(page==1 && n==selectedEnvelope);
    ampOverlay.setVisible(page==1);pitchOverlay.setVisible(page==1);
    adsrValues.setVisible(page==1);
    equalizer.setVisible(page==2);eqTab.setVisible(false);filterTab.setVisible(page==1);

    slice.setVisible(page==0);sliceCount.setVisible(page==0);reset.setVisible(page!=5 && page!=7);chain.setVisible(page==7);viewport.setVisible(page!=7 && page!=2);
    attackOnly.setVisible(page==6);
    keySpectrum->setVisible(page==8);keyCoarse.setVisible(page==8);keyFine.setVisible(page==8);keyCoarseLabel.setVisible(page==8);keyFineLabel.setVisible(page==8);keyStatus.setVisible(page==8);if(page==8)viewport.setVisible(false);
    lfoGraph->setVisible(page==3);routing->setVisible(page==4);waveform->setVisible(page!=2 && page!=8 && page!=4 && page!=5 && page!=3);waveformDivider->setVisible(page!=2 && page!=8 && page!=4 && page!=5 && page!=3);
    resized(); repaint();
}
void SoundEditor::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g,name,{18,12,getWidth()-524,30},22);
    static const char* help[]{
        "Choose Source to drag trim markers; Edited previews the sound. Scroll pans; Ctrl + scroll zooms X; Shift + scroll zooms Y.",
        "Click line: add point. Drag point: move. Drag line: curve. Double-click point: remove. Right-click: hold at S.",
        "16-band EQ. Select and enable bands below. Drag a point for frequency/gain; wheel adjusts Q.",
        "Turn LFO On and raise a depth to hear modulation and see it in Edited. Filter depth also needs Filter On.",
        "Sends A-F feed shared local effects, then Main. Edit A-F to add effects; set reverb/delay to 100% wet.",
        "Automate Macro 1-16 in your DAW. Mappings belong to each instrument; Min/Max allow inverted ranges. Later mappings take priority.",
        "Add Click, Pulse or Noise independently of the sample envelopes. Position delays the hit in ms; 0 starts with the note.",
        "Drag VST3 effects into this instrument\'s chain. Audio flows left to right. The waveform shows the sound before this chain.",
        "C through B, combined across octaves. Coarse = semitones; Fine = cents. The spectrum follows the edited sound."};
    text(g,help[page],{page==1?376:18,116,getWidth()-(page==2?470:page==1?528:page==6?270:36),24},12,muted);
    text(g,message.isEmpty()?"Non-destructive edits. The original audio file is preserved.":message,{18,getHeight()-29,getWidth()-36,23},11,muted);
}
void SoundEditor::resized()
{
    noteIndicator.setBounds(18,46,getWidth()-36,22);
    soloButton.setBounds(getWidth()-496,15,78,32);
    notesButton.setBounds(getWidth()-410,15,78,32);exportButton.setBounds(getWidth()-324,15,86,32);
    back.setBounds(getWidth()-230,15,125,32); reset.setBounds(getWidth()-95,15,80,32);
    const int tabWidth=(getWidth()-36-8*((int)tabs.size()-1))/(int)tabs.size();
    constexpr int order[]{0,1,2,6,3,8,7,4,5};
    for(int n=0;n<(int)tabs.size();++n) tabs[(size_t)order[n]].setBounds(18+n*(tabWidth+8),78,tabWidth,30);
    ampOverlay.setBounds(18,116,112,24);pitchOverlay.setBounds(136,116,112,24);
    adsrValues.setBounds(getWidth()-146,116,128,24);attackOnly.setBounds(getWidth()-238,116,220,24);
    eqTab.setBounds(getWidth()-446,116,126,24);filterTab.setBounds(254,116,112,24);
    int graphHeight=0;
    const int graphWidth=getWidth()-36;
    const int waveTop=150;
    const int reserved=(page==1 || page==0)?246:page==2?220:page==8?180:page==7?326:230;
    const int minimumWaveHeight=page==7?160:184;
    const int height=page==4?juce::jmax(250,juce::jmin(330,getHeight()-waveTop-195)):juce::jlimit(minimumWaveHeight,juce::jmax(minimumWaveHeight,getHeight()-waveTop-reserved-49-(graphHeight?graphHeight+8:0)),(page==0 || page==1 || page==2 || page==8)?getHeight():waveformHeight);
    lfoGraph->setBounds(18,waveTop,graphWidth,height);
    routing->setBounds(18,waveTop,graphWidth,height);
    waveform->setBounds(18,waveTop,juce::jmax(1,getWidth()-36),height);
    waveformDivider->setBounds(18,waveTop+height,juce::jmax(1,getWidth()-36),12);
    const int graphTop=waveTop+12+height;
    if(page==1){graphHeight=108;for(int n=0;n<3;++n)envelopes[(size_t)n]->setBounds(18,waveTop,graphWidth,height+120);}
    if(page==2)equalizer.setBounds(18,waveTop,graphWidth,height+equalizer.controlsHeight());
    const int controlsTop=graphTop+(graphHeight?graphHeight+8:0);
    if(page==8){keySpectrum->setBounds(18,waveTop,graphWidth,height);int row=controlsTop;int half=(graphWidth-16)/2;keyCoarseLabel.setBounds(18,row,half,20);keyCoarseLabel.setJustificationType(juce::Justification::centred);keyCoarse.setBounds(18+(half-104)/2,row+20,104,90);keyFineLabel.setBounds(34+half,row,half,20);keyFineLabel.setJustificationType(juce::Justification::centred);keyFine.setBounds(34+half+(half-104)/2,row+20,104,90);keyStatus.setBounds(18,row+114,graphWidth,26);}

    chain.setBounds(18,controlsTop,juce::jmax(1,getWidth()-36),juce::jmax(1,getHeight()-controlsTop-37));
    viewport.setBounds(18,page==5?waveTop:controlsTop,juce::jmax(1,getWidth()-36),juce::jmax(1,getHeight()-(page==5?waveTop:controlsTop)-37));
    const int width=juce::jmax(1,viewport.getWidth()-12),columns=juce::jmax(2,width/152),gap=page==1?10:16,cell=(width-gap*(columns-1))/columns;
    const int rowHeight=116;
    int top=page==1?2:6;
    int n=0; for(auto& field:fields) if(field->isVisible())
    {field->setBounds((n%columns)*(cell+gap),top+(n/columns)*rowHeight,cell,112); ++n;}
    int bottom=top+((n+columns-1)/columns)*rowHeight+(page==1?2:6);
    if(page==0) {sliceCount.setBounds((n%columns)*(cell+gap),top+(n/columns)*rowHeight,cell,30);++n;slice.setBounds((n%columns)*(cell+gap),top+(n/columns)*rowHeight,cell,30);++n;bottom=top+((n+columns-1)/columns)*rowHeight+6;}
    if(page==5)
    {
        const int cellWidth=(width-30)/2,macroRow=206;
        for(int index=0;index<drum::numMacros;++index) macroFields[(size_t)index]->setBounds((index%2)*(cellWidth+30),(index/2)*macroRow,cellWidth,macroRow-10);
        bottom=(drum::numMacros/2)*macroRow;
    }
    content.setSize(width,juce::jmax(viewport.getHeight(),bottom));
    syncEnvelopeOverlay();
}
void SoundEditor::timerCallback()
{
    processor.refreshMacroNames();
    if(!isVisible()) return;
    refreshPreview();
    refreshEnvelopes();syncEnvelopeOverlay();
    for(int n=0;n<drum::numMacros;++n)
    {
        if(!macroFields[(size_t)n]->name.hasKeyboardFocus(true))macroFields[(size_t)n]->name.setText(processor.macro(n).getName(64),false);
        auto& slider=macroFields[(size_t)n]->value;
        if(!slider.isMouseButtonDown() && !slider.hasKeyboardFocus(true)) slider.setValue(processor.macro(n).get()*100,juce::dontSendNotification);
    }
}
void SoundEditor::filesDropped(const juce::StringArray& files,int x,int y)
{
    if(chain.isInterestedInFileDrag(files)){setPage(7);auto point=chain.getLocalPoint(this,juce::Point<int>{x,y});chain.filesDropped(files,point.x,point.y);return;}
    if(files.size()!=1 || instrumentId.isEmpty() || !juce::File::isAbsolutePath(files[0])) return;
    processor.getSampleEngine().requestLoad(kitId,instrumentId,juce::File(files[0]));
    message="Loading replacement audio..."; repaint();
}
