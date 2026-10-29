#include "EnvelopeEditor.h"
namespace
{
const juce::Colour background(0xff10151f),ink(0xffe5ebf5),muted(0xff8d9bb0),grid(0xff2b3648);
juce::String timeText(double ms) {return ms>=1000?juce::String(ms/1000,2)+" s":juce::String(ms,1)+" ms";}
void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> area,float size,juce::Colour c)
{g.setColour(c);g.setFont(juce::FontOptions(size));g.drawFittedText(s,area,juce::Justification::centredLeft,1);}
}
class EnvelopeEditor::Handle final : public juce::Component,public juce::SettableTooltipClient
{
public:
    Handle(EnvelopeEditor& e,int n):owner(e),index(n)
    {
        setComponentID(e.getComponentID()+"-point-"+juce::String(n));setName("Envelope point "+juce::String(n+1));
        setWantsKeyboardFocus(true);setViewportIgnoreDragFlag(true);
        setTooltip("Drag freely, including past neighbouring points. Double-click to remove. Right-click to hold here until note-off. Arrow keys adjust; Shift gives fine steps.");
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(owner.active?owner.colour:muted);g.fillEllipse(getLocalBounds().toFloat().reduced(1));
        g.setColour(background);g.setFont(juce::FontOptions(9.0f));
        g.drawText(index==owner.shape.hold?"S":juce::String(index+1),getLocalBounds(),juce::Justification::centred);
        if(hasKeyboardFocus(false)){g.setColour(ink);g.drawEllipse(getLocalBounds().toFloat().reduced(1),2);}
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(e.mods.isMiddleButtonDown()){owner.mouseDown(e.getEventRelativeTo(&owner));return;}
        if(e.mods.isPopupMenu()) {owner.holdPoint(index);return;}
        if(isShowing())grabKeyboardFocus();owner.beginPoint(index,e.getEventRelativeTo(&owner).position);
    }
    void mouseDrag(const juce::MouseEvent& e) override {owner.mouseDrag(e.getEventRelativeTo(&owner));}
    void mouseUp(const juce::MouseEvent&) override {owner.finish();}
    void mouseDoubleClick(const juce::MouseEvent&) override {owner.removePoint(index);}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) override {owner.mouseWheelMove(e.getEventRelativeTo(&owner),w);}
    void focusGained(FocusChangeType) override {repaint();}
    void focusLost(FocusChangeType) override {repaint();}
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(!owner.editable())return false;
        if(key.getKeyCode()==juce::KeyPress::deleteKey || key.getKeyCode()==juce::KeyPress::backspaceKey){owner.removePoint(index);return true;}
        auto edited=owner.shape;auto& p=edited.points[(size_t)index];const bool fine=key.getModifiers().isShiftDown();const int code=key.getKeyCode();
        if(code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey)p.ms+=(code==juce::KeyPress::leftKey?-1:1)*(fine?0.1:1);
        else if(code==juce::KeyPress::upKey || code==juce::KeyPress::downKey)p.level+=(code==juce::KeyPress::upKey?1:-1)*(owner.amount<0?-1:1)*(fine?0.001:0.01);
        else return false;
        p.ms=std::clamp(p.ms,std::min(owner.limit,index?edited.points[(size_t)index-1].ms:0.0),std::min(owner.limit,index+1<edited.count?edited.points[(size_t)index+1].ms:owner.limit));
        p.level=std::clamp(p.level,0.0,1.0);owner.submit(edited);return true;
    }
private:
    EnvelopeEditor& owner;int index;
};
EnvelopeEditor::EnvelopeEditor(juce::String id,juce::String heading,juce::Colour c):title(std::move(heading)),colour(c)
{
    setComponentID(std::move(id));setName(title+" envelope");setViewportIgnoreDragFlag(true);
    for(int n=0;n<drum::maxEnvelopePoints;++n){handles[(size_t)n]=std::make_unique<Handle>(*this,n);addAndMakeVisible(*handles[(size_t)n]);}
    for(auto* b:{&zoomOut,&zoomIn,&fitButton,&activate,&adsr,&reset,&undo,&redo})addAndMakeVisible(*b);
    zoomOut.setComponentID(getComponentID()+"-zoom-out");zoomIn.setComponentID(getComponentID()+"-zoom-in");fitButton.setComponentID(getComponentID()+"-fit");activate.setComponentID(getComponentID()+"-activate");adsr.setComponentID(getComponentID()+"-adsr");
    reset.setComponentID(getComponentID()+"-reset");undo.setComponentID(getComponentID()+"-undo");redo.setComponentID(getComponentID()+"-redo");
    reset.setTooltip("Reset this envelope's shape, ADSR and amount to defaults. Undo restores the previous settings.");
    adsr.setTooltip("Use the saved numeric ADSR values instead of custom points.");
    undo.setTooltip("Undo one edit in this envelope, including a whole drag.");redo.setTooltip("Redo an undone envelope edit.");
    zoomOut.setTooltip("Zoom out. Ctrl + wheel zooms around the pointer.");zoomIn.setTooltip("Zoom in. Wheel pans; middle-drag also pans.");
    zoomOut.onClick=[this]{zoom(2,0.5);};
    zoomIn.onClick=[this]{zoom(0.5,0.5);};
    fitButton.onClick=[this]{finish();automatic=true;fit();layoutHandles();repaint();};
    activate.onClick=[this]{finish();if(onActivate)onActivate();};reset.onClick=[this]{finish();if(onReset)onReset();};activate.setVisible(false);
    adsr.onClick=[this]{finish();if(onUseAdsr)onUseAdsr();};undo.onClick=[this]{finish();if(onUndo)onUndo();};redo.onClick=[this]{finish();if(onRedo)onRedo();};
    addAndMakeVisible(depth);depth.setComponentID(getComponentID()+"-amount");depth.setName("Envelope amount");
    depth.setSliderStyle(juce::Slider::LinearHorizontal);depth.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,22);
    depth.setColour(juce::Slider::trackColourId,colour);depth.setColour(juce::Slider::thumbColourId,colour);
    depth.onValueChange=[this]{if(!updating && onAmount)onAmount(depth.getValue());};
    depth.setDoubleClickReturnValue(true,0);
    depth.onDragStart=[this]{if(onBeginGesture)onBeginGesture();};depth.onDragEnd=[this]{if(onEndGesture)onEndGesture();};
}
EnvelopeEditor::~EnvelopeEditor()=default;
juce::Rectangle<float> EnvelopeEditor::plot() const {return {35,64,std::max(1.0f,(float)getWidth()-55),std::max(1.0f,(float)getHeight()-104)};}
juce::Point<float> EnvelopeEditor::position(const drum::EnvelopePoint& p) const
{const auto a=plot();return {a.getX()+(float)((p.ms-offset)/span)*a.getWidth(),amount<0?a.getY()+(float)p.level*a.getHeight():a.getBottom()-(float)p.level*a.getHeight()};}
double EnvelopeEditor::timeAt(float x) const {return offset+(x-plot().getX())/plot().getWidth()*span;}
double EnvelopeEditor::levelAt(float y) const {return amount<0?(y-plot().getY())/plot().getHeight():(plot().getBottom()-y)/plot().getHeight();}
double EnvelopeEditor::extent() const {return std::max(limit,shape.count?shape.points[(size_t)shape.count-1].ms:0.0);}
void EnvelopeEditor::fit() {offset=0;span=std::clamp(shape.count?shape.points[(size_t)shape.count-1].ms*1.1:500.0,minimumSpan(),extent());}
void EnvelopeEditor::setTimeLimit(double ms)
{
    const auto next=std::clamp(ms,0.001,80000.0);if(limit==next)return;finish();limit=next;
    fitButton.setTooltip("Fit the envelope. Editing limit: original sample length, "+timeText(limit)+". Ctrl + wheel zooms; wheel or middle-drag pans.");
    if(automatic)fit();else {span=std::clamp(span,minimumSpan(),extent());offset=std::clamp(offset,0.0,extent()-span);}
    layoutHandles();repaint();
}
void EnvelopeEditor::setHistory(bool canUndo,bool canRedo) {undo.setEnabled(canUndo);redo.setEnabled(canRedo);}
void EnvelopeEditor::zoom(double factor,double anchor)
{
    if(dragging>=0 || line>=0 || panning)return;
    automatic=false;anchor=std::clamp(anchor,0.0,1.0);const auto time=offset+anchor*span;
    span=std::clamp(span*factor,minimumSpan(),extent());offset=std::clamp(time-anchor*span,0.0,extent()-span);layoutHandles();repaint();
}
void EnvelopeEditor::pan(double delta)
{automatic=false;offset=std::clamp(offset+delta,0.0,std::max(0.0,extent()-span));layoutHandles();repaint();}
void EnvelopeEditor::update(Values v,const drum::EnvelopeShape& stored,bool isLocked,double a,juce::String u,bool enabled,juce::String description,bool newSelection,bool amountMapped)
{
    if(newSelection || isLocked){dragging=line=-1;bending=false;gesture=false;panning=false;}
    const auto next=stored.count?stored:drum::adsrShape(v.attack,v.decay,v.sustain,v.release);
    const bool changed=next!=shape || isLocked!=locked || a!=amount || enabled!=active || u!=unit || description!=status || newSelection;
    shape=next;custom=stored.count!=0;locked=isLocked;amount=a;unit=std::move(u);active=enabled;status=std::move(description);
    if(newSelection){automatic=true;feedback.clear();}
    if(automatic && dragging<0 && line<0)fit();
    updating=true;depth.setVisible(unit!="%");depth.setRange(unit=="oct"?-8:-48,unit=="oct"?8:48,0.01);depth.setTextValueSuffix(" "+unit);
    if(!depth.isMouseButtonDown() && !depth.hasKeyboardFocus(true))depth.setValue(amount,juce::dontSendNotification);updating=false;
    adsr.setEnabled(custom && !locked);reset.setEnabled(!locked && !amountMapped);depth.setEnabled(!amountMapped);
    if(changed){layoutHandles();repaint();}
}
void EnvelopeEditor::setActivation(const juce::String& s)
{
    activate.setTooltip(s=="Unlink timing"?"Keep the current envelope timing and remove only this instrument's envelope timing macro assignments. Other mappings and macro values stay unchanged.":"Enable Envelope playback so the amplitude envelope shapes this sound.");
    if(activate.getButtonText()!=s || activate.isVisible()!=s.isNotEmpty()){activate.setButtonText(s);activate.setVisible(s.isNotEmpty());resized();}
}
void EnvelopeEditor::resized()
{
    zoomOut.setBounds(getWidth()-106,7,25,22);zoomIn.setBounds(getWidth()-78,7,25,22);fitButton.setBounds(getWidth()-50,7,40,22);
    adsr.setBounds(getWidth()-155,7,44,22);activate.setBounds(105,7,110,22);
    depth.setBounds(92,7,std::max(70,getWidth()-253),22);
    reset.setBounds(10,32,44,20);undo.setBounds(58,32,44,20);redo.setBounds(106,32,44,20);layoutHandles();
}
void EnvelopeEditor::layoutHandles()
{
    const auto a=plot();std::array<juce::Point<float>,drum::maxEnvelopePoints> placed;
    for(int n=0;n<drum::maxEnvelopePoints;++n)
    {
        auto& h=*handles[(size_t)n];if(n>=shape.count){h.setVisible(false);continue;}
        auto centre=position(shape.points[(size_t)n]);const bool visible=centre.x>=a.getX()-0.1f && centre.x<=a.getRight()+0.1f;
        for(int previous=0;previous<n;++previous)if(centre.getDistanceFrom(placed[(size_t)previous])<22)centre.y+=centre.y<a.getCentreY()?22:-22;
        centre.y=std::clamp(centre.y,a.getY(),a.getBottom());placed[(size_t)n]=centre;
        h.setVisible(visible);h.setEnabled(editable());h.setAlpha(locked?0.35f:1);h.setBounds(juce::Rectangle<int>(20,20).withCentre(centre.roundToInt()));
        h.setMouseCursor(locked?juce::MouseCursor::NormalCursor:juce::MouseCursor::DraggingHandCursor);
    }
}
void EnvelopeEditor::paint(juce::Graphics& g)
{
    g.setColour(background);g.fillRoundedRectangle(getLocalBounds().toFloat(),7);label(g,title,{10,6,94,24},13,ink);
    label(g,status,{158,32,getWidth()-168,18},10,active?colour:muted);
    if(shape.count<2)return;
    const auto a=plot();const auto c=active?colour:muted;
    for(int n=0;n<5;++n){const float x=a.getX()+a.getWidth()*n/4;g.setColour(grid);g.drawVerticalLine((int)x,a.getY(),a.getBottom());label(g,timeText(offset+span*n/4),{(int)x-(n==4?53:0),(int)a.getBottom()+9,58,15},9,muted);}
    g.setColour(grid);g.drawHorizontalLine((int)a.getY(),a.getX(),a.getRight());g.drawHorizontalLine((int)a.getBottom(),a.getX(),a.getRight());
    label(g,amount<0?"0":unit=="%" || amount==0?"100%":juce::String(amount,1),{2,57,32,15},9,muted);
    label(g,amount<0?juce::String(amount,1):"0",{2,(int)a.getBottom()-7,32,15},9,muted);
    {
        juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(a.expanded(1).toNearestInt());juce::Path path;
        if(shape.points[0].ms>0)path.startNewSubPath(position({0,0}));else path.startNewSubPath(position(shape.points[0]));
        for(int n=0;n<shape.count;++n)
        {
            if(n==0){path.lineTo(position(shape.points[0]));continue;}
            const auto& left=shape.points[(size_t)n-1];const auto& right=shape.points[(size_t)n];
            const int steps=std::clamp((int)((right.ms-left.ms)/span*a.getWidth()),1,1200);
            for(int step=1;step<=steps;++step){const double t=(double)step/steps;path.lineTo(position({left.ms+(right.ms-left.ms)*t,drum::envelopeSegment(left.level,right,t)}));}
        }
        g.setColour(c);g.strokePath(path,juce::PathStrokeType(2));
        if(limit<offset+span){const auto end=position({limit,0}).x;g.setColour(juce::Colours::orange.withAlpha(0.7f));g.drawVerticalLine((int)end,a.getY(),a.getBottom());label(g,"Sample end",{(int)end-68,(int)a.getBottom()-18,68,15},9,muted);}
        const auto hold=position(shape.points[(size_t)shape.hold]);float dash[]{3,3};g.setColour(muted.withAlpha(0.7f));g.drawDashedLine({{hold.x,a.getY()},{hold.x,a.getBottom()}},dash,2);
        label(g,"Hold / note off",{(int)hold.x+5,(int)a.getY()+3,90,15},9,muted);
        if(bending && line>0){const auto& p=shape.points[(size_t)line];const auto& prev=shape.points[(size_t)line-1];const auto grip=position({prev.ms+(p.ms-prev.ms)*p.pivot,drum::envelopeSegment(prev.level,p,p.pivot)});g.setColour(ink);g.drawEllipse(grip.x-4,grip.y-4,8,8,1);}
    }
    for(int n=0;n<shape.count;++n)if(handles[(size_t)n]->isVisible()){g.setColour(c.withAlpha(0.5f));g.drawLine({position(shape.points[(size_t)n]),handles[(size_t)n]->getBounds().getCentre().toFloat()});}
    label(g,feedback.isEmpty()?juce::String(shape.count)+" points / click line: add / drag line: curve / double-click point: remove":feedback,{10,getHeight()-19,getWidth()-20,17},10,ink);
}
void EnvelopeEditor::submit(drum::EnvelopeShape value,bool audibleEdit)
{for(int n=0;n<value.count;++n)value.points[(size_t)n].ms=std::min(value.points[(size_t)n].ms,limit);if(editable() && drum::validEnvelope(value) && onShape)onShape(value,audibleEdit);}
void EnvelopeEditor::beginPoint(int n,juce::Point<float> p)
{if(editable() && n<shape.count){dragging=n;line=-1;initial=shape;dragStart=p;gesture=true;if(onBeginGesture)onBeginGesture();}}
void EnvelopeEditor::dragPoint(juce::Point<float> p)
{
    if(dragging<0 || !editable())return;auto edited=initial;auto& node=edited.points[(size_t)dragging];
    node.ms=std::clamp(std::round((node.ms+(p.x-dragStart.x)/plot().getWidth()*span)*100)/100,0.0,limit);
    node.level=std::clamp(node.level+(amount<0?1:-1)*(p.y-dragStart.y)/plot().getHeight(),0.0,1.0);
    feedback="Point "+juce::String(dragging+1)+": "+timeText(node.ms)+" / "+juce::String(node.level*100,1)+"%";
    // Sort a fixed index array, retaining the logical sustain point as nodes cross.
    std::array<int,drum::maxEnvelopePoints> order{};for(int n=0;n<edited.count;++n)order[(size_t)n]=n;
    std::sort(order.begin(),order.begin()+edited.count,[&](int a,int b){return edited.points[(size_t)a].ms==edited.points[(size_t)b].ms?a<b:edited.points[(size_t)a].ms<edited.points[(size_t)b].ms;});
    auto sorted=edited;for(int n=0;n<edited.count;++n){sorted.points[(size_t)n]=edited.points[(size_t)order[(size_t)n]];if(order[(size_t)n]==edited.hold)sorted.hold=std::min(n,edited.count-2);}
    submit(sorted);
}
void EnvelopeEditor::removePoint(int n)
{
    finish();if(!editable() || n>=shape.count)return;
    if(shape.count<=2){feedback="Keep at least two points.";repaint();return;}
    auto edited=shape;for(int i=n;i<edited.count-1;++i)edited.points[(size_t)i]=edited.points[(size_t)i+1];edited.points[(size_t)--edited.count]={};
    if(n<=edited.hold)--edited.hold;edited.hold=std::clamp(edited.hold,0,edited.count-2);
    if(n<edited.count)edited.points[(size_t)n].bend=0;submit(edited);
}
void EnvelopeEditor::holdPoint(int n)
{finish();if(editable() && n>=0 && n<shape.count-1){auto edited=shape;edited.hold=n;submit(edited,false);}}
void EnvelopeEditor::finish(){dragging=line=-1;bending=false;panning=false;if(gesture){gesture=false;if(onEndGesture)onEndGesture();}feedback.clear();if(automatic)fit();layoutHandles();repaint();}
void EnvelopeEditor::visibilityChanged(){if(!isVisible())finish();}
void EnvelopeEditor::mouseDown(const juce::MouseEvent& e)
{
    if(e.mods.isMiddleButtonDown()){finish();panning=true;automatic=false;dragStart=e.position;panStart=offset;return;}
    if(!editable() || !plot().expanded(8).contains(e.position))return;
    float best=14;int picked=-1;double pickedTime=0,pickedLevel=0;
    for(int n=1;n<shape.count;++n)
    {
        const auto& a=shape.points[(size_t)n-1];const auto& b=shape.points[(size_t)n];auto previous=position(a);
        for(int step=1;step<=64;++step)
        {
            const double t=(double)step/64;auto next=position({a.ms+(b.ms-a.ms)*t,drum::envelopeSegment(a.level,b,t)});juce::Point<float> nearest;
            const float distance=juce::Line<float>(previous,next).getDistanceFromPoint(e.position,nearest);
            if(distance<best && b.ms-a.ms>0.02){best=distance;picked=n;pickedTime=timeAt(nearest.x);pickedLevel=levelAt(nearest.y);}previous=next;
        }
    }
    if(picked>0){line=picked;dragging=-1;initial=shape;dragStart=e.position;grabTime=pickedTime;grabLevel=pickedLevel;bending=false;gesture=true;if(onBeginGesture)onBeginGesture();}
}
void EnvelopeEditor::mouseDrag(const juce::MouseEvent& e)
{
    if(panning){pan(panStart-(e.position.x-dragStart.x)/plot().getWidth()*span-offset);return;}
    if(dragging>=0){dragPoint(e.position);return;}
    if(line<=0 || !editable() || (!bending && e.position.getDistanceFrom(dragStart)<3))return;
    bending=true;auto edited=initial;auto& right=edited.points[(size_t)line];const auto& left=edited.points[(size_t)line-1];
    const double t=std::clamp((grabTime+(e.position.x-dragStart.x)/plot().getWidth()*span-left.ms)/(right.ms-left.ms),0.02,0.98);
    const double level=std::clamp(grabLevel+(amount<0?1:-1)*(e.position.y-dragStart.y)/plot().getHeight(),0.0,1.0);
    drum::bendEnvelopeSegment(left.level,right,t,level);
    feedback="Curve: "+juce::String(level*100,1)+"% at "+timeText(left.ms+t*(right.ms-left.ms));submit(edited);
}
void EnvelopeEditor::mouseUp(const juce::MouseEvent&)
{
    if(line>0 && !bending && editable())
    {
        if(shape.count==drum::maxEnvelopePoints){finish();feedback="Maximum 32 points.";repaint();return;}
        auto edited=shape;if(drum::insertEnvelopePoint(edited,line,grabTime))submit(edited,false);
    }
    finish();
}
void EnvelopeEditor::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel)
{
    if(dragging>=0 || line>=0 || panning)return;
    const auto delta=std::abs(wheel.deltaX)>std::abs(wheel.deltaY)?wheel.deltaX:wheel.deltaY;
    if(e.mods.isCtrlDown())zoom(std::pow(1.4,-delta*5),(e.position.x-plot().getX())/plot().getWidth());
    else pan(-delta*span);
}
