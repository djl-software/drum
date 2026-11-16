#include "platform/HostFloat.h"
#include "../src/EditorPresentation.h"
#import <AppKit/AppKit.h>
#include "../src/LiveBrowserDrop.h"
#include <stdexcept>
#include <iostream>

@interface DrumAXFixture : NSObject
@property(retain) NSDictionary* attributes;
@end
@implementation DrumAXFixture
- (id)accessibilityAttributeValue:(NSString*)key{return [self.attributes objectForKey:key];}
- (void)dealloc{[_attributes release];[super dealloc];}
@end

void runMacWindowTests() {
    @autoreleasepool {
        auto node=[](NSDictionary* values){auto* n=[[[DrumAXFixture alloc] init] autorelease];n.attributes=values;return n;};
        auto* row=node(@{NSAccessibilityValueAttribute:@"ValhallaVintageVerb"});
        auto* list=node(@{NSAccessibilityRoleAttribute:@"AXOutline",NSAccessibilityDescriptionAttribute:@"Plug-Ins List",
            NSAccessibilityPositionAttribute:[NSValue valueWithPoint:NSMakePoint(20,20)],
            NSAccessibilitySizeAttribute:[NSValue valueWithSize:NSMakeSize(200,400)],NSAccessibilitySelectedRowsAttribute:@[row]});
        auto* browser=node(@{NSAccessibilityIdentifierAttribute:@"ContentBrowser",NSAccessibilityChildrenAttribute:@[list]});
        auto* root=node(@{NSAccessibilityChildrenAttribute:@[browser]});
        int budget=100;
        if(djl::mac::selectedBrowserRow(root,NSMakePoint(40,40),budget)!="ValhallaVintageVerb")throw std::runtime_error("Mac browser selection could not identify the source row");
        budget=100;if(djl::mac::selectedBrowserRow(root,NSMakePoint(400,400),budget).isNotEmpty())throw std::runtime_error("A drag outside the browser reused its selection");
        budget=0;if(djl::mac::selectedBrowserRow(root,NSMakePoint(40,40),budget).isNotEmpty())throw std::runtime_error("Mac browser traversal exceeded its budget");
        row.attributes=@{NSAccessibilityRoleAttribute:@"AXRow",NSAccessibilityValueAttribute:@"ValhallaVintageVerb",NSAccessibilityParentAttribute:list};
        list.attributes=@{NSAccessibilityRoleAttribute:@"AXOutline",NSAccessibilityDescriptionAttribute:@"All List",NSAccessibilitySelectedRowsAttribute:@[row],NSAccessibilityParentAttribute:browser};
        if(djl::mac::browserRowFromHit(row)!="ValhallaVintageVerb" || djl::mac::browserRowFromHit(list)!="ValhallaVintageVerb")throw std::runtime_error("Live virtual browser hit/outline lost its selected effect");
        list.attributes=@{NSAccessibilityRoleAttribute:@"AXOutline",NSAccessibilityDescriptionAttribute:@"Browser Sidebar",NSAccessibilitySelectedRowsAttribute:@[row],NSAccessibilityParentAttribute:browser};
        if(djl::mac::browserRowFromHit(row).isNotEmpty())throw std::runtime_error("Browser sidebar accepted as an effect source");
        list.attributes=@{NSAccessibilityRoleAttribute:@"AXOutline",NSAccessibilitySelectedRowsAttribute:@[row,row],NSAccessibilityParentAttribute:browser};
        if(djl::mac::browserRowFromHit(row).isNotEmpty())throw std::runtime_error("Multiple browser selections accepted as one effect");
        // Break the fixture's parent/child retain cycles.
        row.attributes=@{};list.attributes=@{};
        auto escape=[](double stamp){return [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:stamp windowNumber:0 context:nil characters:@"\033" charactersIgnoringModifiers:@"\033" isARepeat:NO keyCode:53];};
        auto generated=escape(100.0),copy=escape(100.0),user=escape(100.1);
        if(!djl::mac::isCancellationEvent(copy,generated) || djl::mac::isCancellationEvent(user,generated) || djl::mac::isCancellationEvent(copy,nil))throw std::runtime_error("Copied host cancellation was confused with a user Escape");

    }

    class Window final:public juce::DocumentWindow {
    public:Window():DocumentWindow("Drum window ordering test",juce::Colours::black,closeButton){setUsingNativeTitleBar(true);centreWithSize(420,280);}
        void closeButtonPressed() override{setVisible(false);}
    };
    auto native=[](juce::Component& c){return [(NSView*)c.getPeer()->getNativeHandle() window];};
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    Window parent,child,grandchild;juce::Component owner,nested;
    parent.setContentNonOwned(&owner,false);owner.setBounds(0,0,420,280);owner.addAndMakeVisible(nested);nested.setBounds(10,10,100,100);
    parent.setVisible(true);auto* p=native(parent);[p setLevel:NSFloatingWindowLevel];
    djl::HostFloat::showAuxiliaryWindow(child,&nested);auto* c=native(child);
    check([c parentWindow]==p,"Auxiliary window is not attached to its actual parent");
    check([c level]>=[p level] && [c isVisible],"Auxiliary window opened beneath its parent");
    djl::HostFloat::showAuxiliaryWindow(grandchild,&child);auto* g=native(grandchild);
    check([g parentWindow]==c && [g level]>=[c level],"Nested plug-in window opened beneath its parent");
    child.setVisible(false);djl::HostFloat::showAuxiliaryWindow(child,&nested);
    check([c parentWindow]==p && [c isVisible],"Reopened send window lost its parent or visibility");
    auto* windows=[NSApp orderedWindows];
    check([windows indexOfObject:c]<[windows indexOfObject:p],"Send editor is behind the main window in native ordering");
    {
        djl::HostFloat floating;
        [p setLevel:NSNormalWindowLevel];floating.update(owner,true,true);
        check([p level]==([NSApp isActive]?NSFloatingWindowLevel:NSNormalWindowLevel),"Float did not follow host activation");
        [p orderOut:nil];floating.update(owner,true,true);
        check((bool)[p isVisible]==(bool)[NSApp isActive],"Float raised itself over another app or failed to recover in its active host");
        [p orderFront:nil];[NSApp deactivate];juce::MessageManager::getInstance()->runDispatchLoopUntil(50);floating.update(owner,true,true);
        check([p level]==NSNormalWindowLevel && [c level]==NSNormalWindowLevel && [g level]==NSNormalWindowLevel,"Inactive Float or its children remain pinned above other apps");
        floating.update(owner,false,true);
        check([p level]==NSNormalWindowLevel,"Float did not restore the original window level");
    }
    {
        parent.setSize(980,720);[p orderFront:nil];djl::EditorPlacement placement;
        check(djl::captureEditorPlacement(owner,placement),"Editor placement capture failed");const auto frame=[p frame];
        owner.setSize(850,650);[p setFrameOrigin:NSMakePoint(frame.origin.x+60,frame.origin.y+50)];
        check(djl::restoreEditorPlacement(owner,placement),"Editor placement restore failed");
        check(owner.getWidth()==placement.width && owner.getHeight()==placement.height && std::abs([p frame].origin.x-frame.origin.x)<1 && std::abs([p frame].origin.y-frame.origin.y)<1,"Reopening changed editor size or position");
        djl::EditorPresentation saved,recalled;saved.hosted=placement;saved.floating=placement;saved.mixer=true;saved.group="fixture-group";juce::XmlElement xml("DrumSession");saved.save(xml);recalled.restore(xml);
        check(recalled.mixer && recalled.group==saved.group && recalled.hosted.width==placement.width && recalled.floating.x==placement.x,"Mixer presentation did not survive host-state recall");
        [p orderOut:nil];check(!djl::captureEditorPlacement(owner,placement),"A hidden host shell overwrote the remembered placement");[p orderFront:nil];
    }
    {
        Window host;host.setVisible(true);auto* h=native(host);
        auto level=[p level];auto hidden=[p hidesOnDeactivate];
        djl::mac::DragPresentation drag;drag.begin(c,h);
        check([p parentWindow]==h && [c parentWindow]==p,"Browser drag did not preserve the editor/send family above Live");
        [h setLevel:NSFloatingWindowLevel+2];[h orderFront:nil];drag.update(NSMakePoint(300,400));
        check([p level]>=[h level],"Host promotion covered Drum during drag tracking");
        windows=[NSApp orderedWindows];
        check([windows indexOfObject:p]<[windows indexOfObject:h],"Browser host reordered in front of Drum during a drag");
        drag.showFeedback("ValhallaVintageVerb",NSMakePoint(300,400));
        auto* feedback=drag.feedbackWindow();auto initial=[feedback frame].origin;
        check([feedback isVisible] && [feedback ignoresMouseEvents],"Claimed drag has no mouse-transparent pointer/file feedback");
        drag.update(NSMakePoint(340,420));auto moved=[feedback frame].origin;
        check(std::abs(moved.x-initial.x-40)<.01 && std::abs(moved.y-initial.y-20)<.01,"Drag feedback did not follow the pointer");
        drag.end();
        check(!drag.feedbackWindow() && ![p parentWindow] && [c parentWindow]==p,"Drag cleanup lost the send relationship or retained feedback");
        check([p level]==level && [p hidesOnDeactivate]==hidden,"Drag cleanup changed the editor's normal presentation");
    }
    parent.clearContentComponent();
    std::cout<<"Mac auxiliary and nested window ordering passed\n";
}
