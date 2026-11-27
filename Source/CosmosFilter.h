#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int filterModelCount=72;
struct FilterParams {int model=0;float cutoff=8000,resonance=0,drive=0;bool autoGain=false;float morph=.5f;};
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
    float combGain=0,combDelay=1,combDamping=0,morph=.5f;bool creativeSVF=false,phaser=false;std::array<SpectralBiquad,4> spectral{};
    bool lpg=false,asymmetric=false,selfResonator=false;float gateAttack=0,gateRelease=0,gateRate=0,rotationCos=1,rotationSin=0,resonatorRadius=0,resonatorDamping=.002f;
    FilterPlan()=default;
    FilterPlan(FilterParams p,double sr){
      model=std::clamp(p.model,0,filterModelCount-1);
      double hz=std::clamp(double(p.cutoff),20.,sr*.45);
      legacyAlpha=float(1-std::exp(-6.283185307179586*hz/sr));
      if(model<2)return;
      if(model>=54){
        double res=std::clamp(double(p.resonance),0.,1.);
        morph=std::clamp(p.morph,0.f,1.f);
        drive=std::clamp(p.drive,0.f,1.f);
        if(drive>0){drive=1+8*drive;driveNorm=1/std::tanh(drive);}
        if(model<=56){
          lpg=true;gateAttack=float(1-std::exp(-1./(sr*(model==55?.002:.012))));gateRelease=float(1-std::exp(-1./(sr*(model==55?.035:.22))));
          gateRate=float(1-std::exp(-6.283185307179586*hz/sr));return;
        }
        if(model<=58){
          asymmetric=true;g=float(std::tan(3.141592653589793*hz/sr));k=float(1/(.707+3.3*res));
          a1=1/(1+g*(g+k));a2=g*a1;a3=g*a2;return;
        }
        if(model==71){
          selfResonator=true;double w=6.283185307179586*hz/sr;
          rotationCos=float(std::cos(w));rotationSin=float(std::sin(w));
          resonatorRadius=float(std::exp((res-.95)*300/sr));resonatorDamping=float(.002*48000./sr);return;
        }
        if(model<=62){
          spectralCount=4;spectralParallel=true;
          constexpr double ratios[4][4]={{1,1.414,2.137,3.73},{1,1.62,2.91,5.17},{1,2.76,5.4,8.93},{1,1.07,2.31,4.42}};
          for(int i=0;i<4;++i)spectral[i]=SpectralBiquad::make(0,hz*ratios[model-59][i],4+18*res,0,sr);
        }else if(model<=66){
          spectralCount=4;spectralParallel=model<=64;
          for(int i=0;i<4;++i){
            double centre=hz*std::pow(model%2?1.7:2.4,i-1.5);
            spectral[i]=SpectralBiquad::make(spectralParallel?0:2,centre,1+6*res,(i%2?-1:1)*(3+9*res),sr);
          }
        }else{
          spectralCount=2;
          // Cascaded hybrid responses: resonator+bell, resonator+shelf,
          // allpass+bell, and two oppositely signed shelves.
          if(model==67){spectral[0]=SpectralBiquad::make(0,hz,1+6*res,0,sr);spectral[1]=SpectralBiquad::make(2,hz*2,1,9*res,sr);}
          if(model==68){spectral[0]=SpectralBiquad::make(0,hz,1+6*res,0,sr);spectral[1]=SpectralBiquad::make(4,hz*1.5,1,12*res,sr);}
          if(model==69){spectral[0]=SpectralBiquad::make(1,hz,.8+3*res,0,sr);spectral[1]=SpectralBiquad::make(2,hz*2,1,-12*res,sr);phaser=true;}
          if(model==70){spectral[0]=SpectralBiquad::make(3,hz*.5,1,-12*res,sr);spectral[1]=SpectralBiquad::make(4,hz*2,1,12*res,sr);}
        }
        return;
      }
      if(model>=42){
        double res=std::clamp(double(p.resonance),0.,1.);
        morph=std::clamp(p.morph,0.f,1.f);
        drive=std::clamp(p.drive,0.f,1.f);
        if(drive>0){drive=1+8*drive;driveNorm=1/std::tanh(drive);}
        if(model<=44){
          creativeSVF=true;g=float(std::tan(3.141592653589793*hz/sr));k=float(1/(.707+3.3*res));
          a1=1/(1+g*(g+k));a2=g*a1;a3=g*a2;return;
        }
        if(model==49||model==50){
          comb=true;combFeedback=true;combGain=float((model==49?1:-1)*(.1+.84*res));combDelay=float(std::clamp(sr/hz,1.,double(combCapacity-2)));combDamping=.03f+.92f*(1-morph);return;
        }
        if(model==47||model==48){
          phaser=true;spectralCount=model==47?2:3;
          for(int i=0;i<spectralCount;++i)spectral[i]=SpectralBiquad::make(1,hz*std::pow(1.7,i-1),.7+2*res,0,sr);
          return;
        }
        spectralCount=3;spectralParallel=true;
        constexpr double vowel[3][2][3]={{{800,1150,2900},{300,2200,3000}},{{450,800,2830},{325,700,2530}},{{800,1150,2900},{450,800,2830}}};
        for(int i=0;i<3;++i){
          double centre=model==45?hz*(i+1):model==46?hz*(2*i+1):std::lerp(vowel[model-51][0][i],vowel[model-51][1][i],double(morph))*hz/1200.;
          spectral[i]=SpectralBiquad::make(0,centre,2+10*res,0,sr);
        }
        return;
      }
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
    std::array<float,combCapacity> delay{};int delayWrite=0,delayValid=0;float dampState=0;
    std::array<double,4> spectralZ1{},spectralZ2{};
    float gate=0,gateLow=0,previousInput=0,resX=0,resY=0;
    void reset() noexcept{prior=-1;delayWrite=delayValid=0;}
    static float asymmetricPrimitive(float x) noexcept {float u=x+.25f;return std::abs(u)+std::log1p(std::exp(-2*std::abs(u)))-std::tanh(.25f)*x;}
    static float asymmetricShape(float x) noexcept{return std::tanh(x+.25f)-std::tanh(.25f);}
    float tick(float x,const FilterPlan& p)noexcept{
      if(prior!=p.model){s1={};s2={};analogState={};pole=legacy=0;delayWrite=delayValid=0;dampState=0;spectralZ1={};spectralZ2={};gate=gateLow=previousInput=resX=resY=0;prior=p.model;}
      if(p.model==1)return x;
      if(p.model==0){legacy+=p.legacyAlpha*(x-legacy);return legacy;}
      if(p.model>=26){
        if(p.lpg){
          float target=std::clamp(std::abs(x)*(1+4*p.morph),0.f,1.f);
          gate+=(target>gate?p.gateAttack:p.gateRelease)*(target-gate);
          float coefficient=p.gateRate*(.025f+.975f*gate);
          gateLow+=coefficient*(x-gateLow);
          if(p.model==56){pole+=coefficient*.25f*(gateLow-pole);return (gateLow-pole)*gate;}
          return gateLow*gate;
        }
        if(p.asymmetric){
          float u=x*(p.drive>0?p.drive:1.f),delta=u-previousInput;
          float shaped=std::abs(delta)>.001f?(asymmetricPrimitive(u)-asymmetricPrimitive(previousInput))/delta:asymmetricShape(.5f*(u+previousInput));
          previousInput=u;
          float v3=shaped-s2[0],v1=p.a1*s1[0]+p.a2*v3,v2=s2[0]+p.a2*s1[0]+p.a3*v3;
          s1[0]=2*v1-s1[0];s2[0]=2*v2-s2[0];
          return p.model==57?v2:shaped-p.k*v1-v2;
        }
        if(p.selfResonator){
          float mag2=resX*resX+resY*resY;
          float radius=p.resonatorRadius/(1+p.resonatorDamping*mag2);
          float a=radius*(p.rotationCos*resX-p.rotationSin*resY)+.02f*x;
          float b=radius*(p.rotationSin*resX+p.rotationCos*resY);
          resX=std::clamp(a,-1.f,1.f);resY=std::clamp(b,-1.f,1.f);return resY;
        }
        if(p.drive>0)x=std::tanh(x*p.drive)*p.driveNorm;
        if(p.creativeSVF){float v3=x-s2[0],v1=p.a1*s1[0]+p.a2*v3,v2=s2[0]+p.a2*s1[0]+p.a3*v3;s1[0]=2*v1-s1[0];s2[0]=2*v2-s2[0];float hp=x-p.k*v1-v2,bp=p.k*v1,notch=hp+v2;if(p.model==42)return p.morph<.5f?std::lerp(v2,bp,2*p.morph):std::lerp(bp,hp,2*p.morph-1);return p.model==43?std::lerp(v2,notch,p.morph):std::lerp(bp,hp,p.morph);}
        if(p.comb){
          int whole=int(p.combDelay);float frac=p.combDelay-whole;
          auto read=[&](int back){return back<=delayValid?delay[(delayWrite+combCapacity-back)%combCapacity]:0.f;};
          float d=std::lerp(read(whole),read(whole+1),frac);
          if(p.combDamping>0){dampState+=p.combDamping*(d-dampState);d=dampState;}
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
        double out=p.spectralParallel?y/std::max(1,p.spectralCount):p.phaser?std::lerp(double(x),u,double(p.morph)):u;
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
