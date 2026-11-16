#include "platform/HostFloat.h"
#include "EditorPresentation.h"
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
namespace djl::mac {void trace(const juce::String&);}
namespace djl {
namespace {
NSWindow* editorWindow(juce::Component& editor){
    auto* peer=editor.getPeer();if(!peer)return nil;
    auto* view=static_cast<NSView*>(peer->getNativeHandle());auto* window=[view window];
    if(!window || ![window isVisible] || !([window styleMask]&NSWindowStyleMaskTitled))return nil;
    auto bounds=[view convertRect:[view bounds] toView:[window contentView]],content=[[window contentView] bounds];
    return bounds.size.width>=content.size.width*.8 && bounds.size.height>=content.size.height*.8?window:nil;
}
void setFamilyLevel(NSWindow* window,NSInteger level){[window setLevel:level];for(NSWindow* child in [window childWindows])setFamilyLevel(child,level);}
}
bool restoreEditorPlacement(juce::Component& editor,const EditorPlacement& saved){
    auto* window=editorWindow(editor);if(!window)return false;
    if(saved.validSize())editor.setSize(saved.width,saved.height);
    if(saved.positioned){auto frame=[window frame];frame.origin=NSMakePoint(saved.x,saved.y);bool visible=false;
        for(NSScreen* screen in [NSScreen screens]){auto intersection=NSIntersectionRect(frame,[screen visibleFrame]);visible|=intersection.size.width>=100 && intersection.size.height>=100;}
        if(!visible)if(auto* screen=[window screen]?:[NSScreen mainScreen]){auto area=[screen visibleFrame];frame.origin=NSMakePoint(NSMidX(area)-frame.size.width*.5,NSMidY(area)-frame.size.height*.5);}
        [window setFrameOrigin:frame.origin];
    }
    return true;
}
bool captureEditorPlacement(juce::Component& editor,EditorPlacement& saved){
    auto* window=editorWindow(editor);if(!window)return false;auto frame=[window frame];
    saved={(int)std::lround(frame.origin.x),(int)std::lround(frame.origin.y),editor.getWidth(),editor.getHeight(),true};return true;
}
void HostFloat::showAuxiliaryWindow(juce::Component& child,juce::Component* owner){
 child.setVisible(true);
 if(auto* peer=child.getPeer())if(auto* w=[static_cast<NSView*>(peer->getNativeHandle()) window]){
  NSWindow* parent=nil;if(owner)if(auto* parentPeer=owner->getPeer())parent=[static_cast<NSView*>(parentPeer->getNativeHandle()) window];
  // Live's host shell can have a higher level than an ordinary JUCE window.
  // Attach above its real NSWindow and retain the relationship when refocusing.
  [w setHidesOnDeactivate:NO];
  if(parent && parent!=w){if([w parentWindow]!=parent){if([w parentWindow])[[w parentWindow] removeChildWindow:w];[parent addChildWindow:w ordered:NSWindowAbove];}[w setLevel:[parent level]];}
  [w makeKeyAndOrderFront:nil];
  [w orderFrontRegardless];
 }
 // JUCE toFront can reset the native level. Restore the parent's level last.
 child.toFront(true);
 if(auto* peer=child.getPeer())if(auto* w=[static_cast<NSView*>(peer->getNativeHandle()) window])
  if(auto* parent=[w parentWindow]){[w setLevel:[parent level]];[w orderWindow:NSWindowAbove relativeTo:[parent windowNumber]];}
}
void HostFloat::setHostedTitle(juce::Component& editor,const juce::String& title){
 if(auto* peer=editor.getPeer()){
  auto* view=static_cast<NSView*>(peer->getNativeHandle());auto* w=[view window];
  if(!w || w==[NSApp mainWindow] || !([w styleMask]&NSWindowStyleMaskTitled))return;
  const auto size=[view convertRect:[view bounds] toView:[w contentView]].size,content=[[w contentView] bounds].size;
  if(size.width<content.width*.8 || size.height<content.height*.8)return;
  static char visibleKey;const bool wasVisible=[(NSNumber*)objc_getAssociatedObject(view,&visibleKey) boolValue];
  const bool visible=[w isVisible];objc_setAssociatedObject(view,&visibleKey,@(visible),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  if(visible && !wasVisible){
    bool onScreen=false;for(NSScreen* screen in [NSScreen screens]){
      auto intersection=NSIntersectionRect([w frame],[screen visibleFrame]);onScreen|=intersection.size.width>100 && intersection.size.height>100;
    }
    if(!onScreen)if(auto* screen=[NSScreen mainScreen]){auto area=[screen visibleFrame],frame=[w frame];[w setFrameOrigin:NSMakePoint(NSMidX(area)-frame.size.width*.5,NSMidY(area)-frame.size.height*.5)];}
    if([NSApp isActive])[w makeKeyAndOrderFront:nil];
    mac::trace("Hosted editor shown: "+juce::String::fromUTF8([NSStringFromRect([w frame]) UTF8String])+" / level "+juce::String((int)[w level]));
  }
  auto* value=[NSString stringWithUTF8String:title.toRawUTF8()];if(![[w title] isEqualToString:value])[w setTitle:value];
 }
}
void HostFloat::showHostedEditor(juce::Component& editor,bool show){
 if(auto* peer=editor.getPeer()){
  auto* view=static_cast<NSView*>(peer->getNativeHandle());auto* w=[view window];
  if(w){auto bounds=[view convertRect:[view bounds] toView:[w contentView]];auto content=[[w contentView] bounds];
   if(bounds.size.width>=content.size.width*0.8 && bounds.size.height>=content.size.height*0.8 && w!=[NSApp mainWindow]){
    if(show)[w orderFront:nil];else [w orderOut:nil];
   }
  }
 }
}
HostFloat::~HostFloat() { restore(); }
void HostFloat::restore() {
    if(auto* w=static_cast<NSWindow*>(window)) {
        [w setLevel:originalLevel]; [w setHidesOnDeactivate:originalHide]; [w release];
    }
    window=nullptr;lastHostActive=false;
}
void HostFloat::update(juce::Component& editor,bool enabled,bool standalone) {
    auto* peer=editor.getPeer();
    void* candidate=nullptr;
    if(peer) {
        auto* view=static_cast<NSView*>(peer->getNativeHandle());
        auto* w=[view window];
        // Refuse docked views occupying only a small portion of a host window.
        if(w && (standalone || ([w styleMask]&NSWindowStyleMaskTitled))) {
            auto bounds=[view convertRect:[view bounds] toView:[w contentView]];
            auto content=[[w contentView] bounds];
            if(standalone || (bounds.size.width>=content.size.width*0.8 && bounds.size.height>=content.size.height*0.8)) candidate=w;
        }
    }
    if(!enabled || candidate!=window) restore();
    if(!enabled || !candidate) return;
    if(!window) {
        window=candidate;
        auto* w=static_cast<NSWindow*>(window); [w retain];
        originalLevel=[w level]; originalHide=[w hidesOnDeactivate];
    }
    auto* w=static_cast<NSWindow*>(window);
    const bool active=[NSApp isActive];
    [w setHidesOnDeactivate:NO];
    // Float stays above Live only while Live is foreground. An inactive Live
    // must not pin Drum (or its send/effect children) above another app.
    const auto level=active?std::max((NSInteger)NSFloatingWindowLevel,[[w parentWindow] level]):(NSInteger)NSNormalWindowLevel;
    setFamilyLevel(w,level);
    if(active && ![w isMiniaturized] && (![w isVisible] || !lastHostActive))[w orderFront:nil];
    lastHostActive=active;
}
}
