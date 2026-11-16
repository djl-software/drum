#pragma once
#include "PluginEditor.h"
#if JUCE_MAC
#include "../../Mac/src/EditorWidth.h"
#endif

class DrumCompanionWindow final : public juce::DocumentWindow
{
public:
    explicit DrumCompanionWindow(DrumAudioProcessor& p):DocumentWindow(p.windowTitle(),juce::Colour(0xff10151f),closeButton),processor(p)
    {
        setUsingNativeTitleBar(true);auto* host=dynamic_cast<DrumAudioProcessorEditor*>(p.getHostedDrumEditor());auto bounds=host?host->getScreenBounds():juce::Rectangle<int>{};
        auto* display=new DrumAudioProcessorEditor(p,true);if(host){display->setSize(host->getWidth(),host->getHeight());display->copyPresentationFrom(*host);}
#if JUCE_MAC
        {const juce::ScopedLock guard(p.editorPresentation.lock);const auto& saved=p.editorPresentation.floating;if(saved.validSize())display->setSize(saved.width,saved.height);}
#endif
        setContentOwned(display,true);
        setResizable(true,true);setResizeLimits(800,600,
#if JUCE_MAC
            djl::editorWidthLimit(*display),
#else
            1920,
#endif
            1200);setAlwaysOnTop(false);if(host)setTopLeftPosition(bounds.getX(),bounds.getY());else centreWithSize(getWidth(),getHeight());
        // New tracks get separate visible windows rather than covering exactly
        // the same bounds, while each processor retains just one companion.
        for(int n=0;n<juce::TopLevelWindow::getNumTopLevelWindows();++n){auto* other=juce::TopLevelWindow::getTopLevelWindow(n);if(other!=this && dynamic_cast<DrumCompanionWindow*>(other) && other->getBounds().getPosition()==getBounds().getPosition()){setTopLeftPosition(getX()+28,getY()+28);n=-1;}}
        setVisible(true);
    }
    void closeButtonPressed() override {processor.closeFloatingWindow();}
private:DrumAudioProcessor& processor;
};
