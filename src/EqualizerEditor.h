#pragma once
#include "Equalizer.h"
#include "ResetSlider.h"
#include "SpectrumAnalyzer.h"
#include <juce_gui_basics/juce_gui_basics.h>

class EqualizerEditor final : public juce::Component,private juce::Timer
{
public:
    EqualizerEditor();
    ~EqualizerEditor() override;
    void setSpectrum(drum::SpectrumTap*);
    void visibilityChanged() override;
    void update(const drum::Equalizer&,double sampleRate);
    std::function<void(const drum::Equalizer&)> onChange;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    int controlsHeight() const {return 144;}
    juce::Rectangle<int> graphBounds() const {return {0,0,getWidth(),getHeight()-controlsHeight()};}
private:
    void timerCallback() override;
    drum::SpectrumTap* spectrum=nullptr;
    drum::SpectrumAnalysis analysis;
    juce::Rectangle<float> plot() const;
    juce::Point<float> point(int) const;
    void select(int);
    void save();
    drum::Equalizer equalizer;
    double rate=44100,initialQ=1;
    int selected=0;
    bool updating=false;
    juce::Point<float> down;
    juce::ToggleButton enabled{"EQ on"},bandOn{"Band on"};
    std::array<juce::TextButton,16> bands;
    std::array<juce::ToggleButton,16> bandEnabled;
    juce::TextButton reset{"Reset EQ"};
    std::array<juce::Label,6> labels;
    juce::ComboBox type,slope;
    ResetSlider frequency,gain,q,output;
};
