#include <juce_audio_utils/juce_audio_utils.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <memory>
#include <cstdint>
#include <algorithm>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
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
int main(){
    juce::ScopedJuceInitialiser_GUI gui;
    auto a=std::unique_ptr<juce::AudioProcessor>(createPluginFilter()),b=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto master=[](juce::AudioProcessor& p)->juce::AudioProcessorParameter*{
        for(auto* x:p.getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(x))if(id->paramID=="master")return x;
        return nullptr;
    };
    auto* ma=master(*a);auto* mb=master(*b);if(!ma||!mb||a->getNumPrograms()!=200)return 1;
    std::filesystem::create_directories("cosmos-preset-audio");
    std::ofstream report("cosmos-presets.csv");
    report<<"preset,name,rms,peak,finite,state_max_error,automation_restored,passed\n";
    int failed=0;
    for(int i=0;i<a->getNumPrograms();++i){
        a->setCurrentProgram(i);ma->beginChangeGesture();ma->setValueNotifyingHost(.61f);ma->endChangeGesture();
        juce::MemoryBlock state;a->getStateInformation(state);
        b->setCurrentProgram((i+1)%200);b->setStateInformation(state.getData(),int(state.getSize()));
        auto r=render(*a),s=render(*b);float maxError=0;
        for(size_t j=0;j<r.data.size();++j)maxError=std::max(maxError,std::abs(r.data[j]-s.data[j]));
        bool restored=std::abs(mb->getValue()-.61f)<1e-5f&&b->getCurrentProgram()==i;
        bool ok=r.finite&&s.finite&&r.rms>1e-7&&r.peak<=1.00001f&&maxError<1e-5f&&restored;
        auto name=std::to_string(i+1);name.insert(0,3-name.size(),'0');
        ok=wav(std::filesystem::path("cosmos-preset-audio")/(name+".wav"),r)&&ok;
        report<<i+1<<",\""<<a->getProgramName(i).toStdString()<<"\","<<r.rms<<','<<r.peak<<','<<r.finite<<','<<maxError<<','<<restored<<','<<ok<<'\n';
        if(!ok){++failed;std::cerr<<"Preset "<<i+1<<" failed\n";}
    }
    std::cout<<200-failed<<"/200 presets passed automated DSP/state checks\n";
    return failed||!report?1:0;
}
