#pragma once
#include <juce_events/juce_events.h>
#if JUCE_WINDOWS
#include <windows.h>
#include <objbase.h>
// UI Automation may synchronously call the host UI while its worker exits.
// Service sent messages and COM marshaling, but do not run queued user input,
// close commands or JUCE timers recursively inside editor destruction.
inline void stopBrowserWorker(juce::Thread& worker) {
    worker.signalThreadShouldExit();worker.notify();
    if(!juce::MessageManager::getInstance()->isThisTheMessageThread()){worker.waitForThreadToExit(-1);return;}
    auto thread=OpenThread(SYNCHRONIZE,FALSE,(DWORD)(uintptr_t)worker.getThreadId());
    while(worker.isThreadRunning()){
        MSG message{};PeekMessageW(&message,nullptr,WM_NULL,WM_NULL,PM_NOREMOVE);
        if(thread){DWORD index=0;auto result=CoWaitForMultipleHandles(COWAIT_DEFAULT,20,1,&thread,&index);if(FAILED(result))worker.waitForThreadToExit(1);}
        else worker.waitForThreadToExit(5);
    }
    if(thread)CloseHandle(thread);
}
#endif
