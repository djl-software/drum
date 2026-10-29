#pragma once
#include "EffectRack.h"

class ChainEditor final : public juce::Component, public juce::DragAndDropContainer,
    public juce::DragAndDropTarget, public juce::FileDragAndDropTarget, private juce::Timer, private juce::ListBoxModel
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
    void scan();
    void addFile(const juce::File&,int position=-1);
    void apply(const drum::Result&);
    int insertionAt(juce::Point<int>) const;
    void timerCallback() override {if(isShowing())refresh();}
    int getNumRows() override {return (int)filtered.size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    drum::KitLibrary& library;
    drum::EffectRack& effects;
    juce::String kitId,instrumentId;
    juce::TextEditor search;
    juce::ListBox browser;
    juce::TextButton folder{"Add folder"},rescan{"Refresh"},add{"Add selected"};
    juce::Label status;
    juce::Viewport viewport;
    juce::Component flow;
    std::vector<juce::File> files,filtered;
    juce::StringArray roots;
    std::vector<drum::EffectSlot> slots;
    std::vector<std::unique_ptr<Card>> cards;
    std::unique_ptr<juce::FileChooser> chooser;
    bool loading=false;
};
