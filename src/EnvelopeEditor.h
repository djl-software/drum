#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// A held-note ADSR editor. Its time axis is independent of the sample waveform.
class EnvelopeEditor final : public juce::Component
{
public:
    struct Values
    {
        double attack=0,decay=100,sustain=1,release=100;
        bool operator==(const Values&) const = default;
    };
    EnvelopeEditor(juce::String id,juce::String title,juce::Colour colour);
    ~EnvelopeEditor() override;
    void update(Values,const std::array<bool,4>& mapped,double amount,juce::String unit,
                bool active,juce::String status,bool newSelection);
    std::function<void(int,Values)> onChange;
    std::function<void()> onActivate;
    void setActivation(const juce::String& label);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void visibilityChanged() override;
private:
    class Handle;
    friend class Handle;
    juce::Rectangle<float> plot() const;
    std::array<juce::Point<float>,5> points() const;
    void begin(int,juce::Point<float>);
    void drag(juce::Point<float>);
    void finish();
    void edit(int,Values);
    void fit();
    void layoutHandles();
    bool editable(int) const;
    Values values,initial;
    std::array<bool,4> mapped {};
    std::array<std::unique_ptr<Handle>,4> handles;
    juce::TextButton zoomOut{"-"},zoomIn{"+"},fitButton{"Fit"},activate;
    juce::String title,unit,status;
    juce::Colour colour;
    double amount=1,span=500;
    bool active=true,automatic=true;
    int dragging=-1;
    juce::Point<float> dragStart;
};
