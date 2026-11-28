#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>
#include <numbers>
#include <cstdint>
namespace cosmos {
constexpr int fxRacks=5,fxSlots=8,fxTypes=49;
struct FxParams{int type=0;bool enabled=false;float mix=1,input=1,output=1,a=.5f,b=.5f;int macro=0;float depth=0;bool autoGain=false;int quality=0,cc=-1;float ccValue=-1;bool sync=false;float stereo=.5f;double bpm=120;};
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
struct FxSlotState{
 int type=0;float wet=0;std::array<float,2> low{},dc{},env{};float phase=0,reduction=0;std::array<float,2> low2{},previous{},fastEnv{},slowEnv{};std::array<float,3> bandEnv{};float linkedEnv=0;FxSpaceState space;
 void prepare(){space.prepare();clear();wet=0;type=0;}
 void reset()noexcept{clear();wet=0;type=0;}
 void clear()noexcept{low={};dc={};env={};low2={};previous={};fastEnv={};slowEnv={};bandEnv={};linkedEnv=0;phase=0;reduction=0;space.reset();}
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
