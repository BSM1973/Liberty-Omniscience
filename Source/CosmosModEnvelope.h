#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int modEnvCount=12;
struct ModEnvParams {
    int mode=0,play=0,curve=0,sync=0,destination=0,layer=0;
    float attack=.05f,hold=.1f,decay=.3f,sustain=.5f,release=.4f,segmentTime=.2f,segmentLevel=.7f,depth=0,velocity=0;
};
struct ModEnvPlan {
    ModEnvParams p{};std::array<double,5> increment{};
    ModEnvPlan()=default;
    ModEnvPlan(ModEnvParams x,double sr,double bpm):p(x){
        double unit=p.sync?std::clamp(bpm,20.,400.)/60.:1.;
        std::array<float,5> durations{p.attack,p.hold,p.segmentTime,p.decay,p.release};
        for(int i=0;i<5;++i)increment[i]=std::min(1.,unit/(std::max(8000.,sr)*std::max(.0001,double(durations[i]))));
    }
};
inline float envelopeCurve(float t,int curve)noexcept{
    t=std::clamp(t,0.f,1.f);return curve==1?t*t:curve==2?std::sqrt(t):t;
}
struct ModEnvState {
    enum Stage{attack,hold,segment,decay,sustain,release,off};
    Stage stage=off;float level=0,from=0;double progress=0;bool released=false;
    void start()noexcept{stage=attack;level=from=0;progress=0;released=false;}
    void next(Stage s)noexcept{stage=s;progress=0;from=level;}
    void stop()noexcept{released=true;if(stage!=off&&stage!=release)next(release);}
    float tick(const ModEnvPlan& plan,float velocity)noexcept{
        const auto& p=plan.p;
        if(stage==off)return 0;
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
