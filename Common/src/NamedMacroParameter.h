#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class NamedMacroParameter final : public juce::AudioParameterFloat
{
public:
    explicit NamedMacroParameter(int n):AudioParameterFloat(juce::ParameterID("macro"+juce::String(n+1),1),"Macro "+juce::String(n+1),0.f,1.f,.5f),fallback("Macro "+juce::String(n+1)),title(fallback){}
    juce::String getName(int maximumLength) const override{const juce::ScopedLock guard(lock);return title.substring(0,maximumLength);}
    bool rename(const juce::String& name){const juce::ScopedLock guard(lock);auto next=name.isEmpty()?fallback:name;if(next==title)return false;title=next;return true;}
private:
    const juce::String fallback;
    mutable juce::CriticalSection lock;
    juce::String title;
};
