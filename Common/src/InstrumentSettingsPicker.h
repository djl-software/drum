#pragma once
#include "KitLibrary.h"
#include "ResetSlider.h"
#include <juce_gui_basics/juce_gui_basics.h>

class InstrumentSettingsPicker final:public juce::Component{public:
                    std::function<void()> onChange;
                    std::array<juce::ToggleButton,14> choices;juce::TextButton all{"All"},none{"None"};
                    InstrumentSettingsPicker(){setLookAndFeel(&look);setOpaque(true);const char* names[]{"Sample / playback / choke","Amplitude envelope / mode","Pitch envelope","Filter + filter envelope","16-band EQ","LFO","Attack layer","Tuning / pitch response","Mixer / mute / solo / pan","Output destination","Send levels A-F","Independent send shapes A-F","Macro mappings","Instrument effect chain"};
                        for(int n=0;n<14;++n){addAndMakeVisible(choices[n]);choices[n].setButtonText(names[n]);choices[n].setToggleState(false,juce::dontSendNotification);choices[n].setComponentID("settings-section-"+juce::String(n));choices[n].onClick=[this]{if(onChange)onChange();};}
                        for(auto* b:{&all,&none})addAndMakeVisible(b);all.onClick=[this]{setAll(true);};none.onClick=[this]{setAll(false);};setSize(540,270);}
                    ~InstrumentSettingsPicker() override {setLookAndFeel(nullptr);}
                    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff19212e));}
                    void setAll(bool on){for(auto& c:choices)c.setToggleState(on,juce::dontSendNotification);if(onChange)onChange();}
                    void resized() override{all.setBounds(8,0,74,26);none.setBounds(90,0,74,26);for(int n=0;n<14;++n)choices[n].setBounds((n%2)*270,36+(n/2)*32,268,28);}
                    drum::InstrumentSettingsSelection selected() const{return {choices[0].getToggleState(),choices[1].getToggleState(),choices[2].getToggleState(),choices[3].getToggleState(),choices[4].getToggleState(),choices[5].getToggleState(),choices[6].getToggleState(),choices[7].getToggleState(),choices[8].getToggleState(),choices[9].getToggleState(),choices[10].getToggleState(),choices[11].getToggleState(),choices[12].getToggleState(),choices[13].getToggleState()};}
                    private:ValueBarLook look;
                };
