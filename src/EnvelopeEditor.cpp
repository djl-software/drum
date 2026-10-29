#include "EnvelopeEditor.h"

namespace
{
const juce::Colour background(0xff10151f),ink(0xffe5ebf5),muted(0xff8d9bb0),grid(0xff2b3648);
constexpr double heldMilliseconds=250;
juce::String timeText(double ms) {return ms>=1000?juce::String(ms/1000,2)+" s":juce::String(ms,1)+" ms";}
void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> area,float size,juce::Colour c)
{g.setColour(c);g.setFont(juce::FontOptions(size));g.drawFittedText(s,area,juce::Justification::centredLeft,1);}
}
class EnvelopeEditor::Handle final : public juce::Component,public juce::SettableTooltipClient
{
public:
    Handle(EnvelopeEditor& e,int n):owner(e),index(n)
    {
        static const char* names[]{"attack","decay","sustain","release"};
        setComponentID(e.getComponentID()+"-"+names[n]);setName(juce::String(names[n])+" envelope point");
        setWantsKeyboardFocus(true);setViewportIgnoreDragFlag(true);
        setTooltip(n==0?"Drag horizontally: attack time. Arrow keys: 1 ms; Shift: 0.1 ms.":n==1?"Drag horizontally: decay time; vertically: sustain level.":n==2?"Drag vertically: sustain level. Sustain lasts until note-off.":"Drag horizontally: release time after note-off.");
    }
    void paint(juce::Graphics& g) override
    {
        auto area=getLocalBounds().toFloat().reduced(1);
        g.setColour(owner.active?owner.colour:muted);g.fillEllipse(area);
        g.setColour(background);g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawText(juce::String::charToString("ADSR"[index]),getLocalBounds(),juce::Justification::centred);
        if(hasKeyboardFocus(false)) {g.setColour(ink);g.drawEllipse(area,2);}
    }
    void mouseDown(const juce::MouseEvent& e) override
    {if(isShowing())grabKeyboardFocus();owner.begin(index,e.getEventRelativeTo(&owner).position);}
    void mouseDrag(const juce::MouseEvent& e) override {owner.drag(e.getEventRelativeTo(&owner).position);}
    void mouseUp(const juce::MouseEvent&) override {owner.finish();}
    void focusGained(FocusChangeType) override {repaint();}
    void focusLost(FocusChangeType) override {repaint();}
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(!owner.editable(index))return false;
        auto v=owner.values;const auto code=key.getKeyCode();const double timeStep=key.getModifiers().isShiftDown()?0.1:1;
        if((code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey) && index!=2)
        {
            const double step=code==juce::KeyPress::leftKey?-timeStep:timeStep;
            if(index==0)v.attack+=step;else if(index==1)v.decay+=step;else v.release+=step;
        }
        else if((code==juce::KeyPress::upKey || code==juce::KeyPress::downKey) && (index==1 || index==2))
            v.sustain+=(code==juce::KeyPress::upKey?1:-1)*(owner.amount<0?-1:1)*(key.getModifiers().isShiftDown()?0.001:0.01);
        else return false;
        owner.edit(index,v);return true;
    }
private:
    EnvelopeEditor& owner;
    int index;
};
EnvelopeEditor::EnvelopeEditor(juce::String id,juce::String heading,juce::Colour c):title(std::move(heading)),colour(c)
{
    setComponentID(std::move(id));setName(title+" envelope");setViewportIgnoreDragFlag(true);
    for(int n=0;n<4;++n) {handles[(size_t)n]=std::make_unique<Handle>(*this,n);addAndMakeVisible(*handles[(size_t)n]);}
    for(auto* b:{&zoomOut,&zoomIn,&fitButton,&activate})addAndMakeVisible(*b);
    zoomOut.setComponentID(getComponentID()+"-zoom-out");zoomIn.setComponentID(getComponentID()+"-zoom-in");fitButton.setComponentID(getComponentID()+"-fit");activate.setComponentID(getComponentID()+"-activate");
    zoomOut.setTooltip("Show more envelope time");zoomIn.setTooltip("Inspect shorter envelope times");fitButton.setTooltip("Fit the full envelope; keep fitting when edits finish");
    zoomOut.onClick=[this]{finish();automatic=false;span=juce::jmin(80000.0,span*2);layoutHandles();repaint();};
    zoomIn.onClick=[this]{finish();automatic=false;span=juce::jmax(10.0,span/2);layoutHandles();repaint();};
    fitButton.onClick=[this]{finish();automatic=true;fit();layoutHandles();repaint();};
    activate.onClick=[this]{if(onActivate)onActivate();};activate.setVisible(false);
}
EnvelopeEditor::~EnvelopeEditor()=default;
juce::Rectangle<float> EnvelopeEditor::plot() const {return {35,56,juce::jmax(1.0f,(float)getWidth()-55),juce::jmax(1.0f,(float)getHeight()-96)};}
std::array<juce::Point<float>,5> EnvelopeEditor::points() const
{
    const auto area=plot();const bool down=amount<0;
    auto point=[&](double t,double level){return juce::Point<float>{area.getX()+(float)(t/span)*area.getWidth(),down?area.getY()+(float)level*area.getHeight():area.getBottom()-(float)level*area.getHeight()};};
    return {point(0,0),point(values.attack,1),point(values.attack+values.decay,values.sustain),
            point(values.attack+values.decay+heldMilliseconds,values.sustain),point(values.attack+values.decay+heldMilliseconds+values.release,0)};
}
void EnvelopeEditor::fit() {span=juce::jlimit(100.0,80000.0,(values.attack+values.decay+heldMilliseconds+values.release)*1.1);}
bool EnvelopeEditor::editable(int n) const {return isEnabled() && !mapped[(size_t)n];}
void EnvelopeEditor::update(Values v,const std::array<bool,4>& m,double a,juce::String u,bool enabled,juce::String description,bool newSelection)
{
    if(newSelection || (dragging>=0 && m[(size_t)dragging]))dragging=-1;
    const bool changed=v!=values || m!=mapped || a!=amount || enabled!=active || u!=unit || description!=status || newSelection;
    values=v;mapped=m;amount=a;unit=std::move(u);active=enabled;status=std::move(description);
    if(newSelection)automatic=true;
    if(automatic && dragging<0)fit();
    if(changed) {layoutHandles();repaint();}
}
void EnvelopeEditor::setActivation(const juce::String& s)
{if(activate.getButtonText()!=s || activate.isVisible()!=s.isNotEmpty()) {activate.setButtonText(s);activate.setVisible(s.isNotEmpty());resized();}}
void EnvelopeEditor::resized()
{
    zoomOut.setBounds(getWidth()-106,7,25,22);zoomIn.setBounds(getWidth()-78,7,25,22);fitButton.setBounds(getWidth()-50,7,40,22);
    activate.setBounds(getWidth()-208,7,94,22);layoutHandles();
}
void EnvelopeEditor::layoutHandles()
{
    const auto p=points();const auto area=plot();
    std::array<juce::Point<float>,4> placed;
    for(int n=0;n<4;++n)
    {
        auto centre=p[(size_t)n+1];const bool visible=centre.x<=area.getRight()+0.1f;
        // Distinct grips keep zero-time or zero-level stages reachable without
        // moving the curve itself; thin connectors indicate displaced grips.
        for(int previous=0;previous<n;++previous)if(centre.getDistanceFrom(placed[(size_t)previous])<20)centre.y+=centre.y<area.getCentreY()?21:-21;
        placed[(size_t)n]=centre;auto& h=*handles[(size_t)n];h.setVisible(visible);h.setEnabled(editable(n));h.setAlpha(mapped[(size_t)n]?0.35f:1);
        h.setBounds(juce::Rectangle<int>(18,18).withCentre(centre.roundToInt()));
        h.setMouseCursor(mapped[(size_t)n]?juce::MouseCursor::NormalCursor:juce::MouseCursor::DraggingHandCursor);
    }
}
void EnvelopeEditor::paint(juce::Graphics& g)
{
    g.setColour(background);g.fillRoundedRectangle(getLocalBounds().toFloat(),7);
    label(g,title,{10,6,getWidth()-(activate.isVisible()?226:124),24},13,ink);
    label(g,status,{10,31,getWidth()-20,18},11,active?colour:muted);
    const auto area=plot();const auto p=points();const auto c=active?colour:muted;
    for(int n=0;n<5;++n)
    {
        const float x=area.getX()+area.getWidth()*n/4;g.setColour(grid);g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());
        label(g,n==0?"0":timeText(span*n/4),{(int)x-(n==4?53:0),(int)area.getBottom()+9,58,15},9,muted);
    }
    g.setColour(grid);g.drawHorizontalLine((int)area.getY(),area.getX(),area.getRight());g.drawHorizontalLine((int)area.getBottom(),area.getX(),area.getRight());
    label(g,amount<0?"0":unit=="%" || amount==0?"100%":juce::String(amount,1),{2,49,32,15},9,muted);
    label(g,amount<0?juce::String(amount,1):"0",{2,(int)area.getBottom()-7,32,15},9,muted);
    {
        juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(area.expanded(1).toNearestInt());
        juce::Path line;line.startNewSubPath(p[0]);for(size_t n=1;n<p.size();++n)line.lineTo(p[n]);
        auto fill=line;fill.lineTo(p[4].x,p[0].y);fill.closeSubPath();g.setColour(c.withAlpha(0.08f));g.fillPath(fill);
        g.setColour(c);g.strokePath(line,juce::PathStrokeType(2));
        if(p[3].x<area.getRight())
        {
            float dash[]{3,3};g.setColour(muted.withAlpha(0.65f));g.drawDashedLine({{p[3].x,area.getY()},{p[3].x,area.getBottom()}},dash,2);
            label(g,"Note off",{(int)p[3].x+5,(int)area.getY()+3,62,15},9,muted);
        }
    }
    for(int n=0;n<4;++n)if(handles[(size_t)n]->isVisible())
    {g.setColour(c.withAlpha(0.55f));g.drawLine({p[(size_t)n+1],handles[(size_t)n]->getBounds().getCentre().toFloat()});}
    label(g,"A "+timeText(values.attack)+"   D "+timeText(values.decay)+"   S "+juce::String(values.sustain*100,1)+"%   R "+timeText(values.release),{10,getHeight()-19,getWidth()-20,17},10,ink);
}
void EnvelopeEditor::begin(int index,juce::Point<float> position)
{if(editable(index)) {dragging=index;initial=values;dragStart=position;}}
void EnvelopeEditor::drag(juce::Point<float> position)
{
    if(dragging<0 || !editable(dragging))return;
    auto v=initial;const auto delta=position-dragStart;const double ms=delta.x/plot().getWidth()*span;
    if(dragging==0)v.attack+=ms;else if(dragging==1)v.decay+=ms;else if(dragging==3)v.release+=ms;
    if(dragging==1 || dragging==2)v.sustain+=(amount<0?1:-1)*delta.y/plot().getHeight();
    edit(dragging,v);
}
void EnvelopeEditor::edit(int index,Values v)
{
    v.attack=std::round(juce::jlimit(0.0,20000.0,v.attack)*10)/10;v.decay=std::round(juce::jlimit(0.0,20000.0,v.decay)*10)/10;
    v.release=std::round(juce::jlimit(0.0,30000.0,v.release)*10)/10;v.sustain=std::round(juce::jlimit(0.0,1.0,v.sustain)*1000)/1000;
    if(onChange && v!=values)onChange(index,v);
}
void EnvelopeEditor::finish() {dragging=-1;if(automatic)fit();layoutHandles();repaint();}
void EnvelopeEditor::visibilityChanged() {if(!isVisible())finish();}
void EnvelopeEditor::mouseDown(const juce::MouseEvent& e)
{
    if(!plot().expanded(8).contains(e.position))return;
    const auto p=points();float best=9;int index=-1;
    for(int n=0;n<4;++n)
    {
        juce::Point<float> nearest;
        const auto distance=juce::Line<float>(p[(size_t)n],p[(size_t)n+1]).getDistanceFromPoint(e.position,nearest);
        if(editable(n) && distance<best) {best=distance;index=n;}
    }
    if(index>=0)begin(index,e.position);
}
void EnvelopeEditor::mouseDrag(const juce::MouseEvent& e) {drag(e.position);}
void EnvelopeEditor::mouseUp(const juce::MouseEvent&) {finish();}
