#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

inline bool isDjlRotary(const juce::Slider& slider)
{
    const auto style=slider.getSliderStyle();
    return style==juce::Slider::Rotary || style==juce::Slider::RotaryHorizontalDrag
        || style==juce::Slider::RotaryVerticalDrag || style==juce::Slider::RotaryHorizontalVerticalDrag;
}

inline void configureDjlRotary(juce::Slider& slider,bool bipolar=false)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,96,23);
    slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
    slider.setMouseDragSensitivity(180);
    slider.setSliderSnapsToMousePosition(false);
    slider.setViewportIgnoreDragFlag(true);
    slider.getProperties().set("bipolar",bipolar);
}

inline void drawDjlRotary(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider& slider)
{
    const juce::Colour border(0xff2b3648),accent(0xff67dfc5),text(0xffe5ebf5);
    const auto area=juce::Rectangle<float>((float)x,(float)y,(float)width,(float)height).reduced(4);
    const auto centre=area.getCentre();const float radius=juce::jmin(area.getWidth(),area.getHeight())*.5f;
    if(radius<2)return;
    const float alpha=slider.isEnabled()?1.f:.35f;
    const float zero=bool(slider.getProperties()["bipolar"])?start+(float)slider.valueToProportionOfLength(0)*(end-start):start;
    const float position=start+value*(end-start);
    juce::Path track,level;track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
    level.addCentredArc(centre.x,centre.y,radius,radius,0,juce::jmin(zero,position),juce::jmax(zero,position),true);
    g.setColour(border.withAlpha(alpha));g.strokePath(track,juce::PathStrokeType(2.5f));
    g.setColour(accent.withAlpha(alpha));g.strokePath(level,juce::PathStrokeType(2.5f));
    const float dial=radius-4;const juce::Rectangle<float> body(centre.x-dial,centre.y-dial,dial*2,dial*2);
    g.setColour(juce::Colours::black.withAlpha(.55f*alpha));g.fillEllipse(body.translated(0,3).expanded(1));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffc0cbd5).withAlpha(alpha),body.getX(),body.getY(),juce::Colour(0xff1b2530).withAlpha(alpha),body.getRight(),body.getBottom(),false));g.fillEllipse(body);
    g.setColour(juce::Colour(0xff080e15).withAlpha(alpha));g.drawEllipse(body,1);
    const auto face=body.reduced(3);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff6c7b8b).withAlpha(alpha),face.getX(),face.getY(),juce::Colour(0xff263341).withAlpha(alpha),face.getRight(),face.getBottom(),false));g.fillEllipse(face);
    g.setColour(juce::Colours::white.withAlpha(.18f*alpha));g.drawEllipse(face.reduced(.5f),.8f);
    for(int n=0;n<24;++n){float angle=position+n*juce::MathConstants<float>::twoPi/24;auto direction=juce::Point<float>(std::sin(angle),-std::cos(angle));g.setColour(juce::Colour(0xff101923).withAlpha(.6f*alpha));g.drawLine({centre+direction*(dial-2.7f),centre+direction*(dial-.9f)},.8f);}
    const auto direction=juce::Point<float>(std::sin(position),-std::cos(position));const auto tip=centre+direction*(dial-5),tail=centre+direction*juce::jmax(1.f,dial*.2f);
    g.setColour(juce::Colours::black.withAlpha(.75f*alpha));g.drawLine({tail.translated(1,1),tip.translated(1,1)},3.3f);
    g.setColour(text.withAlpha(alpha));g.drawLine({tail,tip},2.1f);

}

// JUCE's native peers use different scales for a physical wheel detent.
inline double djlDetentedWheelDelta() noexcept
{
#if JUCE_WINDOWS
    return 60.0/256.0; // WM_MOUSEWHEEL: 120 * .5 / 256.
#elif JUCE_LINUX || JUCE_BSD
    return 50.0/256.0; // X11 wheel-button event.
#elif JUCE_MAC
    return 10.0/256.0;
#else
    return .1;
#endif
}

// Use this base (or port its wheel logic) for DJL pots. Linear controls keep the
// host framework's scrolling behavior. Wheel movement is in normalized space,
// so logarithmic ranges retain their shape and declared parameter steps.
class DjlAdaptiveSlider : public juce::Slider
{
public:
    void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) override
    {
        if(!isDjlRotary(*this) || !isEnabled() || !isScrollWheelEnabled()){
            juce::Slider::mouseWheelMove(event,wheel);return;
        }
        if(event.mods.isAnyMouseButtonDown() || wheel.isInertial || getMaximum()<=getMinimum())return;
        const double delta=(std::abs(wheel.deltaY)>=std::abs(wheel.deltaX)?wheel.deltaY:-wheel.deltaX)*(wheel.isReversed?-1:1);
        if(delta==0 || !std::isfinite(delta))return;
        const auto time=event.eventTime.toMilliseconds();
        if(lastTime==time)return; // Ignore duplicate native wheel packets.
        hideTextBox(false);
        const int direction=delta>0?1:-1;
        double units=std::abs(delta)/(wheel.isSmooth?.1:djlDetentedWheelDelta());
#if JUCE_MAC
        if(!wheel.isSmooth)units=juce::jmin(1.0,units); // Cap accelerated detents, preserve fractional native packets.
#endif
        const double seconds=(time-lastTime)*.001;
        const bool restart=lastTime<0 || seconds<=0 || seconds>.4 || direction!=lastDirection || getValue()!=lastApplied;
        if(restart){pending=valueToProportionOfLength(getValue());speed=units/.3;}
        else {const double instant=units/juce::jmax(.008,seconds);speed=instant<=5?instant:.65*speed+.35*instant;}
        lastTime=time;lastDirection=direction;
        const double denomination=juce::jmax(getInterval(),std::pow(10.,-getNumDecimalPlacesToDisplay()));
        const double neighbour=getValue()+direction*denomination,opposite=getValue()-direction*denomination;
        const double fine=std::abs(valueToProportionOfLength(juce::jlimit(getMinimum(),getMaximum(),neighbour>=getMinimum() && neighbour<=getMaximum()?neighbour:opposite))-valueToProportionOfLength(getValue()));
        const double acceleration=juce::jlimit(0.,1.,(speed-5.)/35.);
        const double step=fine+(juce::jmax(fine,.02)-fine)*acceleration*acceleration;
        const double change=direction*juce::jmin(juce::jmax(.1,fine),units*step)*(event.mods.isShiftDown()?.1:1.);
        pending=juce::jlimit(0.,1.,pending+change);
        const double desired=proportionOfLengthToValue(pending);
        // Preserve fractional trackpad input until it reaches a native step.
        const double snapped=getNormalisableRange().snapToLegalValue(desired);
        if(snapped!=getValue()){
            juce::Slider::ScopedDragNotification gesture(*this);
            setValue(snapped,juce::sendNotificationSync);
        }
        lastApplied=getValue();
    }
private:
    juce::int64 lastTime=-1;
    int lastDirection=0;
    double speed=0,pending=0,lastApplied=0;
};
