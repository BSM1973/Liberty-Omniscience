#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace cosmos {
struct NexusParams{bool enabled=false;float x=.5f,y=.5f;bool power=false;float smooth=30;bool lockX=false,lockY=false,center=false,invertX=false,invertY=false;int macroX=0,macroY=0,ccX=-1,ccY=-1;};
inline std::array<float,4> nexusWeights(float x,float y,bool power)noexcept{x=std::clamp(x,0.f,1.f);y=std::clamp(y,0.f,1.f);std::array<float,4> w{(1-x)*y,x*y,(1-x)*(1-y),x*(1-y)};if(power)for(auto& v:w)v=std::sqrt(v);return w;}
struct NexusMotion{
  float x=.5f,y=.5f,mix=0;bool ready=false;
  void tick(float tx,float ty,float tm,float step)noexcept{if(!ready){x=tx;y=ty;mix=tm;ready=true;}else{x+=step*(tx-x);y+=step*(ty-y);mix+=step*(tm-mix);}}
  std::array<float,4> weights(bool power)const noexcept{auto w=nexusWeights(x,y,power);for(auto& v:w)v=std::lerp(1.f,v,mix);return w;}
};
inline std::array<float,2> nexusTarget(const NexusParams& p,const std::array<float,8>& macros,const std::array<float,128>& cc)noexcept{
  auto axis=[&](float v,int m,int c,bool inv){if(m>0&&m<=8)v=macros[m-1];else if(c>=0&&c<128&&cc[c]>=0)v=cc[c];v=std::clamp(v,0.f,1.f);return inv?1-v:v;};
  return {axis(p.x,p.macroX,p.ccX,p.invertX),axis(p.y,p.macroY,p.ccY,p.invertY)};
}
}
