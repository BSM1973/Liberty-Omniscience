#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cosmos {
struct GrainParams {
    float size=100,density=20,position=.5f,spread=.12f,stretch=1, pitchJitter=0,blur=0,loopStart=0,loopEnd=1;
    int window=0,source=0;
    bool freeze=false,reverse=false,sync=false;
    bool experimentalWindow=false;
};
struct Grain {double pos=0,step=0;int age=0,length=0;};
struct GrainVoice {
    std::array<Grain,16> grains{};
    double countdown=0,scan=0;float priorPosition=-1;
    std::uint32_t rng=1;
    float random() noexcept {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng&0xffffff)/8388608.f-1.f;}
    void seed(std::uint32_t n) noexcept {rng=n?n:1;}
};
inline float grainWindow(int type,float x) noexcept {
    if(type==1)return 1.f-std::abs(2.f*x-1.f);
    if(type==2)return std::min(1.f,std::min(x,1.f-x)*8.f);
    return .5f-.5f*float(std::cos(6.283185307179586*x));
}
inline const std::array<float,4097>& hannLut(){
    static const auto values=[]{
        std::array<float,4097> a{};
        for(int i=0;i<=4096;++i)a[i]=grainWindow(0,float(i)/4096.f);
        return a;
    }();return values;
}
inline float fastGrainWindow(int type,float x)noexcept{
    if(type!=0)return grainWindow(type,x);
    const auto& table=hannLut();float p=std::clamp(x,0.f,1.f)*4096.f;
    int i=std::min(int(p),4095);return std::lerp(table[i],table[i+1],p-i);
}
inline double grainWrap(double x,double lo,double hi) noexcept {
    double n=std::max(1.,hi-lo);double y=x-lo;
    // Keep the identical offset arithmetic, avoid fmod inside one cycle.
    if(y>=0&&y<n)return lo+y;
    if(y>=n&&y<2*n)return lo+(y-n);
    if(y<0&&y>=-n)return lo+(y+n);
    y=std::fmod(y,n);return lo+(y<0?y+n:y);
}
template<class Sample>
float renderGrains(GrainVoice& v,const GrainParams& p,const Sample* s,double noteRatio,double sr,double bpm) noexcept {
    if(!s||s->mono.size()<2)return 0;
    double end=double(s->mono.size()),lo=std::clamp(double(p.loopStart),0.,.999)*end;
    double hi=std::clamp(double(p.loopEnd),0.,1.)*end;
    lo=std::min(lo,end-1.);hi=std::clamp(hi,lo+1.,end);
    if(v.priorPosition!=p.position){v.scan=lo+(hi-lo)*p.position;v.priorPosition=p.position;}
    double speed=std::max(.125,double(p.stretch));
    if(!p.freeze)v.scan=grainWrap(v.scan+(p.reverse?-1.:1.)*s->rate/sr/speed,lo,hi);
    double rate=std::clamp(double(p.density),1.,80.);
    if(p.sync)rate*=std::clamp(bpm,20.,300.)/60.;
    if(--v.countdown<=0){
        Grain* target=nullptr;for(auto& g:v.grains)if(g.age>=g.length){target=&g;break;}
        if(target){
            double jitter=(p.spread+p.blur*.3)*v.random()*(hi-lo);
            target->pos=grainWrap(v.scan+jitter,lo,hi);
            target->step=noteRatio*s->rate/sr*std::exp2(v.random()*p.pitchJitter/1200.)*(p.reverse?-1.:1.);
            target->length=std::max(2,int(sr*std::clamp(double(p.size),5.,500.)*.001));
            target->age=0;
        }
        v.countdown=std::max(1.,sr/rate*(1.+p.blur*.45*v.random()));
    }
    float out=0;
    for(auto& g:v.grains)if(g.age<g.length){
        g.pos=grainWrap(g.pos,lo,hi);size_t i=std::min(size_t(g.pos),s->mono.size()-1);
        double next=grainWrap(double(i)+1,lo,hi);size_t j=std::min(size_t(next),s->mono.size()-1);
        float x=std::lerp(s->mono[i],s->mono[j],float(g.pos-double(i)));
        float progress=float(g.age)/float(g.length-1);
        out+=x*(p.experimentalWindow?fastGrainWindow(p.window,progress):grainWindow(p.window,progress));++g.age;g.pos+=g.step;
    }
    double overlap=rate*double(p.size)*.001;
    return out/float(std::max(1.,overlap*.55));
}
}
