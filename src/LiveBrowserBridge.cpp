#include "LiveBrowserBridge.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <shellapi.h>
using Microsoft::WRL::ComPtr;
namespace {
juce::var resolveLiveEffect(const juce::String& name)
{
    // Windows' SQLite implementation, read-only; no external process or scan.
    juce::DynamicLibrary sqlite;
    auto path=juce::File::getSpecialLocation(juce::File::windowsSystemDirectory).getChildFile("winsqlite3.dll");
    if(!sqlite.open(path.getFullPathName()))return {};
    auto open=reinterpret_cast<int(__cdecl*)(const char*,void**,int,const char*)>(sqlite.getFunction("sqlite3_open_v2"));
    auto close=reinterpret_cast<int(__cdecl*)(void*)>(sqlite.getFunction("sqlite3_close"));
    auto prepare=reinterpret_cast<int(__cdecl*)(void*,const char*,int,void**,const char**)>(sqlite.getFunction("sqlite3_prepare_v2"));
    auto bind=reinterpret_cast<int(__cdecl*)(void*,int,const char*,int,void(*)(void*))>(sqlite.getFunction("sqlite3_bind_text"));
    auto step=reinterpret_cast<int(__cdecl*)(void*)>(sqlite.getFunction("sqlite3_step"));
    auto column=reinterpret_cast<const unsigned char*(__cdecl*)(void*,int)>(sqlite.getFunction("sqlite3_column_text"));
    auto finish=reinterpret_cast<int(__cdecl*)(void*)>(sqlite.getFunction("sqlite3_finalize"));
    if(!open||!close||!prepare||!bind||!step||!column||!finish)return {};
    auto database=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getParentDirectory().getChildFile("Local/Ableton/Live Database/Live-plugins-1.db");
    void* db=nullptr;void* statement=nullptr;juce::Array<juce::var> rows;
    if(open(database.getFullPathName().toRawUTF8(),&db,1,nullptr)==0)
    {
        const char* sql="SELECT DISTINCT p.name,p.vendor,p.dev_identifier,m.path FROM plugins p JOIN plugin_modules m USING(module_id) WHERE p.name=?1 COLLATE NOCASE AND p.dev_identifier LIKE 'device:vst3:audiofx:%' AND p.enabled=1";
        if(prepare(db,sql,-1,&statement,nullptr)==0)
        {
            bind(statement,1,name.toRawUTF8(),-1,nullptr);
            while(step(statement)==100 && rows.size()<32)
            {
                auto text=[&](int c){auto* value=column(statement,c);return value?juce::String::fromUTF8(reinterpret_cast<const char*>(value)):juce::String{};};
                auto file=text(3);if(!juce::File::isAbsolutePath(file)||!juce::File(file).hasFileExtension("vst3")||!juce::File(file).exists())continue;
                auto* row=new juce::DynamicObject();row->setProperty("name",text(0));row->setProperty("vendor",text(1));row->setProperty("id",text(2));row->setProperty("path",file);rows.add(juce::var(row));
            }
        }
    }
    if(statement)finish(statement);if(db)close(db);
    auto* result=new juce::DynamicObject();result->setProperty("matches",rows);return juce::var(result);
}
}
struct LiveBrowserBridge::Impl final : juce::Thread, IDropTarget
{
    LiveBrowserBridge& bridge;HWND window=nullptr,parent=nullptr;HHOOK hook=nullptr;
    juce::CriticalSection lock;POINT point{};uint64_t generation=0,work=0,completed=0,pendingDrop=0;
    juce::String context,pendingContext;juce::var result;std::atomic<bool> enabled {false};std::atomic<ULONG> references {1};
    double pressedAt=0;bool dragAccepted=false;uint64_t expired=0;
    inline static std::vector<Impl*> receivers;
    Impl(LiveBrowserBridge& b,HWND p):Thread("Drum browser identity"),bridge(b),parent(p)
    {
        window=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Drop Live VST3 effects here",WS_CHILD|WS_BORDER|SS_CENTER|SS_CENTERIMAGE|SS_NOTIFY,0,0,1,1,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
        // A fixed child receives OLE immediately, without covering any card.
        const auto registration=window?RegisterDragDrop(window,this):E_FAIL;
        if(window && FAILED(registration))SetWindowTextW(window,L"Browser drop registration failed - use Load VST3");
        receivers.push_back(this);
        DWORD thread=GetWindowThreadProcessId(GetAncestor(parent,GA_ROOTOWNER),nullptr);
        hook=SetWindowsHookExW(WH_MOUSE,mouse,nullptr,thread);startThread();
        if(window && !hook)SetWindowTextW(window,L"Browser drag tracking unavailable - use Load VST3");
    }
    ~Impl() override
    {
        if(hook)UnhookWindowsHookEx(hook);std::erase(receivers,this);
        signalThreadShouldExit();notify();waitForThreadToExit(-1);
        if(window){RevokeDragDrop(window);DestroyWindow(window);}
    }
    static LRESULT CALLBACK mouse(int code,WPARAM message,LPARAM data)
    {
        if(code>=0 && message==WM_LBUTTONDOWN)
        {
            const auto pt=reinterpret_cast<MOUSEHOOKSTRUCT*>(data)->pt;
            // Hooks are local to Live's UI thread, never global input hooks.
            for(auto* receiver:receivers)if(receiver->enabled)
            {const juce::ScopedLock guard(receiver->lock);receiver->point=pt;++receiver->generation;receiver->pressedAt=juce::Time::getMillisecondCounterHiRes();receiver->pendingDrop=0;receiver->notify();}
        }
        return CallNextHookEx(nullptr,code,message,data);
    }
    void run() override
    {
        const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        ComPtr<IUIAutomation> automation;CoCreateInstance(__uuidof(CUIAutomation8),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation));
        if(automation){ComPtr<IUIAutomation6> modern;if(SUCCEEDED(automation.As(&modern))){modern->put_ConnectionTimeout(500);modern->put_TransactionTimeout(500);}}
        while(!threadShouldExit())
        {
            POINT origin{};uint64_t current=0;
            {const juce::ScopedLock guard(lock);if(work!=generation){current=generation;work=current;origin=point;}}
            if(!current){wait(50);continue;}
            juce::String name;bool browser=false;
            if(automation)
            {
                ComPtr<IUIAutomationElement> element;automation->ElementFromPoint(origin,&element);
                ComPtr<IUIAutomationTreeWalker> walker;automation->get_ControlViewWalker(&walker);
                for(int depth=0;element && walker && depth<24;++depth)
                {
                    int pid=0;element->get_CurrentProcessId(&pid);if(pid!=(int)GetCurrentProcessId())break;
                    CONTROLTYPEID type=0;element->get_CurrentControlType(&type);
                    if(name.isEmpty() && (type==UIA_TreeItemControlTypeId||type==UIA_DataItemControlTypeId||type==UIA_ListItemControlTypeId))
                    {BSTR text=nullptr;element->get_CurrentName(&text);if(text){name=juce::String(text);SysFreeString(text);}}
                    BSTR id=nullptr;element->get_CurrentAutomationId(&id);if(id){browser=juce::String(id)=="ContentBrowser";SysFreeString(id);}if(browser)break;
                    ComPtr<IUIAutomationElement> next;walker->GetParentElement(element.Get(),&next);element=next;
                }
            }
            auto found=browser && name.isNotEmpty()?resolveLiveEffect(name):juce::var{};
            {const juce::ScopedLock guard(lock);if(current==generation){result=found;completed=current;}}
        }
        automation.Reset();if(SUCCEEDED(hr))CoUninitialize();
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override
    {if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IDropTarget){*out=static_cast<IDropTarget*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++references;}
    ULONG STDMETHODCALLTYPE Release() override{return --references;}
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data,DWORD,POINTL,DWORD* effect) override
    {
        ComPtr<IEnumFORMATETC> formats;FORMATETC format{};ULONG fetched=0;
        bool hasFormats=data && SUCCEEDED(data->EnumFormatEtc(DATADIR_GET,&formats)) && formats && formats->Next(1,&format,&fetched)==S_OK;
        if(hasFormats && format.ptd)CoTaskMemFree(format.ptd);
        const juce::ScopedLock guard(lock);
        dragAccepted=enabled && !hasFormats && generation>expired && juce::Time::getMillisecondCounterHiRes()-pressedAt<20000;
        *effect=dragAccepted?DROPEFFECT_COPY:DROPEFFECT_NONE;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL,DWORD* effect) override{*effect=enabled && dragAccepted?DROPEFFECT_COPY:DROPEFFECT_NONE;return S_OK;}
    HRESULT STDMETHODCALLTYPE DragLeave() override{return S_OK;}
    HRESULT STDMETHODCALLTYPE Drop(IDataObject*,DWORD,POINTL,DWORD* effect) override
    {
        const juce::ScopedLock guard(lock);*effect=enabled && dragAccepted?DROPEFFECT_COPY:DROPEFFECT_NONE;
        if(enabled && dragAccepted){pendingDrop=generation;pendingContext=context;}dragAccepted=false;return S_OK;
    }
    void poll(const juce::String& next,juce::Rectangle<int> bounds,bool visible)
    {
        juce::var delivered;
        {
            const juce::ScopedLock guard(lock);
            if(context!=next || !visible){pendingDrop=0;result=juce::var{};}
            context=next;enabled=visible;
            if(!pendingDrop && !(GetAsyncKeyState(VK_LBUTTON)&0x8000) && juce::Time::getMillisecondCounterHiRes()-pressedAt>300)expired=generation;
            if(pendingDrop && completed==pendingDrop){if(pendingContext==context)delivered=result;pendingDrop=0;result=juce::var{};}
        }
        if(window)
        {
            SetWindowPos(window,nullptr,bounds.getX(),bounds.getY(),bounds.getWidth(),bounds.getHeight(),SWP_NOACTIVATE|SWP_NOZORDER|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
        }
        if(!delivered.isVoid() && bridge.onDrop)bridge.onDrop(delivered);
    }
};
#else
struct LiveBrowserBridge::Impl {};
#endif
LiveBrowserBridge::LiveBrowserBridge(juce::Component& c):owner(c){}
LiveBrowserBridge::~LiveBrowserBridge()=default;
void LiveBrowserBridge::poll(const juce::String& context,juce::Rectangle<int> area)
{
#if JUCE_WINDOWS
    auto* peer=owner.getPeer();if(!peer){impl.reset();return;}
    auto parent=static_cast<HWND>(peer->getNativeHandle());
    if(impl && impl->parent!=parent)impl.reset();
    if(!impl && owner.isShowing())
    {
        wchar_t host[32768]{};GetModuleFileNameW(nullptr,host,32768);
        if(!juce::File(juce::String(host)).getFileName().startsWithIgnoreCase("Ableton Live"))return;
        impl=std::make_unique<Impl>(*this,parent);
    }
    if(!impl)return;
    auto local=peer->getComponent().getLocalArea(&owner,area);RECT client{};GetClientRect(parent,&client);
    const double scale=double(client.right-client.left)/juce::jmax(1,peer->getComponent().getWidth());
    impl->poll(context,{juce::roundToInt(local.getX()*scale),juce::roundToInt(local.getY()*scale),juce::roundToInt(local.getWidth()*scale),juce::roundToInt(local.getHeight()*scale)},owner.isShowing());
#else
    juce::ignoreUnused(context,area);
#endif
}
