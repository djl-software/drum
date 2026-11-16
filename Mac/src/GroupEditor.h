#pragma once
#include "PluginProcessor.h"
#include "ChainEditor.h"
#include "MixerSendBar.h"
#include "MixerLook.h"
#include "SendRoutingMenu.h"

class GroupEditor final : public juce::Component,private juce::Timer {
public:
    std::function<void()> onBack,onExit;
    GroupEditor(DrumAudioProcessor& p,juce::String kit,juce::String group):processor(p),kitId(std::move(kit)),groupId(std::move(group)),chain(p.getLibrary(),p.getEffects()){
        setLookAndFeel(&look);look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff293848));look.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff397f73));look.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffe5ebf5));
        addAndMakeVisible(back);addAndMakeVisible(exit);back.setComponentID("group-back");exit.setComponentID("group-exit");back.onClick=[this]{if(onBack)onBack();};exit.onClick=[this]{if(onExit)onExit();};
        setComponentID("group-processing");addAndMakeVisible(chain);addAndMakeVisible(main);main.setButtonText("Group chain");main.onClick=[this]{select(-1);};
        for(int n=0;n<6;++n){auto index=(size_t)n;addAndMakeVisible(levels[index]);levels[index].setComponentID("group-send-"+juce::String(n));levels[index].onValueChange=[this,n]{if(updating)return;auto result=processor.getLibrary().setGroupSend(kitId,groupId,n,levels[(size_t)n].getValue());if(result.ok)changed();else{auto state=processor.getLibrary().snapshot();if(auto* kit=drum::activeKit(state))if(auto* group=drum::findGroup(*kit,groupId)){updating=true;levels[(size_t)n].setValue(group->sends[(size_t)n],juce::dontSendNotification);updating=false;levels[(size_t)n].setTooltip(result.message);}}refresh();};
            addAndMakeVisible(edit[index]);edit[index].setButtonText("Edit Send "+juce::String::charToString('A'+n));edit[index].setComponentID("group-edit-send-"+juce::String(n));edit[index].onClick=[this,n]{select(n);};
            addAndMakeVisible(destinations[index]);destinations[index].setComponentID("group-send-destination-"+juce::String(n));destinations[index].onClick=[this,n]{drum::showSendRoutingMenu(destinations[(size_t)n],processor.getLibrary(),processor.getEffects(),kitId,drum::groupReturnId(groupId,n),[this]{changed();refresh();});};}
        select(-1);startTimerHz(10);
    }
    ~GroupEditor() override{stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(0xff10151f));g.setColour(juce::Colour(0xffe5ebf5));g.setFont(20);g.drawText(name+" - group processing",20,8,getWidth()-230,28,juce::Justification::left);
        g.setFont(12);g.setColour(juce::Colour(0xff8d9bb0));g.drawText("Member chains and send taps > group chain > group fader > group sends. Choose destinations on each send row.",20,38,getWidth()-40,22,juce::Justification::left);
        const int split=250;g.setColour(juce::Colour(0xff67dfc5));g.drawLine(220,156,split,156,2);g.drawVerticalLine(split,80,266);
        for(int n=0;n<6;++n){const int y=76+n*32;g.setColour(levels[(size_t)n].getValue()>drum::sendFloor?juce::Colour(0xff6cb5ff):juce::Colour(0xff526077));g.drawLine(split,y+12,277,y+12,2);g.setFont(12);g.drawText("Send "+juce::String::charToString('A'+n),280,y,65,25,juce::Justification::left);}
        g.setColour(juce::Colour(0xff293848));g.drawHorizontalLine(280,18,(float)getWidth()-18);
    }
    void resized() override{back.setBounds(getWidth()-200,8,85,28);exit.setBounds(getWidth()-105,8,85,28);main.setBounds(20,132,200,44);for(int n=0;n<6;++n){int y=76+n*32;levels[(size_t)n].setBounds(345,y,85,25);edit[(size_t)n].setBounds(445,y,120,25);destinations[(size_t)n].setBounds(580,y,juce::jmax(160,getWidth()-600),25);}chain.setBounds(20,298,getWidth()-40,getHeight()-312);}
private:
    void changed(){processor.getEffects().sync();processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));}
    void select(int n){selected=n;chain.select(kitId,n<0?drum::groupChainId(groupId):drum::groupReturnId(groupId,n));main.setToggleState(n<0,juce::dontSendNotification);for(int i=0;i<6;++i)edit[(size_t)i].setToggleState(i==n,juce::dontSendNotification);refresh();}
    void refresh(){auto state=processor.getLibrary().snapshot();auto* kit=drum::activeKit(state);auto* group=kit && kit->id==kitId?drum::findGroup(*kit,groupId):nullptr;if(!group){setEnabled(false);return;}name=group->name;updating=true;for(int n=0;n<6;++n){if(!levels[(size_t)n].isMouseButtonDown() && !levels[(size_t)n].hasKeyboardFocus(true))levels[(size_t)n].setValue(group->sends[(size_t)n],juce::dontSendNotification);destinations[(size_t)n].setButtonText("To: "+drum::sendDestinationText(*kit,group->returnRouting[(size_t)n]));}updating=false;repaint();}
    void timerCallback() override{if(isShowing())refresh();}
    DrumAudioProcessor& processor;juce::String kitId,groupId,name;DrumMixerLook look;ChainEditor chain;juce::TextButton back{"Back"},exit{"Done"},main;std::array<MixerSendBar,6> levels;std::array<juce::TextButton,6> edit,destinations;int selected=-1;bool updating=false;
};
