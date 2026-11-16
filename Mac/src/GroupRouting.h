#pragma once

namespace drum
{
// Shared send settings, separate audio histories per source group. These lanes
// are runtime-only: saved kit IDs and the other platforms' layouts stay intact.
inline constexpr int macInstrumentReturnBase=effectLanes;
inline constexpr int macGroupFaderBase=macInstrumentReturnBase+maxGroups*6;
inline constexpr int macProcessingLanes=macGroupFaderBase+maxGroups;
inline constexpr int instrumentReturnLane(int group,int send){return macInstrumentReturnBase+group*6+send;}
}
