#pragma once
#include <juce_core/juce_core.h>
namespace drum {
// Browser drags expose a row name, not Live's device identity. Keep a chooser
// for known native names even if the plugin database has just one VST3 match.
inline bool knownLiveDeviceName(const juce::String& name) {
    static const juce::StringArray names{
        "Align Delay","Amp","Audio Effect Rack","Auto Filter","Auto Pan","Auto Shift",
        "Beat Repeat","Cabinet","Channel EQ","Chorus","Chorus-Ensemble","Compressor","Corpus",
        "Delay","Drum Buss","Dynamic Tube","Echo","Envelope Follower","EQ Eight","EQ Three",
        "Erosion","External Audio Effect","Filter Delay","Flanger","Frequency Shifter","Gate",
        "Glue Compressor","Grain Delay","Hybrid Reverb","LFO","Limiter","Looper",
        "Multiband Dynamics","Overdrive","Pedal","Phaser","Phaser-Flanger","Redux","Resonators",
        "Reverb","Roar","Saturator","Shifter","Simple Delay","Spectral Resonator","Spectral Time",
        "Spectrum","Tuner","Utility","Vinyl Distortion","Vocoder",
        "Analog","Collision","Drift","Drum Rack","Electric","External Instrument","Impulse",
        "Instrument Rack","Meld","Operator","Sampler","Simpler","Tension","Wavetable"};
    return names.contains(name.trim(),true);
}
inline bool browserMatchNeedsChoice(const juce::String& name,int matches) {
    return matches!=1 || knownLiveDeviceName(name);
}
}
