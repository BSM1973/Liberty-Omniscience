#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int filterModelCount=18;
struct FilterParams {int model=0;float cutoff=8000,resonance=0,drive=0;};
struct FilterPlan {
    int model=0,sections=0,kind=0;bool onePole=false;
    float g=0,a1=1,a2=0,a3=0,k=1.41421356f,b1=1,b2=0,b3=0,drive=0,driveNorm=1,legacyAlpha=0;
    FilterPlan()=default;
    FilterPlan(FilterParams p,double sr){
      model=std::clamp(p.model,0,filterModelCount-1);
      double hz=std::clamp(double(p.cutoff),20.,sr*.45);
      legacyAlpha=float(1-std::exp(-6.283185307179586*hz/sr));
      if(model<2)return;
      g=float(std::tan(3.141592653589793*hz/sr));
      const int m=model;
      onePole=m==2||m==3||m==16||m==17;
      kind=(m==3||m==5||m==9||m==13||m==15||m==17)?1:(m==6||m==10)?2:(m==7||m==11)?3:0;
      sections=m<=3?0:m<=7?1:m<=11?2:m<=13?3:m<=15?4:1;
      k=1.f/(.70710678f+3.3f*std::clamp(p.resonance,0.f,1.f));
      a1=1/(1+g*(g+k));a2=g*a1;a3=g*a2;
      b1=1/(1+g*(g+1.41421356f));b2=g*b1;b3=g*b2;
      drive=std::clamp(p.drive,0.f,1.f);
      if(drive>0){drive=1+8*drive;driveNorm=1/std::tanh(drive);}
    }
};
struct FilterState {
    std::array<float,4> s1{},s2{};float pole=0,legacy=0;int prior=-1;
    float tick(float x,const FilterPlan& p)noexcept{
      if(prior!=p.model){s1={};s2={};pole=legacy=0;prior=p.model;}
      if(p.model==1)return x;
      if(p.model==0){legacy+=p.legacyAlpha*(x-legacy);return legacy;}
      if(p.drive>0)x=std::tanh(x*p.drive)*p.driveNorm;
      if(p.onePole){float v=(x-pole)*p.g/(1+p.g),low=v+pole;pole=low+v;x=p.kind==1?x-low:low;}
      for(int i=0;i<p.sections;++i){
        float a=i?p.b1:p.a1,b=i?p.b2:p.a2,c=i?p.b3:p.a3,k=i?1.41421356f:p.k;
        float v3=x-s2[i],v1=a*s1[i]+b*v3,v2=s2[i]+b*s1[i]+c*v3;
        s1[i]=2*v1-s1[i];s2[i]=2*v2-s2[i];
        float high=x-k*v1-v2;
        x=p.kind==1?high:p.kind==2?v1:p.kind==3?high+v2:v2;
      }
      if(!std::isfinite(x)){s1={};s2={};pole=legacy=0;return 0;}
      return x;
    }
};
}
