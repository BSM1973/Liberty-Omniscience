#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <complex>
#include "CosmosSIMD.h"
namespace cosmos {
// Streaming WOLA processor. 512 samples of intrinsic latency, 128-sample hop.
// The held spectral reference is captured from the same slot, not an external sidechain.
struct CreativeFFT {
 static constexpr int maxN=4096,maxBins=maxN/2+1;int n=512,hop=128,bins=257;
 using C=std::complex<float>;
 static constexpr float pi=3.14159265358979323846f,tau=2*pi;
 struct Channel {
  std::array<float,maxN> input{},output{},real{};
  std::array<C,maxN> work{},original{};
  std::array<float,maxBins> held{},prevPhase{},heldPhase{},advance{},envelope{},carrierPhase{},pitchPhase{},harmonyPhase{};
  bool captured=false,wasFreeze=false,phaseReady=false;
 };
 std::vector<Channel> channels;
 struct Plan{std::array<std::array<float,maxN>,4> window{};std::array<C,maxN/2> twiddle{};Plan(){for(int q=0;q<4;++q){int size=512<<q;for(int j=0;j<size;++j)window[q][j]=.5f-.5f*std::cos(tau*j/size);}for(int j=0;j<maxN/2;++j)twiddle[j]=C(std::cos(-tau*j/maxN),std::sin(-tau*j/maxN));}};
 static const Plan& plan(){static const Plan p;return p;}
 const Plan* preparedPlan=nullptr;int quality=0;
 int pos=0,filled=0,clock=0;
 void prepare(){
  channels.resize(2);
  preparedPlan=&plan();
  reset();
 }
 void reset()noexcept{pos=filled=clock=0;for(auto& c:channels)c=Channel{};}
 void select(int size)noexcept{size=std::clamp(size,512,maxN);if(size!=n){n=size;hop=n/4;bins=n/2+1;quality=n==512?0:n==1024?1:n==2048?2:3;reset();}}
 template<size_t N> void fft(std::array<C,N>& a,bool inverse)const noexcept{
  const int n=std::min(int(N),this->n);
  for(int i=1,j=0;i<n;++i){int bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
  for(int len=2;len<=n;len*=2)for(int i=0;i<n;i+=len)for(int j=0;j<len/2;++j){
   C w=preparedPlan->twiddle[j*maxN/len];if(inverse)w=std::conj(w);const C u=a[i+j],v=a[i+j+len/2]*w;
   a[i+j]=u+v;a[i+j+len/2]=u-v;
  }
  if(inverse)for(int j=0;j<n;++j)a[j]/=float(n);
 }
 static float wrap(float p)noexcept{return std::remainder(p,tau);}
 float magnitudeAt(const std::array<float,maxBins>& a,float x)const noexcept{
  if(x<0||x>bins-1)return 0;int k=int(x);return std::lerp(a[k],a[std::min(k+1,bins-1)],x-k);
 }
 static void triangularBlur(const float* mag,float* result,int bins,int radius)noexcept{
  std::array<double,maxBins+1> sums{},weighted{};
  for(int k=0;k<bins;++k){sums[k+1]=sums[k]+mag[k];weighted[k+1]=weighted[k]+double(k)*mag[k];}
  for(int k=0;k<bins;++k){int lo=std::max(0,k-radius),hi=std::min(bins-1,k+radius);
   double left=(radius+1-k)*(sums[k+1]-sums[lo])+weighted[k+1]-weighted[lo];
   double right=(radius+1+k)*(sums[hi+1]-sums[k+1])-weighted[hi+1]+weighted[k+1];
   int l=k-lo,r=hi-k;double weight=double(radius+1)*(1+l+r)-.5*(l*(l+1)+r*(r+1));
   result[k]=float(std::max(0.,(left+right)/weight));
  }
 }
 void frame(Channel& c,int type,float a,float b,bool freeze,float sr)noexcept{
  const auto& window=preparedPlan->window[quality];for(int j=0;j<n;++j)c.work[j]=C(c.input[(pos+j)%n]*window[j],0);
  fft(c.work,false);std::copy_n(c.work.begin(),n,c.original.begin());
  std::array<float,maxBins> mag{},arg{},step{},blurred{};
  const bool pitch=type>=85&&type<=87,needsPhase=type==79||pitch;const bool firstPhase=!c.phaseReady;
  float total=0;
  for(int k=0;k<bins;++k){
   mag[k]=std::abs(c.original[k]);if(needsPhase)arg[k]=std::arg(c.original[k]);total+=mag[k];
   float expected=tau*k*hop/n;
   if(needsPhase){step[k]=c.phaseReady?expected+wrap(arg[k]-c.prevPhase[k]-expected):expected;c.prevPhase[k]=arg[k];}
  }
  c.phaseReady=true;
  if(!freeze&&c.wasFreeze)c.captured=false;
  if((type==79||type==81)&&(!c.captured||(freeze&&!c.wasFreeze))&&total>1e-4f){
   c.held=mag;c.heldPhase=arg;c.advance=step;c.captured=true;
  }
  c.wasFreeze=freeze;
  if(pitch||type==88||type==89){
   const int radius=std::max(2,int(400*n/sr));float sum=0;int lo=0,hi=std::min(bins-1,radius);for(int j=lo;j<=hi;++j)sum+=mag[j];for(int k=0;k<bins;++k){int l=std::max(0,k-radius),h=std::min(bins-1,k+radius);while(lo<l)sum-=mag[lo++];while(hi<h)sum+=mag[++hi];c.envelope[k]=std::max(0.f,sum/(hi-lo+1));}
  }
  if(type==80&&b>0)triangularBlur(mag.data(),blurred.data(),bins,1+int(a*12*n/512.f));
  const float pitchRatio=std::exp2(std::round((2*a-1)*12.f)/12.f);
  const float gateThreshold=.02f*(n/512.f)*std::pow(1000.f,a),compThreshold=.1f*(n/512.f)*std::pow(100.f,a);
  const int shift=int(std::round((2*a-1)*32*n/512.f));const C shiftPhase=std::polar(1.f,tau*shift*float((clock/hop)%4)*hop/n);
  const auto scaleMag=[](C z,float from,float to){return from>1e-12f?z*(to/from):C{};};
  for(int k=0;k<bins;++k){
   float m=mag[k],ph=arg[k];C z=c.original[k];
   switch(type){
    case 79:if(c.captured){c.heldPhase[k]=wrap(c.heldPhase[k]+c.advance[k]);z=std::lerp(0.f,1.f,freeze?1.f:a)*std::polar(c.held[k],c.heldPhase[k])+(1-(freeze?1.f:a))*z;}break;
    case 80:if(b>0)z=scaleMag(z,m,std::lerp(m,blurred[k],b));break;
    case 81:if(c.captured){z=scaleMag(z,m,std::lerp(m,c.held[k],a));}break;
    case 82:{int src=k-shift;z=src>=0&&src<bins?c.original[src]*shiftPhase:C{};z=(1-b)*c.original[k]+b*z;break;}
    case 83:{float threshold=gateThreshold;float g=m>=threshold?1.f:std::pow(m/std::max(threshold,1e-8f),1+7*b);z*=g;break;}
    case 84:{float threshold=compThreshold;if(m>threshold)z*=std::pow(threshold/m,1-1/(1+15*b));break;}
    case 85:case 86:case 87:{
     auto voice=[&](float ratio,std::array<float,maxBins>& phase){
      float src=k/ratio;if(src>bins-1)return C{};int j=int(src);float frac=src-j;int next=std::min(j+1,bins-1);
      float m=std::lerp(mag[j],mag[next],frac);
      // Interpolate instantaneous frequency (rather than wrapping absolute bin phases).
      float increment=std::lerp(step[j],step[next],frac)*ratio;
      if(firstPhase)phase[k]=arg[j];else phase[k]=wrap(phase[k]+increment);
      if(type==85){float old=magnitudeAt(c.envelope,src),target=c.envelope[k];m*=std::clamp(target/std::max(old,.015f),.5f,2.f);}
      return std::polar(m,phase[k]);
     };
     float ratio=type==87?(a<.5f?.5f:2.f):pitchRatio;
     C shifted=voice(ratio,c.pitchPhase);
     if(type==86)shifted=.55f*(shifted+voice(1.5f,c.harmonyPhase));
     z=(1-b)*z+b*shifted;break;}
    case 88:{float ratio=std::exp2(2*a-1);float target=magnitudeAt(c.envelope,k/ratio);float gain=std::clamp(target/std::max(.02f,c.envelope[k]),.1f,5.f);z*=std::lerp(1.f,gain,b);break;}
    case 89:{float fundamental=55.f*std::exp2(3*a);float hz=k*sr/n;int harmonic=std::max(1,int(std::round(hz/fundamental)));float center=harmonic*fundamental*n/sr;float distance=std::abs(k-center);float carrier=std::exp(-distance*distance*.7f)/std::sqrt(float(harmonic));c.carrierPhase[k]=wrap(c.carrierPhase[k]+tau*harmonic*fundamental*hop/sr);z=std::polar(c.envelope[k]*carrier*3.f,c.carrierPhase[k]);z=(1-b)*c.original[k]+b*z;break;}
    default:break;
   }
   if(k==0||k==n/2)z=C(z.real(),0);
   c.work[k]=z;if(k>0&&k<n/2)c.work[n-k]=std::conj(z);
  }
  fft(c.work,true);
  for(int j=0;j<n;++j)c.real[j]=c.work[j].real();int first=n-pos;fxWindowAdd(c.output.data()+pos,c.real.data(),window.data(),first,2);fxWindowAdd(c.output.data(),c.real.data()+first,window.data()+first,pos,2);
 }
 void tick(float* v,int type,float a,float b,bool freeze,double sr)noexcept{
  if(channels.size()!=2)return;
  for(int c=0;c<2;++c){auto& s=channels[c];float in=v[c];v[c]=s.output[pos];s.output[pos]=0;s.input[pos]=in;}
  pos=(pos+1)%n;filled=std::min(n,filled+1);
  if(++clock>=1000000000)clock=0;
  if(filled==n&&clock%hop==0)for(auto& c:channels)frame(c,type,a,b,freeze,float(sr));
 }
};
inline int fftSize(int quality)noexcept{return 512<<std::clamp(quality,0,3);}
inline bool creativeSpectral(int type)noexcept{return type>=79&&type<=89;}
inline bool creativeSync(int type)noexcept{return type>=92&&type<=94;}
inline bool creativeFreeze(int type)noexcept{return type==79||type==81;}
inline constexpr const char* creativeA[22]={"Maintien","Largeur","Morphing","Décalage","Seuil","Seuil","Intervalle","Intervalle","Octave","Formants","Porteuse","Taille grain","Vitesse","Durée","Durée","Durée","Fréquence","Fondamentale","Fréquence","Largeur","Attaque","Couleur"};
inline constexpr const char* creativeB[22]={"Réserve","Intensité","Réserve","Intensité","Pente","Ratio","Intensité","Harmonie","Intensité","Intensité","Intensité","Dispersion","Texture","Répétitions","Variation","Répétitions","Feedback","Résonance","Résonance","Balance","Sustain","Intensité"};
inline std::array<float,2> creativePreset(int type,int profile)noexcept{
 constexpr float q[22][2]={{.78f,.5f},{.55f,.8f},{.65f,.5f},{.64f,.85f},{.52f,.8f},{.48f,.7f},{.72f,.85f},{.72f,.65f},{.75f,.7f},{.66f,.75f},{.45f,.9f},{.55f,.6f},{.35f,.7f},{.42f,.6f},{.47f,.7f},{.56f,.7f},{.48f,.65f},{.45f,.65f},{.55f,.6f},{.75f,.5f},{.75f,.45f},{.55f,.7f}};
 const int k=std::clamp(type-79,0,21);float d=profile==1?-.12f:profile==3?.12f:0;
 return {std::clamp(q[k][0]+d,0.f,1.f),std::clamp(q[k][1]+d,0.f,1.f)};
}
struct CreativeState{
 CreativeFFT spectral;
 std::array<std::vector<float>,2> ring;
 std::array<std::array<float,6>,2> reson1{},reson2{};
 std::array<float,2> fast{},slow{},previous{},filter{},comb{};
 int write=0,valid=0,cycle=0;float phase=0,phase2=0,activity=0;
 void prepare(double sr=48000){spectral.prepare();for(auto& x:ring)x.resize(size_t(std::ceil(std::max(8000.,sr)*.6))+8);reset();}
 void reset()noexcept{spectral.reset();write=valid=cycle=0;phase=phase2=activity=0;reson1={};reson2={};fast={};slow={};previous={};filter={};comb={};}
 static float safe(float x)noexcept{return std::isfinite(x)?std::clamp(x,-4.f,4.f):0.f;}
 float read(int c,float d)const noexcept{
  int n=int(ring[c].size());if(n<4)return 0;d=std::clamp(d,1.f,float(n-2));int j=int(d);float f=d-j;
  float x=j<=valid?ring[c][(write-j+n)%n]:0,y=j+1<=valid?ring[c][(write-j-1+n)%n]:0;
  const float z=j>1&&j-1<=valid?ring[c][(write-j+1+n)%n]:x,w=j+2<=valid?ring[c][(write-j-2+n)%n]:y;return safe(x+.5f*f*(y-z+f*(2*z-5*x+4*y-w+f*(3*(x-y)+w-z))));
 }
 float grains(int c,float ph,float span,float base,float ratio)const noexcept{
  float q=ph+.5f;if(q>=1)q-=1;
  auto tap=[&](float z){return (.5f-.5f*std::cos(CreativeFFT::tau*z))*read(c,base+span*(1-ratio)*z);};
  return tap(ph)+tap(q);
 }
 static void advance(float& p,float d)noexcept{p+=d;p-=std::floor(p);}
 float reson(int c,int k,float x,float freq,float radius,float sr)noexcept{
  freq=std::clamp(freq,20.f,sr*.44f);float g=2*radius*std::cos(CreativeFFT::tau*freq/sr);
  float y=safe((1-radius)*x+g*reson1[c][k]-radius*radius*reson2[c][k]);
  reson2[c][k]=reson1[c][k];reson1[c][k]=y;return y;
 }
 void tick(float* v,int type,float a,float b,bool sync,double bpm,bool freeze,double sr,int fftQuality=0)noexcept{
  if(ring[0].empty())return;
  const float rate=float(sr),tau=CreativeFFT::tau;
  if(creativeSpectral(type)){spectral.select(fftSize(fftQuality));spectral.tick(v,type,a,b,freeze,sr);for(int c=0;c<2;++c)v[c]=safe(v[c]);}
  else{
   float in[2]={safe(v[0]),safe(v[1])},store[2]={in[0],in[1]};
   const float span=rate*(.025f+.075f*a),pitchSpan=rate*.06f;
   const float ratio=std::exp2((std::round((2*a-1)*12.f))/12.f);
   const float octave=a<.5f?.5f:2.f;
   const float stretch=std::exp2(2*a-1);
   const float seconds=sync?std::min(.28f,float(60./std::clamp(bpm,20.,400.))*std::exp2(-4+std::round(a*3))):.025f+.225f*a;
   const int period=std::max(32,int(rate*seconds)),repeats=2+int(6*b),unit=std::max(8,period/repeats);
   const float freq=45.f*std::exp2(6*a);
   for(int c=0;c<2;++c){float x=in[c],y=x;
    switch(type){
     case 85:y=std::lerp(x,grains(c,phase,pitchSpan,pitchSpan+2,ratio),b);break;
     case 86:y=(1-b)*x+b*.55f*(grains(c,phase,pitchSpan,pitchSpan+2,ratio)+grains(c,phase2,pitchSpan,pitchSpan+2,1.5f));break;
     case 87:y=std::lerp(x,grains(c,phase,pitchSpan,pitchSpan+2,octave),b);break;
     case 90:{float z=.65f*grains(c,phase,span,span+2,1.2f+b*.6f)+.35f*grains(1-c,phase2,span,span+2,.65f+.3f*b);float k=std::min(1.f,6.2831853f*std::min(rate*.42f,12000.f)/rate);filter[c]+=k*(z-filter[c]);y=filter[c];break;}
     case 91:{float main=grains(c,phase,rate*.09f,rate*.12f,stretch);float second=grains(c,phase2,rate*.067f,rate*.1f,stretch);float onset=std::abs(x-previous[c]);previous[c]=x;fast[c]+=.045f*(onset-fast[c]);slow[c]+=.0015f*(onset-slow[c]);float transient=std::clamp((fast[c]-slow[c])*16.f,0.f,.75f);y=std::lerp(std::lerp(main,second,.5f*b),read(c,rate*.12f),transient);break;}
     case 92:case 93:case 94:{
      int p=cycle%period,sub=p%unit,segment=p/unit;
      if(type==94&&segment==0){y=x;break;}
      int index=sub;if(type==93&&segment%3==1)index=unit-1-sub;
      if(type==93&&segment%3==2)index=(sub*2)%unit;
      const float fade=std::min(1.f,std::min(float(sub+1),float(unit-sub))/std::max(1.f,rate*.0015f));
      float z=read(c,float(unit+p-index));y=std::lerp(x,z*fade,type==94?1.f:.4f+.6f*b);break;}
     case 95:{float z=read(c,rate/std::min(freq,rate*.4f));store[c]=safe(x+.94f*b*z);y=.5f*(x+z);break;}
     case 96:{float z=0;for(int j=0;j<6;++j)z+=reson(c,j,x,freq*(j+1),.92f+.077f*b,rate);y=safe(z*.7f);break;}
     case 97:y=reson(c,0,x*std::sin(tau*phase),freq,.9f+.097f*b,rate);break;
     case 98:{float mid=.5f*(in[0]+in[1]),side=.5f*(in[0]-in[1])*(2.5f*a);y=(mid+(c?-side:side))*(c?std::sqrt(2*b):std::sqrt(2*(1-b)));break;}
     case 99:{float peak=std::abs(x);fast[c]+=std::min(1.f,800.f/rate)*(peak-fast[c]);slow[c]+=std::min(1.f,35.f/rate)*(peak-slow[c]);float attack=(fast[c]-slow[c])/std::max(.025f,slow[c]);y=x*std::clamp(1+3*(2*a-1)*attack+(2*b-1)*.65f,.05f,4.f);break;}
     case 100:{float drive=1+7*a;float shaped=std::tanh(drive*x)/std::tanh(drive);float tap=read(c,rate*(.035f+.075f*a));filter[c]+=.1f*(tap-filter[c]);store[c]=safe(shaped+.6f*b*filter[c]);y=std::lerp(shaped,.55f*shaped+.45f*filter[c],b);break;}
     default:break;
    }
    v[c]=safe(y);
   }
   for(int c=0;c<2;++c)ring[c][write]=store[c];
   if(++write==int(ring[0].size()))write=0;valid=std::min(valid+1,int(ring[0].size())-1);
   if(++cycle>=period)cycle=0;
   advance(phase,type==97?freq/rate:1/std::max(16.f,type==90?span:type==91?rate*.09f:pitchSpan));
   advance(phase2,1/std::max(16.f,type==86?pitchSpan:type==91?rate*.067f:span*.77f));
  }
  activity=std::max(std::max(std::abs(v[0]),std::abs(v[1])),activity*.9995f);
 }
};
}
