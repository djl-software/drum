#include "ResetSlider.h"
#include "PluginEditor.h"
#include "PatternEditor.h"
#include "RotaryDial.h"
#include "DJLAssets.h"
#include "SoundEditor.h"
#include <array>
#include <cmath>

namespace
{
const juce::Colour background(0xff10151f), panel(0xff19212e), border(0xff2b3648), text(0xffe5ebf5), muted(0xff8d9bb0), accent(0xff67dfc5);
juce::Colour groupColour(int index)
{
    static const uint32_t colours[] = { 0xffe9ad70, 0xffb39aee, 0xff69cbd4, 0xffe58da7, 0xff8bc892, 0xff8aaeee, 0xffd7ca79, 0xffcba7a0 };
    return juce::Colour(colours[index % 8]);
}
void label(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> area, float size = 14.0f,
           juce::Colour colour = text, juce::Justification alignment = juce::Justification::centredLeft)
{
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawFittedText(value, area, alignment, 1);
}
void box(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(panel); g.fillRoundedRectangle(area.toFloat(), 12.0f);
    g.setColour(border); g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 12.0f, 1.0f);
}
class DrumLook final : public juce::LookAndFeel_V4
{
public:
    DrumLook()
    {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff283447));
        setColour(juce::TextButton::buttonOnColourId, accent);
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::TextButton::textColourOnId, background);
        setColour(juce::TextEditor::backgroundColourId, background);
        setColour(juce::TextEditor::textColourId, text);
        setColour(juce::TextEditor::outlineColourId, border);
        setColour(juce::TextEditor::focusedOutlineColourId, accent);
        setColour(juce::ComboBox::backgroundColourId, background);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::outlineColourId, border);
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::ScrollBar::thumbColourId, border.brighter(0.2f));
        setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff303d52));
        setColour(juce::TooltipWindow::textColourId, text);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool hover, bool down) override
    {
        auto c = b.getToggleState() ? accent : colour;
        if (hover) c = c.brighter(0.12f);
        if (down) c = c.darker(0.12f);
        g.setColour(c.withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.35f));
        g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(0.5f), b.getComponentID().endsWith("-menu")?2.0f:6.0f);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool hover,bool down) override
    {
        if(button.getComponentID().endsWith("-menu")){
            g.setColour(text);g.setFont(juce::FontOptions(13));g.drawText(button.getButtonText(),button.getLocalBounds().reduced(12,0).withTrimmedRight(12),juce::Justification::centredLeft);
            if(button.getButtonText()!="Close" && !bool(button.getProperties()["direct-navigation"])){juce::Path arrow;float x=(float)button.getWidth()-15,y=(float)button.getHeight()/2;arrow.addTriangle(x-4,y-2,x+4,y-2,x,y+3);g.fillPath(arrow);}return;
        }
        if(button.getComponentID()=="add-group" && button.getButtonText()=="+"){g.setColour(text.withMultipliedAlpha(button.isEnabled()?1.0f:0.35f));g.setFont(juce::FontOptions(28));g.drawText("+",button.getLocalBounds(),juce::Justification::centred);return;}
        if(button.getComponentID()=="play-sample")
        {
            const auto centre=button.getLocalBounds().toFloat().getCentre();
            g.setColour(text.withMultipliedAlpha(button.isEnabled()?1.0f:0.35f));
            if(button.getButtonText()=="Pause")
            {
                g.fillRoundedRectangle(centre.x-6,centre.y-7,4,14,1);
                g.fillRoundedRectangle(centre.x+2,centre.y-7,4,14,1);
            }
            else
            {
                juce::Path triangle;triangle.addTriangle(centre.x-5,centre.y-8,centre.x-5,centre.y+8,centre.x+8,centre.y);
                g.fillPath(triangle);
            }
            return;
        }
        if(button.getComponentID().startsWith("kit-") && button.getComponentID().substring(4).containsOnly("0123456789"))
        {
            g.setFont(juce::FontOptions(13.0f));g.setColour(button.getToggleState()?background:text);
            g.drawText(button.getButtonText(),button.getLocalBounds().reduced(10,0),juce::Justification::centred,true);
        }
        else juce::LookAndFeel_V4::drawButtonText(g,button,hover,down);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::FontOptions(13.0f); }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider& slider) override
    {
        drawDrumRotary(g,x,y,width,height,value,start,end,slider);
    }
};
class InstrumentGrid final : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    std::vector<drum::Instrument> instruments;
    std::function<juce::String(const drum::Instrument&)> keyLabel;
    juce::String selected;
    int colour = 0;
    std::function<void(juce::String)> select;
    std::function<void(juce::String)> context,addToKit,remove;
    std::function<void(const juce::StringArray&,juce::String)> drop;
    bool dragging = false;
    bool isInterestedInFileDrag(const juce::StringArray& files) override { return !files.isEmpty(); }
    void fileDragEnter(const juce::StringArray&,int,int) override { dragging=true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { dragging=false; repaint(); }
    void filesDropped(const juce::StringArray& files,int x,int y) override
    {
        dragging=false; repaint();
        for(int n=0;n<(int)instruments.size();++n) if(cell(n).contains(x,y))
        {
            auto id=instruments[(size_t)n].id; if(select) select(id); if(drop) drop(files,id); return;
        }
        if(drop) drop(files,{});
    }
    std::array<bool,128> active {};
    std::function<void(int,bool)> play;
    juce::String dragId;int dragTarget=-1;bool moving=false;juce::Point<int> dragStart;
    std::function<void(juce::String,int)> reorder;
    int pressedNote=-1;
    int columns() const { return 4; }
    int cellSize() const { return (getWidth()-36)/4; }
    int contentHeight() const { return juce::jmax(220, ((int)instruments.size() + columns() - 1) / columns() * (cellSize()+12)); }
    juce::Rectangle<int> cell(int n) const
    {
        const int width = (getWidth() - (columns() - 1) * 12) / columns();
        return { (n % columns()) * (width + 12), (n / columns()) * (width+12), width, width };
    }
    void paint(juce::Graphics& g) override
    {
        if(dragging) { g.setColour(accent.withAlpha(0.12f)); g.fillRoundedRectangle(getLocalBounds().toFloat(),8); }
        if (instruments.empty())
        {
            label(g, "Build your group", { 12, 55, getWidth()-24, 30 }, 22.0f);
            label(g, "Drop audio samples here, or add an instrument.", { 12, 94, getWidth()-24, 28 }, 14.0f, muted);
            return;
        }
        for (int n = 0; n < (int)instruments.size(); ++n)
        {
            const auto& i = instruments[(size_t)n]; const auto area = cell(n); const bool chosen = i.id == selected;
            const bool playing=active[(size_t)i.note];
            g.setColour(playing ? groupColour(colour).withAlpha(0.65f) : chosen ? groupColour(colour).withAlpha(0.13f) : juce::Colour(0xff202a39));
            g.fillRoundedRectangle(area.toFloat(), 9.0f);
            g.setColour(playing ? accent : chosen ? groupColour(colour) : border);
            g.drawRoundedRectangle(area.toFloat().reduced(0.7f), 9.0f, chosen ? 1.6f : 1.0f);
            if(moving && n==dragTarget){g.setColour(accent);g.fillRect(area.getX(),area.getY(),area.getWidth(),4);}
            label(g, drum::noteName(i.note), { area.getX()+10, area.getY()+8, 50, 19 }, 12, playing?text:groupColour(colour));
            label(g, juce::String(i.note), { area.getRight()-(addToKit?70:40), area.getY()+8, 30, 19 }, 11, playing?text:muted, juce::Justification::centredRight);
            if(addToKit){g.setColour(accent);g.setFont(juce::FontOptions(23));g.drawText("+",juce::Rectangle<int>{area.getRight()-32,area.getY()+4,28,28},juce::Justification::centred);}
            g.setColour(text);g.setFont(juce::FontOptions(15.0f));
            g.drawFittedText(i.name,area.reduced(10,0).withTrimmedTop(30).withTrimmedBottom(61),juce::Justification::centred,2);
            if(keyLabel)label(g,keyLabel(i),area.reduced(10,0).withY(area.getBottom()-59).withHeight(17),11,accent);
            juce::String flags=i.controls.mute?"MUTE  ":i.controls.solo?"SOLO  ":"";
            if(i.controls.choke>0)flags+="CH "+juce::String(i.controls.choke);
            label(g,flags,area.reduced(10,0).withY(area.getBottom()-40).withHeight(16),10,i.controls.mute?juce::Colour(0xfff2a3a3):text);
            label(g,i.samplePath.isEmpty()?"No sample":juce::File(i.samplePath).getFileName(),area.reduced(10,0).withY(area.getBottom()-23).withHeight(16),10,playing?text:muted);

        }
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        if(!event.mods.isLeftButtonDown() && !event.mods.isPopupMenu())return;
        for(int n=0;n<(int)instruments.size();++n)if(cell(n).contains(event.getPosition()))
        {
            if(getWantsKeyboardFocus())grabKeyboardFocus();
            auto clicked=instruments[(size_t)n].id;
            if(event.mods.isPopupMenu()){if(select)select(clicked);if(context)context(clicked);return;}
            if(addToKit && juce::Rectangle<int>(cell(n).getRight()-32,cell(n).getY()+4,28,28).contains(event.getPosition())){if(select)select(clicked);addToKit(clicked);return;}
            dragId=instruments[(size_t)n].id;dragStart=event.getPosition();moving=false;dragTarget=n;
            const auto id=instruments[(size_t)n].id;pressedNote=instruments[(size_t)n].note;
            if(select)select(id);if(play)play(pressedNote,true);break;
        }
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {if(!context)return;releaseKey();for(int n=0;n<(int)instruments.size();++n)if(cell(n).contains(e.getPosition())){context(instruments[(size_t)n].id);return;}}
    bool keyPressed(const juce::KeyPress& key) override {if(remove && selected.isNotEmpty() && key.getKeyCode()==juce::KeyPress::deleteKey){remove(selected);return true;}return false;}
    void mouseDrag(const juce::MouseEvent& e) override {
        if(dragId.isEmpty() || e.getPosition().getDistanceFrom(dragStart)<6)return;
        moving=true;releaseKey();dragTarget=-1;
        for(int n=0;n<(int)instruments.size();++n)if(cell(n).contains(e.getPosition()))dragTarget=n;
        if(auto* view=findParentComponentOfClass<juce::Viewport>())view->autoScroll(e.x-view->getViewPositionX(),e.y-view->getViewPositionY(),24,12);
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        releaseKey();auto id=dragId;int target=dragTarget;bool commit=moving && target>=0;
        dragId.clear();dragTarget=-1;moving=false;if(commit && reorder)reorder(id,target);repaint();
    }
    void releaseKey(){if(pressedNote>=0 && play)play(pressedNote,false);pressedNote=-1;}
};
class KeyMap final : public juce::Component, public juce::SettableTooltipClient
{
public:
    drum::Kit kit;
    juce::String selected;
    std::function<void(juce::String)> select;
    std::array<bool,128> active {};
    std::function<void(int,bool)> play;
    int pressedNote=-1;
    std::array<float,128> depression {};
    bool isBlack(int note) const { int n = note % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }
    juce::Rectangle<float> key(int note) const
    {
        const float width = (float)getWidth() / 52.0f; int whites = 0;
        for (int n = drum::firstNote; n < note; ++n) if (!isBlack(n)) ++whites;
        return isBlack(note) ? juce::Rectangle<float>(whites*width-width*0.32f, 0, width*0.64f, getHeight()*0.62f)
                             : juce::Rectangle<float>(whites*width, 0, width-1, (float)getHeight());
    }
    const drum::Instrument* at(int note, int& colour) const
    {
        for (auto& group : kit.groups) for (auto& i : group.instruments) if (i.note == note) { colour = group.colour; return &i; }
        return nullptr;
    }
    void paint(juce::Graphics& g) override
    {
        for (bool black : { false, true }) for (int n = drum::firstNote; n < drum::firstNote+88; ++n)
        {
            if (isBlack(n) != black) continue;
            const auto base=key(n);int colour=0;const auto* i=at(n,colour);
            const bool playing=active[(size_t)n];const float depth=depression[(size_t)n];
            g.setColour(juce::Colour(0xff080c13));g.fillRoundedRectangle(base,2.0f);
            const float travel=black?4.5f:7.0f;
            auto area=base.withTrimmedBottom(travel).translated(0,travel*depth);
            // The front face gets shorter as the key surface moves down into its bed.
            auto lip=base.withTop(area.getBottom()-1);
            g.setColour(black?juce::Colour(0xff303b4d):juce::Colour(0xff8391a3));g.fillRoundedRectangle(lip,2.0f);
            const auto face=black?juce::Colour(0xff182130):juce::Colour(0xffdce3eb);
            g.setGradientFill(juce::ColourGradient(face.darker(depth*0.12f),0,area.getY(),face.darker(black?0.35f:0.08f),0,area.getBottom(),false));
            g.fillRoundedRectangle(area,2.0f);
            if(playing){g.setColour(accent.withAlpha(black?0.45f:0.25f));g.fillRoundedRectangle(area,2.0f);}
            g.setColour((black?juce::Colours::white:juce::Colours::black).withAlpha(0.16f));
            g.drawHorizontalLine((int)area.getBottom()-1,area.getX()+1,area.getRight()-1);
            if (i)
            {
                g.setColour(groupColour(colour)); g.fillRoundedRectangle(area.withTrimmedTop(area.getHeight()-9).reduced(1,1), 2.0f);
                if (i->id == selected) { g.setColour(accent); g.drawRoundedRectangle(area.reduced(1), 2.0f, 2.0f); }
            }
            if (!black && (n % 12 == 0 || n == 21))
                label(g, drum::noteName(n), area.toNearestInt().removeFromBottom(28), 9.0f, juce::Colour(0xff344156), juce::Justification::centred);
        }
    }
    int hit(juce::Point<float> point) const
    {
        for (bool black : { true, false }) for (int n = 21; n <= 108; ++n) if (isBlack(n) == black && key(n).contains(point)) return n;
        return -1;
    }
    void mouseMove(const juce::MouseEvent& event) override
    {
        auto note = hit(event.position); if (note < 0) return;
        int colour = 0; auto* i = at(note, colour);
        setTooltip(drum::noteName(note) + " / MIDI " + juce::String(note) + (i ? " - " + i->name : " - unassigned"));
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        if(!event.mods.isLeftButtonDown())return;
        pressedNote=hit(event.position);int colour=0;
        if(auto* i=at(pressedNote,colour)){const auto id=i->id;if(select)select(id);}
        if(pressedNote>=0 && play)play(pressedNote,true);
    }
    void mouseUp(const juce::MouseEvent&) override { releaseKey(); }
    void releaseKey(){if(pressedNote>=0 && play)play(pressedNote,false);pressedNote=-1;}
};
}

class LibraryTabButton final : public juce::TextButton {
public:
    std::function<void()> context;
    void mouseDown(const juce::MouseEvent& e) override {if(e.mods.isPopupMenu()){if(context)context();}else juce::TextButton::mouseDown(e);}
    void mouseUp(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::TextButton::mouseUp(e);}
};
class DrumAudioProcessorEditor::Workspace final : public juce::Component, private juce::ListBoxModel, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    std::function<void()> onShowLibrary,onBackToKit;
    std::function<void(int)> onLibraryAction;
    void performLibraryAction(int n){if(n==2)complete(processor.newLibraryDocument());if(n==3)chooseDocument(false);if(n==4 || n==5)saveLibraryTab(view.activeKit,n==5,{});if(n==7)complete(processor.duplicateLibraryDocument(view.activeKit));if(n==9)closeLibraryTab(view.activeKit);}
    explicit Workspace(DrumAudioProcessor& p) : processor(p), library(p.getLibrary()), view(library.snapshot()), tooltip(this, 550), soundEditor(p)
    {
        setLookAndFeel(&look);
        setup(kitMenu,"Kit","kit-menu");kitMenu.onClick=[this]{if(!processor.isMidiTrack() && onBackToKit){finishLibraries();return;}showKitMenu();};
        setup(libraryMenu,"Library","library-menu");libraryMenu.getProperties().set("direct-navigation",processor.isMidiTrack());libraryMenu.onClick=[this]{if(processor.isMidiTrack())showLibrary();else showLibraryMenu();};
        setup(openLibrary,"Libraries","open-kit-library");openLibrary.onClick=[this]{showLibrary();};
        setup(saveLibrary,"Save all","save-kit-library");saveLibrary.onClick=[this]{if(processor.saveSharedLibrary){auto result=processor.saveSharedLibrary();status=result.ok?"Shared library saved.":result.message;statusError=!result.ok;repaint();}};
        setup(reloadLibrary,"Reload library","reload-kit-library");reloadLibrary.onClick=[this]{confirm("Reload shared library?","This replaces unsaved library edits with the saved global kits.",[this]{if(processor.reloadSharedLibrary)complete(processor.reloadSharedLibrary());});};
        setup(useSound,"Add","use-library-sound");useSound.onClick=[this]{addLibraryInstrument(instrumentId);};

        setup(newLibraryButton,"New library","new-library");newLibraryButton.onClick=[this]{auto result=processor.openSharedLibrary();if(!result.ok){complete(result);return;}auto* target=processor.libraryEditorProcessor();auto added=target->getLibrary().addKit();if(added.ok)target->getLibrary().rename(added.id,"Library "+juce::String((int)target->getLibrary().snapshot().kits.size()));else {complete(added);return;}showLibrary();};
        setup(patternButton,"MIDI","open-patterns");patternButton.onClick=[this]{if(!patternEditor){patternEditor=std::make_unique<DrumPatternEditor>(processor);addChildComponent(*patternEditor);resized();}patternEditor->open();};
        setup(newKitButton,"New kit","new-kit");newKitButton.onClick=[this]{confirm("Create a new kit?","Save the current kit first if you want to keep it.",[this]{processor.newKit();groupId.clear();instrumentId.clear();refresh();});};
        setup(openDocumentButton,processor.isMidiTrack()?"Open kit":"Open library","open-document");openDocumentButton.onClick=[this]{confirm("Open a file?","Save current edits first. Opening a kit replaces it; opening a library replaces a collection with the same identity.",[this]{chooseDocument(false);});};
        setup(liveSetupButton,"Setup","live-setup");liveSetupButton.onClick=[this]{
            juce::PopupMenu menu;menu.addItem(1,"Automatically update Ableton clips",true,processor.autoRemapLiveClips());
            juce::Component::SafePointer<Workspace> safe(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&liveSetupButton),[safe](int selected){if(safe && selected==1){safe->processor.setAutoRemapLiveClips(!safe->processor.autoRemapLiveClips());safe->status=safe->processor.autoRemapLiveClips()?"Auto-remap enabled: future kit moves update linked Live clips. Deleted instruments' notes are muted.":"Automatic Live clip updates off.";safe->repaint();}});
        };
        setup(useKitButton,"Update","use-kit-live");useKitButton.setTooltip("Connect this kit using the saved DJL Drum Link rack in Live. The companion inserts it if needed when playback is stopped.");useKitButton.onClick=[this]{
            processor.useLiveKit();status="Connection requested. If unlinked: enable Configure on Live's Drum device, click Update here, then disable Configure.";statusError=false;repaint();
        };
        setup(saveDocumentButton,processor.isMidiTrack()?"Save kit":"Save library as","save-document");saveDocumentButton.onClick=[this]{if(processor.isMidiTrack())saveNamedKit();else chooseDocument(true);};
        setup(saveAsButton,"Save kit as","save-document-as");saveAsButton.onClick=[this]{chooseDocument(true);};
        logo = juce::ImageCache::getFromMemory(DJLAssets::DJLLogoWhite_png, DJLAssets::DJLLogoWhite_pngSize);
        for (int n = 0; n < 8; ++n)
        {
            setup(kitTabs[(size_t)n], "", "kit-"+juce::String(n));
            kitTabs[(size_t)n].onClick = [this,n] { if (n < (int)view.kits.size()) { groupId.clear(); instrumentId.clear(); complete(library.activateKit(view.kits[(size_t)n].id)); } };
            kitTabs[(size_t)n].context=[this,n]{if(!processor.isMidiTrack() && !processor.librariesEmpty() && n<(int)view.kits.size())showLibraryTabMenu(view.kits[(size_t)n].id,kitTabs[(size_t)n]);};
            tabStrip.addAndMakeVisible(kitTabs[(size_t)n]);
        }
        tabViewport.setViewedComponent(&tabStrip,false);tabViewport.setScrollBarsShown(false,true);addAndMakeVisible(tabViewport);
        emptyLibraries.setText("No libraries open. Choose Library > New or Load.",juce::dontSendNotification);emptyLibraries.setJustificationType(juce::Justification::centred);emptyLibraries.setColour(juce::Label::backgroundColourId,background);emptyLibraries.setColour(juce::Label::textColourId,text);addChildComponent(emptyLibraries);
        setup(addKit, "New library", "add-kit"); addKit.onClick = [this] { groupId.clear(); instrumentId.clear(); auto created=library.addKit();if(created.ok)library.rename(created.id,"Library "+juce::String((int)library.snapshot().kits.size()));complete(created); };
        setup(example, "Add example kit", "example-kit"); example.onClick = [this] { groupId.clear(); instrumentId.clear(); complete(library.addKit(true)); };
        setup(copyKit, "Copy LIB", "copy-kit"); copyKit.onClick = [this] { complete(library.duplicateKit(view.activeKit)); };
        setup(deleteKit, "Remove LIB", "delete-kit"); deleteKit.onClick = [this] {
            const auto id = view.activeKit;
            confirm("Remove this kit?", "All groups and instruments in this kit will be removed.", [this,id] { complete(library.removeKit(id)); });
        };
        setup(addGroup, "+ Group", "add-group"); addGroup.onClick = [this] { auto r = library.addGroup(view.activeKit); if (r.ok) { groupId = r.id; instrumentId.clear(); } complete(r); };
        setup(copyGroup, "Copy", "copy-group"); copyGroup.onClick = [this] { auto r = library.duplicateGroup(view.activeKit,groupId); if (r.ok) { groupId=r.id; instrumentId.clear(); } complete(r); };
        setup(deleteGroup, "Remove", "delete-group"); deleteGroup.onClick = [this] {
            auto k=view.activeKit, id=groupId;
            confirm("Remove this group?", "Its instruments and key assignments will also be removed.", [this,k,id] { complete(library.removeGroup(k,id)); });
        };
        setup(groupUp, "Up", "group-up"); groupUp.onClick = [this] { complete(library.reorderGroup(view.activeKit,groupId,-1)); };
        setup(groupDown, "Down", "group-down"); groupDown.onClick = [this] { complete(library.reorderGroup(view.activeKit,groupId,1)); };
        setup(addInstrument, "+ Instrument", "add-instrument"); addInstrument.setColour(juce::TextButton::buttonColourId, accent.darker(0.62f));
        addInstrument.onClick = [this] { if(processor.isMidiTrack()){showLibrary();return;}auto r=library.addInstrument(view.activeKit,groupId); if(r.ok) instrumentId=r.id; complete(r); scrollToSelected(); };
        setup(copyInstrument, "Duplicate", "copy-instrument"); copyInstrument.onClick = [this] { auto r=library.duplicateInstrument(view.activeKit,instrumentId); if(r.ok) instrumentId=r.id; complete(r); scrollToSelected(); };
        setup(deleteInstrument, "Remove", "delete-instrument"); deleteInstrument.onClick = [this] {
            auto k=view.activeKit, id=instrumentId;
            confirm("Remove this instrument?", "Its key will become available for another instrument.", [this,k,id] { complete(library.removeInstrument(k,id)); });
        };
        setup(instrumentUp, "Earlier", "instrument-up"); instrumentUp.onClick = [this] { complete(library.reorderInstrument(view.activeKit,instrumentId,-1)); scrollToSelected(); };
        setup(instrumentDown, "Later", "instrument-down"); instrumentDown.onClick = [this] { complete(library.reorderInstrument(view.activeKit,instrumentId,1)); scrollToSelected(); };
        setup(loadSample, "Load sample", "load-sample");
        loadSample.onClick = [this] {
            if(!instrument()) return;
            auto kitId=view.activeKit, id=instrumentId;
            chooser=std::make_unique<juce::FileChooser>("Load a drum sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            juce::Component::SafePointer<Workspace> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe,kitId,id](const juce::FileChooser& dialog) {
                    if(safe && dialog.getResult().existsAsFile()) safe->beginLoad(kitId,id,dialog.getResult());
                });
        };
        setup(playSample, "Play", "play-sample"); playSample.onClick=[this] {
            auto& engine=processor.getSampleEngine();
            if(loopSample.getToggleState() && engine.auditionLoopRunning())engine.pauseAudition();
            else if(auto* i=instrument()) {if(loopSample.getToggleState())engine.setAuditionLoop(i->note);else engine.audition(i->note);}
            updateTransport();
        };
        setup(loopSample,"Loop","audition-loop");loopSample.setClickingTogglesState(true);
        loopSample.setColour(juce::TextButton::buttonOnColourId,accent.darker(0.4f));
        loopSample.onClick=[this]{if(!loopSample.getToggleState())processor.getSampleEngine().pauseAudition();updateTransport();};
        loopSample.setTooltip("Repeat the selected drum once per beat. Separate from sample-region looping.");
        setupSlider(auditionTempo,"audition-bpm",20,999,0.01," BPM");
        useValueBar(auditionTempo);auditionTempo.setTextBoxStyle(juce::Slider::TextBoxLeft,false,78,28);
        processor.getSampleEngine().followHostAuditionTempo();
        auditionTempo.setValue(processor.getSampleEngine().auditionTempo(),juce::dontSendNotification);
        auditionTempo.onValueChange=[this]{processor.getSampleEngine().setAuditionBpm(auditionTempo.getValue());};
        setupSlider(volume,"instrument-volume",-60,12,0.1," dB");
        volume.textFromValueFunction=[](double v) { return v<=-60 ? juce::String("-inf") : juce::String(v,1); };
        volume.valueFromTextFunction=[](const juce::String& t) { return t.containsIgnoreCase("inf")?-60.0:t.getDoubleValue(); };
        volume.onValueChange=[this] { editControls([this](auto& c){c.volumeDb=volume.getValue();}); };
        setupSlider(pan,"instrument-pan",-100,100,1," %");
        pan.setTooltip("Stereo balance: -100 left, 0 centre, +100 right.");
        pan.onValueChange=[this] { editControls([this](auto& c){c.pan=pan.getValue()/100.0;}); };
        setupSlider(tuning,"instrument-tuning",-48,48,0.01," st");
        tuning.setTooltip("Semitones; tuning changes sample speed and duration.");
        tuning.onValueChange=[this] { editControls([this](auto& c){c.tuning=tuning.getValue();}); };
        setup(mute,"Mute","instrument-mute"); mute.setClickingTogglesState(true);
        mute.onClick=[this] { editControls([this](auto& c){c.mute=mute.getToggleState();}); };
        setup(solo,"Solo","instrument-solo"); solo.setClickingTogglesState(true);
        solo.setTooltip("Only soloed instruments in the active kit play. Mute takes priority.");
        solo.onClick=[this] { editControls([this](auto& c){c.solo=solo.getToggleState();}); };
        addAndMakeVisible(choke); choke.setComponentID("instrument-choke"); choke.setName("Choke group");
        choke.setTooltip("Off or 1-16. A hit cuts earlier hits in the same choke group, across the kit.");
        choke.addItem("Off",1); for(int n=1;n<=16;++n) choke.addItem(juce::String(n),n+1);
        choke.onChange=[this] { editControls([this](auto& c){c.choke=choke.getSelectedId()-1;}); };
        setupName(kitName,"kit-name",[this]{ return view.activeKit; });
        setupName(groupName,"group-name",[this]{ return groupId; });
        setupName(instrumentName,"instrument-name",[this]{ return instrumentId; });
        addAndMakeVisible(groups); groups.setModel(this); groups.setRowHeight(61); groups.setOutlineThickness(0);
        groups.setComponentID("groups"); groups.setName("Instrument groups");
        addAndMakeVisible(viewport); viewport.setViewedComponent(&grid,false); viewport.setScrollBarsShown(true,false);
        grid.drop=[this](const juce::StringArray& files,juce::String id) { handleDrop(files,processor.isMidiTrack()?id:juce::String()); };
        if(!processor.isMidiTrack()){grid.setWantsKeyboardFocus(true);grid.context=[this](juce::String id){showInstrumentMenu(id);};grid.remove=[this](juce::String id){removeLibraryInstrument(id);};grid.addToKit=[this](juce::String id){addLibraryInstrument(id);};}
        grid.setComponentID("instrument-grid");
        grid.keyLabel=[this](const drum::Instrument& i){auto sample=processor.getSampleEngine().waveformFor(i.id);return sample?drum::tileKey(sample->tonal,i.controls.tuning+i.controls.sound.detune/100):i.samplePath.isEmpty()?juce::String():juce::String("Analysing...");};
        viewport.setScrollBarThickness(8); grid.select = [this](juce::String id) { importStatus=false; instrumentId=id; refresh(); };
        grid.reorder=[this](juce::String id,int position){complete(library.positionInstrument(view.activeKit,id,position));scrollToSelected();};
        grid.play=[this](int n,bool down){playKey(n,down);};
        keyboard.play=grid.play;
        keyboard.setComponentID("piano-keyboard");
        addAndMakeVisible(keyboard); keyboard.select = [this](juce::String id) {
            for(auto& g:kit().groups) for(auto& i:g.instruments) if(i.id==id) { groupId=g.id; instrumentId=id; refresh(); scrollToSelected(); return; }
        };
        addAndMakeVisible(note);note.setComponentID("instrument-note");note.setName("Instrument keyboard key");note.setColour(juce::Label::textColourId,text);note.setTooltip("Assigned by kit order. Move this instrument or its group to change its keyboard position.");
        addAndMakeVisible(destination); destination.setComponentID("instrument-group"); destination.setName("Instrument group"); destination.onChange = [this] {
            int n=destination.getSelectedId()-1;
            if(!refreshing && n>=0 && n<(int)kit().groups.size())
            {
                auto target=kit().groups[(size_t)n].id;
                auto r=library.moveInstrument(view.activeKit,instrumentId,target); if(r.ok) groupId=target;
                complete(r); scrollToSelected();
            }
        };
        setup(editSound,"Edit sound","open-sound-editor"); editSound.setColour(juce::TextButton::buttonColourId,accent.darker(0.62f));
        addChildComponent(soundEditor);
        editSound.onClick=[this] { soundEditor.select(view.activeKit,instrument()); soundEditor.setVisible(true); soundEditor.toFront(false); };
        soundEditor.close=[this] { soundEditor.setVisible(false); refresh(); };
        status=processor.isMidiTrack()?(processor.hasOpenKit()?"Kit edits are independent of the library. Save kit to collect its samples.":"Choose Kit > New or Load to begin. Add instruments from Library."):"Right-click a library tab to save, duplicate or close it. * means unsaved changes.";
        for(int n=0;n<128;++n)noteSerials[(size_t)n]=processor.getSampleEngine().noteActivity(n)>>32;
        if(!processor.isMidiTrack()){groupId=processor.librarySelectedGroup;instrumentId=processor.librarySelectedInstrument;}
        refresh(); startTimerHz(30);
    }
    ~Workspace() override { processor.getSampleEngine().pauseAudition();grid.releaseKey();keyboard.releaseKey();stopTimer(); groups.setModel(nullptr); viewport.setViewedComponent(nullptr,false);tabViewport.setViewedComponent(nullptr,false); setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(background);
        g.drawImageWithin(logo,24,20,64,38,juce::RectanglePlacement::centred);
        label(g,"DRUM",{104,17,180,36},29);
        label(g,processor.isMidiTrack()?"KIT":"LIBRARIES",{106,53,240,24},processor.isMidiTrack()?10:18,processor.isMidiTrack()?muted:text);
        label(g,"INSTRUMENT PREVIEW / v" DRUM_VERSION_STRING,{getWidth()-440,57,225,18},11,muted,juce::Justification::centredRight);
        if(processor.isMidiTrack())label(g,"KIT NAME",{24,139,86,32},10,accent);
        box(g,left); box(g,centre); box(g,right); box(g,keyPanel);
        label(g,"GROUPS",left.reduced(16,0).withY(left.getY()+12).withHeight(26),processor.isMidiTrack()?12:18,processor.isMidiTrack()?muted:text);
        label(g,juce::String(kit().groups.size())+" / 16",{left.getRight()-76,processor.isMidiTrack()?left.getY()+12:left.getBottom()-36,60,26},14,muted,juce::Justification::centredRight);
        if(processor.isMidiTrack())label(g,"GROUP NAME",{left.getX()+16,left.getBottom()-130,left.getWidth()-32,20},10,muted);
        label(g,group().name,{centre.getX()+18,centre.getY()+16,centre.getWidth()-175,30},22,groupColour(group().colour));
        label(g,juce::String(group().instruments.size())+" instruments  /  "+juce::String(drum::instrumentCount(kit()))+(processor.isMidiTrack()?" of 88 in kit":" of 88 in library"),{centre.getX()+18,centre.getY()+51,centre.getWidth()-36,22},12,muted);
        label(g,"INSTRUMENT",{right.getX()+18,right.getY()+16,right.getWidth()-36,24},12,muted);
        if (const auto* i=instrument())
        {
            label(g,"NAME",{right.getX()+18,right.getY()+43,right.getWidth()-36,16},10,muted);
            label(g,"KEY",{right.getX()+18,right.getY()+98,112,16},10,muted);
            label(g,"GROUP",{right.getX()+150,right.getY()+98,120,16},10,muted);
            auto source=juce::Rectangle<int>(right.getX()+18,right.getY()+153,right.getWidth()-36,66);
            g.setColour(background); g.fillRoundedRectangle(source.toFloat(),8);
            label(g,i->samplePath.isEmpty()?"Drop audio here":juce::File(i->samplePath).getFileName(),source.reduced(10,0).withTrimmedTop(8).withHeight(22),13);
            auto info=processor.getSampleEngine().statusFor(i->id);
            auto detail=info.ready ? juce::String(info.seconds,2)+" s / "+(i->controls.sound.mode==2?"Envelope":i->controls.sound.mode==1?"Gate":"Trigger") : info.message.isNotEmpty()?info.message:"WAV, AIFF, FLAC, OGG, MP3";
            label(g,detail,source.reduced(10,0).withTrimmedTop(35).withHeight(22),11,info.ready?accent:muted);
            label(g,"Volume",{right.getX()+18,right.getY()+264,56,30},11,muted);
            label(g,"Pan",{right.getX()+18,right.getY()+300,56,30},11,muted);
            label(g,"Tuning",{right.getX()+18,right.getY()+336,56,30},11,muted);
            label(g,"Choke",{right.getX()+18,right.getY()+409,56,28},11,muted);
            juce::ignoreUnused(i);
        }
        else
        {
            label(g,"Select an instrument",{right.getX()+18,right.getY()+75,right.getWidth()-36,28},18);
            label(g,"Its name, key and group appear here.",{right.getX()+18,right.getY()+112,right.getWidth()-36,28},12,muted);
        }
        label(g,"KEY MAP",{keyPanel.getX()+18,keyPanel.getY()+9,85,27},11,muted);
        label(g,juce::String(drum::instrumentCount(kit()))+" / 88 assigned",{keyPanel.getX()+110,keyPanel.getY()+9,150,27},12,accent);
        juce::String mapping="MIDI 21-108   /   hover for names, click to select";
        if(auto* i=instrument()) mapping=i->name+"   /   "+drum::noteName(i->note)+"   /   MIDI "+juce::String(i->note);
        label(g,mapping,{keyPanel.getX()+280,keyPanel.getY()+9,keyPanel.getWidth()-298,27},12,muted,juce::Justification::centredRight);
        label(g,status,{24,getHeight()-29,getWidth()-48,22},12,statusError?juce::Colour(0xfff2a3a3):muted);
        label(g,processor.isMidiTrack()?"Drop samples in empty space to add instruments":"Drop samples anywhere to add instruments",{centre.getX()+18,centre.getBottom()-67,centre.getWidth()-36,22},11,muted);
    }
    void resized() override
    {
        const int w=getWidth(), h=getHeight();
        if(w<=0 || h<=0) return;
        example.setBounds(w-194,23,170,31);
        openLibrary.setBounds(24,88,170,35);saveLibrary.setBounds(310,23,140,31);reloadLibrary.setBounds(458,23,140,31);useSound.setBounds(w-194,23,170,31);
        liveSetupButton.setBounds(w-574,23,78,31);liveSetupButton.setVisible(processor.isMidiTrack());
        playSample.setBounds(w-488,23,68,31);loopSample.setBounds(w-412,23,65,31);auditionTempo.setBounds(w-339,23,135,31);
        const int count=(int)view.kits.size(),gap=6,reserved=count<8?142:0;
        const int available=w-48-reserved-gap*juce::jmax(0,count-1);
        std::array<int,8> widths {};int total=0;
        const auto tabFont=look.getTextButtonFont(kitTabs[0],32);
        for(int n=0;n<count;++n)
        {
            widths[(size_t)n]=juce::jlimit(66,168,(int)std::ceil(juce::GlyphArrangement::getStringWidth(tabFont,view.kits[(size_t)n].name+" LIB"))+24);
            total+=widths[(size_t)n];
        }
        // Preserve compact short names; shorten the widest names only when full.
        while(total>available)
        {auto widest=std::max_element(widths.begin(),widths.begin()+count);--*widest;--total;}
        int tabX=24;
        for(int n=0;n<count;++n){kitTabs[(size_t)n].setBounds(tabX,88,widths[(size_t)n],35);tabX+=widths[(size_t)n]+gap;}
        addKit.setBounds(tabX,88,136,35);
        kitName.setBounds(113,138,juce::jmin(340,w-600),34); copyKit.setBounds(w-242,138,96,34); deleteKit.setBounds(w-136,138,112,34);
        const int bodyTop=192, bodyHeight=h-376;
        left={24,bodyTop,216,bodyHeight}; right={w-312,bodyTop,288,bodyHeight}; centre={252,bodyTop,w-576,bodyHeight}; keyPanel={24,h-166,w-48,128};
        groups.setBounds(left.getX()+8,left.getY()+49,left.getWidth()-16,left.getHeight()-198);
        groupName.setBounds(left.getX()+16,left.getBottom()-107,left.getWidth()-32,31);
        addGroup.setBounds(left.getX()+16,left.getBottom()-65,84,29); copyGroup.setBounds(left.getX()+108,left.getBottom()-65,42,29); deleteGroup.setBounds(left.getX()+156,left.getBottom()-65,48,29);
        groupUp.setBounds(left.getX()+16,left.getBottom()-29,84,23); groupDown.setBounds(left.getX()+108,left.getBottom()-29,96,23);
        for(auto* c:std::initializer_list<juce::Component*>{&groupName,&copyGroup,&deleteGroup,&groupUp,&groupDown})c->setVisible(processor.isMidiTrack());
        addGroup.setButtonText(processor.isMidiTrack()?"+ Group":"+");addGroup.setTooltip("Add a group (up to 16)");
        if(!processor.isMidiTrack()){addGroup.setBounds(left.getRight()-54,left.getY()+8,38,36);groups.setBounds(left.getX()+8,left.getY()+49,left.getWidth()-16,left.getHeight()-92);}
        addInstrument.setBounds(centre.getRight()-145,centre.getY()+18,127,32);
        viewport.setBounds(centre.getX()+18,centre.getY()+90,centre.getWidth()-36,centre.getHeight()-166);
        grid.setSize(juce::jmax(100,viewport.getWidth()-10),grid.getHeight()); grid.setSize(grid.getWidth(),juce::jmax(viewport.getHeight(),grid.contentHeight()));
        instrumentName.setBounds(right.getX()+18,right.getY()+63,right.getWidth()-36,29);
        note.setBounds(right.getX()+18,right.getY()+116,122,29);
        destination.setBounds(right.getX()+148,right.getY()+116,122,29);
        loadSample.setBounds(right.getX()+18,right.getY()+225,108,28);
        volume.setBounds(right.getX()+75,right.getY()+264,195,30);
        pan.setBounds(right.getX()+75,right.getY()+300,195,30);
        tuning.setBounds(right.getX()+75,right.getY()+336,195,30);
        mute.setBounds(right.getX()+18,right.getY()+375,120,28); solo.setBounds(right.getX()+150,right.getY()+375,120,28);
        choke.setBounds(right.getX()+80,right.getY()+409,190,28);
        copyInstrument.setBounds(right.getX()+18,right.getBottom()-37,120,27); deleteInstrument.setBounds(right.getX()+150,right.getBottom()-37,120,27);
        instrumentUp.setBounds(centre.getX()+18,centre.getBottom()-35,120,23); instrumentDown.setBounds(centre.getX()+150,centre.getBottom()-35,120,23);
        keyboard.setBounds(keyPanel.reduced(18,0).withTrimmedTop(43).withTrimmedBottom(14));
        editSound.setBounds(right.getRight()-128,right.getY()+12,110,28);
        soundEditor.setBounds(24,100,w-48,h-124);
        if(processor.isMidiTrack()){
            for(auto& tab:kitTabs)tab.setVisible(false);
            for(auto* component:std::initializer_list<juce::Component*>{&addKit,&copyKit,&deleteKit,&example})component->setVisible(false);
            addInstrument.setButtonText("Add from library");
        }
        useKitButton.setBounds(884,88,94,35);useKitButton.setVisible(processor.isMidiTrack());useKitButton.setEnabled(processor.hasOpenKit());newLibraryButton.setBounds(782,88,96,35);newLibraryButton.setVisible(processor.isMidiTrack());patternButton.setBounds(722,88,54,35);patternButton.setVisible(processor.isMidiTrack());patternButton.setEnabled(processor.hasOpenKit());if(patternEditor)patternEditor->setBounds(24,125,w-48,h-149);
        newKitButton.setVisible(processor.isMidiTrack());saveAsButton.setVisible(processor.isMidiTrack());
        newKitButton.setBounds(200,88,105,35);openDocumentButton.setBounds(processor.isMidiTrack()?313:470,processor.isMidiTrack()?88:138,120,35);
        saveDocumentButton.setBounds(processor.isMidiTrack()?441:598,processor.isMidiTrack()?88:138,140,35);saveAsButton.setBounds(589,88,125,35);
        saveDocumentButton.setEnabled(processor.hasOpenKit());saveAsButton.setEnabled(processor.hasOpenKit());
        if(processor.isMidiTrack() && !processor.hasOpenKit()){for(auto* c:std::initializer_list<juce::Component*>{&addGroup,&copyGroup,&deleteGroup,&groupUp,&groupDown,&kitName,&groupName})c->setEnabled(false);}else {kitName.setEnabled(true);groupName.setEnabled(true);}
        openLibrary.setVisible(processor.isMidiTrack());saveLibrary.setVisible(bool(processor.saveSharedLibrary));reloadLibrary.setVisible(bool(processor.reloadSharedLibrary));useSound.setVisible(bool(processor.useLibrarySound));useSound.setEnabled(instrument()!=nullptr);if(processor.useLibrarySound)example.setVisible(false);
        for(auto* c:std::initializer_list<juce::Component*>{&openLibrary,&newLibraryButton,&newKitButton,&openDocumentButton,&saveDocumentButton,&saveAsButton,&saveLibrary,&reloadLibrary,&patternButton,&copyKit,&deleteKit,&addKit,&example})c->setVisible(false);
        useKitButton.setBounds(24,88,100,35);useKitButton.setVisible(processor.isMidiTrack());
        kitMenu.setBounds(processor.isMidiTrack()?132:160,88,128,35);kitMenu.setButtonText(processor.isMidiTrack()?"Kit":"Close");kitMenu.setVisible(processor.isMidiTrack() || bool(onBackToKit));
        libraryMenu.setBounds(processor.isMidiTrack()?268:24,88,128,35);
        kitName.setVisible(processor.isMidiTrack());tabViewport.setVisible(!processor.isMidiTrack());
        if(!processor.isMidiTrack()){
            tabViewport.setBounds(24,135,w-48,47);int x=0;
            for(int n=0;n<count;++n){auto& tab=kitTabs[(size_t)n];int width=juce::jmax(80,(int)std::ceil(juce::GlyphArrangement::getStringWidth(tabFont,processor.libraryDocumentName(view.kits[(size_t)n].id)+" *"))+32);tab.setBounds(x,0,width,32);x+=width+6;}
            tabStrip.setSize(juce::jmax(w-48,x),32);
        }
        const bool noLibrary=!processor.isMidiTrack() && processor.librariesEmpty();
        emptyLibraries.setBounds(24,192,w-48,h-230);emptyLibraries.setVisible(noLibrary);if(noLibrary)emptyLibraries.toFront(false);
        useSound.setEnabled(!noLibrary && instrument()!=nullptr);playSample.setEnabled(!noLibrary);loopSample.setEnabled(!noLibrary);



    }
    bool isInterestedInFileDrag(const juce::StringArray& files) override { return !files.isEmpty(); }
    void filesDropped(const juce::StringArray& files,int x,int y) override
    { handleDrop(files,processor.isMidiTrack() && right.contains(x,y)&&instrument()?instrumentId:juce::String()); }
private:
    void chooseDocument(bool save){
        auto kind=processor.isMidiTrack()?juce::String("kit"):juce::String("library");auto extension=processor.isMidiTrack()?".drumkit":".drumlib";
        auto folder=drum::PortableFiles::root().getChildFile(processor.isMidiTrack()?"Kits":"Libraries");
        chooser=std::make_unique<juce::FileChooser>((save?"Save ":"Open ")+kind,folder.getChildFile(save?juce::File::createLegalFileName(kit().name)+extension:juce::String()),"*"+juce::String(extension));
        juce::Component::SafePointer<Workspace> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|(save?(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting):juce::FileBrowserComponent::openMode),[safe,save,extension](const juce::FileChooser& dialog){
            if(!safe || dialog.getResult()==juce::File())return;auto file=dialog.getResult();if(save)file=file.withFileExtension(extension);
            auto finish=[safe,save,file]{if(!safe)return;auto result=save?safe->processor.saveDocument(file):safe->processor.openDocument(file);safe->groupId.clear();safe->instrumentId.clear();safe->complete(result);if(result.ok){safe->status=(save?"Saved ":"Opened ")+file.getFileName();safe->repaint();}};
            if(save && file!=dialog.getResult() && file.exists())safe->confirm("Replace existing kit/library file?",file.getFileName()+" already exists.",finish);else finish();
        });
    }
    void showLibrary(){auto result=processor.openSharedLibrary();if(!result.ok){complete(result);juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Library",result.message);return;}if(onShowLibrary)onShowLibrary();}
    void saveNamedKit(){
        auto name=juce::File::createLegalFileName(kit().name.trim());if(name.isEmpty())name="Kit";
        auto current=processor.documentFile();auto folder=current==juce::File()?drum::PortableFiles::root().getChildFile("Kits"):current.getParentDirectory();
        auto file=folder.getChildFile(name+".drumkit");
        auto save=[this,file]{auto result=processor.saveDocument(file);complete(result);if(result.ok){status="Saved "+file.getFileName();repaint();}};
        if(file.existsAsFile() && file!=current)confirm("Replace saved kit?",file.getFileName()+" already exists.",save);else save();
    }
    void showKitMenu(){
        juce::PopupMenu menu;menu.addItem(1,"New");menu.addItem(2,"Load...");menu.addSeparator();menu.addItem(3,"Save",processor.hasOpenKit());menu.addItem(4,"Save as...",processor.hasOpenKit());
        juce::Component::SafePointer<Workspace> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&kitMenu),[safe](int n){if(!safe)return;if(n==1)safe->newKitButton.onClick();if(n==2)safe->openDocumentButton.onClick();if(n==3)safe->saveDocumentButton.onClick();if(n==4)safe->saveAsButton.onClick();});
    }
    void showLibraryMenu(){
        juce::PopupMenu menu;bool root=processor.isMidiTrack(),open=root || !processor.librariesEmpty();
        if(root)menu.addItem(1,"Open library screen");
        menu.addItem(2,"New",root || processor.librariesEmpty() || view.kits.size()<8);menu.addItem(3,"Load...");menu.addSeparator();
        menu.addItem(4,"Save",root || (open && processor.libraryDocumentDirty(view.activeKit)));menu.addItem(5,"Save as...",open);menu.addSeparator();menu.addItem(7,"Duplicate",open && (root || view.kits.size()<8));if(!root)menu.addItem(9,"Close",open);
        juce::Component::SafePointer<Workspace> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&libraryMenu),[safe](int n){if(!safe || n==0)return;if(n==1)safe->showLibrary();else if(safe->processor.isMidiTrack()){if(safe->onLibraryAction)safe->onLibraryAction(n);}else safe->performLibraryAction(n);});
    }
    void showLibraryTabMenu(juce::String id,juce::Component& target){
        processor.getEffects().capture();juce::PopupMenu menu;menu.addItem(1,"Save",processor.libraryDocumentDirty(id));menu.addItem(2,"Save as...");menu.addSeparator();menu.addItem(3,"Duplicate",view.kits.size()<8);menu.addItem(4,"Close");
        juce::Component::SafePointer<Workspace> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target),[safe,id](int n){if(!safe)return;if(n==1 || n==2)safe->saveLibraryTab(id,n==2,{});if(n==3)safe->complete(safe->processor.duplicateLibraryDocument(id));if(n==4)safe->closeLibraryTab(id);});
    }
    void saveLibraryTab(juce::String id,bool saveAs,std::function<void(bool)> after){
        if(processor.librariesEmpty()){if(after)after(false);return;}
        complete(library.activateKit(id));auto file=processor.libraryDocumentFile(id);
        auto save=[safe=juce::Component::SafePointer<Workspace>(this),id,after](juce::File target){if(!safe)return;auto selected=safe->library.activateKit(id);if(!selected.ok){safe->complete(selected);if(after)after(false);return;}auto result=safe->processor.saveDocument(target);safe->complete(result);if(after)after(result.ok);};
        if(!saveAs && file!=juce::File()){save(file);return;}
        auto suggested=file!=juce::File()?file:drum::PortableFiles::root().getChildFile("Libraries").getChildFile(juce::File::createLegalFileName(kit().name)+".drumlib");
        chooser=std::make_unique<juce::FileChooser>("Save library",suggested,"*.drumlib");juce::Component::SafePointer<Workspace> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting,[safe,save,after](const juce::FileChooser& dialog){if(!safe)return;auto selected=dialog.getResult();if(selected==juce::File()){if(after)after(false);return;}auto target=selected.withFileExtension(".drumlib");if(target!=selected && target.existsAsFile())safe->confirm("Replace library file?",target.getFileName()+" already exists.",[save,target]{save(target);});else save(target);});
    }
    void closeLibraryTab(juce::String id){
        processor.getEffects().capture();auto close=[safe=juce::Component::SafePointer<Workspace>(this),id]{if(safe){safe->soundEditor.setVisible(false);safe->complete(safe->processor.closeLibraryDocument(id));}};
        if(!processor.libraryDocumentDirty(id)){close();return;}
        juce::Component::SafePointer<Workspace> safe(this);juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Unsaved library").withMessage("Save changes before closing this library?").withButton("Save").withButton("Close without saving").withButton("Cancel").withAssociatedComponent(this),[safe,id,close](int result){if(!safe)return;if(result==1)safe->saveLibraryTab(id,false,[close](bool ok){if(ok)close();});if(result==2)close();});
    }
    void saveLibrariesAndFinish(){
        for(const auto& item:library.snapshot().kits)if(processor.libraryDocumentDirty(item.id)){juce::Component::SafePointer<Workspace> safe(this);saveLibraryTab(item.id,false,[safe](bool ok){if(safe && ok)juce::MessageManager::callAsync([safe]{if(safe)safe->saveLibrariesAndFinish();});});return;}
        if(onBackToKit)onBackToKit();
    }
    void finishLibraries(){
        processor.getEffects().capture();if(!processor.libraryDocumentsDirty()){if(onBackToKit)onBackToKit();return;}
        juce::Component::SafePointer<Workspace> safe(this);juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Unsaved libraries").withMessage("Save changes to your libraries before returning to the kit?").withButton("Save all").withButton("Quit without saving").withButton("Cancel").withAssociatedComponent(this),[safe](int result){if(!safe)return;if(result==1)safe->saveLibrariesAndFinish();if(result==2){safe->processor.discardLibraryDocuments();safe->refresh();if(safe->onBackToKit)safe->onBackToKit();}});
    }
    void removeLibraryInstrument(juce::String id){
        if(processor.isMidiTrack() || id.isEmpty())return;auto kitId=view.activeKit;auto* sound=drum::findInstrument(kit(),id);if(!sound)return;
        juce::Component::SafePointer<Workspace> safe(this);juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Remove instrument?").withMessage("Remove '"+sound->name+"' from this library? The sample file and copies already added to the kit will be kept.").withIconType(juce::MessageBoxIconType::WarningIcon).withButton("Remove").withButton("Cancel").withAssociatedComponent(this),[safe,kitId,id](int result){if(safe && result==1)safe->complete(safe->library.removeInstrument(kitId,id));});
    }
    void showInstrumentMenu(juce::String id){
        auto kitId=view.activeKit;juce::PopupMenu menu;menu.addItem(1,"Duplicate",drum::instrumentCount(kit())<88);menu.addItem(2,"Remove...");menu.addSeparator();menu.addItem(3,"Add to kit");
        juce::Component::SafePointer<Workspace> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&grid),[safe,kitId,id](int action){if(!safe)return;if(action==1){auto result=safe->library.duplicateInstrument(kitId,id);if(result.ok)safe->instrumentId=result.id;safe->complete(result);}if(action==2)safe->removeLibraryInstrument(id);if(action==3)safe->addLibraryInstrument(id);});
    }
    void addLibraryInstrument(juce::String id){
        if(addingToKit || processor.isMidiTrack() || id.isEmpty())return;
        auto* target=processor.destinationKitProcessor();if(!target){complete(drum::Result::fail("Open Library from a Drum kit instance to add this sound."));return;}
        addingToKit=true;auto sourceKit=view.activeKit;
        if(target->hasOpenKit()){chooseKitGroup(sourceKit,id);return;}
        juce::Component::SafePointer<Workspace> safe(this);juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("No kit open").withMessage("Create a new kit or load a saved kit for this instrument.").withButton("New").withButton("Load").withButton("Close").withAssociatedComponent(this),[safe,sourceKit,id](int choice){
            if(!safe)return;auto* destination=safe->processor.destinationKitProcessor();
            if(choice==1){destination->newKit();safe->chooseKitGroup(sourceKit,id);return;}
            if(choice==2){
                safe->chooser=std::make_unique<juce::FileChooser>("Load kit",drum::PortableFiles::root().getChildFile("Kits"),"*.drumkit");
                safe->chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,sourceKit,id](const juce::FileChooser& dialog){if(!safe)return;if(dialog.getResult()==juce::File()){safe->addingToKit=false;return;}auto result=safe->processor.destinationKitProcessor()->openDocument(dialog.getResult());if(!result.ok){safe->addingToKit=false;safe->complete(result);return;}safe->chooseKitGroup(sourceKit,id);});return;
            }
            safe->addingToKit=false;
        });
    }
    void chooseKitGroup(juce::String sourceKit,juce::String id){
        auto* target=processor.destinationKitProcessor();processor.getEffects().capture();auto source=library.snapshot();const drum::Group* sourceGroup=nullptr;const drum::Instrument* sourceSound=nullptr;
        for(const auto& collection:source.kits)if(collection.id==sourceKit)for(const auto& group:collection.groups)for(const auto& sound:group.instruments)if(sound.id==id){sourceGroup=&group;sourceSound=&sound;}
        if(!target || !target->hasOpenKit() || !sourceSound){addingToKit=false;complete(drum::Result::fail("The source instrument or destination kit is no longer open."));return;}
        auto destination=target->getLibrary().snapshot();auto* active=drum::activeKit(destination);auto destinationId=destination.activeKit;
        if(drum::instrumentCount(*active)>=88){addingToKit=false;complete(drum::Result::fail("The active kit already has 88 instruments."));return;}
        auto copy=*sourceSound;auto groupCopy=*sourceGroup;
        auto transfer=[safe=juce::Component::SafePointer<Workspace>(this),target,destinationId,copy,groupCopy](juce::String groupId,bool create){
            if(!safe)return;safe->addingToKit=false;
            if(!target->hasOpenKit() || target->getLibrary().snapshot().activeKit!=destinationId){safe->complete(drum::Result::fail("The active kit changed; add the instrument again."));return;}
            auto result=create?target->getLibrary().importGroupedInstrument(destinationId,groupCopy,copy,false):target->getLibrary().importInstrument(destinationId,groupId,copy);
            if(result.ok){target->updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));safe->status="Added "+copy.name+" to the active kit.";safe->statusError=false;safe->repaint();}else safe->complete(result);
        };
        for(const auto& group:active->groups)if(group.name==sourceGroup->name){transfer(group.id,false);return;}
        juce::PopupMenu menu;menu.addSectionHeader("Add to a kit group");int n=1;std::vector<juce::String> ids;
        for(const auto& group:active->groups){menu.addItem(n++,group.name);ids.push_back(group.id);}
        menu.addSeparator();menu.addItem(1001,"Create group: "+sourceGroup->name,active->groups.size()<16);menu.addItem(1002,"Close");
        juce::Component::SafePointer<Workspace> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&grid),[safe,transfer,ids](int choice){if(!safe)return;safe->addingToKit=false;if(choice==1001)transfer({},true);else if(choice>0 && choice<=(int)ids.size())transfer(ids[(size_t)choice-1],false);});
    }
    void handleDrop(const juce::StringArray& files,const juce::String& target)
    {
        if(processor.isMidiTrack() && !processor.hasOpenKit()){status="Choose New or Load from the Kit menu first.";statusError=true;repaint();return;}
        auto fail=[this](juce::String message){status=message; statusError=true; repaint();};
        if(files.isEmpty()) return;
        if(target.isNotEmpty() && files.size()!=1) { fail("Drop one sample on a card, or several in empty space to create instruments."); return; }
        if(target.isEmpty() && drum::instrumentCount(kit())+files.size()>88) { fail("Not enough free instrument slots for this drop (88 per kit)."); return; }
        for(auto& path:files)
        {
            if(!juce::File::isAbsolutePath(path)) { fail("The browser did not provide an audio file path."); return; }
            juce::File file(path);
            if(!file.hasFileExtension("wav;aif;aiff;flac;ogg;mp3"))
            { fail("Drag audio samples from Live's Samples browser. For instrument presets, render the sound to WAV first."); return; }
        }
        if(processor.librariesEmpty()){auto created=processor.newLibraryDocument();complete(created);if(!created.ok)return;}
        for(auto& path:files)
            if(target.isEmpty()) processor.getSampleEngine().requestImport(view.activeKit,groupId,juce::File(path));
            else processor.getSampleEngine().requestLoad(view.activeKit,target,juce::File(path));
        status="Loading dropped audio..."; statusError=false;
        importStatus=target.isEmpty(); refresh(false);
    }
    void setupSlider(juce::Slider& slider,const char* id,double minimum,double maximum,double step,const char* suffix)
    {
        addAndMakeVisible(slider); slider.setComponentID(id); slider.setName(juce::String(id).replaceCharacter('-',' ')); slider.setTitle(slider.getName());
        useValueBar(slider); slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,72,26);
        slider.setRange(minimum,maximum,step); slider.setTextValueSuffix(suffix); slider.setDoubleClickReturnValue(true,0);
        slider.setColour(juce::Slider::trackColourId,accent.withAlpha(.3f)); slider.setColour(juce::Slider::thumbColourId,accent);
        slider.setColour(juce::Slider::textBoxTextColourId,text); slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId,border);
    }
    void editControls(const std::function<void(drum::InstrumentControls&)>& edit)
    {
        if(refreshing) return;
        auto state=library.snapshot(); auto* k=drum::activeKit(state);
        auto* i=drum::findInstrument(*k,instrumentId); if(!i || k->id!=view.activeKit) return;
        auto controls=i->controls; edit(controls);
        auto result=library.setControls(k->id,i->id,controls);
        if(result.ok) processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        status=result.ok?"Instrument controls saved with the kit.":result.message; statusError=!result.ok; refresh(false);
    }
    void beginLoad(juce::String kitId,juce::String id,juce::File file)
    {
        importStatus=false;
        processor.getSampleEngine().requestLoad(kitId,id,file);
        status="Loading "+file.getFileName()+"..."; statusError=false; refresh();
    }
    const drum::Kit& kit() const { return *drum::activeKit(view); }
    const drum::Group& group() const { return *drum::findGroup(kit(),groupId); }
    const drum::Instrument* instrument() const { return drum::findInstrument(kit(),instrumentId); }
    void setup(juce::TextButton& b,const juce::String& caption,const juce::String& id)
    {
        addAndMakeVisible(b); b.setButtonText(caption); b.setComponentID(id); b.setTooltip(caption);
    }
    void setupName(juce::TextEditor& editor,const juce::String& id,std::function<juce::String()> item)
    {
        addAndMakeVisible(editor); editor.setComponentID(id); editor.setName(id.replaceCharacter('-', ' ')); editor.setTitle(editor.getName()); editor.setInputRestrictions(64); editor.setFont(juce::FontOptions(15.0f));
        editor.setIndents(9,8); editor.setSelectAllWhenFocused(true); editor.setTooltip("Rename; press Enter or click away to save.");
        auto* field=&editor;
        auto save=[this,field,item] {
            if(refreshing || item().isEmpty()) return;
            juce::String current;
            if(item()==kit().id) current=kit().name;
            else if(item()==groupId) current=group().name;
            else if(auto* i=instrument()) current=i->name;
            if(field->getText()!=current) complete(library.rename(item(),field->getText()));
        };
        editor.onReturnKey=[field,save] { save(); field->giveAwayKeyboardFocus(); };
        editor.onFocusLost=save;
        editor.onEscapeKey=[this,field] { refresh(); field->giveAwayKeyboardFocus(); };
    }
    void complete(drum::Result result)
    {
        statusError=!result.ok; status=result.ok?"Saved to this session. MIDI keys follow group and pad order.":result.message;
        if(result.ok) processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        refresh();
    }
    void confirm(const juce::String& title,const juce::String& message,std::function<void()> action)
    {
        juce::Component::SafePointer<Workspace> safe(this);
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle(title).withMessage(message).withButton("Continue").withButton("Cancel").withAssociatedComponent(this),
            [safe,action](int result) { if(safe && result==1) action(); });
    }
    void refresh(bool forceNames = true)
    {
        const juce::ScopedValueSetter<bool> guard(refreshing,true);
        view=library.snapshot();
        if(!drum::findGroup(kit(),groupId)) groupId=kit().groups.front().id;
        bool inGroup=false; for(auto& i:group().instruments) if(i.id==instrumentId) inGroup=true;
        if(!inGroup) instrumentId=group().instruments.empty()?juce::String():group().instruments.front().id;
        if(!processor.isMidiTrack()){processor.librarySelectedGroup=groupId;processor.librarySelectedInstrument=instrumentId;}
        for(int n=0;n<8;++n)
        {
            auto& b=kitTabs[(size_t)n]; b.setVisible(n<(int)view.kits.size() && !processor.librariesEmpty());
            if(n<(int)view.kits.size()) { auto& k=view.kits[(size_t)n]; auto title=processor.libraryDocumentName(k.id);b.setButtonText(title+(processor.libraryDocumentDirty(k.id)?" *":"")); b.setTooltip(title); b.setToggleState(k.id==view.activeKit,juce::dontSendNotification); }
        }
        addKit.setVisible(view.kits.size()<8); example.setEnabled(view.kits.size()<8); copyKit.setEnabled(view.kits.size()<8); deleteKit.setEnabled(view.kits.size()>1);
        if(forceNames || !kitName.hasKeyboardFocus(true)) kitName.setText(kit().name,false);
        if(forceNames || !groupName.hasKeyboardFocus(true)) groupName.setText(group().name,false);
        groups.updateContent();
        int row=0; for(int n=0;n<(int)kit().groups.size();++n) if(kit().groups[(size_t)n].id==groupId) row=n;
        groups.selectRow(row); groups.repaint();
        addGroup.setEnabled(kit().groups.size()<16); deleteGroup.setEnabled(kit().groups.size()>1);
        copyGroup.setEnabled(kit().groups.size()<16 && drum::instrumentCount(kit())+(int)group().instruments.size()<=88);
        groupUp.setEnabled(row>0); groupDown.setEnabled(row+1<(int)kit().groups.size());
        addInstrument.setEnabled(drum::instrumentCount(kit())<88);
        grid.instruments=group().instruments; grid.selected=instrumentId; grid.colour=group().colour; grid.repaint();
        keyboard.kit=kit(); keyboard.selected=instrumentId; keyboard.repaint();
        auto* i=instrument();
        for(auto* c:std::initializer_list<juce::Component*>{&instrumentName,&note,&destination,&copyInstrument,&deleteInstrument,&instrumentUp,&instrumentDown,&loadSample,&volume,&pan,&tuning,&mute,&solo,&choke,&editSound}) c->setVisible(i!=nullptr);
        soundEditor.select(view.activeKit,i);
        note.setText({},juce::dontSendNotification); destination.clear(juce::dontSendNotification);
        if(i)
        {
            const bool controlsSelectionChanged=controlsInstrumentId!=i->id;
            if(controlsSelectionChanged) { volume.hideTextBox(true); pan.hideTextBox(true); tuning.hideTextBox(true); }
            controlsInstrumentId=i->id;
            if(forceNames || !instrumentName.hasKeyboardFocus(true)) instrumentName.setText(i->name,false);
            auto info=processor.getSampleEngine().statusFor(i->id);

            loadSample.setButtonText(i->samplePath.isEmpty()?"Load sample":"Replace");
            loadSample.setTooltip(info.message.isNotEmpty()?info.message:"Choose an audio file");
            if(controlsSelectionChanged || (!volume.isMouseButtonDown() && !volume.hasKeyboardFocus(true))) volume.setValue(i->controls.volumeDb,juce::dontSendNotification);
            if(controlsSelectionChanged || (!pan.isMouseButtonDown() && !pan.hasKeyboardFocus(true))) pan.setValue(i->controls.pan*100,juce::dontSendNotification);
            if(controlsSelectionChanged || (!tuning.isMouseButtonDown() && !tuning.hasKeyboardFocus(true))) tuning.setValue(i->controls.tuning,juce::dontSendNotification);
            mute.setToggleState(i->controls.mute,juce::dontSendNotification); solo.setToggleState(i->controls.solo,juce::dontSendNotification);
            choke.setSelectedId(i->controls.choke+1,juce::dontSendNotification);
            note.setText(drum::noteName(i->note)+" / MIDI "+juce::String(i->note),juce::dontSendNotification);
            int index=1; for(auto& g:kit().groups) { destination.addItem(g.name,index); if(g.id==groupId) destination.setSelectedId(index,juce::dontSendNotification); ++index; }
            copyInstrument.setEnabled(drum::instrumentCount(kit())<88);
            instrumentUp.setEnabled(group().instruments.front().id!=instrumentId); instrumentDown.setEnabled(group().instruments.back().id!=instrumentId);
        }
        updateTransport();resized(); repaint();
    }
    void scrollToSelected()
    {
        for(int n=0;n<(int)grid.instruments.size();++n) if(grid.instruments[(size_t)n].id==instrumentId)
        {
            auto area=grid.cell(n); auto top=viewport.getViewPositionY();
            if(area.getY()<top) viewport.setViewPosition(0,area.getY());
            else if(area.getBottom()>top+viewport.getHeight()) viewport.setViewPosition(0,area.getBottom()-viewport.getHeight());
        }
    }
    int getNumRows() override { return (int)kit().groups.size(); }
    juce::String getNameForRow(int row) override
    {
        return row>=0 && row<(int)kit().groups.size() ? kit().groups[(size_t)row].name : juce::String();
    }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override
    {
        if(row<0 || row>=(int)kit().groups.size()) return;
        const auto& group=kit().groups[(size_t)row];
        if(selected) { g.setColour(groupColour(group.colour).withAlpha(0.1f)); g.fillRoundedRectangle(4,2,(float)width-8,(float)height-4,7); }
        g.setColour(groupColour(group.colour)); g.fillRoundedRectangle(12,16,4,28,2);
        label(g,group.name,{25,9,width-60,24},14);
        label(g,juce::String(group.instruments.size())+" instruments",{25,34,width-40,18},11,muted);
        if(!processor.isMidiTrack()){g.setColour(text);juce::Path arrow;float x=(float)width-23;arrow.addTriangle(x-5,19,x+5,19,x,25);g.fillPath(arrow);}
    }
    void listBoxItemClicked(int row,const juce::MouseEvent& event) override
    {
        if(!event.mods.isPopupMenu() && event.x<groups.getWidth()-44)return;
        showGroupMenu(row);
    }
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override {showGroupMenu(row);}
    void showGroupMenu(int row)
    {
        if(processor.isMidiTrack() || row<0 || row>=(int)kit().groups.size())return;
        const auto selected=kit().groups[(size_t)row];auto kitId=view.activeKit;
        juce::PopupMenu menu;menu.addItem(1,"Rename...");menu.addItem(2,"Duplicate",kit().groups.size()<16 && drum::instrumentCount(kit())+(int)selected.instruments.size()<=88);menu.addSeparator();menu.addItem(3,"Delete...",kit().groups.size()>1);
        juce::Component::SafePointer<Workspace> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(groups.getRowPosition(row,true).translated(groups.getScreenX(),groups.getScreenY())),[safe,kitId,selected](int action){
            if(!safe)return;
            if(action==1){
                auto* dialog=new juce::AlertWindow("Rename group","Enter the group name.",juce::MessageBoxIconType::NoIcon,safe.getComponent());
                dialog->addTextEditor("name",selected.name);dialog->getTextEditor("name")->setInputRestrictions(64);
                dialog->addButton("Rename",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
                juce::Component::SafePointer<juce::AlertWindow> alert(dialog);
                dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,id=selected.id](int result){if(safe && alert && result==1)safe->complete(safe->library.rename(id,alert->getTextEditorContents("name")));}),true);
            }
            if(action==2){auto result=safe->library.duplicateGroup(kitId,selected.id);if(result.ok){safe->groupId=result.id;safe->instrumentId.clear();}safe->complete(result);}
            if(action==3)juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Delete group?").withMessage("Delete '"+selected.name+"' and all its instruments? Their key assignments will be removed and remaining keys will shift.").withIconType(juce::MessageBoxIconType::WarningIcon).withButton("Delete").withButton("Cancel").withAssociatedComponent(safe.getComponent()),[safe,kitId,id=selected.id](int result){if(safe && result==1)safe->complete(safe->library.removeGroup(kitId,id));});
        });
    }
    void selectedRowsChanged(int row) override
    {
        if(refreshing || row<0 || row>=(int)kit().groups.size()) return;
        groupId=kit().groups[(size_t)row].id; instrumentId.clear(); viewport.setViewPosition(0,0); refresh();
    }
    void playKey(int n,bool down)
    {
        processor.getSampleEngine().keyboardNote(n,down);
        if(down)flashes[(size_t)n]=5;
        localHeld[(size_t)n]=down;updateActivity();
    }
    void updateActivity()
    {
        bool changed=false;
        for(int n=0;n<128;++n)
        {
            auto index=(size_t)n;const auto activity=processor.getSampleEngine().noteActivity(n);
            const auto serial=activity>>32;
            if(serial!=noteSerials[index]){noteSerials[index]=serial;flashes[index]=5;}
            const bool active=(activity&0x1ffff)!=0 || localHeld[index] || flashes[index]>0;
            if(grid.active[index]!=active)changed=true;
            grid.active[index]=keyboard.active[index]=active;
        }
        bool moving=false;
        for(size_t n=0;n<keyboard.depression.size();++n)
        {
            const float target=keyboard.active[n]?1.0f:0.0f,previous=keyboard.depression[n];
            keyboard.depression[n]=juce::jlimit(0.0f,1.0f,previous+(target>previous?0.5f:target<previous?-0.34f:0.0f));
            moving|=keyboard.depression[n]!=previous;
        }
        if(changed)grid.repaint();
        if(changed || moving)keyboard.repaint();
    }
    void timerCallback() override
    {
        for(auto& flash:flashes)if(flash>0)--flash;
        updateActivity();
        updateTransport();
        auto epoch=processor.getSampleEngine().statusRevision();
        if(library.snapshot().revision!=view.revision || epoch!=sampleEpoch)
        {
            sampleEpoch=epoch; refresh(false);
            if(importStatus)
            {
                auto info=processor.getSampleEngine().statusFor(groupId);
                status=info.message; statusError=!info.ready && !info.loading;
            }
            else if(auto* i=instrument())
            {
                auto info=processor.getSampleEngine().statusFor(i->id);
                if(info.message.isNotEmpty()) { status=info.ready?"Ready. Trigger the assigned MIDI note or press Play.":info.message; statusError=!info.ready && !info.loading; }
            }
        }
    }
    DrumLook look;
    DrumAudioProcessor& processor;
    drum::KitLibrary& library;
    drum::State view;
    juce::String groupId,instrumentId,status,controlsInstrumentId;
    bool refreshing=false,statusError=false,importStatus=false;
    juce::Image logo;
    juce::TooltipWindow tooltip;
    std::array<LibraryTabButton,8> kitTabs;
    juce::Component tabStrip;juce::Viewport tabViewport;juce::Label emptyLibraries;
    juce::TextButton addKit,example,copyKit,deleteKit,addGroup,copyGroup,deleteGroup,groupUp,groupDown;
    juce::TextButton addInstrument,copyInstrument,deleteInstrument,instrumentUp,instrumentDown;
    void updateTransport()
    {
        auto* selected=instrument();auto& engine=processor.getSampleEngine();
        if(!auditionTempo.isMouseButtonDown())auditionTempo.setValue(engine.auditionTempo(),juce::dontSendNotification);
        auto identity=view.activeKit+"/"+(selected?selected->id:juce::String());
        if(identity!=auditionSelection){engine.pauseAudition();auditionSelection=identity;}
        playSample.setEnabled(selected && engine.statusFor(selected->id).ready);
        playSample.setButtonText(loopSample.getToggleState() && engine.auditionLoopRunning()?"Pause":"Play");
        playSample.setTooltip(playSample.getButtonText());
    }
    juce::String auditionSelection;
    juce::Slider auditionTempo;
    juce::TextButton loadSample,playSample,loopSample;
    juce::TextButton mute,solo;
    ResetSlider volume,pan,tuning;
    std::unique_ptr<juce::FileChooser> chooser;
    uint64_t sampleEpoch = 0;
    std::array<uint64_t,128> noteSerials {};
    std::array<int,128> flashes {};
    std::array<bool,128> localHeld {};
    juce::TextEditor kitName,groupName,instrumentName;
    juce::Label note;
    juce::ComboBox destination,choke;
    std::unique_ptr<DrumPatternEditor> patternEditor;
    bool addingToKit=false;
    juce::TextButton liveSetupButton,useKitButton,patternButton,newLibraryButton;
    juce::TextButton openLibrary,saveLibrary,reloadLibrary,useSound,newKitButton,openDocumentButton,saveDocumentButton,saveAsButton;
    juce::TextButton kitMenu,libraryMenu;
    juce::ListBox groups;
    InstrumentGrid grid;
    juce::Viewport viewport;
    KeyMap keyboard;
    juce::Rectangle<int> left,centre,right,keyPanel;
    juce::TextButton editSound;
    SoundEditor soundEditor;
};

DrumAudioProcessorEditor::DrumAudioProcessorEditor(DrumAudioProcessor& p)
    : AudioProcessorEditor(p), workspace(std::make_unique<Workspace>(p)),processor(p)
{
    workspace->onShowLibrary=[this]{showLibraryScreen(true);};
    workspace->onLibraryAction=[this](int action){auto result=processor.openSharedLibrary();if(!result.ok){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Library",result.message);return;}showLibraryScreen(true);libraryWorkspace->performLibraryAction(action);};
    addAndMakeVisible(*workspace); setResizable(true,true); setResizeLimits(800,600,1920,1200);
    int width=1280,height=900;
    if(auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        width=juce::jlimit(800,1280,(int)(display->userArea.getWidth()*0.84));
        height=juce::jlimit(600,900,(int)(display->userArea.getHeight()*0.90));
    }
    setSize(width,height);
}
DrumAudioProcessorEditor::~DrumAudioProcessorEditor(){libraryWorkspace.reset();processor.closeSharedLibrary();}
void DrumAudioProcessorEditor::showLibraryScreen(bool show){
    if(show && !libraryWorkspace){libraryWorkspace=std::make_unique<Workspace>(*processor.libraryEditorProcessor());libraryWorkspace->onBackToKit=[this]{showLibraryScreen(false);};addChildComponent(*libraryWorkspace);}
    if(!show)processor.closeSharedLibrary();
    showingLibrary=show;workspace->setVisible(!show);if(libraryWorkspace)libraryWorkspace->setVisible(show);resized();
}
void DrumAudioProcessorEditor::resized()
{
    const float scale=juce::jmin(1.0f,(float)getWidth()/1080.0f,(float)getHeight()/860.0f);
    if(scale<=0) return;
    for(auto* screen:{workspace.get(),libraryWorkspace.get()})if(screen){screen->setTransform(juce::AffineTransform::scale(scale));screen->setBounds(0,0,juce::roundToInt(getWidth()/scale),juce::roundToInt(getHeight()/scale));}
}
