#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <vector>
#include "HybridOscillator.h"
#include "CosmosFilter.h"

namespace cosmos {
constexpr int layers = 4, voices = 32;
constexpr double pi = std::numbers::pi_v<double>;
struct Layer {
    bool enabled = true;
    int wave = 0;
    float gain=.45f, pan=0, tune=0, cutoff=12000, attack=.025f, decay=.35f, sustain=.8f, release=.5f;
    std::array<OscParams,oscillatorCount> osc{};
    const SampleData* sample=nullptr;
    FilterParams filter1{},filter2{1,8000,0,0};
    bool filterParallel=false;float filterMix=.5f,filterTrim=1.f;
    Layer(){osc[0].level=1;}
};
struct Patch { std::array<Layer,layers> layer{}; float master=.7f; double bpm=120.; bool experimentalGrouped=false; bool experimentalTiled=false; int experimentalInterpolation=0; bool experimentalGrainWindow=false; bool experimentalGrainPrepared=false; bool experimentalAnalog=false; bool experimentalEnv=false; };
inline float blep(double p, double dt) noexcept {
    if(dt<=0) return 0;
    if(p<dt){ p/=dt; return float(p+p-p*p-1); }
    if(p>1-dt){ p=(p-1)/dt; return float(p*p+p+p+1); }
    return 0;
}
inline float oscillator(int wave, double p, double dt) noexcept {
    switch(wave){
      case 1: return float(2*p-1)-blep(p,dt);
      case 2: return (p<.5?1.f:-1.f)+blep(p,dt)-blep(std::fmod(p+.5,1.),dt);
      case 3: return float(1-4*std::abs(p-.5));
      default: return float(std::sin(2*pi*p));
    }
}
inline const std::array<float,16385>& sineLut(){
    static const auto a=[]{
      std::array<float,16385> v{};
      for(int i=0;i<=16384;++i)v[i]=float(std::sin(2*pi*double(i)/16384.));
      return v;
    }();return a;
}
inline float fastOscillator(int wave,double p,double dt) noexcept{
    if(wave==2){double q=p+.5;if(q>=1.)q-=1.;return (p<.5?1.f:-1.f)+blep(p,dt)-blep(q,dt);}
    if(wave!=0)return oscillator(wave,p,dt);
    double q=p*16384.;int i=std::clamp(int(q),0,16383);const auto& a=sineLut();
    return std::lerp(a[i],a[i+1],float(q-i));
}
struct EnvPlan {
    float attack=0,decay=0,sustain=0,releaseSamples=1;
    EnvPlan()=default;
    EnvPlan(const Layer& p,double sr){
      auto samples=[sr](float t){return float(std::max(1.,sr*double(t)));};
      attack=1/samples(p.attack);decay=(1-p.sustain)/samples(p.decay);sustain=p.sustain;releaseSamples=samples(p.release);
    }
};
struct Env {
    enum Stage { attack, decay, sustain, release, off };
    Stage stage=off; float level=0, releaseStart=0;
    void start() noexcept { level=0; stage=attack; }
    void stop() noexcept { if(stage!=off && stage!=release){releaseStart=level;stage=release;} }
    float tickPrepared(const EnvPlan& p)noexcept{
      switch(stage){
        case attack: level+=p.attack;if(level>=1){level=1;stage=decay;}break;
        case decay: level-=p.decay;if(level<=p.sustain){level=p.sustain;stage=sustain;}break;
        case sustain: level=p.sustain;break;
        case release: level-=releaseStart/p.releaseSamples;if(level<=.000001f){level=0;stage=off;}break;
        case off:level=0;break;
      }
      return level;
    }
    float tick(const Layer& p,double sr) noexcept {
      auto samples=[sr](float t){return float(std::max(1.,sr*double(t)));};
      switch(stage){
        case attack: level+=1/samples(p.attack); if(level>=1){level=1;stage=decay;} break;
        case decay: level-= (1-p.sustain)/samples(p.decay); if(level<=p.sustain){level=p.sustain;stage=sustain;} break;
        case sustain: level=p.sustain; break;
        case release: level-=releaseStart/samples(p.release); if(level<=.000001f){level=0;stage=off;} break;
        case off: level=0; break;
      }
      return level;
    }
};
struct Voice {
    bool active=false, held=false;
    int note=0, channel=0; float velocity=0;
    std::uint64_t age=0;
    std::array<double,layers> phase{};
    std::array<float,layers> low{};
    std::array<std::array<FilterState,2>,layers> filters{};
    std::array<Env,layers> env{};
    std::array<std::array<HybridVoice,oscillatorCount>,layers> hybrid{};
    void stop() noexcept {held=false;for(auto& e:env)e.stop();}
};
class Engine {
    double sr=48000;
    std::uint64_t counter=0;
    // Allocate the fixed voice pool once, outside the audio callback.
    std::vector<Voice> v = std::vector<Voice>(voices);
    std::array<bool,16> pedal{};
    std::array<float,16> bend{};
public:
    void prepare(double rate) noexcept {sr=std::max(8000.,rate);(void)waveBank();(void)hdBank();(void)factoryGrains();(void)hannLut();(void)sineLut();reset();}
    void reset() noexcept {for(auto& voice:v)voice=Voice{};pedal={};bend={};counter=0;}
    void noteOn(int ch,int note,float velocity) noexcept {
      if(ch<0||ch>=16)return;
      if(velocity<=0){noteOff(ch,note);return;}
      Voice* w=nullptr;
      for(auto& a:v)if(!a.active){w=&a;break;}
      if(!w)w=&*std::min_element(v.begin(),v.end(),[](const Voice& a,const Voice& b){if(a.held!=b.held)return !a.held;return a.age<b.age;});
      *w=Voice{}; w->active=w->held=true;w->note=note;w->channel=ch;w->velocity=std::clamp(velocity,0.f,1.f);w->age=++counter;
      for(auto& e:w->env)e.start();
      for(int k=0;k<layers;++k)for(int o=0;o<oscillatorCount;++o)w->hybrid[k][o].seed(o,k,note,w->age);
    }
    void noteOff(int ch,int note) noexcept {
      if(ch<0||ch>=16)return;
      for(auto& a:v)if(a.active&&a.held&&a.channel==ch&&a.note==note){a.held=false;if(!pedal[ch])a.stop();}
    }
    void sustain(int ch,bool down) noexcept {
      if(ch<0||ch>=16)return;
      pedal[ch]=down;if(!down)for(auto& a:v)if(a.active&&a.channel==ch&&!a.held)a.stop();
    }
    void pitch(int ch,int value) noexcept {if(ch>=0&&ch<16)bend[ch]=2.f*float(value-8192)/8192.f;}
    void allOff(int ch,bool immediate) noexcept {for(auto& a:v)if(a.active&&a.channel==ch){if(immediate)a=Voice{};else a.stop();}}
    std::array<float,16> grainPositions(int layer,int osc,const SampleData* source)const noexcept{
      std::array<float,16> out;out.fill(-1.f);
      if(!source||source->mono.empty())return out;
      for(const auto& a:v)if(a.active){
        const auto& gs=a.hybrid[layer][osc].grain.grains;
        bool found=false;for(size_t i=0;i<gs.size();++i)if(gs[i].age<gs[i].length){out[i]=float(gs[i].pos/double(source->mono.size()));found=true;}
        if(found)break;
      }
      return out;
    }
    bool voiceActive(int index)const noexcept{return index>=0&&index<voices&&v[size_t(index)].active;}
    void render(float* left,float* right,int n,const Patch& patch) noexcept {renderRange(left,right,n,patch,0,voices);}
    // Disjoint ranges may be processed concurrently with immutable patch and MIDI state.
    // Caller must join all tasks before modifying MIDI state or reading telemetry.
    void renderRange(float* left,float* right,int n,const Patch& patch,int first,int last) noexcept {
      if(n<=0)return;
      bool any=false;for(int i=std::max(0,first);i<std::min(voices,last);++i)any|=v[size_t(i)].active;
      if(!any)return;
      std::array<float,layers> alpha{},gl{},gr{};
      std::array<EnvPlan,layers> envelopes{};
      std::array<std::array<FilterPlan,2>,layers> filters{};
      auto analog=patch.experimentalAnalog?fastOscillator:oscillator;
      std::array<std::array<OscParams,oscillatorCount>,layers> prepared{};
      std::array<std::array<const SampleData*,oscillatorCount>,layers> sources{};
      for(int k=0;k<layers;++k){
        const auto& p=patch.layer[k];envelopes[k]=EnvPlan(p,sr);auto f=p.filter1;f.cutoff=p.cutoff;filters[k][0]=FilterPlan(f,sr);filters[k][1]=FilterPlan(p.filter2,sr);
        for(int o=0;o<oscillatorCount;++o){
          prepared[k][o]=p.osc[o];auto& x=prepared[k][o];x.experimentalGrouped=patch.experimentalGrouped;x.experimentalInterpolation=patch.experimentalInterpolation;x.grain.experimentalWindow=patch.experimentalGrainWindow;x.grain.experimentalPrepared=patch.experimentalGrainPrepared;
          if(o==0)x.wave=p.wave;
          sources[k][o]=x.mode==OscMode::granular&&x.grain.source>0&&x.grain.source<=4?&factoryGrains()[size_t(x.grain.source-1)]:p.sample;
        }
        alpha[k]=float(1-std::exp(-2*pi*std::clamp(double(p.cutoff),20.,sr*.45)/sr));
        double angle=(std::clamp(p.pan,-1.f,1.f)+1)*pi*.25;
        float g=p.enabled?p.gain:0.f;gl[k]=g*float(std::cos(angle));gr[k]=g*float(std::sin(angle));
      }
      for(int voiceIndex=std::max(0,first);voiceIndex<std::min(voices,last);++voiceIndex){
        auto& a=v[size_t(voiceIndex)];if(!a.active)continue;
        std::array<double,layers> dt{};
        for(int k=0;k<layers;++k)dt[k]=std::clamp(440.*std::exp2((a.note-69+patch.layer[k].tune+bend[a.channel])/12.)/sr,0.,.45);
        constexpr int tileSize=32;
        std::array<std::array<std::array<float,tileSize>,oscillatorCount>,layers> tile{};
        for(int offset=0;offset<n&&a.active;offset+=tileSize){
          int count=std::min(tileSize,n-offset);
          std::array<std::array<bool,oscillatorCount>,layers> tiled{};
          if(patch.experimentalTiled&&patch.experimentalGrouped){
            for(int k=0;k<layers;++k){
              // Release can end mid-tile. Keep that path sample accurate.
              auto stage=a.env[k].stage;
              if(stage==Env::release||stage==Env::off)continue;
              for(int o=0;o<oscillatorCount;++o)if(canGroupTable(prepared[k][o])&&prepared[k][o].level>0){
                tiled[k][o]=true;renderTableTile(a.hybrid[k][o],prepared[k][o],dt[k],tile[k][o].data(),count);
              }
            }
          }
          for(int j=0;j<count;++j){
          int i=offset+j;
          float l=0,r=0;bool alive=false;
          for(int k=0;k<layers;++k){
            const auto& p=patch.layer[k];auto& e=a.env[k];
            float env=patch.experimentalEnv?e.tickPrepared(envelopes[k]):e.tick(p,sr);alive|=e.stage!=Env::off;
            if(e.stage==Env::off)continue;
            float x=0;
            if(legacy(p.osc[0])){
              x=analog(p.wave,a.phase[k],dt[k]);
              a.phase[k]+=dt[k];if(a.phase[k]>=1)a.phase[k]-=1;
              a.hybrid[k][0].phase[0]=a.phase[k];
            }else{
              x=tiled[k][0]?tile[k][0][j]:renderHybrid(a.hybrid[k][0],prepared[k][0],sources[k][0],dt[k],sr,analog,patch.bpm);
              a.phase[k]=a.hybrid[k][0].phase[0];
            }
            for(int o=1;o<oscillatorCount;++o)if(p.osc[o].level>0)
              x+=tiled[k][o]?tile[k][o][j]:renderHybrid(a.hybrid[k][o],prepared[k][o],sources[k][o],dt[k],sr,analog,patch.bpm);
            a.low[k]+=alpha[k]*(x-a.low[k]);
            float filtered=p.filter1.model==0?a.low[k]:a.filters[k][0].tick(x,filters[k][0]);
            if(p.filter2.model!=1){float second=a.filters[k][1].tick(p.filterParallel?x:filtered,filters[k][1]);filtered=p.filterParallel?std::lerp(filtered,second,p.filterMix):second;}
            float s=filtered*p.filterTrim*env;l+=s*gl[k];r+=s*gr[k];
          }
          if(!alive){a.active=false;break;}
          const float g=a.velocity*.12f*patch.master;left[i]+=l*g;right[i]+=r*g;
          }
        }
      }
    }
};
}
