#pragma once
#include <juce_core/juce_core.h>
#import <AppKit/AppKit.h>
namespace djl::mac {
// Keeps the editor family above the browser's host window for one gesture.
// The feedback panel ignores mouse input and is excluded from drop hit tests.
class DragPresentation {
public:
    ~DragPresentation();
    void begin(NSWindow* target, NSWindow* source);
    void update(NSPoint point);
    void showFeedback(const juce::String& name, NSPoint point);
    void end();
    NSWindow* feedbackWindow() const { return feedback; }
private:
    NSWindow* root=nil; NSWindow* source=nil; NSWindow* feedback=nil;
    NSInteger originalLevel=0; bool originalHide=false,attached=false;
};
// Native, in-process read of the browser selection at a drag's starting point.
juce::String browserRowFromHit(id element);
bool isCancellationEvent(NSEvent* event,NSEvent* cancellation);
juce::String selectedBrowserRow(id element,NSPoint point,int& budget,bool browser=false);
}
