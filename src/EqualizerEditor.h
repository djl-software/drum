#pragma once
#include "Equalizer.h"
#include <juce_gui_basics/juce_gui_basics.h>

class EqualizerEditor final : public juce::Component
{
public:
    EqualizerEditor();
    void update(const drum::Equalizer&,double sampleRate);
    std::function<void(const drum::Equalizer&)> onChange;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
private:
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
    std::array<juce::TextButton,8> bands;
    juce::ComboBox type;
    juce::Slider frequency,gain,q,output;
};
