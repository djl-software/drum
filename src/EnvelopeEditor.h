#pragma once
#include "EnvelopeShape.h"
#include "ResetSlider.h"
#include <juce_gui_basics/juce_gui_basics.h>
class EnvelopeEditor final : public juce::Component
{
public:
    struct Values {double attack=0,decay=100,sustain=1,release=100;};
    EnvelopeEditor(juce::String id,juce::String title,juce::Colour colour);
    ~EnvelopeEditor() override;
    void update(Values,const drum::EnvelopeShape&,bool locked,double amount,juce::String unit,bool active,juce::String status,bool newSelection,bool amountMapped=false);
    void setCycleMode(){cycle=true;adsr.setVisible(false);}
    void setOverlay(juce::Rectangle<float>,double offsetMs,double spanMs);
    bool hitTest(int x,int y) override;
    std::function<void(double)> onOverlayPan;
    std::function<void(const juce::MouseEvent&,const juce::MouseWheelDetails&)> onOverlayWheel;
    juce::Point<float> pointFor(const drum::EnvelopePoint& p) const {return position(p);}
    void setActivation(const juce::String&);
    void setTimeLimit(double ms);
    void setHistory(bool canUndo,bool canRedo);
    std::function<void(const drum::EnvelopeShape&,bool)> onShape;
    std::function<void()> onActivate,onReset,onUseAdsr,onUndo,onRedo,onBeginGesture,onEndGesture;
    std::function<void(double)> onAmount;
    juce::Rectangle<float> getPlotBounds() const {return plot();}
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void visibilityChanged() override;
private:
    class Handle;
    friend class Handle;
    juce::Rectangle<float> plot() const;
    juce::Point<float> position(const drum::EnvelopePoint&) const;
    double timeAt(float x) const;
    double levelAt(float y) const;
    void beginPoint(int,juce::Point<float>);
    void dragPoint(juce::Point<float>,bool fine=false);
    void removePoint(int);
    void holdPoint(int);
    void submit(drum::EnvelopeShape,bool audibleEdit=true);
    void finish();
    void fit();
    void zoom(double factor,double anchor);
    void pan(double delta);
    double extent() const;
    double minimumSpan() const {return std::min(10.0,limit);}
    void layoutHandles();
    bool editable() const {return isEnabled() && !locked;}
    drum::EnvelopeShape shape,initial;
    std::array<std::unique_ptr<Handle>,drum::maxEnvelopePoints> handles;
    juce::TextButton zoomOut{"-"},zoomIn{"+"},fitButton{"Fit"},activate,adsr{"ADSR"},reset{"Reset"},undo{"Undo"},redo{"Redo"};
    ResetSlider depth;
    juce::String title,unit,status,feedback;
    juce::Colour colour;
    double amount=1,span=500,offset=0,grabTime=0,grabLevel=0,limit=80000,panStart=0;
    bool cycle=false,overlay=false;juce::Rectangle<float> overlayPlot;
    bool active=true,automatic=true,locked=false,custom=false,bending=false,updating=false,panning=false,gesture=false;
    int dragging=-1,line=-1;
    juce::Point<float> dragStart;
};
