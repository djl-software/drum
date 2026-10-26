#include "PluginEditor.h"
#include "DJLAssets.h"

DrumAudioProcessorEditor::DrumAudioProcessorEditor(DrumAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      logo(juce::ImageCache::getFromMemory(DJLAssets::DJLLogoWhite_png, DJLAssets::DJLLogoWhite_pngSize))
{
    setSize(720, 440);
}

void DrumAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff171b24));
    g.drawImageWithin(logo, 24, 20, 90, 50, juce::RectanglePlacement::centred);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(30.0f));
    g.drawText("Drum", 132, 24, getWidth() - 156, 42, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffaeb9ca));
    g.setFont(juce::FontOptions(18.0f));
    g.drawText("Ready for development", getLocalBounds(), juce::Justification::centred);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("v" DRUM_VERSION_STRING, getLocalBounds().reduced(24).removeFromBottom(24), juce::Justification::bottomRight);
}
