#include "HostFloat.h"
#if JUCE_MAC
#import <AppKit/AppKit.h>
#elif JUCE_WINDOWS
#include <windows.h>
#endif
namespace djl {
void HostFloat::showAuxiliaryWindow(juce::Component& child,juce::Component* owner){
 child.setVisible(true);
#if JUCE_MAC
 if(auto* peer=child.getPeer())if(auto* w=[static_cast<NSView*>(peer->getNativeHandle()) window]){
  NSWindow* parent=nil;if(owner)if(auto* parentPeer=owner->getPeer())parent=[static_cast<NSView*>(parentPeer->getNativeHandle()) window];
  if(parent && parent!=w){if([w parentWindow]!=parent){if([w parentWindow])[[w parentWindow] removeChildWindow:w];[parent addChildWindow:w ordered:NSWindowAbove];}[w setLevel:[parent level]];}
  [w makeKeyAndOrderFront:nil];
 }
#elif JUCE_WINDOWS
 if(owner && owner->getPeer() && child.getPeer()){auto w=static_cast<HWND>(child.getPeer()->getNativeHandle()),parent=GetAncestor(static_cast<HWND>(owner->getPeer()->getNativeHandle()),GA_ROOT);if(w!=parent)SetWindowLongPtr(w,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(parent));}
#endif
 child.toFront(true);
}
void HostFloat::setHostedTitle(juce::Component& editor,const juce::String& title){
#if JUCE_MAC
 if(auto* peer=editor.getPeer()){auto* view=static_cast<NSView*>(peer->getNativeHandle());auto* w=[view window];if(w && w!=[NSApp mainWindow] && ([w styleMask]&NSWindowStyleMaskTitled)){auto size=[view convertRect:[view bounds] toView:[w contentView]].size;auto content=[[w contentView] bounds].size;if(size.width>=content.width*.8 && size.height>=content.height*.8){auto* value=[NSString stringWithUTF8String:title.toRawUTF8()];if(![[w title] isEqualToString:value])[w setTitle:value];}}}
#elif JUCE_WINDOWS
 if(auto* peer=editor.getPeer()){auto child=static_cast<HWND>(peer->getNativeHandle());auto root=GetAncestor(child,GA_ROOT);RECT view{},client{};GetClientRect(child,&view);GetClientRect(root,&client);if(GetWindow(root,GW_OWNER) && view.right>=client.right*.8 && view.bottom>=client.bottom*.8)SetWindowTextW(root,title.toWideCharPointer());}
#else
 juce::ignoreUnused(editor,title);
#endif
}
void HostFloat::showHostedEditor(juce::Component& editor,bool show){
#if JUCE_MAC
 if(auto* peer=editor.getPeer()){
  auto* view=static_cast<NSView*>(peer->getNativeHandle());auto* w=[view window];
  if(w){auto bounds=[view convertRect:[view bounds] toView:[w contentView]];auto content=[[w contentView] bounds];
   if(bounds.size.width>=content.size.width*0.8 && bounds.size.height>=content.size.height*0.8 && w!=[NSApp mainWindow]){
    if(show)[w orderFront:nil];else [w orderOut:nil];
   }
  }
 }
#else
 juce::ignoreUnused(editor,show);
#endif
}
HostFloat::~HostFloat() { restore(); }
void HostFloat::restore() {
#if JUCE_MAC
    if(auto* w=static_cast<NSWindow*>(window)) {
        [w setLevel:originalLevel]; [w setHidesOnDeactivate:originalHide]; [w release];
    }
#elif JUCE_WINDOWS
    auto w=static_cast<HWND>(window);
    if(w && IsWindow(w)) SetWindowPos(w,originalLevel?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
#endif
    window=nullptr;lastHostActive=false;
}
void HostFloat::update(juce::Component& editor,bool enabled,bool standalone) {
    auto* peer=editor.getPeer();
    void* candidate=nullptr;
#if JUCE_MAC
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
#elif JUCE_WINDOWS
    if(peer) {
        auto w=GetAncestor(static_cast<HWND>(peer->getNativeHandle()),GA_ROOT);
        RECT client{},view{};
        GetClientRect(w,&client); GetClientRect(static_cast<HWND>(peer->getNativeHandle()),&view);
        if(standalone || (GetWindow(w,GW_OWNER) && view.right>=client.right*0.8 && view.bottom>=client.bottom*0.8)) candidate=w;
    }
#endif
    if(!enabled || candidate!=window) restore();
    if(!enabled || !candidate) return;
    if(!window) {
        window=candidate;
#if JUCE_MAC
        auto* w=static_cast<NSWindow*>(window); [w retain];
        originalLevel=[w level]; originalHide=[w hidesOnDeactivate];
#elif JUCE_WINDOWS
        originalLevel=(GetWindowLongPtr(static_cast<HWND>(window),GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
#endif
    }
#if JUCE_MAC
    auto* w=static_cast<NSWindow*>(window);
    const bool active=[NSApp isActive];
    [w setHidesOnDeactivate:NO];
    // Keep one normal window level. Lowering a floating-level window when the
    // app deactivates can put it behind Live, making it appear to disappear.
    if([w level]!=NSNormalWindowLevel)[w setLevel:NSNormalWindowLevel];
    if(active && !lastHostActive)[w orderFront:nil];
    lastHostActive=active;
#elif JUCE_WINDOWS
    DWORD foregroundProcess=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foregroundProcess);
    const bool active=foregroundProcess==GetCurrentProcessId();
    auto hwnd=static_cast<HWND>(window);
    if(GetWindowLongPtr(hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST)SetWindowPos(hwnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    if(active && !lastHostActive)SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    lastHostActive=active;
#endif
}
}
