#pragma once
#include "KitLibrary.h"

namespace drum {
// The sound editor diagram follows this instrument, not other kit sources.
inline std::array<bool,6> displayedSendRoutes(const SoundParameters& sound, bool audible,
                                             const std::array<ReturnRouting,6>& routes)
{
    std::array<bool,6> active{};
    const double levels[]{sound.sendA,sound.sendB,sound.sendC,sound.sendD,sound.sendE,sound.sendF};
    auto enabled=[&](int n){const auto& mixer=routes[(size_t)n].mixer;return !mixer.mute && mixer.volumeDb>-70;};
    for(int n=0;n<6;++n)active[(size_t)n]=audible && levels[n]>sendFloor && enabled(n);
    for(int step=0;step<6;++step)
        for(int source=0;source<6;++source)if(active[(size_t)source])
            for(int target=0;target<6;++target)
                if(routes[(size_t)source].sends[(size_t)target]>sendFloor && enabled(target))active[(size_t)target]=true;
    return active;
}
}
