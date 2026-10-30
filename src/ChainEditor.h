#pragma once
#include "EffectRack.h"
#include "LiveBrowserBridge.h"
#include <optional>

class ChainEditor final : public juce::Component, public juce::DragAndDropContainer,
    public juce::DragAndDropTarget, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    ChainEditor(drum::KitLibrary&,drum::EffectRack&);
    ~ChainEditor() override;
    void select(const juce::String& kit,const juce::String& instrument);
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
private:
    class Card;
    void refresh();
    void addFile(const juce::File&,int position=-1);
    void apply(const drum::Result&);
    int insertionAt(juce::Point<int>) const;
    void timerCallback() override;
    void browserDropped(const juce::var&);
    void showChainMenu();
    drum::KitLibrary& library;
    drum::EffectRack& effects;
    juce::String kitId,instrumentId;
    juce::TextButton load{"Load VST3..."};
    juce::TextButton builtin{"Add effect"},presets{"Chain presets / defaults"};
    inline static std::optional<std::vector<drum::EffectSlot>> clipboard;
    juce::Label status;
    juce::Viewport viewport;
    juce::Component flow;
    std::vector<drum::EffectSlot> slots;
    std::vector<std::unique_ptr<Card>> cards;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<LiveBrowserBridge> browserBridge;
};
