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
const std::array<const char*,8> keys={"gain","pan","tune","cutoff","attack","decay","sustain","release"};
const std::array<const char*,8> names={"Volume","Panoramique","Accordage","Fréquence","Attaque","Déclin","Sustain","Relâchement"};
const std::array<const char*,10> categories={"Analog","Pads","Bass","Keys","Plucks","Cinematic","Leads","Textures","Bells","Soft"};
const std::array<const char*,10> adjectives={"Dawn","Orbit","Nebula","Horizon","Nova","Eclipse","Zenith","Vortex","Halo","Astra"};
juce::String id(int k,const char* key){return juce::String::charToString(juce::juce_wchar('A'+k))+"_"+key;}
const std::array<const char*,9> oscKeys={"level","tune","unison","detune","drift","morph","ratio","depth","drive"};
const std::array<const char*,9> oscNames={"Niveau","Accordage","Unisson","Dispersion","Dérive","Morphing","Ratio","Profondeur","Saturation"};
juce::String oid(int k,int o,const char* key){return id(k,(juce::String("osc")+juce::String(o+1)+"_"+key).toRawUTF8());}
juce::String presetName(int i){return juce::String(categories[size_t(i/10)])+" / "+adjectives[size_t(i%10)];}
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
    add("attack","Attaque",.001f,8,.03f,.3f);add("decay","Déclin",.005f,8,.35f,.3f);
    add("sustain","Sustain",0,1,.75f);add("release","Relâchement",.005f,10,.5f,.3f);
    for(int o=0;o<5;++o){
      auto key=[&](const char* n){return oid(k,o,n);};
      auto of=[&](const char* n,float lo,float hi,float def,float step=0.f){p.add(std::make_unique<F>(ID{key(n),1},key(n),juce::NormalisableRange<float>(lo,hi,step),def));};
      p.add(std::make_unique<C>(ID{key("mode"),1},key("mode"),juce::StringArray{"Analogique","Wavetable HD","FM / PM","AM","Sample"},0));
      if(o!=0)p.add(std::make_unique<C>(ID{key("wave"),1},key("wave"),juce::StringArray{"Sinus","Dent de scie","Carrée","Triangle"},1));
      of("level",0,1,o==0?1.f:0.f);of("tune",-24,24,0);of("unison",1,16,1,1);of("detune",0,100,0);of("drift",0,35,0);of("morph",0,1,0);of("ratio",.125f,16,2);of("depth",0,1,0);of("drive",0,1,0);
      p.add(std::make_unique<B>(ID{key("sync"),1},key("sync"),false));p.add(std::make_unique<B>(ID{key("ring"),1},key("ring"),false));
    }
  }
  return p;
}
class Processor final:public juce::AudioProcessor {
public:
  State state{*this,nullptr,"OMNISCIENCE",layout()};
  cosmos::Engine engine;
  std::array<std::array<std::atomic<float>*,10>,4> params{};
  std::atomic<float>* master=nullptr;
  std::atomic<int> program{-1};
  std::array<std::array<std::array<std::atomic<float>*,13>,5>,4> oscillators{};
  std::array<std::atomic<const cosmos::SampleData*>,4> samples{};
  juce::AudioFormatManager formats;
  mutable std::mutex sampleMutex;
  std::vector<std::unique_ptr<cosmos::SampleData>> retainedSamples;
  std::array<juce::String,4> samplePaths{};
  size_t retainedBytes=0;
  juce::String loadSample(int layer,const juce::File& file){
    std::lock_guard<std::mutex> lock(sampleMutex);
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
    auto ptr=s.get();retainedSamples.push_back(std::move(s));retainedBytes+=size_t(n)*sizeof(float);
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
      reset("mode");if(o!=0)reset("wave");for(auto key:oscKeys)reset(key);reset("sync");reset("ring");
    }
  }
  Processor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)){
    formats.registerBasicFormats();
    master=state.getRawParameterValue("master");
    for(int k=0;k<4;++k)for(int o=0;o<5;++o){
      auto& a=oscillators[k][o];a[0]=state.getRawParameterValue(oid(k,o,"mode"));a[1]=state.getRawParameterValue(o==0?id(k,"wave"):oid(k,o,"wave"));
      for(int j=0;j<9;++j)a[j+2]=state.getRawParameterValue(oid(k,o,oscKeys[j]));
      a[11]=state.getRawParameterValue(oid(k,o,"sync"));a[12]=state.getRawParameterValue(oid(k,o,"ring"));
    }
    for(int k=0;k<4;++k){params[k][0]=state.getRawParameterValue(id(k,"enabled"));params[k][1]=state.getRawParameterValue(id(k,"wave"));for(int j=0;j<8;++j)params[k][j+2]=state.getRawParameterValue(id(k,keys[j]));}
    setCurrentProgram(0);
  }
  const juce::String getName() const override{return "Omniscience";}
  bool acceptsMidi()const override{return true;} bool producesMidi()const override{return false;} bool isMidiEffect()const override{return false;}
  double getTailLengthSeconds()const override{return 10;}
  bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
  void prepareToPlay(double sr,int)override{engine.prepare(sr);}
  void releaseResources()override{engine.reset();}
  cosmos::Patch patch() const {
    cosmos::Patch p;p.master=master->load();
    for(int k=0;k<4;++k){auto& l=p.layer[k];auto& a=params[k];l.enabled=a[0]->load()>.5f;l.wave=int(a[1]->load());l.gain=a[2]->load();l.pan=a[3]->load();l.tune=a[4]->load();l.cutoff=a[5]->load();l.attack=a[6]->load();l.decay=a[7]->load();l.sustain=a[8]->load();l.release=a[9]->load();l.sample=samples[k].load(std::memory_order_acquire);
      for(int o=0;o<5;++o){
        const auto& q=oscillators[k][o];auto& v=l.osc[o];v.mode=cosmos::OscMode(int(q[0]->load()));v.wave=int(q[1]->load());v.level=q[2]->load();v.tune=q[3]->load();v.unison=int(q[4]->load());v.detune=q[5]->load();v.drift=q[6]->load();v.morph=q[7]->load();v.ratio=q[8]->load();v.depth=q[9]->load();v.drive=q[10]->load();v.sync=q[11]->load()>.5f;v.ring=q[12]->load()>.5f;
      }
    }
    return p;
  }
  void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi)override{
    juce::ScopedNoDenormals nd;b.clear();if(b.getNumChannels()<2)return;
    const auto p=patch();auto* l=b.getWritePointer(0);auto* r=b.getWritePointer(1);int pos=0,n=b.getNumSamples();
    for(const auto meta:midi){
      int at=juce::jlimit(pos,n,meta.samplePosition);engine.render(l+pos,r+pos,at-pos,p);pos=at;
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
    midi.clear();
  }
  int getNumPrograms()override{return 100;} int getCurrentProgram()override{return juce::jmax(0,program.load());}
  const juce::String getProgramName(int i)override{return presetName(juce::jlimit(0,99,i));}
  void changeProgramName(int,const juce::String&)override{}
  void setCurrentProgram(int i)override{
    if(i<0||i>=100)return;
    resetOscillators();
    auto set=[&](const juce::String& key,float v){if(auto* p=state.getParameter(key))p->setValueNotifyingHost(p->convertTo0to1(v));};
    int cat=i/10,var=i%10;
    const float attacks[]={.015f,.75f,.006f,.008f,.004f,.5f,.025f,.9f,.003f,.12f};
    const float decays[]={.4f,1.4f,.22f,.7f,.18f,1.7f,.5f,2.f,1.1f,.65f};
    const float sustains[]={.72f,.85f,.7f,.45f,.12f,.78f,.65f,.65f,.08f,.7f};
    const float releases[]={.35f,2.1f,.12f,.45f,.25f,2.5f,.4f,3.f,1.2f,1.f};
    const float cutoffs[]={6500,3300,950,5500,7200,4200,11000,2400,13000,1900};
    set("master",.7f);
    for(int k=0;k<4;++k){
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
    program.store(i);
  }
  void getStateInformation(juce::MemoryBlock& dest)override{
    auto tree=state.copyState();tree.setProperty("program",program.load(),nullptr);tree.setProperty("schema",2,nullptr);
    {std::lock_guard<std::mutex> lock(sampleMutex);for(int k=0;k<4;++k)tree.setProperty(juce::Identifier("sample"+juce::String(k)),samplePaths[k],nullptr);}
    if(auto xml=tree.createXml())copyXmlToBinary(*xml,dest);
  }
  void setStateInformation(const void* data,int size)override{
    if(auto xml=getXmlFromBinary(data,size)){auto t=juce::ValueTree::fromXml(*xml);if(t.hasType(state.state.getType())){
      if(int(t.getProperty("schema",1))<2)resetOscillators();
      program.store(juce::jlimit(0,99,int(t.getProperty("program",0))));state.replaceState(t);
      for(int k=0;k<4;++k){
        auto path=t.getProperty(juce::Identifier("sample"+juce::String(k))).toString();
        {std::lock_guard<std::mutex> lock(sampleMutex);samples[k].store(nullptr);samplePaths[k]=path;}
        if(path.isNotEmpty())loadSample(k,juce::File(path));
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
    juce::Path arc;arc.addCentredArc(c.x,c.y,r+5,r+5,0,start,start+pos*(end-start),true);g.setColour(cyan);g.strokePath(arc,juce::PathStrokeType(2.5f));
    float a=start+pos*(end-start);g.drawLine(c.x+std::sin(a)*r*.45f,c.y-std::cos(a)*r*.45f,c.x+std::sin(a)*r*.83f,c.y-std::cos(a)*r*.83f,2.5f);
  }
};
class WavePreview final:public juce::Component{
  cosmos::OscParams osc;const cosmos::SampleData* sample=nullptr;
public:
  void update(cosmos::OscParams o,const cosmos::SampleData* s){osc=o;sample=s;repaint();}
  void paint(juce::Graphics& g)override{
    g.fillAll(ink);g.setColour(juce::Colour(0xff35495e));g.drawRect(getLocalBounds());
    const float mid=getHeight()*.5f;g.drawHorizontalLine(int(mid),0,float(getWidth()));
    juce::Path path;
    for(int i=0;i<getWidth();++i){
      double phase=double(i)/std::max(1,getWidth()-1);float y=0;
      switch(osc.mode){
        case cosmos::OscMode::wavetable:y=cosmos::waveBank().read(phase,osc.morph,1./2048);break;
        case cosmos::OscMode::fm:y=float(std::sin(2*cosmos::pi*phase+6*osc.depth*std::sin(2*cosmos::pi*phase*osc.ratio)));break;
        case cosmos::OscMode::am:y=cosmos::oscillator(osc.wave,phase,1./2048)*(1-osc.depth*.5f+osc.depth*.5f*float(std::sin(2*cosmos::pi*phase*osc.ratio)));break;
        case cosmos::OscMode::sample:y=sample?cosmos::sampleRead(sample,phase*std::max(0.,double(sample->mono.size())-1.)):0;break;
        default:y=cosmos::oscillator(osc.wave,phase,1./2048);break;
      }
      if(osc.ring)y*=float(std::sin(2*cosmos::pi*phase*osc.ratio));
      if(osc.drive>0){float d=1+12*osc.drive;y=std::tanh(d*y)/std::tanh(d);}
      float py=mid-y*mid*.85f;
      if(i==0)path.startNewSubPath(float(i),py);else path.lineTo(float(i),py);
    }
    g.setColour(cyan);g.strokePath(path,juce::PathStrokeType(1.5f));
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
  void setOscPage(bool show){
    oscPage=show;oscPageButton.setButtonText(show?"COUCHE":"OSCILLATEURS");
    wave.setVisible(!show);enabled.setVisible(!show);
    for(int j=0;j<8;++j){knobs[j].setVisible(!show);labels[j].setVisible(!show);}
    for(auto& t:oscTabs)t.setVisible(show);
    for(int j=0;j<9;++j){oscKnobs[j].setVisible(show);oscLabels[j].setVisible(show);}
    oscMode.setVisible(show);oscWave.setVisible(show);oscSync.setVisible(show);oscRing.setVisible(show);
    importButton.setVisible(show);sampleLabel.setVisible(show);preview.setVisible(show);
    if(show)bindOsc(selectedOsc);repaint();
  }
  void bindOsc(int o){
    selectedOsc=o;for(auto& a:oscAtt)a.reset();modeAtt.reset();oscWaveAtt.reset();syncAtt.reset();ringAtt.reset();
    for(int j=0;j<9;++j)oscAtt[j]=std::make_unique<SA>(p.state,oid(selected,o,oscKeys[j]),oscKnobs[j]);
    modeAtt=std::make_unique<CA>(p.state,oid(selected,o,"mode"),oscMode);
    oscWaveAtt=std::make_unique<CA>(p.state,o==0?id(selected,"wave"):oid(selected,o,"wave"),oscWave);
    syncAtt=std::make_unique<BA>(p.state,oid(selected,o,"sync"),oscSync);ringAtt=std::make_unique<BA>(p.state,oid(selected,o,"ring"),oscRing);
    for(int j=0;j<5;++j)oscTabs[j].setToggleState(j==o,juce::dontSendNotification);
    timerCallback();
  }
  void importSample(){
    const int k=selected,o=selectedOsc;
    chooser=std::make_unique<juce::FileChooser>("Importer une source audio",juce::File{},"*.wav;*.aif;*.aiff");
    auto safe=juce::Component::SafePointer<Editor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,k,o](const juce::FileChooser& f){
      if(auto* e=safe.getComponent()){
        auto file=f.getResult();if(file==juce::File{})return;
        auto error=e->p.loadSample(k,file);
        if(error.isNotEmpty()){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Omniscience",error);return;}
        if(auto* par=e->p.state.getParameter(oid(k,o,"mode")))par->setValueNotifyingHost(par->convertTo0to1(4.f));
        if(auto* par=e->p.state.getParameter(oid(k,o,"level")))par->setValueNotifyingHost(par->convertTo0to1(1.f));
        e->timerCallback();
      }
    });
  }
  void add(juce::Component& c){canvas.addAndMakeVisible(c);}
  void label(juce::Label& l,juce::String text,float size){l.setText(text,juce::dontSendNotification);l.setFont(juce::Font(juce::FontOptions(size)));l.setColour(juce::Label::textColourId,light);l.setJustificationType(juce::Justification::centred);add(l);}
  void configure(juce::Slider& s){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,85,22);add(s);}
  void filter(){visible.clear();auto q=search.getText();for(int i=0;i<100;++i)if(presetName(i).containsIgnoreCase(q))visible.push_back(i);browser.deselectAllRows();browser.updateContent();}
  int getNumRows()override{return int(visible.size());}
  void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool sel)override{if(row<0||row>=int(visible.size()))return;g.fillAll(sel?juce::Colour(0xff294b67):(row%2?ink:juce::Colour(0xff17202d)));g.setColour(light);g.setFont(14);g.drawText(presetName(visible[size_t(row)]),8,0,w-12,h,juce::Justification::centredLeft,true);}
  void selectedRowsChanged(int row)override{if(row>=0&&row<int(visible.size()))p.setCurrentProgram(visible[size_t(row)]);}
  void timerCallback()override{
    patchLabel.setText(p.getProgramName(p.getCurrentProgram()),juce::dontSendNotification);
    if(oscPage){auto patch=p.patch();auto& l=patch.layer[selected];preview.update(l.osc[selectedOsc],l.sample);sampleLabel.setText(p.sampleName(selected),juce::dontSendNotification);}
  }
  void selectLayer(int k){
    selected=k;for(auto& a:att)a.reset();waveAtt.reset();enabledAtt.reset();
    for(int j=0;j<8;++j)att[j]=std::make_unique<SA>(p.state,id(k,keys[j]),knobs[j]);
    waveAtt=std::make_unique<CA>(p.state,id(k,"wave"),wave);enabledAtt=std::make_unique<BA>(p.state,id(k,"enabled"),enabled);
    for(int j=0;j<4;++j)tabs[j].setToggleState(j==k,juce::dontSendNotification);
    layerTitle.setText("ÉDITION DE LA COUCHE "+juce::String::charToString(juce::juce_wchar('A'+k)),juce::dontSendNotification);
    if(oscPage)bindOsc(selectedOsc);
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
    oscMode.addItemList({"Analogique","Wavetable HD","FM / PM","AM","Sample"},1);add(oscMode);
    oscWave.addItemList({"Sinus","Dent de scie","Carrée","Triangle"},1);add(oscWave);
    importButton.onClick=[this]{importSample();};add(importButton);label(sampleLabel,"Aucun sample",11);add(preview);
    for(int j=0;j<9;++j){configure(oscKnobs[j]);label(oscLabels[j],juce::String::fromUTF8(oscNames[j]),13);}
    add(oscSync);add(oscRing);selectLayer(0);setOscPage(false);timerCallback();startTimerHz(5);
    setResizable(true,true);setResizeLimits(900,570,1800,1140);if(auto* c=getConstrainer())c->setFixedAspectRatio(1200./760.);setSize(1200,760);
  }
  ~Editor()override{stopTimer();browser.setModel(nullptr);setLookAndFeel(nullptr);}
  void paint(juce::Graphics& g)override{
    g.fillAll(ink);auto sc=float(getWidth())/1200.f;g.addTransform(juce::AffineTransform::scale(sc));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff253d62),0,0,ink,1200,142,false));g.fillRect(0,0,1200,138);
    g.setColour(cyan);g.setFont(juce::Font(juce::FontOptions(14.f)));g.drawText("LIBERTY MUSIC SOFTWARE",22,13,260,24,juce::Justification::centredLeft);
    g.setColour(light);g.setFont(juce::Font(juce::FontOptions(38.f)));g.drawText("O M N I S C I E N C E",286,19,730,52,juce::Justification::centred);
    g.setColour(juce::Colour(0xff99b9d3));g.setFont(14);g.drawText("COSMOS ENGINE  /  HYBRID SYNTHESIZER",286,72,730,25,juce::Justification::centred);
    g.setColour(panel);g.fillRoundedRectangle(14,150,266,580,8);g.fillRoundedRectangle(292,216,894,514,8);
    g.setColour(cyan);g.setFont(17);g.drawText("BIBLIOTHÈQUE  /  100",25,162,242,28,juce::Justification::centred);
    g.setColour(juce::Colour(0xff35495e));if(!oscPage)for(int j=0;j<8;++j)g.drawRoundedRectangle(310.f+216.f*(j%4),316.f+180.f*(j/4),204.f,168.f,6.f,1.f);
    if(oscPage)for(int j=0;j<10;++j)g.drawRoundedRectangle(310.f+172.f*(j%5),449.f+132.f*(j/5),164.f,126.f,6.f,1.f);
    g.setColour(juce::Colour(0xff9ab6ce));g.setFont(13);g.drawText("v0.2  HYBRID SYNTHESIS  •  4 couches / 20 oscillateurs / 32 voix",18,733,1155,24,juce::Justification::centredRight);
  }
  void resized()override{
    canvas.setTransform(juce::AffineTransform::scale(float(getWidth())/1200.f));canvas.setBounds(0,0,1200,760);
    search.setBounds(26,199,242,30);browser.setBounds(26,239,242,478);patchLabel.setBounds(290,101,730,32);
    masterLabel.setBounds(1040,8,140,20);master.setBounds(1045,30,130,102);
    for(int k=0;k<4;++k)tabs[k].setBounds(292+225*k,151,219,52);
    layerTitle.setBounds(312,227,440,32);wave.setBounds(312,270,350,32);enabled.setBounds(704,270,220,32);
    for(int j=0;j<8;++j){int x=310+216*(j%4),y=316+180*(j/4);labels[j].setBounds(x+4,y+5,196,24);knobs[j].setBounds(x+20,y+31,164,130);}
    oscPageButton.setBounds(945,230,220,30);
    for(int o=0;o<5;++o)oscTabs[o].setBounds(310+172*o,270,164,30);
    oscMode.setBounds(310,310,245,30);oscWave.setBounds(565,310,245,30);importButton.setBounds(820,310,345,30);
    sampleLabel.setBounds(310,342,855,20);preview.setBounds(310,365,855,74);
    for(int j=0;j<9;++j){int x=310+172*(j%5),y=449+132*(j/5);oscLabels[j].setBounds(x+3,y+2,158,20);oscKnobs[j].setBounds(x+5,y+22,154,100);}
    oscSync.setBounds(1010,598,145,33);oscRing.setBounds(1010,640,145,33);
  }
};
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Processor();}
