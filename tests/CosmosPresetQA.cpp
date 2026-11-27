#include <juce_audio_utils/juce_audio_utils.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <array>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
void cosmosSetReferenceDSP(juce::AudioProcessor&,bool);
bool cosmosTestMidiLearn(juce::AudioProcessor&);
constexpr int block=128, blocks=375, frames=block*blocks;
constexpr double sr=48000.;
struct Render {std::vector<float> data;double rms=0;float peak=0;bool finite=true;};
Render render(juce::AudioProcessor& p){
    p.releaseResources();p.prepareToPlay(sr,block);
    Render out;out.data.reserve(frames*2);double sum=0;
    juce::AudioBuffer<float> audio(2,block);
    for(int i=0;i<blocks;++i){
        juce::MidiBuffer midi;
        if(i==0)midi.addEvent(juce::MidiMessage::noteOn(1,60,.75f),0);
        if(i==300)midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
        audio.clear();p.processBlock(audio,midi);
        for(int j=0;j<block;++j)for(int ch=0;ch<2;++ch){
            float x=audio.getSample(ch,j);out.finite&=std::isfinite(x);out.peak=std::max(out.peak,std::abs(x));sum+=double(x)*x;out.data.push_back(x);
        }
    }
    out.rms=std::sqrt(sum/out.data.size());return out;
}
void u16(std::ostream& f,unsigned v){f.put(char(v&255));f.put(char((v>>8)&255));}
void u32(std::ostream& f,unsigned v){u16(f,v&65535);u16(f,v>>16);}
bool wav(const std::filesystem::path& path,const Render& r){
    std::ofstream f(path,std::ios::binary);
    unsigned size=unsigned(r.data.size()*2);
    f.write("RIFF",4);u32(f,36+size);f.write("WAVEfmt ",8);u32(f,16);u16(f,1);u16(f,2);u32(f,48000);u32(f,192000);u16(f,4);u16(f,16);f.write("data",4);u32(f,size);
    for(float x:r.data){auto n=std::int16_t(std::lround(std::clamp(std::isfinite(x)?x:0.f,-1.f,1.f)*32767));u16(f,std::uint16_t(n));}
    return bool(f);
}
// Measures the actual JUCE processor callback, not just the standalone DSP engine.
bool qualifyProcessor(juce::AudioProcessor& a,juce::AudioProcessor& b,juce::AudioProcessorParameter& ma,juce::AudioProcessorParameter& mb){
    std::ofstream report("cosmos-processor-qualification.csv");
    report<<"preset,sample_rate,block,voices,mean_budget_pct,p95_budget_pct,p99_budget_pct,max_budget_pct,misses,blocks,reference_rms_error,reference_peak_error,finite,dsp_pass,deadline_pass\n";
    bool all=true;
    for(int preset:{0,15,50,100,115,150,199})for(double rate:{44100.,48000.,96000.})for(int n:{64,128,512})for(int notes:{1,8,32}){
        a.setCurrentProgram(preset);juce::MemoryBlock state;a.getStateInformation(state);b.setStateInformation(state.getData(),int(state.getSize()));
        a.releaseResources();b.releaseResources();a.prepareToPlay(rate,n);b.prepareToPlay(rate,n);
        juce::AudioBuffer<float> x(2,n),y(2,n);juce::MidiBuffer mx,my;
        mx.ensureSize(4096);my.ensureSize(4096);
        const int count=std::max(32,int(rate*.25/n));
        std::vector<double> times;times.reserve(count);
        double sum=0,err=0,signal=0;float peak=0;int misses=0;bool finite=true;
        for(int i=0;i<count;++i){
            mx.clear();my.clear();
            if(i==0)for(int j=0;j<notes;++j)mx.addEvent(juce::MidiMessage::noteOn(1,40+j,.65f),j%std::min(n,13));
            if(i==count/4)mx.addEvent(juce::MidiMessage::pitchWheel(1,9300),n/3);
            if(i==count/2){ma.setValueNotifyingHost(.53f);mb.setValueNotifyingHost(.53f);mx.addEvent(juce::MidiMessage::controllerEvent(1,64,127),n/4);}
            if(i==count*3/4)for(int j=0;j<notes;++j)mx.addEvent(juce::MidiMessage::noteOff(1,40+j),j%std::min(n,13));
            if(i==count-2)mx.addEvent(juce::MidiMessage::controllerEvent(1,64,0),n/3);
            my.addEvents(mx,0,n,0);
            const auto start=std::chrono::steady_clock::now();a.processBlock(x,mx);
            const double budget=100.*std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()*rate/n;
            times.push_back(budget);sum+=budget;misses+=budget>100.;
            b.processBlock(y,my);
            for(int ch=0;ch<2;++ch)for(int j=0;j<n;++j){
                float u=x.getSample(ch,j),v=y.getSample(ch,j);finite&=std::isfinite(u)&&std::isfinite(v);
                double delta=double(u)-v;err+=delta*delta;signal+=double(v)*v;peak=std::max(peak,std::abs(u-v));
            }
        }
        std::sort(times.begin(),times.end());
        auto p=[&](double f){return times[size_t(f*(times.size()-1))];};
        double relative=std::sqrt(err/std::max(signal,1e-20));
        bool pass=finite&&signal>1e-12&&peak<1e-4f&&relative<1e-4;
        report<<preset+1<<','<<rate<<','<<n<<','<<notes<<','<<sum/count<<','<<p(.95)<<','<<p(.99)<<','<<times.back()<<','<<misses<<','<<count<<','<<relative<<','<<peak<<','<<finite<<','<<pass<<','<<(misses==0)<<'\n';
        all&=pass;
        if(!pass)std::cerr<<"Processor qualification DSP mismatch: "<<preset+1<<" / "<<rate<<" / "<<n<<" / "<<notes<<'\n';
    }
    std::cout<<"Processor qualification matrix: 189 scenarios; DSP "<<(all?"passed":"FAILED")<<"; deadline outcomes in CSV (informational on shared runners)\n";
    return all&&bool(report);
}
bool cosmosTestImport(juce::AudioProcessor&,const juce::File&,bool);
bool cosmosTestAssets(juce::AudioProcessor&,bool);
bool cosmosTestCache(juce::AudioProcessor&);
bool cosmosTestLayout(juce::AudioProcessor&,const juce::File&);
void be16(std::ostream& f,unsigned v){f.put(char((v>>8)&255));f.put(char(v&255));}
void be32(std::ostream& f,unsigned v){be16(f,v>>16);be16(f,v&65535);}
bool aiff(const std::filesystem::path& path,const Render& r){
    std::ofstream f(path,std::ios::binary);unsigned bytes=unsigned(r.data.size()*2);
    f.write("FORM",4);be32(f,46+bytes);f.write("AIFFCOMM",8);be32(f,18);be16(f,2);be32(f,unsigned(r.data.size()/2));be16(f,16);
    const unsigned char rate[]={0x40,0x0e,0xbb,0x80,0,0,0,0,0,0};f.write(reinterpret_cast<const char*>(rate),10);
    f.write("SSND",4);be32(f,bytes+8);be32(f,0);be32(f,0);
    for(float x:r.data)be16(f,std::uint16_t(std::int16_t(std::lround(std::clamp(x,-1.f,1.f)*32767))));
    return bool(f);
}
bool qualifyImports(){
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("cosmos-assets","",false);
    if(!dir.createDirectory())return false;
    auto file=dir.getChildFile("cycle.wav");Render cycle;cycle.data.resize(4096);
    for(int i=0;i<2048;++i)cycle.data[2*i]=cycle.data[2*i+1]=.5f*float(std::sin(6.283185307179586*i/2048.));
    bool ok=wav(std::filesystem::path(file.getFullPathName().toStdString()),cycle);
    auto p=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ok=ok&&cosmosTestImport(*p,file,false)&&cosmosTestImport(*p,file,true)&&cosmosTestAssets(*p,true);
    juce::MemoryBlock state;p->getStateInformation(state);
    auto restored=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    for(int i=0;i<12;++i){restored->setStateInformation(state.getData(),int(state.getSize()));ok=ok&&cosmosTestAssets(*restored,true)&&cosmosTestCache(*restored);}
    ok=ok&&!cosmosTestImport(*restored,dir.getChildFile("missing.wav"),true);
    ok=file.deleteFile()&&ok;
    restored->setStateInformation(state.getData(),int(state.getSize()));ok=ok&&cosmosTestAssets(*restored,false);
    auto af=dir.getChildFile("cycle.aiff");
    auto ai=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    ok=aiff(std::filesystem::path(af.getFullPathName().toStdString()),cycle)&&ok;
    ok=cosmosTestImport(*ai,af,false)&&ok;
    juce::MemoryBlock aiState;ai->getStateInformation(aiState);
    auto aiRestored=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    aiRestored->setStateInformation(aiState.getData(),int(aiState.getSize()));
    ok=cosmosTestImport(*aiRestored,af,false)&&ok;
    dir.deleteRecursively();
    std::cout<<"Personal WAV/AIFF/table import and repeated restore: "<<(ok?"passed":"FAILED")<<'\n';
    return ok;
}
int main(){
    juce::ScopedJuceInitialiser_GUI gui;
    auto a=std::unique_ptr<juce::AudioProcessor>(createPluginFilter()),b=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto master=[](juce::AudioProcessor& p)->juce::AudioProcessorParameter*{
        for(auto* x:p.getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(x))if(id->paramID=="master")return x;
        return nullptr;
    };
    cosmosSetReferenceDSP(*b,true);
    auto* ma=master(*a);auto* mb=master(*b);if(!ma||!mb||a->getNumPrograms()!=270)return 1;
    std::filesystem::create_directories("cosmos-preset-audio");
    std::ofstream report("cosmos-presets.csv");
    report<<"preset,name,rms,peak,finite,reference_state_max_error,automation_restored,passed\n";
    int failed=0;
    for(int i=0;i<a->getNumPrograms();++i){
        a->setCurrentProgram(i);ma->beginChangeGesture();ma->setValueNotifyingHost(.61f);ma->endChangeGesture();
        juce::MemoryBlock state;a->getStateInformation(state);
        b->setCurrentProgram((i+1)%270);b->setStateInformation(state.getData(),int(state.getSize()));
        auto r=render(*a),s=render(*b);float maxError=0;
        for(size_t j=0;j<r.data.size();++j)maxError=std::max(maxError,std::abs(r.data[j]-s.data[j]));
        bool restored=std::abs(mb->getValue()-.61f)<1e-5f&&b->getCurrentProgram()==i;
        bool ok=r.finite&&s.finite&&r.rms>1e-7&&r.peak<=1.00001f&&maxError<1e-4f&&restored;
        auto name=std::to_string(i+1);name.insert(0,3-name.size(),'0');
        ok=wav(std::filesystem::path("cosmos-preset-audio")/(name+".wav"),r)&&ok;
        report<<i+1<<",\""<<a->getProgramName(i).toStdString()<<"\","<<r.rms<<','<<r.peak<<','<<r.finite<<','<<maxError<<','<<restored<<','<<ok<<'\n';
        if(!ok){++failed;std::cerr<<"Preset "<<i+1<<" failed\n";}
    }
    std::cout<<270-failed<<"/270 presets passed automated DSP/state checks\n";
    auto findParameter=[](juce::AudioProcessor& p,const juce::String& key)->juce::AudioProcessorParameter*{for(auto* x:p.getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(x))if(id->paramID==key)return x;return nullptr;};
    bool matrixRestored=true;a->setCurrentProgram(0);b->setCurrentProgram(0);
    const std::array<std::pair<const char*,float>,20> controls{{{"macro8",.71f},{"matrix64_enabled",1.f},{"matrix64_oscillator",.4f},{"matrix64_curve",1.f},{"matrix64_smoothing",.35f},{"modenv12_freeEnabled",1.f},{"modenv12_freeCount",.5f},{"modenv12_freeY2",.73f},{"modenv12_freeCurve1",1.f},{"matrix64_point2",.83f},{"lfo8_phase",.4f},{"modenv12_freeX2",.6f},{"nexusEnabled",1.f},{"nexusX",.3f},{"nexusY",.8f},{"nexusMacroX",.5f},{"nexusCCY",.5f},{"morphEnabled",1.f},{"morphAmount",.4f},{"morphCurve",.5f}}};
    for(const auto& c:controls){auto* par=findParameter(*a,c.first);if(!par){matrixRestored=false;continue;}par->beginChangeGesture();par->setValueNotifyingHost(c.second);par->endChangeGesture();}
    juce::MemoryBlock matrixState;a->getStateInformation(matrixState);b->setStateInformation(matrixState.getData(),int(matrixState.getSize()));
    for(const auto& c:controls){auto* x=findParameter(*a,c.first);auto* y=findParameter(*b,c.first);matrixRestored&=x&&y&&std::abs(x->getValue()-y->getValue())<1e-6f;}
    std::cout<<"Macro and extended matrix state: "<<(matrixRestored?"passed":"FAILED")<<'\n';
    bool learned=cosmosTestMidiLearn(*a);std::cout<<"MIDI Learn processor CC: "<<(learned?"passed":"FAILED")<<'\n';
    bool qualified=matrixRestored&&learned&&qualifyProcessor(*a,*b,*ma,*mb);
    bool imported=qualifyImports();
    auto guiDir=juce::File::getCurrentWorkingDirectory().getChildFile("cosmos-ui-qa");
    bool layout=guiDir.createDirectory().wasOk()&&cosmosTestLayout(*a,guiDir);
    std::cout<<"Editor geometry and thirty-nine screenshots: "<<(layout?"passed":"FAILED")<<'\n';
    return failed||!report||!qualified||!imported||!layout?1:0;
}
