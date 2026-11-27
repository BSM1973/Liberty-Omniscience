#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int motionPoints=1024;
struct MotionClip{
 int count=0;double duration=4;bool sync=true;std::array<float,motionPoints> x{},y{};
 std::array<float,2> at(double phase,int mode)const noexcept{
  if(count<1)return {.5f,.5f};
  if(mode==0)phase=std::clamp(phase,0.,1.);
  else if(mode==3){phase=std::fmod(std::max(0.,phase),2.);if(phase>1)phase=2-phase;}
  else {phase-=std::floor(phase);if(mode==2)phase=1-phase;}
  double pos=phase*(count-1);int i=std::min(count-1,int(pos)),j=std::min(count-1,i+1);float f=float(pos-i);
  return {std::lerp(x[i],x[j],f),std::lerp(y[i],y[j],f)};
 }
};
struct MotionRecorder{
 MotionClip clip;double elapsed=0;int next=0;
 void start(double duration,bool sync)noexcept{clip.count=0;clip.duration=std::max(.01,duration);clip.sync=sync;elapsed=0;next=0;}
 bool capture(float x,float y,double delta)noexcept{
  while(next<motionPoints&&double(next)/(motionPoints-1)*clip.duration<=elapsed){clip.x[next]=std::clamp(x,0.f,1.f);clip.y[next]=std::clamp(y,0.f,1.f);clip.count=++next;}
  elapsed+=std::max(0.,delta);
  return next>=motionPoints;
 }
 void finish()noexcept{if(clip.count>1)clip.duration*=double(clip.count-1)/(motionPoints-1);else if(clip.count==1)clip.duration=.01;}
};
inline std::array<float,2> motionTransform(std::array<float,2> p,float intensity,float rotation,bool ix,bool iy)noexcept{
 float x=(p[0]-.5f)*(ix?-1.f:1.f),y=(p[1]-.5f)*(iy?-1.f:1.f);float c=std::cos(rotation*.01745329252f),s=std::sin(rotation*.01745329252f);
 return {std::clamp(.5f+intensity*(c*x-s*y),0.f,1.f),std::clamp(.5f+intensity*(s*x+c*y),0.f,1.f)};
}
}
