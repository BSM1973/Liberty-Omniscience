#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <vector>
#include "HybridOscillator.h"
#include "CosmosFilter.h"
#include "CosmosLFO.h"
#include "CosmosModEnvelope.h"
#include "CosmosMatrix.h"
#include "CosmosNexus.h"
#include "CosmosFX.h"

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
    float filterKey=0,filterVelocity=0,filterDepth=0,filterAttack=.02f,filterDecay=.3f,filterSustain=.5f,filterRelease=.4f;
    Layer(){osc[0].level=1;}
};
struct MorphParams {bool enabled=false;float amount=1;int curve=0;std::array<bool,5> family{true,true,true,true,true};bool protectTune=true,protectGain=true;};
struct Patch { std::array<FxRack,fxRacks> fx{};std::array<Layer,layers> layer{}; std::array<LfoParams,lfoCount> lfo{}; std::array<ModEnvParams,modEnvCount> modEnv{};std::array<MatrixRoute,matrixCount> matrix{};std::array<float,8> macros{};NexusParams nexus{};MorphParams morph{}; float master=.7f; double bpm=120.,ppq=0;bool transportValid=false; bool experimentalGrouped=false; bool experimentalTiled=false; int experimentalInterpolation=0; bool experimentalGrainWindow=false; bool experimentalGrainPrepared=false; bool experimentalAnalog=false; bool experimentalEnv=false;bool experimentalMatrixFast=true;bool experimentalNexusShared=true; };
// Interpolate continuous layer parameters without mutating the stored patch or switching engines.
inline void applyMorph(Patch& p,float x,float y)noexcept{
 const auto m=p.morph;if(!m.enabled||m.amount<=0)return;
 auto w=nexusWeights(x,y,false);float total=0;for(auto& v:w){if(m.curve==1)v*=v;else if(m.curve==2)v=std::sqrt(v);total+=v;}for(auto& v:w)v/=std::max(total,1e-9f);
 const auto original=p.layer;
 auto blend=[&](auto getter,bool logarithmic=false){float sum=0;for(int j=0;j<4;++j)if(w[j]>0){float v=getter(original[j]);sum+=w[j]*(logarithmic?std::log(std::max(v,1e-5f)):v);}return logarithmic?std::exp(sum):sum;};
 std::array<float,128> targets{};
 for(int k=0;k<4;++k){int targetIndex=0;auto& l=p.layer[k];auto set=[&](float& v,auto getter,bool log=false){float target;if(p.experimentalNexusShared){if(k==0)targets[targetIndex]=blend(getter,log);target=targets[targetIndex++];}else target=blend(getter,log);v=std::lerp(v,target,m.amount);};
 if(m.family[0]){if(!m.protectGain)set(l.gain,[](const Layer& z){return z.gain;});if(!m.protectTune)set(l.tune,[](const Layer& z){return z.tune;});set(l.pan,[](const Layer& z){return z.pan;});}
 if(m.family[1])for(int o=0;o<oscillatorCount;++o){auto& a=l.osc[o];auto osc=[&](float& v,auto getter,bool log=false){set(v,[&](const Layer& z){return getter(z.osc[o]);},log);};if(!m.protectGain)osc(a.level,[](const OscParams& z){return z.level;});if(!m.protectTune)osc(a.tune,[](const OscParams& z){return z.tune;});osc(a.detune,[](const OscParams& z){return z.detune;});osc(a.drift,[](const OscParams& z){return z.drift;});osc(a.drive,[](const OscParams& z){return z.drive;});osc(a.morph,[](const OscParams& z){return z.morph;});osc(a.ratio,[](const OscParams& z){return z.ratio;},true);osc(a.depth,[](const OscParams& z){return z.depth;});}
 if(m.family[2])for(int o=0;o<oscillatorCount;++o){auto& a=l.osc[o].grain;auto grain=[&](float& v,auto getter,bool log=false){set(v,[&](const Layer& z){return getter(z.osc[o].grain);},log);};grain(a.size,[](const GrainParams& z){return z.size;},true);grain(a.density,[](const GrainParams& z){return z.density;},true);grain(a.position,[](const GrainParams& z){return z.position;});grain(a.spread,[](const GrainParams& z){return z.spread;});grain(a.stretch,[](const GrainParams& z){return z.stretch;},true);grain(a.blur,[](const GrainParams& z){return z.blur;});}
 if(m.family[3]){set(l.cutoff,[](const Layer& z){return z.cutoff;},true);set(l.filter1.resonance,[](const Layer& z){return z.filter1.resonance;});set(l.filter1.drive,[](const Layer& z){return z.filter1.drive;});set(l.filter1.morph,[](const Layer& z){return z.filter1.morph;});set(l.filter2.cutoff,[](const Layer& z){return z.filter2.cutoff;},true);set(l.filter2.resonance,[](const Layer& z){return z.filter2.resonance;});set(l.filter2.drive,[](const Layer& z){return z.filter2.drive;});set(l.filter2.morph,[](const Layer& z){return z.filter2.morph;});}
 if(m.family[4]){set(l.attack,[](const Layer& z){return z.attack;},true);set(l.decay,[](const Layer& z){return z.decay;},true);set(l.sustain,[](const Layer& z){return z.sustain;});set(l.release,[](const Layer& z){return z.release;},true);set(l.filterAttack,[](const Layer& z){return z.filterAttack;},true);set(l.filterDecay,[](const Layer& z){return z.filterDecay;},true);set(l.filterSustain,[](const Layer& z){return z.filterSustain;});set(l.filterRelease,[](const Layer& z){return z.filterRelease;},true);}
 }
}
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
    EnvPlan(const Layer& p,double sr):EnvPlan(p.attack,p.decay,p.sustain,p.release,sr){}
    EnvPlan(float at,float de,float su,float re,double sr){
      auto samples=[sr](float t){return float(std::max(1.,sr*double(t)));};
      attack=1/samples(at);decay=(1-su)/samples(de);sustain=su;releaseSamples=samples(re);
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
    std::array<Env,layers> env{},filterEnv{};
    std::array<LfoState,lfoCount> lfo{};std::array<ModEnvState,modEnvCount> modEnv{};
    std::array<float,matrixCount> routeSmooth{},routeView{};std::array<bool,matrixCount> routeInit{};std::array<float,lfoCount> lfoFeedback{},lfoPhaseView{};
    std::array<std::array<HybridVoice,oscillatorCount>,layers> hybrid{};
    void reset() noexcept {active=held=false;note=channel=0;velocity=0;age=0;phase={};low={};env={};filterEnv={};lfo={};modEnv={};routeSmooth={};routeView={};routeInit={};lfoFeedback={};lfoPhaseView={};hybrid={};for(auto& pair:filters)for(auto& f:pair)f.reset();}
    void stop() noexcept {held=false;for(auto& e:env)e.stop();for(auto& e:filterEnv)e.stop();for(auto& e:modEnv)e.stop();}
};
class Engine {
    double sr=48000;
    std::uint64_t counter=0;
    std::array<double,lfoCount> freeCycles{};
    // Allocate the 40 FX states once with the engine, not on the audio thread or engine stack.
    std::vector<FxRackState> fxState = std::vector<FxRackState>(fxRacks);std::array<FxAlignment,layers> fxLayerAlign;
    NexusMotion nexusMotion;std::array<float,128> nexusCC{};
    // Allocate the fixed voice pool once, outside the audio callback.
    std::vector<Voice> v = std::vector<Voice>(voices);
    std::array<bool,16> pedal{};
    std::array<float,16> bend{},channelPressure{};
    using MidiTable=std::array<std::array<float,128>,16>;
    std::vector<MidiTable> midiTables=std::vector<MidiTable>(2);
public:
    void prepare(double rate) noexcept {sr=std::max(8000.,rate);(void)waveBank();(void)hdBank();(void)factoryGrains();(void)hannLut();(void)sineLut();for(auto& rack:fxState)for(auto& slot:rack.slots)slot.prepare(sr);for(auto& a:fxLayerAlign)a.prepare(fxSlots*4096);reset();}
    void reset() noexcept {for(auto& voice:v)voice.reset();pedal={};bend={};midiTables[0]={};channelPressure={};midiTables[1]={};counter=0;freeCycles={};nexusMotion={};nexusCC.fill(-1);for(auto& rack:fxState)for(auto& slot:rack.slots)slot.reset();for(auto& a:fxLayerAlign)a.reset();}
    void noteOn(int ch,int note,float velocity) noexcept {
      if(ch<0||ch>=16)return;
      if(velocity<=0){noteOff(ch,note);return;}
      Voice* w=nullptr;
      for(auto& a:v)if(!a.active){w=&a;break;}
      if(!w)w=&*std::min_element(v.begin(),v.end(),[](const Voice& a,const Voice& b){if(a.held!=b.held)return !a.held;return a.age<b.age;});
      if(note>=0&&note<128)midiTables[1][ch][note]=0;w->reset(); w->active=w->held=true;w->note=note;w->channel=ch;w->velocity=std::clamp(velocity,0.f,1.f);w->age=++counter;
      for(auto& e:w->env)e.start();for(auto& e:w->filterEnv)e.start();for(auto& e:w->modEnv)e.start();
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
    void controller(int ch,int number,int value)noexcept{if(ch>=0&&ch<16&&number>=0&&number<128){midiTables[0][ch][number]=std::clamp(value,0,127)/127.f;nexusCC[number]=midiTables[0][ch][number];}}
    void pressure(int ch,int value)noexcept{if(ch>=0&&ch<16)channelPressure[ch]=std::clamp(value,0,127)/127.f;}
    void polyAftertouch(int ch,int note,int value)noexcept{if(ch>=0&&ch<16&&note>=0&&note<128)midiTables[1][ch][note]=std::clamp(value,0,127)/127.f;}
    void pitch(int ch,int value) noexcept {if(ch>=0&&ch<16)bend[ch]=2.f*float(value-8192)/8192.f;}
    void allOff(int ch,bool immediate) noexcept {for(auto& a:v)if(a.active&&a.channel==ch){if(immediate)a.reset();else a.stop();}}
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
    std::array<float,matrixCount> matrixValues()const noexcept{for(const auto& a:v)if(a.active)return a.routeView;return {};}
    std::array<float,lfoCount> lfoPhases()const noexcept{for(const auto& a:v)if(a.active)return a.lfoPhaseView;std::array<float,lfoCount> result;result.fill(-1);return result;}
    static int latency(const Patch& p)noexcept{int n=0;for(int k=0;k<layers;++k)n=std::max(n,rackLatency(p.fx[k]));return n+rackLatency(p.fx[4]);}
    size_t fxAllocatedBytes()const noexcept{size_t bytes=sizeof(CreativeFFT::Plan);for(const auto& rack:fxState)for(const auto& slot:rack.slots)bytes+=slot.allocatedBytes();for(const auto& a:fxLayerAlign)for(const auto& v:a.ring)bytes+=v.capacity()*sizeof(float);return bytes;}
    const std::array<float,24>& fxSpectrum(int rack,int slot)const noexcept{return fxState[rack].slots[slot].creative.spectral.meter;}
    float fxPhase(int rack,int slot)const noexcept{return fxState[rack].slots[slot].space.phase;}
    float fxReduction(int rack,int slot)const noexcept{return fxState[rack].slots[slot].reduction;}
    bool voiceActive(int index)const noexcept{return index>=0&&index<voices&&v[size_t(index)].active;}
    std::array<float,2> nexusPosition()const noexcept{return {nexusMotion.x,nexusMotion.y};}
    std::array<float,4> nexusGains(bool power)const noexcept{return nexusMotion.weights(power);}
    void renderSynth(float* left,float* right,int n,const Patch& patch,float* const* buses=nullptr) noexcept {if(n<=0)return;if(patch.morph.enabled&&patch.morph.amount>0){auto transformed=patch;auto target=nexusTarget(patch.nexus,patch.macros,nexusCC);auto motion=nexusMotion;float step=patch.nexus.smooth<=0?1.f:float(1-std::exp(-1000./(sr*patch.nexus.smooth)));motion.tick(target[0],target[1],patch.nexus.enabled?1.f:0.f,step);applyMorph(transformed,motion.x,motion.y);renderRange(left,right,n,transformed,0,voices,buses);}else renderRange(left,right,n,patch,0,voices,buses);auto target=nexusTarget(patch.nexus,patch.macros,nexusCC);float step=patch.nexus.smooth<=0?1.f:float(1-std::exp(-1000./(sr*patch.nexus.smooth)));for(int j=0;j<n;++j)nexusMotion.tick(target[0],target[1],patch.nexus.enabled?1.f:0.f,step);for(int i=0;i<lfoCount;++i)if(patch.lfo[i].mode!=0){freeCycles[i]+=n*lfoHz(patch.lfo[i],patch.bpm)/sr;if(freeCycles[i]>=4294967296.)freeCycles[i]-=4294967296.;}}
    void render(float* left,float* right,int n,const Patch& patch)noexcept{
      if(n<=0)return;bool layerFX=false;for(int k=0;k<layers;++k)layerFX|=fxState[k].active(patch.fx[k]);
      if(!layerFX){for(auto& a:fxLayerAlign)a.reset();if(!fxState[4].active(patch.fx[4])){renderSynth(left,right,n,patch);return;}}
      std::array<int,layers> rackDelays{};int layerLatency=0;for(int k=0;k<layers;++k){rackDelays[k]=rackLatency(patch.fx[k]);layerLatency=std::max(layerLatency,rackDelays[k]);}
      constexpr int capacity=1024;std::array<std::array<float,capacity>,8> buffer;
      std::array<float,capacity> mixL,mixR;std::array<float*,8> ptr;for(int j=0;j<8;++j)ptr[j]=buffer[j].data();
      for(int offset=0;offset<n;offset+=capacity){int count=std::min(capacity,n-offset);std::fill_n(mixL.data(),count,0.f);std::fill_n(mixR.data(),count,0.f);auto p=patch;if(p.transportValid)p.ppq+=offset*p.bpm/(60*sr);
        if(layerFX){for(auto& x:buffer)std::fill_n(x.data(),count,0.f);renderSynth(mixL.data(),mixR.data(),count,p,ptr.data());for(int i=0;i<count;++i){float l=0,r=0;for(int k=0;k<layers;++k){float a=buffer[k*2][i],b=buffer[k*2+1][i];fxState[k].tick(a,b,p.fx[k],sr,p.macros);fxLayerAlign[k].tick(a,b,layerLatency-rackDelays[k]);l+=a;r+=b;}mixL[i]=l;mixR[i]=r;}}
        else renderSynth(mixL.data(),mixR.data(),count,p);
        for(int i=0;i<count;++i){fxState[4].tick(mixL[i],mixR[i],p.fx[4],sr,p.macros);left[offset+i]+=mixL[i];right[offset+i]+=mixR[i];}
      }
    }
    // Disjoint ranges may be processed concurrently with immutable patch and MIDI state.
    // Caller must join all tasks before modifying MIDI state or reading telemetry.
    void renderRange(float* left,float* right,int n,const Patch& patch,int first,int last,float* const* buses=nullptr) noexcept {
      if(n<=0)return;
      bool any=false;for(int i=std::max(0,first);i<std::min(voices,last);++i)any|=v[size_t(i)].active;
      if(!any)return;
      std::array<float,layers> alpha{},gl{},gr{};
      std::array<EnvPlan,layers> envelopes{},filterEnvelopes{};
      std::array<bool,layers> modulated{},lfoCutoff{},lfoOsc{};std::array<int,lfoCount> activeLfo{};std::array<double,lfoCount> lfoStep{};int activeCount=0;
      std::array<int,matrixCount> routes{};std::array<bool,matrixSourceCount> needed{};int routeCount=0;std::array<float,matrixCount> smoothStep{};std::array<int,matrixCount> layerFirst{},layerEnd{};std::array<bool,layers> individualOsc{};
      for(int i=0;i<matrixCount;++i){const auto& m=patch.matrix[i];if(!validRoute(m))continue;routes[routeCount++]=i;layerFirst[i]=m.layer?m.layer-1:0;layerEnd[i]=m.layer?m.layer:layers;needed[m.source]=true;if(m.destination>=13)needed[m.destination-12]=true;if(m.smoothing>0&&m.source>=23&&m.source<=154)smoothStep[i]=float(1-std::exp(-1./(sr*m.smoothing*.001)));for(int k=0;k<layers;++k)if(m.layer==0||m.layer==k+1){if(m.destination<=2||(m.destination>=11&&m.destination<=12))lfoCutoff[k]=true;if(m.destination>=5&&m.destination<=10){lfoOsc[k]=true;if(m.oscillator>0)individualOsc[k]=true;}}}
      for(int i=0;i<lfoCount;++i){const auto& l=patch.lfo[i];if((l.destination<=0||l.destination>8||l.depth==0)&&!needed[i+1])continue;activeLfo[activeCount++]=i;lfoStep[i]=lfoHz(l,patch.bpm)/sr;if(l.depth!=0&&(l.destination==1||l.destination==2))for(int k=0;k<layers;++k)if(l.layer==0||l.layer==k+1)lfoCutoff[k]=true;if(l.depth!=0&&l.destination>=5)for(int k=0;k<layers;++k)if(l.layer==0||l.layer==k+1)lfoOsc[k]=true;}
      std::array<ModEnvPlan,modEnvCount> envPlans{};std::array<int,modEnvCount> activeEnv{};int envCount=0;
      for(int i=0;i<modEnvCount;++i){const auto& m=patch.modEnv[i];if((m.destination<=0||m.destination>8||m.depth==0)&&!needed[9+i])continue;auto raw=m;raw.depth=1;envPlans[i]=ModEnvPlan(raw,sr,patch.bpm);activeEnv[envCount++]=i;for(int k=0;k<layers;++k)if(m.layer==0||m.layer==k+1){if(m.depth!=0&&m.destination>0&&m.destination<=2)lfoCutoff[k]=true;if(m.depth!=0&&m.destination>=5)lfoOsc[k]=true;}}
      std::array<std::array<FilterPlan,2>,layers> filters{};
      auto analog=patch.experimentalAnalog?fastOscillator:oscillator;
      std::array<std::array<OscParams,oscillatorCount>,layers> prepared{};
      std::array<std::array<const SampleData*,oscillatorCount>,layers> sources{};
      for(int k=0;k<layers;++k){
        const auto& p=patch.layer[k];envelopes[k]=EnvPlan(p,sr);filterEnvelopes[k]=EnvPlan(p.filterAttack,p.filterDecay,p.filterSustain,p.filterRelease,sr);modulated[k]=p.filterKey!=0||p.filterVelocity!=0||p.filterDepth!=0;auto f=p.filter1;f.cutoff=p.cutoff;filters[k][0]=FilterPlan(f,sr);filters[k][1]=FilterPlan(p.filter2,sr);
        for(int o=0;o<oscillatorCount;++o){
          prepared[k][o]=p.osc[o];auto& x=prepared[k][o];x.experimentalGrouped=patch.experimentalGrouped;x.experimentalInterpolation=patch.experimentalInterpolation;x.grain.experimentalWindow=patch.experimentalGrainWindow;x.grain.experimentalPrepared=patch.experimentalGrainPrepared;
          if(o==0)x.wave=p.wave;
          sources[k][o]=x.mode==OscMode::granular&&x.grain.source>0&&x.grain.source<=4?&factoryGrains()[size_t(x.grain.source-1)]:p.sample;
        }
        alpha[k]=float(1-std::exp(-2*pi*std::clamp(double(p.cutoff),20.,sr*.45)/sr));
        double angle=(std::clamp(p.pan,-1.f,1.f)+1)*pi*.25;
        float g=p.enabled?p.gain:0.f;gl[k]=g*float(std::cos(angle));gr[k]=g*float(std::sin(angle));
      }
      const auto nxTarget=nexusTarget(patch.nexus,patch.macros,nexusCC);const float nxStep=patch.nexus.smooth<=0?1.f:float(1-std::exp(-1000./(sr*patch.nexus.smooth)));const bool nxActive=patch.nexus.enabled||nexusMotion.mix>1e-7f||needed[163]||needed[164];
      // Shared values have no voice-dependent state. Keep the original path for A/B and oversized host blocks.
      constexpr int sharedCapacity=1024;const bool shared=patch.experimentalNexusShared&&n<=sharedCapacity;
      std::array<std::array<float,4>,sharedCapacity> sharedGains;std::array<std::array<float,2>,sharedCapacity> sharedXY;
      std::array<std::array<float,sharedCapacity>,lfoCount> sharedLfo;
      if(shared&&nxActive){auto motion=nexusMotion;for(int i=0;i<n;++i){motion.tick(nxTarget[0],nxTarget[1],patch.nexus.enabled?1.f:0.f,nxStep);sharedXY[i]={motion.x,motion.y};sharedGains[i]=motion.mix>0?motion.weights(patch.nexus.power):std::array<float,4>{1,1,1,1};}}
      if(shared)for(int j=0;j<activeCount;++j){int idx=activeLfo[j];const auto& source=patch.lfo[idx];if(source.mode==0)continue;auto raw=source;raw.depth=1;raw.polarity=0;const double base=source.mode==2&&patch.transportValid&&source.division>0?patch.ppq*lfoHz(source,patch.bpm)*60/std::clamp(patch.bpm,20.,400.):freeCycles[idx];for(int i=0;i<n;++i)sharedLfo[idx][i]=lfoAt(raw,base+i*lfoStep[idx]);}
      for(int voiceIndex=std::max(0,first);voiceIndex<std::min(voices,last);++voiceIndex){
        auto& a=v[size_t(voiceIndex)];if(!a.active)continue;
        auto nx=nexusMotion;
        std::array<float,matrixCount> staticValues;std::array<bool,matrixCount> staticRoute{};if(shared)for(int j=0;j<routeCount;++j){int ri=routes[j];const auto& m=patch.matrix[ri];if(m.source<21||m.source>162||smoothStep[ri]>0)continue;float raw=0;if(m.source==21)raw=a.velocity;else if(m.source==22)raw=std::clamp(a.note/127.f,0.f,1.f);else if(m.source==23)raw=midiTables[0][a.channel][1];else if(m.source==24)raw=channelPressure[a.channel];else if(m.source==25)raw=midiTables[1][a.channel][std::clamp(a.note,0,127)];else if(m.source==26)raw=std::clamp(.5f+bend[a.channel]*.25f,0.f,1.f);else if(m.source<=154)raw=midiTables[0][a.channel][m.source-27];else raw=patch.macros[m.source-155];staticRoute[ri]=true;staticValues[ri]=matrixValue(raw,m);a.routeInit[ri]=false;}
        a.lfoPhaseView.fill(-1);for(int ri=0;ri<matrixCount;++ri)if(!validRoute(patch.matrix[ri])){a.routeView[ri]=0;a.routeInit[ri]=false;}
        std::array<double,layers> dt{};auto voiceFilters=filters;auto voiceAlpha=alpha;std::array<float,layers> lastRatio{},lastRatioB{},lastResA{},lastResB{};lastResA.fill(-1);lastResB.fill(-1);
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
              if(stage==Env::release||stage==Env::off||lfoOsc[k])continue;
              for(int o=0;o<oscillatorCount;++o)if(canGroupTable(prepared[k][o])&&prepared[k][o].level>0){
                tiled[k][o]=true;renderTableTile(a.hybrid[k][o],prepared[k][o],dt[k],tile[k][o].data(),count);
              }
            }
          }
          for(int j=0;j<count;++j){
          int i=offset+j;
          float l=0,r=0;bool alive=false;std::array<float,4> nxGain{1,1,1,1};if(nxActive){if(shared){nx.x=sharedXY[i][0];nx.y=sharedXY[i][1];nxGain=sharedGains[i];}else{nx.tick(nxTarget[0],nxTarget[1],patch.nexus.enabled?1.f:0.f,nxStep);if(nx.mix>0)nxGain=nx.weights(patch.nexus.power);}}
          std::array<std::array<float,matrixDestCount>,layers> modulation{};std::array<float,21> rawValues{};std::array<std::array<std::array<float,6>,oscillatorCount>,layers> oscMod{};std::array<float,lfoCount> nextFeedback{};
          for(int jLfo=0;jLfo<activeCount;++jLfo){int idx=activeLfo[jLfo];const auto& source=patch.lfo[idx];auto raw=source;raw.depth=1;raw.polarity=0;float value;if(source.mode==0){if(i==n-1)a.lfoPhaseView[idx]=float(a.lfo[idx].phase);value=a.lfo[idx].tick(raw,lfoStep[idx]);}else{if(shared&&i!=n-1)value=sharedLfo[idx][i];else{double cycles=freeCycles[idx]+i*lfoStep[idx];if(source.mode==2&&patch.transportValid&&source.division>0)cycles=patch.ppq*lfoHz(source,patch.bpm)*60/std::clamp(patch.bpm,20.,400.)+i*lfoStep[idx];if(i==n-1)a.lfoPhaseView[idx]=float(cycles-std::floor(cycles));value=shared?sharedLfo[idx][i]:lfoAt(raw,cycles);}}value=std::clamp(value*std::clamp(1.f+a.lfoFeedback[idx],0.f,2.f),-1.f,1.f);rawValues[idx+1]=.5f+.5f*value;value=(source.polarity?rawValues[idx+1]:value)*source.depth;if(source.destination>0&&source.destination<=8)for(int k=0;k<layers;++k)if(source.layer==0||source.layer==k+1)modulation[k][source.destination-1]+=value;}
          for(int jEnv=0;jEnv<envCount;++jEnv){int idx=activeEnv[jEnv];const auto& source=patch.modEnv[idx];float value=a.modEnv[idx].tick(envPlans[idx],a.velocity);rawValues[9+idx]=value;if(source.destination>0&&source.destination<=8)for(int k=0;k<layers;++k)if(source.layer==0||source.layer==k+1)modulation[k][source.destination-1]+=value*source.depth;}
          for(int route=0;route<routeCount;++route){int ri=routes[route];const auto& m=patch.matrix[ri];float value;if(staticRoute[ri])value=staticValues[ri];else{float raw=0;
            if(m.source<=20)raw=rawValues[m.source];else if(m.source==21)raw=a.velocity;else if(m.source==22)raw=std::clamp(a.note/127.f,0.f,1.f);else if(m.source==23)raw=midiTables[0][a.channel][1];else if(m.source==24)raw=channelPressure[a.channel];else if(m.source==25)raw=midiTables[1][a.channel][std::clamp(a.note,0,127)];else if(m.source==26)raw=std::clamp(.5f+bend[a.channel]*.25f,0.f,1.f);else if(m.source<=154)raw=midiTables[0][a.channel][m.source-27];else if(m.source<=162)raw=patch.macros[m.source-155];else raw=m.source==163?nx.x:nx.y;
            if(smoothStep[ri]>0){if(!a.routeInit[ri]){a.routeSmooth[ri]=raw;a.routeInit[ri]=true;}else a.routeSmooth[ri]+=smoothStep[ri]*(raw-a.routeSmooth[ri]);raw=a.routeSmooth[ri];}else a.routeInit[ri]=false;
            value=matrixValue(raw,m);}a.routeView[ri]=value;if(m.destination>=13){nextFeedback[m.destination-13]+=value;continue;}int firstLayer=patch.experimentalMatrixFast?layerFirst[ri]:0,endLayer=patch.experimentalMatrixFast?layerEnd[ri]:layers;for(int k=firstLayer;k<endLayer;++k)if(patch.experimentalMatrixFast||m.layer==0||m.layer==k+1){if(m.oscillator>0&&m.destination>=5&&m.destination<=10)oscMod[k][m.oscillator-1][m.destination-5]+=value;else modulation[k][m.destination-1]+=value;}
          }
          a.lfoFeedback=nextFeedback;
          for(int k=0;k<layers;++k){
            const auto& p=patch.layer[k];auto& e=a.env[k];
            float fe=a.filterEnv[k].tickPrepared(filterEnvelopes[k]);
            if((modulated[k]||lfoCutoff[k])&&(i%16==0)){float oct=p.filterKey*(a.note-60)/12.f+p.filterVelocity*(a.velocity-.5f)*4.f+p.filterDepth*fe;auto fa=p.filter1,fb=p.filter2;float ratio=std::exp2(std::clamp(oct+2*modulation[k][0],-10.f,10.f)),ratioB=std::exp2(std::clamp(oct+2*modulation[k][1],-10.f,10.f));fa.resonance=std::clamp(fa.resonance+modulation[k][10],0.f,1.f);fb.resonance=std::clamp(fb.resonance+modulation[k][11],0.f,1.f);if(ratio!=lastRatio[k]||fa.resonance!=lastResA[k]){lastRatio[k]=ratio;lastResA[k]=fa.resonance;fa.cutoff=p.cutoff*ratio;voiceFilters[k][0]=FilterPlan(fa,sr);voiceAlpha[k]=voiceFilters[k][0].legacyAlpha;}if(ratioB!=lastRatioB[k]||fb.resonance!=lastResB[k]){lastRatioB[k]=ratioB;lastResB[k]=fb.resonance;fb.cutoff*=ratioB;voiceFilters[k][1]=FilterPlan(fb,sr);}}
            float env=patch.experimentalEnv?e.tickPrepared(envelopes[k]):e.tick(p,sr);alive|=e.stage!=Env::off;
            if(e.stage==Env::off)continue;
            float x=0;
            double pitchDt=dt[k];if(modulation[k][4]!=0)pitchDt=std::clamp(pitchDt*std::exp2(std::clamp(double(modulation[k][4]),-4.,4.)),0.,.45);
            auto pitchFor=[&](int o){float v=oscMod[k][o][0];return v==0?pitchDt:std::clamp(dt[k]*std::exp2(std::clamp(double(modulation[k][4]+v),-4.,4.)),0.,.45);};
            auto renderOsc=[&](int o){const auto& base=prepared[k][o];if(tiled[k][o])return tile[k][o][j];
              auto mod=[&](int d){return modulation[k][d]+oscMod[k][o][d-4];};
              double localPitch=pitchFor(o);
              if(((base.mode==OscMode::fm||base.mode==OscMode::am)&&(mod(8)!=0||mod(9)!=0))||(base.mode==OscMode::wavetable&&mod(5)!=0)||(base.mode==OscMode::granular&&(mod(6)!=0||mod(7)!=0))){
                auto m=base;if(m.mode==OscMode::fm||m.mode==OscMode::am){m.depth=std::clamp(m.depth+mod(8),0.f,1.f);m.ratio=std::clamp(m.ratio*std::exp2(std::clamp(mod(9),-3.f,3.f)),.125f,16.f);}else if(m.mode==OscMode::wavetable)m.morph=std::clamp(m.morph+mod(5)*.5f,0.f,1.f);
                else {m.grain.position=std::clamp(m.grain.position+mod(6)*.5f,0.f,1.f);m.grain.density=std::clamp(m.grain.density*std::exp2(std::clamp(mod(7),-3.f,3.f)),1.f,80.f);}
                return renderHybrid(a.hybrid[k][o],m,sources[k][o],localPitch,sr,analog,patch.bpm);
              }
              return renderHybrid(a.hybrid[k][o],base,sources[k][o],localPitch,sr,analog,patch.bpm);
            };
            if(legacy(p.osc[0])){
              double localPitch=pitchFor(0);x=analog(p.wave,a.phase[k],localPitch);
              a.phase[k]+=localPitch;if(a.phase[k]>=1)a.phase[k]-=1;
              a.hybrid[k][0].phase[0]=a.phase[k];
            }else{
              x=renderOsc(0);
              a.phase[k]=a.hybrid[k][0].phase[0];
            }
            for(int o=1;o<oscillatorCount;++o)if(p.osc[o].level>0)
              x+=renderOsc(o);
            a.low[k]+=voiceAlpha[k]*(x-a.low[k]);
            float filtered=p.filter1.model==0?a.low[k]:a.filters[k][0].tick(x,voiceFilters[k][0]);
            if(p.filter2.model!=1){float second=a.filters[k][1].tick(p.filterParallel?x:filtered,voiceFilters[k][1]);filtered=p.filterParallel?std::lerp(filtered,second,p.filterMix):second;}
            float s=filtered*p.filterTrim*env;if(nxActive)s*=nxGain[k];if(modulation[k][2]!=0)s*=std::clamp(1.f+modulation[k][2],0.f,2.f);if(modulation[k][3]!=0){double angle=(std::clamp(p.pan+modulation[k][3],-1.f,1.f)+1)*pi*.25;float gain=p.enabled?p.gain:0.f;float sl=s*gain*float(std::cos(angle)),srOut=s*gain*float(std::sin(angle));if(buses){float g=a.velocity*.12f*patch.master;buses[2*k][i]+=sl*g;buses[2*k+1][i]+=srOut*g;}else{l+=sl;r+=srOut;}}else{float sl=s*gl[k],srOut=s*gr[k];if(buses){float g=a.velocity*.12f*patch.master;buses[2*k][i]+=sl*g;buses[2*k+1][i]+=srOut*g;}else{l+=sl;r+=srOut;}}
          }
          if(!alive){a.active=false;break;}
          const float g=a.velocity*.12f*patch.master;left[i]+=l*g;right[i]+=r*g;
          }
        }
      }
    }
};
}
