#pragma once

namespace djl
{
// Keep the hosted editor's original size; the footer belongs to Drum, so even
// third-party editors have an obvious way back to the owning window.
class DoneFooter final : public juce::Component
{
public:
    DoneFooter(juce::DocumentWindow& owner,juce::Component* editor):body(editor){
        addAndMakeVisible(*body);addAndMakeVisible(done);done.setComponentID("subwindow-done");done.setTooltip("Close this window and return to Drum.");
        juce::Component::SafePointer<juce::DocumentWindow> window(&owner);
        done.onClick=[window]{if(window)window->closeButtonPressed();};
        setSize(juce::jmax(120,body->getWidth()),body->getHeight()+44);
    }
    void resized() override{auto bounds=getLocalBounds();auto footer=bounds.removeFromBottom(44);arranging=true;body->setBounds(bounds);arranging=false;done.setBounds(footer.removeFromRight(108).reduced(10,8));}
    void childBoundsChanged(juce::Component* child) override{if(!arranging && child==body.get())setSize(juce::jmax(120,body->getWidth()),body->getHeight()+44);}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(0xff19212e));}
private:
    std::unique_ptr<juce::Component> body;
    juce::TextButton done{"Done"};
    bool arranging=false;
};
inline juce::Component* withDoneButton(juce::DocumentWindow& owner,juce::Component* content){return new DoneFooter(owner,content);}
}
