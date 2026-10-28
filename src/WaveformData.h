#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace drum
{
// Built on the loader thread, then immutable alongside the source audio.
// Exact range queries use cached blocks plus at most 126 raw edge samples.
class WaveformData
{
public:
    struct Range { float minimum = 0, maximum = 0; };
    void build(const juce::AudioBuffer<float>&);
    Range range(const juce::AudioBuffer<float>&, int channel, int begin, int end) const;
    size_t bytes() const;
    bool channelsAreIdentical() const { return identicalChannels; }
private:
    static constexpr int blockSize = 64;
    using Level = std::vector<Range>;
    std::vector<std::vector<Level>> channels;
    bool identicalChannels = true;
};
}
