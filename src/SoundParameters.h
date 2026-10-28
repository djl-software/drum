#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace drum
{
// One registry supplies state validation, persistence and the editor. Values are
// plain scalars so an immutable render bank can copy them without allocations.
#define DRUM_SOUND_PARAMETERS(X) \
 X(start,"Start",0,0,1,0.00001," %",0,"",100) \
 X(end,"End",1,0,1,0.00001," %",0,"",100) \
 X(sampleGain,"Sample gain",0,-24,24,0.1," dB",0,"",1) \
 X(reverse,"Direction",0,0,1,1,"",0,"Forward|Reverse",1) \
 X(normalise,"Normalise",0,0,1,1,"",0,"Off|On",1) \
 X(snap,"Zero-crossing snap",0,0,1,1,"",0,"Off|On",1) \
 X(mode,"Playback",0,0,2,1,"",0,"Trigger|Gate|Classic ADSR",1) \
 X(voices,"Voices",128,1,128,1,"",0,"",1) \
 X(retrigger,"Retrigger",0,0,1,1,"",0,"Overlap|Cut previous",1) \
 X(loop,"Loop (Classic)",0,0,1,1,"",0,"Off|On",1) \
 X(loopStart,"Loop start",0,0,1,0.00001," %",0,"",100) \
 X(loopEnd,"Loop end",1,0,1,0.00001," %",0,"",100) \
 X(loopFade,"Loop crossfade",0,0,500,0.1," ms",0,"",1) \
 X(fadeIn,"Fade in",0,0,10000,0.1," ms",0,"",1) \
 X(fadeOut,"Fade out / Gate release",2,0,10000,0.1," ms",0,"",1) \
 X(ampAttack,"Amp attack",0,0,20000,0.1," ms",1,"",1) \
 X(ampDecay,"Amp decay",100,0,20000,0.1," ms",1,"",1) \
 X(ampSustain,"Amp sustain",1,0,1,0.001," %",1,"",100) \
 X(ampRelease,"Amp release",100,0,30000,0.1," ms",1,"",1) \
 X(pitchAmount,"Pitch envelope",0,-48,48,0.01," st",1,"",1) \
 X(pitchAttack,"Pitch attack",0,0,20000,0.1," ms",1,"",1) \
 X(pitchDecay,"Pitch decay",100,0,20000,0.1," ms",1,"",1) \
 X(pitchSustain,"Pitch sustain",0,0,1,0.001," %",1,"",100) \
 X(pitchRelease,"Pitch release",100,0,30000,0.1," ms",1,"",1) \
 X(detune,"Fine tuning",0,-100,100,0.1," cents",1,"",1) \
 X(velocityVolume,"Velocity to volume",1,0,1,0.001," %",1,"",100) \
 X(velocityPitch,"Velocity to pitch",0,-24,24,0.01," st",1,"",1) \
 X(randomPan,"Random pan",0,0,1,0.001," %",1,"",100) \
 X(bendRange,"Pitch bend range",0,0,24,1," st",1,"",1) \
 X(filterOn,"Filter",0,0,1,1,"",2,"Off|On",1) \
 X(filterType,"Filter type",0,0,3,1,"",2,"Low-pass|High-pass|Band-pass|Notch",1) \
 X(filterSlope,"Slope",0,0,1,1,"",2,"12 dB/oct|24 dB/oct",1) \
 X(cutoff,"Cutoff",20000,20,20000,1," Hz",2,"",1) \
 X(resonance,"Resonance (Q)",0.707,0.1,10,0.001,"",2,"",1) \
 X(drive,"Drive",0,0,24,0.1," dB",2,"",1) \
 X(filterAmount,"Filter envelope",0,-8,8,0.01," oct",2,"",1) \
 X(filterAttack,"Filter attack",0,0,20000,0.1," ms",2,"",1) \
 X(filterDecay,"Filter decay",100,0,20000,0.1," ms",2,"",1) \
 X(filterSustain,"Filter sustain",0,0,1,0.001," %",2,"",100) \
 X(filterRelease,"Filter release",100,0,30000,0.1," ms",2,"",1) \
 X(velocityFilter,"Velocity to filter",0,-8,8,0.01," oct",2,"",1) \
 X(lfoOn,"LFO",0,0,1,1,"",3,"Off|On",1) \
 X(lfoShape,"Waveform",0,0,5,1,"",3,"Sine|Triangle|Square|Saw up|Saw down|Random",1) \
 X(lfoRate,"Rate",1,0.01,30,0.01," Hz",3,"",1) \
 X(lfoSync,"Clock",0,0,1,1,"",3,"Free Hz|Host tempo",1) \
 X(lfoDivision,"Beat division",4,0,8,1,"",3,"1/64|1/32|1/16|1/8|1/4|1/2|1 bar|2 bars|4 bars",1) \
 X(lfoAttack,"LFO fade in",0,0,10000,0.1," ms",3,"",1) \
 X(lfoRetrigger,"LFO retrigger",1,0,1,1,"",3,"Free-running|Per note",1) \
 X(lfoPhase,"Phase",0,0,360,0.1," deg",3,"",1) \
 X(lfoVolume,"Volume depth",0,0,1,0.001," %",3,"",100) \
 X(lfoPitch,"Pitch depth",0,0,24,0.01," st",3,"",1) \
 X(lfoPan,"Pan depth",0,0,1,0.001," %",3,"",100) \
 X(lfoFilter,"Filter depth",0,0,8,0.01," oct",3,"",1) \
 X(output,"Audio output",0,0,15,1,"",4,"Main|Out 2|Out 3|Out 4|Out 5|Out 6|Out 7|Out 8|Out 9|Out 10|Out 11|Out 12|Out 13|Out 14|Out 15|Out 16",1) \
 X(sendA,"Send A",-60,-60,0,0.1," dB",4,"",1) \
 X(sendB,"Send B",-60,-60,0,0.1," dB",4,"",1) \
 X(sendC,"Send C",-60,-60,0,0.1," dB",4,"",1) \
 X(sendD,"Send D",-60,-60,0,0.1," dB",4,"",1) \
 X(sendE,"Send E",-60,-60,0,0.1," dB",4,"",1) \
 X(sendF,"Send F",-60,-60,0,0.1," dB",4,"",1)

struct SoundParameters
{
#define DRUM_DECLARE(key,label,initial,low,high,step,unit,page,choices,scale) double key = initial;
    DRUM_SOUND_PARAMETERS(DRUM_DECLARE)
#undef DRUM_DECLARE
    bool operator==(const SoundParameters&) const = default;
};
struct SoundParameter
{
    const char* key;
    const char* label;
    double SoundParameters::* member;
    double initial, minimum, maximum, step;
    const char* unit;
    int page;
    const char* choices;
    double scale;
};
inline constexpr SoundParameter soundParameters[] {
#define DRUM_DESCRIBE(key,label,initial,low,high,step,unit,page,choices,scale) {#key,label,&SoundParameters::key,initial,low,high,step,unit,page,choices,scale},
    DRUM_SOUND_PARAMETERS(DRUM_DESCRIBE)
#undef DRUM_DESCRIBE
};
#undef DRUM_SOUND_PARAMETERS
inline bool validSound(const SoundParameters& sound)
{
    for (auto& p : soundParameters)
    {
        const auto v = sound.*(p.member);
        if (!std::isfinite(v) || v < p.minimum || v > p.maximum
            || (p.step == 1 && v != std::floor(v))) return false;
    }
    return sound.start < sound.end && sound.loopStart < sound.loopEnd;
}
constexpr int numMacros=16;
struct MacroMapping
{
    int target=0;
    double minimum=0,maximum=1;
    bool operator==(const MacroMapping&) const = default;
};
struct MacroTarget
{
    const char* name;
    double SoundParameters::* member;
    double minimum,maximum;
};
// The first three targets address the instrument mixer; the rest address sound parameters.
inline constexpr MacroTarget macroTargets[] {
    {"Unmapped",nullptr,0,1},{"Volume",nullptr,-60,12},{"Pan",nullptr,-1,1},{"Tuning",nullptr,-48,48},
    {"Sample gain",&SoundParameters::sampleGain,-24,24},{"Filter cutoff",&SoundParameters::cutoff,20,20000},
    {"Resonance",&SoundParameters::resonance,0.1,10},{"Amp attack",&SoundParameters::ampAttack,0,20000},
    {"Amp decay",&SoundParameters::ampDecay,0,20000},{"Amp release",&SoundParameters::ampRelease,0,30000},
    {"Filter envelope",&SoundParameters::filterAmount,-8,8},{"Pitch envelope",&SoundParameters::pitchAmount,-48,48},
    {"LFO rate",&SoundParameters::lfoRate,0.01,30},{"LFO pitch",&SoundParameters::lfoPitch,0,24},
    {"LFO volume",&SoundParameters::lfoVolume,0,1},
    {"Send A",&SoundParameters::sendA,-60,0},{"Send B",&SoundParameters::sendB,-60,0},
    {"Send C",&SoundParameters::sendC,-60,0},{"Send D",&SoundParameters::sendD,-60,0},
    {"Send E",&SoundParameters::sendE,-60,0},{"Send F",&SoundParameters::sendF,-60,0}
};
inline bool validMapping(const MacroMapping& m)
{
    if(m.target<0 || m.target>=(int)std::size(macroTargets)) return false;
    auto& target=macroTargets[(size_t)m.target];
    return std::isfinite(m.minimum) && std::isfinite(m.maximum)
        && m.minimum>=target.minimum && m.minimum<=target.maximum && m.maximum>=target.minimum && m.maximum<=target.maximum;
}
}
