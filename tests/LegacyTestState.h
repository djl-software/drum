#pragma once
#include <juce_core/juce_core.h>
// Synthetic old-schema fixtures must not contain chains introduced in schema 11.
inline void stripChainEffects(juce::XmlElement& xml)
{
    xml.deleteAllChildElementsWithTagName("Effect");
    for(auto* child:xml.getChildIterator())stripChainEffects(*child);
}
