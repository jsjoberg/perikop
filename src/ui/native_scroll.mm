#include "ui/native_scroll.hpp"
#import <Cocoa/Cocoa.h>
namespace ortho {
void* install_native_scroll(wxWindow* window,std::function<void(double)> scroll) {
    NSView* view=static_cast<NSView*>(window->GetHandle());
    id monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel handler:^NSEvent*(NSEvent* event){
        if([event window]!=[view window]||[view isHiddenOrHasHiddenAncestor])return event;
        const NSPoint point=[view convertPoint:[event locationInWindow] fromView:nil];
        if(!NSPointInRect(point,[view bounds]))return event;
        const double delta=[event hasPreciseScrollingDeltas]?[event scrollingDeltaY]:[event scrollingDeltaY]*10;
        if(delta!=0)scroll(-delta);
        return nil;
    }];
    return [monitor retain];
}
void remove_native_scroll(void* token){if(token){id monitor=static_cast<id>(token);[NSEvent removeMonitor:monitor];[monitor release];}}
}
