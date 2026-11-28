#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int fxRacks=5,fxSlots=8,fxTypes=29;
struct FxParams{int type=0;bool enabled=false;float mix=1,input=1,output=1,a=.5f,b=.5f;int macro=0;float depth=0;bool autoGain=false;int quality=0,cc=-1;float ccValue=-1;};
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
struct FxSlotState{
 int type=0;float wet=0;std::array<float,2> low{},dc{},env{};float phase=0,reduction=0;std::array<float,2> low2{},previous{},fastEnv{},slowEnv{};std::array<float,3> bandEnv{};float linkedEnv=0;
 void clear()noexcept{low={};dc={};env={};low2={};previous={};fastEnv={};slowEnv={};bandEnv={};linkedEnv=0;phase=0;reduction=0;}
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
