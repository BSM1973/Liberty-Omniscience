#include "CosmosEngine.h"
#include <iostream>
#include <vector>
#include <cstdlib>
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
 std::cout<<"Cosmos foundation, hybrid, granular and HD tests passed\n";
}
