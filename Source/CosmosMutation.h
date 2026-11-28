#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
namespace cosmos {
struct MutationRng{
 std::uint32_t state;
 explicit MutationRng(std::uint32_t seed):state(seed?seed:1){}
 float next()noexcept{state^=state<<13;state^=state>>17;state^=state<<5;return float(state>>8)/16777216.f;}
};
inline float mutationValue(float value,float amount,int mode,int family,MutationRng& rng)noexcept{
 constexpr float profiles[7][8]={{.08f,.10f,.08f,.08f,.10f,.08f,.08f,.06f},{.18f,.23f,.16f,.20f,.22f,.18f,.20f,.14f},{.12f,.18f,.12f,.25f,.17f,.28f,.30f,.16f},{.22f,.30f,.22f,.38f,.28f,.36f,.32f,.20f},{.35f,.55f,.48f,.50f,.40f,.35f,.50f,.35f},{.75f,.90f,.75f,.80f,.70f,.65f,.85f,.60f},{.40f,.40f,.40f,.40f,.40f,.40f,.40f,.40f}};
 float delta=(rng.next()*2-1)*std::clamp(amount,0.f,1.f)*profiles[std::clamp(mode,0,6)][std::clamp(family,0,7)];
 return std::clamp(value+delta,0.f,1.f);
}
}
