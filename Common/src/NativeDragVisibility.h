#pragma once
#if JUCE_WINDOWS
#include <windows.h>
#include <commctrl.h>
#include <vector>

// Defer only hide requests for this editor while an external drag is in flight.
// The host's requested visibility is restored when the drag finishes/cancels.
class NativeDragVisibility
{
public:
    explicit NativeDragVisibility(HWND editor)
    {
        const auto root=GetAncestor(editor,GA_ROOT);
        const auto host=GetAncestor(editor,GA_ROOTOWNER);
        for(auto hwnd=editor;hwnd;)
        {
            // Never intercept the main host window's visibility.
            if(hwnd!=editor && hwnd==host)break;
            if(SetWindowSubclass(hwnd,procedure,reinterpret_cast<UINT_PTR>(this),reinterpret_cast<DWORD_PTR>(this)))
                windows.push_back({hwnd,false});
            if(hwnd==root || !(GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_CHILD))break;
            hwnd=GetParent(hwnd);
        }
    }
    ~NativeDragVisibility()
    {
        release();
        for(const auto& window:windows)if(IsWindow(window.handle))
            RemoveWindowSubclass(window.handle,procedure,reinterpret_cast<UINT_PTR>(this));
    }
    void hold(){holding=true;}
    bool hasDeferredHide() const {for(const auto& window:windows)if(window.hideRequested)return true;return false;}
    size_t guardedWindowCount() const {return windows.size();}
    void release()
    {
        holding=false;
        for(auto& window:windows)if(window.hideRequested)
        {
            window.hideRequested=false;
            if(IsWindow(window.handle))ShowWindow(window.handle,SW_HIDE);
        }
    }
private:
    struct Window {HWND handle;bool hideRequested;};
    std::vector<Window> windows;
    bool holding=false;
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data)
    {
        auto& self=*reinterpret_cast<NativeDragVisibility*>(data);
        if(message==WM_WINDOWPOSCHANGING)
        {
            auto& position=*reinterpret_cast<WINDOWPOS*>(l);
            for(auto& window:self.windows)if(window.handle==hwnd)
            {
                if(position.flags&SWP_SHOWWINDOW)window.hideRequested=false;
                if(self.holding && (position.flags&SWP_HIDEWINDOW))
                {
                    window.hideRequested=true;
                    position.flags&=~SWP_HIDEWINDOW;
                }
                break;
            }
        }
        if(message==WM_NCDESTROY)
        {
            for(auto& window:self.windows)if(window.handle==hwnd){window.handle=nullptr;window.hideRequested=false;}
            RemoveWindowSubclass(hwnd,procedure,id);
        }
        return DefSubclassProc(hwnd,message,w,l);
    }
};
#endif
