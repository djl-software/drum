#pragma once
#include "SoundParameters.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdlib>

// Numeric send box: drag vertically without jumping, Shift for fine changes,
// type a value to edit, and double-click/Delete to return to silence.
class MixerSendBar final : public juce::Slider
{
public:
    MixerSendBar()
    {
        setSliderStyle(juce::Slider::LinearBar);setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        setWantsKeyboardFocus(true);setViewportIgnoreDragFlag(true);getProperties().set("mixer-send",true);
        setRange(drum::sendFloor,drum::sendCeiling,.1);setSkewFactorFromMidPoint(-12);setDoubleClickReturnValue(true,drum::sendFloor);
        textFromValueFunction=[](double db){return db<=drum::sendFloor?juce::String::fromUTF8("\xe2\x88\x92\xe2\x88\x9e"):(db>0?"+":"")+juce::String(db,1);};
        valueFromTextFunction=[](const juce::String& v){return v.containsIgnoreCase("inf") || v.containsIgnoreCase("off") || v.contains(juce::String::charToString(0x221e))?drum::sendFloor:juce::jlimit(drum::sendFloor,drum::sendCeiling,v.getDoubleValue());};
        addChildComponent(entry);entry.setComponentID("send-value-entry");entry.setFont(juce::FontOptions(10));entry.setJustification(juce::Justification::centred);entry.setSelectAllWhenFocused(true);entry.onReturnKey=[this]{finish(true);};entry.onEscapeKey=[this]{finish(false);};entry.onFocusLost=[this]{finish(true);};
        setTooltip("Drag up/down. Shift: fine. Type a number or press Enter to edit. Double-click/Delete: off. Range: -infinity to +6 dB.");
    }
    ~MixerSendBar() override {editing=false;entry.onReturnKey=nullptr;entry.onEscapeKey=nullptr;entry.onFocusLost=nullptr;}
    void mouseDown(const juce::MouseEvent& e) override {if(!e.mods.isLeftButtonDown())return;grabKeyboardFocus();lastY=e.position.y;}
    void mouseDrag(const juce::MouseEvent& e) override {if(!e.mods.isLeftButtonDown())return;setValue(getValue()+(lastY-e.position.y)*(getMaximum()-getMinimum())/190.0*(e.mods.isShiftDown()?.1:1.0),juce::sendNotificationSync);lastY=e.position.y;}
    void mouseUp(const juce::MouseEvent&) override {}
    void mouseDoubleClick(const juce::MouseEvent&) override {setValue(getDoubleClickReturnValue(),juce::sendNotificationSync);}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override {float delta=wheel.deltaY!=0?wheel.deltaY:wheel.deltaX;if(delta==0)return;setValue(getValue()+(delta>0?1:-1)*(bool(getProperties()["mixer-pan"])? .01:1)*(e.mods.isShiftDown()?.1:1),juce::sendNotificationSync);}
    bool keyPressed(const juce::KeyPress& key) override {
        int code=key.getKeyCode();if(code==juce::KeyPress::returnKey){edit({});return true;}
        if(code==juce::KeyPress::deleteKey || code==juce::KeyPress::backspaceKey){setValue(getDoubleClickReturnValue(),juce::sendNotificationSync);return true;}
        if(code==juce::KeyPress::upKey || code==juce::KeyPress::downKey || code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey){int direction=code==juce::KeyPress::upKey || code==juce::KeyPress::rightKey?1:-1;setValue(getValue()+direction*(bool(getProperties()["mixer-pan"])? .01:1)*(key.getModifiers().isShiftDown()?.1:1),juce::sendNotificationSync);return true;}
        auto character=key.getTextCharacter();if((character>='0' && character<='9') || character=='-' || character=='+' || character=='.'){edit(juce::String::charToString(character));return true;}return false;
    }
    void resized() override {juce::Slider::resized();entry.setBounds(getLocalBounds());}
private:
    void edit(juce::String initial){editing=true;entry.setText(initial.isEmpty()?getTextFromValue(getValue()):initial,false);entry.setVisible(true);entry.toFront(true);if(initial.isEmpty())entry.selectAll();else entry.moveCaretToEnd();}
    void finish(bool accept){if(!editing)return;editing=false;if(accept){auto text=entry.getText().trim();if(bool(getProperties()["mixer-pan"])){setValue(getValueFromText(text),juce::sendNotificationSync);entry.setVisible(false);repaint();return;}if(text.containsIgnoreCase("inf") || text.containsIgnoreCase("off") || text.contains(juce::String::charToString(0x221e)))setValue(drum::sendFloor,juce::sendNotificationSync);else {auto number=text.replace("dB","",true).trim();auto utf8=number.toRawUTF8();char* end=nullptr;double db=std::strtod(utf8,&end);if(end!=utf8 && *end=='\0' && std::isfinite(db))setValue(db,juce::sendNotificationSync);}}entry.setVisible(false);repaint();}
    juce::TextEditor entry;bool editing=false;float lastY=0;
};
