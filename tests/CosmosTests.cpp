#include "CosmosEngine.h"
#include "CosmosParallelBench.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <utility>
void check(bool ok,const char* what){if(!ok){std::cerr<<what<<"\n";std::exit(1);}}
static_assert(sizeof(cosmos::Engine)<4096,"Engine voice storage must not live on the stack");
int main(){
 {
  cosmos::Engine e;e.prepare(48000);cosmos::Patch p;
  for(auto& l:p.layer)for(auto& o:l.osc){o.mode=cosmos::OscMode::granular;o.level=.15f;o.grain.source=1;o.grain.size=30;o.grain.density=20;}
  std::vector<float> l(128),r(128);
  for(int i=0;i<64;++i)e.noteOn(0,36+i,.7f);
  for(int b=0;b<16;++b){std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);e.render(l.data(),r.data(),128,p);
    check(std::all_of(l.begin(),l.end(),[](float x){return std::isfinite(x);}),"full voice pool finite");}
  e.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);e.render(l.data(),r.data(),128,p);
  check(std::all_of(l.begin(),l.end(),[](float x){return x==0;}),"voice pool reset silence");
 }

 for(double sr:{44100.,48000.,96000.}){
  cosmos::Engine e;e.prepare(sr);cosmos::Patch p;
  for(int k=0;k<4;++k){p.layer[k].wave=k;p.layer[k].release=.04f;}
  std::vector<float> l(4096),r(4096);
  e.noteOn(0,69,1);e.render(l.data(),r.data(),4096,p);
  double energy=0;for(auto x:l){check(std::isfinite(x),"finite output");energy+=x*x;}check(energy>.01,"audible output");
  e.noteOff(0,69);for(int i=0;i<4;++i){std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);e.render(l.data(),r.data(),4096,p);}
  check(std::all_of(l.begin(),l.end(),[](float x){return x==0;}),"release silence");
  e.reset();for(auto& a:p.layer)a.enabled=false;e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);
  check(std::all_of(l.begin(),l.end(),[](float x){return x==0;}),"disabled layers silence");
  e.reset();p.layer[0].enabled=true;p.layer[0].pan=-1;e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);
  check(std::all_of(r.begin(),r.end(),[](float x){return std::abs(x)<1e-7f;}),"hard left");
  e.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);p.layer[0].pan=0;
  e.noteOn(0,60,1);e.sustain(0,true);e.noteOff(0,60);e.render(l.data(),r.data(),4096,p);
  check(std::any_of(l.begin(),l.end(),[](float x){return std::abs(x)>1e-5f;}),"sustain pedal");
  e.sustain(0,false);for(int i=0;i<4;++i){std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);e.render(l.data(),r.data(),4096,p);}
  check(std::all_of(l.begin(),l.end(),[](float x){return x==0;}),"pedal release");
 }
 {
  const auto& bank=cosmos::waveBank();
  check(std::abs(bank.read(0,0,.001)-bank.read(1,0,.001))<1e-6f,"wavetable wrap");
  check(std::abs(bank.read(.21,0,.001)-bank.read(.21,.65f,.001))>.001f,"wavetable morph");
  cosmos::Engine e;e.prepare(48000);cosmos::Patch p;
  for(auto& l:p.layer)l.enabled=false;p.layer[0].enabled=true;p.layer[0].attack=.001f;
  std::vector<float> l(4096),r(4096);
  for(int mode=0;mode<4;++mode){
    e.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);
    auto& o=p.layer[0].osc[0];o.mode=cosmos::OscMode(mode);o.unison=16;o.detune=18;o.drift=3;o.morph=.5f;o.depth=.6f;o.drive=.2f;o.sync=true;o.ring=true;
    e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);
    double energy=0;for(float x:l){check(std::isfinite(x),"hybrid finite");energy+=x*x;}check(energy>.0001,"hybrid audible");
  }
  cosmos::SampleData sample;sample.rate=48000;sample.mono.assign(48000,.5f);p.layer[0].sample=&sample;
  auto& o=p.layer[0].osc[0];o={};o.level=1;o.mode=cosmos::OscMode::sample;
  e.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);
  check(std::any_of(l.begin(),l.end(),[](float x){return x>.001f;}),"sample playback");
  p.layer[0].sample=nullptr;e.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);
  check(std::all_of(l.begin(),l.end(),[](float x){return x==0;}),"missing sample silence");
  o.level=0;auto& extra=p.layer[0].osc[4];extra.level=1;extra.mode=cosmos::OscMode::wavetable;
  e.reset();e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);
  check(std::any_of(l.begin(),l.end(),[](float x){return std::abs(x)>.001f;}),"fifth oscillator audible");
 }
 {
  cosmos::Engine e;e.prepare(48000);cosmos::Patch p;
  for(auto& l:p.layer)l.enabled=false;p.layer[0].enabled=true;p.layer[0].attack=.001f;
  auto& o=p.layer[0].osc[0];o.mode=cosmos::OscMode::granular;o.grain.source=1;o.grain.size=60;o.grain.density=35;
  std::vector<float> l(4096),r(4096);
  for(int w=0;w<3;++w){
    e.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);
    o.grain.window=w;o.grain.reverse=w==1;o.grain.freeze=w==2;o.grain.loopStart=.2f;o.grain.loopEnd=.8f;
    e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);
    double sum=0;for(float x:l){check(std::isfinite(x),"granular finite");sum+=x*x;}check(sum>.0001,"granular audible");
    auto positions=e.grainPositions(0,0,&cosmos::factoryGrains()[0]);check(std::any_of(positions.begin(),positions.end(),[](float x){return x>=0;}),"grain telemetry");
  }
  e.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);o.grain.source=0;
  e.noteOn(0,60,1);e.render(l.data(),r.data(),4096,p);check(std::all_of(l.begin(),l.end(),[](float x){return x==0;}),"granular missing sample silence");
 }
 {
  const auto& hd=cosmos::hdBank();
  check(hd.allocatedBytes()==0,"factory HD duplicate storage eliminated");
  for(float m:{0.f,.2f,.4f,.6f,.8f,1.f})for(int i=0;i<128;++i){
    double phase=i/128.;float a=hd.read(phase,m,.001),b=cosmos::waveBank().read(phase,m,.001);
    check(std::isfinite(a)&&std::abs(a-b)<.025f,"HD legacy interpolation");
  }
  std::vector<float> cycle(cosmos::tableSize);
  for(int i=0;i<cosmos::tableSize;++i)cycle[size_t(i)]=float(.7*std::sin(2*cosmos::hpi*i/cosmos::tableSize)+.2*std::sin(2*cosmos::hpi*64*i/cosmos::tableSize));
  cosmos::HDTable user(cycle);
  double err=0;for(int i=0;i<cosmos::tableSize;++i){double phase=double(i)/cosmos::tableSize;double d=user.read(phase,.5f,.1)-.7*std::sin(2*cosmos::hpi*phase);err+=d*d;}
  check(std::sqrt(err/cosmos::tableSize)<.01,"band limiting high harmonics");
 }
 {
  cosmos::Engine scalar,grouped;scalar.prepare(48000);grouped.prepare(48000);
  cosmos::Patch a;for(auto& layer:a.layer)for(auto& o:layer.osc){o.mode=cosmos::OscMode::wavetable;o.tableSource=1;o.level=.2f;o.unison=16;o.detune=23;o.morph=.43f;}
  auto b=a;b.experimentalGrouped=true;
  for(int i=0;i<8;++i){scalar.noteOn(0,48+i,.7f);grouped.noteOn(0,48+i,.7f);}
  std::vector<float> l(128),r(128),ll(128),rr(128);double err=0,ref=0;float peak=0;
  for(int block=0;block<64;++block){
    for(auto* v:{&l,&r,&ll,&rr})std::fill(v->begin(),v->end(),0.f);
    if(block==32){scalar.pitch(0,10000);grouped.pitch(0,10000);}
    scalar.render(l.data(),r.data(),128,a);grouped.render(ll.data(),rr.data(),128,b);
    for(int i=0;i<128;++i){
      for(auto pair:{std::pair<float,float>{l[i],ll[i]},std::pair<float,float>{r[i],rr[i]}}){
        double d=pair.first-pair.second;err+=d*d;ref+=double(pair.first)*pair.first;peak=std::max(peak,std::abs(pair.first-pair.second));
      }
    }
  }
  check(ref>0&&std::sqrt(err/ref)<1e-5&&peak<1e-4f,"grouped engine equivalence");
 }
 {
  cosmos::Engine scalar,tiled;scalar.prepare(48000);tiled.prepare(48000);
  cosmos::Patch a;
  for(auto& layer:a.layer){layer.release=.015f;for(auto& o:layer.osc){o.mode=cosmos::OscMode::wavetable;o.tableSource=1;o.level=.2f;o.unison=16;o.detune=23;o.morph=.43f;}}
  auto b=a;b.experimentalGrouped=true;b.experimentalTiled=true;
  for(int i=0;i<8;++i){scalar.noteOn(0,48+i,.7f);tiled.noteOn(0,48+i,.7f);}
  std::vector<float> l(137),r(137),ll(137),rr(137);double err=0,ref=0;float peak=0;
  for(int block=0;block<64;++block){
    int n=block%2?137:19;
    for(auto* v:{&l,&r,&ll,&rr})std::fill(v->begin(),v->end(),0.f);
    if(block==16){scalar.pitch(0,10000);tiled.pitch(0,10000);}
    if(block==24){a.layer[0].osc[0].morph=.77f;b.layer[0].osc[0].morph=.77f;}
    if(block==32)for(int i=0;i<8;++i){scalar.noteOff(0,48+i);tiled.noteOff(0,48+i);}
    scalar.render(l.data(),r.data(),n,a);tiled.render(ll.data(),rr.data(),n,b);
    for(int i=0;i<n;++i)for(auto pair:{std::pair<float,float>{l[i],ll[i]},std::pair<float,float>{r[i],rr[i]}}){
      double d=pair.first-pair.second;err+=d*d;ref+=double(pair.first)*pair.first;peak=std::max(peak,std::abs(pair.first-pair.second));
    }
  }
  check(ref>0&&std::sqrt(err/ref)<1e-5&&peak<1e-4f,"tiled engine equivalence");
  for(double lo:{0.,.125,123.45})for(double length:{1.,3.5,47892.1})for(double x:{-100000.,-1.,0.,1.,123.45,48000.,100000.}){
    double y=std::fmod(x-lo,length);double reference=lo+(y<0?y+length:y);
    check(std::abs(cosmos::grainWrap(x,lo,lo+length)-reference)<1e-8,"fast granular wrap");
  }
 }
 {
  // Compare both interpolation backends against the reference processor path.
  for(int backend:{1,2}){
    cosmos::Engine scalar,fast;scalar.prepare(48000);fast.prepare(48000);
    cosmos::Patch a;
    for(auto& layer:a.layer){layer.release=.03f;for(auto& o:layer.osc){o.mode=cosmos::OscMode::wavetable;o.tableSource=1;o.level=.2f;o.unison=16;o.detune=23;o.morph=.43f;}}
    auto b=a;b.experimentalGrouped=true;b.experimentalTiled=true;b.experimentalInterpolation=backend;
    for(int i=0;i<8;++i){scalar.noteOn(0,48+i,.7f);fast.noteOn(0,48+i,.7f);}
    std::vector<float> l(137),r(137),ll(137),rr(137);double err=0,ref=0;float peak=0;
    for(int block=0;block<64;++block){
      int n=block%2?137:19;
      for(auto* v:{&l,&r,&ll,&rr})std::fill(v->begin(),v->end(),0.f);
      if(block==16){scalar.pitch(0,10000);fast.pitch(0,10000);}
      if(block==24){a.layer[0].osc[0].morph=.77f;b.layer[0].osc[0].morph=.77f;}
      if(block==32)for(int i=0;i<8;++i){scalar.noteOff(0,48+i);fast.noteOff(0,48+i);}
      scalar.render(l.data(),r.data(),n,a);fast.render(ll.data(),rr.data(),n,b);
      for(int i=0;i<n;++i)for(auto pair:{std::pair<float,float>{l[i],ll[i]},std::pair<float,float>{r[i],rr[i]}}){
        double d=pair.first-pair.second;err+=d*d;ref+=double(pair.first)*pair.first;peak=std::max(peak,std::abs(pair.first-pair.second));
      }
    }
    check(ref>0&&std::sqrt(err/ref)<1e-5&&peak<1e-4f,"SIMD interpolation equivalence");
  }
  for(int i=0;i<=65536;++i){
    float x=float(i)/65536.f;
    check(std::abs(cosmos::fastGrainWindow(0,x)-cosmos::grainWindow(0,x))<1e-6f,"Hann LUT error bound");
  }
  cosmos::Engine scalar,fast;scalar.prepare(48000);fast.prepare(48000);
  cosmos::Patch a;
  for(auto& layer:a.layer)for(auto& o:layer.osc){o.mode=cosmos::OscMode::granular;o.grain.source=1;o.grain.size=80;o.grain.density=30;o.level=.2f;}
  auto b=a;b.experimentalGrainWindow=true;b.experimentalGrainPrepared=true;
  for(int i=0;i<8;++i){scalar.noteOn(0,48+i,.7f);fast.noteOn(0,48+i,.7f);}
  std::vector<float> l(128),r(128),ll(128),rr(128);double err=0,ref=0;
  for(int block=0;block<128;++block){
    for(auto* v:{&l,&r,&ll,&rr})std::fill(v->begin(),v->end(),0.f);
    scalar.render(l.data(),r.data(),128,a);fast.render(ll.data(),rr.data(),128,b);
    for(int i=0;i<128;++i){double d=l[i]-ll[i];err+=d*d;ref+=double(l[i])*l[i];}
  }
  check(ref>0&&std::sqrt(err/ref)<1e-5,"granular LUT engine equivalence");
 }
 {
  cosmos::Engine serial,parallel;serial.prepare(48000);parallel.prepare(48000);
  cosmos::ParallelBench workers(parallel,4,137);
  cosmos::Patch p;p.experimentalGrouped=true;p.experimentalTiled=true;p.experimentalInterpolation=2;
  for(auto& layer:p.layer)for(auto& o:layer.osc){o.mode=cosmos::OscMode::wavetable;o.level=.2f;o.unison=16;o.detune=17;o.tableSource=1;}
  for(int i=0;i<32;++i){serial.noteOn(0,48+i,.7f);parallel.noteOn(0,48+i,.7f);}
  std::vector<float> l(137),r(137),ll(137),rr(137);
  for(int b=0;b<48;++b){
    int n=b%2?137:19;
    for(auto* x:{&l,&r,&ll,&rr})std::fill(x->begin(),x->end(),0.f);
    if(b==16){serial.pitch(0,9000);parallel.pitch(0,9000);}
    if(b==24)for(int i=0;i<32;++i){serial.noteOff(0,48+i);parallel.noteOff(0,48+i);}
    serial.render(l.data(),r.data(),n,p);workers.render(ll.data(),rr.data(),n,p);
    for(int i=0;i<n;++i)check(std::abs(l[i]-ll[i])<1e-6f&&std::abs(r[i]-rr[i])<1e-6f,"parallel ordered mix equivalence");
  }
 }
 {
  for(int wave=0;wave<4;++wave)for(int i=0;i<65536;++i){
    double phase=double(i)/65536.;
    check(std::abs(cosmos::fastOscillator(wave,phase,.017)-cosmos::oscillator(wave,phase,.017))<1e-6f,"analog LUT equivalence");
  }
  for(double sr:{44100.,48000.,96000.}){
    cosmos::Layer p;p.attack=.003f;p.decay=.015f;p.release=.02f;
    cosmos::Env a,b;a.start();b.start();
    for(int i=0;i<12000;++i){
      if(i==1500)p.sustain=.37f;if(i==4000){a.stop();b.stop();}
      if(i==4500)p.release=.01f;
      check(a.tick(p,sr)==b.tickPrepared(cosmos::EnvPlan(p,sr))&&a.stage==b.stage,"ADSR prepared equivalence");
    }
  }
  // Coherent spectral probe: high harmonics in a user table must not fold
  // into the measured band when the mip level excludes them.
  std::vector<float> cycle(cosmos::tableSize);
  for(int i=0;i<cosmos::tableSize;++i)cycle[i]=float(.5*std::sin(2*cosmos::pi*i/cosmos::tableSize)+.4*std::sin(2*cosmos::pi*63*i/cosmos::tableSize));
  cosmos::HDTable table(cycle);
  for(double delta:{1./16.,3./32.,5./32.}){
    double err=0,signal=0;
    for(int i=0;i<4096;++i){
      double phase=cosmos::wrap(i*delta),ref=.5*std::sin(2*cosmos::pi*phase);
      double d=table.read(phase,.5f,delta)-ref;err+=d*d;signal+=ref*ref;
    }
    check(std::sqrt(err/signal)<.0001,"coherent wavetable alias residual");
  }
 }
 {
  cosmos::Engine serial,parallel;serial.prepare(48000);parallel.prepare(48000);
  cosmos::ParallelBench pool(parallel,4,128,true);
  cosmos::Patch p;p.experimentalGrouped=true;p.experimentalTiled=true;p.experimentalInterpolation=2;
  for(auto& layer:p.layer){layer.release=.005f;for(auto& o:layer.osc){o.mode=cosmos::OscMode::wavetable;o.level=.2f;o.tableSource=1;o.unison=4;}}
  std::vector<float> l(128),r(128),ll(128),rr(128);
  for(int b=0;b<80;++b){
    if(b==1||b==30){serial.noteOn(0,60,.7f);parallel.noteOn(0,60,.7f);}
    if(b==4)for(int i=0;i<8;++i){serial.noteOn(0,48+i,.7f);parallel.noteOn(0,48+i,.7f);}
    if(b==15){serial.allOff(0,false);parallel.allOff(0,false);}
    if(b==50){serial.allOff(0,true);parallel.allOff(0,true);}
    for(auto* x:{&l,&r,&ll,&rr})std::fill(x->begin(),x->end(),0.f);
    serial.render(l.data(),r.data(),128,p);pool.render(ll.data(),rr.data(),128,p);
    for(int i=0;i<128;++i)check(std::abs(l[i]-ll[i])<1e-6f&&std::abs(r[i]-rr[i])<1e-6f,"compact active voice pool equivalence");
  }
 }
 {
  cosmos::Engine serial,parallel;serial.prepare(48000);parallel.prepare(48000);
  cosmos::ParallelBench pool(parallel,4,128,true);
  cosmos::Patch p;
  std::vector<float> l(128),r(128),ll(128),rr(128);
  for(int block=0;block<80;++block){
    if(block%8==0){serial.allOff(0,true);parallel.allOff(0,true);
      for(int i=0;i<(block/8)%5;++i){serial.noteOn(0,48+i,.7f);parallel.noteOn(0,48+i,.7f);}}
    for(auto* x:{&l,&r,&ll,&rr})std::fill(x->begin(),x->end(),0.f);
    serial.render(l.data(),r.data(),128,p);pool.render(ll.data(),rr.data(),128,p);
    for(int i=0;i<128;++i)check(std::abs(l[i]-ll[i])<1e-6f&&std::abs(r[i]-rr[i])<1e-6f,"adaptive worker count equivalence");
  }
 }
 {
  for(double sr:{44100.,48000.,96000.})for(int model=0;model<cosmos::filterModelCount;++model){
    cosmos::FilterState f;cosmos::FilterPlan p({model,1200,.75f,.3f},sr);double energy=0;
    for(int j=0;j<8192;++j){float y=f.tick(j==0?1.f:0.f,p);check(std::isfinite(y)&&std::abs(y)<100,"filter impulse bounded");energy+=double(y)*y;}
    check(energy>0,"filter impulse audible");
  }
  for(int model:{4,5,8,9,14,15}){cosmos::FilterState low,high;cosmos::FilterPlan p({model,1000,0,0},48000);double a=0,b=0;
    for(int j=0;j<8192;++j){float x=low.tick(float(std::sin(2*cosmos::pi*100*j/48000)),p),y=high.tick(float(std::sin(2*cosmos::pi*8000*j/48000)),p);if(j>4096){a+=x*x;b+=y*y;}}
    check(model%2==0?a>b:b>a,"filter frequency discrimination");
  }
 }
 {
  for(double sr:{44100.,48000.,96000.})for(int model=18;model<cosmos::filterModelCount;++model){
    cosmos::FilterState st;double energy=0;
    for(int i=0;i<16000;++i){
      cosmos::FilterPlan p({model,i<8000?900.f:18000.f,1.f,.8f,true},sr);
      float y=st.tick(i<4000?float(.8*std::sin(2*cosmos::pi*330*i/sr)):0.f,p);
      check(std::isfinite(y)&&std::abs(y)<30.f,"analog filter bounded under resonance and cutoff jump");energy+=double(y)*y;
    }
    check(energy>1e-6,"analog filter signal");
  }
 }
 std::cout<<"Cosmos foundation, hybrid, granular, HD, grouped, tiled, SIMD interpolation and Hann A/B tests passed\n";
}
