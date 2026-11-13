#pragma once
#include "KitLibrary.h"
namespace drum {
// The same XML and sibling Samples folder are used on every platform.
struct PortableFiles {
    static juce::File root();
    static Result collect(juce::XmlElement&, const juce::File& document);
    static Result resolve(juce::XmlElement&, const juce::File& document);
    static Result write(const juce::XmlElement&, const juce::File& document);
};
}
