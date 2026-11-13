#include "WaveformData.h"
#include <algorithm>
#include <limits>

namespace drum
{
namespace
{
WaveformData::Range emptyRange()
{
    return {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
}
void merge(WaveformData::Range& into, WaveformData::Range value)
{
    into.minimum = std::min(into.minimum, value.minimum);
    into.maximum = std::max(into.maximum, value.maximum);
}
}
void WaveformData::build(const juce::AudioBuffer<float>& audio)
{
    identicalChannels = audio.getNumChannels() < 2 || audio.getNumSamples() == 0
        || std::equal(audio.getReadPointer(0), audio.getReadPointer(0) + audio.getNumSamples(), audio.getReadPointer(1));
    channels.clear();
    channels.resize((size_t)audio.getNumChannels());
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        auto& levels = channels[(size_t)channel];
        Level base((size_t)((audio.getNumSamples() + blockSize - 1) / blockSize), emptyRange());
        const auto* samples = audio.getReadPointer(channel);
        for (int n = 0; n < audio.getNumSamples(); ++n)
            merge(base[(size_t)(n / blockSize)], {samples[n], samples[n]});
        levels.push_back(std::move(base));
        while (levels.back().size() > 1)
        {
            const auto& previous = levels.back();
            Level next((previous.size() + 1) / 2);
            for (size_t n = 0; n < previous.size(); n += 2)
            {
                auto value = previous[n];
                if (n + 1 < previous.size()) merge(value, previous[n + 1]);
                next[n / 2] = value;
            }
            levels.push_back(std::move(next));
        }
    }
}
WaveformData::Range WaveformData::range(const juce::AudioBuffer<float>& audio, int channel, int begin, int end) const
{
    begin = juce::jlimit(0, audio.getNumSamples(), begin);
    end = juce::jlimit(begin, audio.getNumSamples(), end);
    if (begin == end || channel < 0 || channel >= (int)channels.size()) return {};
    const auto* samples = audio.getReadPointer(channel);
    auto result = emptyRange();
    while (begin < end && begin % blockSize != 0)
    {
        merge(result, {samples[begin], samples[begin]});
        ++begin;
    }
    while (end > begin && end % blockSize != 0)
    {
        --end;
        merge(result, {samples[end], samples[end]});
    }
    const auto& levels = channels[(size_t)channel];
    int first = begin / blockSize, last = end / blockSize;
    while (first < last)
    {
        int level = 0, span = 1;
        while ((size_t)(level + 1) < levels.size() && first % (span * 2) == 0 && span * 2 <= last - first)
        { ++level; span *= 2; }
        merge(result, levels[(size_t)level][(size_t)(first / span)]);
        first += span;
    }
    return result;
}
size_t WaveformData::bytes() const
{
    size_t total = 0;
    for (const auto& levels : channels)
        for (const auto& level : levels) total += level.capacity() * sizeof(Range);
    return total;
}
}
