#pragma once
#include "CosmosMotion.h"
#include <numbers>
#include <cstdint>
namespace cosmos {
constexpr int trajectoryCount=50,trajectoryCapacity=128;
inline const char* trajectoryFamily(int n)noexcept{constexpr const char* names[]={"Cercle","Ellipse","Spirale","Infini","Étoile","Triangle","Carré","Lissajous","Orbite","Zigzag"};return names[std::clamp(n/5,0,9)];}
struct Trajectory{
 int count=0;bool closed=true,curve=false;std::array<float,trajectoryCapacity>x{},y{};
 std::array<float,2> at(float t)const noexcept{
  if(count<2)return {.5f,.5f};t=std::clamp(t,0.f,1.f);float pos=t*(closed?count:count-1);int i=std::min(int(pos),closed?count-1:count-2);float f=pos-i;
  auto idx=[&](int j){return closed?(j%count+count)%count:std::clamp(j,0,count-1);};
  auto sample=[&](const auto& a){float b=a[idx(i)],c=a[idx(i+1)];if(!curve)return std::lerp(b,c,f);float a0=a[idx(i-1)],d=a[idx(i+2)];float u=1-f;return std::clamp(u*u*u*b+3*u*u*f*(b+(c-a0)/6)+3*u*f*f*(c-(d-b)/6)+f*f*f*c,0.f,1.f);};return {sample(x),sample(y)};
 }
 MotionClip clip(double duration,bool sync)const noexcept{MotionClip c;c.count=motionPoints;c.duration=duration;c.sync=sync;for(int j=0;j<c.count;++j){auto p=at(float(j)/(c.count-1));c.x[j]=p[0];c.y[j]=p[1];}return c;}
};
inline Trajectory factoryTrajectory(int index)noexcept{
 index=std::clamp(index,0,trajectoryCount-1);int family=index/5,v=index%5;Trajectory a;a.count=96;a.closed=family!=2&&family!=9;a.curve=false;
 constexpr double pi=std::numbers::pi_v<double>;
 for(int j=0;j<a.count;++j){double t=double(j)/(a.closed?a.count:a.count-1),p=2*pi*t,r=.27+.035*v,x=0,y=0;
  switch(family){case 0:x=r*std::cos(p);y=r*std::sin(p);break;case 1:x=.44*std::cos(p);y=(.14+.055*v)*std::sin(p);break;case 2:x=.44*t*std::cos((v+1)*p);y=.44*t*std::sin((v+1)*p);break;case 3:x=.43*std::sin(p);y=(.2+.045*v)*std::sin(2*p);break;
  case 4:case 5:case 6:{int n=family==4?2*(5+v):family==5?3:4;double k=t*n;int i=int(k);double f=k-i;auto rr=[&](int z){return family==4&&z%2?r*.45:r;};x=std::lerp(rr(i)*std::cos(2*pi*i/n+v*.12),rr(i+1)*std::cos(2*pi*(i+1)/n+v*.12),f);y=std::lerp(rr(i)*std::sin(2*pi*i/n+v*.12),rr(i+1)*std::sin(2*pi*(i+1)/n+v*.12),f);break;}
  case 7:x=.43*std::sin((v+2)*p);y=.4*std::sin(3*p+.3);break;case 8:x=.29*std::cos(p)+.12*std::cos((v+3)*p);y=.29*std::sin(p)+.12*std::sin((v+3)*p);break;default:x=.88*t-.44;y=.4*(1-4*std::abs(std::fmod(t*(v+2),1.)-.5));break;}
  a.x[j]=std::clamp(float(.5+x),0.f,1.f);a.y[j]=std::clamp(float(.5+y),0.f,1.f);
 }return a;
}
inline std::array<float,2> shapeTrajectory(std::array<float,2> xy,float sx,float sy,float cx,float cy,float jitter,float phase)noexcept{
 float z=std::sin(phase*37.6991118f);return {std::clamp(cx+(xy[0]-.5f)*sx+jitter*.1f*z,0.f,1.f),std::clamp(cy+(xy[1]-.5f)*sy+jitter*.1f*std::cos(phase*56.5486678f),0.f,1.f)};
}
}
