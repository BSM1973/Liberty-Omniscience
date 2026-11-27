#include <juce_audio_utils/juce_audio_utils.h>
#include "CosmosEngine.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

namespace {
using State=juce::AudioProcessorValueTreeState;
using ID=juce::ParameterID;
using F=juce::AudioParameterFloat;
using B=juce::AudioParameterBool;
using C=juce::AudioParameterChoice;
const juce::StringArray filterNames={"Historique","Bypass","Passe-bas 6","Passe-haut 6","Passe-bas 12","Passe-haut 12","Passe-bande 12","Coupe-bande 12","Passe-bas 24","Passe-haut 24","Passe-bande cascade","Coupe-bande cascade","Passe-bas 36","Passe-haut 36","Passe-bas 48","Passe-haut 48","Passe-bas 18","Passe-haut 18","Ladder 24","Transistor Ladder 24","Diode Ladder 24","OTA 12","OTA 24","Analogique 12","Analogique HP 24","Analogique HP 12","Comb rétroaction +","Comb rétroaction −","Comb direct +","Comb direct −","Formant A","Formant E","Formant I","Formant O","Formant U","All-Pass 2 pôles","All-Pass cascade","Cloche boost","Cloche cut","Shelf grave","Shelf aigu","Triple résonance","Morph LP BP HP","Morph LP Notch","Morph BP HP","Harmoniques 1 2 3","Harmoniques impaires","Phaser 4 pôles","Phaser 6 pôles","Comb amorti +","Comb amorti −","Morph vocal A I","Morph vocal O U","Morph vocal A O","Vactrol doux","Low Pass Gate rapide","Vactrol passe-bande","Asymétrique LP","Asymétrique HP","Métal cloche","Métal plaque","Métal barre","Métal membrane","Multibande serré","Multibande large","Cloche alternée serrée","Cloche alternée large","Hybride BP cloche","Hybride BP shelf","Hybride phase cloche","Hybride double shelf","Résonateur auto-oscillant"};
const std::array<const char*,9> filterModKeys={"fMorph","f2Morph","fKey","fVel","fDepth","fAttack","fDecay","fSustain","fRelease"};
const std::array<const char*,9> filterModLabels={"Morphing A","Morphing B","Suivi clavier","Vélocité","Env. (octaves)","Attaque filtre","Déclin filtre","Sustain filtre","Relâchement"};
const std::array<const char*,8> filterKeys={"cutoff","fRes","fDrive","f2Cutoff","f2Res","f2Drive","fMix","fTrim"};
const std::array<const char*,8> filterLabels={"Fréquence 1","Résonance 1","Saturation 1","Fréquence 2","Résonance 2","Saturation 2","Balance parallèle","Niveau"};
const std::array<const char*,8> keys={"gain","pan","tune","cutoff","attack","decay","sustain","release"};
const std::array<const char*,8> names={"Volume","Panoramique","Accordage","Fréquence","Attaque","Déclin","Sustain","Relâchement"};
const std::array<const char*,10> categories={"Analog","Pads","Bass","Keys","Plucks","Cinematic","Leads","Textures","Bells","Soft"};
const std::array<const char*,10> adjectives={"Dawn","Orbit","Nebula","Horizon","Nova","Eclipse","Zenith","Vortex","Halo","Astra"};
juce::String id(int k,const char* key){return juce::String::charToString(juce::juce_wchar('A'+k))+"_"+key;}
const std::array<const char*,9> oscKeys={"level","tune","unison","detune","drift","morph","ratio","depth","drive"};
const std::array<const char*,9> oscNames={"Niveau","Accordage","Unisson","Dispersion","Dérive","Morphing","Ratio","Profondeur","Saturation"};
const std::array<const char*,9> grainKeys={"gSize","gDensity","gPosition","gSpread","gStretch","gPitch","gBlur","gStart","gEnd"};
const std::array<const char*,9> grainNames={"Taille (ms)","Densité","Position","Dispersion","Stretch","Jitter (cents)","Blur","Début boucle","Fin boucle"};
const std::array<const char*,3> grainFlags={"gFreeze","gReverse","gSync"};
juce::String oid(int k,int o,const char* key){return id(k,(juce::String("osc")+juce::String(o+1)+"_"+key).toRawUTF8());}
juce::String presetName(int i){if(i>=200)return "Filtres / "+filterNames[i-198];if(i>=100)return juce::String("Granular / ")+categories[size_t((i-100)/10)]+" "+adjectives[size_t(i%10)];return juce::String(categories[size_t(i/10)])+" / "+adjectives[size_t(i%10)];}
const juce::StringArray lfoShapes={"Sinus","Triangle","Dent montante","Dent descendante","Carrée","Sample & Hold","Exponentielle","Logarithmique","S & H lissé","Aléatoire progressif","Cosinus relevé","Escalier 8"};
const juce::StringArray lfoDest={"Aucune","Fréquence filtre A","Fréquence filtre B","Amplitude","Panoramique","Accordage","Morphing Wavetable","Position granulaire","Densité granulaire"};
const juce::StringArray lfoLayers={"Toutes","Couche A","Couche B","Couche C","Couche D"};
const juce::StringArray lfoDivisions={"Libre (Hz)","1 mesure (4/4)","Blanche","Noire","Croche","Double croche","Triolet de croche","Croche pointée"};
const juce::StringArray lfoPolarities={"Bipolaire","Unipolaire"},lfoModes={"Redéclenchement","Free Run","Transport"};
const std::array<const char*,9> lfoKeys={"shape","destination","layer","division","rate","depth","phase","polarity","mode"};
juce::String lid(int i,const char* k){return "lfo"+juce::String(i+1)+"_"+k;}
const juce::StringArray envModes={"ADSR","AHDSR","Multisegment 5"},envPlay={"Maintien","Boucle","One Shot"},envCurves={"Linéaire","Exponentielle","Logarithmique"},envSync={"Secondes","Temps (BPM)"};
const std::array<const char*,15> envKeys={"mode","play","curve","sync","destination","layer","attack","hold","decay","sustain","release","segmentTime","segmentLevel","depth","velocity"};
juce::String eid(int i,const char* k){return "modenv"+juce::String(i+1)+"_"+k;}
State::ParameterLayout layout(){
  State::ParameterLayout p;
  p.add(std::make_unique<F>(ID{"master",1},"Volume général",0.f,1.f,.7f));
  for(int k=0;k<4;++k){
    auto add=[&](const char* key,const char* name,float lo,float hi,float def,float skew=1.f){
      p.add(std::make_unique<F>(ID{id(k,key),1},id(k,name),juce::NormalisableRange<float>(lo,hi,0.f,skew),def));
    };
    p.add(std::make_unique<B>(ID{id(k,"enabled"),1},id(k,"Actif"),true));
    p.add(std::make_unique<C>(ID{id(k,"wave"),1},id(k,"Onde"),juce::StringArray{"Sinus","Dent de scie","Carrée","Triangle"},k%4));
    add("gain","Volume",0,1,k==0?.5f:.17f);add("pan","Panoramique",-1,1,k==1?-.35f:k==2?.35f:0);
    add("tune","Accordage",-24,24,k==2?12.f:0.f);
    add("cutoff","Fréquence",20,20000,8000,.3f);
    p.add(std::make_unique<C>(ID{id(k,"fModel"),1},id(k,"Filtre 1"),filterNames,0));
    p.add(std::make_unique<C>(ID{id(k,"f2Model"),1},id(k,"Filtre 2"),filterNames,1));
    p.add(std::make_unique<C>(ID{id(k,"fRoute"),1},id(k,"Routage"),juce::StringArray{"Série","Parallèle"},0));
    add("fRes","Résonance 1",0,1,0);add("fDrive","Saturation 1",0,1,0);add("f2Cutoff","Fréquence 2",20,20000,8000,.3f);
    add("f2Res","Résonance 2",0,1,0);add("f2Drive","Saturation 2",0,1,0);add("fMix","Balance",0,1,.5f);add("fTrim","Niveau filtres",0,2,1);
    p.add(std::make_unique<B>(ID{id(k,"fAuto"),1},id(k,"Compensation automatique"),false));
    add("fMorph","Morphing A",0,1,.5f);add("f2Morph","Morphing B",0,1,.5f);add("fKey","Suivi clavier",-1,1,0);add("fVel","Vélocité filtre",-1,1,0);add("fDepth","Enveloppe filtre",-6,6,0);
    add("fAttack","Attaque filtre",.001f,8,.02f,.3f);add("fDecay","Déclin filtre",.005f,8,.3f,.3f);add("fSustain","Sustain filtre",0,1,.5f);add("fRelease","Relâchement filtre",.005f,10,.4f,.3f);
    add("attack","Attaque",.001f,8,.03f,.3f);add("decay","Déclin",.005f,8,.35f,.3f);
    add("sustain","Sustain",0,1,.75f);add("release","Relâchement",.005f,10,.5f,.3f);
    for(int o=0;o<5;++o){
      auto key=[&](const char* n){return oid(k,o,n);};
      auto of=[&](const char* n,float lo,float hi,float def,float step=0.f){p.add(std::make_unique<F>(ID{key(n),1},key(n),juce::NormalisableRange<float>(lo,hi,step),def));};
      p.add(std::make_unique<C>(ID{key("mode"),1},key("mode"),juce::StringArray{"Analogique","Wavetable HD","FM / PM","AM","Sample","Granulaire"},0));
      if(o!=0)p.add(std::make_unique<C>(ID{key("wave"),1},key("wave"),juce::StringArray{"Sinus","Dent de scie","Carrée","Triangle"},1));
      of("level",0,1,o==0?1.f:0.f);of("tune",-24,24,0);of("unison",1,16,1,1);of("detune",0,100,0);of("drift",0,35,0);of("morph",0,1,0);of("ratio",.125f,16,2);of("depth",0,1,0);of("drive",0,1,0);
      p.add(std::make_unique<C>(ID{key("tableSource"),1},key("tableSource"),juce::StringArray{"Historique 16","Factory HD 256","WAV personnel"},0));
      of("gSize",5,500,100);of("gDensity",1,80,20);of("gPosition",0,1,.5f);of("gSpread",0,1,.12f);of("gStretch",.125f,8,1);of("gPitch",0,1200,0);of("gBlur",0,1,0);of("gStart",0,1,0);of("gEnd",0,1,1);
      for(auto flag:grainFlags)p.add(std::make_unique<B>(ID{key(flag),1},key(flag),false));
      p.add(std::make_unique<C>(ID{key("gWindow"),1},key("gWindow"),juce::StringArray{"Hann","Triangle","Trapèze"},0));
      p.add(std::make_unique<C>(ID{key("gSource"),1},key("gSource"),juce::StringArray{"Sample personnel","Cosmic Analog","Metal Resonance","Nebula Noise","Crystal Bell"},0));
      p.add(std::make_unique<B>(ID{key("sync"),1},key("sync"),false));p.add(std::make_unique<B>(ID{key("ring"),1},key("ring"),false));
    }
  }
  for(int i=0;i<cosmos::lfoCount;++i){
    auto addChoice=[&](const char* key,const juce::StringArray& choices){p.add(std::make_unique<C>(ID{lid(i,key),1},lid(i,key),choices,0));};
    addChoice("shape",lfoShapes);addChoice("destination",lfoDest);addChoice("layer",lfoLayers);addChoice("division",lfoDivisions);addChoice("polarity",lfoPolarities);addChoice("mode",lfoModes);
    p.add(std::make_unique<F>(ID{lid(i,"rate"),1},lid(i,"rate"),juce::NormalisableRange<float>(.01f,40.f,0.f,.3f),1.f));
    p.add(std::make_unique<F>(ID{lid(i,"depth"),1},lid(i,"depth"),-1.f,1.f,0.f));
    p.add(std::make_unique<F>(ID{lid(i,"phase"),1},lid(i,"phase"),0.f,1.f,0.f));
  }
  for(int i=0;i<cosmos::modEnvCount;++i){
    auto choice=[&](const char* key,const juce::StringArray& values){p.add(std::make_unique<C>(ID{eid(i,key),1},eid(i,key),values,0));};
    choice("mode",envModes);choice("play",envPlay);choice("curve",envCurves);choice("sync",envSync);choice("destination",lfoDest);choice("layer",lfoLayers);
    auto add=[&](const char* key,float lo,float hi,float def,float skew=1.f){p.add(std::make_unique<F>(ID{eid(i,key),1},eid(i,key),juce::NormalisableRange<float>(lo,hi,0.f,skew),def));};
    add("attack",.001f,8,.05f,.3f);add("hold",0,8,.1f,.3f);add("decay",.001f,8,.3f,.3f);add("sustain",0,1,.5f);add("release",.001f,10,.4f,.3f);
    add("segmentTime",.001f,8,.2f,.3f);add("segmentLevel",0,1,.7f);add("depth",-1,1,0);add("velocity",0,1,0);
  }
  return p;
}
class Processor final:public juce::AudioProcessor {
public:
  State state{*this,nullptr,"OMNISCIENCE",layout()};
  cosmos::Engine engine;
  std::array<std::array<std::atomic<float>*,10>,4> params{};
  std::atomic<float>* master=nullptr;
  std::array<std::array<std::atomic<float>*,9>,cosmos::lfoCount> lfoParams{};
  std::array<std::array<std::atomic<float>*,15>,cosmos::modEnvCount> modEnvParams{};
  std::array<std::array<std::atomic<float>*,11>,4> filterParams{};
  std::array<std::array<std::atomic<float>*,9>,4> filterModParams{};
  std::atomic<int> program{-1};
  std::array<std::array<std::array<std::atomic<float>*,13>,5>,4> oscillators{};
  std::array<std::atomic<const cosmos::SampleData*>,4> samples{};
  std::array<std::array<std::array<std::atomic<float>*,14>,5>,4> grainParams{};
  std::array<std::array<std::array<std::atomic<float>,16>,5>,4> grainView{};
  std::array<std::array<std::atomic<float>*,5>,4> tableParams{};
  std::array<std::atomic<const cosmos::HDTable*>,4> tables{};
  std::vector<std::unique_ptr<cosmos::HDTable>> retainedTables;
  std::array<juce::String,4> tablePaths{};
  std::vector<juce::String> retainedTablePaths,retainedSamplePaths;
  std::vector<juce::int64> retainedTableSizes,retainedSampleSizes;
  std::vector<juce::Time> retainedTableTimes,retainedSampleTimes;
  juce::String loadTable(int k,const juce::File& file){
    std::lock_guard<std::mutex> lock(sampleMutex);
    if(k<0||k>=4)return "Couche invalide.";
    if(!file.existsAsFile())return "Fichier absent.";
    const auto path=file.getFullPathName();const auto bytes=file.getSize();const auto modified=file.getLastModificationTime();
    for(size_t i=0;i<retainedTables.size();++i)if(retainedTablePaths[i]==path&&retainedTableSizes[i]==bytes&&retainedTableTimes[i]==modified){tablePaths[k]=path;tables[k].store(retainedTables[i].get(),std::memory_order_release);return {};}
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if(!reader||reader->lengthInSamples<cosmos::tableSize||reader->lengthInSamples%cosmos::tableSize||reader->lengthInSamples>cosmos::hdFrames*cosmos::tableSize)
      return "WAV requis : 1 à 256 cycles de 2048 échantillons.";
    if(retainedTables.size()>=8)return "Limite de huit tables importées par session.";
    int n=int(reader->lengthInSamples);juce::AudioBuffer<float> buffer(2,n);
    if(!reader->read(&buffer,0,n,0,true,true))return "Lecture impossible.";
    std::vector<float> input(static_cast<size_t>(n));
    for(int i=0;i<n;++i)input[size_t(i)]=reader->numChannels>1?.5f*(buffer.getSample(0,i)+buffer.getSample(1,i)):buffer.getSample(0,i);
    auto table=std::make_unique<cosmos::HDTable>(input);auto* ptr=table.get();
    retainedTablePaths.push_back(path);retainedTableSizes.push_back(bytes);retainedTableTimes.push_back(modified);retainedTables.push_back(std::move(table));tablePaths[k]=path;tables[k].store(ptr,std::memory_order_release);return {};
  }
  juce::String tableName(int k)const{
    std::lock_guard<std::mutex> lock(sampleMutex);
    return tablePaths[k].isEmpty()?"Aucune table personnelle":juce::File(tablePaths[k]).getFileName()+(tables[k].load()?"":" (absente)");
  }
  juce::AudioFormatManager formats;
  mutable std::mutex sampleMutex;
  std::vector<std::unique_ptr<cosmos::SampleData>> retainedSamples;
  std::array<juce::String,4> samplePaths{};
  size_t retainedBytes=0;
  juce::String loadSample(int layer,const juce::File& file){
    std::lock_guard<std::mutex> lock(sampleMutex);
    if(layer<0||layer>=4)return "Couche invalide.";
    if(!file.existsAsFile())return "Fichier absent.";
    const auto path=file.getFullPathName();const auto bytes=file.getSize();const auto modified=file.getLastModificationTime();
    for(size_t i=0;i<retainedSamples.size();++i)if(retainedSamplePaths[i]==path&&retainedSampleSizes[i]==bytes&&retainedSampleTimes[i]==modified){samplePaths[layer]=path;samples[layer].store(retainedSamples[i].get(),std::memory_order_release);return {};}
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if(!reader || reader->lengthInSamples<=0 || reader->lengthInSamples>5760000 || reader->sampleRate<=0 || reader->lengthInSamples>reader->sampleRate*30)
      return "Fichier incompatible ou supérieur à 30 secondes.";
    auto n=int(reader->lengthInSamples);
    if(retainedBytes+size_t(n)*sizeof(float)>64u*1024u*1024u)return "Limite mémoire de session atteinte (64 Mio).";
    juce::AudioBuffer<float> buffer(2,n);
    if(!reader->read(&buffer,0,n,0,true,true))return "Lecture impossible.";
    auto s=std::make_unique<cosmos::SampleData>();s->rate=reader->sampleRate;s->mono.resize(size_t(n));
    for(int i=0;i<n;++i){
      float x=buffer.getSample(0,i);if(reader->numChannels>1)x=.5f*(x+buffer.getSample(1,i));
      s->mono[size_t(i)]=std::isfinite(x)?std::clamp(x,-1.f,1.f):0.f;
    }
    auto ptr=s.get();retainedSamplePaths.push_back(path);retainedSampleSizes.push_back(bytes);retainedSampleTimes.push_back(modified);retainedSamples.push_back(std::move(s));retainedBytes+=size_t(n)*sizeof(float);
    samplePaths[layer]=file.getFullPathName();samples[layer].store(ptr,std::memory_order_release);
    return {};
  }
  juce::String sampleName(int layer) const {
    std::lock_guard<std::mutex> lock(sampleMutex);
    if(samplePaths[layer].isEmpty())return "Aucun sample";
    return juce::File(samplePaths[layer]).getFileName()+(samples[layer].load()?"":" (absent)");
  }
  void resetOscillators(){
    for(int k=0;k<4;++k)for(int o=0;o<5;++o){
      auto reset=[&](const char* key){if(auto* p=state.getParameter(oid(k,o,key)))p->setValueNotifyingHost(p->getDefaultValue());};
      reset("mode");if(o!=0)reset("wave");for(auto key:oscKeys)reset(key);reset("sync");reset("ring");for(auto key:grainKeys)reset(key);for(auto key:grainFlags)reset(key);reset("gWindow");reset("gSource");reset("tableSource");
    }
  }
  Processor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)){
    formats.registerBasicFormats();
    master=state.getRawParameterValue("master");
    for(int k=0;k<4;++k)for(int o=0;o<5;++o){
      auto& a=oscillators[k][o];a[0]=state.getRawParameterValue(oid(k,o,"mode"));a[1]=state.getRawParameterValue(o==0?id(k,"wave"):oid(k,o,"wave"));
      for(int j=0;j<9;++j)a[j+2]=state.getRawParameterValue(oid(k,o,oscKeys[j]));
      a[11]=state.getRawParameterValue(oid(k,o,"sync"));a[12]=state.getRawParameterValue(oid(k,o,"ring"));
      tableParams[k][o]=state.getRawParameterValue(oid(k,o,"tableSource"));
      auto& g=grainParams[k][o];for(int j=0;j<9;++j)g[j]=state.getRawParameterValue(oid(k,o,grainKeys[j]));
      for(int j=0;j<3;++j)g[9+j]=state.getRawParameterValue(oid(k,o,grainFlags[j]));
      g[12]=state.getRawParameterValue(oid(k,o,"gWindow"));g[13]=state.getRawParameterValue(oid(k,o,"gSource"));
      for(auto& x:grainView[k][o])x.store(-1.f);
    }
    for(int k=0;k<4;++k){params[k][0]=state.getRawParameterValue(id(k,"enabled"));params[k][1]=state.getRawParameterValue(id(k,"wave"));for(int j=0;j<8;++j)params[k][j+2]=state.getRawParameterValue(id(k,keys[j]));}
    for(int k=0;k<4;++k){const char* fk[]={"fModel","f2Model","fRoute","fRes","fDrive","f2Cutoff","f2Res","f2Drive","fMix","fTrim","fAuto"};for(int j=0;j<11;++j)filterParams[k][j]=state.getRawParameterValue(id(k,fk[j]));}
    for(int k=0;k<4;++k)for(int j=0;j<9;++j)filterModParams[k][j]=state.getRawParameterValue(id(k,filterModKeys[j]));
    for(int i=0;i<cosmos::lfoCount;++i)for(int j=0;j<9;++j)lfoParams[i][j]=state.getRawParameterValue(lid(i,lfoKeys[j]));
    for(int i=0;i<cosmos::modEnvCount;++i)for(int j=0;j<15;++j)modEnvParams[i][j]=state.getRawParameterValue(eid(i,envKeys[j]));
    setCurrentProgram(0);
  }
  const juce::String getName() const override{return "Omniscience";}
  bool acceptsMidi()const override{return true;} bool producesMidi()const override{return false;} bool isMidiEffect()const override{return false;}
  double getTailLengthSeconds()const override{return 10;}
  bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
  void prepareToPlay(double sr,int)override{engine.prepare(sr);}
  void releaseResources()override{engine.reset();}
  bool referenceDSPForQA=false;
  cosmos::Patch patch() const {
    cosmos::Patch p;p.master=master->load();
    for(int i=0;i<cosmos::lfoCount;++i){const auto& x=lfoParams[i];p.lfo[i]={int(x[0]->load()),int(x[1]->load()),int(x[2]->load()),int(x[3]->load()),x[4]->load(),x[5]->load(),x[6]->load(),int(x[7]->load()),int(x[8]->load())};}
    for(int i=0;i<cosmos::modEnvCount;++i){const auto& x=modEnvParams[i];p.modEnv[i]={int(x[0]->load()),int(x[1]->load()),int(x[2]->load()),int(x[3]->load()),int(x[4]->load()),int(x[5]->load()),x[6]->load(),x[7]->load(),x[8]->load(),x[9]->load(),x[10]->load(),x[11]->load(),x[12]->load(),x[13]->load(),x[14]->load()};}
    p.experimentalGrouped=true;p.experimentalTiled=true;p.experimentalInterpolation=1;
#if defined(_M_X64)
    p.experimentalInterpolation=2;
#endif
#if defined(__aarch64__)
    p.experimentalGrainWindow=true;p.experimentalGrainPrepared=true;p.experimentalAnalog=true;p.experimentalEnv=true;
#endif
    if(referenceDSPForQA){p.experimentalGrouped=false;p.experimentalTiled=false;p.experimentalInterpolation=0;p.experimentalGrainWindow=false;p.experimentalGrainPrepared=false;p.experimentalAnalog=false;p.experimentalEnv=false;}
    for(int k=0;k<4;++k){auto& l=p.layer[k];auto& a=params[k];l.enabled=a[0]->load()>.5f;l.wave=int(a[1]->load());l.gain=a[2]->load();l.pan=a[3]->load();l.tune=a[4]->load();l.cutoff=a[5]->load();l.attack=a[6]->load();l.decay=a[7]->load();l.sustain=a[8]->load();l.release=a[9]->load();l.sample=samples[k].load(std::memory_order_acquire);
      const auto& f=filterParams[k];l.filter1={int(f[0]->load()),l.cutoff,f[3]->load(),f[4]->load()};l.filter2={int(f[1]->load()),f[5]->load(),f[6]->load(),f[7]->load()};l.filterParallel=f[2]->load()>.5f;l.filterMix=f[8]->load();l.filterTrim=f[9]->load();l.filter1.autoGain=l.filter2.autoGain=f[10]->load()>.5f;
      const auto& m=filterModParams[k];l.filter1.morph=m[0]->load();l.filter2.morph=m[1]->load();l.filterKey=m[2]->load();l.filterVelocity=m[3]->load();l.filterDepth=m[4]->load();l.filterAttack=m[5]->load();l.filterDecay=m[6]->load();l.filterSustain=m[7]->load();l.filterRelease=m[8]->load();
      for(int o=0;o<5;++o){
        const auto& q=oscillators[k][o];auto& v=l.osc[o];v.mode=cosmos::OscMode(int(q[0]->load()));v.wave=int(q[1]->load());v.level=q[2]->load();v.tune=q[3]->load();v.unison=int(q[4]->load());v.detune=q[5]->load();v.drift=q[6]->load();v.morph=q[7]->load();v.ratio=q[8]->load();v.depth=q[9]->load();v.drive=q[10]->load();v.sync=q[11]->load()>.5f;v.ring=q[12]->load()>.5f;
        v.tableSource=int(tableParams[k][o]->load());v.userTable=tables[k].load(std::memory_order_acquire);
        const auto& g=grainParams[k][o];auto& z=v.grain;
        z.size=g[0]->load();z.density=g[1]->load();z.position=g[2]->load();z.spread=g[3]->load();z.stretch=g[4]->load();z.pitchJitter=g[5]->load();z.blur=g[6]->load();z.loopStart=g[7]->load();z.loopEnd=g[8]->load();
        z.freeze=g[9]->load()>.5f;z.reverse=g[10]->load()>.5f;z.sync=g[11]->load()>.5f;z.window=int(g[12]->load());z.source=juce::jlimit(0,4,int(g[13]->load()));
      }
    }
    return p;
  }
  void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi)override{
    juce::ScopedNoDenormals nd;b.clear();if(b.getNumChannels()<2)return;
    auto p=patch();
    if(auto* ph=getPlayHead())if(auto position=ph->getPosition()){if(auto bpm=position->getBpm())p.bpm=*bpm;if(auto ppq=position->getPpqPosition()){p.ppq=*ppq;p.transportValid=position->getIsPlaying()&&std::isfinite(*ppq);}}
    auto* l=b.getWritePointer(0);auto* r=b.getWritePointer(1);int pos=0,n=b.getNumSamples();
    for(const auto meta:midi){
      int at=juce::jlimit(pos,n,meta.samplePosition);engine.render(l+pos,r+pos,at-pos,p);if(p.transportValid)p.ppq+=(at-pos)*std::clamp(p.bpm,20.,400.)/(60*std::max(8000.,getSampleRate()));pos=at;
      auto m=meta.getMessage();int ch=m.getChannel()-1;
      if(m.isNoteOn())engine.noteOn(ch,m.getNoteNumber(),m.getFloatVelocity());
      else if(m.isNoteOff())engine.noteOff(ch,m.getNoteNumber());
      else if(m.isPitchWheel())engine.pitch(ch,m.getPitchWheelValue());
      else if(m.isAllSoundOff())engine.allOff(ch,true);
      else if(m.isAllNotesOff())engine.allOff(ch,false);
      else if(m.isController()&&m.getControllerNumber()==64)engine.sustain(ch,m.getControllerValue()>=64);
    }
    engine.render(l+pos,r+pos,n-pos,p);
    for(int i=0;i<n;++i){l[i]=std::tanh(l[i]);r[i]=std::tanh(r[i]);}
    for(int k=0;k<4;++k)for(int o=0;o<5;++o){
      const auto& z=p.layer[k].osc[o];
      std::array<float,16> positions;positions.fill(-1);
      if(z.mode==cosmos::OscMode::granular&&z.level>0&&p.layer[k].enabled)
        positions=engine.grainPositions(k,o,z.grain.source?&cosmos::factoryGrains()[size_t(z.grain.source-1)]:p.layer[k].sample);
      for(size_t i=0;i<positions.size();++i)grainView[k][o][i].store(positions[i],std::memory_order_relaxed);
    }
    midi.clear();
  }
  int getNumPrograms()override{return 270;} int getCurrentProgram()override{return juce::jmax(0,program.load());}
  const juce::String getProgramName(int i)override{return presetName(juce::jlimit(0,269,i));}
  void changeProgramName(int,const juce::String&)override{}
  void setCurrentProgram(int i)override{
    if(i<0||i>=270)return;
    resetOscillators();
    for(int j=0;j<cosmos::lfoCount;++j)for(auto key:lfoKeys)if(auto* par=state.getParameter(lid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
    for(int j=0;j<cosmos::modEnvCount;++j)for(auto key:envKeys)if(auto* par=state.getParameter(eid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
    auto set=[&](const juce::String& key,float v){if(auto* p=state.getParameter(key))p->setValueNotifyingHost(p->convertTo0to1(v));};
    int cat=(i%100)/10,var=i%10;
    const float attacks[]={.015f,.75f,.006f,.008f,.004f,.5f,.025f,.9f,.003f,.12f};
    const float decays[]={.4f,1.4f,.22f,.7f,.18f,1.7f,.5f,2.f,1.1f,.65f};
    const float sustains[]={.72f,.85f,.7f,.45f,.12f,.78f,.65f,.65f,.08f,.7f};
    const float releases[]={.35f,2.1f,.12f,.45f,.25f,2.5f,.4f,3.f,1.2f,1.f};
    const float cutoffs[]={6500,3300,950,5500,7200,4200,11000,2400,13000,1900};
    set("master",.7f);
    for(int k=0;k<4;++k){
      set(id(k,"fModel"),0);set(id(k,"f2Model"),1);set(id(k,"fRoute"),0);set(id(k,"fRes"),0);set(id(k,"fDrive"),0);set(id(k,"f2Cutoff"),8000);set(id(k,"f2Res"),0);set(id(k,"f2Drive"),0);set(id(k,"fMix"),.5f);set(id(k,"fTrim"),1);set(id(k,"fAuto"),0);for(auto key:filterModKeys)if(auto* par=state.getParameter(id(k,key)))par->setValueNotifyingHost(par->getDefaultValue());
      set(id(k,"enabled"),k==0||((cat==1||cat==5||cat==7)?true:k<=1+(var%3==2)));
      set(id(k,"wave"),float((cat+k+var/3)%4));
      set(id(k,"gain"),k==0?.55f:.12f+.025f*float((var+k)%5));
      set(id(k,"pan"),k==0?0.f:(k%2?-.2f:.2f)*(1+float(var%4)*.4f));
      set(id(k,"tune"),k==0?0.f:k==1?(float(var)-4.f)*.035f:k==2?(cat==2?-12.f:12.f):float(var%2?7:0));
      set(id(k,"cutoff"),cutoffs[cat]*(.65f+.09f*var)*(1.f+.12f*k));
      set(id(k,"attack"),attacks[cat]*(1+.16f*k+.09f*var));
      set(id(k,"decay"),decays[cat]*(.8f+.1f*var));set(id(k,"sustain"),sustains[cat]);
      set(id(k,"release"),releases[cat]*(.75f+.1f*var+.1f*k));
    }
    if(i>=100&&i<200){
      set(id(0,"attack"),.05f+.03f*var);set(id(0,"release"),.5f+.15f*cat);
      for(int k=0;k<2;++k){
        set(id(k,"enabled"),1);set(oid(k,0,"mode"),5);set(oid(k,0,"gSource"),1+(cat+k)%4);
        set(oid(k,0,"gSize"),35+cat*35+var*3);set(oid(k,0,"gDensity"),8+var*5);
        set(oid(k,0,"gPosition"),.08f+.085f*var);set(oid(k,0,"gStretch"),.5f+.3f*cat);
        set(oid(k,0,"gSpread"),.04f+.07f*cat);set(oid(k,0,"gPitch"),var*8);set(oid(k,0,"gBlur"),float(cat%4)*.2f);
        set(oid(k,0,"gFreeze"),var%3==0);set(oid(k,0,"gReverse"),cat%3==2);set(oid(k,0,"gWindow"),cat%3);
      }
      set(oid(0,1,"level"),.12f);set(oid(0,1,"mode"),1);set(oid(0,1,"morph"),var/9.f);
    }
    if(i>=200){for(int k=0;k<4;++k){set(id(k,"enabled"),k==0);set(id(k,"wave"),1);set(id(k,"fModel"),i-198);set(id(k,"cutoff"),1200);set(id(k,"fRes"),i>=216?.72f:.25f);if(i>=216&&i<224){set(id(k,"fDrive"),.28f);set(id(k,"fAuto"),1);}if(i>=224){set(id(k,"cutoff"),i<228?330.f:1200.f);set(id(k,"fRes"),.55f);set(id(k,"fDrive"),0);set(id(k,"gain"),k==0?.24f:.1f);}}}
    if(i>=240&&i<252){set(id(0,"fMorph"),.35f+.05f*(i-240));set(id(0,"fKey"),.35f);set(id(0,"fVel"),.2f);set(id(0,"fDepth"),1.2f);}
    if(i>=252){set(id(0,"fMorph"),.2f+.15f*((i-252)%5));set(id(0,"fRes"),i==269?.99f:.45f+.1f*((i-252)%4));set(id(0,"cutoff"),i==269?330.f:450.f+150.f*((i-252)%7));set(id(0,"fDrive"),i==255||i==256?.22f:0.f);set(id(0,"gain"),.35f);}
    program.store(i);
  }
  void getStateInformation(juce::MemoryBlock& dest)override{
    auto tree=state.copyState();tree.setProperty("program",program.load(),nullptr);tree.setProperty("schema",10,nullptr);
    {std::lock_guard<std::mutex> lock(sampleMutex);for(int k=0;k<4;++k){tree.setProperty(juce::Identifier("sample"+juce::String(k)),samplePaths[k],nullptr);tree.setProperty(juce::Identifier("table"+juce::String(k)),tablePaths[k],nullptr);}}
    if(auto xml=tree.createXml())copyXmlToBinary(*xml,dest);
  }
  void setStateInformation(const void* data,int size)override{
    if(auto xml=getXmlFromBinary(data,size)){auto t=juce::ValueTree::fromXml(*xml);if(t.hasType(state.state.getType())){
      if(int(t.getProperty("schema",1))<2)resetOscillators();
      if(int(t.getProperty("schema",1))<3)for(int k=0;k<4;++k)for(int o=0;o<5;++o){
        auto reset=[&](const char* key){if(auto* par=state.getParameter(oid(k,o,key)))par->setValueNotifyingHost(par->getDefaultValue());};
        for(auto key:grainKeys)reset(key);for(auto key:grainFlags)reset(key);reset("gWindow");reset("gSource");
      }
      if(int(t.getProperty("schema",1))<4)for(int k=0;k<4;++k)for(int o=0;o<5;++o)if(auto* par=state.getParameter(oid(k,o,"tableSource")))par->setValueNotifyingHost(0.f);
      program.store(juce::jlimit(0,269,int(t.getProperty("program",0))));state.replaceState(t);
      if(int(t.getProperty("schema",1))<10)for(int j=0;j<cosmos::modEnvCount;++j)for(auto key:envKeys)if(auto* par=state.getParameter(eid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
      if(int(t.getProperty("schema",1))<9)for(int j=0;j<cosmos::lfoCount;++j)for(auto key:{"polarity","mode"})if(auto* par=state.getParameter(lid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
      if(int(t.getProperty("schema",1))<8)for(int j=0;j<cosmos::lfoCount;++j)for(auto key:lfoKeys)if(auto* par=state.getParameter(lid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
      if(int(t.getProperty("schema",1))<7)for(int k=0;k<4;++k)for(auto key:filterModKeys)if(auto* par=state.getParameter(id(k,key)))par->setValueNotifyingHost(par->getDefaultValue());
      if(int(t.getProperty("schema",1))<6)for(int k=0;k<4;++k)if(auto* par=state.getParameter(id(k,"fAuto")))par->setValueNotifyingHost(0.f);
      if(int(t.getProperty("schema",1))<5)for(int k=0;k<4;++k){const char* fk[]={"fModel","f2Model","fRoute","fRes","fDrive","f2Cutoff","f2Res","f2Drive","fMix","fTrim"};for(auto key:fk)if(auto* par=state.getParameter(id(k,key)))par->setValueNotifyingHost(par->getDefaultValue());}
      for(int k=0;k<4;++k){
        auto path=t.getProperty(juce::Identifier("sample"+juce::String(k))).toString();
        {std::lock_guard<std::mutex> lock(sampleMutex);samples[k].store(nullptr);samplePaths[k]=path;}
        if(path.isNotEmpty())loadSample(k,juce::File(path));
        auto tab=t.getProperty(juce::Identifier("table"+juce::String(k))).toString();
        {std::lock_guard<std::mutex> lock(sampleMutex);tables[k].store(nullptr);tablePaths[k]=tab;}
        if(tab.isNotEmpty())loadTable(k,juce::File(tab));
      }
    }}
  }
  bool hasEditor()const override{return true;} juce::AudioProcessorEditor* createEditor()override;
};
const auto cyan=juce::Colour(0xff64cbe9), ink=juce::Colour(0xff101722), panel=juce::Colour(0xff202b3a), light=juce::Colour(0xffd9e7f3);
class Theme final:public juce::LookAndFeel_V4{
public:
  Theme(){setColour(juce::Slider::textBoxTextColourId,light);setColour(juce::Slider::textBoxBackgroundColourId,ink);setColour(juce::Slider::textBoxOutlineColourId,panel);setColour(juce::TextButton::buttonColourId,panel);setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff326181));setColour(juce::TextButton::textColourOffId,light);setColour(juce::TextButton::textColourOnId,juce::Colours::white);setColour(juce::ComboBox::backgroundColourId,panel);setColour(juce::ComboBox::textColourId,light);setColour(juce::ToggleButton::textColourId,light);setColour(juce::ListBox::backgroundColourId,ink);}
  void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&)override{
    auto rect=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(12);
    float r=std::min(rect.getWidth(),rect.getHeight())*.43f;auto c=rect.getCentre();auto knob=juce::Rectangle<float>(2*r,2*r).withCentre(c);
    g.setColour(juce::Colour(0xff0b1119));g.fillEllipse(knob.expanded(5));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff53677f),c.x,c.y-r,juce::Colour(0xff192331),c.x,c.y+r,false));g.fillEllipse(knob);
    g.setColour(juce::Colour(0xff72859b));g.drawEllipse(knob,1.f);
    juce::Path arc;arc.addCentredArc(c.x,c.y,r+5,r+5,0,start,start+pos*(end-start),true);juce::Path track;track.addCentredArc(c.x,c.y,r+5,r+5,0,start,end,true);g.setColour(juce::Colour(0xff34485d));g.strokePath(track,juce::PathStrokeType(2.5f));g.setColour(cyan.withAlpha(.18f));g.strokePath(arc,juce::PathStrokeType(6.f));g.setColour(cyan);g.strokePath(arc,juce::PathStrokeType(2.5f));
    float a=start+pos*(end-start);g.drawLine(c.x+std::sin(a)*r*.45f,c.y-std::cos(a)*r*.45f,c.x+std::sin(a)*r*.83f,c.y-std::cos(a)*r*.83f,2.5f);
  }
};
class WavePreview final:public juce::Component{
  cosmos::OscParams osc;const cosmos::SampleData* sample=nullptr;std::array<float,16> grains{};int dragging=0;
public:
  std::function<void(int,float,bool,bool)> onLoopEdit;
  void mouseDown(const juce::MouseEvent& e)override{
    if(osc.mode!=cosmos::OscMode::granular||!sample||!onLoopEdit)return;
    float q=juce::jlimit(0.f,1.f,float(e.x)/std::max(1,getWidth()-1));
    dragging=std::abs(q-osc.grain.loopStart)<=std::abs(q-osc.grain.loopEnd)?1:2;
    onLoopEdit(dragging,q,true,false);
  }
  void mouseDrag(const juce::MouseEvent& e)override{if(dragging&&onLoopEdit)onLoopEdit(dragging,juce::jlimit(0.f,1.f,float(e.x)/std::max(1,getWidth()-1)),false,false);}
  void mouseUp(const juce::MouseEvent& e)override{if(dragging&&onLoopEdit)onLoopEdit(dragging,juce::jlimit(0.f,1.f,float(e.x)/std::max(1,getWidth()-1)),false,true);dragging=0;}
  void update(cosmos::OscParams o,const cosmos::SampleData* s,std::array<float,16> positions){osc=o;sample=s;grains=positions;repaint();}
  void paint(juce::Graphics& g)override{
    g.fillAll(ink);g.setColour(juce::Colour(0xff35495e));g.drawRect(getLocalBounds());
    const float mid=getHeight()*.5f;g.drawHorizontalLine(int(mid),0,float(getWidth()));
    juce::Path path;
    for(int i=0;i<getWidth();++i){
      double phase=double(i)/std::max(1,getWidth()-1);float y=0;
      switch(osc.mode){
        case cosmos::OscMode::wavetable:y=cosmos::readTable(osc,phase,1./2048);break;
        case cosmos::OscMode::fm:y=float(std::sin(2*cosmos::pi*phase+6*osc.depth*std::sin(2*cosmos::pi*phase*osc.ratio)));break;
        case cosmos::OscMode::am:y=cosmos::oscillator(osc.wave,phase,1./2048)*(1-osc.depth*.5f+osc.depth*.5f*float(std::sin(2*cosmos::pi*phase*osc.ratio)));break;
        case cosmos::OscMode::granular:
        case cosmos::OscMode::sample:y=sample?cosmos::sampleRead(sample,phase*std::max(0.,double(sample->mono.size())-1.)):0;break;
        default:y=cosmos::oscillator(osc.wave,phase,1./2048);break;
      }
      if(osc.ring)y*=float(std::sin(2*cosmos::pi*phase*osc.ratio));
      if(osc.drive>0){float d=1+12*osc.drive;y=std::tanh(d*y)/std::tanh(d);}
      float py=mid-y*mid*.85f;
      if(i==0)path.startNewSubPath(float(i),py);else path.lineTo(float(i),py);
    }
    g.setColour(cyan);g.strokePath(path,juce::PathStrokeType(1.5f));
    if(osc.mode==cosmos::OscMode::granular){
      g.setColour(juce::Colour(0xff456fa0).withAlpha(.22f));
      float lo=osc.grain.loopStart*getWidth(),hi=osc.grain.loopEnd*getWidth();
      g.fillRect(lo,0.f,std::max(0.f,hi-lo),float(getHeight()));
      g.setColour(juce::Colour(0xfff0bc75));g.drawVerticalLine(int(lo),0,float(getHeight()));g.drawVerticalLine(std::min(getWidth()-1,int(hi)),0,float(getHeight()));
      g.fillRect(int(lo),0,7,12);g.fillRect(std::max(0,int(hi)-7),0,7,12);
      g.setColour(juce::Colour(0xff80e7be));for(float x:grains)if(x>=0&&x<=1)g.drawVerticalLine(int(x*(getWidth()-1)),0,float(getHeight()));
    }
  }
};
class FilterResponse final:public juce::Component{
    std::array<std::array<float,96>,2> curves{};
    std::array<cosmos::FilterParams,2> prior{};double rate=0;
    int dragging=-1;
    float frequencyAt(int x)const{double q=std::clamp((x-12.)/std::max(1.,getWidth()-24.),0.,1.);return float(20*std::pow(std::min(20000.,std::max(8000.,rate)*.45)/20.,q));}
public:
    std::function<void(int,float,bool,bool)> onCutoff;
    void mouseDown(const juce::MouseEvent& e)override{dragging=e.mods.isShiftDown()?1:0;if(onCutoff)onCutoff(dragging,frequencyAt(e.x),true,false);}
    void mouseDrag(const juce::MouseEvent& e)override{if(dragging>=0&&onCutoff)onCutoff(dragging,frequencyAt(e.x),false,false);}
    void mouseUp(const juce::MouseEvent& e)override{if(dragging>=0&&onCutoff)onCutoff(dragging,frequencyAt(e.x),false,true);dragging=-1;}
    void update(cosmos::FilterParams a,cosmos::FilterParams b,double sr){
      sr=std::max(8000.,sr);std::array<cosmos::FilterParams,2> next{a,b};
      auto same=[](auto x,auto y){return x.model==y.model&&x.cutoff==y.cutoff&&x.resonance==y.resonance&&x.drive==y.drive&&x.autoGain==y.autoGain&&x.morph==y.morph;};
      if(rate==sr&&same(next[0],prior[0])&&same(next[1],prior[1]))return;
      rate=sr;prior=next;
      for(int k=0;k<2;++k){
        cosmos::FilterState st;cosmos::FilterPlan plan(next[k],sr);
        std::array<float,4096> impulse{};
        constexpr float amplitude=.0001f;
        for(int i=0;i<int(impulse.size());++i)impulse[i]=st.tick(i==0?amplitude:0.f,plan)/amplitude;
        for(int j=0;j<96;++j){
          double hz=20*std::pow(std::min(20000.,sr*.45)/20.,j/95.);
          double w=2*cosmos::pi*hz/sr,c=std::cos(w),s=std::sin(w),cr=1,ci=0,re=0,im=0;
          for(float x:impulse){re+=x*cr;im+=x*ci;double z=cr*c-ci*s;ci=ci*c+cr*s;cr=z;}
          curves[k][j]=float(std::clamp(20*std::log10(std::max(1e-8,std::hypot(re,im))),-60.,24.));
        }
      }
      repaint();
    }
    void paint(juce::Graphics& g)override{
      g.fillAll(juce::Colour(0xff0b131e));g.setColour(juce::Colour(0xff40566f));g.drawRect(getLocalBounds());
      g.setFont(11);g.setColour(light);g.drawText("Réponse impulsionnelle — glisser : A / Maj + glisser : B",12,2,getWidth()-24,17,juce::Justification::centredLeft);
      auto bounds=getLocalBounds().toFloat().reduced(12);bounds.removeFromTop(14);bounds.removeFromBottom(14);
      auto y=[&](float db){return bounds.getY()+(24-db)/84*bounds.getHeight();};
      for(int db:{0,-12,-24,-36,-48,-60}){g.setColour(juce::Colour(0xff2d4053));g.drawHorizontalLine(int(y(float(db))),bounds.getX(),bounds.getRight());}
      for(double hz:{20.,100.,1000.,10000.,20000.}){
        double maximum=std::min(20000.,std::max(8000.,rate)*.45);if(hz>maximum)continue;
        float x=bounds.getX()+bounds.getWidth()*float(std::log(hz/20)/std::log(maximum/20));
        g.setColour(juce::Colour(0xff304559));g.drawVerticalLine(int(x),bounds.getY(),bounds.getBottom());
        g.setColour(juce::Colour(0xff9ab6ce));g.setFont(10);g.drawText(hz>=1000?juce::String(int(hz/1000))+"k":juce::String(int(hz)),juce::jlimit(0,std::max(0,getWidth()-36),int(x)-18),int(bounds.getBottom())+1,36,12,juce::Justification::centred);
      }
      for(int k=0;k<2;++k){juce::Path path;for(int i=0;i<96;++i){float x=bounds.getX()+bounds.getWidth()*i/95.f;if(i==0)path.startNewSubPath(x,y(curves[k][i]));else path.lineTo(x,y(curves[k][i]));}g.setColour(k==0?cyan:juce::Colour(0xffe5b16d));g.strokePath(path,juce::PathStrokeType(1.7f));}
    }
};
class LfoOverview final:public juce::Component{
  std::array<cosmos::LfoParams,cosmos::lfoCount> values{};int selected=0;
public:
  void update(const std::array<cosmos::LfoParams,cosmos::lfoCount>& p,int index){values=p;selected=index;repaint();}
  void paint(juce::Graphics& g)override{
    float w=getWidth()/8.f;
    for(int k=0;k<8;++k){auto area=juce::Rectangle<float>(k*w,0,w, float(getHeight())).reduced(3);g.setColour(juce::Colour(0xff101b29));g.fillRoundedRectangle(area,5);g.setColour(k==selected?cyan:juce::Colour(0xff40566f));g.drawRoundedRectangle(area,5,1);
      auto graph=area.reduced(5);graph.removeFromTop(15);graph.removeFromBottom(4);g.setFont(11);g.drawText("LFO "+juce::String(k+1),area.toNearestInt().removeFromTop(15),juce::Justification::centred);
      juce::Path path;for(int j=0;j<=96;++j){float x=graph.getX()+j*graph.getWidth()/96;auto v=values[k];v.depth=1;float y=graph.getCentreY()-cosmos::lfoAt(v,j/96.)*graph.getHeight()*.44f;if(j==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}g.strokePath(path,juce::PathStrokeType(1.3f));}
  }
};
class ModEnvelopeView final:public juce::Component{
    cosmos::ModEnvParams p{};int handle=-1;
    std::array<juce::Point<float>,6> points{};int count=0,sustainIndex=0,segmentIndex=-1;
    void geometry(){
      count=0;segmentIndex=-1;auto b=getLocalBounds().toFloat().reduced(12);
      float total=p.attack+p.decay+p.release+(p.mode?p.hold:0)+(p.mode==2?p.segmentTime:0);
      float x=b.getX(),scale=(b.getWidth()*.82f)/std::max(.001f,total);
      auto node=[&](float dt,float level){x+=dt*scale;points[count++]={x,b.getBottom()-level*b.getHeight()};};
      node(0,0);node(p.attack,1);if(p.mode)node(p.hold,1);
      if(p.mode==2){segmentIndex=count;node(p.segmentTime,p.segmentLevel);}
      sustainIndex=count;node(p.decay,p.sustain);node(p.release,0);
    }
    float valueAt(float y)const{auto b=getLocalBounds().toFloat().reduced(12);return std::clamp((b.getBottom()-y)/std::max(1.f,b.getHeight()),0.f,1.f);}
public:
    std::function<void(int,float,bool,bool)> onEdit;
    void update(cosmos::ModEnvParams value){p=value;geometry();repaint();}
    void mouseDown(const juce::MouseEvent& e)override{geometry();handle=(segmentIndex>=0&&std::abs(e.position.x-points[segmentIndex].x)<std::abs(e.position.x-points[sustainIndex].x))?1:0;if(onEdit)onEdit(handle,valueAt(e.position.y),true,false);}
    void mouseDrag(const juce::MouseEvent& e)override{if(handle>=0&&onEdit)onEdit(handle,valueAt(e.position.y),false,false);}
    void mouseUp(const juce::MouseEvent& e)override{if(handle>=0&&onEdit)onEdit(handle,valueAt(e.position.y),false,true);handle=-1;}
    void paint(juce::Graphics& g)override{
      geometry();g.fillAll(juce::Colour(0xff0b131e));g.setColour(juce::Colour(0xff40566f));g.drawRect(getLocalBounds());
      auto b=getLocalBounds().toFloat().reduced(12);g.setColour(juce::Colour(0xff2f4456));for(int i=0;i<=4;++i)g.drawHorizontalLine(int(b.getY()+b.getHeight()*i/4),b.getX(),b.getRight());
      juce::Path path;path.startNewSubPath(points[0]);for(int i=1;i<count;++i)for(int j=1;j<=32;++j){float t=j/32.f;path.lineTo(std::lerp(points[i-1].x,points[i].x,t),std::lerp(points[i-1].y,points[i].y,cosmos::envelopeCurve(t,p.curve)));}
      g.setColour(cyan);g.strokePath(path,juce::PathStrokeType(1.7f));
      for(int k=0;k<count;++k){g.setColour(k==sustainIndex||k==segmentIndex?juce::Colour(0xffe5b16d):cyan);g.fillEllipse(points[k].x-3,points[k].y-3,6,6);}
    }
};
class Editor final:public juce::AudioProcessorEditor,private juce::ListBoxModel,private juce::Timer{
  Processor& p;Theme theme;juce::Component canvas;
  juce::TextEditor search;juce::ListBox browser;juce::Label patchLabel,layerTitle,masterLabel;
  juce::Slider master;juce::ComboBox wave;juce::ToggleButton enabled{"Couche active"};
  std::array<juce::TextButton,4> tabs;std::array<juce::Slider,8> knobs;std::array<juce::Label,8> labels;
  using SA=State::SliderAttachment;using CA=State::ComboBoxAttachment;using BA=State::ButtonAttachment;
  std::unique_ptr<SA> masterAtt;std::array<std::unique_ptr<SA>,8> att;std::unique_ptr<CA> waveAtt;std::unique_ptr<BA> enabledAtt;
  std::vector<int> visible;int selected=0,selectedOsc=0;bool oscPage=false;
  juce::TextButton oscPageButton{"OSCILLATEURS"},importButton{"Importer WAV / AIFF"};
  std::array<juce::TextButton,5> oscTabs;
  juce::ComboBox oscMode,oscWave;
  std::array<juce::Slider,9> oscKnobs;std::array<juce::Label,9> oscLabels;
  juce::ToggleButton oscSync{"Hard Sync"},oscRing{"Ring Mod"};
  juce::Label sampleLabel;
  WavePreview preview;
  std::array<std::unique_ptr<SA>,9> oscAtt;std::unique_ptr<CA> modeAtt,oscWaveAtt;std::unique_ptr<BA> syncAtt,ringAtt;
  std::unique_ptr<juce::FileChooser> chooser;
  bool grainPage=false,filterPage=false,filterModPage=false,lfoPage=false;
  bool envPage=false;int selectedEnv=0;juce::TextButton envButton{"ENVELOPPES"};std::array<juce::TextButton,12> envTabs;
  ModEnvelopeView envView;std::array<juce::ComboBox,6> envBoxes;std::array<juce::Label,6> envBoxLabels;std::array<juce::Slider,9> envKnobs;std::array<juce::Label,9> envLabels;std::array<std::unique_ptr<CA>,6> envChoiceAtt;std::array<std::unique_ptr<SA>,9> envSliderAtt;
  void bindEnvelope(int i){selectedEnv=i;for(auto& a:envChoiceAtt)a.reset();for(auto& a:envSliderAtt)a.reset();for(int j=0;j<6;++j)envChoiceAtt[j]=std::make_unique<CA>(p.state,eid(i,envKeys[j]),envBoxes[j]);for(int j=0;j<9;++j)envSliderAtt[j]=std::make_unique<SA>(p.state,eid(i,envKeys[j+6]),envKnobs[j]);for(int j=0;j<12;++j)envTabs[j].setToggleState(i==j,juce::dontSendNotification);}
  int selectedLfo=0;juce::TextButton lfoButton{"LFO"};std::array<juce::TextButton,8> lfoTabs;
  LfoOverview lfoOverview;std::array<juce::ComboBox,6> lfoBoxes;std::array<juce::Label,6> lfoBoxLabels;std::array<juce::Slider,3> lfoKnobs;std::array<juce::Label,3> lfoLabels;std::array<std::unique_ptr<CA>,6> lfoChoiceAtt;std::array<std::unique_ptr<SA>,3> lfoSliderAtt;
  void bindLfo(int i){selectedLfo=i;for(auto& a:lfoChoiceAtt)a.reset();for(auto& a:lfoSliderAtt)a.reset();for(int j=0;j<6;++j)lfoChoiceAtt[j]=std::make_unique<CA>(p.state,lid(i,lfoKeys[j<4?j:j+3]),lfoBoxes[j]);for(int j=0;j<3;++j)lfoSliderAtt[j]=std::make_unique<SA>(p.state,lid(i,lfoKeys[j+4]),lfoKnobs[j]);for(int j=0;j<8;++j)lfoTabs[j].setToggleState(i==j,juce::dontSendNotification);}
  void setLfoPage(bool show){envPage=false;lfoPage=show;filterPage=false;filterModPage=false;setOscPage(false);if(show)bindLfo(selectedLfo);}
  juce::TextButton filterModButton{"MODULATION"};
  std::array<juce::Slider,9> filterModKnobs;std::array<juce::Label,9> filterModText;std::array<std::unique_ptr<SA>,9> filterModAtt;
  juce::ToggleButton filterAuto{"Compensation auto"};std::unique_ptr<BA> filterAutoAtt;FilterResponse filterResponse;
  juce::TextButton filterButton{"FILTRES"};juce::ComboBox filterA,filterB,filterRoute;
  std::array<juce::Slider,8> filterKnobs;std::array<juce::Label,8> filterText;
  std::array<std::unique_ptr<SA>,8> filterAtt;std::array<std::unique_ptr<CA>,3> filterChoice;
  void bindFilters(){for(auto& a:filterModAtt)a.reset();for(int j=0;j<9;++j)filterModAtt[j]=std::make_unique<SA>(p.state,id(selected,filterModKeys[j]),filterModKnobs[j]);filterAutoAtt.reset();filterAutoAtt=std::make_unique<BA>(p.state,id(selected,"fAuto"),filterAuto);for(auto& a:filterAtt)a.reset();for(auto& a:filterChoice)a.reset();for(int j=0;j<8;++j)filterAtt[j]=std::make_unique<SA>(p.state,id(selected,filterKeys[j]),filterKnobs[j]);filterChoice[0]=std::make_unique<CA>(p.state,id(selected,"fModel"),filterA);filterChoice[1]=std::make_unique<CA>(p.state,id(selected,"f2Model"),filterB);filterChoice[2]=std::make_unique<CA>(p.state,id(selected,"fRoute"),filterRoute);}
  void setFilterPage(bool show){if(!show)filterModPage=false;filterPage=show;setOscPage(false);bindFilters();}
  juce::ComboBox tableBox;juce::TextButton tableImport{"Importer table WAV"};std::unique_ptr<CA> tableAtt;
  juce::TextButton grainButton{"GRAINS"};
  std::array<juce::Slider,9> grainKnobs;
  std::array<juce::Label,9> grainLabels;
  std::array<juce::ToggleButton,3> grainToggles;
  juce::ComboBox grainWindowBox,grainSourceBox;
  std::array<std::unique_ptr<SA>,9> grainAtt;
  std::array<std::unique_ptr<BA>,3> grainToggleAtt;
  std::unique_ptr<CA> grainWindowAtt,grainSourceAtt;
  void grainVisibility(){
    bool show=oscPage&&grainPage;
    grainButton.setVisible(oscPage);grainButton.setButtonText(show?"SYNTHÈSE":"GRAINS");
    for(int j=0;j<9;++j){oscKnobs[j].setVisible(oscPage&&!show);oscLabels[j].setVisible(oscPage&&!show);grainKnobs[j].setVisible(show);grainLabels[j].setVisible(show);}
    oscSync.setVisible(oscPage&&!show);oscRing.setVisible(oscPage&&!show);
    for(auto& t:grainToggles)t.setVisible(show);
    grainWindowBox.setVisible(show);grainSourceBox.setVisible(show);
    oscWave.setVisible(oscPage&&!show);tableBox.setVisible(oscPage&&!show&&oscMode.getSelectedId()==2);tableImport.setVisible(oscPage&&!show&&oscMode.getSelectedId()==2);
    repaint();
  }
  void setOscPage(bool show){
    if(show)filterPage=false;oscPage=show;oscPageButton.setButtonText(show?"COUCHE":"OSCILLATEURS");
    wave.setVisible(!show&&!filterPage&&!lfoPage);enabled.setVisible(!show&&!filterPage&&!lfoPage);
    lfoButton.setButtonText(lfoPage?"COUCHE":"LFO");bool lf=lfoPage&&!envPage,ev=lfoPage&&envPage;envButton.setVisible(lfoPage);envButton.setButtonText(ev?"LFO":"ENVELOPPES");for(auto& t:lfoTabs)t.setVisible(lf);lfoOverview.setVisible(lf);for(int j=0;j<6;++j){lfoBoxes[j].setVisible(lf);lfoBoxLabels[j].setVisible(lf);envBoxes[j].setVisible(ev);envBoxLabels[j].setVisible(ev);}for(int j=0;j<3;++j){lfoKnobs[j].setVisible(lf);lfoLabels[j].setVisible(lf);}for(auto& t:envTabs)t.setVisible(ev);envView.setVisible(ev);for(int j=0;j<9;++j){envKnobs[j].setVisible(ev);envLabels[j].setVisible(ev);}
    filterButton.setVisible(!show&&!lfoPage);filterButton.setButtonText(filterPage?"COUCHE":"FILTRES");oscPageButton.setVisible(!filterPage&&!lfoPage);
    bool mainFilter=filterPage&&!filterModPage,modFilter=filterPage&&filterModPage;filterModButton.setVisible(filterPage);filterModButton.setButtonText(modFilter?"RÉPONSE":"MODULATION");filterAuto.setVisible(modFilter);filterResponse.setVisible(mainFilter);filterA.setVisible(mainFilter);filterB.setVisible(mainFilter);filterRoute.setVisible(mainFilter);for(int j=0;j<8;++j){filterKnobs[j].setVisible(mainFilter);filterText[j].setVisible(mainFilter);}for(int j=0;j<9;++j){filterModKnobs[j].setVisible(modFilter);filterModText[j].setVisible(modFilter);}
    for(int j=0;j<8;++j){knobs[j].setVisible(!show&&!filterPage&&!lfoPage);labels[j].setVisible(!show&&!filterPage&&!lfoPage);}
    for(auto& t:oscTabs)t.setVisible(show);
    for(int j=0;j<9;++j){oscKnobs[j].setVisible(show);oscLabels[j].setVisible(show);}
    oscMode.setVisible(show);oscWave.setVisible(show);oscSync.setVisible(show);oscRing.setVisible(show);
    importButton.setVisible(show);sampleLabel.setVisible(show);preview.setVisible(show);
    if(show)bindOsc(selectedOsc);grainVisibility();repaint();
  }
  void bindOsc(int o){
    selectedOsc=o;tableAtt.reset();tableAtt=std::make_unique<CA>(p.state,oid(selected,o,"tableSource"),tableBox);
    for(auto& a:grainAtt)a.reset();for(auto& a:grainToggleAtt)a.reset();grainWindowAtt.reset();grainSourceAtt.reset();
    for(int j=0;j<9;++j)grainAtt[j]=std::make_unique<SA>(p.state,oid(selected,o,grainKeys[j]),grainKnobs[j]);
    for(int j=0;j<3;++j)grainToggleAtt[j]=std::make_unique<BA>(p.state,oid(selected,o,grainFlags[j]),grainToggles[j]);
    grainWindowAtt=std::make_unique<CA>(p.state,oid(selected,o,"gWindow"),grainWindowBox);grainSourceAtt=std::make_unique<CA>(p.state,oid(selected,o,"gSource"),grainSourceBox);
    for(auto& a:oscAtt)a.reset();modeAtt.reset();oscWaveAtt.reset();syncAtt.reset();ringAtt.reset();
    for(int j=0;j<9;++j)oscAtt[j]=std::make_unique<SA>(p.state,oid(selected,o,oscKeys[j]),oscKnobs[j]);
    modeAtt=std::make_unique<CA>(p.state,oid(selected,o,"mode"),oscMode);
    oscWaveAtt=std::make_unique<CA>(p.state,o==0?id(selected,"wave"):oid(selected,o,"wave"),oscWave);
    syncAtt=std::make_unique<BA>(p.state,oid(selected,o,"sync"),oscSync);ringAtt=std::make_unique<BA>(p.state,oid(selected,o,"ring"),oscRing);
    for(int j=0;j<5;++j)oscTabs[j].setToggleState(j==o,juce::dontSendNotification);
    timerCallback();
  }
  void importTable(){
    const int k=selected,o=selectedOsc;
    chooser=std::make_unique<juce::FileChooser>("Wavetable : cycles de 2048",juce::File{},"*.wav");
    auto safe=juce::Component::SafePointer<Editor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,k,o](const juce::FileChooser& f){
      if(auto* e=safe.getComponent()){
        auto file=f.getResult();if(file==juce::File{})return;
        auto error=e->p.loadTable(k,file);if(error.isNotEmpty()){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Omniscience",error);return;}
        auto set=[&](const char* key,float v){if(auto* par=e->p.state.getParameter(oid(k,o,key)))par->setValueNotifyingHost(par->convertTo0to1(v));};
        set("mode",1);set("level",1);set("tableSource",2);e->timerCallback();
      }
    });
  }
  void importSample(){
    const int k=selected,o=selectedOsc;const bool importGranular=grainPage;
    chooser=std::make_unique<juce::FileChooser>("Importer une source audio",juce::File{},"*.wav;*.aif;*.aiff");
    auto safe=juce::Component::SafePointer<Editor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,k,o,importGranular](const juce::FileChooser& f){
      if(auto* e=safe.getComponent()){
        auto file=f.getResult();if(file==juce::File{})return;
        auto error=e->p.loadSample(k,file);
        if(error.isNotEmpty()){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Omniscience",error);return;}
        if(auto* par=e->p.state.getParameter(oid(k,o,"mode")))par->setValueNotifyingHost(par->convertTo0to1(importGranular?5.f:4.f));
        if(auto* par=e->p.state.getParameter(oid(k,o,"gSource")))par->setValueNotifyingHost(0.f);
        if(auto* par=e->p.state.getParameter(oid(k,o,"level")))par->setValueNotifyingHost(par->convertTo0to1(1.f));
        e->timerCallback();
      }
    });
  }
  void add(juce::Component& c){canvas.addAndMakeVisible(c);}
  void label(juce::Label& l,juce::String text,float size){l.setText(text,juce::dontSendNotification);l.setFont(juce::Font(juce::FontOptions(size)));l.setColour(juce::Label::textColourId,light);l.setJustificationType(juce::Justification::centred);add(l);}
  void configure(juce::Slider& s){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,85,22);add(s);}
  void filter(){visible.clear();auto q=search.getText();for(int i=0;i<270;++i)if(presetName(i).containsIgnoreCase(q))visible.push_back(i);browser.deselectAllRows();browser.updateContent();}
  int getNumRows()override{return int(visible.size());}
  void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool sel)override{if(row<0||row>=int(visible.size()))return;g.fillAll(sel?juce::Colour(0xff294b67):(row%2?ink:juce::Colour(0xff17202d)));g.setColour(light);g.setFont(14);g.drawText(presetName(visible[size_t(row)]),8,0,w-12,h,juce::Justification::centredLeft,true);}
  void selectedRowsChanged(int row)override{if(row>=0&&row<int(visible.size()))p.setCurrentProgram(visible[size_t(row)]);}
  void timerCallback()override{
    patchLabel.setText(p.getProgramName(p.getCurrentProgram()),juce::dontSendNotification);
    if(lfoPage){if(envPage)envView.update(p.patch().modEnv[selectedEnv]);else lfoOverview.update(p.patch().lfo,selectedLfo);}
    if(filterPage&&!filterModPage){auto layer=p.patch().layer[selected];filterResponse.update(layer.filter1,layer.filter2,p.getSampleRate()>0?p.getSampleRate():48000.);}
    if(oscPage){if(!grainPage){const bool table=oscMode.getSelectedId()==2;tableBox.setVisible(table);tableImport.setVisible(table);}auto patch=p.patch();auto& l=patch.layer[selected];auto o=l.osc[selectedOsc];auto* s=l.sample;
      if(o.mode==cosmos::OscMode::granular&&o.grain.source)s=&cosmos::factoryGrains()[size_t(o.grain.source-1)];
      std::array<float,16> positions;for(size_t i=0;i<positions.size();++i)positions[i]=p.grainView[selected][selectedOsc][i].load(std::memory_order_relaxed);
      preview.update(o,s,positions);sampleLabel.setText(o.mode==cosmos::OscMode::wavetable?(o.tableSource==2?p.tableName(selected):o.tableSource==1?juce::String("Factory HD 256 × 2048"):juce::String("Historique 16 × 2048")):o.mode==cosmos::OscMode::granular&&o.grain.source?juce::String("Source d'usine"):p.sampleName(selected),juce::dontSendNotification);}
  }
  void selectLayer(int k){
    selected=k;for(auto& a:att)a.reset();waveAtt.reset();enabledAtt.reset();
    for(int j=0;j<8;++j)att[j]=std::make_unique<SA>(p.state,id(k,keys[j]),knobs[j]);
    waveAtt=std::make_unique<CA>(p.state,id(k,"wave"),wave);enabledAtt=std::make_unique<BA>(p.state,id(k,"enabled"),enabled);
    for(int j=0;j<4;++j)tabs[j].setToggleState(j==k,juce::dontSendNotification);
    layerTitle.setText("ÉDITION DE LA COUCHE "+juce::String::charToString(juce::juce_wchar('A'+k)),juce::dontSendNotification);
    if(oscPage)bindOsc(selectedOsc);if(filterPage)bindFilters();
  }
public:
  explicit Editor(Processor& pr):AudioProcessorEditor(pr),p(pr){
    setLookAndFeel(&theme);addAndMakeVisible(canvas);canvas.setSize(1200,760);
    search.setTextToShowWhenEmpty("Rechercher un preset…",juce::Colour(0xff91a8c0));search.setColour(juce::TextEditor::backgroundColourId,ink);search.setColour(juce::TextEditor::textColourId,light);search.onTextChange=[this]{filter();};add(search);
    browser.setModel(this);browser.setRowHeight(28);add(browser);filter();
    label(patchLabel,"",22);label(layerTitle,"",18);label(masterLabel,"MASTER",14);configure(master);masterAtt=std::make_unique<SA>(p.state,"master",master);
    for(int k=0;k<4;++k){tabs[k].setButtonText("COUCHE "+juce::String::charToString(juce::juce_wchar('A'+k)));tabs[k].onClick=[this,k]{selectLayer(k);};add(tabs[k]);}
    wave.addItemList({"Sinus","Dent de scie","Carrée","Triangle"},1);add(wave);add(enabled);
    for(int j=0;j<8;++j){configure(knobs[j]);label(labels[j],juce::String::fromUTF8(names[j]),15);}
    oscPageButton.onClick=[this]{setOscPage(!oscPage);};add(oscPageButton);
    for(int o=0;o<5;++o){oscTabs[o].setButtonText("OSC "+juce::String(o+1));oscTabs[o].onClick=[this,o]{bindOsc(o);};add(oscTabs[o]);}
    oscMode.addItemList({"Analogique","Wavetable HD","FM / PM","AM","Sample","Granulaire"},1);add(oscMode);
    oscWave.addItemList({"Sinus","Dent de scie","Carrée","Triangle"},1);add(oscWave);
    importButton.onClick=[this]{importSample();};add(importButton);label(sampleLabel,"Aucun sample",11);add(preview);
    tableBox.addItemList({"Historique 16","Factory HD 256","WAV personnel"},1);add(tableBox);tableImport.onClick=[this]{importTable();};add(tableImport);
    preview.onLoopEdit=[this](int handle,float q,bool begin,bool end){
      auto* par=p.state.getParameter(oid(selected,selectedOsc,handle==1?"gStart":"gEnd"));if(!par)return;
      auto z=p.patch().layer[selected].osc[selectedOsc].grain;
      float value=handle==1?juce::jlimit(0.f,std::max(0.f,z.loopEnd-.001f),q):juce::jlimit(std::min(1.f,z.loopStart+.001f),1.f,q);
      if(begin)par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(value));if(end)par->endChangeGesture();timerCallback();
    };
    for(int j=0;j<9;++j){configure(oscKnobs[j]);label(oscLabels[j],juce::String::fromUTF8(oscNames[j]),13);}
    add(oscSync);add(oscRing);
    grainButton.onClick=[this]{grainPage=!grainPage;grainVisibility();};add(grainButton);
    for(int j=0;j<9;++j){configure(grainKnobs[j]);label(grainLabels[j],juce::String::fromUTF8(grainNames[j]),13);}
    const char* flagNames[]={"Freeze","Reverse","Tempo"};
    for(int j=0;j<3;++j){grainToggles[j].setButtonText(flagNames[j]);add(grainToggles[j]);}
    grainWindowBox.addItemList({"Hann","Triangle","Trapèze"},1);add(grainWindowBox);
    grainSourceBox.addItemList({"Sample personnel","Cosmic Analog","Metal Resonance","Nebula Noise","Crystal Bell"},1);add(grainSourceBox);
    filterA.addItemList(filterNames,1);filterB.addItemList(filterNames,1);filterRoute.addItemList({"Série","Parallèle"},1);add(filterA);add(filterB);add(filterRoute);add(filterAuto);add(filterResponse);filterResponse.onCutoff=[this](int which,float hz,bool begin,bool end){if(auto* par=p.state.getParameter(id(selected,which==0?"cutoff":"f2Cutoff"))){if(begin)par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(juce::jlimit(20.f,20000.f,hz)));if(end)par->endChangeGesture();}};for(int j=0;j<8;++j){configure(filterKnobs[j]);label(filterText[j],juce::String::fromUTF8(filterLabels[j]),14);}
    for(int j=0;j<9;++j){configure(filterModKnobs[j]);label(filterModText[j],juce::String::fromUTF8(filterModLabels[j]),14);}
    filterModButton.onClick=[this]{filterModPage=!filterModPage;setOscPage(false);};add(filterModButton);
    filterButton.onClick=[this]{setFilterPage(!filterPage);};add(filterButton);
    lfoButton.onClick=[this]{setLfoPage(!lfoPage);};add(lfoButton);for(int j=0;j<8;++j){lfoTabs[j].setButtonText("LFO "+juce::String(j+1));lfoTabs[j].onClick=[this,j]{bindLfo(j);};add(lfoTabs[j]);}
    add(lfoOverview);const juce::StringArray* lists[]={&lfoShapes,&lfoDest,&lfoLayers,&lfoDivisions,&lfoPolarities,&lfoModes};const char* boxNames[]={"Forme","Destination","Couche cible","Synchronisation","Polarité","Déclenchement"};const char* knobNames[]={"Fréquence (Hz)","Profondeur","Phase initiale"};
    for(int j=0;j<6;++j){lfoBoxes[j].addItemList(*lists[j],1);add(lfoBoxes[j]);label(lfoBoxLabels[j],juce::String::fromUTF8(boxNames[j]),14);}for(int j=0;j<3;++j){configure(lfoKnobs[j]);label(lfoLabels[j],juce::String::fromUTF8(knobNames[j]),15);}
    envButton.onClick=[this]{envPage=!envPage;setOscPage(false);if(envPage)bindEnvelope(selectedEnv);else bindLfo(selectedLfo);};add(envButton);
    for(int j=0;j<12;++j){envTabs[j].setButtonText("ENV "+juce::String(j+1));envTabs[j].onClick=[this,j]{bindEnvelope(j);};add(envTabs[j]);}
    const juce::StringArray* envLists[]={&envModes,&envPlay,&envCurves,&envSync,&lfoDest,&lfoLayers};const char* envBoxNames[]={"Structure","Lecture","Courbe","Unité de temps","Destination","Couche cible"};const char* envKnobNames[]={"Attaque","Maintien","Déclin","Sustain","Relâchement","Durée segment","Niveau segment","Profondeur","Vélocité"};
    for(int j=0;j<6;++j){envBoxes[j].addItemList(*envLists[j],1);add(envBoxes[j]);label(envBoxLabels[j],juce::String::fromUTF8(envBoxNames[j]),13);}
    for(int j=0;j<9;++j){configure(envKnobs[j]);label(envLabels[j],juce::String::fromUTF8(envKnobNames[j]),13);}
    add(envView);envView.onEdit=[this](int which,float v,bool begin,bool end){if(auto* par=p.state.getParameter(eid(selectedEnv,which?"segmentLevel":"sustain"))){if(begin)par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(v));if(end)par->endChangeGesture();}};
    selectLayer(0);setOscPage(false);timerCallback();startTimerHz(20);
    setResizable(true,true);setResizeLimits(900,570,1800,1140);if(auto* c=getConstrainer())c->setFixedAspectRatio(1200./760.);setSize(1200,760);
  }
  #ifdef COSMOS_PRESET_QA
  bool qualifyLayout(const juce::File& folder){
    bool ok=true;
    for(int width:{900,1200,1800})for(int page=0;page<7;++page){
      setSize(width,int(width*760./1200.));grainPage=page==2;filterPage=false;lfoPage=false;setOscPage(page==1||page==2);if(page==3||page==4){filterModPage=page==4;setFilterPage(true);}if(page>=5){setLfoPage(true);if(page==6){envPage=true;setOscPage(false);bindEnvelope(0);}}timerCallback();
      for(int i=0;i<canvas.getNumChildComponents();++i){auto* a=canvas.getChildComponent(i);if(!a->isVisible())continue;
        ok&=canvas.getLocalBounds().contains(a->getBounds());
        for(int j=i+1;j<canvas.getNumChildComponents();++j){auto* b=canvas.getChildComponent(j);if(b->isVisible()&&a->getBounds().intersects(b->getBounds())){ok=false;juce::Logger::writeToLog("Layout overlap: "+a->getName()+" / "+b->getName());}}
      }
      auto image=createComponentSnapshot(getLocalBounds());auto file=folder.getChildFile(juce::String(width)+"-page-"+juce::String(page)+".png");
      juce::FileOutputStream out(file);juce::PNGImageFormat png;ok&=out.openedOk()&&png.writeImageToStream(image,out);
    }
    return ok;
  }
  #endif
  ~Editor()override{stopTimer();browser.setModel(nullptr);setLookAndFeel(nullptr);}
  void paint(juce::Graphics& g)override{
    g.fillAll(ink);auto sc=float(getWidth())/1200.f;g.addTransform(juce::AffineTransform::scale(sc));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff253d62),0,0,ink,1200,142,false));g.fillRect(0,0,1200,138);
    g.setColour(cyan);g.setFont(juce::Font(juce::FontOptions(14.f)));g.drawText("LIBERTY MUSIC SOFTWARE",22,13,260,24,juce::Justification::centredLeft);
    g.setColour(light);g.setFont(juce::Font(juce::FontOptions(38.f)));g.drawText("O M N I S C I E N C E",286,19,730,52,juce::Justification::centred);
    g.setColour(juce::Colour(0xff99b9d3));g.setFont(14);g.drawText("COSMOS ENGINE  /  HYBRID SYNTHESIZER",286,72,730,25,juce::Justification::centred);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff202d40),14,150,juce::Colour(0xff111924),14,730,false));g.fillRoundedRectangle(14,150,266,580,8);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff202b3c),292,216,juce::Colour(0xff141c28),292,730,false));g.fillRoundedRectangle(292,216,894,514,8);
    g.setColour(juce::Colour(0xff40566f));g.drawRoundedRectangle(14,150,266,580,8,1);g.drawRoundedRectangle(292,216,894,514,8,1);
    g.setColour(cyan.withAlpha(.65f));g.fillRect(303,218,870,2);
    g.setColour(cyan);g.setFont(17);g.drawText("BIBLIOTHÈQUE  /  270",25,162,242,28,juce::Justification::centred);
    g.setColour(juce::Colour(0xff35495e));if(!oscPage&&!filterPage&&!lfoPage)for(int j=0;j<8;++j)g.drawRoundedRectangle(310.f+216.f*(j%4),316.f+180.f*(j/4),204.f,168.f,6.f,1.f);
    if(oscPage)for(int j=0;j<10;++j)g.drawRoundedRectangle(310.f+172.f*(j%5),449.f+132.f*(j/5),164.f,126.f,6.f,1.f);
    g.setColour(juce::Colour(0xff9ab6ce));g.setFont(13);g.drawText("v0.5.2  COSMOS ENVELOPE UNIVERSE  •  4 couches / 20 oscillateurs / 32 voix",18,733,1155,24,juce::Justification::centredRight);
  }
  void resized()override{
    canvas.setTransform(juce::AffineTransform::scale(float(getWidth())/1200.f));canvas.setBounds(0,0,1200,760);
    search.setBounds(26,199,242,30);browser.setBounds(26,239,242,478);patchLabel.setBounds(290,101,730,32);
    masterLabel.setBounds(1040,8,140,20);master.setBounds(1045,30,130,102);
    for(int k=0;k<4;++k)tabs[k].setBounds(292+225*k,151,219,52);
    layerTitle.setBounds(312,227,280,32);lfoButton.setBounds(600,230,150,30);envButton.setBounds(760,230,175,30);
    for(int j=0;j<12;++j)envTabs[j].setBounds(310+71*j,270,68,30);envView.setBounds(310,308,855,85);
    for(int j=0;j<6;++j){int x=310+285*(j%3),y=398+50*(j/3);envBoxLabels[j].setBounds(x,y,270,18);envBoxes[j].setBounds(x,y+19,270,27);}
    for(int j=0;j<9;++j){int x=310+172*(j%5),y=501+108*(j/5);envLabels[j].setBounds(x,y,162,19);envKnobs[j].setBounds(x+25,y+20,112,83);}
    for(int j=0;j<8;++j)lfoTabs[j].setBounds(310+107*j,270,102,32);
    lfoOverview.setBounds(310,310,855,98);
    for(int j=0;j<6;++j){int x=310+285*(j%3),y=414+61*(j/3);lfoBoxLabels[j].setBounds(x,y,270,20);lfoBoxes[j].setBounds(x,y+22,270,30);}
    for(int j=0;j<3;++j){int x=310+285*j;lfoLabels[j].setBounds(x,540,270,22);lfoKnobs[j].setBounds(x+55,565,160,145);}wave.setBounds(312,270,350,32);enabled.setBounds(704,270,220,32);
    for(int j=0;j<8;++j){int x=310+216*(j%4),y=316+180*(j/4);labels[j].setBounds(x+4,y+5,196,24);knobs[j].setBounds(x+20,y+31,164,130);}
    oscPageButton.setBounds(945,230,220,30);filterButton.setBounds(760,230,175,30);
    filterModButton.setBounds(945,230,220,30);filterAuto.setBounds(310,270,250,30);
    for(int j=0;j<9;++j){int x=310+172*(j%5),y=310+192*(j/5);filterModText[j].setBounds(x+3,y+4,158,22);filterModKnobs[j].setBounds(x+7,y+28,150,150);}filterResponse.setBounds(310,310,855,106);
    filterA.setBounds(312,270,270,32);filterB.setBounds(590,270,270,32);filterRoute.setBounds(868,270,297,32);
    for(int j=0;j<8;++j){int x=310+216*(j%4),y=422+148*(j/4);filterText[j].setBounds(x+4,y+2,196,22);filterKnobs[j].setBounds(x+20,y+25,164,113);}
    for(int o=0;o<5;++o)oscTabs[o].setBounds(310+172*o,270,164,30);
    oscMode.setBounds(310,310,185,30);oscWave.setBounds(500,310,155,30);tableBox.setBounds(660,310,165,30);tableImport.setBounds(830,310,160,30);importButton.setBounds(995,310,170,30);
    sampleLabel.setBounds(310,342,855,20);preview.setBounds(310,365,855,74);
    for(int j=0;j<9;++j){int x=310+172*(j%5),y=449+132*(j/5);oscLabels[j].setBounds(x+3,y+2,158,20);oscKnobs[j].setBounds(x+5,y+22,154,100);}
    oscSync.setBounds(1010,598,145,33);oscRing.setBounds(1010,640,145,33);
    grainButton.setBounds(760,230,175,30);grainSourceBox.setBounds(500,310,325,30);
    for(int j=0;j<9;++j){int x=310+172*(j%5),y=449+132*(j/5);grainLabels[j].setBounds(x+3,y+2,158,20);grainKnobs[j].setBounds(x+5,y+22,154,100);}
    grainWindowBox.setBounds(1004,584,158,26);
    for(int j=0;j<3;++j)grainToggles[j].setBounds(1008,612+28*j,146,26);
  }
};
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Processor();}

#ifdef COSMOS_PRESET_QA
void cosmosSetReferenceDSP(juce::AudioProcessor& p,bool ref){static_cast<Processor&>(p).referenceDSPForQA=ref;}
bool cosmosTestImport(juce::AudioProcessor& p,const juce::File& f,bool table){auto& x=static_cast<Processor&>(p);return (table?x.loadTable(0,f):x.loadSample(0,f)).isEmpty();}
bool cosmosTestAssets(juce::AudioProcessor& p,bool expected){auto& x=static_cast<Processor&>(p);return (x.samples[0].load()!=nullptr)==expected&&(x.tables[0].load()!=nullptr)==expected;}
bool cosmosTestLayout(juce::AudioProcessor& p,const juce::File& dir){std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());return static_cast<Editor&>(*editor).qualifyLayout(dir);}
bool cosmosTestCache(juce::AudioProcessor& p){auto& x=static_cast<Processor&>(p);return x.retainedSamples.size()==1&&x.retainedTables.size()==1;}
#endif
