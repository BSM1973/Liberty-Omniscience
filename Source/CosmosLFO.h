#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace cosmos {
constexpr int lfoCount=8;
struct LfoParams {int shape=0,destination=0,layer=0,division=0;float rate=1,depth=0,phase=0;};
inline double lfoHz(const LfoParams& p,double bpm) noexcept{
  constexpr double beats[]={0,4,2,1,.5,.25,1./3, .75};
  return p.division>0&&p.division<8?std::clamp(bpm,20.,400.)/(60*beats[p.division]):std::clamp(double(p.rate),.01,40.);
}
inline float lfoWave(int shape,double phase,std::uint32_t cycle=0) noexcept{
  auto wrap=phase-std::floor(phase);
  switch(shape){
    case 1:return float(1-4*std::abs(wrap-.5));
    case 2:return float(2*wrap-1);
    case 3:return float(1-2*wrap);
    case 4:return wrap<.5?1.f:-1.f;
    case 5:{auto z=cycle+0x9e3779b9u;z^=z>>16;z*=0x7feb352du;z^=z>>15;z*=0x846ca68bu;z^=z>>16;return float(z>>8)*(2.f/16777215.f)-1.f;}
    default:return float(std::sin(6.283185307179586*wrap));
  }
}
struct LfoState {
  double phase=0;std::uint32_t cycle=0;
  float tick(const LfoParams& p,double increment)noexcept{
    double position=phase+p.phase;
    float v=lfoWave(p.shape,position,cycle+std::uint32_t(position>=1));
    phase+=increment;if(phase>=1){phase-=1;++cycle;}
    return v*p.depth;
  }
};
}
