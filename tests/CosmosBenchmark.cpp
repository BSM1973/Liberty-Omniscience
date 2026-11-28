#include "CosmosEngine.h"
#include "CosmosParallelBench.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
int modulationBenchmark(){
  constexpr int n=128;constexpr double sr=48000.;std::cout<<"scenario,voices,seconds,mean_budget_pct,p95,p99,max,misses,finite,peak\n";
  for(int mode=0;mode<4;++mode){int notes=mode==1?8:32,blocks=mode==3?22500:375;auto e=std::make_unique<cosmos::Engine>();e->prepare(sr);cosmos::Patch p;
    if(mode){for(int i=0;i<64;++i)p.matrix[i]={true,1+i%20,3+(i%2),i%5,0,.0125f};for(auto& ep:p.modEnv){ep.play=1;ep.free.enabled=true;ep.segmentTime=.15f;}}
    for(int i=0;i<notes;++i)e->noteOn(0,40+i,.7f);
    std::vector<double> times;times.reserve(blocks);bool finite=true;float peak=0;double sum=0;int misses=0;
    for(int block=0;block<blocks;++block){std::array<float,n> l{},r{};auto start=std::chrono::steady_clock::now();e->render(l.data(),r.data(),n,p);double cost=100*std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/(n/sr);times.push_back(cost);sum+=cost;misses+=cost>100;for(int i=0;i<n;++i){finite&=std::isfinite(l[i])&&std::isfinite(r[i]);peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});}}
    std::sort(times.begin(),times.end());std::cout<<(mode?"64_routes":"inactive")<<','<<notes<<','<<blocks*n/sr<<','<<sum/blocks<<','<<times[size_t(.95*(blocks-1))]<<','<<times[size_t(.99*(blocks-1))]<<','<<times.back()<<','<<misses<<','<<finite<<','<<peak<<'\n';if(!finite)return 1;
  }return 0;
}
int nexusBenchmark(bool longRun){
 std::cout<<"scenario,backend,sample_rate,block,voices,seconds,mean_budget_pct,p95,p99,max,misses,finite,peak\n";
 auto run=[&](int n,double sr,double seconds,bool fast,bool hd){auto e=std::make_unique<cosmos::Engine>();e->prepare(sr);cosmos::Patch p;p.experimentalNexusShared=fast;p.nexus.enabled=true;p.nexus.power=true;p.morph.enabled=true;p.morph.amount=.65f;p.experimentalGrouped=true;p.experimentalTiled=true;p.experimentalInterpolation=2;
 for(int j=0;j<8;++j){p.lfo[j].mode=1;p.lfo[j].rate=.3f+j*.11f;}for(int j=0;j<12;++j){p.modEnv[j].free.enabled=true;p.modEnv[j].play=1;p.modEnv[j].segmentTime=.15f;}for(int j=0;j<64;++j)p.matrix[j]={true,j<20?j+1:j<40?155+j%8:j<56?163+j%2:21+j%6,3+j%2,j%5,0,.0125f};p.macros.fill(.65f);
 if(hd)for(auto& l:p.layer)for(int o=0;o<5;++o){auto& x=l.osc[o];x.mode=cosmos::OscMode::wavetable;x.tableSource=1;x.level=o==0?1.f:.1f;x.unison=16;x.detune=15;x.morph=.4f;}
 for(int j=0;j<32;++j)e->noteOn(0,40+j,.7f);std::vector<float> l(n),r(n);int blocks=std::max(1,int(std::ceil(seconds*sr/n)));std::vector<double> times;times.reserve(blocks);double sum=0;int misses=0;bool finite=true;float peak=0;
 for(int j=0;j<blocks;++j){p.nexus.x=.5f+.45f*std::sin(float(j)*.013f);p.nexus.y=.5f+.45f*std::cos(float(j)*.017f);std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);auto start=std::chrono::steady_clock::now();e->render(l.data(),r.data(),n,p);double cost=100*std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/(n/sr);times.push_back(cost);sum+=cost;misses+=cost>100;for(int i=0;i<n;++i){finite&=std::isfinite(l[i])&&std::isfinite(r[i]);peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});}}
 std::sort(times.begin(),times.end());std::cout<<(hd?"hd20_unison16":"nexus_morph_matrix")<<','<<(fast?"shared":"reference")<<','<<sr<<','<<n<<",32,"<<blocks*n/sr<<','<<sum/blocks<<','<<times[size_t(.95*(blocks-1))]<<','<<times[size_t(.99*(blocks-1))]<<','<<times.back()<<','<<misses<<','<<finite<<','<<peak<<std::endl;return finite;};
 bool ok=true;if(longRun){ok&=run(128,48000,600,true,false);}else{for(double sr:{44100.,48000.,96000.,192000.})for(int n:{64,128,256,512,1024})for(bool fast:{false,true})ok&=run(n,sr,.10,fast,false);for(bool fast:{false,true}){ok&=run(128,48000,1,fast,true);ok&=run(128,48000,60,fast,false);}}return ok?0:1;
}

int fxQualification(){
 constexpr int n=128;constexpr double sr=48000.;
 std::cout<<"effect,profile,peak,rms,delta_rms,finite,mean_budget_pct\n";
 auto s=std::make_unique<cosmos::FxSlotState>();s->prepare(sr);std::array<float,8> m{};
 for(int type=1;type<cosmos::fxTypes;++type)for(int profile=1;profile<=3;++profile){
  s->reset();cosmos::FxParams p;p.enabled=true;p.type=type;p.a=p.b=profile==1?.2f:profile==2?.5f:.8f;
  if(type>=79){auto q=cosmos::creativePreset(type,profile);p.a=q[0];p.b=q[1];}
  else if(type>=64){auto q=cosmos::reverbPreset(type,profile);p.a=q[0];p.b=q[1];p.delayFeedbackR=q[2];p.delayTone=q[3];p.stereo=q[4];}
  else if(type>=49){auto q=cosmos::echoPreset(type,profile);p.a=q[0];p.b=q[1];p.delayFeedbackR=q[2];p.delayTone=q[3];}
  bool finite=true;double sq=0,diff=0;float peak=0;int frames=48000;
  auto start=std::chrono::steady_clock::now();
  for(int j=0;j<frames;++j){float dry=j<12000?.45f*std::sin(j*.061f)+.2f*std::sin(j*.147f):0,l=dry,r=j<12000?.35f*std::cos(j*.089f):0;s->tick(l,r,p,sr,m);finite&=std::isfinite(l)&&std::isfinite(r);peak=std::max({peak,std::abs(l),std::abs(r)});sq+=double(l)*l+double(r)*r;diff+=double(l-dry)*(l-dry);}
  double budget=100*std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/(frames/sr);
  std::cout<<type<<','<<profile<<','<<peak<<','<<std::sqrt(sq/(2*frames))<<','<<std::sqrt(diff/frames)<<','<<finite<<','<<budget<<'\n';if(!finite)return 1;
 }
 return 0;
}
int fxFortyBenchmark(){
 constexpr int n=128;constexpr double sr=48000.;
 std::cout<<"scenario,slots,rendered_seconds,mean_budget_pct,p95,p99,max,misses,peak,finite,fx_allocated_bytes\n";
 for(int mode=0;mode<3;++mode){
  auto e=std::make_unique<cosmos::Engine>();e->prepare(sr);cosmos::Patch p;
  const int mixed[]={9,19,29,49,64,80,90,99};
  for(int k=0;k<5;++k){p.fx[k].parallel=mode==2;for(int j=0;j<8;++j){auto& f=p.fx[k].slots[j];f.enabled=true;f.type=mode==0?mixed[j]:mode==1?80:mixed[j];f.mix=.15f;f.a=.55f;f.b=.55f;f.fftQuality=mode==1?2:0;}}
  for(int j=0;j<8;++j)e->noteOn(0,48+j,.65f);
  int blocks=mode==0?22500:375;std::vector<double> times;times.reserve(blocks);double sum=0;int misses=0;float peak=0;bool finite=true;
  for(int j=0;j<blocks;++j){std::array<float,n> l{},r{};auto start=std::chrono::steady_clock::now();e->render(l.data(),r.data(),n,p);double cost=100*std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/(n/sr);times.push_back(cost);sum+=cost;misses+=cost>100;for(int i=0;i<n;++i){finite&=std::isfinite(l[i])&&std::isfinite(r[i]);peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});}}
  std::sort(times.begin(),times.end());std::cout<<(mode==0?"mixed_serial":mode==1?"fft2048_serial":"mixed_parallel")<<",40,"<<blocks*n/sr<<','<<sum/blocks<<','<<times[size_t(.95*(blocks-1))]<<','<<times[size_t(.99*(blocks-1))]<<','<<times.back()<<','<<misses<<','<<peak<<','<<finite<<','<<e->fxAllocatedBytes()<<std::endl;if(!finite)return 1;
 }
 return 0;
}
int main(int argc,char** argv){if(argc>1&&std::string(argv[1])=="--fx-qa")return fxQualification();if(argc>1&&std::string(argv[1])=="--fx-40")return fxFortyBenchmark();if(argc>1&&std::string(argv[1])=="--nexus")return nexusBenchmark(false);if(argc>1&&std::string(argv[1])=="--nexus-long")return nexusBenchmark(true);if(argc>1&&std::string(argv[1])=="--modulation")return modulationBenchmark();
  constexpr int n=128;constexpr double sr=48000.;const bool longRun=argc>1&&std::string(argv[1])=="--long";const int blocks=longRun?22500:375;
  auto engine=std::make_unique<cosmos::Engine>();auto& e=*engine;e.prepare(sr);
  std::vector<float> l(n),r(n);
  std::cout<<"scenario,backend,rendered_seconds,voices,block,sample_rate,mean_cpu_pct,max_cpu_pct,engine_object_bytes,voice_pool_bytes,factory_hd_extra_bytes,imported_hd_table_bytes,peak,finite,p95_budget_pct,p99_budget_pct,deadline_misses,total_blocks\n";
  for(int mode=0;mode<17;++mode)for(int notes:{1,8,32}){
    if(longRun&&(notes!=32||(mode!=13&&mode!=14)))continue;
    cosmos::Patch p;e.reset();p.experimentalGrouped=mode>=3&&mode<=6;p.experimentalTiled=mode>=4&&mode<=6;p.experimentalInterpolation=mode==5?1:mode==6?2:0;p.experimentalGrainWindow=mode==7;p.experimentalGrainPrepared=mode==7;
    bool parallelHD=mode==8||mode==9||mode==13,parallelGrain=mode==10||mode==11||mode==14;
    p.experimentalAnalog=mode==12||mode==15;p.experimentalEnv=mode==12||mode==16;
    if(parallelHD){p.experimentalGrouped=true;p.experimentalTiled=true;p.experimentalInterpolation=2;}
    if(parallelGrain){p.experimentalGrainWindow=true;p.experimentalGrainPrepared=true;}
    std::unique_ptr<cosmos::ParallelBench> workers;
    if(mode>=8&&mode<=11)workers=std::make_unique<cosmos::ParallelBench>(e,mode%2?4:2,n);
    if(mode==13||mode==14)workers=std::make_unique<cosmos::ParallelBench>(e,4,n,true);
    for(auto& layer:p.layer){
      layer.enabled=true;layer.attack=.001f;
      for(int o=0;o<5;++o){
        auto& x=layer.osc[o];x.level=o==0?1.f:.1f;
        if(mode==1||(mode>=3&&mode<=6)||parallelHD){x.mode=cosmos::OscMode::wavetable;x.tableSource=1;x.unison=16;x.detune=20;x.morph=.45f;}
        if(mode==2||mode==7||parallelGrain){x.mode=cosmos::OscMode::granular;x.grain.source=1;x.grain.size=80;x.grain.density=30;}
      }
    }
    for(int i=0;i<notes;++i)e.noteOn(0,48+i,.7f);
    double sum=0,max=0;float peak=0;bool finite=true;std::vector<double> timings;timings.reserve(blocks);int missed=0;
    for(int b=0;b<blocks;++b){
      std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);
      auto start=std::chrono::steady_clock::now();if(workers)workers->render(l.data(),r.data(),n,p);else e.render(l.data(),r.data(),n,p);
      double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
      sum+=elapsed;max=std::max(max,elapsed);timings.push_back(100*elapsed/(n/sr));if(elapsed>n/sr)++missed;
      for(float x:l){finite&=std::isfinite(x);peak=std::max(peak,std::abs(x));}
      for(float x:r){finite&=std::isfinite(x);peak=std::max(peak,std::abs(x));}
    }
    std::sort(timings.begin(),timings.end());
    auto percentile=[&](double p){return timings[size_t(p*(timings.size()-1))];};
    std::cout<<(mode==15?"analog_lut_only":mode==16?"adsr_prepared_only":mode==13?"hd_parallel4_active":mode==14?"grain_parallel4_active":mode==12?"analog_prepared":mode==0?"analog":mode==1?"hd_unison16_scalar":mode==2?"granular":mode==3?"hd_unison16_grouped":mode==4?"hd_unison16_tiled":mode==5?"hd_unison16_simd_native":mode==6?"hd_unison16_simd_auto":mode==7?"granular_hann_lut":parallelHD?(mode==8?"hd_parallel2":"hd_parallel4"):(mode==10?"grain_parallel2":"grain_parallel4"))<<','<<(mode==3?cosmos::phaseBackend():mode==4?"cached_tile32":mode==5?cosmos::interpolationBackend(1):mode==6||parallelHD?cosmos::interpolationBackend(2):"scalar")<<','<<blocks*n/sr<<','<<notes<<','<<n<<','<<sr<<','<<100*sum/blocks/(n/sr)<<','<<100*max/(n/sr)<<','<<sizeof(cosmos::Engine)<<','<<sizeof(cosmos::Voice)*cosmos::voices<<','<<cosmos::hdBank().allocatedBytes()<<','<<cosmos::HDTable::bytes<<','<<peak<<','<<finite<<','<<percentile(.95)<<','<<percentile(.99)<<','<<missed<<','<<blocks<<'\n';
    if(!finite)return 1;
  }
}
