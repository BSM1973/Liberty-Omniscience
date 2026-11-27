#include "CosmosEngine.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <limits>
#include <complex>
#include <memory>
#include <numeric>
using namespace cosmos;
using Clock=std::chrono::steady_clock;
struct Tone {double rms=0,peak=0,fundamental=0,thd=0,residual=0,ns=0;bool finite=true;};
Tone tone(int model,double sr,float cutoff,float resonance,float drive,bool autoGain,float morph,double hz,float amp){
    FilterState state;FilterPlan plan({model,cutoff,resonance,drive,autoGain,morph},sr);
    int n=int(sr/10); // 0.1 s, integer multiples of 10 Hz for coherent analysis
    std::vector<float> x(n),y(n);double w=2*pi*hz/sr;
    for(int j=0;j<n;++j)x[j]=amp*float(std::sin(w*j));
    for(int i=0;i<3;++i)for(float s:x)state.tick(s,plan);
    auto start=Clock::now();for(int j=0;j<n;++j)y[j]=state.tick(x[j],plan);
    Tone t;t.ns=std::chrono::duration<double,std::nano>(Clock::now()-start).count()/n;
    double dc=0,energy=0;for(float s:y){t.finite&=std::isfinite(s);energy+=double(s)*s;dc+=s;t.peak=std::max(t.peak,std::abs(double(s)));}
    t.rms=std::sqrt(energy/n);dc/=n;
    double harmonicPower=0;
    for(int k=1;k<=10&&hz*k<sr*.5;++k){double re=0,im=0;for(int j=0;j<n;++j){re+=y[j]*std::cos(k*w*j);im+=y[j]*std::sin(k*w*j);}
      double power=2*(re*re+im*im)/(double(n)*n);if(k==1)t.fundamental=std::sqrt(power);else harmonicPower+=power;
    }
    t.thd=std::sqrt(harmonicPower)/std::max(t.fundamental,1e-12);
    t.residual=std::sqrt(std::max(0.,t.rms*t.rms-dc*dc-t.fundamental*t.fundamental-harmonicPower));
    return t;
}
double db(double x){return 20*std::log10(std::max(x,1e-12));}
int main(){
    std::ofstream report("cosmos-filter-quality.csv"),response("cosmos-filter-response.csv"),routing("cosmos-filter-routing.csv");
    report<<"model,sample_rate,cutoff,resonance,drive,auto_gain,morph,tone_hz,input_rms,output_rms,peak,gain_db,thd_ratio,unclassified_residual_rms,ns_per_sample,finite\n";
    response<<"model,sample_rate,requested_cutoff,resonance,frequency_hz,measured_db\n";
    routing<<"model_a,model_b,route,balance,max_error,finite\n";
    bool ok=true;int cases=0;
    for(int model=0;model<filterModelCount;++model)for(double sr:{44100.,48000.,96000.}){
      for(float res:{0.f,.65f,.95f})for(float drive:{0.f,.5f})for(bool ag:{false,true}){
        auto t=tone(model,sr,1200,res,drive,ag,.5f,1000,.2f);
        auto high=tone(model,sr,1200,res,drive,ag,.5f,10000,.2f);
        for(int h=0;h<2;++h){const auto& a=h?high:t;bool pass=a.finite&&a.peak<100.;ok&=pass;++cases;
          report<<model<<','<<sr<<",1200,"<<res<<','<<drive<<','<<ag<<",0.5,"<<(h?10000:1000)<<','<<.2/std::sqrt(2.)<<','<<a.rms<<','<<a.peak<<','<<db(a.fundamental/(.2/std::sqrt(2.)))<<','<<a.thd<<','<<a.residual<<','<<a.ns<<','<<pass<<'\n';}
      }
      FilterPlan p({model,1200,.2f,0},sr);FilterState st;
      std::vector<float> impulse(16384);for(int i=0;i<int(impulse.size());++i)impulse[i]=st.tick(i==0?.001f:0,p)/.001f;
      for(double hz:{50.,100.,200.,300.,600.,900.,1200.,1800.,2400.,4800.,9600.,18000.}){
        double w=2*pi*hz/sr,co=std::cos(w),si=std::sin(w),cr=1,ci=0,re=0,im=0;
        for(float x:impulse){re+=x*cr;im+=x*ci;double next=cr*co-ci*si;ci=ci*co+cr*si;cr=next;}
        double gain=db(std::hypot(re,im));ok&=std::isfinite(gain);response<<model<<','<<sr<<",1200,0.2,"<<hz<<','<<gain<<'\n';
      }
    }
    // End-to-end engine: compare series and parallel against independent filter states.
    constexpr int n=128;constexpr double sr=48000.;
    for(auto pair:{std::array<int,2>{4,5},{18,39},{26,30},{42,49},{57,71}})for(bool parallel:{false,true})for(float mix:{0.f,.25f,.5f,1.f}){
      auto engine=std::make_unique<Engine>();engine->prepare(sr);Patch p;
      for(auto& l:p.layer)l.enabled=false;
      auto& l=p.layer[0];l.enabled=true;l.gain=1;l.pan=-1;l.wave=0;l.cutoff=1200;l.attack=.001f;l.decay=.1f;l.sustain=.8f;l.filter1={pair[0],1200,.4f,0};l.filter2={pair[1],2400,.35f,0};l.filterParallel=parallel;l.filterMix=mix;l.filterTrim=.7f;
      FilterPlan a(l.filter1,sr),b(l.filter2,sr);FilterState sa,sb;Env env;env.start();engine->noteOn(0,69,1.f);
      float error=0;bool finite=true;double phase=0;
      for(int block=0;block<16;++block){std::array<float,n> left{},right{};engine->render(left.data(),right.data(),n,p);
        for(int j=0;j<n;++j){float x=oscillator(0,phase,440./sr);phase+=440./sr;if(phase>=1)phase-=1;
          float aOut=sa.tick(x,a),bOut=sb.tick(parallel?x:aOut,b);
          float ref=(parallel?std::lerp(aOut,bOut,mix):bOut)*l.filterTrim*env.tick(l,sr)*(.12f*p.master);
          finite&=std::isfinite(left[j])&&std::isfinite(right[j]);error=std::max(error,std::abs(left[j]-ref));}
      }
      ok&=finite&&error<1e-5f;routing<<pair[0]<<','<<pair[1]<<','<<(parallel?"parallel":"series")<<','<<mix<<','<<error<<','<<finite<<'\n';
    }
    std::cout<<"Filter QC: "<<cases<<" tone scenarios, "<<filterModelCount*3*12<<" response points, 40 routing scenarios; "<<(ok?"passed":"FAILED")<<"\n";
    return ok&&report&&response&&routing?0:1;
}
