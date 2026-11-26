#include "CosmosEngine.h"
#include "CosmosParallelBench.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
int main(int argc,char** argv){
  constexpr int n=128;constexpr double sr=48000.;const bool longRun=argc>1&&std::string(argv[1])=="--long";const int blocks=longRun?22500:375;
  auto engine=std::make_unique<cosmos::Engine>();auto& e=*engine;e.prepare(sr);
  std::vector<float> l(n),r(n);
  std::cout<<"scenario,backend,rendered_seconds,voices,block,sample_rate,mean_cpu_pct,max_cpu_pct,engine_object_bytes,voice_pool_bytes,factory_hd_extra_bytes,imported_hd_table_bytes,peak,finite,p95_budget_pct,p99_budget_pct,deadline_misses,total_blocks\n";
  for(int mode=0;mode<13;++mode)for(int notes:{1,8,32}){
    if(longRun&&(notes!=32||(mode!=9&&mode!=11)))continue;
    cosmos::Patch p;e.reset();p.experimentalGrouped=mode>=3&&mode<=6;p.experimentalTiled=mode>=4&&mode<=6;p.experimentalInterpolation=mode==5?1:mode==6?2:0;p.experimentalGrainWindow=mode==7;p.experimentalGrainPrepared=mode==7;
    bool parallelHD=mode==8||mode==9,parallelGrain=mode==10||mode==11;
    p.experimentalAnalog=mode==12;p.experimentalEnv=mode==12;
    if(parallelHD){p.experimentalGrouped=true;p.experimentalTiled=true;p.experimentalInterpolation=2;}
    if(parallelGrain){p.experimentalGrainWindow=true;p.experimentalGrainPrepared=true;}
    std::unique_ptr<cosmos::ParallelBench> workers;
    if(mode>=8&&mode<=11)workers=std::make_unique<cosmos::ParallelBench>(e,mode%2?4:2,n);
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
    std::cout<<(mode==12?"analog_prepared":mode==0?"analog":mode==1?"hd_unison16_scalar":mode==2?"granular":mode==3?"hd_unison16_grouped":mode==4?"hd_unison16_tiled":mode==5?"hd_unison16_simd_native":mode==6?"hd_unison16_simd_auto":mode==7?"granular_hann_lut":parallelHD?(mode==8?"hd_parallel2":"hd_parallel4"):(mode==10?"grain_parallel2":"grain_parallel4"))<<','<<(mode==3?cosmos::phaseBackend():mode==4?"cached_tile32":mode==5?cosmos::interpolationBackend(1):mode==6||parallelHD?cosmos::interpolationBackend(2):"scalar")<<','<<blocks*n/sr<<','<<notes<<','<<n<<','<<sr<<','<<100*sum/blocks/(n/sr)<<','<<100*max/(n/sr)<<','<<sizeof(cosmos::Engine)<<','<<sizeof(cosmos::Voice)*cosmos::voices<<','<<cosmos::hdBank().allocatedBytes()<<','<<cosmos::HDTable::bytes<<','<<peak<<','<<finite<<','<<percentile(.95)<<','<<percentile(.99)<<','<<missed<<','<<blocks<<'\n';
    if(!finite)return 1;
  }
}
