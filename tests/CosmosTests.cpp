#include "CosmosEngine.h"
#include "CosmosMotion.h"
#include "CosmosTrajectory.h"
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
 {
  for(int model:{26,27,28,29}){
    cosmos::FilterState st;cosmos::FilterPlan p({model,1000,.5f,0},48000);
    float echo=0;for(int j=0;j<=48;++j){float y=st.tick(j==0?1.f:0.f,p);if(j==48)echo=y;}
    check(model%2?echo<-.1f:echo>.1f,"comb polarity and delay");
  }
  for(int model:{35,36})for(double hz:{100.,1000.,8000.}){
    cosmos::FilterState st;cosmos::FilterPlan p({model,1200,.4f,0},48000);double in=0,out=0;
    for(int j=0;j<12000;++j){float x=float(std::sin(2*cosmos::pi*hz*j/48000));float y=st.tick(x,p);if(j>=6000){in+=x*x;out+=y*y;}}
    check(std::abs(out/in-1)<.01,"allpass unity magnitude");
  }
 }
 {
  for(int model=42;model<cosmos::filterModelCount;++model)for(float morph:{0.f,.5f,1.f}){
    cosmos::FilterState st;cosmos::FilterPlan p({model,1200,.8f,.1f,false,morph},48000);
    double energy=0;for(int j=0;j<8192;++j){float y=st.tick(j==0?1.f:0.f,p);check(std::isfinite(y)&&std::abs(y)<100,"creative filter bounded");energy+=double(y)*y;}
    check(energy>1e-10,"creative response nonzero");
  }
  cosmos::Env fe;fe.start();cosmos::EnvPlan plan(.01f,.02f,.5f,.03f,48000);for(int j=0;j<1600;++j)fe.tickPrepared(plan);
  check(std::abs(fe.level-.5f)<1e-4f,"filter envelope sustain");fe.stop();for(int j=0;j<1500;++j)fe.tickPrepared(plan);check(fe.stage==cosmos::Env::off,"filter envelope release");
 }
 {
  for(double sr:{44100.,48000.,96000.})for(int model=54;model<72;++model){
    cosmos::FilterState st;cosmos::FilterPlan p({model,1200,.8f,.2f,false,.5f},sr);
    double energy=0;for(int j=0;j<8192;++j){float x=j<4096?.5f*float(std::sin(2*cosmos::pi*330*j/sr)):0.f;float y=st.tick(x,p);check(std::isfinite(y)&&std::abs(y)<100,"ultimate filter bounded");energy+=double(y)*y;}
    check(energy>1e-10,"ultimate filter nonzero");
  }
  for(double sr:{44100.,48000.,96000.}){
    cosmos::FilterState st;cosmos::FilterPlan p({71,440,1,0},sr);
    int crossings=0;float previous=0;double energy=0;
    for(int j=0;j<int(sr);++j){float y=st.tick(j==0?.5f:0.f,p);if(j>=int(sr/2)){if(previous<=0&&y>0)++crossings;energy+=double(y)*y;}previous=y;}
    check(std::abs(2*crossings-440)<=4&&energy>1e-4,"self resonator frequency and sustained energy");
  }
  // Compare ADAA shaper to midpoint-free direct saturation at a coherent high tone.
  // Third harmonic folds from 30 kHz to 18 kHz at 48 kHz.
  double ar=0,ai=0,dr=0,di=0;float previous=0;
  for(int j=0;j<4800;++j){float x=2*float(std::sin(2*cosmos::pi*10000*j/48000)),delta=x-previous;
    float a=std::abs(delta)>.001f?(cosmos::FilterState::asymmetricPrimitive(x)-cosmos::FilterState::asymmetricPrimitive(previous))/delta:cosmos::FilterState::asymmetricShape(.5f*(x+previous));
    float d=cosmos::FilterState::asymmetricShape(x);previous=x;
    double c=std::cos(2*cosmos::pi*18000*j/48000),s=std::sin(2*cosmos::pi*18000*j/48000);ar+=a*c;ai+=a*s;dr+=d*c;di+=d*s;}
  check(std::hypot(ar,ai)<std::hypot(dr,di),"ADAA reduces selected folded third harmonic");
 }
 {
  double minRms=1e9,maxRms=0;
  for(double sr:{44100.,48000.,96000.}){
    cosmos::FilterState st;cosmos::FilterPlan p({71,440,1,0},sr);double sum=0;
    for(int j=0;j<int(sr*2);++j){float y=st.tick(j==0?.5f:0.f,p);if(j>=int(sr))sum+=double(y)*y;}
    double rms=std::sqrt(sum/int(sr));minRms=std::min(minRms,rms);maxRms=std::max(maxRms,rms);
  }
  check(minRms>.1&&maxRms/minRms<1.03,"self resonator steady amplitude across sample rates");
  for(int model:{26,27,28,29,49,50}){
    cosmos::FilterState used,fresh;cosmos::FilterPlan p({model,330,.9f,0},48000);
    for(int j=0;j<10000;++j)used.tick(.2f,p);
    used.reset();
    for(int j=0;j<10000;++j){float x=j==0?.4f:0.f;check(used.tick(x,p)==fresh.tick(x,p),"comb reset no stale history");}
  }
 }
 {
  cosmos::LfoParams p;p.depth=1;p.rate=2;cosmos::LfoState s;int rises=0;float prev=0;
  for(int i=0;i<48000;++i){float y=s.tick(p,cosmos::lfoHz(p,120)/48000);if(y>0&&prev<=0)++rises;prev=y;check(std::isfinite(y)&&std::abs(y)<=1,"LFO bounded");}
  check(rises==2,"LFO frequency");p.division=3;check(cosmos::lfoHz(p,90)==1.5,"LFO tempo sync");
  for(int shape=0;shape<6;++shape)for(int j=0;j<1000;++j){float y=cosmos::lfoWave(shape,j/1000.);check(std::isfinite(y)&&std::abs(y)<=1,"LFO shape range");}
  cosmos::Engine dry,wet,split;dry.prepare(48000);wet.prepare(48000);split.prepare(48000);
  cosmos::Patch a,b; b=a;b.lfo[0].destination=3;b.lfo[0].depth=.8f;b.lfo[0].rate=5;
  for(auto* e:{&dry,&wet,&split})e->noteOn(0,60,.8f);
  double diff=0;
  for(int block=0;block<50;++block){std::array<float,128> l{},r{},ll{},rr{},sl{},sr{};dry.render(l.data(),r.data(),128,a);wet.render(ll.data(),rr.data(),128,b);split.render(sl.data(),sr.data(),19,b);split.render(sl.data()+19,sr.data()+19,109,b);for(int j=0;j<128;++j){diff+=std::abs(ll[j]-l[j]);check(std::abs(ll[j]-sl[j])<1e-6f&&std::abs(rr[j]-sr[j])<1e-6f,"amplitude LFO independent of render partition");}}
  check(diff>.01,"LFO actual modulation");
 }
 {
  cosmos::LfoParams p;p.depth=1;
  for(int shape=0;shape<12;++shape)for(int j=0;j<2048;++j){p.shape=shape;p.polarity=0;auto a=cosmos::lfoAt(p,j/512.-2);p.polarity=1;auto b=cosmos::lfoAt(p,j/512.-2);check(std::isfinite(a)&&std::abs(a)<=1.00001f&&b>=0&&b<=1.00001f&&std::abs(b-(.5f+.5f*a))<1e-6f,"advanced waveform and polarity");}
  p.shape=9;p.polarity=0;check(std::abs(cosmos::lfoAt(p,.999999)-cosmos::lfoAt(p,1.000001))<1e-5f,"progressive random continuity");
  for(int target:{3,5,6,7,8}){
    auto a=std::make_unique<cosmos::Engine>(),b=std::make_unique<cosmos::Engine>();a->prepare(48000);b->prepare(48000);
    cosmos::Patch dry;for(int k=1;k<4;++k)dry.layer[k].enabled=false;auto& o=dry.layer[0].osc[0];if(target==6){o.mode=cosmos::OscMode::wavetable;o.morph=.5f;}if(target>=7){o.mode=cosmos::OscMode::granular;o.grain.source=1;o.grain.density=20;o.grain.position=.5f;}
    auto wet=dry;wet.lfo[0].destination=target;wet.lfo[0].depth=.8f;wet.lfo[0].rate=4;wet.lfo[0].mode=1;
    std::array<float,128> z{},zz{};b->render(z.data(),zz.data(),128,wet);
    a->noteOn(0,60,.8f);b->noteOn(0,60,.8f);double delta=0;
    for(int block=0;block<120;++block){std::array<float,128> l{},r{},ll{},rr{};a->render(l.data(),r.data(),128,dry);b->render(ll.data(),rr.data(),128,wet);for(int j=0;j<128;++j){check(std::isfinite(ll[j])&&std::isfinite(rr[j]),"advanced LFO output finite");delta+=std::abs(ll[j]-l[j]);}}
    check(delta>.001,"advanced LFO actual destination");
  }
 }
 {
  for(int mode=0;mode<3;++mode)for(int curve=0;curve<3;++curve)for(int play=0;play<3;++play){
    cosmos::ModEnvParams p;p.mode=mode;p.curve=curve;p.play=play;p.depth=1;p.attack=.01f;p.hold=.01f;p.decay=.01f;p.segmentTime=.01f;p.release=.01f;p.velocity=1;
    cosmos::ModEnvPlan plan(p,48000,120);cosmos::ModEnvState s;s.start();double energy=0;
    for(int j=0;j<4800;++j){float y=s.tick(plan,.5f);check(std::isfinite(y)&&y>=0&&y<=.500001f,"mod envelope bounded and velocity");energy+=y*y;}
    check(energy>0,"mod envelope signal");if(play==2)check(s.stage==cosmos::ModEnvState::off,"one shot complete");if(play==1)check(s.stage!=cosmos::ModEnvState::off,"loop remains active");
    s.stop();for(int j=0;j<1000;++j)s.tick(plan,.5f);check(s.stage==cosmos::ModEnvState::off,"envelope release complete");
  }
  cosmos::ModEnvParams p;p.sync=1;p.attack=1;p.depth=1;cosmos::ModEnvPlan plan(p,48000,120);cosmos::ModEnvState s;s.start();for(int j=0;j<12000;++j)s.tick(plan,1);check(std::abs(s.level-.5f)<1e-5f,"beat envelope timing");
  auto a=std::make_unique<cosmos::Engine>(),b=std::make_unique<cosmos::Engine>();a->prepare(48000);b->prepare(48000);cosmos::Patch dry,wet;wet.modEnv[11].destination=3;wet.modEnv[11].depth=-.8f;wet.modEnv[11].layer=1;a->noteOn(0,60,.8f);b->noteOn(0,60,.8f);double diff=0;
  for(int block=0;block<100;++block){std::array<float,128> l{},r{},ll{},rr{};a->render(l.data(),r.data(),128,dry);b->render(ll.data(),rr.data(),128,wet);for(int j=0;j<128;++j){check(std::isfinite(ll[j]),"mod env audio finite");diff+=std::abs(ll[j]-l[j]);}}
  check(diff>.01,"twelfth mod envelope actual audio route");
 }
 {
  auto a=std::make_unique<cosmos::Engine>(),b=std::make_unique<cosmos::Engine>();a->prepare(48000);b->prepare(48000);
  cosmos::Patch dry,wet;for(int k=1;k<4;++k){dry.layer[k].enabled=false;wet.layer[k].enabled=false;}
  for(auto& m:wet.matrix)m={true,23,3,1,0,-.01f};
  a->noteOn(0,60,.8f);b->noteOn(0,60,.8f);b->controller(0,1,127);
  double energyA=0,energyB=0;
  for(int block=0;block<100;++block){std::array<float,128> l{},r{},ll{},rr{};a->render(l.data(),r.data(),128,dry);b->render(ll.data(),rr.data(),128,wet);for(int j=0;j<128;++j){check(std::isfinite(ll[j]),"64 route output finite");energyA+=double(l[j])*l[j];energyB+=double(ll[j])*ll[j];}}
  check(energyA>0&&std::abs(std::sqrt(energyB/energyA)-.36)<.001,"64 routes additive CC1 modulation");
  for(int source:{1,8,9,20,21,22,24,25,26,154}){
    a->prepare(48000);b->prepare(48000);wet=dry;wet.matrix[63]={true,source,3,1,0,-.8f};
    a->noteOn(0,60,.8f);b->noteOn(0,60,.8f);b->pressure(0,100);b->polyAftertouch(0,60,100);b->controller(0,127,100);
    double diff=0;for(int block=0;block<100;++block){std::array<float,128> l{},r{},ll{},rr{};a->render(l.data(),r.data(),128,dry);b->render(ll.data(),rr.data(),128,wet);for(int j=0;j<128;++j)diff+=std::abs(l[j]-ll[j]);}check(diff>.001,"matrix source active without direct route");
  }
 }
 {
  cosmos::MatrixRoute r{true,155,3,1,1,1};r.curve=1;
  check(std::abs(cosmos::matrixValue(.25f,r)+.25f)<1e-6f,"signed curve");r.curve=2;check(std::abs(cosmos::matrixValue(.625f,r)-.5f)<1e-6f,"root curve");
  auto a=std::make_unique<cosmos::Engine>(),b=std::make_unique<cosmos::Engine>();a->prepare(48000);b->prepare(48000);cosmos::Patch dry,wet;
  for(int k=1;k<4;++k){dry.layer[k].enabled=false;wet.layer[k].enabled=false;}
  dry.layer[0].osc[1].level=.6f;wet=dry;wet.macros[7]=1;wet.matrix[63]={true,162,5,1,0,1,2,0,0};
  a->noteOn(0,60,.8f);b->noteOn(0,60,.8f);double diff=0;
  for(int block=0;block<50;++block){std::array<float,128> l{},r{},ll{},rr{};a->render(l.data(),r.data(),128,dry);b->render(ll.data(),rr.data(),128,wet);for(int j=0;j<128;++j){check(std::isfinite(ll[j]),"macro target finite");diff+=std::abs(ll[j]-l[j]);}}
  check(diff>.01&&std::abs(b->matrixValues()[63]-1)<1e-6f,"macro 8 target oscillator 2 and telemetry");
  b->prepare(48000);wet=dry;wet.matrix[0]={true,23,3,1,0,1,0,0,50};b->noteOn(0,60,.8f);std::array<float,128> l{},rr{};b->render(l.data(),rr.data(),128,wet);b->controller(0,1,127);b->render(l.data(),rr.data(),128,wet);auto sm=b->matrixValues()[0];check(sm>0&&sm<.1f,"MIDI smoothing actual");for(int j=0;j<200;++j)b->render(l.data(),rr.data(),128,wet);check(b->matrixValues()[0]>.99f,"MIDI smoothing converges");
 }
 {
  cosmos::MatrixRoute r{true,155,3,1,0,1};r.curve=4;r.points={0,.1f,.8f,.4f,1};
  check(std::abs(cosmos::matrixValue(.375f,r)-.45f)<1e-6f,"custom curve interpolation");
  r.polarity=1;check(std::abs(cosmos::matrixValue(.3125f,r)+.45f)<1e-6f,"custom curve signed interpolation");
  auto e=std::make_unique<cosmos::Engine>();e->prepare(48000);cosmos::Patch p;p.lfo[0].destination=3;p.lfo[0].depth=.5f;p.lfo[0].rate=2;e->noteOn(0,60,.8f);std::array<float,128> l{},rr{};e->render(l.data(),rr.data(),128,p);check(std::abs(e->lfoPhases()[0]-127.f*2/48000)<1e-5f,"LFO phase telemetry");
 }
 {
  cosmos::ModEnvParams p;p.free.enabled=true;p.free.count=4;p.free.x={0,.25f,.75f,1};p.free.y={0,1,.2f,.5f};p.free.curve[0]=1;p.segmentTime=1;p.depth=1;p.release=.01f;
  cosmos::ModEnvPlan plan(p,48000,120);check(std::abs(cosmos::freeEnvelopeValue(plan,.125f)-.25f)<1e-6f,"free envelope segment curve");
  cosmos::ModEnvState s;s.start();for(int i=0;i<48001;++i)s.tick(plan,1);check(s.stage==cosmos::ModEnvState::sustain&&std::abs(s.level-.5f)<1e-5f,"free envelope sustain");s.stop();for(int i=0;i<500;++i)s.tick(plan,1);check(s.stage==cosmos::ModEnvState::off,"free envelope release");
  auto a=std::make_unique<cosmos::Engine>(),b=std::make_unique<cosmos::Engine>();a->prepare(48000);b->prepare(48000);cosmos::Patch ref;ref.experimentalMatrixFast=false;ref.modEnv[0]=p;
  for(int j=0;j<64;++j)ref.matrix[j]={true,1+j%20,3,j%5,0,.01f};auto fast=ref;fast.experimentalMatrixFast=true;
  for(int j=0;j<32;++j){a->noteOn(0,40+j,.7f);b->noteOn(0,40+j,.7f);}
  for(int block=0;block<120;++block){std::array<float,128> l{},r{},ll{},rr{};a->render(l.data(),r.data(),128,ref);b->render(ll.data(),rr.data(),128,fast);for(int j=0;j<128;++j)check(l[j]==ll[j]&&r[j]==rr[j]&&std::isfinite(l[j]),"64 route layer range exact AB");}
 }
 {
  // Individual sources and every route slot, verified via actual voice telemetry.
  auto e=std::make_unique<cosmos::Engine>();e->prepare(48000);cosmos::Patch p;
  for(int j=0;j<8;++j){p.lfo[j].shape=0;p.lfo[j].rate=float(j+1);p.lfo[j].phase=.25f;}
  for(int j=0;j<12;++j){auto& ep=p.modEnv[j];ep.free.enabled=true;ep.free.count=16;ep.segmentTime=.001f;for(int k=0;k<16;++k){ep.free.x[k]=k/15.f;ep.free.y[k]=k/15.f;ep.free.curve[k]=k%3;}}
  for(int j=0;j<8;++j)p.macros[j]=(j+1)/8.f;
  for(int j=0;j<64;++j){int source=j<20?j+1:j<28?155+j-20:21;p.matrix[j]={true,source,3,1,0,.01f};}
  e->noteOn(0,60,.8f);std::array<float,128> l{},r{};e->render(l.data(),r.data(),128,p);
  auto values=e->matrixValues();auto phases=e->lfoPhases();
  for(int j=0;j<8;++j){check(std::abs(phases[j]-127.f*(j+1)/48000)<1e-5f,"individual LFO phase");check(values[j]>.009f,"individual LFO route");check(std::abs(values[20+j]-.01f*p.macros[j])<1e-6f,"individual macro route");}
  for(int j=0;j<12;++j){check(std::abs(values[8+j]-.01f)<1e-6f,"individual free envelope route");auto ep=p.modEnv[j];cosmos::ModEnvPlan plan(ep,48000,120);for(int k=0;k<16;++k)check(std::abs(cosmos::freeEnvelopeValue(plan,k/15.f)-k/15.f)<1e-5f,"all 192 free envelope points");}
  for(int j=28;j<64;++j)check(std::abs(values[j]-.008f)<1e-6f,"individual route slot");
  // MIDI per-channel pressure, poly pressure, pitch and CC; no channel leakage.
  e->prepare(48000);p=cosmos::Patch{};for(int j=0;j<4;++j)p.matrix[j]={true,24+j,3,1,0,.1f};
  e->noteOn(0,60,.8f);e->pressure(1,127);e->polyAftertouch(1,60,127);e->controller(1,0,127);e->render(l.data(),r.data(),128,p);values=e->matrixValues();check(values[0]==0&&values[1]==0&&values[3]==0,"MIDI channel isolation");
  e->pressure(0,127);e->polyAftertouch(0,60,64);e->pitch(0,12288);e->controller(0,0,100);e->render(l.data(),r.data(),128,p);values=e->matrixValues();check(std::abs(values[0]-.1f)<1e-6f&&std::abs(values[1]-.1f*64/127)<1e-6f&&std::abs(values[2]-.075f)<1e-6f&&std::abs(values[3]-.1f*100/127)<1e-6f,"MIDI source values");
  // Host PPQ supplied to engine: seek, tempo change, loop wrap and fallback.
  e->prepare(48000);p=cosmos::Patch{};p.lfo[0].mode=2;p.lfo[0].division=3;p.matrix[0]={true,1,3,1,0,.1f};p.transportValid=true;e->noteOn(0,60,.8f);
  for(double bpm:{60.,120.,180.})for(double ppq:{7.25,.125,-.5}){p.bpm=bpm;p.ppq=ppq;e->render(l.data(),r.data(),128,p);double end=ppq+127*bpm/(60*48000);check(std::abs(e->lfoPhases()[0]-(end-std::floor(end)))<1e-5,"transport phase seek and tempo");}
  p.transportValid=false;e->render(l.data(),r.data(),128,p);check(e->lfoPhases()[0]>=0&&e->lfoPhases()[0]<1,"transport fallback");
 }
 {
  for(int power:{0,1})for(int x=0;x<=10;++x)for(int y=0;y<=10;++y){auto w=cosmos::nexusWeights(x/10.f,y/10.f,power);float sum=0;for(auto a:w)sum+=power?a*a:a;check(std::abs(sum-1)<1e-6f,"NEXUS gain law");}
  for(int corner=0;corner<4;++corner){auto w=cosmos::nexusWeights(float(corner%2),corner<2?1.f:0.f,false);for(int j=0;j<4;++j)check(w[j]==float(j==corner),"NEXUS corner isolation");}
  auto a=std::make_unique<cosmos::Engine>(),b=std::make_unique<cosmos::Engine>();a->prepare(48000);b->prepare(48000);cosmos::Patch dry,wet;for(int j=1;j<4;++j)dry.layer[j].enabled=false;wet=dry;wet.nexus.enabled=true;wet.nexus.x=0;wet.nexus.y=1;wet.nexus.smooth=0;
  a->noteOn(0,60,.8f);b->noteOn(0,60,.8f);for(int block=0;block<16;++block){std::array<float,128> l{},r{},ll{},rr{};a->render(l.data(),r.data(),128,dry);b->render(ll.data(),rr.data(),128,wet);for(int j=0;j<128;++j)check(l[j]==ll[j]&&r[j]==rr[j],"NEXUS A matches isolated layer");}
  wet.nexus.macroX=8;wet.macros[7]=.75f;wet.nexus.ccY=74;b->controller(0,74,127);wet.matrix[0]={true,163,3,1,0,.1f};wet.matrix[1]={true,164,3,1,0,.1f};std::array<float,128> l{},r{};b->render(l.data(),r.data(),128,wet);auto v=b->matrixValues();check(std::abs(v[0]-.075f)<1e-6f&&std::abs(v[1]-.1f)<1e-6f,"NEXUS macro MIDI matrix sources");
  wet.nexus.smooth=100;wet.nexus.macroX=0;wet.nexus.x=0;b->render(l.data(),r.data(),128,wet);check(b->nexusPosition()[0]>.7f&&b->nexusPosition()[0]<.75f,"NEXUS smooth transition");
 }
 {
 cosmos::Patch p;p.layer[0].cutoff=100;p.layer[1].cutoff=10000;p.layer[0].tune=-12;p.layer[1].tune=12;p.morph.enabled=true;
 auto z=p;cosmos::applyMorph(z,.5f,1);check(std::abs(z.layer[0].cutoff-1000)<.01f,"morph logarithmic cutoff");check(z.layer[0].tune==-12,"morph protected tuning");check(p.layer[0].cutoff==100,"morph original intact");
 z=p;z.morph.family[3]=false;cosmos::applyMorph(z,1,1);check(z.layer[0].cutoff==100,"morph filter family lock");z=p;z.morph.amount=0;cosmos::applyMorph(z,1,1);check(z.layer[0].cutoff==100,"morph zero amount");z=p;z.morph.protectTune=false;cosmos::applyMorph(z,1,1);check(z.layer[0].tune==12,"morph corner tuning");
 auto a=std::make_unique<cosmos::Engine>();a->prepare(48000);p.nexus.enabled=true;p.nexus.x=.5f;p.nexus.y=1;a->noteOn(0,60,.8f);std::array<float,128> l{},r{};a->render(l.data(),r.data(),128,p);for(auto v:l)check(std::isfinite(v),"morph render finite");
 }
 {
 cosmos::MotionRecorder rec;rec.start(4,true);for(int j=0;j<=1024;++j)rec.capture(float(j)/1024,.25f,4./1024);check(rec.clip.count==1024,"motion capacity");auto a=rec.clip.at(.5,0);check(std::abs(a[0]-.5f)<.01f&&std::abs(a[1]-.25f)<1e-6f,"motion interpolation");auto b=rec.clip.at(1.25,1),c=rec.clip.at(.25,2),d=rec.clip.at(1.25,3);check(std::abs(b[0]-.25f)<.01f&&std::abs(c[0]-.75f)<.01f&&std::abs(d[0]-.75f)<.01f,"motion playback modes");auto f=cosmos::motionTransform({1,.5f},1,90,false,false);check(std::abs(f[0]-.5f)<1e-5f&&std::abs(f[1]-1)<1e-5f,"motion rotation");rec.start(8,false);rec.capture(.2f,.8f,.1);rec.finish();check(rec.clip.count>0&&rec.clip.duration>0,"motion early stop");
 }
 {
 for(int i=0;i<50;++i){auto tr=cosmos::factoryTrajectory(i);check(tr.count==96,"trajectory factory size");auto c=tr.clip(4,true);for(int j=0;j<c.count;++j)check(std::isfinite(c.x[j])&&c.x[j]>=0&&c.x[j]<=1&&c.y[j]>=0&&c.y[j]<=1,"trajectory bounded");if(tr.closed)check(std::abs(c.x[0]-c.x[c.count-1])<1e-6f&&std::abs(c.y[0]-c.y[c.count-1])<1e-6f,"closed trajectory");tr.curve=true;auto q=tr.at(.42f);check(std::isfinite(q[0])&&std::isfinite(q[1]),"Bezier trajectory");}
 auto q=cosmos::shapeTrajectory({1,0},.5f,.5f,.5f,.5f,0,0);check(std::abs(q[0]-.75f)<1e-6f&&std::abs(q[1]-.25f)<1e-6f,"trajectory scale");
 }
 std::cout<<"Cosmos foundation, hybrid, granular, HD, grouped, tiled, SIMD interpolation and Hann A/B tests passed\n";
}
