#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>
#include <cstdint>
#include "GranularEngine.h"

namespace cosmos {
constexpr int oscillatorCount=5, maxUnison=16, tableSize=2048, tableFrames=16, tableBands=8;
constexpr double hpi=std::numbers::pi_v<double>;
enum class OscMode { analog, wavetable, fm, am, sample, granular };
struct SampleData { std::vector<float> mono; double rate=48000.; };
inline const std::array<SampleData,4>& factoryGrains(){
    static const auto bank=[]{
        std::array<SampleData,4> result;
        for(int k=0;k<4;++k){
            auto& s=result[k];s.rate=24000;s.mono.resize(48000);
            std::uint32_t noise=12345u+unsigned(k)*971;
            for(size_t i=0;i<s.mono.size();++i){
                double t=double(i)/s.rate;noise=noise*1664525u+1013904223u;
                double n=double(noise>>8)/8388608.-1.;
                double ph=2*hpi*261.625565*t;
                double x=0;
                if(k==0)x=.45*std::sin(ph)+.2*std::sin(ph*2.001)+.12*std::sin(ph*3.007);
                if(k==1)x=.38*std::sin(ph+2.5*std::sin(ph*1.498)*std::exp(-t))+.12*n*std::exp(-2*t);
                if(k==2)x=.27*std::sin(ph*.5)+.2*std::sin(ph*1.003)+.16*std::sin(ph*1.498)+.08*n;
                if(k==3)x=.38*std::sin(ph*2)*std::exp(-t*1.4)+.2*std::sin(ph*3.83)*std::exp(-t*2)+.08*std::sin(ph);
                double fade=std::min(1.,std::min(t,2.-t)*50.);
                s.mono[i]=float(x*fade);
            }
        }
        return result;
    }();return bank;
}
struct OscParams {
    OscMode mode=OscMode::analog;
    int wave=0, unison=1;
    float level=0, tune=0, detune=0, drift=0, morph=0, ratio=2, depth=0, drive=0;
    bool sync=false, ring=false;
    GrainParams grain{};
};
inline double wrap(double x) noexcept {return x-std::floor(x);}
inline float wavetableHarmonic(int frame,int n) noexcept {
    float t=float(frame)/float(tableFrames-1);
    const float sine=n==1?1.f:0.f;
    const float saw=(n%2==1?1.f:-1.f)/float(n);
    const float square=n%2==1?1.f/float(n):0.f;
    const float triangle=n%2==1?((n%4==1?1.f:-1.f)/float(n*n)):0.f;
    if(t<1.f/3) return std::lerp(sine,saw,t*3);
    if(t<2.f/3) return std::lerp(saw,square,(t-1.f/3)*3);
    return std::lerp(square,triangle,(t-2.f/3)*3);
}
class WavetableBank {
    std::array<std::array<std::array<float,tableSize+1>,tableFrames>,tableBands> tables{};
public:
    WavetableBank() {
        for(int b=0;b<tableBands;++b){
            int harmonics=1<<b;
            for(int f=0;f<tableFrames;++f){
                auto& t=tables[b][f];float peak=.000001f;
                for(int i=0;i<tableSize;++i){
                    double p=2*hpi*i/tableSize;float sum=0;
                    for(int n=1;n<=harmonics;++n)sum+=wavetableHarmonic(f,n)*float(std::sin(p*n));
                    t[i]=sum;peak=std::max(peak,std::abs(sum));
                }
                for(int i=0;i<tableSize;++i)t[i]/=peak;
                t[tableSize]=t[0];
            }
        }
    }
    float read(double phase,float morph,double delta)const noexcept {
        int band=0;const double limit=.47/std::max(delta,1.e-12);
        while(band+1<tableBands && double(1<<(band+1))<=limit)++band;
        const float f=std::clamp(morph,0.f,1.f)*(tableFrames-1);
        const int fi=std::min(int(f),tableFrames-2);
        const double p=wrap(phase)*tableSize;
        const int i=std::min(int(p),tableSize-1);
        const float frac=float(p-i);
        auto at=[&](int frame){const auto& a=tables[band][frame];return std::lerp(a[i],a[i+1],frac);};
        return std::lerp(at(fi),at(fi+1),f-fi);
    }
};
inline const WavetableBank& waveBank(){static const WavetableBank bank;return bank;}
struct HybridVoice {
    std::array<double,maxUnison> phase{}, mod{}, samplePos{}, driftPhase{};
    double syncPhase=0;
    GrainVoice grain;
    void seed(int osc,int layer,int note,std::uint64_t age) noexcept {
        grain.seed(std::uint32_t(age*131u+note*17u+layer*41u+osc*71u+1u));
        for(int u=0;u<maxUnison;++u){
            auto seed=(age*31u+std::uint64_t(note*13+layer*41+osc*71+u*109))%1021u;
            driftPhase[u]=double(seed)/1021.;
            phase[u]=u==0?0.:double(u)/maxUnison;
        }
    }
};
inline bool legacy(const OscParams& o) noexcept {
    return o.mode==OscMode::analog && o.unison==1 && o.level==1 && o.tune==0 && o.drift==0 &&
           o.drive==0 && !o.sync && !o.ring;
}
inline float sampleRead(const SampleData* s,double pos) noexcept {
    if(!s||s->mono.empty()||pos<0||pos>=double(s->mono.size()))return 0;
    const auto i=std::size_t(pos),j=std::min(i+1,s->mono.size()-1);
    return std::lerp(s->mono[i],s->mono[j],float(pos-double(i)));
}
template<class Analog>
inline float renderHybrid(HybridVoice& v,const OscParams& o,const SampleData* sample,double noteDelta,double sr,Analog analog,double bpm=120.) noexcept {
    if(o.level<=0)return 0;
    int un=std::clamp(o.unison,1,maxUnison);
    const double base=std::clamp(noteDelta*std::exp2(double(o.tune)/12.),0.,.45);
    if(o.mode==OscMode::granular){
        float x=renderGrains(v.grain,o.grain,sample,base*sr/261.625565,sr,bpm);
        if(o.ring){x*=float(std::sin(2*hpi*v.mod[0]));v.mod[0]=wrap(v.mod[0]+std::min(.45,base*double(o.ratio)));}
        if(o.drive>0){float d=1+12*o.drive;x=std::tanh(d*x)/std::tanh(d);}
        return x*o.level;
    }
    v.syncPhase+=noteDelta;
    bool reset=v.syncPhase>=1.;if(reset)v.syncPhase-=1.;
    float sum=0;
    for(int u=0;u<un;++u){
        double spread=un==1?0.:2.*u/(un-1)-1.;
        double drift=0;
        if(o.drift>0){
            drift=std::sin(2*hpi*v.driftPhase[u])*o.drift;
            v.driftPhase[u]=wrap(v.driftPhase[u]+(.25+.037*u)/sr);
        }
        const double dt=std::clamp(base*std::exp2((spread*o.detune+drift)/1200.),0.,.45);
        if(o.sync&&reset)v.phase[u]=0;
        float m=float(std::sin(2*hpi*v.mod[u])),x=0;
        switch(o.mode){
            case OscMode::wavetable:x=waveBank().read(v.phase[u],o.morph,dt);break;
            case OscMode::fm:x=float(std::sin(2*hpi*v.phase[u]+double(o.depth)*6.*m));break;
            case OscMode::am:x=analog(o.wave,v.phase[u],dt)*(1.f-o.depth*.5f+o.depth*.5f*m);break;
            case OscMode::sample:
                x=sampleRead(sample,v.samplePos[u]);
                if(sample)v.samplePos[u]+=dt*sr/261.625565*sample->rate/sr;
                break;
            default:x=analog(o.wave,v.phase[u],dt);break;
        }
        if(o.ring)x*=m;
        if(o.drive>0){float d=1.f+o.drive*12.f;x=std::tanh(d*x)/std::tanh(d);}
        v.phase[u]=wrap(v.phase[u]+dt);
        v.mod[u]=wrap(v.mod[u]+std::min(.45,dt*double(o.ratio)));
        sum+=x;
    }
    return sum*o.level/float(un);
}
}
