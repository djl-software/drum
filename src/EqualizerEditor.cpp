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
    addAndMakeVisible(enabled);enabled.setComponentID("eq-enabled");enabled.onClick=[this]{equalizer.enabled=enabled.getToggleState();save();};
    addAndMakeVisible(bandOn);bandOn.setComponentID("eq-band-enabled");bandOn.onClick=[this]{equalizer.bands[(size_t)selected].enabled=bandOn.getToggleState();if(bandOn.getToggleState())equalizer.enabled=true;save();};
    for(int n=0;n<8;++n)
    {
        auto& button=bands[(size_t)n];addAndMakeVisible(button);button.setButtonText(juce::String(n+1));button.setComponentID("eq-band-"+juce::String(n));
        button.onClick=[this,n]{select(n);};button.setTooltip("Select band "+juce::String(n+1)+". Drag its dot to enable and shape it; double-click the dot to bypass.");
    }
    addAndMakeVisible(type);type.setComponentID("eq-type");type.addItemList({"Low cut 12","Low cut 48","Low shelf","Bell","Notch","High shelf","High cut 12","High cut 48"},1);
    type.onChange=[this]{if(!updating){equalizer.bands[(size_t)selected].type=type.getSelectedId()-1;save();}};
    for(auto* s:{&frequency,&gain,&q,&output})
    {
        addAndMakeVisible(*s);useValueBar(*s);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,78,24);
        s->setColour(juce::Slider::trackColourId,mint.withAlpha(.3f));s->setColour(juce::Slider::thumbColourId,mint);s->setColour(juce::Slider::textBoxTextColourId,ink);
    }
    frequency.setComponentID("eq-frequency");frequency.setName("EQ frequency");frequency.setRange(20,20000,1);frequency.setSkewFactorFromMidPoint(1000);frequency.setTextValueSuffix(" Hz");
    gain.setComponentID("eq-gain");gain.setName("EQ gain");gain.setRange(-60,24,0.1);gain.setTextValueSuffix(" dB");
    q.setComponentID("eq-q");q.setName("EQ Q");q.setRange(0.1,18,0.00000001);q.setNumDecimalPlacesToDisplay(2);q.setSkewFactorFromMidPoint(1);q.setTextValueSuffix(" Q");
    output.setComponentID("eq-output");output.setName("EQ output gain");output.setRange(-24,24,0.1);output.setTextValueSuffix(" dB out");output.setTooltip("EQ output gain. Reduce after boosting to keep headroom.");
    frequency.onValueChange=[this]{if(!updating){equalizer.bands[(size_t)selected].frequency=frequency.getValue();save();}};
    gain.onValueChange=[this]{if(!updating){equalizer.bands[(size_t)selected].gain=gain.getValue();save();}};
    q.onValueChange=[this]{if(!updating){equalizer.bands[(size_t)selected].q=q.getValue();save();}};
    output.onValueChange=[this]{if(!updating){equalizer.output=output.getValue();save();}};
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
    enabled.setToggleState(equalizer.enabled,juce::dontSendNotification);bandOn.setToggleState(b.enabled,juce::dontSendNotification);
    for(int i=0;i<8;++i)bands[(size_t)i].setToggleState(i==n,juce::dontSendNotification);
    type.setSelectedId(b.type+1,juce::dontSendNotification);
    auto set=[](juce::Slider& s,double v){if(!s.isMouseButtonDown() && !s.hasKeyboardFocus(true))s.setValue(v,juce::dontSendNotification);};
    set(frequency,b.frequency);set(gain,b.gain);set(q,b.q);set(output,equalizer.output);gain.setEnabled(drum::eqHasGain(b.type));
    updating=false;repaint();
}
void EqualizerEditor::save(){select(selected);if(onChange)onChange(equalizer);}
juce::Rectangle<float> EqualizerEditor::plot() const {return {38,35,(float)getWidth()-52,(float)getHeight()-88};}
juce::Point<float> EqualizerEditor::point(int n) const
{
    const auto a=plot();const auto& b=equalizer.bands[(size_t)n];
    const double y=drum::eqHasGain(b.type)?b.gain:std::log2(b.q/0.70710678)*6;
    return {a.getX()+(float)(std::log(b.frequency/20)/std::log(1000.0))*a.getWidth(),a.getY()+(24-(float)std::clamp(y,-60.0,24.0))/84*a.getHeight()};
}
void EqualizerEditor::resized()
{
    enabled.setBounds(8,3,84,26);for(int n=0;n<8;++n)bands[(size_t)n].setBounds(100+n*34,4,30,24);
    output.setBounds(getWidth()-242,3,232,26);
    bandOn.setBounds(8,getHeight()-28,85,24);type.setBounds(96,getHeight()-28,140,24);
    const int width=(getWidth()-254)/3;frequency.setBounds(244,getHeight()-28,width,24);gain.setBounds(248+width,getHeight()-28,width,24);q.setBounds(252+width*2,getHeight()-28,width,24);
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
    for(int n=0;n<8;++n)
    {
        const auto p=point(n);static const juce::uint32 colours[]={0xfff28b82,0xffffbd69,0xfff4dc72,0xff8cda83,0xff59d9cd,0xff68b6ff,0xffb49bff,0xffed91d1};
        g.setColour(juce::Colour(colours[n]).withAlpha(equalizer.enabled && equalizer.bands[(size_t)n].enabled?1.f:.55f));
        if(n==selected)g.drawEllipse(p.x-10,p.y-10,20,20,1);
        g.fillEllipse(p.x-7,p.y-7,14,14);g.setColour(bg);g.setFont(juce::FontOptions(10));g.drawText(juce::String(n+1),juce::Rectangle<int>(14,14).withCentre(p.roundToInt()),juce::Justification::centred);
    }
    label(g,"Drag: frequency / gain (cuts: Q). Alt-drag or wheel: Q.",{390,7,std::max(1,getWidth()-645),18});
}
void EqualizerEditor::mouseDown(const juce::MouseEvent& e)
{
    if(!plot().expanded(12).contains(e.position))return;
    float distance=100000;int n=0;for(int i=0;i<8;++i){const auto d=point(i).getDistanceFrom(e.position);if(d<distance){distance=d;n=i;}}
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
