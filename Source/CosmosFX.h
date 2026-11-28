#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>
#include <numbers>
#include <cstdint>
#include "CosmosCreativeFX.h"
namespace cosmos {
constexpr int fxRacks=5,fxSlots=8,fxTypes=101;
struct FxParams{int type=0;bool enabled=false;float mix=1,input=1,output=1,a=.5f,b=.5f;int macro=0;float depth=0;bool autoGain=false;int quality=0,cc=-1;float ccValue=-1;bool sync=false;float stereo=.5f;double bpm=120;float delayFeedbackR=.5f,delayTone=.75f;bool delayFreeze=false;};
struct FxRack{bool parallel=false;std::array<FxParams,fxSlots> slots{};};
inline float analogShape(int type,float x,float a,float b)noexcept{
 float drive=1+15*a,z=x*drive;switch(type){
 case 19:return std::tanh(z+.3f*b)-std::tanh(.3f*b);
 case 20:return z/(1+std::abs(z)+.12f*z*z);
 case 21:return z/std::sqrt(1+z*z);
 case 22:return std::abs(z)<1?z-z*z*z/3:std::copysign(2.f/3,z);
 case 23:return std::atan(z*(1+b))/1.57079632679f;
 case 24:return z>=0?std::tanh(z):.6f*std::tanh(z*(1+2*b));
 case 25:return std::clamp(z*(1+5*b),-.8f,.8f);
 case 26:return std::sin(z*(.5f+2*b));
 case 27:{float q=std::fmod(z+1,4.f);if(q<0)q+=4;return 1-std::abs(q-2);}
 case 28:return std::lerp(z,std::abs(z),b)/(1+std::abs(z));
 default:return x;
 }}
inline float modulationHz(float a,bool sync,double bpm)noexcept{
 if(!sync)return .05f*std::exp2(8*a);
 constexpr float beats[]={16,8,4,2,1,.5f,.25f,.125f,.0625f};return float(std::clamp(bpm,20.,400.)/60)/beats[std::clamp(int(a*8+.5f),0,8)];
}
struct FxSpaceState{
 static constexpr int capacity=16384;
 std::array<std::vector<float>,2> delay;
 std::array<std::array<float,12>,2> ap{};
 std::array<float,2> feedback{},lp{},bp{},last{};
 int write=0,valid=0;float phase=0;
 void prepare(){for(auto& d:delay)d.resize(capacity);reset();}
 void reset()noexcept{write=valid=0;phase=0;ap={};feedback={};lp={};bp={};last={};}
 float read(int c,float samples)const noexcept{
  if(delay[c].empty())return 0;float d=std::clamp(samples,1.f,float(capacity-2));int i=int(d);float frac=d-i;
  float a=i<=valid?delay[c][(write-i+capacity)%capacity]:0,b=i+1<=valid?delay[c][(write-i-1+capacity)%capacity]:0;
  return std::lerp(a,b,frac);
 }
 void push(float l,float r)noexcept{if(delay[0].empty())return;delay[0][write]=l;delay[1][write]=r;write=(write+1)%capacity;valid=std::min(valid+1,capacity-1);}
 void tick(float* v,int type,float a,float b,float stereo,bool sync,double bpm,double sr)noexcept{
  constexpr float tau=6.28318530718f;float hz=modulationHz(a,sync,bpm);float s[2]={std::sin(tau*phase),std::sin(tau*(phase+.5f*stereo))};float in[2]={v[0],v[1]};float out[2]={in[0],in[1]};
  if(type>=29&&type<=34||type==40||type==44||type==46){
   for(int c=0;c<2;++c){float ms=type<=32? (type==29?13.f:type==30?19.f:type==31?23.f:8.f)+b*(3+5*s[c]):type==33?2.5f+2.2f*b*s[c]:type==34?5.f+4.5f*b*s[c]:type==40?8+6*b*s[c]:type==44?12+10*b*s[c]:c==0?.05f:1+29*b;
    float wet=read(c,float(sr*.001)*std::max(.05f,ms));
    if(type==31)wet=(wet+read(c,float(sr*.001)*(13+6*b*std::sin(tau*(phase+.333f))))+read(c,float(sr*.001)*(29+5*b*std::sin(tau*(phase+.667f)))))/3;
    if(type==32)wet=.5f*(wet+read(1-c,float(sr*.001)*(11+2*b*s[1-c])));
    if(type==34){float reference=read(c,float(sr*.005));out[c]=.5f*(reference-wet);}
    else if(type==40||type==44||type==46)out[c]=wet;
    else out[c]=.5f*(in[c]+wet);
    feedback[c]=wet;
   }
   push(in[0]+(type==33?.65f*b*feedback[0]:0),in[1]+(type==33?.65f*b*feedback[1]:0));
  }else if(type>=35&&type<=38){
    int poles=type==35?4:type==36?8:type==37?12:4;for(int c=0;c<2;++c){float x=in[c]+.55f*b*feedback[c];for(int j=0;j<poles;++j){float freq=std::min(float(sr*.4),150.f*std::exp2((.5f+.5f*s[c])*4+(type==38?j*.36f:0)));float g=std::tan(float(std::numbers::pi_v<double>*freq/sr)),k=(1-g)/(1+g);float y=ap[c][j]-k*x;ap[c][j]=x+k*y;x=y;}feedback[c]=x;out[c]=.5f*(in[c]+x);}
  }else if(type==39){for(int c=0;c<2;++c){float f=.5f+.5f*s[c];lp[c]+=.08f*(in[c]-lp[c]);out[c]=(lp[c]*(.8f+.2f*f)+(in[c]-lp[c])*(.35f+.65f*f))*(1-b*.65f+b*.65f*f);}}
  else if(type==41){float pan=b*s[0];out[0]=in[0]*std::sqrt(1-pan);out[1]=in[1]*std::sqrt(1+pan);}
  else if(type==42){for(int c=0;c<2;++c)out[c]=in[c]*std::lerp(1.f,s[c],b);hz=10+990*a;}
  else if(type==43){ // quadrature all-pass approximation, frequency shift by single-sideband modulation
   constexpr float c0[]={.0417f,.4453f,.8418f,.9742f},c1[]={.1618f,.6524f,.9100f,.9930f};for(int c=0;c<2;++c){float x=in[c],y=last[c];last[c]=in[c];for(int j=0;j<4;++j){float z=c0[j]*x+ap[c][j];ap[c][j]=x-c0[j]*z;x=z;z=c1[j]*y+ap[c][j+4];ap[c][j+4]=y-c1[j]*z;y=z;}out[c]=x*std::cos(tau*phase)-y*std::sin(tau*phase)*(b<.5f?-1:1);}hz=1+999*a;
  }else if(type==45){float mid=(in[0]+in[1])*.5f,side=(in[0]-in[1])*.5f;lp[0]+=.025f*(side-lp[0]);side=lp[0]+(side-lp[0])*(1+2*b);out[0]=mid+side;out[1]=mid-side;}
  else if(type==47||type==48){for(int c=0;c<2;++c){float freq=type==47?200.f*std::exp2(3*(.5f+.5f*s[c])):80.f*std::exp2(6*a);float g=std::tan(float(std::numbers::pi_v<double>*std::min(double(freq),sr*.4)/sr));float q=type==47?1.5f-b:1.8f-1.6f*b;float hp=(in[c]-(q+g)*bp[c]-lp[c])/(1+q*g+g*g);float band=g*hp+bp[c],low=g*band+lp[c];bp[c]=g*hp+band;lp[c]=g*band+low;out[c]=band*q;}}
  v[0]=out[0];v[1]=out[1];phase+=hz/float(sr);phase-=std::floor(phase);
 }
};

inline constexpr float delayLimitSeconds=2.f;
inline float delaySeconds(float a,bool sync,double bpm) noexcept {
 a=std::clamp(a,0.f,1.f);
 if(!sync)return .005f*std::pow(400.f,a);
 // Ordered by length: straight, dotted and triplet divisions.
 constexpr float beats[]={1.f/12,1.f/8,1.f/6,3.f/16,1.f/4,1.f/3,3.f/8,.5f,2.f/3,.75f,1.f,4.f/3,1.5f,2.f,3.f,4.f};
 return std::min(delayLimitSeconds,beats[std::clamp(int(a*15+.5f),0,15)]*float(60./std::clamp(bpm,20.,400.)));
}
struct EchoState {
 std::array<std::vector<float>,2> ring;
 std::array<float,2> lp{},hp{},bandLow{},bandHigh{},envelope{},smoothed{};
 std::array<std::array<float,4>,2> diffusion{};
 int write=0,valid=0,grainPos=0;float phase=0,freeze=0,activity=0;
 void prepare(double sr=48000){const auto n=size_t(std::ceil(std::max(8000.,sr)*delayLimitSeconds))+8;for(auto& x:ring)x.resize(n);reset();}
 void reset()noexcept{write=valid=grainPos=0;phase=freeze=activity=0;lp={};hp={};bandLow={};bandHigh={};envelope={};smoothed={};diffusion={};}
 float read(int c,float d)const noexcept{
  const int n=int(ring[c].size());if(n<4)return 0;
  d=std::clamp(d,1.f,float(n-2));int i=int(d);float f=d-i;
  float x=i<=valid?ring[c][(write-i+n)%n]:0,y=i+1<=valid?ring[c][(write-i-1+n)%n]:0;
  return std::lerp(x,y,f);
 }
 static float bounded(float x)noexcept{return std::isfinite(x)?std::clamp(x,-4.f,4.f):0.f;}
 float grains(int c,float base,int length,float slope)const noexcept{
  const int p=grainPos%length,q=(p+length/2)%length;
  auto tap=[&](int j){float w=.5f-.5f*std::cos(6.28318530718f*j/length);return w*read(c,base+slope*j);};
  return tap(p)+tap(q);
 }
 void tick(float* v,int type,float a,float b,const FxParams& p,double sr)noexcept{
  if(ring[0].empty())return;
  const float rate=float(sr),seconds=delaySeconds(a,p.sync,p.bpm);
  const float target=seconds*rate,omega=6.28318530718f/rate;
  const float tone=std::clamp(p.delayTone,0.f,1.f);
  const float cut=std::min(rate*.45f,300.f*std::exp2(tone*6.f));
  const float fc=1-std::exp(-omega*cut),hc=1-std::exp(-omega*25.f);
  const float fl=1-std::exp(-omega*400.f),fh=1-std::exp(-omega*2400.f);
  const float slewing=std::min(1.f,100.f/rate);
  float in[2]={bounded(v[0]),bounded(v[1])},wet[2]{},loop[2]{},frozen[2]{};
  freeze+=std::clamp((p.delayFreeze?1.f:0.f)-freeze,-slewing,slewing);
  for(int c=0;c<2;++c){
   float desired=target*(type==53&&c==1?1.5f:type==52?1.f:1.f);
   desired=std::clamp(desired,1.f,float(ring[c].size()-2));
   if(smoothed[c]<=0)smoothed[c]=desired;
   smoothed[c]+=slewing*(desired-smoothed[c]);
   const float d=smoothed[c],osc=std::sin(6.28318530718f*(phase+c*.25f));
   const int grain=std::max(16,int(std::min(d*.45f,rate*.045f))&~1);
   frozen[c]=read(c,d);
   float x=frozen[c];
   switch(type){
    case 49:break; // Digital
    case 50:x=std::tanh(1.35f*x)/1.35f;break;
    case 51:x=read(c,d+rate*.0015f*osc+rate*.0002f*std::sin(6.28318530718f*phase*7));x=std::tanh(1.5f*x)/1.5f;break;
    case 52:break; // Ping-pong feedback cross routing below
    case 53:break; // Dual stereo delay
    case 54:x=.45f*x+.32f*read(c,d*.5f)+.23f*read(c,d*.75f);break;
    case 55:x=grains(c,std::max(1.f,d-grain),grain,2.f);break;
    case 56:envelope[c]+=std::min(1.f,(std::abs(in[c])>envelope[c]?180.f:8.f)/rate)*(std::abs(in[c])-envelope[c]);break;
    case 57:{for(int j=0;j<4;++j){float y=diffusion[c][j]-.55f*x;diffusion[c][j]=x+.55f*y;x=y;}x=.55f*x+.25f*read(c,d*.83f)+.2f*read(1-c,d*.67f);break;}
    case 58:x=grains(c,std::max(1.f,d-grain*.5f),grain,.5f);break;
    case 59:x=.45f*x+.55f*grains(c,d+grain,grain,-1.f);break;
    case 60:x=read(c,d+rate*.004f*osc);break;
    case 61:{ // Frequency-dependent delays via a three-band complementary filter bank.
      const float m=read(c,d*.65f),h=read(c,d*.37f);
      bandLow[c]+=fl*(x-bandLow[c]);bandHigh[c]+=fh*(h-bandHigh[c]);
      diffusion[c][0]+=fl*(m-diffusion[c][0]);diffusion[c][1]+=fh*(m-diffusion[c][1]);
      x=bandLow[c]+diffusion[c][1]-diffusion[c][0]+h-bandHigh[c];break;}
    case 62:break; // Tunable feedback filter, also on wet signal below
    case 63:x=.55f*read(c,d+rate*.001f*osc)+.3f*read(c,d*.67f)+.15f*read(1-c,d*.43f);x=std::tanh(1.8f*x)/1.8f;break;
    default:break;
   }
   x=bounded(x);
   const float coef=type==50?std::min(fc,.12f):type==51||type==63?std::min(fc,.22f):fc;
   lp[c]+=coef*(x-lp[c]);hp[c]+=hc*(lp[c]-hp[c]);
   loop[c]=lp[c]-hp[c];
   wet[c]=type==62?loop[c]:type==56?x/(1+8*envelope[c]):x;
   wet[c]=std::lerp(wet[c],frozen[c],freeze);
  }
  float fb[2]={std::clamp(b,0.f,1.f)*.94f,std::clamp(p.delayFeedbackR,0.f,1.f)*.94f};
  for(int c=0;c<2;++c){
   int src=type==52?1-c:c;
   // At full freeze, recirculate the unfiltered fixed delay with no new input.
   ring[c][write]=bounded(std::lerp(in[c]+fb[c]*loop[src],frozen[c],freeze));
   v[c]=bounded(wet[c]);
  }
  write=(write+1)%int(ring[0].size());valid=std::min(valid+1,int(ring[0].size())-1);
  if(++grainPos>=1000000000)grainPos=0;
  phase+=float((type==51||type==63?.35: .6)/sr);phase-=std::floor(phase);
  activity=std::max(std::max(std::abs(v[0]),std::abs(v[1])),activity*.9995f);
 }
};
inline std::array<float,4> echoPreset(int type,int profile)noexcept{
 constexpr float settings[15][4]={{.58f,.32f,.32f,.9f},{.62f,.46f,.44f,.45f},{.67f,.52f,.48f,.58f},{.7f,.55f,.55f,.8f},{.65f,.43f,.55f,.82f},{.72f,.48f,.44f,.72f},{.64f,.37f,.42f,.85f},{.64f,.55f,.5f,.8f},{.66f,.56f,.52f,.65f},{.7f,.53f,.48f,.78f},{.68f,.55f,.55f,.7f},{.66f,.5f,.48f,.75f},{.71f,.45f,.48f,.9f},{.62f,.56f,.56f,.38f},{.72f,.57f,.51f,.55f}};
 auto q=std::array<float,4>{};for(int j=0;j<4;++j)q[j]=settings[std::clamp(type-49,0,14)][j];
 float d=profile==1?-.18f:profile==3?.18f:0.f;
 q[0]=std::clamp(q[0]+d*.5f,0.f,1.f);for(int j=1;j<3;++j)q[j]=std::clamp(q[j]+d,0.f,.95f);return q;
}


struct ReverbProfile{float size,decay,mod,early,colour;};
inline constexpr ReverbProfile reverbProfiles[15]={
 {.40f,.65f,.15f,.75f,.65f}, // Room
 {.55f,.85f,.10f,.85f,.80f}, // Studio
 {.75f,1.10f,.20f,.65f,.68f}, // Chamber
 {1.15f,1.65f,.30f,.45f,.70f}, // Hall
 {1.40f,2.05f,.35f,.40f,.78f}, // Concert
 {1.80f,2.60f,.40f,.30f,.58f}, // Cathedral
 {.85f,1.35f,.45f,.22f,.90f}, // Plate
 {.65f,.90f,.18f,.60f,.85f}, // Spring
 {1.55f,2.40f,.65f,.22f,.48f}, // Ambient
 {1.35f,2.20f,.45f,.25f,.82f}, // Shimmer
 {1.05f,1.40f,.25f,.70f,.75f}, // Reverse
 {.80f,1.10f,.25f,.45f,.82f}, // Gated
 {1.20f,1.70f,1.f,.35f,.75f}, // Modulated
 {1.30f,1.95f,.65f,.30f,.70f}, // Granular
 {1.65f,3.50f,.30f,.25f,.65f} // Infinite
};
inline std::array<float,5> reverbPreset(int type,int profile)noexcept{
 auto r=reverbProfiles[std::clamp(type-64,0,14)];
 float d=profile==1?-.16f:profile==3?.16f:0.f;
 return {std::clamp(.44f+d,0.f,1.f),std::clamp(.72f+d*.5f,0.f,1.f),std::clamp(r.early-d,0.f,1.f),r.colour,std::clamp(r.mod*.65f+d,0.f,1.f)};
}
struct ReverbState{
 static constexpr int lines=8;
 std::array<std::vector<float>,lines> tank;
 std::array<std::vector<float>,2> early,pitch;
 std::array<float,lines> damp{},scatter{},length{};
 int write=0,valid=0,earlyWrite=0,earlyValid=0,pitchWrite=0,pitchValid=0,grainPos=0;
 float phase=0,freeze=0,gate=0,gateHold=0,activity=0,shimmerPhase=0;
 void prepare(double sr=48000){
  const auto n=size_t(std::ceil(std::max(8000.,sr)*.24))+8;
  for(auto& x:tank)x.resize(n);
  for(auto& x:early)x.resize(n);
  for(auto& x:pitch)x.resize(size_t(std::ceil(std::max(8000.,sr)*.065))+8);
  reset();
 }
 void reset()noexcept{write=valid=earlyWrite=earlyValid=pitchWrite=pitchValid=grainPos=0;phase=freeze=gate=gateHold=activity=shimmerPhase=0;damp={};scatter={};length={};}
 static float read(const std::vector<float>& buf,int w,int v,float samples)noexcept{
  const int n=int(buf.size());if(n<4)return 0;
  samples=std::clamp(samples,1.f,float(n-2));const int k=int(samples);const float f=samples-k;
  const float a=k<=v?buf[(w-k+n)%n]:0,b=k+1<=v?buf[(w-k-1+n)%n]:0;
  return std::lerp(a,b,f);
 }
 static void advance(int& w,int& v,int n)noexcept{if(++w==n)w=0;v=std::min(v+1,n-1);}
 static float safe(float x)noexcept{return std::isfinite(x)?std::clamp(x,-4.f,4.f):0.f;}
 static void hadamard(std::array<float,lines>& x)noexcept{
  for(int span=1;span<lines;span*=2)for(int i=0;i<lines;i+=2*span)for(int j=0;j<span;++j){const float a=x[i+j],b=x[i+j+span];x[i+j]=a+b;x[i+j+span]=a-b;}
  for(auto& v:x)v*=.3535533905932738f;
 }
 float grain(int c,float base,int span,float slope)const noexcept{
  const int p=grainPos%span,q=(p+span/2)%span;
  auto tap=[&](int j){return (.5f-.5f*std::cos(6.28318530718f*j/span))*read(early[c],earlyWrite,earlyValid,base+slope*j);};
  return tap(p)+tap(q);
 }
 float octave(int c,float sr)const noexcept{
  const float span=std::max(16.f,sr*.045f);
  auto tap=[&](float ph){return (.5f-.5f*std::cos(6.28318530718f*ph))*read(pitch[c],pitchWrite,pitchValid,2+span*(1-ph));};
  float q=shimmerPhase+.5f;if(q>=1)q-=1;return tap(shimmerPhase)+tap(q);
 }
 void tick(float* v,int type,float a,float b,const FxParams& p,double sr)noexcept{
  if(tank[0].empty())return;
  constexpr float tau=6.28318530718f;
  constexpr float ms[lines]={29.7f,37.1f,41.9f,53.3f,61.7f,71.9f,79.3f,89.9f};
  constexpr float signsL[lines]={1,-1,1,1,-1,1,-1,-1},signsR[lines]={1,1,-1,1,1,-1,-1,-1};
  constexpr float erMs[6]={3.3f,7.1f,11.7f,17.9f,23.3f,31.1f};
  constexpr float erGain[6]={.29f,.22f,.17f,.13f,.10f,.07f};
  const auto prof=reverbProfiles[std::clamp(type-64,0,14)];
  const float rate=float(sr),smooth=std::min(1.f,80.f/rate),diff=std::clamp(b,0.f,1.f);
  const float mod=std::clamp(p.stereo,0.f,1.f)*prof.mod;
  const float er=std::clamp(p.delayFeedbackR,0.f,1.f);
  const float tone=std::clamp(p.delayTone,0.f,1.f);
  const float rt60=std::clamp(.25f*std::exp2(5*a)*prof.decay,.12f,24.f);
  const float fc=1-std::exp(-tau*std::min(rate*.44f,550.f*std::exp2(5*tone))/rate);
  freeze+=std::clamp((p.delayFreeze?1.f:0.f)-freeze,-smooth,smooth);
  const float in[2]={safe(v[0]),safe(v[1])};
  const float peak=std::max(std::abs(in[0]),std::abs(in[1]));
  if(peak>.015f)gateHold=.12f+.65f*a;
  gateHold=std::max(0.f,gateHold-1/rate);
  gate+=std::min(1.f,(gateHold>0?300.f:45.f)/rate)*((gateHold>0?1.f:0.f)-gate);
  float erOut[2]{},excite[2]{};
  for(int c=0;c<2;++c){
   for(int j=0;j<6;++j)erOut[c]+=erGain[j]*read(early[(j%3==0)?1-c:c],earlyWrite,earlyValid,rate*.001f*erMs[j]*prof.size*(c?1.09f:1.f));
   excite[c]=.72f*in[c]+.28f*erOut[c];
   if(type==74){int span=std::max(16,int(rate*.075f)&~1);excite[c]=grain(c,2.f,span,2.f);}
   if(type==77){int span=std::max(16,int(rate*.038f)&~1);excite[c]=.55f*excite[c]+.45f*grain(c,rate*.013f,span,.65f);}
   if(type==73)excite[c]+=.16f*octave(c,rate)*(1-freeze);
   if(type==71){float x=excite[c];float z=read(early[c],earlyWrite,earlyValid,rate*.0027f);excite[c]=.65f*x+.35f*(z-read(early[c],earlyWrite,earlyValid,rate*.0061f));}
  }
  std::array<float,lines> taps{},feedback{};
  float out[2]{};
  for(int j=0;j<lines;++j){
   float base=rate*.001f*ms[j]*prof.size;
   if(length[j]<=0)length[j]=base;length[j]+=smooth*(base-length[j]);
   float wobble=rate*.001f*(type==76?5.f:1.8f)*mod*std::sin(tau*(phase+j*.137f))*(1-freeze);
   if(type==71)wobble+=rate*.00035f*std::sin(tau*(phase*5+j*.23f))*(1-freeze);
   taps[j]=read(tank[j],write,valid,length[j]+wobble);
   damp[j]+=fc*(taps[j]-damp[j]);
   float x=std::lerp(damp[j],taps[j],freeze);
   const float k=.15f+.60f*diff;
   const float y=scatter[j]-k*x;scatter[j]=x+k*y;
   feedback[j]=std::lerp(std::lerp(x,y,diff*.72f),x,freeze);
   out[0]+=signsL[j]*taps[j]*.35355339f;out[1]+=signsR[j]*taps[j]*.35355339f;
  }
  hadamard(feedback);
  for(int j=0;j<lines;++j){
   float gain=std::pow(10.f,-3.f*(length[j]/rate)/rt60);
   if(type==78)gain=std::max(gain,.992f);
   gain=std::lerp(std::min(gain,.9993f),1.f,freeze);
   const float input=(signsL[j]*excite[0]+signsR[j]*excite[1])*.20f*(1-freeze);
   tank[j][write]=safe(input+gain*feedback[j]);
  }
  for(int c=0;c<2;++c){
   early[c][earlyWrite]=in[c]*(1-freeze);
   pitch[c][pitchWrite]=type==73?safe(out[c]):0.f;
   float tail=out[c]*.64f;
   if(type==75)tail*=std::lerp(gate,1.f,freeze);
   v[c]=safe(tail+er*prof.early*erOut[c]*(1-freeze));
  }
  advance(write,valid,int(tank[0].size()));advance(earlyWrite,earlyValid,int(early[0].size()));advance(pitchWrite,pitchValid,int(pitch[0].size()));
  if(++grainPos>=1000000000)grainPos=0;
  phase+=(type==76?.82f:.29f)/rate;phase-=std::floor(phase);
  shimmerPhase+=1/std::max(16.f,rate*.045f);shimmerPhase-=std::floor(shimmerPhase);
  activity=std::max(std::max(std::abs(v[0]),std::abs(v[1])),activity*.9995f);
 }
};

struct FxSlotState{
 int type=0;float wet=0;std::array<float,2> low{},dc{},env{};float phase=0,reduction=0;std::array<float,2> low2{},previous{},fastEnv{},slowEnv{};std::array<float,3> bandEnv{};float linkedEnv=0;FxSpaceState space;EchoState echo;ReverbState reverb;CreativeState creative;
 void prepare(double sr=48000){space.prepare();echo.prepare(sr);reverb.prepare(sr);creative.prepare(sr);clear();wet=0;type=0;}
 void reset()noexcept{clear();wet=0;type=0;}
 void clear()noexcept{low={};dc={};env={};low2={};previous={};fastEnv={};slowEnv={};bandEnv={};linkedEnv=0;phase=0;reduction=0;space.reset();echo.reset();reverb.reset();creative.reset();}
 bool active(const FxParams& p)const noexcept{return (p.enabled&&p.type>0)||wet>0;}
 void tick(float& l,float& r,const FxParams& p,double sr,const std::array<float,8>& macros)noexcept{
  if(!active(p)){reduction=0;return;}const int desired=p.enabled?std::clamp(p.type,0,fxTypes-1):0;
  if(type!=desired&&wet<=0){type=desired;clear();}
  float target=type==desired&&type!=0?std::clamp(p.mix,0.f,1.f):0.f;
  float step=float(1/std::max(1.,sr*.005));wet+=std::clamp(target-wet,-step,step);if(type==0||wet<=0)return;
  float a=std::clamp((p.cc>=0&&p.ccValue>=0?p.ccValue:p.a)+(p.macro>0&&p.macro<=8?p.depth*(macros[p.macro-1]-.5f)*2:0),0.f,1.f),b=std::clamp(p.b,0.f,1.f);
  float dry[2]={l,r},v[2]={l*p.input,r*p.input};
  float coeff=(type==4||type==5)?float(1-std::exp(-6.28318530718*std::min(20.*std::exp2(10.*a),sr*.45)/sr)):0.f;reduction=0;
  if(type==7){float m=(v[0]+v[1])*.5f,s=(v[0]-v[1])*.5f*(2*a);v[0]=m+s;v[1]=m-s;}
  else if(type==8){float mod=1-b*.5f+b*.5f*std::sin(phase*6.28318530718f);phase+=float((.1+19.9*a)/sr);if(phase>=1)phase-=1;v[0]*=mod;v[1]*=mod;}
  else if(type>=9&&type<=18){
    const float threshold=std::pow(10.f,(-60.f+60.f*a)/20.f),ratio=1+19*b;
    auto follow=[&](float& memory,float value,float attack,float release){float t=value>memory?attack:release;memory+=float(1-std::exp(-1/(sr*t)))*(value-memory);return memory;};
    auto compress=[&](float level,float rat){return level>threshold?std::pow(threshold/std::max(level,1e-9f),1-1/rat):1.f;};
    float gain=1,peak=std::max(std::abs(v[0]),std::abs(v[1]));
    if(type==13){float band[2][3]{};float c1=float(1-std::exp(-6.28318530718*180/sr)),c2=float(1-std::exp(-6.28318530718*2600/sr));for(int c=0;c<2;++c){low[c]+=c1*(v[c]-low[c]);low2[c]+=c2*(v[c]-low2[c]);band[c][0]=low[c];band[c][1]=low2[c]-low[c];band[c][2]=v[c]-low2[c];v[c]=0;}for(int j=0;j<3;++j){float level=follow(bandEnv[j],std::max(std::abs(band[0][j]),std::abs(band[1][j])),.01f,.12f);float g=compress(level,ratio);gain=std::min(gain,g);for(int c=0;c<2;++c)v[c]+=band[c][j]*g;}}
    else if(type==18){float hi[2];float c=float(1-std::exp(-6.28318530718*std::min(3500.+4500*b,sr*.4)/sr));for(int j=0;j<2;++j){low[j]+=c*(v[j]-low[j]);hi[j]=v[j]-low[j];}gain=compress(follow(linkedEnv,std::max(std::abs(hi[0]),std::abs(hi[1])),.001f,.08f),6);for(int j=0;j<2;++j)v[j]=low[j]+hi[j]*gain;}
    else if(type==17){float f=follow(fastEnv[0],peak,.001f,.02f),s=follow(slowEnv[0],peak,.035f,.18f);gain=std::clamp(1+(2*a-1)*4*(f-s)/std::max(.02f,s)+(2*b-1)*.5f,.1f,4.f);for(auto& x:v)x*=gain;}
    else{float level=peak;if(type==11){level=std::sqrt(std::max(0.f,follow(linkedEnv,peak*peak,.025f,.35f)));}else if(type==12){level=follow(linkedEnv,peak,.015f,.25f); }else if(type!=14){level=follow(linkedEnv,peak,type==10?.0004f:.005f,type==10?.045f:.12f);}
      if(type==14){gain=peak>threshold?threshold/std::max(peak,1e-9f):1.f;}
      else if(type==15){gain=level<threshold?std::pow(std::max(level/threshold,1e-6f),1+3*b):1.f;}
      else if(type==16){float desired=level>=threshold?1.f:std::pow(10.f,-(20+60*b)/20);gain=follow(env[0],desired,.003f,.045f);}
      else gain=compress(level,type==12?1+(ratio-1)*std::clamp(level,0.f,1.f):ratio);
      for(auto& x:v)x*=gain;
    }
    reduction=std::max(0.f,-20*std::log10(std::max(gain,1e-6f)));if(p.autoGain&&type>=9&&type<=13){float makeup=std::min(4.f,std::pow(1/std::max(threshold,.001f),(1-1/ratio)*.25f));for(auto& x:v)x*=makeup;}
  }
  else if(type>=79){creative.tick(v,type,a,b,p.sync,p.bpm,p.delayFreeze,sr);space.phase=creative.activity;}
  else if(type>=64){reverb.tick(v,type,a,b,p,sr);space.phase=reverb.activity;}
  else if(type>=49){echo.tick(v,type,a,b,p,sr);space.phase=echo.phase;}
  else if(type>=29){space.tick(v,type,a,b,p.stereo,p.sync,p.bpm,sr);}
  else if(type>=19){for(int c=0;c<2;++c){float x=v[c],out=0;int n=p.quality==2?4:p.quality==1?2:1;for(int j=1;j<=n;++j)out+=analogShape(type,std::lerp(previous[c],x,float(j)/n),a,b);previous[c]=x;v[c]=out/n;if(p.autoGain)v[c]/=std::sqrt(1+15*a);}}
  else for(int c=0;c<2;++c){float x=v[c];switch(type){
   case 1:v[c]=x*(.125f+3.875f*a);break;
   case 2:{float drive=1+19*a;v[c]=std::tanh(x*drive)/std::tanh(drive);break;}
   case 3:{float drive=1+15*a;v[c]=std::clamp(x*drive,-1.f,1.f);break;}
   case 4:low[c]+=coeff*(x-low[c]);v[c]=low[c];break;
   case 5:low[c]+=coeff*(x-low[c]);v[c]=x-low[c];break;
   case 6:{float levels=std::exp2(2+14*(1-a));v[c]=std::round(x*levels)/levels;break;}
   default:break;
  }}
  l=std::lerp(dry[0],v[0]*p.output,wet);r=std::lerp(dry[1],v[1]*p.output,wet);
 }
};
struct FxRackState{
 std::array<FxSlotState,fxSlots> slots{};
 bool active(const FxRack& p)const noexcept{for(int j=0;j<fxSlots;++j)if(slots[j].active(p.slots[j]))return true;return false;}
 void tick(float& l,float& r,const FxRack& p,double sr,const std::array<float,8>& macros)noexcept{
  if(!p.parallel){for(int j=0;j<fxSlots;++j)slots[j].tick(l,r,p.slots[j],sr,macros);return;}
  float sl=0,srOut=0;int count=0;for(int j=0;j<fxSlots;++j)if(slots[j].active(p.slots[j])){float a=l,b=r;slots[j].tick(a,b,p.slots[j],sr,macros);sl+=a;srOut+=b;++count;}if(count){l=sl/count;r=srOut/count;}
 }
};
}
