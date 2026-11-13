#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace djl {
// Changes the existing host-owned editor shell; never reparents the editor.
class HostFloat {
public:
    ~HostFloat();
    static void showAuxiliaryWindow(juce::Component& window,juce::Component* owner);
    static void setHostedTitle(juce::Component&,const juce::String&);
    static void showHostedEditor(juce::Component&,bool);
    void update(juce::Component&, bool enabled, bool standalone);
private:
    void restore();
    void* window=nullptr;
    long originalLevel=0;
    bool originalHide=false,lastHostActive=false;
};
}
