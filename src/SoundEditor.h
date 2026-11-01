#pragma once
#include "PluginProcessor.h"
#include "SoundPreview.h"
#include "EnvelopeEditor.h"
#include "EqualizerEditor.h"
#include "ChainEditor.h"
#include "ResetSlider.h"
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
    bool isInterestedInFileDrag(const juce::StringArray& files) override { return chain.isInterestedInFileDrag(files) || (files.size()==1 && !files[0].endsWithIgnoreCase(".vst3")); }
    void filesDropped(const juce::StringArray&,int,int) override;
private:
    struct Field;
    struct MacroField;
    class Waveform;
    class WaveformDivider;
    class KeySpectrum;
    void changeKeyTuning();
    void change(const drum::SoundParameter&,double);
    void refresh();
    void refreshPreview();
    void syncEnvelopeOverlay();
    void refreshEnvelopes(bool newSelection=false);
    void changeEnvelope(int,const drum::EnvelopeShape&,bool audibleEdit);
    void activateEnvelope(int);
    void resetEnvelope(int);
    void undoEnvelope(int,bool redo);
    void beginEnvelopeGesture(int);
    void endEnvelopeGesture(int);
    double envelopeLimit() const;
    void constrainEnvelope(drum::SoundParameters&,int, double drum::SoundParameters::* changed=nullptr) const;
    struct EnvelopeState
    {
        drum::EnvelopeShape shape;
        std::array<double,4> values{};
        double amount=0,enabled=0;
        std::array<drum::MacroMapping,drum::numMacros> macros{};
        bool operator==(const EnvelopeState&) const = default;
    };
    static EnvelopeState envelopeState(const drum::InstrumentControls&,int);
    void recordEnvelope(int,const drum::InstrumentControls&,const drum::InstrumentControls&);
    struct EnvelopeHistory
    {
        std::vector<EnvelopeState> undo,redo;
        EnvelopeState expected;
        bool known=false,gesture=false,recorded=false;
    };
    std::array<EnvelopeHistory,3> envelopeHistory;
    void setPage(int);
    void timerCallback() override;
    DrumAudioProcessor& processor;
    juce::String kitId,instrumentId,name,path,message;
    drum::SoundParameters sound;
    drum::InstrumentControls selectedControls;
    std::shared_ptr<const drum::Sample> selectedSample;
    drum::SoundPreview preview;
    bool updating=false,chainPresent=false;
    int page=0,note=21;
    ChainEditor chain;
    juce::TextButton back,reset;
    juce::Label noteIndicator;
    juce::TextButton slice;
    juce::ToggleButton adsrValues{"ADSR values"};
    juce::TextButton ampOverlay{"Amplitude"},pitchOverlay{"Pitch"};
    int selectedEnvelope=0;
    juce::TextButton eqTab{"8-band EQ"},filterTab{"Filter"};
    EqualizerEditor equalizer;
    juce::ComboBox sliceCount;
    std::array<juce::TextButton,9> tabs;
    std::unique_ptr<KeySpectrum> keySpectrum;
    ResetSlider keyCoarse,keyFine;
    juce::Label keyCoarseLabel,keyFineLabel,keyStatus;
    juce::Viewport viewport;
    juce::Component content;
    std::unique_ptr<Waveform> waveform;
    std::unique_ptr<WaveformDivider> waveformDivider;
    std::array<std::unique_ptr<EnvelopeEditor>,3> envelopes;
    int waveformHeight=340;
    std::vector<std::unique_ptr<Field>> fields;
    std::vector<std::unique_ptr<MacroField>> macroFields;
};
