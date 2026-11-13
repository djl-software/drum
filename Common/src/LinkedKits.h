#pragma once
#include <juce_core/juce_core.h>
#include <map>

// In-process exchange only: kit files are written only by explicit Save actions.
struct LinkedKits {
    struct Entry {int clients=0;uint64_t revision=0;std::unique_ptr<juce::XmlElement> state;};
    juce::CriticalSection lock;
    std::map<juce::String,Entry> entries;
};
