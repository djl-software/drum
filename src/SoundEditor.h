#pragma once
#include "PluginProcessor.h"
#include "SoundPreview.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SoundEditor final : public juce::Component, public juce::FileDragAndDropTarget,private juce::Timer
{
public:
    explicit SoundEditor(DrumAudioProcessor&);
    ~SoundEditor() override;
    void select(const juce::String& kit,const drum::Instrument* instrument);
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    std::function<void()> close;
    bool isInterestedInFileDrag(const juce::StringArray& files) override { return files.size()==1; }
    void filesDropped(const juce::StringArray&,int,int) override;
private:
    struct Field;
    struct MacroField;
    class Waveform;
    void change(const drum::SoundParameter&,double);
    void refresh();
    void refreshPreview();
    void setPage(int);
    void timerCallback() override;
    DrumAudioProcessor& processor;
    juce::String kitId,instrumentId,name,path,message;
    drum::SoundParameters sound;
    drum::InstrumentControls selectedControls;
    std::shared_ptr<const drum::Sample> selectedSample;
    drum::SoundPreview preview;
    bool updating=false;
    int page=0,note=21;
    juce::TextButton back,play,stop,reset;
    juce::TextButton slice;
    juce::ComboBox sliceCount;
    std::array<juce::TextButton,6> tabs;
    juce::Viewport viewport;
    juce::Component content;
    std::unique_ptr<Waveform> waveform;
    std::vector<std::unique_ptr<Field>> fields;
    std::vector<std::unique_ptr<MacroField>> macroFields;
};
