#pragma once
namespace drum {
enum class BuiltinKind { eq, compressor, transient, saturation, clipper, limiter, gate, utility, stereoWidth,
    dynamicEq, bitcrusher, tempoDelay, ducker, room, resonator, frequencyShifter };
inline constexpr int builtinCount=16;
}
