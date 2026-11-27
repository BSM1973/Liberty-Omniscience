#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int modEnvCount=12,freeEnvMax=16;
struct FreeEnvelope {bool enabled=false;int count=4;std::array<float,freeEnvMax> x{0,.2f,.65f,1},y{0,1,.3f,.5f};std::array<int,freeEnvMax> curve{};};
inline FreeEnvelope sanitizeFreeEnvelope(FreeEnvelope f)noexcept{f.count=std::clamp(f.count,2,freeEnvMax);f.x[0]=0;f.y[0]=0;f.x[f.count-1]=1;for(int i=1;i<f.count-1;++i)f.x[i]=std::clamp(f.x[i],f.x[i-1]+.002f,1.f-.002f*(f.count-1-i));for(int i=0;i<f.count;++i){f.y[i]=std::clamp(f.y[i],0.f,1.f);f.curve[i]=std::clamp(f.curve[i],0,2);}return f;}
struct ModEnvParams {
    int mode=0,play=0,curve=0,sync=0,destination=0,layer=0;
    float attack=.05f,hold=.1f,decay=.3f,sustain=.5f,release=.4f,segmentTime=.2f,segmentLevel=.7f,depth=0,velocity=0;FreeEnvelope free{};
};
struct ModEnvPlan {
    ModEnvParams p{};std::array<double,5> increment{};std::array<float,freeEnvMax> inverseWidth{};
    ModEnvPlan()=default;
    ModEnvPlan(ModEnvParams x,double sr,double bpm):p(x){
        double unit=p.sync?std::clamp(bpm,20.,400.)/60.:1.;
        std::array<float,5> durations{p.attack,p.hold,p.segmentTime,p.decay,p.release};
        for(int i=0;i<5;++i)increment[i]=std::min(1.,unit/(std::max(8000.,sr)*std::max(.0001,double(durations[i]))));if(p.free.enabled){p.free=sanitizeFreeEnvelope(p.free);for(int i=0;i<p.free.count-1;++i)inverseWidth[i]=1.f/(p.free.x[i+1]-p.free.x[i]);}
    }
};
inline float envelopeCurve(float t,int curve)noexcept{
    t=std::clamp(t,0.f,1.f);return curve==1?t*t:curve==2?std::sqrt(t):t;
}
inline float freeEnvelopeValue(const ModEnvPlan& p,float t)noexcept{const auto& f=p.p.free;int i=0;while(i+1<f.count-1&&t>=f.x[i+1])++i;return std::lerp(f.y[i],f.y[i+1],envelopeCurve((t-f.x[i])*p.inverseWidth[i],f.curve[i]));}
struct ModEnvState {
    enum Stage{attack,hold,segment,decay,sustain,release,off};
    Stage stage=off;float level=0,from=0;double progress=0;bool released=false;
    void start()noexcept{stage=attack;level=from=0;progress=0;released=false;}
    void next(Stage s)noexcept{stage=s;progress=0;from=level;}
    void stop()noexcept{released=true;if(stage!=off&&stage!=release)next(release);}
    float tick(const ModEnvPlan& plan,float velocity)noexcept{
        const auto& p=plan.p;
        if(stage==off)return 0;
        if(p.free.enabled&&stage!=release){if(stage==sustain){level=p.free.y[p.free.count-1];if(p.play!=0)next(release);}else{progress=std::min(1.,progress+plan.increment[2]);level=freeEnvelopeValue(plan,float(progress));if(progress>=1)next(p.play==0?sustain:release);}return level*p.depth*std::lerp(1.f,std::clamp(velocity,0.f,1.f),std::clamp(p.velocity,0.f,1.f));}
        if(stage==sustain){level=p.sustain;if(p.play!=0)next(release);}
        else {
            int index=stage==attack?0:stage==hold?1:stage==segment?2:stage==decay?3:4;
            float to=stage==attack||stage==hold?1.f:stage==segment?p.segmentLevel:stage==decay?p.sustain:0.f;
            progress=std::min(1.,progress+plan.increment[index]);
            level=std::lerp(from,to,envelopeCurve(float(progress),p.curve));
            if(progress>=1){
                if(stage==attack)next(p.mode==0?decay:hold);
                else if(stage==hold)next(p.mode==2?segment:decay);
                else if(stage==segment)next(decay);
                else if(stage==decay)next(p.play==0?sustain:release);
                else if(stage==release){if(p.play==1&&!released)next(attack);else next(off);}
            }
        }
        return level*p.depth*std::lerp(1.f,std::clamp(velocity,0.f,1.f),std::clamp(p.velocity,0.f,1.f));
    }
};
}
