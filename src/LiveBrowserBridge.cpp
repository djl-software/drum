#include "LiveBrowserBridge.h"
#if JUCE_WINDOWS
#include <windows.h>
namespace { void moduleAnchor() {} }
#endif

LiveBrowserBridge::LiveBrowserBridge(juce::Component& c):owner(c) {}
LiveBrowserBridge::~LiveBrowserBridge()
{
    if(folder!=juce::File()) folder.getChildFile("stop").replaceWithText("stop");
    if(process.isRunning() && !process.waitForProcessToFinish(500)) process.kill();
    if(folder!=juce::File()) folder.deleteRecursively();
}
void LiveBrowserBridge::poll(const juce::String& context,juce::Rectangle<int> area)
{
#if JUCE_WINDOWS
    auto* peer=owner.getPeer(); if(!peer)return;
    auto hwnd=static_cast<HWND>(peer->getNativeHandle());
    if(!attempted && owner.isShowing())
    {
        attempted=true;
        wchar_t host[MAX_PATH]{};GetModuleFileNameW(nullptr,host,MAX_PATH);
        if(!juce::File(juce::String(host)).getFileName().startsWithIgnoreCase("Ableton Live"))return;
        HMODULE module{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&moduleAnchor),&module);
        wchar_t path[32768]{};GetModuleFileNameW(module,path,32768);
        auto helper=juce::File(juce::String(path)).getSiblingFile("DrumLiveBrowserBridge.exe");
        if(!helper.existsAsFile())return;
        folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("DJL-Drum-bridge-"+juce::Uuid().toString());
        if(!folder.createDirectory())return;
        process.start(juce::StringArray{helper.getFullPathName(),juce::String((juce::int64)GetCurrentProcessId()),
            juce::String((juce::int64)(intptr_t)hwnd),folder.getFullPathName()},0);
    }
    if(folder==juce::File() || !process.isRunning())return;
    auto local=peer->getComponent().getLocalArea(&owner,area); RECT client{};GetClientRect(hwnd,&client);
    const double scale=double(client.right-client.left)/juce::jmax(1,peer->getComponent().getWidth());
    auto* data=new juce::DynamicObject();
    data->setProperty("enabled",owner.isShowing() && !context.isEmpty());data->setProperty("context",context);
    data->setProperty("x",juce::roundToInt(local.getX()*scale));data->setProperty("y",juce::roundToInt(local.getY()*scale));
    data->setProperty("width",juce::roundToInt(local.getWidth()*scale));data->setProperty("height",juce::roundToInt(local.getHeight()*scale));
    auto request=juce::JSON::toString(juce::var(data),true);
    if(request!=lastRequest){folder.getChildFile("request.json").replaceWithText(request);lastRequest=request;}
    for(const auto& file:folder.findChildFiles(juce::File::findFiles,false,"drop-*.json"))
    {
        const auto value=juce::JSON::parse(file);file.deleteFile();
        if(value["context"].toString()==context && owner.isShowing() && onDrop)onDrop(value);
    }
#else
    juce::ignoreUnused(context,area);
#endif
}
