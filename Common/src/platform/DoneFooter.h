#pragma once
#if JUCE_MAC
#include "../../../Mac/src/DoneFooter.h"
#else
namespace djl
{
inline juce::Component* withDoneButton(juce::DocumentWindow&,juce::Component* content){return content;}
}
#endif
