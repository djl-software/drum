#include "PluginEditor.h"
#include "DJLAssets.h"
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
        g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(0.5f), 6.0f);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::FontOptions(13.0f); }
};
class InstrumentGrid final : public juce::Component
{
public:
    std::vector<drum::Instrument> instruments;
    juce::String selected;
    int colour = 0;
    std::function<void(juce::String)> select;
    int columns() const { return juce::jmax(1, (getWidth() + 12) / 170); }
    int contentHeight() const { return juce::jmax(220, ((int)instruments.size() + columns() - 1) / columns() * 112); }
    juce::Rectangle<int> cell(int n) const
    {
        const int width = (getWidth() - (columns() - 1) * 12) / columns();
        return { (n % columns()) * (width + 12), (n / columns()) * 112, width, 100 };
    }
    void paint(juce::Graphics& g) override
    {
        if (instruments.empty())
        {
            label(g, "Build your group", { 12, 55, getWidth()-24, 30 }, 22.0f);
            label(g, "Add instruments, then name and map them.", { 12, 94, getWidth()-24, 28 }, 14.0f, muted);
            return;
        }
        for (int n = 0; n < (int)instruments.size(); ++n)
        {
            const auto& i = instruments[(size_t)n]; const auto area = cell(n); const bool chosen = i.id == selected;
            g.setColour(chosen ? groupColour(colour).withAlpha(0.13f) : juce::Colour(0xff202a39));
            g.fillRoundedRectangle(area.toFloat(), 9.0f);
            g.setColour(chosen ? groupColour(colour) : border);
            g.drawRoundedRectangle(area.toFloat().reduced(0.7f), 9.0f, chosen ? 1.6f : 1.0f);
            label(g, drum::noteName(i.note), { area.getX()+12, area.getY()+10, 56, 19 }, 12, groupColour(colour));
            label(g, "MIDI " + juce::String(i.note), { area.getX()+68, area.getY()+10, area.getWidth()-80, 19 }, 11, muted, juce::Justification::centredRight);
            label(g, i.name, area.withTrimmedTop(37).withHeight(24).reduced(12,0), 16.0f);
            label(g, "Empty instrument", area.withTrimmedTop(71).withHeight(18).reduced(12,0), 11, muted);
        }
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        for (int n = 0; n < (int)instruments.size(); ++n) if (cell(n).contains(event.getPosition())) { if (select) select(instruments[(size_t)n].id); break; }
    }
};
class KeyMap final : public juce::Component, public juce::SettableTooltipClient
{
public:
    drum::Kit kit;
    juce::String selected;
    std::function<void(juce::String)> select;
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
            auto area = key(n); int colour = 0; const auto* i = at(n, colour);
            g.setColour(black ? juce::Colour(0xff101620) : juce::Colour(0xffc6cfdb));
            g.fillRoundedRectangle(area, 2.0f);
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
        int colour = 0; if (auto* i = at(hit(event.position), colour)) if (select) select(i->id);
    }
};
}

class DrumAudioProcessorEditor::Workspace final : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit Workspace(DrumAudioProcessor& p) : processor(p), library(p.getLibrary()), view(library.snapshot()), tooltip(this, 550)
    {
        setLookAndFeel(&look);
        logo = juce::ImageCache::getFromMemory(DJLAssets::DJLLogoWhite_png, DJLAssets::DJLLogoWhite_pngSize);
        for (int n = 0; n < 8; ++n)
        {
            setup(kitTabs[(size_t)n], "", "kit-"+juce::String(n));
            kitTabs[(size_t)n].onClick = [this,n] { if (n < (int)view.kits.size()) { groupId.clear(); instrumentId.clear(); complete(library.activateKit(view.kits[(size_t)n].id)); } };
        }
        setup(addKit, "+ Kit", "add-kit"); addKit.onClick = [this] { groupId.clear(); instrumentId.clear(); complete(library.addKit()); };
        setup(example, "Add example kit", "example-kit"); example.onClick = [this] { groupId.clear(); instrumentId.clear(); complete(library.addKit(true)); };
        setup(copyKit, "Copy kit", "copy-kit"); copyKit.onClick = [this] { complete(library.duplicateKit(view.activeKit)); };
        setup(deleteKit, "Remove kit", "delete-kit"); deleteKit.onClick = [this] {
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
        addInstrument.onClick = [this] { auto r=library.addInstrument(view.activeKit,groupId); if(r.ok) instrumentId=r.id; complete(r); scrollToSelected(); };
        setup(copyInstrument, "Duplicate", "copy-instrument"); copyInstrument.onClick = [this] { auto r=library.duplicateInstrument(view.activeKit,instrumentId); if(r.ok) instrumentId=r.id; complete(r); scrollToSelected(); };
        setup(deleteInstrument, "Remove", "delete-instrument"); deleteInstrument.onClick = [this] {
            auto k=view.activeKit, id=instrumentId;
            confirm("Remove this instrument?", "Its key will become available for another instrument.", [this,k,id] { complete(library.removeInstrument(k,id)); });
        };
        setup(instrumentUp, "Earlier", "instrument-up"); instrumentUp.onClick = [this] { complete(library.reorderInstrument(view.activeKit,instrumentId,-1)); scrollToSelected(); };
        setup(instrumentDown, "Later", "instrument-down"); instrumentDown.onClick = [this] { complete(library.reorderInstrument(view.activeKit,instrumentId,1)); scrollToSelected(); };
        setupName(kitName,"kit-name",[this]{ return view.activeKit; });
        setupName(groupName,"group-name",[this]{ return groupId; });
        setupName(instrumentName,"instrument-name",[this]{ return instrumentId; });
        addAndMakeVisible(groups); groups.setModel(this); groups.setRowHeight(61); groups.setOutlineThickness(0);
        groups.setComponentID("groups"); groups.setName("Instrument groups");
        addAndMakeVisible(viewport); viewport.setViewedComponent(&grid,false); viewport.setScrollBarsShown(true,false);
        viewport.setScrollBarThickness(8); grid.select = [this](juce::String id) { instrumentId=id; refresh(); };
        addAndMakeVisible(keyboard); keyboard.select = [this](juce::String id) {
            for(auto& g:kit().groups) for(auto& i:g.instruments) if(i.id==id) { groupId=g.id; instrumentId=id; refresh(); scrollToSelected(); return; }
        };
        addAndMakeVisible(note); note.setComponentID("instrument-note"); note.setName("Instrument MIDI note"); note.onChange = [this] { if(!refreshing) complete(library.assignNote(view.activeKit,instrumentId,note.getSelectedId()-1)); };
        addAndMakeVisible(destination); destination.setComponentID("instrument-group"); destination.setName("Instrument group"); destination.onChange = [this] {
            int n=destination.getSelectedId()-1;
            if(!refreshing && n>=0 && n<(int)kit().groups.size())
            {
                auto target=kit().groups[(size_t)n].id;
                auto r=library.moveInstrument(view.activeKit,instrumentId,target); if(r.ok) groupId=target;
                complete(r); scrollToSelected();
            }
        };
        status="Arrange your kit. Changes are saved with the DAW project.";
        refresh(); startTimerHz(10);
    }
    ~Workspace() override { stopTimer(); groups.setModel(nullptr); viewport.setViewedComponent(nullptr,false); setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(background);
        g.drawImageWithin(logo,24,20,64,38,juce::RectanglePlacement::centred);
        label(g,"DRUM",{104,17,180,36},29);
        label(g,"KIT WORKSPACE",{106,53,200,18},10,muted);
        label(g,"LAYOUT PREVIEW  /  v" DRUM_VERSION_STRING,{getWidth()-440,22,225,28},11,muted,juce::Justification::centredRight);
        label(g,"ACTIVE KIT",{24,139,86,32},10,accent);
        label(g,juce::String(view.kits.size())+" / 8 kits",{getWidth()-360,139,95,32},12,muted,juce::Justification::centredRight);
        box(g,left); box(g,centre); box(g,right); box(g,keyPanel);
        label(g,"GROUPS",left.reduced(16,0).withY(left.getY()+12).withHeight(26),12,muted);
        label(g,juce::String(kit().groups.size())+" / 16",{left.getRight()-76,left.getY()+12,60,26},12,muted,juce::Justification::centredRight);
        label(g,"GROUP NAME",{left.getX()+16,left.getBottom()-130,left.getWidth()-32,20},10,muted);
        label(g,group().name,{centre.getX()+18,centre.getY()+16,centre.getWidth()-175,30},22,groupColour(group().colour));
        label(g,juce::String(group().instruments.size())+" instruments  /  "+juce::String(drum::instrumentCount(kit()))+" of 88 in kit",{centre.getX()+18,centre.getY()+51,centre.getWidth()-36,22},12,muted);
        label(g,"INSTRUMENT",{right.getX()+18,right.getY()+16,right.getWidth()-36,24},12,muted);
        if (const auto* i=instrument())
        {
            label(g,"NAME",{right.getX()+18,right.getY()+57,right.getWidth()-36,18},10,muted);
            label(g,"KEY ASSIGNMENT",{right.getX()+18,right.getY()+133,right.getWidth()-36,18},10,muted);
            label(g,"GROUP",{right.getX()+18,right.getY()+206,right.getWidth()-36,18},10,muted);
            auto source=juce::Rectangle<int>(right.getX()+18,right.getY()+286,right.getWidth()-36,82);
            g.setColour(background); g.fillRoundedRectangle(source.toFloat(),8);
            label(g,"SOUND SOURCE",source.reduced(12,0).withTrimmedTop(10).withHeight(20),10,muted);
            label(g,"No sample loaded",source.reduced(12,0).withTrimmedTop(34).withHeight(23),15);
            label(g,"Playback and sound controls come next.",{right.getX()+18,right.getY()+378,right.getWidth()-36,23},11,muted);
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
    }
    void resized() override
    {
        const int w=getWidth(), h=getHeight();
        if(w<=0 || h<=0) return;
        example.setBounds(w-194,23,170,31);
        const int kitWidth=juce::jmin(132,(w-132)/8);
        for(int n=0;n<8;++n) kitTabs[(size_t)n].setBounds(24+n*(kitWidth+6),88,kitWidth,35);
        addKit.setBounds(24+(int)view.kits.size()*(kitWidth+6),88,78,35);
        kitName.setBounds(113,138,juce::jmin(340,w-600),34); copyKit.setBounds(w-242,138,96,34); deleteKit.setBounds(w-136,138,112,34);
        const int bodyTop=192, bodyHeight=h-376;
        left={24,bodyTop,216,bodyHeight}; right={w-312,bodyTop,288,bodyHeight}; centre={252,bodyTop,w-576,bodyHeight}; keyPanel={24,h-166,w-48,128};
        groups.setBounds(left.getX()+8,left.getY()+49,left.getWidth()-16,left.getHeight()-198);
        groupName.setBounds(left.getX()+16,left.getBottom()-107,left.getWidth()-32,31);
        addGroup.setBounds(left.getX()+16,left.getBottom()-65,84,29); copyGroup.setBounds(left.getX()+108,left.getBottom()-65,42,29); deleteGroup.setBounds(left.getX()+156,left.getBottom()-65,48,29);
        groupUp.setBounds(left.getX()+16,left.getBottom()-29,84,23); groupDown.setBounds(left.getX()+108,left.getBottom()-29,96,23);
        addInstrument.setBounds(centre.getRight()-145,centre.getY()+18,127,32);
        viewport.setBounds(centre.getX()+18,centre.getY()+90,centre.getWidth()-36,centre.getHeight()-108);
        grid.setSize(juce::jmax(100,viewport.getWidth()-10),grid.getHeight()); grid.setSize(grid.getWidth(),grid.contentHeight());
        instrumentName.setBounds(right.getX()+18,right.getY()+80,right.getWidth()-36,34);
        note.setBounds(right.getX()+18,right.getY()+155,right.getWidth()-36,34);
        destination.setBounds(right.getX()+18,right.getY()+228,right.getWidth()-36,34);
        copyInstrument.setBounds(right.getX()+18,right.getBottom()-72,120,29); deleteInstrument.setBounds(right.getX()+150,right.getBottom()-72,120,29);
        instrumentUp.setBounds(right.getX()+18,right.getBottom()-35,120,23); instrumentDown.setBounds(right.getX()+150,right.getBottom()-35,120,23);
        keyboard.setBounds(keyPanel.reduced(18,0).withTrimmedTop(43).withTrimmedBottom(14));
    }
private:
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
        statusError=!result.ok; status=result.ok?"Saved to this session. Key assignments are preserved when you organise instruments.":result.message;
        if(result.ok) processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
        refresh();
    }
    void confirm(const juce::String& title,const juce::String& message,std::function<void()> action)
    {
        juce::Component::SafePointer<Workspace> safe(this);
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle(title).withMessage(message).withButton("Remove").withButton("Cancel").withAssociatedComponent(this),
            [safe,action](int result) { if(safe && result==1) action(); });
    }
    void refresh()
    {
        const juce::ScopedValueSetter<bool> guard(refreshing,true);
        view=library.snapshot();
        if(!drum::findGroup(kit(),groupId)) groupId=kit().groups.front().id;
        bool inGroup=false; for(auto& i:group().instruments) if(i.id==instrumentId) inGroup=true;
        if(!inGroup) instrumentId=group().instruments.empty()?juce::String():group().instruments.front().id;
        for(int n=0;n<8;++n)
        {
            auto& b=kitTabs[(size_t)n]; b.setVisible(n<(int)view.kits.size());
            if(n<(int)view.kits.size()) { auto& k=view.kits[(size_t)n]; b.setButtonText(k.name); b.setTooltip(k.name); b.setToggleState(k.id==view.activeKit,juce::dontSendNotification); }
        }
        addKit.setVisible(view.kits.size()<8); example.setEnabled(view.kits.size()<8); copyKit.setEnabled(view.kits.size()<8); deleteKit.setEnabled(view.kits.size()>1);
        kitName.setText(kit().name,false); groupName.setText(group().name,false);
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
        for(auto* c:std::initializer_list<juce::Component*>{&instrumentName,&note,&destination,&copyInstrument,&deleteInstrument,&instrumentUp,&instrumentDown}) c->setVisible(i!=nullptr);
        note.clear(juce::dontSendNotification); destination.clear(juce::dontSendNotification);
        if(i)
        {
            instrumentName.setText(i->name,false);
            for(int n=21;n<=108;++n)
            {
                note.addItem("MIDI "+juce::String(n)+"  /  "+drum::noteName(n),n+1);
                for(auto& g:kit().groups) for(auto& other:g.instruments) if(other.note==n && other.id!=i->id) note.setItemEnabled(n+1,false);
            }
            note.setSelectedId(i->note+1,juce::dontSendNotification);
            int index=1; for(auto& g:kit().groups) { destination.addItem(g.name,index); if(g.id==groupId) destination.setSelectedId(index,juce::dontSendNotification); ++index; }
            copyInstrument.setEnabled(drum::instrumentCount(kit())<88);
            instrumentUp.setEnabled(group().instruments.front().id!=instrumentId); instrumentDown.setEnabled(group().instruments.back().id!=instrumentId);
        }
        resized(); repaint();
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
    }
    void selectedRowsChanged(int row) override
    {
        if(refreshing || row<0 || row>=(int)kit().groups.size()) return;
        groupId=kit().groups[(size_t)row].id; instrumentId.clear(); viewport.setViewPosition(0,0); refresh();
    }
    void timerCallback() override { if(library.snapshot().revision!=view.revision) refresh(); }
    DrumLook look;
    DrumAudioProcessor& processor;
    drum::KitLibrary& library;
    drum::State view;
    juce::String groupId,instrumentId,status;
    bool refreshing=false,statusError=false;
    juce::Image logo;
    juce::TooltipWindow tooltip;
    std::array<juce::TextButton,8> kitTabs;
    juce::TextButton addKit,example,copyKit,deleteKit,addGroup,copyGroup,deleteGroup,groupUp,groupDown;
    juce::TextButton addInstrument,copyInstrument,deleteInstrument,instrumentUp,instrumentDown;
    juce::TextEditor kitName,groupName,instrumentName;
    juce::ComboBox note,destination;
    juce::ListBox groups;
    InstrumentGrid grid;
    juce::Viewport viewport;
    KeyMap keyboard;
    juce::Rectangle<int> left,centre,right,keyPanel;
};

DrumAudioProcessorEditor::DrumAudioProcessorEditor(DrumAudioProcessor& p)
    : AudioProcessorEditor(p), workspace(std::make_unique<Workspace>(p))
{
    addAndMakeVisible(*workspace); setResizable(true,true); setResizeLimits(1080,860,1920,1200); setSize(1280,900);
}
DrumAudioProcessorEditor::~DrumAudioProcessorEditor() = default;
void DrumAudioProcessorEditor::resized() { workspace->setBounds(getLocalBounds()); }
