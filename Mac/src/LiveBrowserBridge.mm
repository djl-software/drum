#include "LiveBrowserBridge.h"
#include "BrowserIdentity.h"
#include "LiveBrowserDrop.h"
#import <AppKit/AppKit.h>
#include <sqlite3.h>
#include <unistd.h>

namespace djl::mac {
void trace(const juce::String&);
DragPresentation::~DragPresentation(){end();}
void DragPresentation::begin(NSWindow* target, NSWindow* browserWindow) {
    end();if(!target || !browserWindow || target==browserWindow)return;
    NSWindow* top=target;
    while([top parentWindow] && [top parentWindow]!=browserWindow)top=[top parentWindow];
    if(top==browserWindow)return;
    root=[top retain];source=[browserWindow retain];
    originalLevel=[root level];originalHide=[root hidesOnDeactivate];
    if(![root parentWindow]){[source addChildWindow:root ordered:NSWindowAbove];attached=true;}
    update([NSEvent mouseLocation]);
}
void DragPresentation::update(NSPoint point) {
    if(root){
        // Live may reorder/promote its main window inside its nested drag loop.
        // Like the working send editor, Drum must be a native child above it.
        [root setHidesOnDeactivate:NO];
        [root setLevel:std::max(originalLevel,std::max((NSInteger)NSFloatingWindowLevel,[source level]+1))];
        [root orderWindow:NSWindowAbove relativeTo:[source windowNumber]];
        [root orderFrontRegardless];
    }
    if(feedback){
        auto cursor=[NSCursor arrowCursor];auto size=[feedback frame].size;
        auto hot=[cursor hotSpot];
        [feedback setFrameOrigin:NSMakePoint(point.x-hot.x,point.y-size.height+hot.y)];
        [feedback orderFrontRegardless];
    }
}
void DragPresentation::showFeedback(const juce::String& name,NSPoint point) {
    if(!feedback){
        auto* panel=[[NSPanel alloc] initWithContentRect:NSMakeRect(0,0,320,44)
            styleMask:NSWindowStyleMaskBorderless|NSWindowStyleMaskNonactivatingPanel
            backing:NSBackingStoreBuffered defer:NO];
        feedback=panel;[panel setReleasedWhenClosed:NO];[panel setOpaque:NO];
        [panel setBackgroundColor:[NSColor clearColor]];[panel setHasShadow:NO];
        [panel setIgnoresMouseEvents:YES];[panel setHidesOnDeactivate:NO];
        [panel setLevel:CGWindowLevelForKey(kCGDraggingWindowLevelKey)];
        auto* view=[panel contentView];
        auto cursor=[NSCursor arrowCursor];auto cursorSize=[[cursor image] size];
        auto* arrow=[[[NSImageView alloc] initWithFrame:NSMakeRect(0,44-cursorSize.height,cursorSize.width,cursorSize.height)] autorelease];
        [arrow setImage:[cursor image]];[view addSubview:arrow];
        auto* badge=[[[NSView alloc] initWithFrame:NSMakeRect(20,0,298,30)] autorelease];
        [badge setWantsLayer:YES];badge.layer.backgroundColor=[[NSColor colorWithCalibratedWhite:0.10 alpha:0.95] CGColor];badge.layer.cornerRadius=6;[view addSubview:badge];
        auto* icon=[[[NSImageView alloc] initWithFrame:NSMakeRect(6,5,20,20)] autorelease];
        [icon setImage:[NSImage imageNamed:NSImageNameMultipleDocuments]];[badge addSubview:icon];
        auto* label=[NSTextField labelWithString:[NSString stringWithUTF8String:name.toRawUTF8()]];
        [label setFrame:NSMakeRect(33,5,258,20)];[label setTextColor:[NSColor whiteColor]];
        [label setFont:[NSFont systemFontOfSize:12 weight:NSFontWeightMedium]];
        [label setLineBreakMode:NSLineBreakByTruncatingTail];[badge addSubview:label];
        [[NSCursor arrowCursor] set];
    }
    update(point);
}
void DragPresentation::end(){
    if(feedback){[feedback orderOut:nil];[feedback release];feedback=nil;}
    if(root){if(attached && [root parentWindow]==source)[source removeChildWindow:root];
        [root setLevel:originalLevel];[root setHidesOnDeactivate:originalHide];[root release];root=nil;}
    [source release];source=nil;attached=false;
}
void trace(const juce::String& message) {
    auto file=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("DJL/Drum/native-drag-mac.log");
    file.getParentDirectory().createDirectory();if(file.getSize()>65536)file.replaceWithText("");
    file.appendText(message+"\n");
}
id attribute(id element,NSString* key) {
    // Live versions expose a mixture of legacy and modern native AX objects.
    // Stay inside the host process; no cross-process AX permission is needed.
    @try {
        if(element==NSApp && [key isEqualToString:NSAccessibilityChildrenAttribute])return [NSApp windows];
        id value=nil;if([element respondsToSelector:@selector(accessibilityAttributeValue:)])value=[element accessibilityAttributeValue:key];if(value)return value;
        if([key isEqualToString:NSAccessibilityChildrenAttribute]){
            if([element respondsToSelector:@selector(accessibilityChildren)])return [element accessibilityChildren];
        }
        if([key isEqualToString:NSAccessibilityParentAttribute] && [element respondsToSelector:@selector(accessibilityParent)])return [element accessibilityParent];
        if([key isEqualToString:NSAccessibilityIdentifierAttribute] && [element respondsToSelector:@selector(accessibilityIdentifier)])return [element accessibilityIdentifier];
        if([key isEqualToString:NSAccessibilityRoleAttribute] && [element respondsToSelector:@selector(accessibilityRole)])return [element accessibilityRole];
        if([key isEqualToString:NSAccessibilityDescriptionAttribute] && [element respondsToSelector:@selector(accessibilityLabel)])return [element accessibilityLabel];
        if([key isEqualToString:NSAccessibilityValueAttribute] && [element respondsToSelector:@selector(accessibilityValue)])return [element accessibilityValue];
        if([key isEqualToString:NSAccessibilityTitleAttribute] && [element respondsToSelector:@selector(accessibilityTitle)])return [element accessibilityTitle];
        if([key isEqualToString:NSAccessibilitySelectedRowsAttribute] && [element respondsToSelector:@selector(accessibilitySelectedRows)])return [element accessibilitySelectedRows];
        if([element respondsToSelector:@selector(accessibilityFrame)]){
            auto frame=[element accessibilityFrame];
            if([key isEqualToString:NSAccessibilityPositionAttribute])return [NSValue valueWithPoint:frame.origin];
            if([key isEqualToString:NSAccessibilitySizeAttribute])return [NSValue valueWithSize:frame.size];
        }
    } @catch(NSException*) {}
    return nil;
}
juce::String textAttribute(id element,NSString* key) {
    auto value=attribute(element,key);
    return [value isKindOfClass:[NSString class]]?juce::String::fromUTF8([value UTF8String]):juce::String{};
}
juce::String rowText(id element,int depth=0) {
    for(auto key:{NSAccessibilityValueAttribute,NSAccessibilityTitleAttribute})if(auto value=textAttribute(element,key);value.isNotEmpty())return value;
    if(depth>3)return {};auto children=attribute(element,NSAccessibilityChildrenAttribute);
    if([children isKindOfClass:[NSArray class]])for(id child in children)
        if(auto result=rowText(child,depth+1);result.isNotEmpty())return result;
    return {};
}
juce::String selectedBrowserRow(id element,NSPoint point,int& budget,bool browser) {
    if(--budget<0)return {};
    auto identifier=textAttribute(element,NSAccessibilityIdentifierAttribute);
    browser|=identifier=="ContentBrowser";
    if(browser && textAttribute(element,NSAccessibilityRoleAttribute)=="AXOutline"
        && !textAttribute(element,NSAccessibilityDescriptionAttribute).containsIgnoreCase("sidebar")) {
        auto position=attribute(element,NSAccessibilityPositionAttribute),size=attribute(element,NSAccessibilitySizeAttribute);
        if([position isKindOfClass:[NSValue class]] && [size isKindOfClass:[NSValue class]]) {
            auto p=[position pointValue];auto s=[size sizeValue];
            if(NSPointInRect(point,NSMakeRect(p.x,p.y,s.width,s.height))) {
                auto selected=attribute(element,NSAccessibilitySelectedRowsAttribute);
                if([selected isKindOfClass:[NSArray class]] && [selected count]==1)return rowText([selected objectAtIndex:0]);
            }
        }
    }
    auto children=attribute(element,NSAccessibilityChildrenAttribute);
    if([children isKindOfClass:[NSArray class]])for(id child in children) {
        if(budget<=0)break;
        if(auto result=selectedBrowserRow(child,point,budget,browser);result.isNotEmpty())return result;
    }
    return {};
}
// Live exposes virtual browser nodes through hit testing even when its main
// NSView does not enumerate them via accessibilityChildren.
juce::String browserRowFromHit(id element) {
    juce::String name;bool contentList=false,single=false;
    for(int depth=0;element && depth<32;++depth) {
        auto role=textAttribute(element,NSAccessibilityRoleAttribute);
        auto identifier=textAttribute(element,NSAccessibilityIdentifierAttribute);
        if(role=="AXRow" && name.isEmpty())name=rowText(element);
        if(role=="AXOutline") {
            contentList=!textAttribute(element,NSAccessibilityDescriptionAttribute).containsIgnoreCase("sidebar");
            id selected=attribute(element,NSAccessibilitySelectedRowsAttribute);
            single=[selected isKindOfClass:[NSArray class]] && [selected count]==1;
            if(single && name.isEmpty())name=rowText([selected objectAtIndex:0]);
        }
        if(identifier=="ContentBrowser" && contentList && single)return name;
        id parent=attribute(element,NSAccessibilityParentAttribute);if(parent==element)break;element=parent;
    }
    return {};
}
bool isCancellationEvent(NSEvent* event,NSEvent* cancellation) {
    // AppKit/Live may copy a posted event, so pointer identity is insufficient.
    return cancellation && event.type==NSEventTypeKeyDown && event.keyCode==53
        && std::abs(event.timestamp-cancellation.timestamp)<0.001;
}
juce::String browserHit(NSPoint point) {
    for(NSWindow* window in [NSApp orderedWindows]) {
        if(![window isVisible] || !NSPointInRect(point,[window frame]))continue;
        @try {
            return browserRowFromHit([[window contentView] accessibilityHitTest:point]);
        } @catch(NSException*) {}
        // Only the frontmost host window under the starting point is a source.
        break;
    }
    return {};
}
juce::var resolveEffect(const juce::String& name) {
    auto file=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/Ableton/Live Database/Live-plugins-1.db");
    sqlite3* db=nullptr;sqlite3_stmt* statement=nullptr;juce::Array<juce::var> rows;
    if(sqlite3_open_v2(file.getFullPathName().toRawUTF8(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK) {
        sqlite3_busy_timeout(db,20);
        const char* sql="SELECT DISTINCT p.name,p.vendor,p.dev_identifier,m.path FROM plugins p JOIN plugin_modules m USING(module_id) WHERE p.name=?1 COLLATE NOCASE AND p.dev_identifier LIKE 'device:vst3:audiofx:%' AND p.enabled=1";
        if(sqlite3_prepare_v2(db,sql,-1,&statement,nullptr)==SQLITE_OK) {
            sqlite3_bind_text(statement,1,name.toRawUTF8(),-1,SQLITE_TRANSIENT);
            while(sqlite3_step(statement)==SQLITE_ROW && rows.size()<32) {
                auto text=[&](int c){return juce::String::fromUTF8((const char*)sqlite3_column_text(statement,c));};
                auto path=text(3);if(!juce::File::isAbsolutePath(path) || !juce::File(path).hasFileExtension("vst3") || !juce::File(path).exists())continue;
                auto* row=new juce::DynamicObject();row->setProperty("name",text(0));row->setProperty("vendor",text(1));row->setProperty("id",text(2));row->setProperty("path",path);rows.add(juce::var(row));
            }
        }
    }
    if(statement)sqlite3_finalize(statement);if(db)sqlite3_close(db);
    auto* result=new juce::DynamicObject();result->setProperty("matches",rows);
    // A unique enabled VST3 effect can be inserted directly. Ambiguous names
    // and names shared with native Live devices still show the chooser.
    result->setProperty("requiresConfirmation",rows.size()!=1 || drum::knownLiveDeviceName(name));result->setProperty("nativeNameClash",drum::knownLiveDeviceName(name));
    if(rows.isEmpty())result->setProperty("error","No enabled VST3 audio effect matches this Live browser item. Use Load VST3 for a plug-in bundle.");
    return juce::var(result);
}
NSWindow* nativeWindow(juce::Component& owner) {
    if(auto* peer=owner.getPeer())return [(NSView*)peer->getNativeHandle() window];return nil;
}
}
using namespace djl::mac;
struct LiveBrowserBridge::Impl {
    LiveBrowserBridge& bridge;id monitor=nil;NSTimer* timer=nil;NSPoint origin{};
    CFMachPortRef tap=nullptr;CFRunLoopSourceRef tapSource=nullptr;bool tapPressed=false;
    juce::String name,context,dragContext;bool pressed=false,dragging=false,wasVisible=false,claimed=false;
    DragPresentation presentation;NSWindow* source=nil;
    NSWindow* target=nil;NSEvent* cancellation=nil;NSInteger originalLevel=NSNormalWindowLevel;bool originalHide=false;double started=0;
    explicit Impl(LiveBrowserBridge& b):bridge(b) {
        trace("Receiver attached: "+juce::String::fromUTF8([[[NSBundle mainBundle] bundleIdentifier] UTF8String]));
        monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown|NSEventMaskKeyDown handler:^NSEvent*(NSEvent* event){
            if(event.type==NSEventTypeLeftMouseDown)begin(event.window?[event.window convertPointToScreen:event.locationInWindow]:[NSEvent mouseLocation]);
            if(event.type==NSEventTypeKeyDown && event.keyCode==53 && !isCancellationEvent(event,cancellation)){finish(false);}
            return event;
        }];
        // JUCE timers/local event monitors can be suspended by Live's internal
        // drag tracking. This native timer also runs in AppKit's tracking mode.
        timer=[[NSTimer timerWithTimeInterval:1.0/60.0 repeats:YES block:^(NSTimer*){tick();}] retain];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSEventTrackingRunLoopMode];
        // Observe only mouse events routed to this host process, including
        // those consumed by its own tracking loop. No global/keyboard tap.
        tap=CGEventTapCreateForPid(getpid(),kCGHeadInsertEventTap,kCGEventTapOptionListenOnly,
            CGEventMaskBit(kCGEventLeftMouseDown)|CGEventMaskBit(kCGEventLeftMouseDragged)|CGEventMaskBit(kCGEventLeftMouseUp),
            [](CGEventTapProxy,CGEventType type,CGEventRef event,void* context)->CGEventRef {
                auto& self=*static_cast<Impl*>(context);
                if(type==kCGEventTapDisabledByTimeout || type==kCGEventTapDisabledByUserInput){if(self.tap)CGEventTapEnable(self.tap,true);return event;}
                auto p=CGEventGetLocation(event);auto point=NSMakePoint(p.x,NSMaxY([[[NSScreen screens] objectAtIndex:0] frame])-p.y);
                if(type==kCGEventLeftMouseDown){self.begin(point);self.tapPressed=true;}
                if(type==kCGEventLeftMouseDragged)self.tickAt(true,point);
                if(type==kCGEventLeftMouseUp){self.tapPressed=false;self.tickAt(false,point);}
                return event;
            },this);
        if(tap){tapSource=CFMachPortCreateRunLoopSource(kCFAllocatorDefault,tap,0);CFRunLoopAddSource(CFRunLoopGetMain(),tapSource,kCFRunLoopCommonModes);CFRunLoopAddSource(CFRunLoopGetMain(),tapSource,(CFStringRef)NSEventTrackingRunLoopMode);}
        trace(tap?"Host mouse receiver active":"Host mouse receiver unavailable; using AppKit tracking");
    }
    ~Impl(){if(tapSource){CFRunLoopRemoveSource(CFRunLoopGetMain(),tapSource,kCFRunLoopCommonModes);CFRunLoopRemoveSource(CFRunLoopGetMain(),tapSource,(CFStringRef)NSEventTrackingRunLoopMode);CFRelease(tapSource);}if(tap){CFMachPortInvalidate(tap);CFRelease(tap);}[timer invalidate];[timer release];if(monitor)[NSEvent removeMonitor:monitor];releaseTarget();}
    void releaseTarget(){presentation.end();[source release];source=nil;[cancellation release];cancellation=nil;if(target){[target setHidesOnDeactivate:originalHide];[target setLevel:originalLevel];[target release];target=nil;}}
    void begin(NSPoint point){
        releaseTarget();pressed=true;dragging=false;claimed=false;name={};origin=point;
        wasVisible=bridge.owner.isShowing();started=juce::Time::getMillisecondCounterHiRes();dragContext=context;
    }
    void finish(bool drop){
        auto value=drop?resolveEffect(name):juce::var{};
        if(drop){trace("Accepted browser drop: "+name);value.getDynamicObject()->setProperty("context",dragContext);}
        pressed=false;dragging=false;claimed=false;name={};releaseTarget();
        if(drop){auto callback=bridge.onDrop;juce::MessageManager::callAsync([safe=juce::Component::SafePointer<juce::Component>(&bridge.owner),callback,value]{if(safe && callback)callback(value);});}
    }
    void tick(){tickAt(tapPressed || CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState,kCGMouseButtonLeft),[NSEvent mouseLocation]);}
    void tickAt(bool down,NSPoint point){
        if(down && !pressed && !tap)begin(point);
        if(!pressed)return;
        if(juce::Time::getMillisecondCounterHiRes()-started>30000 || dragContext!=context){finish(false);return;}
        if(down && !dragging && wasVisible && std::hypot(point.x-origin.x,point.y-origin.y)>5){
            int budget=1600;name=browserHit(origin);
            if(name.isEmpty())name=selectedBrowserRow(NSApp,origin,budget);dragging=true;
            trace("Browser tracking: "+name+" / visited "+juce::String(1600-budget));
            if(name.isNotEmpty())if(auto* window=nativeWindow(bridge.owner)){
                target=[window retain];originalLevel=[window level];originalHide=[window hidesOnDeactivate];
                for(NSWindow* candidate in [NSApp orderedWindows])if([candidate isVisible] && NSPointInRect(origin,[candidate frame])){source=[candidate retain];break;}
                presentation.begin(target,source);
            }
        }
        if(target && down){
            [target setHidesOnDeactivate:NO];[target setLevel:std::max(originalLevel,(NSInteger)NSFloatingWindowLevel)];
            if(![target isVisible])[target orderFront:nil];
            presentation.update(point);
            const bool inside=contains(point);
            if(inside && !claimed){
                // Live's browser drag is internal, not an OS file drag. Cancel
                // its insertion before release, then own only this drop gesture.
                // Otherwise Live also loads Valhalla onto the underlying track.
                auto* event=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0
                    timestamp:[NSProcessInfo processInfo].systemUptime windowNumber:[NSApp keyWindow].windowNumber
                    context:nil characters:@"\033" charactersIgnoringModifiers:@"\033" isARepeat:NO keyCode:53];
                presentation.showFeedback(name,point);
                claimed=true;cancellation=[event retain];[NSApp postEvent:event atStart:YES];trace("Claimed Drum chain drop; cancelled Live insertion");
            }
            if(claimed)presentation.update(point);
        }
        if(!down){
            const bool drop=dragging && claimed && name.isNotEmpty() && contains(point);
            finish(drop);
        }
    }
    bool contains(NSPoint point) const {
        auto* window=nativeWindow(bridge.owner);
        if(!window || ![window isVisible])return false;
        auto front=[NSWindow windowNumberAtPoint:point belowWindowWithWindowNumber:0];
        if(auto* feedback=presentation.feedbackWindow();feedback && front==feedback.windowNumber)
            front=[NSWindow windowNumberAtPoint:point belowWindowWithWindowNumber:feedback.windowNumber];
        if(front!=window.windowNumber)return false;
        auto top=NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);
        return bridge.owner.getScreenBounds().contains(juce::Point<int>{juce::roundToInt(point.x),juce::roundToInt(top-point.y)});
    }
    void poll(const juce::String& next){context=next;}
};
LiveBrowserBridge::LiveBrowserBridge(juce::Component& c):owner(c){trace("Chain bridge created");}
LiveBrowserBridge::~LiveBrowserBridge()=default;
void LiveBrowserBridge::poll(const juce::String& context,juce::Rectangle<int> area) {
    juce::ignoreUnused(area);
    if(![[[NSBundle mainBundle] bundleIdentifier] hasPrefix:@"com.ableton.live"])return;
    if(!impl && owner.isShowing())impl=std::make_unique<Impl>(*this);
    if(impl)impl->poll(context);
}
