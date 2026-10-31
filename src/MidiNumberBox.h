#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Relative vertical dragging avoids large jumps when editing wide-range numbers.
class MidiNumberBox final : public juce::Slider {
public:
    MidiNumberBox(){setSliderStyle(LinearVertical);setTextBoxStyle(NoTextBox,false,0,0);setWantsKeyboardFocus(true);setMouseCursor(juce::MouseCursor::UpDownResizeCursor);addChildComponent(entry);entry.setInputRestrictions(32,"0123456789.-");entry.onReturnKey=[this]{finishText(true);};entry.onEscapeKey=[this]{finishText(false);};entry.onFocusLost=[this]{finishText(true,false);};}
    void paint(juce::Graphics& g) override{auto r=getLocalBounds().toFloat().reduced(.5f);g.setColour(juce::Colour(0xff263441));g.fillRoundedRectangle(r,3);g.setColour(hasKeyboardFocus(true)?juce::Colour(0xff5dd8c0):juce::Colour(0xff6b7e88));g.drawRoundedRectangle(r,3,1);g.setColour(isEnabled()?juce::Colours::whitesmoke:juce::Colours::grey);g.setFont(12);g.drawText(getTextFromValue(getValue()),getLocalBounds().reduced(5,0),juce::Justification::centred);}
    void resized() override{entry.setBounds(getLocalBounds().reduced(1));}
    void mouseDown(const juce::MouseEvent& e) override{if(!isEnabled() || e.mods.isPopupMenu())return;grabKeyboardFocus();startValue=getValue();startY=e.position.y;dragging=true;if(onDragStart)onDragStart();}
    void mouseDrag(const juce::MouseEvent& e) override{if(!dragging)return;double distance=(startY-e.position.y)/(e.mods.isShiftDown()?24.0:4.0);setValue(startValue+std::round(distance)*step(),juce::sendNotificationSync);}
    void mouseUp(const juce::MouseEvent&) override{endDrag();}
    void mouseDoubleClick(const juce::MouseEvent&) override{beginText();}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override{if(!isEnabled() || entry.isVisible())return;double delta=wheel.deltaY!=0?wheel.deltaY:wheel.deltaX;if(delta==0)return;wheelRemainder+=delta*(e.mods.isShiftDown()?1:10);int ticks=(int)std::trunc(wheelRemainder+std::copysign(1.e-6,wheelRemainder));if(!ticks)return;wheelRemainder-=ticks;if(onDragStart)onDragStart();setValue(getValue()+ticks*step(),juce::sendNotificationSync);if(onDragEnd)onDragEnd();}
    bool keyPressed(const juce::KeyPress& key) override{if(key.getKeyCode()==juce::KeyPress::returnKey){beginText();return true;}if(key.getKeyCode()==juce::KeyPress::upKey || key.getKeyCode()==juce::KeyPress::downKey){if(onDragStart)onDragStart();setValue(getValue()+(key.getKeyCode()==juce::KeyPress::upKey?1:-1)*step(),juce::sendNotificationSync);if(onDragEnd)onDragEnd();return true;}return false;}
private:
    void beginText(){endDrag();if(!isEnabled())return;entry.setText(juce::String(getValue(),getInterval()<1?3:0),false);entry.setVisible(true);entry.grabKeyboardFocus();entry.selectAll();}
    double step()const{return getInterval()>0?getInterval():1;}
    void endDrag(){if(!dragging)return;dragging=false;if(onDragEnd)onDragEnd();}
    void finishText(bool accept,bool restoreFocus=true){if(!entry.isVisible())return;auto text=entry.getText().trim();entry.setVisible(false);if(accept && text.containsAnyOf("0123456789")){if(onDragStart)onDragStart();setValue(text.getDoubleValue(),juce::sendNotificationSync);if(onDragEnd)onDragEnd();}if(restoreFocus)grabKeyboardFocus();repaint();}
    juce::TextEditor entry;bool dragging=false;double startValue=0,startY=0,wheelRemainder=0;
};
