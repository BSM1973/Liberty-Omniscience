#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int filterModelCount=42;
struct FilterParams {int model=0;float cutoff=8000,resonance=0,drive=0;bool autoGain=false;};
constexpr int combCapacity=9602;
struct SpectralBiquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    // RBJ digital biquad, bandpass constant peak, allpass, bell and shelves.
    static SpectralBiquad make(int kind,double hz,double q,double gain,double sr){
      double w=6.283185307179586*std::clamp(hz,20.,sr*.45)/sr,c=std::cos(w),s=std::sin(w),alpha=s/(2*q),A=std::pow(10.,gain/40.);
      double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
      if(kind==0){b0=alpha;b2=-alpha;a0=1+alpha;a1=-2*c;a2=1-alpha;}
      else if(kind==1){b0=1-alpha;b1=-2*c;b2=1+alpha;a0=1+alpha;a1=-2*c;a2=1-alpha;}
      else if(kind==2){b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;}
      else {
        double t=std::sqrt(2*A)*s;
        if(kind==3){b0=A*((A+1)-(A-1)*c+t);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-t);a0=(A+1)+(A-1)*c+t;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-t;}
        else {b0=A*((A+1)+(A-1)*c+t);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-t);a0=(A+1)-(A-1)*c+t;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-t;}
      }
      return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
    }
};
struct FilterPlan {
    int model=0,sections=0,kind=0;bool onePole=false;
    float g=0,a1=1,a2=0,a3=0,k=1.41421356f,b1=1,b2=0,b3=0,drive=0,driveNorm=1,legacyAlpha=0;
    int analogPoles=0;float analogStep=0,feedback=0,analogDrive=1,compensation=1;bool analog=false;
    int spectralCount=0;bool spectralParallel=false,comb=false,combFeedback=false;
    float combGain=0,combDelay=1;std::array<SpectralBiquad,3> spectral{};
    FilterPlan()=default;
    FilterPlan(FilterParams p,double sr){
      model=std::clamp(p.model,0,filterModelCount-1);
      double hz=std::clamp(double(p.cutoff),20.,sr*.45);
      legacyAlpha=float(1-std::exp(-6.283185307179586*hz/sr));
      if(model<2)return;
      if(model>=26){
        const double res=std::clamp(double(p.resonance),0.,1.);
        drive=std::clamp(p.drive,0.f,1.f);
        if(drive>0){drive=1+8*drive;driveNorm=1/std::tanh(drive);}
        if(model<=29){comb=true;combFeedback=model<=27;combDelay=float(std::clamp(sr/hz,1.,double(combCapacity-2)));combGain=float((model%2?-1.:1.)*(.1+.84*res));return;}
        if(model<=34||model==41){
          spectralCount=3;spectralParallel=true;
          constexpr double vowels[6][3]={{800,1150,2900},{400,1700,2600},{300,2200,3000},{450,800,2830},{325,700,2530},{400,1200,3600}};
          int v=model==41?5:model-30;
          for(int i=0;i<3;++i)spectral[i]=SpectralBiquad::make(0,vowels[v][i]*hz/1200.,2+8*res,0,sr);
        }else{
          spectralCount=model==36?2:1;
          int kind=model<=36?1:model<=38?2:model==39?3:4;
          double gain=(model==38?-1.:1.)*(3+15*res);
          for(int i=0;i<spectralCount;++i)spectral[i]=SpectralBiquad::make(kind,hz,.707+5*res,gain,sr);
        }
        return;
      }
      if(model>=18){
        analog=true;analogPoles=(model==21||model==23||model==25)?2:4;
        // Two internal substeps, with bounded nonlinear feedback and state.
        analogStep=float(1-std::exp(-6.283185307179586*hz/(sr*2.)));
        float res=std::clamp(p.resonance,0.f,1.f);
        feedback=(analogPoles==4?4.6f:1.85f)*res;
        analogDrive=1+5*std::clamp(p.drive,0.f,1.f);
        compensation=p.autoGain?std::min(2.5f,1+feedback*.45f):1;
        return;
      }
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
    std::array<float,4> s1{},s2{};float pole=0,legacy=0;std::array<float,4> analogState{};int prior=-1;
    std::array<float,combCapacity> delay{};int delayWrite=0,delayValid=0;
    std::array<double,3> spectralZ1{},spectralZ2{};
    float tick(float x,const FilterPlan& p)noexcept{
      if(prior!=p.model){s1={};s2={};analogState={};pole=legacy=0;delayWrite=delayValid=0;spectralZ1={};spectralZ2={};prior=p.model;}
      if(p.model==1)return x;
      if(p.model==0){legacy+=p.legacyAlpha*(x-legacy);return legacy;}
      if(p.model>=26){
        if(p.drive>0)x=std::tanh(x*p.drive)*p.driveNorm;
        if(p.comb){
          int whole=int(p.combDelay);float frac=p.combDelay-whole;
          auto read=[&](int back){return back<=delayValid?delay[(delayWrite+combCapacity-back)%combCapacity]:0.f;};
          float d=std::lerp(read(whole),read(whole+1),frac);
          float y=x+p.combGain*d;
          delay[delayWrite]=p.combFeedback?y:x;
          if(++delayWrite==combCapacity)delayWrite=0;
          delayValid=std::min(combCapacity-1,delayValid+1);
          return y;
        }
        double y=0,u=x;
        for(int i=0;i<p.spectralCount;++i){
          const auto& c=p.spectral[i];double in=p.spectralParallel?x:u;
          double out=c.b0*in+spectralZ1[i];
          spectralZ1[i]=c.b1*in-c.a1*out+spectralZ2[i];spectralZ2[i]=c.b2*in-c.a2*out;
          if(p.spectralParallel)y+=out;else u=out;
        }
        double out=p.spectralParallel?y/3.:u;
        if(!std::isfinite(out)){spectralZ1={};spectralZ2={};return 0;}
        return float(out);
      }
      if(p.analog){
        // Original analog-inspired models, not component-level circuit emulations.
        auto shape=[&](float u){
          if(p.model==20)return u/(1+std::abs(u)); // diode-inspired soft current limiting
          if(p.model==21||p.model==22)return std::clamp(u,-1.f,1.f); // OTA-inspired limiting
          if(p.model==19)return std::tanh(u*1.4f)/1.4f;
          return std::tanh(u);
        };
        for(int sub=0;sub<2;++sub){
          float u=shape(x*p.analogDrive-p.feedback*analogState[p.analogPoles-1]);
          for(int i=0;i<p.analogPoles;++i){
            float delta=shape(u)-shape(analogState[i]);
            analogState[i]=std::clamp(analogState[i]+p.analogStep*delta,-4.f,4.f);
            u=analogState[i];
          }
        }
        float y=analogState[p.analogPoles-1];
        if(p.model==24||p.model==25)y=x-analogState[0]*2+analogState[1];
        return std::isfinite(y)?y*p.compensation:0.f;
      }
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
