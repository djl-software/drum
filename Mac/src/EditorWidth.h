#pragma once

namespace djl
{
inline int editorWidthLimit(const juce::Component& editor){
    const auto& displays=juce::Desktop::getInstance().getDisplays();
    auto* display=displays.getDisplayForRect(editor.getScreenBounds());
    if(!display)display=displays.getPrimaryDisplay();
    return display?juce::jmax(800,display->userArea.getWidth()-16):1920;
}
inline void updateEditorWidth(juce::AudioProcessorEditor& editor,int contentWidth=0){
    const int width=juce::jmax(800,contentWidth>0?juce::jmin(editorWidthLimit(editor),contentWidth):editorWidthLimit(editor));
    if(auto* limits=editor.getConstrainer();limits && limits->getMaximumWidth()!=width)editor.setResizeLimits(800,600,width,1200);
    if(auto* window=dynamic_cast<juce::ResizableWindow*>(editor.getTopLevelComponent()))
        if(auto* limits=window->getConstrainer();limits && limits->getMaximumWidth()!=width)window->setResizeLimits(800,600,width,1200);
}
}
