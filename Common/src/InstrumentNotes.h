#pragma once
#include "PluginProcessor.h"

// The identity is captured when the window opens, so changing selection never
// redirects a notes edit to a different instrument.
class InstrumentNotes final : public juce::Component
{
public:
    InstrumentNotes(DrumAudioProcessor& p,juce::String kit,juce::String instrument)
        :processor(p),kitId(std::move(kit)),instrumentId(std::move(instrument))
    {
        addAndMakeVisible(editor);addAndMakeVisible(status);
        editor.setComponentID("instrument-notes-text");editor.setName("Instrument notes");
        editor.setMultiLine(true,true);editor.setReturnKeyStartsNewLine(true);editor.setScrollbarsShown(true);
        editor.setInputRestrictions(65536);editor.setFont(juce::FontOptions(14));editor.setIndents(10,10);
        editor.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff10151f));
        editor.setColour(juce::TextEditor::textColourId,juce::Colour(0xffe5ebf5));
        editor.setColour(juce::TextEditor::outlineColourId,juce::Colour(0xff2b3648));
        editor.setColour(juce::TextEditor::focusedOutlineColourId,juce::Colour(0xff67dfc5));
        editor.setTextToShowWhenEmpty("Add sound ideas, recording details or reminders...",juce::Colour(0xff8d9bb0));
        auto state=processor.getLibrary().snapshot();
        for(const auto& k:state.kits)if(k.id==kitId)if(auto* i=drum::findInstrument(k,instrumentId))editor.setText(i->notes,false);
        status.setColour(juce::Label::textColourId,juce::Colour(0xff8d9bb0));
        status.setFont(juce::FontOptions(12));status.setText("Saved as you type. Included in instrument exports.",juce::dontSendNotification);
        editor.onTextChange=[this]{auto result=processor.getLibrary().setNotes(kitId,instrumentId,editor.getText());
            status.setText(result.ok?"Saved to this instrument.":result.message,juce::dontSendNotification);
            if(result.ok)processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));};
        setSize(540,340);
    }
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(0xff19212e));}
    void resized() override{editor.setBounds(12,12,getWidth()-24,getHeight()-55);status.setBounds(12,getHeight()-36,getWidth()-24,24);}
private:
    DrumAudioProcessor& processor;
    const juce::String kitId,instrumentId;
    juce::TextEditor editor;juce::Label status;
};
