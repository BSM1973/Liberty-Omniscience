#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>
#include <cstdint>
#include <complex>
#include <stdexcept>
#include "CosmosSIMD.h"
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
class HDTable;
struct OscParams {
    OscMode mode=OscMode::analog;
    int wave=0, unison=1;
    float level=0, tune=0, detune=0, drift=0, morph=0, ratio=2, depth=0, drive=0;
    bool sync=false, ring=false;
    GrainParams grain{};
    int tableSource=0;
    bool experimentalGrouped=false;
    int experimentalInterpolation=0;
    const HDTable* userTable=nullptr;
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
struct TableReadPlan {
    const float* a=nullptr;const float* b=nullptr;float morph=0;
    float read(double phase)const noexcept{
        if(!a)return 0;
        // Phase is already normalized by the phase kernel.
        double p=phase*tableSize;int i=std::min(int(p),tableSize-1);float frac=float(p-i);
        return std::lerp(std::lerp(a[i],a[i+1],frac),std::lerp(b[i],b[i+1],frac),morph);
    }
};
inline int harmonicBand(double delta) noexcept {
    int b=0;double limit=.47/std::max(delta,1.e-12);
    while(b+1<tableBands&&double(1<<(b+1))<=limit)++b;return b;
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
    TableReadPlan plan(float morph,double delta)const noexcept{
        int band=harmonicBand(delta);float f=std::clamp(morph,0.f,1.f)*(tableFrames-1);
        int fi=std::min(int(f),tableFrames-2);
        return {tables[band][fi].data(),tables[band][fi+1].data(),f-fi};
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
constexpr int hdFrames=256;
class HDTable {
    std::vector<float> data;
    using Complex=std::complex<double>;
    static void fft(std::array<Complex,tableSize>& a,bool inverse){
        for(unsigned i=1,j=0;i<tableSize;++i){
            unsigned bit=tableSize/2;
            for(;j&bit;bit>>=1)j^=bit;
            j^=bit;if(i<j)std::swap(a[i],a[j]);
        }
        for(unsigned len=2;len<=tableSize;len*=2){
            auto w0=std::polar(1.,(inverse?2.:-2.)*hpi/len);
            for(unsigned i=0;i<tableSize;i+=len){
                Complex w=1.;
                for(unsigned j=0;j<len/2;++j){
                    auto u=a[i+j],v=a[i+j+len/2]*w;
                    a[i+j]=u+v;a[i+j+len/2]=u-v;w*=w0;
                }
            }
        }
        if(inverse)for(auto& x:a)x/=tableSize;
    }
    float* frame(int band,int f){return data.data()+(size_t(band)*hdFrames+f)*(tableSize+1);}
    const float* frame(int band,int f)const{return data.data()+(size_t(band)*hdFrames+f)*(tableSize+1);}
public:
    static constexpr size_t bytes=size_t(tableBands)*hdFrames*(tableSize+1)*sizeof(float);
    // The 256 factory frame grid is exactly 17 subdivisions of each legacy
    // interval (255 = 15 * 17), so linear interpolation needs no duplicate table.
    HDTable()=default;
    size_t allocatedBytes()const noexcept{return data.capacity()*sizeof(float);}
    explicit HDTable(const std::vector<float>& samples):data(bytes/sizeof(float)){
        if(samples.size()<tableSize||samples.size()%tableSize||samples.size()>size_t(hdFrames)*tableSize)throw std::invalid_argument("Wavetable length");
        int frames=int(samples.size()/tableSize);
        std::vector<std::array<Complex,tableSize>> spectra(static_cast<size_t>(frames));
        for(int f=0;f<frames;++f){
            auto& a=spectra[size_t(f)];
            for(int i=0;i<tableSize;++i){float x=samples[size_t(f)*tableSize+i];a[size_t(i)]=std::isfinite(x)?std::clamp(x,-1.f,1.f):0.f;}
            fft(a,false);a[0]=0;
        }
        for(int f=0;f<hdFrames;++f){
            double q=double(f)*(frames-1)/(hdFrames-1);
            int lo=int(q),hi=std::min(lo+1,frames-1);double mix=q-lo;
            for(int b=0;b<tableBands;++b){
                std::array<Complex,tableSize> a{};int harmonics=1<<b;
                for(int k=1;k<=harmonics;++k){
                    a[size_t(k)]=spectra[size_t(lo)][size_t(k)]*(1.-mix)+spectra[size_t(hi)][size_t(k)]*mix;
                    a[tableSize-size_t(k)]=std::conj(a[size_t(k)]);
                }
                fft(a,true);auto* out=frame(b,f);
                for(int i=0;i<tableSize;++i)out[i]=float(a[size_t(i)].real());
                out[tableSize]=out[0];
            }
        }
    }
    TableReadPlan plan(float morph,double delta)const noexcept{
        if(data.empty())return waveBank().plan(morph,delta);
        int band=harmonicBand(delta);float f=std::clamp(morph,0.f,1.f)*(hdFrames-1);
        int fi=std::min(int(f),hdFrames-2);
        return {frame(band,fi),frame(band,fi+1),f-fi};
    }
    float read(double phase,float morph,double delta)const noexcept{
        if(data.empty())return waveBank().read(phase,morph,delta);
        int b=0;double limit=.47/std::max(delta,1.e-12);
        while(b+1<tableBands&&double(1<<(b+1))<=limit)++b;
        float q=std::clamp(morph,0.f,1.f)*(hdFrames-1);
        int f=std::min(int(q),hdFrames-2);double p=wrap(phase)*tableSize;
        int i=std::min(int(p),tableSize-1);float frac=float(p-i);
        auto* a=frame(b,f);auto* c=frame(b,f+1);
        return std::lerp(std::lerp(a[i],a[i+1],frac),std::lerp(c[i],c[i+1],frac),q-f);
    }
};
inline const HDTable& hdBank(){static const HDTable bank;return bank;}
inline float readTable(const OscParams& o,double phase,double delta) noexcept {
    if(o.tableSource==2)return o.userTable?o.userTable->read(phase,o.morph,delta):0.f;
    if(o.tableSource==1)return hdBank().read(phase,o.morph,delta);
    return waveBank().read(phase,o.morph,delta);
}
inline TableReadPlan planTable(const OscParams& o,double delta)noexcept{
    if(o.tableSource==2)return o.userTable?o.userTable->plan(o.morph,delta):TableReadPlan{};
    if(o.tableSource==1)return hdBank().plan(o.morph,delta);
    return waveBank().plan(o.morph,delta);
}
struct HybridVoice {
    std::array<double,maxUnison> phase{}, mod{}, samplePos{}, driftPhase{};
    double syncPhase=0;
    std::array<double,maxUnison> cachedDelta{};
    double cacheBase=-1;float cacheDetune=-1;int cacheUnison=0;
    std::array<double,maxUnison> cachedModDelta{};
    std::array<TableReadPlan,maxUnison> cachedRead{};
    float cacheMorph=-1,cacheRatio=-1;int cacheSource=-1;
    const HDTable* cacheTable=nullptr;
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
inline bool canGroupTable(const OscParams& o)noexcept{
    return o.experimentalGrouped&&o.mode==OscMode::wavetable&&o.drift==0&&!o.sync&&!o.ring&&o.drive==0;
}
inline int prepareTable(HybridVoice& v,const OscParams& o,double base) noexcept{
    int un=std::clamp(o.unison,1,maxUnison);
    bool pitch=v.cacheBase!=base||v.cacheDetune!=o.detune||v.cacheUnison!=un;
    if(pitch){
      for(int u=0;u<un;++u){
        double spread=un==1?0.:2.*u/(un-1)-1.;
        v.cachedDelta[u]=std::clamp(base*std::exp2(spread*o.detune/1200.),0.,.45);
      }
      v.cacheBase=base;v.cacheDetune=o.detune;v.cacheUnison=un;
    }
    if(pitch||v.cacheMorph!=o.morph||v.cacheSource!=o.tableSource||v.cacheTable!=o.userTable){
      for(int u=0;u<un;++u)v.cachedRead[u]=planTable(o,v.cachedDelta[u]);
      v.cacheMorph=o.morph;v.cacheSource=o.tableSource;v.cacheTable=o.userTable;
    }
    if(pitch||v.cacheRatio!=o.ratio){
      for(int u=0;u<un;++u)v.cachedModDelta[u]=std::min(.45,v.cachedDelta[u]*double(o.ratio));
      v.cacheRatio=o.ratio;
    }
    return un;
}
inline float renderGroupedTable(HybridVoice& v,const OscParams& o,double base,double noteDelta) noexcept {
    int un=prepareTable(v,o,base);
    v.syncPhase+=noteDelta;if(v.syncPhase>=1.)v.syncPhase-=1.;
    float sum=0;
    for(int u=0;u<un;++u)sum+=v.cachedRead[u].read(v.phase[u]);
    advancePhases(v.phase.data(),v.cachedDelta.data(),un);
    advancePhases(v.mod.data(),v.cachedModDelta.data(),un);
    return sum*o.level/float(un);
}
// Process a tile at a time, lane-major. Keep the per-sample summation order.
inline void renderTableTile(HybridVoice& v,const OscParams& o,double noteDelta,float* out,int count)noexcept{
    std::fill_n(out,count,0.f);
    if(o.level<=0)return;
    const double base=std::clamp(noteDelta*std::exp2(double(o.tune)/12.),0.,.45);
    const int un=prepareTable(v,o,base);
    for(int u=0;u<un;++u){
        auto read=v.cachedRead[u];double phase=v.phase[u],mod=v.mod[u];
        const double d=v.cachedDelta[u],m=v.cachedModDelta[u];
        if(o.experimentalInterpolation&&read.a){
            // Gather source samples in phase order; SIMD the two interpolations and sum.
            // Fixed 32-sample scratch, no allocation; preserve unison accumulation order.
            for(int start=0;start<count;start+=32){
                int n=std::min(32,count-start);
                std::array<float,32> a{},aa{},b{},bb{},t{};
                for(int i=0;i<n;++i){
                    double p=phase*tableSize;int idx=std::min(int(p),tableSize-1);t[i]=float(p-idx);
                    a[i]=read.a[idx];aa[i]=read.a[idx+1];b[i]=read.b[idx];bb[i]=read.b[idx+1];
                    phase+=d;if(phase>=1.)phase-=1.;
                    mod+=m;if(mod>=1.)mod-=1.;
                }
                interpolateAdd(out+start,a.data(),aa.data(),b.data(),bb.data(),t.data(),read.morph,n,o.experimentalInterpolation);
            }
        }else for(int i=0;i<count;++i){
            out[i]+=read.read(phase);
            phase+=d;if(phase>=1.)phase-=1.;
            mod+=m;if(mod>=1.)mod-=1.;
        }
        v.phase[u]=phase;v.mod[u]=mod;
    }
    for(int i=0;i<count;++i){
        v.syncPhase+=noteDelta;if(v.syncPhase>=1.)v.syncPhase-=1.;
        out[i]=out[i]*o.level/float(un);
    }
}
template<class Analog>
inline float renderHybrid(HybridVoice& v,const OscParams& o,const SampleData* sample,double noteDelta,double sr,Analog analog,double bpm=120.) noexcept {
    if(o.level<=0)return 0;
    int un=std::clamp(o.unison,1,maxUnison);
    const double base=std::clamp(noteDelta*std::exp2(double(o.tune)/12.),0.,.45);
    if(canGroupTable(o))
        return renderGroupedTable(v,o,base,noteDelta);
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
        float m=(o.mode==OscMode::fm||o.mode==OscMode::am||o.ring)?float(std::sin(2*hpi*v.mod[u])):0.f,x=0;
        switch(o.mode){
            case OscMode::wavetable:x=readTable(o,v.phase[u],dt);break;
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
