#pragma once
#include "PluginProcessor.h"
#include "SoundPreview.h"
#include "EnvelopeEditor.h"
#include "EqualizerEditor.h"
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
    class WaveformDivider;
    void change(const drum::SoundParameter&,double);
    void refresh();
    void refreshPreview();
    void refreshEnvelopes(bool newSelection=false);
    void changeEnvelope(int,const drum::EnvelopeShape&,bool audibleEdit);
    void activateEnvelope(int);
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
    juce::Label noteIndicator;
    juce::TextButton slice;
    juce::ToggleButton adsrValues{"ADSR values"};
    juce::TextButton eqTab{"8-band EQ"},filterTab{"Filter envelope"};
    EqualizerEditor equalizer;
    bool filterEnvelopePage=false;
    juce::ComboBox sliceCount;
    std::array<juce::TextButton,7> tabs;
    juce::Viewport viewport;
    juce::Component content;
    std::unique_ptr<Waveform> waveform;
    std::unique_ptr<WaveformDivider> waveformDivider;
    std::array<std::unique_ptr<EnvelopeEditor>,3> envelopes;
    int waveformHeight=340;
    std::vector<std::unique_ptr<Field>> fields;
    std::vector<std::unique_ptr<MacroField>> macroFields;
};
