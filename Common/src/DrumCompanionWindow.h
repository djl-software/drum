#pragma once
#include "PluginEditor.h"

class DrumCompanionWindow final : public juce::DocumentWindow
{
public:
    explicit DrumCompanionWindow(DrumAudioProcessor& p):DocumentWindow(p.windowTitle(),juce::Colour(0xff10151f),closeButton),processor(p)
    {
        setUsingNativeTitleBar(true);auto* host=dynamic_cast<DrumAudioProcessorEditor*>(p.getHostedDrumEditor());auto bounds=host?host->getScreenBounds():juce::Rectangle<int>{};
        auto* display=new DrumAudioProcessorEditor(p,true);if(host){display->setSize(host->getWidth(),host->getHeight());display->copyPresentationFrom(*host);}setContentOwned(display,true);
        setResizable(true,true);setResizeLimits(800,600,1920,1200);setAlwaysOnTop(false);if(host)setTopLeftPosition(bounds.getX(),bounds.getY());else centreWithSize(getWidth(),getHeight());
        // New tracks get separate visible windows rather than covering exactly
        // the same bounds, while each processor retains just one companion.
        for(int n=0;n<juce::TopLevelWindow::getNumTopLevelWindows();++n){auto* other=juce::TopLevelWindow::getTopLevelWindow(n);if(other!=this && dynamic_cast<DrumCompanionWindow*>(other) && other->getBounds().getPosition()==getBounds().getPosition()){setTopLeftPosition(getX()+28,getY()+28);n=-1;}}
        setVisible(true);
    }
    void closeButtonPressed() override {processor.closeFloatingWindow();}
private:DrumAudioProcessor& processor;
};
