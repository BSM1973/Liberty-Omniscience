#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int fxRacks=5,fxSlots=8,fxTypes=9;
struct FxParams{int type=0;bool enabled=false;float mix=1,input=1,output=1,a=.5f,b=.5f;int macro=0;float depth=0;};
struct FxRack{bool parallel=false;std::array<FxParams,fxSlots> slots{};};
struct FxSlotState{
 int type=0;float wet=0;std::array<float,2> low{},dc{},env{};float phase=0;
 void clear()noexcept{low={};dc={};env={};phase=0;}
 bool active(const FxParams& p)const noexcept{return (p.enabled&&p.type>0)||wet>0;}
 void tick(float& l,float& r,const FxParams& p,double sr,const std::array<float,8>& macros)noexcept{
  if(!active(p))return;const int desired=p.enabled?std::clamp(p.type,0,fxTypes-1):0;
  if(type!=desired&&wet<=0){type=desired;clear();}
  float target=type==desired&&type!=0?std::clamp(p.mix,0.f,1.f):0.f;
  float step=float(1/std::max(1.,sr*.005));wet+=std::clamp(target-wet,-step,step);if(type==0||wet<=0)return;
  float a=std::clamp(p.a+(p.macro>0&&p.macro<=8?p.depth*(macros[p.macro-1]-.5f)*2:0),0.f,1.f),b=std::clamp(p.b,0.f,1.f);
  float dry[2]={l,r},v[2]={l*p.input,r*p.input};
  float coeff=float(1-std::exp(-6.28318530718*std::min(20.*std::exp2(10.*a),sr*.45)/sr));
  if(type==7){float m=(v[0]+v[1])*.5f,s=(v[0]-v[1])*.5f*(2*a);v[0]=m+s;v[1]=m-s;}
  else if(type==8){float mod=1-b*.5f+b*.5f*std::sin(phase*6.28318530718f);phase+=float((.1+19.9*a)/sr);if(phase>=1)phase-=1;v[0]*=mod;v[1]*=mod;}
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
