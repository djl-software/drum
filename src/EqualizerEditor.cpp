#include "EqualizerEditor.h"
namespace
{
const juce::Colour bg(0xff10151f),ink(0xffe5ebf5),muted(0xff8d9bb0),mint(0xff67dfc5);
void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,juce::Colour c=muted)
{g.setColour(c);g.setFont(juce::FontOptions(11));g.drawFittedText(s,r,juce::Justification::centredLeft,1);}
}
EqualizerEditor::EqualizerEditor()
{
    setComponentID("eight-band-eq");setViewportIgnoreDragFlag(true);
    addAndMakeVisible(enabled);enabled.setComponentID("eq-enabled");enabled.setColour(juce::ToggleButton::textColourId,juce::Colours::white);enabled.onClick=[this]{equalizer.enabled=enabled.getToggleState();save();};
    for(int n=0;n<16;++n)
    {
        auto& button=bands[(size_t)n];addAndMakeVisible(button);button.setButtonText(juce::String(n+1));button.setComponentID("eq-band-"+juce::String(n));
        button.onClick=[this,n]{select(n);};
        auto& toggle=bandEnabled[(size_t)n];addAndMakeVisible(toggle);toggle.setComponentID("eq-enable-"+juce::String(n));toggle.setTooltip("Enable band "+juce::String(n+1));toggle.onClick=[this,n]{equalizer.bands[n].enabled=bandEnabled[n].getToggleState();if(equalizer.bands[n].enabled)equalizer.enabled=true;select(n);save();};button.setTooltip("Select band "+juce::String(n+1)+". Drag its dot to enable and shape it; double-click the dot to bypass.");
    }
    addAndMakeVisible(type);type.setComponentID("eq-type");type.addItem("Low cut",1);type.addItem("Low shelf",3);type.addItem("Bell",4);type.addItem("Notch",5);type.addItem("High shelf",6);type.addItem("High cut",7);
    type.onChange=[this]{if(!updating){auto& b=equalizer.bands[(size_t)selected];b.slope=drum::eqSlope(b);b.type=type.getSelectedId()-1;save();}};
    for(auto* s:{&frequency,&gain,&q,&output})
    {
        addAndMakeVisible(*s);useValueBar(*s);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,78,24);
        s->setColour(juce::Slider::trackColourId,mint.withAlpha(.3f));s->setColour(juce::Slider::thumbColourId,mint);s->setColour(juce::Slider::textBoxTextColourId,ink);
    }
    frequency.setComponentID("eq-frequency");frequency.setName("EQ frequency");frequency.setRange(20,20000,1);frequency.setSkewFactorFromMidPoint(1000);frequency.setTextValueSuffix(" Hz");
    gain.setComponentID("eq-gain");gain.setName("EQ gain");gain.setRange(-60,24,0.1);gain.setTextValueSuffix(" dB");
    q.setComponentID("eq-q");q.setName("EQ Q");q.setRange(0.1,18,0.00000001);q.setNumDecimalPlacesToDisplay(2);q.setSkewFactorFromMidPoint(1);q.setTextValueSuffix("");
    output.setComponentID("eq-output");output.setName("EQ output gain");output.setRange(-24,24,0.1);output.setTextValueSuffix(" dB");output.setTooltip("EQ output gain. Reduce after boosting to keep headroom.");
    frequency.onValueChange=[this]{if(!updating){equalizer.bands[(size_t)selected].frequency=frequency.getValue();save();}};
    gain.onValueChange=[this]{if(!updating){equalizer.bands[(size_t)selected].gain=gain.getValue();save();}};
    q.onValueChange=[this]{if(!updating){equalizer.bands[(size_t)selected].q=q.getValue();save();}};
    output.onValueChange=[this]{if(!updating){equalizer.output=output.getValue();save();}};
    addAndMakeVisible(slope);slope.setComponentID("eq-slope");for(int db=6;db<=72;db+=6)slope.addItem(juce::String(db)+" dB/oct",db);
    slope.onChange=[this]{if(!updating){equalizer.bands[selected].slope=slope.getSelectedId();save();}};
    addAndMakeVisible(reset);reset.setComponentID("eq-reset");reset.onClick=[this]{equalizer={};select(0);save();};
    const char* names[]{"Frequency","Gain","Q","Output","Shape","Slope"};for(int i=0;i<6;++i){addAndMakeVisible(labels[i]);labels[i].setText(names[i],juce::dontSendNotification);labels[i].setColour(juce::Label::textColourId,juce::Colours::white);labels[i].setFont(juce::FontOptions(14));labels[i].setJustificationType(juce::Justification::centredRight);}
    select(0);
}
void EqualizerEditor::update(const drum::Equalizer& value,double sampleRate)
{equalizer=value;rate=sampleRate>0?sampleRate:44100;select(selected);}
EqualizerEditor::~EqualizerEditor(){stopTimer();if(spectrum)spectrum->enabled.store(false);}
void EqualizerEditor::setSpectrum(drum::SpectrumTap* tap){if(spectrum)spectrum->enabled.store(false);spectrum=tap;if(tap)analysis.clear(*tap);visibilityChanged();}
void EqualizerEditor::visibilityChanged(){bool active=spectrum && isShowing();if(spectrum)spectrum->enabled.store(active);if(spectrum){if(!isTimerRunning())startTimerHz(15);}else stopTimer();}
void EqualizerEditor::timerCallback(){visibilityChanged();if(spectrum && isShowing() && analysis.update(*spectrum))repaint();}
void EqualizerEditor::select(int n)
{
    const drum::Equalizer defaults;frequency.setDoubleClickReturnValue(true,defaults.bands[(size_t)n].frequency);gain.setDoubleClickReturnValue(true,0);q.setDoubleClickReturnValue(true,0.70710678);output.setDoubleClickReturnValue(true,0);
    selected=n;updating=true;const auto& b=equalizer.bands[(size_t)n];
    enabled.setToggleState(equalizer.enabled,juce::dontSendNotification);
    for(int i=0;i<16;++i){bands[i].setToggleState(i==n,juce::dontSendNotification);bandEnabled[i].setToggleState(equalizer.bands[i].enabled,juce::dontSendNotification);}
    type.setSelectedId((b.type==1?0:b.type==7?6:b.type)+1,juce::dontSendNotification);slope.setSelectedId(drum::eqSlope(b),juce::dontSendNotification);slope.setEnabled(drum::eqIsCut(b.type));
    auto set=[](juce::Slider& s,double v){if(!s.isMouseButtonDown() && !s.hasKeyboardFocus(true))s.setValue(v,juce::dontSendNotification);};
    set(frequency,b.frequency);set(gain,b.gain);set(q,b.q);set(output,equalizer.output);gain.setEnabled(drum::eqHasGain(b.type));q.setEnabled(!drum::eqIsCut(b.type) || drum::eqSlope(b)>6);
    updating=false;repaint();
}
void EqualizerEditor::save(){select(selected);if(onChange)onChange(equalizer);}
juce::Rectangle<float> EqualizerEditor::plot() const {return {40,12,(float)getWidth()-56,(float)graphBounds().getHeight()-38};}
juce::Point<float> EqualizerEditor::point(int n) const
{
    const auto a=plot();const auto& b=equalizer.bands[(size_t)n];
    const double y=drum::eqHasGain(b.type)?b.gain:std::log2(b.q/0.70710678)*6;
    return {a.getX()+(float)(std::log(b.frequency/20)/std::log(1000.0))*a.getWidth(),a.getY()+(24-(float)std::clamp(y,-60.0,24.0))/84*a.getHeight()};
}
void EqualizerEditor::resized()
{
    const int top=graphBounds().getBottom()+6,w=(getWidth()-16)/16;
    for(int n=0;n<16;++n){bands[n].setBounds(8+n*w,top,w-4,24);bandEnabled[n].setBounds(8+n*w+(w-24)/2,top+26,24,20);}
    const int cell=(getWidth()-24)/4,row=top+52;
    auto bar=[&](int n,juce::Component& c,int column,int y){const int x=8+column*cell;labels[n].setBounds(x,y,86,26);c.setBounds(x+92,y,cell-100,26);};
    bar(0,frequency,0,row);bar(1,gain,1,row);bar(2,q,2,row);bar(3,output,3,row);
    bar(4,type,0,row+32);bar(5,slope,1,row+32);enabled.setBounds(48,18,94,26);reset.setBounds(getWidth()-106,row+32,96,26);

}
void EqualizerEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);const auto a=plot();
    if(spectrum){
        juce::Path fill;fill.startNewSubPath(a.getX(),a.getBottom());const double sampleRate=spectrum->rate.load();
        for(int x=0;x<=(int)a.getWidth();++x){double hz=20*std::pow(1000.0,x/a.getWidth());float level=-100;
            level=analysis.levelAt(hz,sampleRate);
            fill.lineTo(a.getX()+x,a.getBottom()-juce::jlimit(0.f,1.f,(level+60)/84)*a.getHeight());}
        fill.lineTo(a.getRight(),a.getBottom());fill.closeSubPath();juce::ColourGradient shade(juce::Colour(0xff4cb7d6),a.getX(),a.getY(),bg,a.getX(),a.getBottom(),false);g.setGradientFill(shade);g.fillPath(fill);
        label(g,"Output spectrum: -60 to +24 dBFS",{(int)a.getRight()-170,(int)a.getY()+4,166,14},muted);
    }
    for(double hz:{20.,50.,100.,200.,500.,1000.,2000.,5000.,10000.,20000.})
    {
        const auto x=a.getX()+(float)(std::log(hz/20)/std::log(1000.0))*a.getWidth();
        g.setColour(muted.withAlpha(0.2f));g.drawVerticalLine((int)x,a.getY(),a.getBottom());
        label(g,hz>=1000?juce::String(hz/1000,0)+"k":juce::String(hz,0),{(int)x-12,(int)a.getBottom()+2,34,16});
    }
    for(int db=-60;db<=24;db+=12)
    {
        const int y=(int)(a.getY()+(24-db)/84.0f*a.getHeight());g.setColour(muted.withAlpha(db==0?0.4f:0.15f));g.drawHorizontalLine(y,a.getX(),a.getRight());
        label(g,juce::String(db),{3,y-8,30,16});
    }
    juce::Path curve;
    for(int x=0;x<=(int)a.getWidth();++x)
    {
        const double hz=20*std::pow(1000.0,x/a.getWidth()),db=20*std::log10(std::max(1e-9,drum::eqMagnitude(equalizer,hz,rate)));
        const float y=a.getY()+(24-(float)std::clamp(db,-60.0,24.0))/84*a.getHeight();
        if(x==0)curve.startNewSubPath(a.getX(),y);else curve.lineTo(a.getX()+x,y);
    }
    g.setColour(equalizer.enabled?juce::Colour(0xfff1b95b):muted);g.strokePath(curve,juce::PathStrokeType(2));
    for(int n=0;n<16;++n)
    {
        if(!equalizer.bands[n].enabled)continue;
        const auto p=point(n);static const juce::uint32 colours[]={0xfff28b82,0xffffbd69,0xfff4dc72,0xff8cda83,0xff59d9cd,0xff68b6ff,0xffb49bff,0xffed91d1};
        g.setColour(juce::Colour(colours[n%8]).withAlpha(equalizer.enabled && equalizer.bands[(size_t)n].enabled?1.f:.55f));
        if(n==selected)g.drawEllipse(p.x-10,p.y-10,20,20,1);
        g.fillEllipse(p.x-7,p.y-7,14,14);g.setColour(bg);g.setFont(juce::FontOptions(10));g.drawText(juce::String(n+1),juce::Rectangle<int>(14,14).withCentre(p.roundToInt()),juce::Justification::centred);
    }

}
void EqualizerEditor::mouseDown(const juce::MouseEvent& e)
{
    if(!plot().expanded(12).contains(e.position))return;
    float distance=18;int n=-1;for(int i=0;i<16;++i){if(!equalizer.bands[i].enabled)continue;const auto d=point(i).getDistanceFrom(e.position);if(d<distance){distance=d;n=i;}}
    if(n<0)n=selected;
    select(n);down=e.position;initialQ=equalizer.bands[(size_t)n].q;
}
void EqualizerEditor::mouseDrag(const juce::MouseEvent& e)
{
    if(!plot().expanded(12).contains(e.mouseDownPosition))return;
    const auto a=plot();auto& b=equalizer.bands[(size_t)selected];b.enabled=true;equalizer.enabled=true;
    if(e.mods.isAltDown())b.q=std::clamp(initialQ*std::pow(2.0,(down.y-e.position.y)/40.0),0.1,18.0);
    else
    {
        b.frequency=std::clamp(20*std::pow(1000.0,(e.position.x-a.getX())/a.getWidth()),20.0,20000.0);
        const double value=std::clamp(24.0-(e.position.y-a.getY())/a.getHeight()*84.0,-60.0,24.0);
        if(drum::eqHasGain(b.type))b.gain=value;else b.q=std::clamp(0.70710678*std::pow(2.0,value/6),0.1,18.0);
    }
    save();
}
void EqualizerEditor::mouseDoubleClick(const juce::MouseEvent& e)
{if(plot().expanded(12).contains(e.position)){mouseDown(e);auto& b=equalizer.bands[(size_t)selected];b.enabled=!b.enabled;if(b.enabled)equalizer.enabled=true;save();}}
void EqualizerEditor::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)
{if(plot().contains(e.position)){auto& b=equalizer.bands[(size_t)selected];b.q=std::clamp(b.q*std::pow(2.0,w.deltaY*3),0.1,18.0);save();}}
