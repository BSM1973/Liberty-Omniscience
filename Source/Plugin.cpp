#include <juce_audio_utils/juce_audio_utils.h>
#include "CosmosEngine.h"
#include "CosmosMotion.h"
#include "CosmosTrajectory.h"
#include "CosmosMutation.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>

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
juce::String presetName(int i){if(i>=270)return juce::String("Mutation / ")+categories[size_t((i-270)/10)]+" "+adjectives[size_t(i%10)];if(i>=200)return "Filtres / "+filterNames[i-198];if(i>=100)return juce::String("Granular / ")+categories[size_t((i-100)/10)]+" "+adjectives[size_t(i%10)];return juce::String(categories[size_t(i/10)])+" / "+adjectives[size_t(i%10)];}
const juce::StringArray lfoShapes={"Sinus","Triangle","Dent montante","Dent descendante","Carrée","Sample & Hold","Exponentielle","Logarithmique","S & H lissé","Aléatoire progressif","Cosinus relevé","Escalier 8"};
const juce::StringArray lfoDest={"Aucune","Fréquence filtre A","Fréquence filtre B","Amplitude","Panoramique","Accordage","Morphing Wavetable","Position granulaire","Densité granulaire"};
const juce::StringArray lfoLayers={"Toutes","Couche A","Couche B","Couche C","Couche D"};
const juce::StringArray lfoDivisions={"Libre (Hz)","1 mesure (4/4)","Blanche","Noire","Croche","Double croche","Triolet de croche","Croche pointée"};
const juce::StringArray lfoPolarities={"Bipolaire","Unipolaire"},lfoModes={"Redéclenchement","Free Run","Transport"};
const std::array<const char*,9> lfoKeys={"shape","destination","layer","division","rate","depth","phase","polarity","mode"};
juce::String lid(int i,const char* k){return "lfo"+juce::String(i+1)+"_"+k;}
const juce::StringArray envModes={"ADSR","AHDSR","Multisegment 5"},envPlay={"Maintien","Boucle","One Shot"},envCurves={"Linéaire","Exponentielle","Logarithmique"},envSync={"Secondes","Temps (BPM)"};
const std::array<const char*,15> envKeys={"mode","play","curve","sync","destination","layer","attack","hold","decay","sustain","release","segmentTime","segmentLevel","depth","velocity"};
juce::String freeKey(int j){if(j==0)return "freeEnabled";if(j==1)return "freeCount";return juce::String(j<18?"freeX":j<34?"freeY":"freeCurve")+juce::String((j-2)%16);}
juce::String eid(int i,const char* k){return "modenv"+juce::String(i+1)+"_"+k;}
const juce::StringArray matrixSources=[](){juce::StringArray a{"Aucune"};for(int i=1;i<=8;++i)a.add("LFO "+juce::String(i));for(int i=1;i<=12;++i)a.add("ENV "+juce::String(i));a.addArray({"Vélocité","Suivi clavier","Molette (CC1)","Aftertouch canal","Aftertouch polyphonique","Pitch Bend"});for(int i=0;i<128;++i)a.add("MIDI CC "+juce::String(i));for(int i=1;i<=8;++i)a.add("Macro "+juce::String(i));a.add("NEXUS X");a.add("NEXUS Y");return a;}();
const juce::StringArray matrixDests=[](){auto a=lfoDest;a.addArray({"Profondeur FM / AM","Ratio FM / AM","Résonance A","Résonance B"});for(int i=1;i<=8;++i)a.add("Profondeur LFO "+juce::String(i));return a;}();
const juce::StringArray matrixOscs={"Tous","OSC 1","OSC 2","OSC 3","OSC 4","OSC 5"},matrixCurves={"Linéaire","Quadratique","Racine","Seuil","Personnalisée"};
const std::array<const char*,14> matrixKeys={"enabled","source","destination","layer","polarity","amount","oscillator","curve","smoothing","point0","point1","point2","point3","point4"};
juce::String macroId(int i){return "macro"+juce::String(i+1);}
juce::String mid(int i,const char* k){return "matrix"+juce::String(i+1)+"_"+k;}
const std::array<const char*,14> nexusKeys={"nexusEnabled","nexusX","nexusY","nexusPower","nexusSmooth","nexusLockX","nexusLockY","nexusCenter","nexusInvertX","nexusInvertY","nexusMacroX","nexusMacroY","nexusCCX","nexusCCY"};
const juce::StringArray nexusMacroChoices={"Manuel / MIDI","Macro 1","Macro 2","Macro 3","Macro 4","Macro 5","Macro 6","Macro 7","Macro 8"};
const std::array<const char*,10> morphKeys={"morphEnabled","morphAmount","morphCurve","morphLayer","morphOsc","morphGrain","morphFilter","morphEnv","morphProtectTune","morphProtectGain"};
const juce::StringArray morphCurves={"Linéaire","Quadratique","Racine"};
const std::array<const char*,8> motionKeys={"motionPlay","motionSync","motionBars","motionMode","motionSpeed","motionIntensity","motionRotate","motionFreeSeconds"};
const juce::StringArray motionModes={"One Shot","Loop","Reverse","Ping-Pong"},motionBars={"1","2","4","8","16","32"};
const std::array<const char*,8> trajectoryKeys={"trajEnabled","trajScaleX","trajScaleY","trajCenterX","trajCenterY","trajEase","trajJitter","trajMacro"};
const std::array<const char*,7> qcKeys={"snapshotXY","snapshotCornerA","snapshotCornerB","snapshotCornerC","snapshotCornerD","motionOverdub","motionQuantize"};
const std::array<const char*,5> snapshotKeys={"snapshotTime","snapshotProtectTune","snapshotProtectGain","snapshotMidi","snapshotMacro"};
const juce::StringArray mutationModes={"Subtle","Musical","Organic","Cinematic","Experimental","Chaos","Custom"};
const std::array<const char*,13> mutationKeys={"mutationMode","mutationAmount","mutationSeed","mutationTune","mutationGain","mutationOsc","mutationWT","mutationFM","mutationGrain","mutationFilter","mutationEnv","mutationLFO","mutationMatrix"};
const juce::StringArray fxNames={"Aucun","Gain","Saturation douce","Clipping","Passe-bas","Passe-haut","Bitcrusher","Largeur stéréo","Trémolo","VCA","FET","Optique","Vari-Mu","Multibande","Limiteur","Expandeur","Noise Gate","Transient Shaper","De-Esser","Lampes","Bandes","Transistors","Soft Clipper","Overdrive","Asymétrique","Fuzz","Waveshaper","Wavefolder","Rectification","Chorus analogique","Chorus stéréo","Ensemble Chorus","Dimension Chorus","Flanger analogique","Through-Zero Flanger","Phaser 4 pôles","Phaser 8 pôles","Phaser 12 pôles","Uni-Vibe","Rotary Speaker","Vibrato","Auto Pan","Ring Modulator","Frequency Shifter","Doppler","Stereo Enhancer","Haas Spatializer","Auto Wah","Resonator","Digital Delay","Analog Delay","Tape Echo","Ping-Pong Delay","Stereo Dual Delay","Multitap Delay","Reverse Delay","Ducking Delay","Diffusion Delay","Granular Delay","Shimmer Delay","Modulated Delay","Spectral Delay","Filter Delay","Space Echo","Room Reverb","Studio Reverb","Chamber Reverb","Hall Reverb","Concert Hall","Cathedral Reverb","Plate Reverb","Spring Reverb","Ambient Reverb","Shimmer Reverb","Reverse Reverb","Gated Reverb","Modulated Reverb","Granular Reverb","Infinite Reverb","Spectral Freeze","Spectral Blur","Spectral Morph","Spectral Shifter","Spectral Gate","Spectral Compressor","Pitch Shifter","Harmonizer","Octaver","Formant Shifter","Vocoder","Granular Processor","Time Stretcher","Stutter","Glitch Processor","Beat Repeat","Comb Filter","Resonator Bank","Ring Resonator","Stereo Imager","Transient Designer Pro","Cosmos Multi-FX"};
const std::array<const char*,17> fxKeys={"type","enabled","mix","input","output","a","b","macro","depth","autoGain","quality","cc","sync","stereo","delayFeedbackR","delayTone","delayFreeze"};
juce::String fxid(int rack,int slot,const char* key){return "fx"+juce::String(rack+1)+"_"+juce::String(slot+1)+"_"+key;}
juce::String fxroute(int rack){return "fxroute"+juce::String(rack+1);}
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
    p.add(std::make_unique<B>(ID{eid(i,"freeEnabled"),1},eid(i,"freeEnabled"),false));p.add(std::make_unique<F>(ID{eid(i,"freeCount"),1},eid(i,"freeCount"),juce::NormalisableRange<float>(2,16,1),4));cosmos::FreeEnvelope defaults;for(int j=0;j<16;++j){auto x=freeKey(2+j),y=freeKey(18+j),c=freeKey(34+j);add(x.toRawUTF8(),0,1,defaults.x[j]);add(y.toRawUTF8(),0,1,defaults.y[j]);p.add(std::make_unique<F>(ID{eid(i,c.toRawUTF8()),1},eid(i,c.toRawUTF8()),juce::NormalisableRange<float>(0,2,1),0));}
  }
  for(int i=0;i<cosmos::matrixCount;++i){
    auto choice=[&](const char* key,const juce::StringArray& a){p.add(std::make_unique<C>(ID{mid(i,key),1},mid(i,key),a,0));};
    p.add(std::make_unique<B>(ID{mid(i,"enabled"),1},mid(i,"enabled"),false));choice("source",matrixSources);choice("destination",matrixDests);choice("layer",lfoLayers);choice("polarity",juce::StringArray{"Unipolaire","Bipolaire"});
    p.add(std::make_unique<F>(ID{mid(i,"amount"),1},mid(i,"amount"),-1.f,1.f,0.f));choice("oscillator",matrixOscs);choice("curve",matrixCurves);p.add(std::make_unique<F>(ID{mid(i,"smoothing"),1},mid(i,"smoothing"),0.f,200.f,0.f));for(int j=0;j<5;++j)p.add(std::make_unique<F>(ID{mid(i,matrixKeys[9+j]),1},mid(i,matrixKeys[9+j]),0.f,1.f,j*.25f));
  }
  for(int i=0;i<8;++i)p.add(std::make_unique<F>(ID{macroId(i),1},macroId(i),0.f,1.f,0.f));
  for(int j:{0,3,5,6,7,8,9})p.add(std::make_unique<B>(ID{nexusKeys[j],1},nexusKeys[j],false));
  for(int j:{1,2})p.add(std::make_unique<F>(ID{nexusKeys[j],1},nexusKeys[j],0.f,1.f,.5f));
  p.add(std::make_unique<F>(ID{nexusKeys[4],1},nexusKeys[4],0.f,500.f,30.f));
  for(int j:{10,11})p.add(std::make_unique<C>(ID{nexusKeys[j],1},nexusKeys[j],nexusMacroChoices,0));
  for(int j:{12,13})p.add(std::make_unique<F>(ID{nexusKeys[j],1},nexusKeys[j],juce::NormalisableRange<float>(-1,127,1),-1));
  p.add(std::make_unique<B>(ID{morphKeys[0],1},morphKeys[0],false));p.add(std::make_unique<F>(ID{morphKeys[1],1},morphKeys[1],0.f,1.f,1.f));p.add(std::make_unique<C>(ID{morphKeys[2],1},morphKeys[2],morphCurves,0));for(int j=3;j<10;++j)p.add(std::make_unique<B>(ID{morphKeys[j],1},morphKeys[j],true));
  p.add(std::make_unique<B>(ID{motionKeys[0],1},"Motion lecture",false));p.add(std::make_unique<B>(ID{motionKeys[1],1},"Motion synchro",true));p.add(std::make_unique<C>(ID{motionKeys[2],1},"Motion mesures",motionBars,2));p.add(std::make_unique<C>(ID{motionKeys[3],1},"Motion mode",motionModes,1));p.add(std::make_unique<F>(ID{motionKeys[4],1},"Motion vitesse",.25f,4.f,1.f));p.add(std::make_unique<F>(ID{motionKeys[5],1},"Motion intensité",0.f,1.f,1.f));p.add(std::make_unique<F>(ID{motionKeys[6],1},"Motion rotation",-180.f,180.f,0.f));p.add(std::make_unique<F>(ID{motionKeys[7],1},"Motion secondes",1.f,120.f,8.f));
  p.add(std::make_unique<B>(ID{trajectoryKeys[0],1},"Trajectoire active",false));
  for(int j=1;j<=2;++j)p.add(std::make_unique<F>(ID{trajectoryKeys[j],1},trajectoryKeys[j],0.f,2.f,1.f));
  for(int j=3;j<=4;++j)p.add(std::make_unique<F>(ID{trajectoryKeys[j],1},trajectoryKeys[j],0.f,1.f,.5f));
  p.add(std::make_unique<C>(ID{trajectoryKeys[5],1},"Trajectoire vitesse",juce::StringArray{"Constante","Accélérer","Ralentir","Lissée"},0));
  p.add(std::make_unique<F>(ID{trajectoryKeys[6],1},"Trajectoire variation",0.f,1.f,0.f));p.add(std::make_unique<C>(ID{trajectoryKeys[7],1},"Trajectoire macro",nexusMacroChoices,0));
  p.add(std::make_unique<F>(ID{snapshotKeys[0],1},"Transition snapshot (s)",juce::NormalisableRange<float>(.01f,60.f,0.f,.3f),.01f));
  for(int j=1;j<=3;++j)p.add(std::make_unique<B>(ID{snapshotKeys[j],1},snapshotKeys[j],j!=3));
  p.add(std::make_unique<C>(ID{snapshotKeys[4],1},"Macro snapshots",nexusMacroChoices,0));
  p.add(std::make_unique<C>(ID{mutationKeys[0],1},"Mutation mode",mutationModes,1));p.add(std::make_unique<F>(ID{mutationKeys[1],1},"Mutation intensité",0.f,1.f,.35f));p.add(std::make_unique<F>(ID{mutationKeys[2],1},"Mutation graine",juce::NormalisableRange<float>(1.f,16777215.f,1.f),12345.f));for(int j=3;j<13;++j)p.add(std::make_unique<B>(ID{mutationKeys[j],1},mutationKeys[j],true));
  p.add(std::make_unique<B>(ID{qcKeys[0],1},"Morphing snapshots XY",false));
  juce::StringArray corners{"Vide"};for(int j=1;j<=16;++j)corners.add("S"+juce::String(j));
  for(int j=1;j<=4;++j)p.add(std::make_unique<C>(ID{qcKeys[j],1},qcKeys[j],corners,j));
  p.add(std::make_unique<B>(ID{qcKeys[5],1},"Motion overdub",false));
  p.add(std::make_unique<C>(ID{qcKeys[6],1},"Motion quantification",juce::StringArray{"Libre","1/4","1/8","1/16","1/32"},0));
  for(int k=0;k<cosmos::fxRacks;++k){p.add(std::make_unique<B>(ID{fxroute(k),1},"FX parallèle",false));for(int j=0;j<cosmos::fxSlots;++j){auto key=[&](int i){return ID{fxid(k,j,fxKeys[i]),1};};p.add(std::make_unique<C>(key(0),"FX type",fxNames,0));p.add(std::make_unique<B>(key(1),"FX actif",false));p.add(std::make_unique<F>(key(2),"FX Dry/Wet",0.f,1.f,1.f));p.add(std::make_unique<F>(key(3),"FX entrée",0.f,2.f,1.f));p.add(std::make_unique<F>(key(4),"FX sortie",0.f,2.f,1.f));p.add(std::make_unique<F>(key(5),"FX A",0.f,1.f,.5f));p.add(std::make_unique<F>(key(6),"FX B",0.f,1.f,.5f));p.add(std::make_unique<C>(key(7),"FX macro",nexusMacroChoices,0));p.add(std::make_unique<F>(key(8),"FX profondeur",-1.f,1.f,0.f));p.add(std::make_unique<B>(key(9),"FX compensation",false));p.add(std::make_unique<C>(key(10),"FX sous-échantillons",juce::StringArray{"1x","2x","4x"},0));p.add(std::make_unique<F>(key(11),"FX CC",juce::NormalisableRange<float>(-1.f,127.f,1.f),-1.f));p.add(std::make_unique<B>(key(12),"FX tempo",false));p.add(std::make_unique<F>(key(13),"FX phase stéréo",0.f,1.f,.5f));p.add(std::make_unique<F>(key(14),"Feedback droit",0.f,1.f,.5f));p.add(std::make_unique<F>(key(15),"Filtre feedback",0.f,1.f,.75f));p.add(std::make_unique<B>(key(16),"Freeze délai",false));}}
  return p;
}
class Processor final:public juce::AudioProcessor,private juce::Timer {
public:
  State state{*this,nullptr,"OMNISCIENCE",layout()};
  cosmos::Engine engine;
  std::array<std::array<std::array<std::atomic<float>*,17>,8>,5> fxParams{};std::array<std::array<std::atomic<float>,8>,5> fxPhaseView{};std::array<std::array<std::atomic<float>,8>,5> fxReductionView{};std::array<std::atomic<float>,128> fxCCValue{};std::atomic<int> fxLearn{-1},fxLearnResult{-1};std::array<std::atomic<float>*,5> fxRoute{};std::array<std::atomic<bool>,cosmos::fxTypes> fxFav{};
  void resetFX(){fxLearn.store(-1);fxLearnResult.store(-1);for(auto& v:fxCCValue)v.store(-1);for(int k=0;k<5;++k){if(auto* par=state.getParameter(fxroute(k)))par->setValueNotifyingHost(par->getDefaultValue());for(int j=0;j<8;++j)for(auto key:fxKeys)if(auto* par=state.getParameter(fxid(k,j,key)))par->setValueNotifyingHost(par->getDefaultValue());}}
  void swapFX(int k,int a,int b){if(k<0||k>=5||a<0||a>=8||b<0||b>=8||a==b)return;for(auto key:fxKeys){auto* x=state.getParameter(fxid(k,a,key));auto* y=state.getParameter(fxid(k,b,key));if(x&&y){float u=x->getValue(),v=y->getValue();x->setValueNotifyingHost(v);y->setValueNotifyingHost(u);}}}

  std::array<std::atomic<float>*,5> snapshotParams{};
  std::array<std::atomic<float>*,7> qcParams{};
  std::atomic<int> bankRevision{0};int cornerRevision=-1;std::array<int,4> cornerIds{-1,-1,-1,-1};
  struct CornerValue{juce::RangedAudioParameter* parameter=nullptr;std::array<float,4> v{};};
  std::vector<CornerValue> cornerPlan;float lastCornerX=-1,lastCornerY=-1;bool cornerTune=false,cornerGain=false;
  void updateSnapshotCorners(){
    std::array<int,4> ids;for(int j=0;j<4;++j)ids[j]=int(qcParams[j+1]->load())-1;
    bool tune=snapshotParams[1]->load()>.5f,gain=snapshotParams[2]->load()>.5f;
    int revision=bankRevision.load();
    if(revision!=cornerRevision||ids!=cornerIds||tune!=cornerTune||gain!=cornerGain){
      cornerRevision=revision;cornerIds=ids;cornerTune=tune;cornerGain=gain;cornerPlan.clear();lastCornerX=lastCornerY=-1;
      std::array<juce::ValueTree,4> slots;for(int j=0;j<4;++j){slots[j]=snapshotSlot(ids[j]);if(!slots[j].isValid())return;}
      std::array<std::map<juce::String,float>,4> values;for(int j=0;j<4;++j)for(auto v:slots[j]){float f=float(v.getProperty("value",0));if(std::isfinite(f))values[j][v.getProperty("id").toString()]=std::clamp(f,0.f,1.f);}
      for(const auto& item:values[0]){const auto& key=item.first;auto* par=state.getParameter(key);if(!par||par->isDiscrete()||key.startsWith("snapshot")||key.startsWith("nexus")||key.startsWith("motion")||key.startsWith("traj")||key.startsWith("morph")||key.startsWith("mutation")||key.startsWith("macro"))continue;
        if(tune&&(key.endsWithIgnoreCase("tune")||key.endsWithIgnoreCase("detune")||key.endsWithIgnoreCase("drift")||key.endsWith("gPitch")))continue;
        if(gain&&(key=="master"||key.endsWithIgnoreCase("gain")||key.endsWithIgnoreCase("level")||key.endsWithIgnoreCase("fTrim")))continue;
        if(mutationFamily(key)<0||!values[1].count(key)||!values[2].count(key)||!values[3].count(key))continue;
        cornerPlan.push_back({par,{item.second,values[1][key],values[2][key],values[3][key]}});
      }
    }
    float x=nexusView[0].load(),y=nexusView[1].load();if(std::abs(x-lastCornerX)<.0005f&&std::abs(y-lastCornerY)<.0005f)return;
    lastCornerX=x;lastCornerY=y;auto w=cosmos::nexusWeights(x,y,false);for(auto& c:cornerPlan){float v=0;for(int j=0;j<4;++j)v+=w[j]*c.v[j];v=std::clamp(v,0.f,1.f);if(std::abs(c.parameter->getValue()-v)>1e-6f)c.parameter->setValueNotifyingHost(v);}
  }

  std::array<std::atomic<float>*,13> mutationParams{};
  std::atomic<int> mutationEpoch{0};
  struct MutationItem{juce::String key;float before=0,after=0;};
  using MutationBatch=std::vector<MutationItem>;
  static int mutationFamily(const juce::String& key){
    if(key.startsWith("matrix"))return key.endsWith("_amount")?7:-1;
    if(key.startsWith("lfo"))return key.endsWith("_rate")||key.endsWith("_depth")||key.endsWith("_phase")?6:-1;
    if(key.startsWith("modenv"))return key.endsWith("_attack")||key.endsWith("_hold")||key.endsWith("_decay")||key.endsWith("_sustain")||key.endsWith("_release")||key.endsWith("_segmentTime")||key.endsWith("_segmentLevel")||key.endsWith("_depth")||key.endsWith("_velocity")?5:-1;
    if(key.length()<3||key[0]<'A'||key[0]>'D'||key[1]!='_')return -1;
    auto tail=key.fromFirstOccurrenceOf("_",false,false);
    if(tail.startsWith("osc")){auto field=tail.fromFirstOccurrenceOf("_",false,false);if(field.startsWith("g")&&field!="gain")return field=="gSize"||field=="gDensity"||field=="gPosition"||field=="gSpread"||field=="gStretch"||field=="gPitch"||field=="gBlur"?3:-1;if(field=="morph")return 1;if(field=="ratio"||field=="depth")return 2;return field=="level"||field=="tune"||field=="detune"||field=="drift"||field=="drive"?0:-1;}
    if(tail=="gain"||tail=="pan"||tail=="tune")return 0;
    if(tail=="attack"||tail=="decay"||tail=="sustain"||tail=="release"||tail=="fAttack"||tail=="fDecay"||tail=="fSustain"||tail=="fRelease")return 5;
    return tail=="cutoff"||tail=="f2Cutoff"||tail=="fRes"||tail=="f2Res"||tail=="fDrive"||tail=="f2Drive"||tail=="fMorph"||tail=="f2Morph"||tail=="fMix"||tail=="fTrim"?4:-1;
  }
  MutationBatch makeMutation(int mode,float amount,std::uint32_t seed,const std::array<bool,8>& family,bool tune,bool gain,bool factory=false){
    MutationBatch batch;cosmos::MutationRng rng(seed);auto original=patch();
    bool routedPitchOrGain=false;for(const auto& m:original.matrix)if(m.enabled&&((tune&&m.destination==5)||(gain&&m.destination==3)))routedPitchOrGain=true;
    for(auto* raw:getParameters())if(auto* par=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw)){
      auto key=par->paramID;int f=mutationFamily(key);if(f<0||!family[f]||raw->isDiscrete())continue;
      if(tune&&(key.endsWithIgnoreCase("tune")||key.endsWithIgnoreCase("detune")||key.endsWithIgnoreCase("drift")||key.endsWith("gPitch")||f==2))continue;
      if(gain&&(key.endsWithIgnoreCase("gain")||key.endsWithIgnoreCase("level")||key.endsWith("fTrim")||key.endsWith("Drive")||key.endsWith("_drive")))continue;
      if(f==7){int row=key.fromFirstOccurrenceOf("matrix",false,false).getIntValue()-1;if(row<0||row>=64)continue;auto m=original.matrix[row];if(!m.enabled||(tune&&m.destination==5)||(gain&&m.destination==3))continue;}
      if(f==6||key.startsWith("modenv")){if(routedPitchOrGain)continue;int row=key.fromFirstOccurrenceOf(f==6?"lfo":"modenv",false,false).getIntValue()-1;if(row<0||row>=(f==6?8:12))continue;int dest=f==6?original.lfo[row].destination:original.modEnv[row].destination;if((tune&&dest==5)||(gain&&dest==3))continue;}
      float before=raw->getValue();float after=cosmos::mutationValue(before,amount,mode,f,rng);
      if(factory&&key.endsWithIgnoreCase("Res"))after=std::min(after,.55f);
      if(before!=after)batch.push_back({key,before,after});
    }return batch;
  }
  void applyMutation(const MutationBatch& batch,bool forward){for(const auto& item:batch)if(auto* par=state.getParameter(item.key)){par->beginChangeGesture();par->setValueNotifyingHost(forward?item.after:item.before);par->endChangeGesture();}}

  mutable std::mutex snapshotMutex,trajectoryMutex;
  juce::ValueTree snapshotBank{"COSMOS_SNAPSHOTS"};
  cosmos::Trajectory savedTrajectory;
  std::atomic<int> trajectoryRevision{0},snapshotRequest{-1},snapshotEpoch{0},snapshotActive{-1};
  std::atomic<float> snapshotProgress{0};
  int seenSnapshotEpoch=-1,lastMacroSlot=-1,lastMacroSource=0;
  struct SnapshotChange{juce::AudioProcessorParameter* parameter=nullptr;float from=0,to=0;};
  std::vector<SnapshotChange> snapshotChanges;
  double snapshotStarted=0,snapshotDuration=0;
  cosmos::Trajectory readTrajectory()const{std::lock_guard<std::mutex> lock(trajectoryMutex);return savedTrajectory;}
  void saveTrajectory(const cosmos::Trajectory& t){{std::lock_guard<std::mutex> lock(trajectoryMutex);savedTrajectory=t;}trajectoryRevision.fetch_add(1);}
  std::pair<juce::String,int> snapshotMetadata(int i)const{std::lock_guard<std::mutex> lock(snapshotMutex);for(auto c:snapshotBank)if(int(c.getProperty("slot",-1))==i)return {c.getProperty("name","Mémoire").toString(),std::clamp(int(c.getProperty("colour",0)),0,5)};return {"Vide",-1};}
  juce::ValueTree snapshotSlot(int i)const{std::lock_guard<std::mutex> lock(snapshotMutex);for(auto c:snapshotBank)if(int(c.getProperty("slot",-1))==i)return c.createCopy();return {};}
  void putSnapshot(int i,juce::ValueTree t){if(i<0||i>=16)return;std::lock_guard<std::mutex> lock(snapshotMutex);for(int j=snapshotBank.getNumChildren()-1;j>=0;--j)if(int(snapshotBank.getChild(j).getProperty("slot",-1))==i)snapshotBank.removeChild(j,nullptr);if(t.isValid()){t.setProperty("slot",i,nullptr);snapshotBank.addChild(t,-1,nullptr);}bankRevision.fetch_add(1);}
  bool captureSnapshot(int i,juce::String name,int colour){if(i<0||i>=16)return false;juce::ValueTree t("SNAPSHOT");t.setProperty("name",name.substring(0,40),nullptr);t.setProperty("colour",std::clamp(colour,0,5),nullptr);for(auto* raw:getParameters())if(auto* par=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw)){if(par->paramID.startsWith("snapshot"))continue;juce::ValueTree v("V");v.setProperty("id",par->paramID,nullptr);v.setProperty("value",par->getValue(),nullptr);t.addChild(v,-1,nullptr);}putSnapshot(i,t);return true;}
  void clearSnapshots(){snapshotRequest.store(-1);snapshotActive.store(-1);snapshotProgress.store(0);snapshotEpoch.fetch_add(1);std::lock_guard<std::mutex> lock(snapshotMutex);snapshotBank=juce::ValueTree("COSMOS_SNAPSHOTS");bankRevision.fetch_add(1);}
  bool startSnapshot(int i){auto t=snapshotSlot(i);if(!t.isValid())return false;snapshotChanges.clear();int macro=int(snapshotParams[4]->load());bool tune=snapshotParams[1]->load()>.5f,gain=snapshotParams[2]->load()>.5f;for(auto v:t){auto key=v.getProperty("id").toString();if(key.startsWith("snapshot")||(macro>0&&key==macroId(macro-1)))continue;if(tune&&(key.endsWithIgnoreCase("tune")||key.endsWithIgnoreCase("detune")||key.endsWithIgnoreCase("drift")))continue;if(gain&&(key=="master"||key.endsWithIgnoreCase("gain")||key.endsWithIgnoreCase("level")||key.endsWithIgnoreCase("fTrim")))continue;if(auto* par=state.getParameter(key)){float target=float(v.getProperty("value",par->getValue()));if(std::isfinite(target))snapshotChanges.push_back({par,par->getValue(),std::clamp(target,0.f,1.f)});}}
    snapshotActive.store(i);snapshotStarted=juce::Time::getMillisecondCounterHiRes();snapshotDuration=1000.*snapshotParams[0]->load();snapshotProgress.store(0);if(snapshotDuration<=10.1)advanceSnapshot(1);return true;}
  void advanceSnapshot(float t){for(auto& c:snapshotChanges){if(c.parameter->isDiscrete()&&t<1)continue;float v=std::lerp(c.from,c.to,t);if(c.parameter->getValue()!=v)c.parameter->setValueNotifyingHost(v);}snapshotProgress.store(t);if(t>=1)snapshotChanges.clear();}
  void timerCallback()override{
    int learned=fxLearnResult.exchange(-1);if(learned>=0&&learned<40*128){int k=(learned/128)/8,j=(learned/128)%8;if(auto* par=state.getParameter(fxid(k,j,"cc")))par->setValueNotifyingHost(par->convertTo0to1(float(learned%128)));}
    int epoch=snapshotEpoch.load();if(epoch!=seenSnapshotEpoch){seenSnapshotEpoch=epoch;snapshotChanges.clear();lastMacroSlot=-1;lastMacroSource=0;}
    if(qcParams[0]->load()>.5f){snapshotChanges.clear();snapshotRequest.store(-1);updateSnapshotCorners();return;}lastCornerX=lastCornerY=-1;
    int source=int(snapshotParams[4]->load());if(source>0&&source<=8){int slot=std::clamp(int(macroParams[source-1]->load()*16),0,15);if(source!=lastMacroSource||slot!=lastMacroSlot){snapshotRequest.store(slot);lastMacroSlot=slot;}}else lastMacroSlot=-1;lastMacroSource=source;
    int request=snapshotRequest.exchange(-1);if(request>=0)startSnapshot(request);
    if(!snapshotChanges.empty())advanceSnapshot(float(std::clamp((juce::Time::getMillisecondCounterHiRes()-snapshotStarted)/std::max(10.,snapshotDuration),0.,1.)));
  }

  std::array<std::atomic<float>*,8> motionParams{},trajectoryParams{};
  std::atomic<int> trajectoryFactory{0};std::atomic<bool> trajectoryClosed{true},trajectoryCurve{false};std::array<std::atomic<bool>,50> trajectoryFav{};
  // Atomic publication: UI/state readers never access the audio-owned recorder.
  std::array<std::atomic<float>,cosmos::motionPoints> motionX{},motionY{};
  std::atomic<int> motionCount{0},motionCommand{0},motionGeneration{0};std::atomic<double> motionDuration{4};std::atomic<bool> motionClipSync{true},motionRecording{false};std::atomic<float> motionPhaseView{0};
  cosmos::MotionRecorder recorder;cosmos::MotionClip playbackClip,overdubBase;bool recordingOverdub=false;int seenMotion=-1;double motionElapsed=0,motionOrigin=0;bool motionWasPlaying=false,motionHadTransport=false;
  void publishMotion(const cosmos::MotionClip& c){motionCount.store(0);for(int j=0;j<c.count;++j){motionX[j].store(c.x[j]);motionY[j].store(c.y[j]);}motionDuration.store(c.duration);motionClipSync.store(c.sync);motionCount.store(c.count);motionGeneration.fetch_add(1);}
  cosmos::MotionClip readMotion()const{cosmos::MotionClip c;c.count=std::clamp(motionCount.load(),0,cosmos::motionPoints);c.duration=motionDuration.load();c.sync=motionClipSync.load();for(int j=0;j<c.count;++j){c.x[j]=motionX[j].load();c.y[j]=motionY[j].load();}return c;}
  void resetMotion(){motionCommand.store(3);motionCount.store(0);motionGeneration.fetch_add(1);}
  void processMotion(cosmos::Patch& p,int samples){
    int cmd=motionCommand.exchange(0);double seconds=samples/std::max(8000.,getSampleRate()),beats=seconds*std::clamp(p.bpm,20.,400.)/60.;
    if(cmd==3){motionRecording.store(false);motionWasPlaying=false;motionElapsed=0;recorder.clip.count=0;}
    if(cmd==1){bool sync=motionParams[1]->load()>.5f;recordingOverdub=qcParams[5]->load()>.5f;overdubBase=recordingOverdub?readMotion():cosmos::MotionClip{};recordingOverdub&=overdubBase.count>1;recorder.start(recordingOverdub?overdubBase.duration:sync?4.*(1<<std::clamp(int(motionParams[2]->load()),0,5)):motionParams[7]->load(),recordingOverdub?overdubBase.sync:sync);motionRecording.store(true);}
    auto finish=[&]{if(recordingOverdub){auto c=overdubBase;for(int j=0;j<c.count;++j){float phase=float(j)/std::max(1,c.count-1);float pos=phase*(cosmos::motionPoints-1);if(pos>recorder.clip.count-1)continue;int a=int(pos),b=std::min(a+1,recorder.clip.count-1);c.x[j]=.5f*(c.x[j]+std::lerp(recorder.clip.x[a],recorder.clip.x[b],pos-a));c.y[j]=.5f*(c.y[j]+std::lerp(recorder.clip.y[a],recorder.clip.y[b],pos-a));}publishMotion(c);}else{recorder.finish();publishMotion(recorder.clip);}};
    if(cmd==2&&motionRecording.exchange(false))finish();
    if(motionRecording.load()){bool done=recorder.capture(p.nexus.x,p.nexus.y,recorder.clip.sync?beats:seconds);motionPhaseView.store(float(std::clamp(recorder.elapsed/recorder.clip.duration,0.,1.)));if(done){motionRecording.store(false);finish();}return;}
    int gen=motionGeneration.load();if(gen!=seenMotion){playbackClip=readMotion();seenMotion=gen;motionWasPlaying=false;}
    bool play=motionParams[0]->load()>.5f&&playbackClip.count>1;
    if(!play){motionWasPlaying=false;motionPhaseView.store(0);return;}
    bool host=playbackClip.sync&&p.transportValid;
    if(!motionWasPlaying){motionElapsed=0;motionOrigin=p.ppq;}
    else if(host&&!motionHadTransport)motionOrigin=p.ppq-motionElapsed;
    double elapsed=host?std::max(0.,p.ppq-motionOrigin):motionElapsed;
    double phase=elapsed*motionParams[4]->load()/std::max(.01,playbackClip.duration);
    int quant=int(qcParams[6]->load());if(quant>0&&playbackClip.sync){double steps=playbackClip.duration*(1<<std::clamp(quant-1,0,3));phase=std::floor(phase*steps+1e-9)/std::max(1.,steps);}
    int mode=int(motionParams[3]->load());double eval=phase;
    const bool shaped=trajectoryParams[0]->load()>.5f;
    if(shaped){double whole=std::floor(phase),f=phase-whole;if(mode==0&&phase>=1)f=1,whole=0;int ease=int(trajectoryParams[5]->load());if(ease==1)f*=f;else if(ease==2)f=std::sqrt(f);else if(ease==3)f=f*f*(3-2*f);eval=whole+f;}
    auto xy=cosmos::motionTransform(playbackClip.at(eval,mode),motionParams[5]->load(),motionParams[6]->load(),false,false);
    if(shaped){int macro=int(trajectoryParams[7]->load());float scale=macro>0&&macro<=8?p.macros[macro-1]:1.f;xy=cosmos::shapeTrajectory(xy,trajectoryParams[1]->load()*scale,trajectoryParams[2]->load()*scale,trajectoryParams[3]->load(),trajectoryParams[4]->load(),trajectoryParams[6]->load(),float(eval-std::floor(eval)));}
    p.nexus.x=xy[0];p.nexus.y=xy[1];p.nexus.macroX=p.nexus.macroY=0;p.nexus.ccX=p.nexus.ccY=-1;
    motionElapsed=elapsed+(playbackClip.sync?beats:seconds);motionWasPlaying=true;motionHadTransport=host;motionPhaseView.store(float(mode==0?std::clamp(phase,0.,1.):phase-std::floor(phase)));
  }

  std::array<std::array<std::atomic<float>*,10>,4> params{};
  std::atomic<float>* master=nullptr;std::array<std::atomic<float>*,14> nexusParams{};std::array<std::atomic<float>*,10> morphParams{};std::array<std::atomic<float>,2> nexusView{};std::array<std::atomic<float>,4> nexusGainView{};
  std::array<std::array<std::atomic<float>*,9>,cosmos::lfoCount> lfoParams{};
  std::array<std::array<std::atomic<float>*,15>,cosmos::modEnvCount> modEnvParams{};std::array<std::array<std::atomic<float>*,50>,cosmos::modEnvCount> freeEnvParams{};
  std::array<std::array<std::atomic<float>*,14>,cosmos::matrixCount> matrixParams{};
  std::array<std::atomic<float>*,8> macroParams{};std::array<std::atomic<float>,cosmos::matrixCount> matrixView{};std::array<std::atomic<float>,8> lfoPhaseView{};std::atomic<int> learnRow{-1},learnResult{-1};
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
    for(int i=0;i<cosmos::modEnvCount;++i)for(int j=0;j<50;++j)freeEnvParams[i][j]=state.getRawParameterValue(eid(i,freeKey(j).toRawUTF8()));
    for(int i=0;i<cosmos::matrixCount;++i)for(int j=0;j<14;++j)matrixParams[i][j]=state.getRawParameterValue(mid(i,matrixKeys[j]));
    for(int i=0;i<8;++i)macroParams[i]=state.getRawParameterValue(macroId(i));
    for(int j=0;j<14;++j)nexusParams[j]=state.getRawParameterValue(nexusKeys[j]);
    for(int j=0;j<10;++j)morphParams[j]=state.getRawParameterValue(morphKeys[j]);
    for(int j=0;j<8;++j){motionParams[j]=state.getRawParameterValue(motionKeys[j]);trajectoryParams[j]=state.getRawParameterValue(trajectoryKeys[j]);}
    for(int j=0;j<5;++j)snapshotParams[j]=state.getRawParameterValue(snapshotKeys[j]);for(int j=0;j<13;++j)mutationParams[j]=state.getRawParameterValue(mutationKeys[j]);
    for(int j=0;j<7;++j)qcParams[j]=state.getRawParameterValue(qcKeys[j]);
    for(int k=0;k<5;++k){fxRoute[k]=state.getRawParameterValue(fxroute(k));for(int j=0;j<8;++j)for(int i=0;i<17;++i)fxParams[k][j][i]=state.getRawParameterValue(fxid(k,j,fxKeys[i]));}
    for(auto& v:fxCCValue)v.store(-1);setCurrentProgram(0);startTimerHz(50);
  }
  ~Processor()override{stopTimer();}
  const juce::String getName() const override{return "Omniscience";}
  bool acceptsMidi()const override{return true;} bool producesMidi()const override{return false;} bool isMidiEffect()const override{return false;}
  double getTailLengthSeconds()const override{return 10;}
  bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
  void prepareToPlay(double sr,int)override{engine.prepare(sr);}
  void releaseResources()override{engine.reset();}
  bool referenceDSPForQA=false;
  cosmos::Patch patch() const {
    cosmos::Patch p;p.master=master->load();for(int k=0;k<5;++k){p.fx[k].parallel=fxRoute[k]->load()>.5f;for(int j=0;j<8;++j){const auto& x=fxParams[k][j];p.fx[k].slots[j]={int(x[0]->load()),x[1]->load()>.5f,x[2]->load(),x[3]->load(),x[4]->load(),x[5]->load(),x[6]->load(),int(x[7]->load()),x[8]->load(),x[9]->load()>.5f,int(x[10]->load()),int(x[11]->load()),-1};auto& f=p.fx[k].slots[j];if(f.cc>=0&&f.cc<128)f.ccValue=fxCCValue[f.cc].load();f.sync=x[12]->load()>.5f;f.stereo=x[13]->load();f.delayFeedbackR=x[14]->load();f.delayTone=x[15]->load();f.delayFreeze=x[16]->load()>.5f;}}p.morph.enabled=morphParams[0]->load()>.5f;p.morph.amount=morphParams[1]->load();p.morph.curve=int(morphParams[2]->load());for(int j=0;j<5;++j)p.morph.family[j]=morphParams[3+j]->load()>.5f;p.morph.protectTune=morphParams[8]->load()>.5f;p.morph.protectGain=morphParams[9]->load()>.5f;const auto& z=nexusParams;p.nexus={z[0]->load()>.5f,z[1]->load(),z[2]->load(),z[3]->load()>.5f,z[4]->load(),z[5]->load()>.5f,z[6]->load()>.5f,z[7]->load()>.5f,z[8]->load()>.5f,z[9]->load()>.5f,int(z[10]->load()),int(z[11]->load()),int(z[12]->load()),int(z[13]->load())};
    for(int i=0;i<cosmos::lfoCount;++i){const auto& x=lfoParams[i];p.lfo[i]={int(x[0]->load()),int(x[1]->load()),int(x[2]->load()),int(x[3]->load()),x[4]->load(),x[5]->load(),x[6]->load(),int(x[7]->load()),int(x[8]->load())};}
    for(int i=0;i<cosmos::modEnvCount;++i){const auto& x=modEnvParams[i];p.modEnv[i]={int(x[0]->load()),int(x[1]->load()),int(x[2]->load()),int(x[3]->load()),int(x[4]->load()),int(x[5]->load()),x[6]->load(),x[7]->load(),x[8]->load(),x[9]->load(),x[10]->load(),x[11]->load(),x[12]->load(),x[13]->load(),x[14]->load()};const auto& f=freeEnvParams[i];auto& free=p.modEnv[i].free;free.enabled=f[0]->load()>.5f;if(free.enabled){free.count=int(f[1]->load());for(int j=0;j<16;++j){free.x[j]=f[2+j]->load();free.y[j]=f[18+j]->load();free.curve[j]=int(f[34+j]->load());}}}
    for(int i=0;i<cosmos::matrixCount;++i){const auto& x=matrixParams[i];p.matrix[i]={x[0]->load()>.5f,int(x[1]->load()),int(x[2]->load()),int(x[3]->load()),int(x[4]->load()),x[5]->load(),int(x[6]->load()),int(x[7]->load()),x[8]->load()};for(int j=0;j<5;++j)p.matrix[i].points[j]=x[9+j]->load();}
    for(int i=0;i<8;++i)p.macros[i]=macroParams[i]->load();
    p.experimentalGrouped=true;p.experimentalTiled=true;p.experimentalInterpolation=1;
#if defined(_M_X64)
    p.experimentalInterpolation=2;
#endif
#if defined(__aarch64__)
    p.experimentalGrainWindow=true;p.experimentalGrainPrepared=true;p.experimentalAnalog=true;p.experimentalEnv=true;
#endif
    if(referenceDSPForQA){p.experimentalGrouped=false;p.experimentalTiled=false;p.experimentalInterpolation=0;p.experimentalGrainWindow=false;p.experimentalGrainPrepared=false;p.experimentalAnalog=false;p.experimentalEnv=false;p.experimentalMatrixFast=false;p.experimentalNexusShared=false;}
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
  int consumeLearn(){int learned=learnResult.exchange(-1);if(learned>=64*128&&learned<66*128){if(auto* par=state.getParameter(nexusKeys[12+learned/128-64])){par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(float(learned%128)));par->endChangeGesture();}return learned;}if(learned>=0)if(auto* par=state.getParameter(mid(learned/128,"source"))){par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(float(27+learned%128)));par->endChangeGesture();if(auto* enabled=state.getParameter(mid(learned/128,"enabled"))){enabled->beginChangeGesture();enabled->setValueNotifyingHost(1.f);enabled->endChangeGesture();}}return learned;}
  void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi)override{
    juce::ScopedNoDenormals nd;b.clear();if(b.getNumChannels()<2)return;
    auto p=patch();
    if(auto* ph=getPlayHead())if(auto position=ph->getPosition()){if(auto bpm=position->getBpm())p.bpm=*bpm;if(auto ppq=position->getPpqPosition()){p.ppq=*ppq;p.transportValid=position->getIsPlaying()&&std::isfinite(*ppq);}}
    for(auto& rack:p.fx)for(auto& f:rack.slots)f.bpm=p.bpm;
    processMotion(p,b.getNumSamples());
    auto* l=b.getWritePointer(0);auto* r=b.getWritePointer(1);int pos=0,n=b.getNumSamples();
    for(const auto meta:midi){
      int at=juce::jlimit(pos,n,meta.samplePosition);engine.render(l+pos,r+pos,at-pos,p);if(p.transportValid)p.ppq+=(at-pos)*std::clamp(p.bpm,20.,400.)/(60*std::max(8000.,getSampleRate()));pos=at;
      auto m=meta.getMessage();int ch=m.getChannel()-1;
      if(m.isNoteOn())engine.noteOn(ch,m.getNoteNumber(),m.getFloatVelocity());
      else if(m.isNoteOff())engine.noteOff(ch,m.getNoteNumber());
      else if(m.isPitchWheel())engine.pitch(ch,m.getPitchWheelValue());
      else if(m.isAllSoundOff())engine.allOff(ch,true);
      else if(m.isAllNotesOff())engine.allOff(ch,false);
      else if(m.isController()){fxCCValue[m.getControllerNumber()].store(m.getControllerValue()/127.f);if(int slot=fxLearn.exchange(-1);slot>=0&&slot<40)fxLearnResult.store(slot*128+m.getControllerNumber());if(snapshotParams[3]->load()>.5f&&m.getControllerNumber()==119&&m.getControllerValue()<16)snapshotRequest.store(m.getControllerValue());engine.controller(ch,m.getControllerNumber(),m.getControllerValue());if(int row=learnRow.exchange(-1);row>=0&&row<66)learnResult.store(row*128+m.getControllerNumber());if(m.getControllerNumber()==64)engine.sustain(ch,m.getControllerValue()>=64);}
      else if(m.isChannelPressure())engine.pressure(ch,m.getChannelPressureValue());else if(m.isAftertouch())engine.polyAftertouch(ch,m.getNoteNumber(),m.getAfterTouchValue());
    }
    engine.render(l+pos,r+pos,n-pos,p);for(int k=0;k<5;++k)for(int j=0;j<8;++j){fxReductionView[k][j].store(engine.fxReduction(k,j),std::memory_order_relaxed);fxPhaseView[k][j].store(engine.fxPhase(k,j),std::memory_order_relaxed);}
    auto nx=engine.nexusPosition();auto nw=engine.nexusGains(p.nexus.power);for(int j=0;j<2;++j)nexusView[j].store(nx[j],std::memory_order_relaxed);for(int j=0;j<4;++j)nexusGainView[j].store(nw[j],std::memory_order_relaxed);auto phases=engine.lfoPhases();for(int j=0;j<8;++j)lfoPhaseView[j].store(phases[j],std::memory_order_relaxed);auto mv=engine.matrixValues();for(int j=0;j<cosmos::matrixCount;++j)matrixView[j].store(mv[j],std::memory_order_relaxed);
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
  int getNumPrograms()override{return 370;} int getCurrentProgram()override{return juce::jmax(0,program.load());}
  const juce::String getProgramName(int i)override{return presetName(juce::jlimit(0,369,i));}
  void changeProgramName(int,const juce::String&)override{}
  void setCurrentProgram(int i)override{
    if(i<0||i>=370)return;resetFX();int requested=i;if(i>=270)i-=270;mutationEpoch.fetch_add(1);for(auto key:mutationKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());
    for(auto key:qcKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());
    clearSnapshots();saveTrajectory(cosmos::factoryTrajectory(0));for(auto key:snapshotKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());
    trajectoryFactory.store(0);trajectoryClosed.store(true);trajectoryCurve.store(false);for(auto key:trajectoryKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());
    resetMotion();for(auto key:motionKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());
    for(auto key:morphKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());
    for(auto key:nexusKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());
    resetOscillators();for(int j=0;j<8;++j)state.state.removeProperty(juce::Identifier(macroId(j)+"Name"),nullptr);
    for(int j=0;j<cosmos::lfoCount;++j)for(auto key:lfoKeys)if(auto* par=state.getParameter(lid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
    for(int j=0;j<cosmos::modEnvCount;++j)for(auto key:envKeys)if(auto* par=state.getParameter(eid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
    for(int i=0;i<cosmos::modEnvCount;++i)for(int j=0;j<50;++j)if(auto* par=state.getParameter(eid(i,freeKey(j).toRawUTF8())))par->setValueNotifyingHost(par->getDefaultValue());
    for(int j=0;j<cosmos::matrixCount;++j)for(auto key:matrixKeys)if(auto* par=state.getParameter(mid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
    for(int j=0;j<8;++j)if(auto* par=state.getParameter(macroId(j)))par->setValueNotifyingHost(0.f);learnRow.store(-1);learnResult.store(-1);
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
    if(requested>=270){std::array<bool,8> families{false,false,false,false,true,true,false,false};auto batch=makeMutation(1,.40f,std::uint32_t(0x43ad123u+requested*7919u),families,true,true,true);applyMutation(batch,true);}
    program.store(requested);
  }
  void getStateInformation(juce::MemoryBlock& dest)override{
    auto tree=state.copyState();tree.setProperty("program",program.load(),nullptr);tree.setProperty("schema",25,nullptr);juce::String favFX;for(auto& f:fxFav)favFX+=f.load()?"1":"0";tree.setProperty("fxFavorites",favFX,nullptr);{std::lock_guard<std::mutex> lock(snapshotMutex);if(auto old=tree.getChildWithName("COSMOS_SNAPSHOTS");old.isValid())tree.removeChild(old,nullptr);tree.addChild(snapshotBank.createCopy(),-1,nullptr);}auto tr=readTrajectory();juce::ValueTree geom("TRAJECTORY_GEOMETRY");geom.setProperty("closed",tr.closed,nullptr);geom.setProperty("curve",tr.curve,nullptr);for(int j=0;j<tr.count;++j){juce::ValueTree pt("P");pt.setProperty("x",tr.x[j],nullptr);pt.setProperty("y",tr.y[j],nullptr);geom.addChild(pt,-1,nullptr);}if(auto old=tree.getChildWithName("TRAJECTORY_GEOMETRY");old.isValid())tree.removeChild(old,nullptr);tree.addChild(geom,-1,nullptr);tree.setProperty("trajFactory",trajectoryFactory.load(),nullptr);tree.setProperty("trajClosed",trajectoryClosed.load(),nullptr);tree.setProperty("trajCurve",trajectoryCurve.load(),nullptr);juce::String fav;for(int j=0;j<50;++j)fav+=trajectoryFav[j].load()?"1":"0";tree.setProperty("trajFav",fav,nullptr);auto motion=readMotion();juce::ValueTree mt("NEXUS_MOTION");mt.setProperty("duration",motion.duration,nullptr);mt.setProperty("sync",motion.sync,nullptr);for(int j=0;j<motion.count;++j){juce::ValueTree point("P");point.setProperty("x",motion.x[j],nullptr);point.setProperty("y",motion.y[j],nullptr);mt.addChild(point,-1,nullptr);}if(auto old=tree.getChildWithName("NEXUS_MOTION");old.isValid())tree.removeChild(old,nullptr);tree.addChild(mt,-1,nullptr);
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
      program.store(juce::jlimit(0,369,int(t.getProperty("program",0))));state.replaceState(t);if(int(t.getProperty("schema",1))<22)resetFX();else if(int(t.getProperty("schema",1))<23)for(int k=0;k<5;++k)for(int j=0;j<8;++j)for(int i=9;i<12;++i)if(auto* par=state.getParameter(fxid(k,j,fxKeys[i])))par->setValueNotifyingHost(par->getDefaultValue());if(int(t.getProperty("schema",1))<24)for(int k=0;k<5;++k)for(int j=0;j<8;++j)for(int i=12;i<14;++i)if(auto* par=state.getParameter(fxid(k,j,fxKeys[i])))par->setValueNotifyingHost(par->getDefaultValue());if(int(t.getProperty("schema",1))<25)for(int k=0;k<5;++k)for(int j=0;j<8;++j)for(int i=14;i<17;++i)if(auto* par=state.getParameter(fxid(k,j,fxKeys[i])))par->setValueNotifyingHost(par->getDefaultValue());auto favFX=t.getProperty("fxFavorites","").toString();for(int j=0;j<cosmos::fxTypes;++j)fxFav[j].store(j<favFX.length()&&favFX[j]=='1');if(int(t.getProperty("schema",1))<21)for(auto key:qcKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());mutationEpoch.fetch_add(1);if(int(t.getProperty("schema",1))<20)for(auto key:mutationKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());clearSnapshots();{auto bank=t.getChildWithName("COSMOS_SNAPSHOTS");if(bank.isValid()){for(int j=0;j<std::min(16,bank.getNumChildren());++j){auto c=bank.getChild(j);int slot=int(c.getProperty("slot",-1));if(slot>=0&&slot<16)putSnapshot(slot,c.createCopy());}}}if(int(t.getProperty("schema",1))<19)for(auto key:snapshotKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());auto geom=t.getChildWithName("TRAJECTORY_GEOMETRY");auto tr=cosmos::factoryTrajectory(std::clamp(int(t.getProperty("trajFactory",0)),0,49));if(geom.isValid()&&geom.getNumChildren()>=2){tr.count=std::min(cosmos::trajectoryCapacity,geom.getNumChildren());tr.closed=bool(geom.getProperty("closed",true));tr.curve=bool(geom.getProperty("curve",false));for(int j=0;j<tr.count;++j){auto pt=geom.getChild(j);tr.x[j]=std::clamp(float(pt.getProperty("x",.5)),0.f,1.f);tr.y[j]=std::clamp(float(pt.getProperty("y",.5)),0.f,1.f);}}saveTrajectory(tr);trajectoryFactory.store(std::clamp(int(t.getProperty("trajFactory",0)),0,49));trajectoryClosed.store(bool(t.getProperty("trajClosed",true)));trajectoryCurve.store(bool(t.getProperty("trajCurve",false)));auto fav=t.getProperty("trajFav","").toString();for(int j=0;j<50;++j)trajectoryFav[j].store(j<fav.length()&&fav[j]=='1');if(int(t.getProperty("schema",1))<18)for(auto key:trajectoryKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());resetMotion();cosmos::MotionClip restored;auto mt=t.getChildWithName("NEXUS_MOTION");if(mt.isValid()){restored.duration=std::clamp(double(mt.getProperty("duration",4)),.01,128.);restored.sync=bool(mt.getProperty("sync",true));restored.count=std::min(cosmos::motionPoints,mt.getNumChildren());for(int j=0;j<restored.count;++j){auto pt=mt.getChild(j);restored.x[j]=std::clamp(float(pt.getProperty("x",.5)),0.f,1.f);restored.y[j]=std::clamp(float(pt.getProperty("y",.5)),0.f,1.f);}}publishMotion(restored);if(int(t.getProperty("schema",1))<17)for(auto key:motionKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());if(int(t.getProperty("schema",1))<16)for(auto key:morphKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());if(int(t.getProperty("schema",1))<15)for(auto key:nexusKeys)if(auto* par=state.getParameter(key))par->setValueNotifyingHost(par->getDefaultValue());if(int(t.getProperty("schema",1))<14)for(int i=0;i<cosmos::modEnvCount;++i)for(int j=0;j<50;++j)if(auto* par=state.getParameter(eid(i,freeKey(j).toRawUTF8())))par->setValueNotifyingHost(par->getDefaultValue());if(int(t.getProperty("schema",1))<13)for(int i=0;i<cosmos::matrixCount;++i)for(int j=9;j<14;++j)if(auto* par=state.getParameter(mid(i,matrixKeys[j])))par->setValueNotifyingHost(par->getDefaultValue());learnRow.store(-1);learnResult.store(-1);
      if(int(t.getProperty("schema",1))<12){for(int j=0;j<cosmos::matrixCount;++j)for(auto key:{"oscillator","curve","smoothing"})if(auto* par=state.getParameter(mid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());for(int j=0;j<8;++j)if(auto* par=state.getParameter(macroId(j)))par->setValueNotifyingHost(0.f);}
      if(int(t.getProperty("schema",1))<11)for(int j=0;j<cosmos::matrixCount;++j)for(auto key:matrixKeys)if(auto* par=state.getParameter(mid(j,key)))par->setValueNotifyingHost(par->getDefaultValue());
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
  void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider)override{
    auto rect=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(12);
    float r=std::min(rect.getWidth(),rect.getHeight())*.43f;auto c=rect.getCentre();auto knob=juce::Rectangle<float>(2*r,2*r).withCentre(c);
    g.setColour(juce::Colour(0xff0b1119));g.fillEllipse(knob.expanded(5));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff53677f),c.x,c.y-r,juce::Colour(0xff192331),c.x,c.y+r,false));g.fillEllipse(knob);
    g.setColour(juce::Colour(0xff72859b));g.drawEllipse(knob,1.f);
    juce::Path arc;arc.addCentredArc(c.x,c.y,r+5,r+5,0,start,start+pos*(end-start),true);juce::Path track;track.addCentredArc(c.x,c.y,r+5,r+5,0,start,end,true);g.setColour(juce::Colour(0xff34485d));g.strokePath(track,juce::PathStrokeType(2.5f));g.setColour(cyan.withAlpha(.18f));g.strokePath(arc,juce::PathStrokeType(6.f));g.setColour(cyan);g.strokePath(arc,juce::PathStrokeType(2.5f));
    if(bool(slider.getProperties().getWithDefault("modAssigned",false))){float v=float(slider.getProperties().getWithDefault("modValue",0.f));float target=std::clamp(pos+v*.25f,0.f,1.f);juce::Path mod;mod.addCentredArc(c.x,c.y,r+9,r+9,0,start+std::min(pos,target)*(end-start),start+std::max(pos,target)*(end-start)+.015f,true);g.setColour(juce::Colour(0xff5de0bb));g.strokePath(mod,juce::PathStrokeType(2.f));g.setColour(juce::Colour(0xff5de0bb).withAlpha(.6f));g.drawEllipse(knob.expanded(9),.7f);g.setColour(cyan);}
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
  std::array<cosmos::LfoParams,cosmos::lfoCount> values{};int selected=0;std::array<float,8> phases{};
public:
  void update(const std::array<cosmos::LfoParams,cosmos::lfoCount>& p,int index,const std::array<float,8>& phase){bool changed=selected!=index||phases!=phase;for(int j=0;j<8;++j)changed|=values[j].shape!=p[j].shape||values[j].phase!=p[j].phase||values[j].polarity!=p[j].polarity;if(changed){values=p;selected=index;phases=phase;repaint();}}
  void paint(juce::Graphics& g)override{
    float w=getWidth()/8.f;
    for(int k=0;k<8;++k){auto area=juce::Rectangle<float>(k*w,0,w, float(getHeight())).reduced(3);g.setColour(juce::Colour(0xff101b29));g.fillRoundedRectangle(area,5);g.setColour(k==selected?cyan:juce::Colour(0xff40566f));g.drawRoundedRectangle(area,5,1);
      auto graph=area.reduced(5);graph.removeFromTop(15);graph.removeFromBottom(4);g.setFont(11);g.drawText("LFO "+juce::String(k+1),area.toNearestInt().removeFromTop(15),juce::Justification::centred);
      juce::Path path;for(int j=0;j<=96;++j){float x=graph.getX()+j*graph.getWidth()/96;auto v=values[k];v.depth=1;float y=graph.getCentreY()-cosmos::lfoAt(v,j/96.)*graph.getHeight()*.44f;if(j==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}g.strokePath(path,juce::PathStrokeType(1.3f));if(phases[k]>=0){g.setColour(juce::Colour(0xff5de0bb));g.drawVerticalLine(int(graph.getX()+phases[k]*graph.getWidth()),graph.getY(),graph.getBottom());}}
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
class FreeEnvelopeEditor final:public juce::Component{
  cosmos::FreeEnvelope f{};int handle=-1;
  juce::Rectangle<float> graph()const{return getLocalBounds().toFloat().reduced(10).withTrimmedTop(19);}
  juce::Point<float> xy(int i)const{auto b=graph();return {b.getX()+b.getWidth()*f.x[i],b.getBottom()-b.getHeight()*f.y[i]};}
  int nearest(juce::Point<float> pos)const{int best=-1;float distance=13;for(int i=0;i<f.count;++i){float d=xy(i).getDistanceFrom(pos);if(d<distance){best=i;distance=d;}}return best;}
  void publish(){if(onEdit)onEdit(f);repaint();}
public:
  std::function<void(const cosmos::FreeEnvelope&)> onEdit;
  void update(cosmos::FreeEnvelope value){if(handle>=0)return;value=cosmos::sanitizeFreeEnvelope(value);if(f.count!=value.count||f.x!=value.x||f.y!=value.y||f.curve!=value.curve){f=value;repaint();}}
  void mouseDown(const juce::MouseEvent& e)override{
    handle=-1;int i=nearest(e.position);
    if(e.mods.isPopupMenu()){if(i>0&&i<f.count-1&&f.count>2){for(int j=i;j<f.count-1;++j){f.x[j]=f.x[j+1];f.y[j]=f.y[j+1];f.curve[j]=f.curve[j+1];}--f.count;publish();}return;}
    if(e.mods.isShiftDown()){if(i>=0&&i<f.count-1){f.curve[i]=(f.curve[i]+1)%3;publish();}return;}
    if(i>0)handle=i;
  }
  void mouseDoubleClick(const juce::MouseEvent& e)override{
    handle=-1;if(nearest(e.position)>=0||f.count>=cosmos::freeEnvMax||!graph().contains(e.position))return;
    auto b=graph();float x=(e.position.x-b.getX())/b.getWidth();int i=1;while(i<f.count-1&&f.x[i]<x)++i;
    if(x<=f.x[i-1]+.002f||x>=f.x[i]-.002f)return;for(int j=f.count;j>i;--j){f.x[j]=f.x[j-1];f.y[j]=f.y[j-1];f.curve[j]=f.curve[j-1];}++f.count;f.x[i]=x;f.y[i]=std::clamp((b.getBottom()-e.position.y)/b.getHeight(),0.f,1.f);f.curve[i]=0;publish();
  }
  void mouseDrag(const juce::MouseEvent& e)override{if(handle<1)return;auto b=graph();if(handle<f.count-1)f.x[handle]=std::clamp((e.position.x-b.getX())/b.getWidth(),f.x[handle-1]+.002f,f.x[handle+1]-.002f);f.y[handle]=std::clamp((b.getBottom()-e.position.y)/b.getHeight(),0.f,1.f);repaint();}
  void mouseUp(const juce::MouseEvent&)override{if(handle>=1){handle=-1;publish();}}
  void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff0b131e));auto b=graph();g.setColour(light);g.setFont(12);g.drawText(juce::String(f.count)+"/16 points • double-clic : ajouter • clic droit : retirer • Maj + clic : courbe",getLocalBounds().removeFromTop(24),juce::Justification::centred);g.setColour(juce::Colour(0xff34485d));for(int i=0;i<=8;++i)g.drawVerticalLine(int(b.getX()+b.getWidth()*i/8),b.getY(),b.getBottom());for(int i=0;i<=4;++i)g.drawHorizontalLine(int(b.getY()+b.getHeight()*i/4),b.getX(),b.getRight());juce::Path path;path.startNewSubPath(xy(0));for(int i=0;i<f.count-1;++i){auto a=xy(i),z=xy(i+1);for(int j=1;j<=32;++j){float t=j/32.f;path.lineTo(std::lerp(a.x,z.x,t),std::lerp(a.y,z.y,cosmos::envelopeCurve(t,f.curve[i])));}}g.setColour(cyan);g.strokePath(path,juce::PathStrokeType(1.7f));for(int i=0;i<f.count;++i){auto v=xy(i);g.setColour(i==handle?juce::Colour(0xff5de0bb):cyan);g.fillEllipse(v.x-4,v.y-4,8,8);}}
};
class RouteNumber final:public juce::TextButton{
public:
  std::function<void()> onLearn;
  void mouseDown(const juce::MouseEvent& e)override{if(e.mods.isPopupMenu()){if(onLearn)onLearn();return;}juce::TextButton::mouseDown(e);}
  void mouseUp(const juce::MouseEvent& e)override{if(!e.mods.isPopupMenu())juce::TextButton::mouseUp(e);}
};
class ModSlider final:public juce::Slider,public juce::DragAndDropTarget{
public:
  int destination=0;bool individual=false;
  std::function<void(int)> assign;
  bool isInterestedInDragSource(const SourceDetails& d)override{auto s=d.description.toString();int source=s.fromFirstOccurrenceOf(":",false,false).getIntValue();return destination>0&&s.startsWith("cosmos-source:")&&source>0&&source<cosmos::matrixSourceCount;}
  void itemDropped(const SourceDetails& d)override{if(isInterestedInDragSource(d)&&assign)assign(d.description.toString().fromFirstOccurrenceOf(":",false,false).getIntValue());}
  void mouseDown(const juce::MouseEvent& e)override{if(destination>0&&e.mods.isPopupMenu()){if(assign)assign(-1);return;}juce::Slider::mouseDown(e);}
  void mouseUp(const juce::MouseEvent& e)override{if(!(destination>0&&e.mods.isPopupMenu()))juce::Slider::mouseUp(e);}
};
class ModDrag final:public juce::TextButton{
public:
  std::function<int()> source;
  void mouseDrag(const juce::MouseEvent& e)override{if(e.getDistanceFromDragStart()>5)if(auto* d=juce::DragAndDropContainer::findParentDragContainerFor(this))if(!d->isDragAndDropActive()&&source){int i=source();if(i>0)d->startDragging("cosmos-source:"+juce::String(i),this);}}
};
class CustomRouteCurve final:public juce::Component{
  std::array<float,5> points{0,.25f,.5f,.75f,1};int handle=-1;
  void edit(const juce::MouseEvent& e,bool begin,bool end){if(handle>=0&&onEdit)onEdit(handle,std::clamp(1.f-(e.position.y-8)/std::max(1.f,getHeight()-16.f),0.f,1.f),begin,end);}
public:
  std::function<void(int,float,bool,bool)> onEdit;
  void update(std::array<float,5> p){if(points!=p){points=p;repaint();}}
  void mouseDown(const juce::MouseEvent& e)override{handle=std::clamp(int(std::round((e.position.x-8)*4/std::max(1.f,getWidth()-16.f))),0,4);edit(e,true,false);}
  void mouseDrag(const juce::MouseEvent& e)override{edit(e,false,false);}
  void mouseUp(const juce::MouseEvent& e)override{edit(e,false,true);handle=-1;}
  void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff101b29));juce::Path line;for(int i=0;i<5;++i){float x=8+(getWidth()-16.f)*i/4,y=8+(getHeight()-16.f)*(1-points[i]);g.setColour(juce::Colour(0xff34485d));g.drawVerticalLine(int(x),0,float(getHeight()));if(i==0)line.startNewSubPath(x,y);else line.lineTo(x,y);}g.setColour(cyan);g.strokePath(line,juce::PathStrokeType(2));for(int i=0;i<5;++i)g.fillEllipse(4+(getWidth()-16.f)*i/4,4+(getHeight()-16.f)*(1-points[i]),8,8);}
};
class NexusPad final:public juce::Component{
  float x=.5f,y=.5f;std::array<float,4> weights{1,1,1,1};bool active=false,dragging=false;
  juce::Rectangle<float> graph()const{return getLocalBounds().toFloat().reduced(28).withTrimmedBottom(42);}
  void move(const juce::MouseEvent& e,bool begin,bool end){auto b=graph();if(onMove)onMove(std::clamp((e.position.x-b.getX())/b.getWidth(),0.f,1.f),std::clamp((b.getBottom()-e.position.y)/b.getHeight(),0.f,1.f),begin,end);}
public:
  std::function<void(float,float,bool,bool)> onMove;
  void update(float a,float b,std::array<float,4> w,bool en){if(std::abs(a-x)>.001f||std::abs(b-y)>.001f||w!=weights||active!=en){x=a;y=b;weights=w;active=en;repaint();}}
  void mouseDown(const juce::MouseEvent& e)override{dragging=true;move(e,true,false);}
  void mouseDrag(const juce::MouseEvent& e)override{if(dragging)move(e,false,false);}
  void mouseUp(const juce::MouseEvent& e)override{if(dragging){dragging=false;move(e,false,true);}}
  void paint(juce::Graphics& g)override{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff263b57),0,0,juce::Colour(0xff09131d),float(getWidth()),float(getHeight()),false));g.fillRoundedRectangle(getLocalBounds().toFloat(),8);auto b=graph();g.setColour(juce::Colour(0xff0b1521));g.fillRoundedRectangle(b,5);g.setColour(juce::Colour(0xff344f67));for(int j=0;j<=10;++j){g.drawVerticalLine(int(b.getX()+b.getWidth()*j/10),b.getY(),b.getBottom());g.drawHorizontalLine(int(b.getY()+b.getHeight()*j/10),b.getX(),b.getRight());}
    auto q=juce::Point<float>(b.getX()+b.getWidth()*x,b.getBottom()-b.getHeight()*y);g.setColour(cyan.withAlpha(.7f));g.drawVerticalLine(int(q.x),b.getY(),b.getBottom());g.setColour(juce::Colour(0xff5de0bb));g.drawHorizontalLine(int(q.y),b.getX(),b.getRight());for(int j=3;j>=1;--j){float radius=7.f+j*5;g.setColour(juce::Colour(0xff5de0bb).withAlpha(.45f/j));g.drawEllipse(q.x-radius,q.y-radius,2*radius,2*radius,1.2f);}g.setColour(juce::Colour(0xff9aefd9));g.fillEllipse(q.x-5,q.y-5,10,10);
    g.setFont(16);g.setColour(light);g.drawText("A",b.toNearestInt().removeFromTop(22).removeFromLeft(22),juce::Justification::centred);g.drawText("B",b.toNearestInt().removeFromTop(22).removeFromRight(22),juce::Justification::centred);g.drawText("C",b.toNearestInt().removeFromBottom(22).removeFromLeft(22),juce::Justification::centred);g.drawText("D",b.toNearestInt().removeFromBottom(22).removeFromRight(22),juce::Justification::centred);
    for(int j=0;j<4;++j){float w=(getWidth()-32.f)/4,x0=16+j*w,y0=getHeight()-48.f;g.setColour(juce::Colour(0xff30475d));g.fillRoundedRectangle(x0,y0,w-6,9,3);g.setColour(cyan);g.fillRoundedRectangle(x0,y0,(w-6)*std::clamp(weights[j],0.f,1.f),9,3);g.setColour(light);g.setFont(13);g.drawText(juce::String::charToString(juce::juce_wchar('A'+j))+" "+juce::String(weights[j]*100,1)+" %",int(x0),int(y0+12),int(w-6),20,juce::Justification::centred);}
    g.setColour(light);g.setFont(11);g.drawText(active?"NEXUS ACTIF":"NEXUS INACTIF",0,4,getWidth(),18,juce::Justification::centred);
  }
};
class MotionGraph final:public juce::Component{
 cosmos::MotionClip clip;float phase=0;bool rec=false;
public:
 void update(const cosmos::MotionClip& c,float p,bool r){clip=c;phase=p;rec=r;repaint();}
 void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff0b1723));auto b=getLocalBounds().toFloat().reduced(10);g.setColour(juce::Colour(0xff344b60));for(int j=0;j<=8;++j)g.drawVerticalLine(int(b.getX()+b.getWidth()*j/8),b.getY(),b.getBottom());for(int j=0;j<=4;++j)g.drawHorizontalLine(int(b.getY()+b.getHeight()*j/4),b.getX(),b.getRight());for(int axis=0;axis<2;++axis){juce::Path path;for(int j=0;j<clip.count;++j){float x=b.getX()+b.getWidth()*j/std::max(1,clip.count-1),y=b.getBottom()-b.getHeight()*(axis?clip.y[j]:clip.x[j]);if(j==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}g.setColour(juce::Colour(axis?0xff5de0bb:0xff51bddd));g.strokePath(path,juce::PathStrokeType(1.5f));}g.setColour(juce::Colours::white);g.drawVerticalLine(int(b.getX()+b.getWidth()*phase),b.getY(),b.getBottom());if(rec){g.setColour(juce::Colours::red);g.fillEllipse(12,12,9,9);}}
};
class TrajectoryEditor final:public juce::Component{
 int grabbed=-1;cosmos::Trajectory path;
 juce::Rectangle<float> area()const{return getLocalBounds().toFloat().reduced(12);}
 std::array<float,2> point(const juce::MouseEvent& e)const{auto b=area();return {std::clamp((e.position.x-b.getX())/b.getWidth(),0.f,1.f),std::clamp((b.getBottom()-e.position.y)/b.getHeight(),0.f,1.f)};}
 int nearest(std::array<float,2> xy)const{int k=-1;float best=.0025f;for(int j=0;j<path.count;++j){float dx=xy[0]-path.x[j],dy=xy[1]-path.y[j],d=dx*dx+dy*dy;if(d<best){best=d;k=j;}}return k;}
public:
 std::function<void()> changed,beforeEdit;
 const cosmos::Trajectory& value()const{return path;}
 void set(const cosmos::Trajectory& t){path=t;repaint();}
 void mouseDown(const juce::MouseEvent& e)override{auto xy=point(e);grabbed=nearest(xy);if(e.mods.isRightButtonDown()){if(grabbed>=0&&path.count>2){if(beforeEdit)beforeEdit();for(int j=grabbed;j<path.count-1;++j){path.x[j]=path.x[j+1];path.y[j]=path.y[j+1];}--path.count;if(changed)changed();repaint();}grabbed=-1;return;}if(grabbed>=0&&beforeEdit)beforeEdit();else if(e.getNumberOfClicks()>1&&path.count<cosmos::trajectoryCapacity){if(beforeEdit)beforeEdit();grabbed=path.count++;path.x[grabbed]=xy[0];path.y[grabbed]=xy[1];if(changed)changed();repaint();}}
 void mouseDrag(const juce::MouseEvent& e)override{if(grabbed>=0){auto xy=point(e);path.x[grabbed]=xy[0];path.y[grabbed]=xy[1];repaint();}}
 void mouseUp(const juce::MouseEvent&)override{if(grabbed>=0&&changed)changed();grabbed=-1;}
 void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff0b1723));auto b=area();g.setColour(juce::Colour(0xff344b60));for(int j=0;j<=8;++j){g.drawVerticalLine(int(b.getX()+b.getWidth()*j/8),b.getY(),b.getBottom());g.drawHorizontalLine(int(b.getY()+b.getHeight()*j/8),b.getX(),b.getRight());}juce::Path line;for(int j=0;j<=256;++j){auto q=path.at(j/256.f);float x=b.getX()+b.getWidth()*q[0],y=b.getBottom()-b.getHeight()*q[1];if(j==0)line.startNewSubPath(x,y);else line.lineTo(x,y);}g.setColour(juce::Colour(0xff51bddd));g.strokePath(line,juce::PathStrokeType(1.8f));g.setColour(juce::Colour(0xff5de0bb));for(int j=0;j<path.count;++j)g.fillEllipse(b.getX()+b.getWidth()*path.x[j]-2,b.getBottom()-b.getHeight()*path.y[j]-2,4,4);}
};
class FxSlotButton final:public juce::TextButton,public juce::DragAndDropTarget{
 public:int index=0;std::function<void(int,int)> moved;
 void mouseDrag(const juce::MouseEvent& e)override{if(e.getDistanceFromDragStart()>5)if(auto* d=juce::DragAndDropContainer::findParentDragContainerFor(this))if(!d->isDragAndDropActive())d->startDragging("cosmos-fx:"+juce::String(index),this);}
 bool isInterestedInDragSource(const SourceDetails& d)override{return d.description.toString().startsWith("cosmos-fx:")&&d.description.toString().fromFirstOccurrenceOf(":",false,false).getIntValue()!=index;}
 void itemDropped(const SourceDetails& d)override{int n=d.description.toString().fromFirstOccurrenceOf(":",false,false).getIntValue();if(n>=0&&n<8&&moved)moved(n,index);}
};
class FxMeter final:public juce::Component{
 int type=0;float a=.5f,b=.5f,gr=0,phase=0;
 public:void update(int t,float x,float y,float g,float ph=0){if(t!=type||x!=a||y!=b||std::abs(g-gr)>.05f||std::abs(ph-phase)>.002f){type=t;a=x;b=y;gr=g;phase=ph;repaint();}}
 void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff0b1723));auto z=getLocalBounds().toFloat().reduced(6);auto plot=z.removeFromLeft(z.getWidth()*.67f);g.setColour(juce::Colour(0xff344b60));g.drawRect(plot);g.drawLine(plot.getX(),plot.getCentreY(),plot.getRight(),plot.getCentreY());juce::Path line;if(type>=79){g.setColour(juce::Colour(0xffa6c4d9));g.setFont(12);g.drawFittedText(cosmos::creativeSpectral(type)?"FFT 512 / HOP 128":"CREATIVE DSP",plot.toNearestInt(),juce::Justification::centred,1);g.drawFittedText("NIVEAU",z.removeFromTop(20).toNearestInt(),juce::Justification::centred,1);g.setColour(juce::Colour(0xff344b60));g.fillRect(z);g.setColour(juce::Colour(0xff5de0bb));g.fillRect(z.withWidth(z.getWidth()*std::clamp(phase,0.f,1.f)));return;}else if(type>=64){const float activity=std::clamp(phase,0.f,1.f);for(int j=0;j<=100;++j){float x=j/100.f;float y=std::exp(-x*(8.f-6.f*a))*(.8f+.2f*std::sin(x*80.f)*b);float px=plot.getX()+plot.getWidth()*x,py=plot.getBottom()-plot.getHeight()*.85f*y;if(j==0)line.startNewSubPath(px,py);else line.lineTo(px,py);}g.setColour(juce::Colour(0xff51bddd));g.strokePath(line,juce::PathStrokeType(1.5f));g.setFont(11);g.drawFittedText("QUEUE / NIVEAU",z.removeFromTop(20).toNearestInt(),juce::Justification::centred,1);g.setColour(juce::Colour(0xff344b60));g.fillRect(z);g.setColour(juce::Colour(0xff5de0bb));g.fillRect(z.withWidth(z.getWidth()*activity));return;}else if(type>=49){g.setColour(juce::Colour(0xff51bddd));for(int j=0;j<7;++j){float x=plot.getX()+plot.getWidth()*(j+.5f)/7.f;float h=plot.getHeight()*.85f*std::pow(.94f*b,float(j));g.drawLine(x,plot.getBottom(),x,plot.getBottom()-h,2.f);}g.setColour(juce::Colour(0xffa6c4d9));g.setFont(11);g.drawFittedText("RÉPÉTITIONS G",z.toNearestInt(),juce::Justification::centred,1);return;}else if(type>=29){for(int j=0;j<=100;++j){float y=std::sin(6.2831853f*j/100);float px=plot.getX()+plot.getWidth()*j/100,py=plot.getCentreY()-y*plot.getHeight()*.42f;if(j==0)line.startNewSubPath(px,py);else line.lineTo(px,py);}g.setColour(juce::Colour(0xff51bddd));g.strokePath(line,juce::PathStrokeType(1.4f));g.setColour(juce::Colour(0xff5de0bb));float x=plot.getX()+plot.getWidth()*phase;g.drawLine(x,plot.getY(),x,plot.getBottom());}else if(type>=19){for(int j=0;j<=100;++j){float x=j/50.f-1,y=std::clamp(cosmos::analogShape(type,x,a,b),-1.f,1.f);float px=plot.getX()+plot.getWidth()*j/100,py=plot.getCentreY()-y*plot.getHeight()*.48f;if(j==0)line.startNewSubPath(px,py);else line.lineTo(px,py);}g.setColour(juce::Colour(0xff51bddd));g.strokePath(line,juce::PathStrokeType(1.4f));}else{g.setColour(juce::Colour(0xffa6c4d9));g.setFont(12);g.drawFittedText(type>=9&&type<=18?"DYNAMIQUE":"SIGNAL FX",plot.toNearestInt(),juce::Justification::centred,1);}g.setColour(juce::Colour(0xffa6c4d9));g.setFont(12);g.drawText("GR "+juce::String(gr,1)+" dB",z.removeFromTop(20).toNearestInt(),juce::Justification::centred);z.reduce(5,5);g.setColour(juce::Colour(0xff344b60));g.fillRect(z);g.setColour(juce::Colour(0xff5de0bb));g.fillRect(z.withWidth(z.getWidth()*std::clamp(gr/36,0.f,1.f)));}
};
class Editor final:public juce::AudioProcessorEditor,public juce::DragAndDropContainer,private juce::ListBoxModel,private juce::Timer{
  Processor& p;Theme theme;juce::Component canvas;
  juce::TextEditor search;juce::ListBox browser;juce::Label patchLabel,layerTitle,masterLabel;
  juce::Slider master;juce::ComboBox wave;juce::ToggleButton enabled{"Couche active"};
  std::array<juce::TextButton,4> tabs;std::array<ModSlider,8> knobs;std::array<juce::Label,8> labels;
  using SA=State::SliderAttachment;using CA=State::ComboBoxAttachment;using BA=State::ButtonAttachment;
  std::unique_ptr<SA> masterAtt;std::array<std::unique_ptr<SA>,8> att;std::unique_ptr<CA> waveAtt;std::unique_ptr<BA> enabledAtt;
  std::vector<int> visible;int selected=0,selectedOsc=0;bool oscPage=false;
  juce::TextButton oscPageButton{"OSCILLATEURS"},importButton{"Importer WAV / AIFF"};
  std::array<juce::TextButton,5> oscTabs;
  juce::ComboBox oscMode,oscWave;
  std::array<ModSlider,9> oscKnobs;std::array<juce::Label,9> oscLabels;
  juce::ToggleButton oscSync{"Hard Sync"},oscRing{"Ring Mod"};
  juce::Label sampleLabel;
  WavePreview preview;
  std::array<std::unique_ptr<SA>,9> oscAtt;std::unique_ptr<CA> modeAtt,oscWaveAtt;std::unique_ptr<BA> syncAtt,ringAtt;
  std::unique_ptr<juce::FileChooser> chooser;
  bool nexusPage=false;NexusPad nexusPad;juce::TextButton nexusButton{"NEXUS"};std::array<juce::ToggleButton,7> nxToggles;std::array<std::unique_ptr<BA>,7> nxToggleAtt;std::array<juce::Slider,3> nxSliders;std::array<juce::Label,3> nxSliderLabels;std::array<std::unique_ptr<SA>,3> nxSliderAtt;std::array<juce::ComboBox,2> nxMacros;std::array<std::unique_ptr<CA>,2> nxMacroAtt;std::array<juce::Label,2> nxMacroLabels,nxCCLabels;std::array<juce::TextButton,2> nxLearn,nxClear;bool nxGesture[2]{};
  bool grainPage=false,filterPage=false,filterModPage=false,lfoPage=false;
  void showNexus(bool show){setLfoPage(false);nexusPage=show;lfoPage=show;setOscPage(false);}
  bool morphPage=false;juce::TextButton morphButton{"MORPH ZONE"};std::array<juce::ToggleButton,8> morphToggles;std::array<std::unique_ptr<BA>,8> morphToggleAtt;juce::Slider morphAmount;juce::ComboBox morphCurve;juce::Label morphAmountLabel,morphCurveLabel,morphInfo;std::unique_ptr<SA> morphAmountAtt;std::unique_ptr<CA> morphCurveAtt;
  bool motionPage=false;juce::TextButton motionButton{"MOTION"},motionRec{"REC"},motionStop{"STOP"},motionClear{"Effacer"};juce::ToggleButton motionPlay{"Lecture"},motionSync{"Synchro tempo"};MotionGraph motionGraph;juce::ComboBox motionLength,motionMode;std::array<juce::Slider,4> motionSliders;std::array<juce::Label,4> motionLabels;juce::Label motionStatus;std::unique_ptr<BA> motionPlayAtt,motionSyncAtt;std::unique_ptr<CA> motionLengthAtt,motionModeAtt;std::array<std::unique_ptr<SA>,4> motionSliderAtt;
  bool trajectoryPage=false;juce::TextButton trajectoryButton{"TRAJECTOIRES"},trajectoryApply{"Appliquer / lire"},trajectoryUndo{"Annuler"},trajectoryRedo{"Rétablir"},trajectoryStar{"Favori"};juce::ComboBox trajectoryList,trajectoryEase,trajectoryMacro;juce::TextEditor trajectorySearch;juce::ToggleButton trajectoryOnlyFav{"Favoris"},trajectoryClose{"Fermée"},trajectoryBezier{"Bézier"},trajectoryEnable{"Transformation"};TrajectoryEditor trajectoryEditor;std::array<juce::Slider,5> trajectorySliders;std::array<juce::Label,5> trajectoryLabels;std::array<std::unique_ptr<SA>,5> trajectoryAtt;std::unique_ptr<BA> trajectoryEnableAtt;std::unique_ptr<CA> trajectoryEaseAtt,trajectoryMacroAtt;std::vector<cosmos::Trajectory> trajectoryBack,trajectoryForward;
  void rememberTrajectory(){if(trajectoryBack.size()>=32)trajectoryBack.erase(trajectoryBack.begin());trajectoryBack.push_back(trajectoryEditor.value());trajectoryForward.clear();}
  void refreshTrajectoryList(){auto filter=trajectorySearch.getText();int current=p.trajectoryFactory.load();trajectoryList.clear(juce::dontSendNotification);for(int j=0;j<50;++j){auto name=juce::String::fromUTF8(cosmos::trajectoryFamily(j))+" "+juce::String(j%5+1);if((!trajectoryOnlyFav.getToggleState()||p.trajectoryFav[j].load())&&name.containsIgnoreCase(filter))trajectoryList.addItem(name,j+1);}trajectoryList.setSelectedId(current+1,juce::dontSendNotification);}
  void loadTrajectory(const cosmos::Trajectory& t){p.saveTrajectory(t);trajectoryEditor.set(t);trajectoryClose.setToggleState(t.closed,juce::dontSendNotification);trajectoryBezier.setToggleState(t.curve,juce::dontSendNotification);}
  void applyTrajectory(){if(p.motionRecording.load())return;auto t=trajectoryEditor.value();p.trajectoryClosed.store(t.closed);p.trajectoryCurve.store(t.curve);bool sync=p.motionParams[1]->load()>.5f;double duration=sync?4.*(1<<std::clamp(int(p.motionParams[2]->load()),0,5)):p.motionParams[7]->load();p.publishMotion(t.clip(duration,sync));if(auto* par=p.state.getParameter(motionKeys[0]))par->setValueNotifyingHost(1);if(auto* par=p.state.getParameter(nexusKeys[0]))par->setValueNotifyingHost(1);}
  bool snapshotPage=false;int snapshotSelected=0,snapshotSeenTrajectory=-1;juce::TextButton snapshotButton{"SNAPSHOTS"},snapshotCapture{"Capturer"},snapshotRecall{"Rappeler"},snapshotDelete{"Supprimer"},snapshotCopy{"Copier"},snapshotPaste{"Coller"},snapshotAB{"Comparer A/B"};std::array<juce::TextButton,16> snapshotSlots;juce::TextEditor snapshotName;juce::ComboBox snapshotColour,snapshotMacro;juce::ToggleButton snapshotTune{"Protéger accordage"},snapshotGain{"Protéger volume"},snapshotMidi{"MIDI CC119"};juce::Slider snapshotTime;juce::Label snapshotStatus,snapshotTimeLabel,snapshotMacroLabel;juce::ValueTree snapshotClipboard;std::unique_ptr<SA> snapshotTimeAtt;std::unique_ptr<CA> snapshotMacroAtt;std::array<std::unique_ptr<BA>,3> snapshotToggleAtt;
  void selectSnapshot(int i){snapshotSelected=i;auto t=p.snapshotSlot(i);snapshotName.setText(t.getProperty("name","Mémoire "+juce::String(i+1)).toString(),false);snapshotColour.setSelectedId(std::clamp(int(t.getProperty("colour",0)),0,5)+1,juce::dontSendNotification);}
  void refreshSnapshots(){const juce::Colour palette[]={juce::Colour(0xff29465f),juce::Colour(0xff23645b),juce::Colour(0xff684b84),juce::Colour(0xff825d34),juce::Colour(0xff703b48),juce::Colour(0xff3d677c)};for(int j=0;j<16;++j){auto t=p.snapshotMetadata(j);auto name=juce::String(j+1).paddedLeft('0',2)+"  "+t.first;if(snapshotSlots[j].getButtonText()!=name)snapshotSlots[j].setButtonText(name);auto colour=t.second>=0?palette[t.second]:juce::Colour(0xff182737);if(snapshotSlots[j].findColour(juce::TextButton::buttonColourId)!=colour)snapshotSlots[j].setColour(juce::TextButton::buttonColourId,colour);snapshotSlots[j].setToggleState(j==snapshotSelected,juce::dontSendNotification);}snapshotStatus.setText(p.snapshotActive.load()<0?"Sélectionner une mémoire":juce::String("Mémoire ")+juce::String(p.snapshotActive.load()+1)+" • "+juce::String(p.snapshotProgress.load()*100,0)+" %",juce::dontSendNotification);}
  bool mutationPage=false;int seenMutationEpoch=-1;std::vector<Processor::MutationBatch> mutationBack,mutationForward;bool mutationOriginal=false;
  juce::TextButton mutationButton{"MUTATION"},mutationGenerate{"Générer"},mutationUndo{"Annuler"},mutationRedo{"Rétablir"},mutationAB{"Comparer A/B"},mutationSnapshot{"Vers snapshot"},mutationExport{"Exporter preset"};juce::ComboBox mutationMode;juce::Slider mutationAmount;juce::TextEditor mutationSeed;std::array<juce::ToggleButton,10> mutationToggles;std::array<std::unique_ptr<BA>,10> mutationToggleAtt;std::unique_ptr<CA> mutationModeAtt;std::unique_ptr<SA> mutationAmountAtt;juce::Label mutationInfo,mutationAmountLabel,mutationSeedLabel;
  void syncMutationHistory(){int epoch=p.mutationEpoch.load();if(epoch!=seenMutationEpoch){seenMutationEpoch=epoch;mutationBack.clear();mutationForward.clear();mutationOriginal=false;}}
  void generateMutation(){syncMutationHistory();if(mutationOriginal&&!mutationBack.empty())p.applyMutation(mutationBack.back(),true);mutationOriginal=false;std::array<bool,8> families;for(int j=0;j<8;++j)families[j]=p.mutationParams[5+j]->load()>.5f;auto batch=p.makeMutation(int(p.mutationParams[0]->load()),p.mutationParams[1]->load(),std::uint32_t(p.mutationParams[2]->load()),families,p.mutationParams[3]->load()>.5f,p.mutationParams[4]->load()>.5f);if(batch.empty())return;p.applyMutation(batch,true);if(mutationBack.size()>=32)mutationBack.erase(mutationBack.begin());mutationBack.push_back(std::move(batch));mutationForward.clear();}
  juce::ToggleButton snapshotXY{"Morphing XY"},motionOverdub{"Overdub"};std::array<juce::ComboBox,4> snapshotCorners;std::array<std::unique_ptr<CA>,4> cornerAtt;std::unique_ptr<BA> snapshotXYAtt,overdubAtt;juce::ComboBox motionQuant;std::unique_ptr<CA> motionQuantAtt;juce::TextButton trajectoryImport{"Importer"},trajectoryExport{"Exporter"};juce::Label trajectoryIO;
  void trajectoryFile(bool save){chooser=std::make_unique<juce::FileChooser>(save?"Exporter trajectoire":"Importer trajectoire",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Trajectoire.cosmostraj"),"*.cosmostraj");auto safe=juce::Component::SafePointer<Editor>(this);chooser->launchAsync((save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,[safe,save](const juce::FileChooser& f){if(!safe||f.getResult()==juce::File())return;bool ok=false;if(save){auto tr=safe->trajectoryEditor.value();juce::XmlElement xml("COSMOS_TRAJECTORY");xml.setAttribute("closed",tr.closed?1:0);xml.setAttribute("curve",tr.curve?1:0);for(int j=0;j<tr.count;++j){auto* pt=xml.createNewChildElement("P");pt->setAttribute("x",double(tr.x[j]));pt->setAttribute("y",double(tr.y[j]));}ok=xml.writeTo(f.getResult().withFileExtension("cosmostraj"));}else if(f.getResult().getSize()<=65536){auto xml=juce::XmlDocument::parse(f.getResult());if(xml&&xml->hasTagName("COSMOS_TRAJECTORY")){cosmos::Trajectory tr;tr.closed=xml->getBoolAttribute("closed",true);tr.curve=xml->getBoolAttribute("curve",false);ok=true;for(auto* pt:xml->getChildIterator()){if(!pt->hasTagName("P")||tr.count>=cosmos::trajectoryCapacity){ok=false;break;}double x=pt->getDoubleAttribute("x",-1),y=pt->getDoubleAttribute("y",-1);if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>1||y<0||y>1){ok=false;break;}tr.x[tr.count]=float(x);tr.y[tr.count++]=float(y);}ok&=tr.count>=2;if(ok){safe->rememberTrajectory();safe->loadTrajectory(tr);}}}safe->trajectoryIO.setText(ok?"Fichier OK":"Fichier non valide",juce::dontSendNotification);});}
  bool fxPage=false;int fxRack=0,fxSlot=0;juce::TextButton fxButton{"FX"},fxUp{"Monter"},fxDown{"Descendre"},fxFavorite{"Favori"},fxLoad{"Charger"},fxPreset{"Réglage initial"};std::array<juce::TextButton,5> fxRackButtons;std::array<FxSlotButton,8> fxSlotButtons;juce::ToggleButton fxAuto{"Gain auto"},fxSync{"Tempo"};juce::Slider fxStereo,fxDelayRight,fxDelayTone;juce::Label fxDelayToneLabel,fxEarlyLabel,fxModLabel;juce::ToggleButton fxFreeze{"Freeze"};std::unique_ptr<SA> fxDelayRightAtt,fxDelayToneAtt;std::unique_ptr<BA> fxFreezeAtt;std::unique_ptr<BA> fxSyncAtt;std::unique_ptr<SA> fxStereoAtt;juce::ComboBox fxQuality,fxProfile;juce::TextButton fxMidi{"MIDI Learn"},fxMidiClear{"Effacer CC"};FxMeter fxMeter;std::unique_ptr<BA> fxAutoAtt;std::unique_ptr<CA> fxQualityAtt;juce::ComboBox fxType,fxMacro,fxBrowser;juce::ToggleButton fxEnabled{"Actif"},fxParallel{"Parallèle"},fxOnlyFav{"Favoris"};juce::TextEditor fxSearch;std::array<juce::Slider,6> fxSliders;std::array<juce::Label,6> fxLabels;std::array<std::unique_ptr<SA>,6> fxAtt;std::unique_ptr<CA> fxTypeAtt,fxMacroAtt;std::unique_ptr<BA> fxEnableAtt,fxParallelAtt;
  void refreshFXBrowser(){fxBrowser.clear(juce::dontSendNotification);for(int j=1;j<cosmos::fxTypes;++j)if((!fxOnlyFav.getToggleState()||p.fxFav[j].load())&&fxNames[j].containsIgnoreCase(fxSearch.getText()))fxBrowser.addItem(fxNames[j],j+1);}
  void bindFX(){fxDelayRightAtt.reset();fxDelayToneAtt.reset();fxFreezeAtt.reset();fxDelayRightAtt=std::make_unique<SA>(p.state,fxid(fxRack,fxSlot,"delayFeedbackR"),fxDelayRight);fxDelayToneAtt=std::make_unique<SA>(p.state,fxid(fxRack,fxSlot,"delayTone"),fxDelayTone);fxFreezeAtt=std::make_unique<BA>(p.state,fxid(fxRack,fxSlot,"delayFreeze"),fxFreeze);fxSyncAtt.reset();fxStereoAtt.reset();fxSyncAtt=std::make_unique<BA>(p.state,fxid(fxRack,fxSlot,"sync"),fxSync);fxStereoAtt=std::make_unique<SA>(p.state,fxid(fxRack,fxSlot,"stereo"),fxStereo);fxAutoAtt.reset();fxQualityAtt.reset();fxAutoAtt=std::make_unique<BA>(p.state,fxid(fxRack,fxSlot,"autoGain"),fxAuto);fxQualityAtt=std::make_unique<CA>(p.state,fxid(fxRack,fxSlot,"quality"),fxQuality);fxTypeAtt.reset();fxMacroAtt.reset();fxEnableAtt.reset();fxParallelAtt.reset();for(auto& a:fxAtt)a.reset();fxTypeAtt=std::make_unique<CA>(p.state,fxid(fxRack,fxSlot,fxKeys[0]),fxType);fxEnableAtt=std::make_unique<BA>(p.state,fxid(fxRack,fxSlot,fxKeys[1]),fxEnabled);fxParallelAtt=std::make_unique<BA>(p.state,fxroute(fxRack),fxParallel);fxMacroAtt=std::make_unique<CA>(p.state,fxid(fxRack,fxSlot,fxKeys[7]),fxMacro);for(int j=0;j<6;++j)fxAtt[j]=std::make_unique<SA>(p.state,fxid(fxRack,fxSlot,fxKeys[j==5?8:j+2]),fxSliders[j]);}
  void fxDelayVisibility(bool fx,int t){bool cr=fx&&t>=79,d=fx&&t>=49&&t<79,rv=fx&&t>=64&&t<79;fxFreeze.setVisible(d||(cr&&cosmos::creativeFreeze(t)));fxDelayRight.setVisible(d);fxDelayTone.setVisible(d);fxDelayToneLabel.setVisible(d);fxEarlyLabel.setVisible(rv);fxModLabel.setVisible(rv);fxStereo.setVisible(fx&&!cr&&(!d||rv));fxAuto.setVisible(fx&&!d&&!cr);fxQuality.setVisible(fx&&!d&&!cr);fxSync.setVisible(fx&&!rv&&(!cr||cosmos::creativeSync(t)));fxStereo.setBounds(rv?438:1033,rv?630:626,rv?82:132,28);fxStereo.setTextBoxStyle(rv?juce::Slider::NoTextBox:juce::Slider::TextBoxRight,false,45,22);fxStereo.setTooltip(rv?"Modulation de la réverbération":"Décalage de phase stéréo");fxDelayRight.setTooltip(rv?"Niveau des premières réflexions":"Feedback droit");fxDelayToneLabel.setText(rv?"Amortissement":"Filtre feedback",juce::dontSendNotification);fxDelayTone.setTooltip(rv?"Amortissement des aigus":"Fréquence de coupure dans la boucle");}void refreshFX(){int t=int(p.fxParams[fxRack][fxSlot][0]->load());fxDelayVisibility(nexusPage&&fxPage,t);fxMeter.update(t,p.fxParams[fxRack][fxSlot][5]->load(),p.fxParams[fxRack][fxSlot][6]->load(),p.fxReductionView[fxRack][fxSlot].load(),p.fxPhaseView[fxRack][fxSlot].load());int cc=int(p.fxParams[fxRack][fxSlot][11]->load());fxMidi.setButtonText(p.fxLearn.load()==fxRack*8+fxSlot?"En écoute…":cc>=0?"CC "+juce::String(cc):"MIDI Learn");fxLabels[3].setText(t>=9&&t<=18&&t!=17?"Seuil":t==17?"Attaque":t>=79?juce::String::fromUTF8(cosmos::creativeA[std::clamp(t-79,0,21)]):t>=64?"Décroissance":t>=49?"Temps":t>=29?"Vitesse / fréquence":t>=19?"Drive":"Paramètre A",juce::dontSendNotification);fxSliders[4].setVisible(nexusPage&&fxPage&&t!=79&&t!=81);fxLabels[4].setVisible(nexusPage&&fxPage&&t!=79&&t!=81);fxLabels[4].setText(t>=9&&t<=18&&t!=17?"Ratio / réglage":t==17?"Sustain":t>=79?juce::String::fromUTF8(cosmos::creativeB[std::clamp(t-79,0,21)]):t>=64?"Diffusion":t>=49?"Feedback G":t>=29?"Profondeur":t>=19?"Couleur":"Paramètre B",juce::dontSendNotification);for(int j=0;j<5;++j)fxRackButtons[j].setToggleState(j==fxRack,juce::dontSendNotification);for(int j=0;j<8;++j){int type=std::clamp(int(p.fxParams[fxRack][j][0]->load()),0,cosmos::fxTypes-1);auto name=juce::String(j+1)+"  "+fxNames[type]+(p.fxParams[fxRack][j][1]->load()>.5f?" • ON":" • OFF");if(fxSlotButtons[j].getButtonText()!=name)fxSlotButtons[j].setButtonText(name);fxSlotButtons[j].setToggleState(j==fxSlot,juce::dontSendNotification);}}
  void nexusVisibility(){nexusButton.setButtonText(nexusPage?"COUCHE":"NEXUS");nexusPad.setVisible(nexusPage);morphButton.setVisible(nexusPage);morphButton.setButtonText(morphPage?"CONTRÔLES XY":"MORPH ZONE");bool xy=nexusPage&&!fxPage&&!morphPage&&!motionPage&&!trajectoryPage&&!snapshotPage&&!mutationPage,mv=nexusPage&&!fxPage&&morphPage&&!motionPage&&!trajectoryPage&&!snapshotPage&&!mutationPage,mo=nexusPage&&!fxPage&&motionPage&&!trajectoryPage&&!snapshotPage&&!mutationPage,tr=nexusPage&&!fxPage&&trajectoryPage&&!snapshotPage&&!mutationPage,ss=nexusPage&&!fxPage&&snapshotPage&&!mutationPage,mu=nexusPage&&!fxPage&&mutationPage,fx=nexusPage&&fxPage;fxButton.setButtonText(fxPage?"RETOUR":"FX");fxSync.setVisible(fx);fxStereo.setVisible(fx);fxAuto.setVisible(fx);fxQuality.setVisible(fx);fxProfile.setVisible(fx);fxDelayVisibility(fx,int(p.fxParams[fxRack][fxSlot][0]->load()));fxMidi.setVisible(fx);fxMidiClear.setVisible(fx);fxMeter.setVisible(fx);for(auto& b:fxRackButtons)b.setVisible(fx);for(auto& b:fxSlotButtons)b.setVisible(fx);for(auto& s:fxSliders)s.setVisible(fx);for(auto& l:fxLabels)l.setVisible(fx);for(auto* c:std::array<juce::Component*,12>{&fxUp,&fxDown,&fxFavorite,&fxLoad,&fxPreset,&fxType,&fxMacro,&fxBrowser,&fxEnabled,&fxParallel,&fxOnlyFav,&fxSearch})c->setVisible(fx);snapshotXY.setVisible(ss);for(auto& c:snapshotCorners)c.setVisible(ss);motionOverdub.setVisible(mo);motionQuant.setVisible(mo);trajectoryImport.setVisible(tr);trajectoryExport.setVisible(tr);trajectoryIO.setVisible(tr);mutationButton.setVisible(nexusPage);mutationButton.setButtonText(mutationPage?"RETOUR XY":"MUTATION");for(auto* c:std::array<juce::Component*,12>{&mutationGenerate,&mutationUndo,&mutationRedo,&mutationAB,&mutationSnapshot,&mutationExport,&mutationMode,&mutationAmount,&mutationSeed,&mutationInfo,&mutationAmountLabel,&mutationSeedLabel})c->setVisible(mu);for(auto& b:mutationToggles)b.setVisible(mu);snapshotButton.setVisible(nexusPage);snapshotButton.setButtonText(snapshotPage?"CONTRÔLES XY":"SNAPSHOTS");layerTitle.setVisible(!nexusPage);nexusPad.setVisible(nexusPage&&!snapshotPage&&!mutationPage&&!fxPage);for(auto& b:snapshotSlots)b.setVisible(ss);for(auto* c:std::array<juce::Component*,16>{&snapshotCapture,&snapshotRecall,&snapshotDelete,&snapshotCopy,&snapshotPaste,&snapshotAB,&snapshotName,&snapshotColour,&snapshotMacro,&snapshotTune,&snapshotGain,&snapshotMidi,&snapshotTime,&snapshotStatus,&snapshotTimeLabel,&snapshotMacroLabel})c->setVisible(ss);trajectoryButton.setVisible(nexusPage);lfoButton.setVisible(!nexusPage);trajectoryButton.setButtonText(trajectoryPage?"CONTRÔLES XY":"TRAJECTOIRES");for(auto* c:std::array<juce::Component*,13>{&trajectoryEditor,&trajectoryList,&trajectorySearch,&trajectoryOnlyFav,&trajectoryClose,&trajectoryBezier,&trajectoryEnable,&trajectoryApply,&trajectoryUndo,&trajectoryRedo,&trajectoryStar,&trajectoryEase,&trajectoryMacro})c->setVisible(tr);for(auto& s:trajectorySliders)s.setVisible(tr);for(auto& l:trajectoryLabels)l.setVisible(tr);motionButton.setVisible(nexusPage);motionButton.setButtonText(motionPage?"CONTRÔLES XY":"MOTION");motionRec.setVisible(mo);motionStop.setVisible(mo);motionClear.setVisible(mo);motionPlay.setVisible(mo);motionSync.setVisible(mo);motionGraph.setVisible(mo);motionLength.setVisible(mo);motionMode.setVisible(mo);motionStatus.setVisible(mo);for(auto& s:motionSliders)s.setVisible(mo);for(auto& l:motionLabels)l.setVisible(mo);for(auto& t:morphToggles)t.setVisible(mv);morphAmount.setVisible(mv);morphCurve.setVisible(mv);morphAmountLabel.setVisible(mv);morphCurveLabel.setVisible(mv);morphInfo.setVisible(mv);for(auto& t:nxToggles)t.setVisible(xy);for(auto& t:nxSliders)t.setVisible(xy);for(auto& t:nxSliderLabels)t.setVisible(xy);for(int j=0;j<2;++j){nxMacros[j].setVisible(xy);nxMacroLabels[j].setVisible(xy);nxCCLabels[j].setVisible(xy);nxLearn[j].setVisible(xy);nxClear[j].setVisible(xy);}}
  juce::ComboBox dragSource;ModDrag dragChip;juce::Label dragStatus;CustomRouteCurve customCurve;juce::TextButton curveButton{"COURBE"};bool curvePage=false;
  bool matrixPage=false,macroPage=false;int matrixBank=0,detailRow=0;juce::TextButton macroButton{"MACROS"};juce::Label detailLabel,detailLive;juce::ComboBox detailOsc,detailCurve;juce::Slider detailSmooth;std::unique_ptr<CA> detailOscAtt,detailCurveAtt;std::unique_ptr<SA> detailSmoothAtt;
  std::array<juce::Slider,8> macroKnobs;std::array<juce::Label,8> macroLabels;std::array<std::unique_ptr<SA>,8> macroAtt;
  void routeTo(int source,int dest,bool individual){if(dest<=0||dest>cosmos::matrixDestCount||source==0||source>=cosmos::matrixSourceCount)return;if(p.learnRow.load()>=0||p.learnResult.load()>=0){dragStatus.setText("MIDI Learn en cours",juce::dontSendNotification);return;}int free=-1;auto patch=p.patch();for(int i=0;i<64;++i)if(!patch.matrix[i].enabled&&patch.matrix[i].source==0){free=i;break;}if(free<0){dragStatus.setText("Matrice pleine",juce::dontSendNotification);return;}auto set=[&](const char* key,float v){if(auto* par=p.state.getParameter(mid(free,key))){par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(v));par->endChangeGesture();}};set("curve",0);set("smoothing",0);set("destination",float(dest));set("layer",float(selected+1));set("oscillator",individual?float(selectedOsc+1):0.f);set("amount",.25f);set("polarity",source>0&&source<=8?1.f:0.f);if(source>0){set("source",float(source));set("enabled",1);dragStatus.setText("Route "+juce::String(free+1),juce::dontSendNotification);}else{set("enabled",0);p.learnResult.store(-1);p.learnRow.store(free);dragStatus.setText("MIDI Learn…",juce::dontSendNotification);}}
  void setupTarget(ModSlider& s,int d,bool individual=false){s.destination=d;s.individual=individual;if(d>0){s.assign=[this,d,individual](int source){routeTo(source,d,individual);};s.setTooltip("Déposer une source / clic droit : MIDI Learn CC");}}
  void updateHalo(ModSlider& s,const cosmos::Patch& patch){if(!s.isVisible()||s.destination==0)return;float sum=0;bool assigned=false;for(int i=0;i<64;++i){const auto& m=patch.matrix[i];if(cosmos::validRoute(m)&&m.destination==s.destination&&(m.layer==0||m.layer==selected+1)&&(!s.individual||m.oscillator==0||m.oscillator==selectedOsc+1)&& (s.individual||m.oscillator==0)){assigned=true;sum+=p.matrixView[i].load(std::memory_order_relaxed);}}float q=std::clamp(sum,-1.f,1.f);auto& props=s.getProperties();if(bool(props.getWithDefault("modAssigned",false))!=assigned||std::abs(float(props.getWithDefault("modValue",0.f))-q)>.005f){props.set("modAssigned",assigned);props.set("modValue",q);s.repaint();}}
  void bindDetail(int row){detailRow=row;int i=matrixBank*8+row;detailOscAtt.reset();detailCurveAtt.reset();detailSmoothAtt.reset();detailOscAtt=std::make_unique<CA>(p.state,mid(i,"oscillator"),detailOsc);detailCurveAtt=std::make_unique<CA>(p.state,mid(i,"curve"),detailCurve);detailSmoothAtt=std::make_unique<SA>(p.state,mid(i,"smoothing"),detailSmooth);detailLabel.setText("Route "+juce::String(i+1),juce::dontSendNotification);for(int j=0;j<8;++j)matrixNumbers[j].setToggleState(j==row,juce::dontSendNotification);}juce::TextButton matrixButton{"MATRICE"};std::array<juce::TextButton,8> matrixBanks;
  std::array<juce::Label,7> matrixHeaders;std::array<RouteNumber,8> matrixNumbers;std::array<juce::ToggleButton,8> matrixEnabled;std::array<std::array<juce::ComboBox,4>,8> matrixBoxes;std::array<juce::Slider,8> matrixAmounts;
  std::array<std::unique_ptr<BA>,8> matrixEnabledAtt;std::array<std::array<std::unique_ptr<CA>,4>,8> matrixChoiceAtt;std::array<std::unique_ptr<SA>,8> matrixAmountAtt;
  void bindMatrix(int bank){matrixBank=bank;for(int row=0;row<8;++row){int i=bank*8+row;matrixEnabledAtt[row].reset();matrixAmountAtt[row].reset();for(auto& a:matrixChoiceAtt[row])a.reset();matrixEnabledAtt[row]=std::make_unique<BA>(p.state,mid(i,"enabled"),matrixEnabled[row]);for(int j=0;j<4;++j)matrixChoiceAtt[row][j]=std::make_unique<CA>(p.state,mid(i,matrixKeys[j+1]),matrixBoxes[row][j]);matrixAmountAtt[row]=std::make_unique<SA>(p.state,mid(i,"amount"),matrixAmounts[row]);matrixNumbers[row].setButtonText(juce::String(i+1));matrixBanks[row].setToggleState(row==bank,juce::dontSendNotification);}bindDetail(detailRow);}
  bool envPage=false;int selectedEnv=0;juce::TextButton envButton{"ENVELOPPES"};std::array<juce::TextButton,12> envTabs;
  ModEnvelopeView envView;FreeEnvelopeEditor freeView;juce::ToggleButton freeToggle{"Points libres"};std::unique_ptr<BA> freeToggleAtt;
  void layoutFree(bool free){envView.setVisible(!free);freeView.setVisible(free);freeToggle.setVisible(true);// Preserve validated v0.5.6 geometry clearance.
    freeToggle.setBounds(free?310:460,free?465:396,130,19);envBoxLabels[0].setBounds(310,398,145,18);for(int j=0;j<6;++j){int x=310+285*(j%3),y=(free?492:398)+50*(j/3);envBoxes[j].setVisible(!free||(j!=0&&j!=2));envBoxLabels[j].setVisible(!free||(j!=0&&j!=2));if(j!=0||free)envBoxLabels[j].setBounds(x,y,270,18);envBoxes[j].setBounds(x,y+19,270,27);}for(int j=0;j<9;++j){bool show=!free||j==4||j==5||j==7||j==8;envKnobs[j].setVisible(show);envLabels[j].setVisible(show);int slot=j==4?0:j==5?1:j==7?2:3;int x=free?310+216*slot:310+172*(j%5),y=free?600:501+108*(j/5);envLabels[j].setBounds(x,y,free?200:162,19);envKnobs[j].setBounds(x+(free?44:25),y+20,112,83);}envLabels[5].setText(free?"Durée totale":"Durée segment",juce::dontSendNotification);}std::array<juce::ComboBox,6> envBoxes;std::array<juce::Label,6> envBoxLabels;std::array<juce::Slider,9> envKnobs;std::array<juce::Label,9> envLabels;std::array<std::unique_ptr<CA>,6> envChoiceAtt;std::array<std::unique_ptr<SA>,9> envSliderAtt;
  void bindEnvelope(int i){selectedEnv=i;freeToggleAtt.reset();freeToggleAtt=std::make_unique<BA>(p.state,eid(i,"freeEnabled"),freeToggle);for(auto& a:envChoiceAtt)a.reset();for(auto& a:envSliderAtt)a.reset();for(int j=0;j<6;++j)envChoiceAtt[j]=std::make_unique<CA>(p.state,eid(i,envKeys[j]),envBoxes[j]);for(int j=0;j<9;++j)envSliderAtt[j]=std::make_unique<SA>(p.state,eid(i,envKeys[j+6]),envKnobs[j]);for(int j=0;j<12;++j)envTabs[j].setToggleState(i==j,juce::dontSendNotification);}
  int selectedLfo=0;juce::TextButton lfoButton{"LFO"};std::array<juce::TextButton,8> lfoTabs;
  LfoOverview lfoOverview;std::array<juce::ComboBox,6> lfoBoxes;std::array<juce::Label,6> lfoBoxLabels;std::array<juce::Slider,3> lfoKnobs;std::array<juce::Label,3> lfoLabels;std::array<std::unique_ptr<CA>,6> lfoChoiceAtt;std::array<std::unique_ptr<SA>,3> lfoSliderAtt;
  void bindLfo(int i){selectedLfo=i;for(auto& a:lfoChoiceAtt)a.reset();for(auto& a:lfoSliderAtt)a.reset();for(int j=0;j<6;++j)lfoChoiceAtt[j]=std::make_unique<CA>(p.state,lid(i,lfoKeys[j<4?j:j+3]),lfoBoxes[j]);for(int j=0;j<3;++j)lfoSliderAtt[j]=std::make_unique<SA>(p.state,lid(i,lfoKeys[j+4]),lfoKnobs[j]);for(int j=0;j<8;++j)lfoTabs[j].setToggleState(i==j,juce::dontSendNotification);}
  void setLfoPage(bool show){nexusPage=false;morphPage=false;motionPage=false;trajectoryPage=false;snapshotPage=false;mutationPage=false;fxPage=false;curvePage=false;macroPage=false;matrixPage=false;envPage=false;lfoPage=show;filterPage=false;filterModPage=false;setOscPage(false);if(show)bindLfo(selectedLfo);}
  juce::TextButton filterModButton{"MODULATION"};
  std::array<juce::Slider,9> filterModKnobs;std::array<juce::Label,9> filterModText;std::array<std::unique_ptr<SA>,9> filterModAtt;
  juce::ToggleButton filterAuto{"Compensation auto"};std::unique_ptr<BA> filterAutoAtt;FilterResponse filterResponse;
  juce::TextButton filterButton{"FILTRES"};juce::ComboBox filterA,filterB,filterRoute;
  std::array<ModSlider,8> filterKnobs;std::array<juce::Label,8> filterText;
  std::array<std::unique_ptr<SA>,8> filterAtt;std::array<std::unique_ptr<CA>,3> filterChoice;
  void bindFilters(){for(auto& a:filterModAtt)a.reset();for(int j=0;j<9;++j)filterModAtt[j]=std::make_unique<SA>(p.state,id(selected,filterModKeys[j]),filterModKnobs[j]);filterAutoAtt.reset();filterAutoAtt=std::make_unique<BA>(p.state,id(selected,"fAuto"),filterAuto);for(auto& a:filterAtt)a.reset();for(auto& a:filterChoice)a.reset();for(int j=0;j<8;++j)filterAtt[j]=std::make_unique<SA>(p.state,id(selected,filterKeys[j]),filterKnobs[j]);filterChoice[0]=std::make_unique<CA>(p.state,id(selected,"fModel"),filterA);filterChoice[1]=std::make_unique<CA>(p.state,id(selected,"f2Model"),filterB);filterChoice[2]=std::make_unique<CA>(p.state,id(selected,"fRoute"),filterRoute);}
  void setFilterPage(bool show){if(!show)filterModPage=false;filterPage=show;setOscPage(false);bindFilters();}
  juce::ComboBox tableBox;juce::TextButton tableImport{"Importer table WAV"};std::unique_ptr<CA> tableAtt;
  juce::TextButton grainButton{"GRAINS"};
  std::array<ModSlider,9> grainKnobs;
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
    lfoButton.setButtonText(lfoPage?"COUCHE":"LFO");bool lf=lfoPage&&!envPage&&!matrixPage&&!nexusPage,ev=lfoPage&&envPage&&!matrixPage,mx=lfoPage&&matrixPage&&!macroPage&&!curvePage,mc=lfoPage&&matrixPage&&macroPage,cv=lfoPage&&matrixPage&&curvePage;curveButton.setVisible(lfoPage&&matrixPage&&!macroPage);curveButton.setButtonText(cv?"RETOUR":"COURBE");customCurve.setVisible(cv);macroButton.setVisible(lfoPage&&matrixPage&&!curvePage);macroButton.setButtonText(mc?"ROUTAGES":"MACROS");detailLabel.setVisible(mx);detailOsc.setVisible(mx);detailCurve.setVisible(mx);detailSmooth.setVisible(mx);detailLive.setVisible(mx);for(int j=0;j<8;++j){macroKnobs[j].setVisible(mc);macroLabels[j].setVisible(mc);}envButton.setVisible(lfoPage&&!matrixPage&&!nexusPage);matrixButton.setVisible(lfoPage&&!nexusPage);matrixButton.setButtonText(mx?"LFO":"MATRICE");for(auto& t:matrixBanks)t.setVisible(mx);for(auto& t:matrixHeaders)t.setVisible(mx);for(int row=0;row<8;++row){matrixNumbers[row].setVisible(mx);matrixEnabled[row].setVisible(mx);matrixAmounts[row].setVisible(mx);for(auto& b:matrixBoxes[row])b.setVisible(mx);}envButton.setButtonText(ev?"LFO":"ENVELOPPES");for(auto& t:lfoTabs)t.setVisible(lf);lfoOverview.setVisible(lf);for(int j=0;j<6;++j){lfoBoxes[j].setVisible(lf);lfoBoxLabels[j].setVisible(lf);envBoxes[j].setVisible(ev);envBoxLabels[j].setVisible(ev);}for(int j=0;j<3;++j){lfoKnobs[j].setVisible(lf);lfoLabels[j].setVisible(lf);}for(auto& t:envTabs)t.setVisible(ev);envView.setVisible(ev);freeView.setVisible(false);freeToggle.setVisible(ev);for(int j=0;j<9;++j){envKnobs[j].setVisible(ev);envLabels[j].setVisible(ev);}
    filterButton.setVisible(!show&&!lfoPage);filterButton.setButtonText(filterPage?"COUCHE":"FILTRES");oscPageButton.setVisible(!filterPage&&!lfoPage);
    bool mainFilter=filterPage&&!filterModPage,modFilter=filterPage&&filterModPage;filterModButton.setVisible(filterPage);filterModButton.setButtonText(modFilter?"RÉPONSE":"MODULATION");filterAuto.setVisible(modFilter);filterResponse.setVisible(mainFilter);filterA.setVisible(mainFilter);filterB.setVisible(mainFilter);filterRoute.setVisible(mainFilter);for(int j=0;j<8;++j){filterKnobs[j].setVisible(mainFilter);filterText[j].setVisible(mainFilter);}for(int j=0;j<9;++j){filterModKnobs[j].setVisible(modFilter);filterModText[j].setVisible(modFilter);}
    for(int j=0;j<8;++j){knobs[j].setVisible(!show&&!filterPage&&!lfoPage);labels[j].setVisible(!show&&!filterPage&&!lfoPage);}
    for(auto& t:oscTabs)t.setVisible(show);
    for(int j=0;j<9;++j){oscKnobs[j].setVisible(show);oscLabels[j].setVisible(show);}
    oscMode.setVisible(show);oscWave.setVisible(show);oscSync.setVisible(show);oscRing.setVisible(show);
    importButton.setVisible(show);sampleLabel.setVisible(show);preview.setVisible(show);
    if(show)bindOsc(selectedOsc);grainVisibility();nexusVisibility();repaint();
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
  void filter(){visible.clear();auto q=search.getText();for(int i=0;i<370;++i)if(presetName(i).containsIgnoreCase(q))visible.push_back(i);browser.deselectAllRows();browser.updateContent();}
  int getNumRows()override{return int(visible.size());}
  void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool sel)override{if(row<0||row>=int(visible.size()))return;g.fillAll(sel?juce::Colour(0xff294b67):(row%2?ink:juce::Colour(0xff17202d)));g.setColour(light);g.setFont(14);g.drawText(presetName(visible[size_t(row)]),8,0,w-12,h,juce::Justification::centredLeft,true);}
  void selectedRowsChanged(int row)override{if(row>=0&&row<int(visible.size()))p.setCurrentProgram(visible[size_t(row)]);}
  void timerCallback()override{
    patchLabel.setText(p.getProgramName(p.getCurrentProgram()),juce::dontSendNotification);
    if(int learned=p.consumeLearn();learned>=0)dragStatus.setText("CC "+juce::String(learned%128)+(learned/128>=64?juce::String(learned/128==64?" → NEXUS X":" → NEXUS Y"):juce::String(" → route ")+juce::String(learned/128+1)),juce::dontSendNotification);
    if(lfoPage&&matrixPage&&!macroPage){int i=matrixBank*8+detailRow;detailLive.setText(p.learnRow.load()==i?juce::String("MIDI Learn…"):juce::String("Mod. ")+juce::String(p.matrixView[i].load()*100,1)+" %",juce::dontSendNotification);}
    auto viewPatch=p.patch();for(auto& s:knobs)updateHalo(s,viewPatch);for(auto& s:filterKnobs)updateHalo(s,viewPatch);for(auto& s:oscKnobs)updateHalo(s,viewPatch);for(auto& s:grainKnobs)updateHalo(s,viewPatch);
    if(curvePage&&lfoPage&&matrixPage)customCurve.update(viewPatch.matrix[matrixBank*8+detailRow].points);
    if(macroPage&&lfoPage&&matrixPage)for(int j=0;j<8;++j)if(!macroLabels[j].isBeingEdited()){auto name=p.state.state.getProperty(juce::Identifier(macroId(j)+"Name"),"MACRO "+juce::String(j+1)).toString();macroLabels[j].setText(name,juce::dontSendNotification);}
    if(nexusPage&&fxPage)refreshFX();if(nexusPage&&mutationPage){syncMutationHistory();mutationUndo.setEnabled(!mutationBack.empty());mutationRedo.setEnabled(!mutationForward.empty());mutationAB.setEnabled(!mutationBack.empty());}if(nexusPage&&snapshotPage)refreshSnapshots();if(nexusPage&&trajectoryPage&&snapshotSeenTrajectory!=p.trajectoryRevision.load()){snapshotSeenTrajectory=p.trajectoryRevision.load();auto t=p.readTrajectory();trajectoryEditor.set(t);trajectoryClose.setToggleState(t.closed,juce::dontSendNotification);trajectoryBezier.setToggleState(t.curve,juce::dontSendNotification);}
    if(nexusPage&&motionPage){auto clip=p.readMotion();motionGraph.update(clip,p.motionPhaseView.load(),p.motionRecording.load());motionStatus.setText(p.motionRecording.load()?juce::String("Enregistrement…"):juce::String(clip.count)+" points enregistrés",juce::dontSendNotification);}
    if(nexusPage){const auto& nx=viewPatch.nexus;std::array<float,4> w;for(int j=0;j<4;++j)w[j]=p.nexusGainView[j].load(std::memory_order_relaxed);nexusPad.update(p.nexusView[0].load(),p.nexusView[1].load(),w,nx.enabled);nxSliders[0].setEnabled(!nx.lockX);nxSliders[1].setEnabled(!nx.lockY);for(int j=0;j<2;++j){int cc=j?nx.ccY:nx.ccX;nxCCLabels[j].setText(p.learnRow.load()==64+j?juce::String("En écoute…"):cc>=0?juce::String("CC ")+juce::String(cc):juce::String("CC —"),juce::dontSendNotification);}}
    if(lfoPage&&!matrixPage&&!nexusPage){if(envPage){const auto& ep=viewPatch.modEnv[selectedEnv];layoutFree(ep.free.enabled);if(ep.free.enabled)freeView.update(ep.free);else envView.update(ep);}else{std::array<float,8> phases;for(int j=0;j<8;++j)phases[j]=p.lfoPhaseView[j].load(std::memory_order_relaxed);lfoOverview.update(viewPatch.lfo,selectedLfo,phases);}}
    if(filterPage&&!filterModPage){auto layer=viewPatch.layer[selected];filterResponse.update(layer.filter1,layer.filter2,p.getSampleRate()>0?p.getSampleRate():48000.);}
    if(oscPage){if(!grainPage){const bool table=oscMode.getSelectedId()==2;tableBox.setVisible(table);tableImport.setVisible(table);}const auto& l=viewPatch.layer[selected];auto o=l.osc[selectedOsc];auto* s=l.sample;
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
    add(freeView);add(freeToggle);freeView.onEdit=[this](const cosmos::FreeEnvelope& f){auto set=[&](int j,float v){if(auto* par=p.state.getParameter(eid(selectedEnv,freeKey(j).toRawUTF8()))){if(std::abs(par->convertFrom0to1(par->getValue())-v)<1e-6f)return;par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(v));par->endChangeGesture();}};for(int j=0;j<16;++j){set(2+j,f.x[j]);set(18+j,f.y[j]);set(34+j,float(f.curve[j]));}set(1,float(f.count));};
    add(envView);envView.onEdit=[this](int which,float v,bool begin,bool end){if(auto* par=p.state.getParameter(eid(selectedEnv,which?"segmentLevel":"sustain"))){if(begin)par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(v));if(end)par->endChangeGesture();}};
    matrixButton.onClick=[this]{curvePage=false;macroPage=false;matrixPage=!matrixPage;envPage=false;setOscPage(false);if(matrixPage)bindMatrix(matrixBank);else bindLfo(selectedLfo);};add(matrixButton);
    const char* headers[]={"N°","Actif","Source","Destination","Couche","Polarité","Profondeur"};
    for(int j=0;j<7;++j)label(matrixHeaders[j],juce::String::fromUTF8(headers[j]),13);
    const juce::StringArray polarity{"Unipolaire","Bipolaire"};const juce::StringArray* matrixLists[]={&matrixSources,&matrixDests,&lfoLayers,&polarity};
    for(int row=0;row<8;++row){matrixBanks[row].setButtonText(juce::String(row*8+1)+"–"+juce::String(row*8+8));matrixBanks[row].onClick=[this,row]{bindMatrix(row);};add(matrixBanks[row]);add(matrixNumbers[row]);matrixNumbers[row].onClick=[this,row]{bindDetail(row);};matrixNumbers[row].onLearn=[this,row]{bindDetail(row);p.learnResult.store(-1);p.learnRow.store(matrixBank*8+row);detailLive.setText("MIDI Learn…",juce::dontSendNotification);};matrixNumbers[row].setTooltip("Clic : détails / clic droit : MIDI Learn CC");add(matrixEnabled[row]);for(int j=0;j<4;++j){matrixBoxes[row][j].addItemList(*matrixLists[j],1);add(matrixBoxes[row][j]);}matrixAmounts[row].setSliderStyle(juce::Slider::LinearHorizontal);matrixAmounts[row].setTextBoxStyle(juce::Slider::TextBoxRight,false,48,24);add(matrixAmounts[row]);}
    macroButton.onClick=[this]{curvePage=false;macroPage=!macroPage;setOscPage(false);if(!macroPage)bindMatrix(matrixBank);};add(macroButton);
    for(int j=0;j<8;++j){configure(macroKnobs[j]);label(macroLabels[j],"MACRO "+juce::String(j+1),15);macroAtt[j]=std::make_unique<SA>(p.state,macroId(j),macroKnobs[j]);macroLabels[j].setEditable(false,true,false);macroLabels[j].onTextChange=[this,j]{auto name=macroLabels[j].getText().trim().substring(0,24);p.state.state.setProperty(juce::Identifier(macroId(j)+"Name"),name,nullptr);};}
    label(detailLabel,"Route",13);label(detailLive,"",12);detailOsc.addItemList(matrixOscs,1);detailOsc.setTooltip("Oscillateur cible (destinations oscillateurs uniquement)");add(detailOsc);detailCurve.addItemList(matrixCurves,1);detailCurve.setTooltip("Courbe de transformation");add(detailCurve);detailSmooth.setSliderStyle(juce::Slider::LinearHorizontal);detailSmooth.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,24);detailSmooth.setTextValueSuffix(" ms");detailSmooth.setTooltip("Lissage MIDI");add(detailSmooth);
    dragSource.addItemList(matrixSources,1);dragSource.setSelectedId(2,juce::dontSendNotification);add(dragSource);dragChip.setButtonText("GLISSER");dragChip.source=[this]{return dragSource.getSelectedId()-1;};dragChip.setTooltip("Glisser la source sur un potentiomètre compatible");add(dragChip);label(dragStatus,"",11);
    setupTarget(knobs[0],3);setupTarget(knobs[1],4);setupTarget(knobs[2],5);setupTarget(knobs[3],1);setupTarget(filterKnobs[0],1);setupTarget(filterKnobs[1],11);setupTarget(filterKnobs[3],2);setupTarget(filterKnobs[4],12);setupTarget(oscKnobs[1],5,true);setupTarget(oscKnobs[5],6,true);setupTarget(oscKnobs[6],10,true);setupTarget(oscKnobs[7],9,true);setupTarget(grainKnobs[1],8,true);setupTarget(grainKnobs[2],7,true);
    curveButton.onClick=[this]{curvePage=!curvePage;setOscPage(false);};add(curveButton);add(customCurve);customCurve.onEdit=[this](int j,float v,bool begin,bool end){int i=matrixBank*8+detailRow;if(auto* par=p.state.getParameter(mid(i,matrixKeys[9+j]))){if(begin)par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(v));if(end)par->endChangeGesture();}};
    mutationButton.onClick=[this]{mutationPage=!mutationPage;snapshotPage=trajectoryPage=motionPage=morphPage=false;syncMutationHistory();mutationSeed.setText(juce::String(int(p.mutationParams[2]->load())),false);nexusVisibility();};add(mutationButton);
    for(auto* c:std::array<juce::Component*,9>{&mutationGenerate,&mutationUndo,&mutationRedo,&mutationAB,&mutationSnapshot,&mutationExport,&mutationMode,&mutationAmount,&mutationSeed})add(*c);
    mutationMode.addItemList(mutationModes,1);mutationModeAtt=std::make_unique<CA>(p.state,mutationKeys[0],mutationMode);mutationAmount.setSliderStyle(juce::Slider::LinearHorizontal);mutationAmount.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,22);mutationAmountAtt=std::make_unique<SA>(p.state,mutationKeys[1],mutationAmount);mutationSeed.setInputRestrictions(8,"0123456789");mutationSeed.onTextChange=[this]{if(auto* par=p.state.getParameter(mutationKeys[2]))par->setValueNotifyingHost(par->convertTo0to1(float(std::clamp(mutationSeed.getText().getIntValue(),1,16777215))));};
    const char* mn[]={"Protéger accordage","Protéger volume","Oscillateurs","Wavetable","FM / AM","Granulaire","Filtres","Enveloppes","LFO","Matrice"};for(int j=0;j<10;++j){mutationToggles[j].setButtonText(juce::String::fromUTF8(mn[j]));add(mutationToggles[j]);mutationToggleAtt[j]=std::make_unique<BA>(p.state,mutationKeys[j+3],mutationToggles[j]);}
    label(mutationAmountLabel,"Intensité",14);label(mutationSeedLabel,"Graine",14);label(mutationInfo,"Mutation déterministe et non destructive pour les presets Factory",15);
    mutationGenerate.onClick=[this]{generateMutation();};mutationUndo.onClick=[this]{syncMutationHistory();if(!mutationBack.empty()){p.applyMutation(mutationBack.back(),false);mutationForward.push_back(std::move(mutationBack.back()));mutationBack.pop_back();mutationOriginal=false;}};mutationRedo.onClick=[this]{syncMutationHistory();if(!mutationForward.empty()){p.applyMutation(mutationForward.back(),true);mutationBack.push_back(std::move(mutationForward.back()));mutationForward.pop_back();mutationOriginal=false;}};mutationAB.onClick=[this]{syncMutationHistory();if(!mutationBack.empty()){mutationOriginal=!mutationOriginal;p.applyMutation(mutationBack.back(),!mutationOriginal);}};mutationSnapshot.onClick=[this]{p.captureSnapshot(snapshotSelected,"Mutation "+juce::String(int(p.mutationParams[2]->load())),1);};
    mutationExport.onClick=[this]{chooser=std::make_unique<juce::FileChooser>("Exporter preset Omniscience",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Mutation.omniscience"),"*.omniscience");auto safe=juce::Component::SafePointer<Editor>(this);chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe](const juce::FileChooser& f){if(safe&&f.getResult()!=juce::File()){juce::MemoryBlock b;safe->p.getStateInformation(b);bool ok=f.getResult().withFileExtension("omniscience").replaceWithData(b.getData(),b.getSize());safe->mutationInfo.setText(ok?"Preset exporté":"Échec de l'export",juce::dontSendNotification);}});};
    add(fxButton);fxButton.onClick=[this]{if(fxPage){showNexus(false);}else{showNexus(true);fxPage=true;nexusVisibility();bindFX();refreshFX();}};
    const char* rackNames[]={"A","B","C","D","MASTER"};for(int j=0;j<5;++j){add(fxRackButtons[j]);fxRackButtons[j].setButtonText(rackNames[j]);fxRackButtons[j].onClick=[this,j]{fxRack=j;bindFX();refreshFX();};}
    for(int j=0;j<8;++j){add(fxSlotButtons[j]);fxSlotButtons[j].index=j;fxSlotButtons[j].moved=[this](int a,int b){p.swapFX(fxRack,a,b);fxSlot=b;bindFX();refreshFX();};fxSlotButtons[j].onClick=[this,j]{fxSlot=j;bindFX();refreshFX();};}
    for(auto* c:std::array<juce::Component*,12>{&fxUp,&fxDown,&fxFavorite,&fxLoad,&fxPreset,&fxType,&fxMacro,&fxBrowser,&fxEnabled,&fxParallel,&fxOnlyFav,&fxSearch})add(*c);
    add(fxSync);add(fxStereo);add(fxFreeze);add(fxDelayRight);add(fxDelayTone);add(fxDelayToneLabel);add(fxEarlyLabel);add(fxModLabel);fxEarlyLabel.setText("Réflexions",juce::dontSendNotification);fxModLabel.setText("Mod.",juce::dontSendNotification);fxDelayToneLabel.setText("Filtre feedback",juce::dontSendNotification);for(auto* s:{&fxDelayRight,&fxDelayTone}){s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,45,22);}fxDelayRight.setTooltip("Feedback droit");fxDelayTone.setTooltip("Fréquence de coupure dans la boucle");fxStereo.setSliderStyle(juce::Slider::LinearHorizontal);fxStereo.setTextBoxStyle(juce::Slider::TextBoxRight,false,45,22);fxStereo.setTooltip("Décalage de phase stéréo");add(fxAuto);add(fxQuality);add(fxProfile);add(fxMidi);add(fxMidiClear);add(fxMeter);fxQuality.addItemList({"1x","2x","4x"},1);fxProfile.addItemList({"Doux","Équilibré","Intense"},1);fxProfile.onChange=[this]{int i=fxProfile.getSelectedId();if(i>0){int t=int(p.fxParams[fxRack][fxSlot][0]->load());if(t>=79){auto q=cosmos::creativePreset(t,i);for(int j=0;j<2;++j)if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,fxKeys[5+j])))par->setValueNotifyingHost(q[j]);if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,"delayFreeze")))par->setValueNotifyingHost(0);return;}if(t>=64){auto q=cosmos::reverbPreset(t,i);const int keys[]={5,6,14,15,13};for(int j=0;j<5;++j)if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,fxKeys[keys[j]])))par->setValueNotifyingHost(q[j]);if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,"delayFreeze")))par->setValueNotifyingHost(0);return;}if(t>=49){auto q=cosmos::echoPreset(t,i);const int keys[]={5,6,14,15};for(int j=0;j<4;++j)if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,fxKeys[keys[j]])))par->setValueNotifyingHost(q[j]);if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,"delayFreeze")))par->setValueNotifyingHost(0);return;}for(int j=5;j<=6;++j)if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,fxKeys[j])))par->setValueNotifyingHost(i==1?.2f:i==2?.5f:.8f);}};fxMidi.onClick=[this]{p.learnRow.store(-1);p.fxLearn.store(fxRack*8+fxSlot);};fxMidiClear.onClick=[this]{p.fxLearn.store(-1);p.fxLearnResult.store(-1);if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,"cc")))par->setValueNotifyingHost(0);};
    fxType.addItemList(fxNames,1);fxMacro.addItemList(nexusMacroChoices,1);fxSearch.setTextToShowWhenEmpty("Rechercher un effet…",juce::Colour(0xff7893aa));fxSearch.onTextChange=[this]{refreshFXBrowser();};fxOnlyFav.onClick=[this]{refreshFXBrowser();};fxFavorite.onClick=[this]{int j=std::clamp(int(p.fxParams[fxRack][fxSlot][0]->load()),0,cosmos::fxTypes-1);if(j){p.fxFav[j].store(!p.fxFav[j].load());refreshFXBrowser();}};
    fxLoad.onClick=[this]{int j=fxBrowser.getSelectedId()-1;if(j>0&&j<cosmos::fxTypes){auto* type=p.state.getParameter(fxid(fxRack,fxSlot,"type"));auto* on=p.state.getParameter(fxid(fxRack,fxSlot,"enabled"));type->setValueNotifyingHost(type->convertTo0to1(float(j)));on->setValueNotifyingHost(1);refreshFX();}};
    fxUp.onClick=[this]{if(fxSlot>0){p.swapFX(fxRack,fxSlot,fxSlot-1);--fxSlot;bindFX();refreshFX();}};fxDown.onClick=[this]{if(fxSlot<7){p.swapFX(fxRack,fxSlot,fxSlot+1);++fxSlot;bindFX();refreshFX();}};fxPreset.onClick=[this]{for(int j=2;j<17;++j)if(auto* par=p.state.getParameter(fxid(fxRack,fxSlot,fxKeys[j])))par->setValueNotifyingHost(par->getDefaultValue());};
    const char* fl[]={"Dry / Wet","Entrée","Sortie","Paramètre A","Paramètre B","Macro profondeur"};for(int j=0;j<6;++j){label(fxLabels[j],juce::String::fromUTF8(fl[j]),13);add(fxSliders[j]);fxSliders[j].setSliderStyle(juce::Slider::LinearHorizontal);fxSliders[j].setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);}bindFX();refreshFXBrowser();refreshFX();
    add(snapshotXY);snapshotXYAtt=std::make_unique<BA>(p.state,qcKeys[0],snapshotXY);for(int j=0;j<4;++j){add(snapshotCorners[j]);snapshotCorners[j].addItem("Vide",1);for(int i=1;i<=16;++i)snapshotCorners[j].addItem(juce::String::charToString(juce::juce_wchar('A'+j))+" : S"+juce::String(i),i+1);cornerAtt[j]=std::make_unique<CA>(p.state,qcKeys[j+1],snapshotCorners[j]);}
    add(motionOverdub);overdubAtt=std::make_unique<BA>(p.state,qcKeys[5],motionOverdub);add(motionQuant);motionQuant.addItemList({"Libre","1/4","1/8","1/16","1/32"},1);motionQuantAtt=std::make_unique<CA>(p.state,qcKeys[6],motionQuant);
    add(trajectoryImport);add(trajectoryExport);label(trajectoryIO,"",12);trajectoryImport.onClick=[this]{trajectoryFile(false);};trajectoryExport.onClick=[this]{trajectoryFile(true);};
    snapshotButton.onClick=[this]{snapshotPage=!snapshotPage;trajectoryPage=motionPage=morphPage=mutationPage=false;nexusVisibility();if(snapshotPage){selectSnapshot(snapshotSelected);refreshSnapshots();}};add(snapshotButton);
    for(int j=0;j<16;++j){add(snapshotSlots[j]);snapshotSlots[j].onClick=[this,j]{selectSnapshot(j);refreshSnapshots();};}
    for(auto* c:std::array<juce::Component*,13>{&snapshotCapture,&snapshotRecall,&snapshotDelete,&snapshotCopy,&snapshotPaste,&snapshotAB,&snapshotName,&snapshotColour,&snapshotMacro,&snapshotTune,&snapshotGain,&snapshotMidi,&snapshotTime})add(*c);
    snapshotColour.addItemList({"Bleu","Émeraude","Violet","Or","Rouge","Cyan"},1);snapshotColour.setSelectedId(1,juce::dontSendNotification);
    snapshotName.setTextToShowWhenEmpty("Nom de la mémoire",juce::Colour(0xff7893aa));snapshotCapture.onClick=[this]{p.captureSnapshot(snapshotSelected,snapshotName.getText(),snapshotColour.getSelectedId()-1);refreshSnapshots();};snapshotRecall.onClick=[this]{p.snapshotRequest.store(snapshotSelected);};snapshotDelete.onClick=[this]{p.putSnapshot(snapshotSelected,{});selectSnapshot(snapshotSelected);refreshSnapshots();};snapshotCopy.onClick=[this]{snapshotClipboard=p.snapshotSlot(snapshotSelected);};snapshotPaste.onClick=[this]{if(snapshotClipboard.isValid()){p.putSnapshot(snapshotSelected,snapshotClipboard.createCopy());selectSnapshot(snapshotSelected);refreshSnapshots();}};snapshotAB.onClick=[this]{p.snapshotRequest.store(p.snapshotActive.load()==0?1:0);};
    snapshotTime.setSliderStyle(juce::Slider::LinearHorizontal);snapshotTime.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,22);snapshotTime.setTextValueSuffix(" s");snapshotTimeAtt=std::make_unique<SA>(p.state,snapshotKeys[0],snapshotTime);snapshotMacro.addItemList(nexusMacroChoices,1);snapshotMacroAtt=std::make_unique<CA>(p.state,snapshotKeys[4],snapshotMacro);std::array<juce::ToggleButton*,3> sb{&snapshotTune,&snapshotGain,&snapshotMidi};for(int j=0;j<3;++j)snapshotToggleAtt[j]=std::make_unique<BA>(p.state,snapshotKeys[j+1],*sb[j]);label(snapshotTimeLabel,"Transition",13);label(snapshotMacroLabel,"Sélection",13);label(snapshotStatus,"Sélectionner une mémoire",13);selectSnapshot(0);refreshSnapshots();
    trajectoryButton.onClick=[this]{trajectoryPage=!trajectoryPage;motionPage=morphPage=snapshotPage=mutationPage=false;nexusVisibility();};add(trajectoryButton);add(trajectoryEditor);add(trajectoryList);add(trajectorySearch);add(trajectoryOnlyFav);add(trajectoryClose);add(trajectoryBezier);add(trajectoryEnable);add(trajectoryApply);add(trajectoryUndo);add(trajectoryRedo);add(trajectoryStar);add(trajectoryEase);add(trajectoryMacro);
    trajectorySearch.setTextToShowWhenEmpty("Rechercher…",juce::Colour(0xff7893aa));trajectorySearch.onTextChange=[this]{refreshTrajectoryList();};trajectoryOnlyFav.onClick=[this]{refreshTrajectoryList();};trajectoryList.onChange=[this]{int i=trajectoryList.getSelectedId()-1;if(i>=0&&i<50){rememberTrajectory();p.trajectoryFactory.store(i);loadTrajectory(cosmos::factoryTrajectory(i));}};trajectoryStar.onClick=[this]{int i=p.trajectoryFactory.load();p.trajectoryFav[i].store(!p.trajectoryFav[i].load());refreshTrajectoryList();};trajectoryEditor.beforeEdit=[this]{rememberTrajectory();};trajectoryEditor.changed=[this]{p.saveTrajectory(trajectoryEditor.value());};
    trajectoryClose.onClick=[this]{rememberTrajectory();auto t=trajectoryEditor.value();t.closed=trajectoryClose.getToggleState();loadTrajectory(t);};trajectoryBezier.onClick=[this]{rememberTrajectory();auto t=trajectoryEditor.value();t.curve=trajectoryBezier.getToggleState();loadTrajectory(t);};trajectoryApply.onClick=[this]{applyTrajectory();};trajectoryUndo.onClick=[this]{if(!trajectoryBack.empty()){trajectoryForward.push_back(trajectoryEditor.value());loadTrajectory(trajectoryBack.back());trajectoryBack.pop_back();}};trajectoryRedo.onClick=[this]{if(!trajectoryForward.empty()){trajectoryBack.push_back(trajectoryEditor.value());loadTrajectory(trajectoryForward.back());trajectoryForward.pop_back();}};
    trajectoryEnableAtt=std::make_unique<BA>(p.state,trajectoryKeys[0],trajectoryEnable);trajectoryEase.addItemList({"Constante","Accélérer","Ralentir","Lissée"},1);trajectoryEaseAtt=std::make_unique<CA>(p.state,trajectoryKeys[5],trajectoryEase);trajectoryMacro.addItemList(nexusMacroChoices,1);trajectoryMacroAtt=std::make_unique<CA>(p.state,trajectoryKeys[7],trajectoryMacro);const char* tl[]={"Échelle X","Échelle Y","Centre X","Centre Y","Variation"};for(int j=0;j<5;++j){label(trajectoryLabels[j],juce::String::fromUTF8(tl[j]),13);trajectorySliders[j].setSliderStyle(juce::Slider::LinearHorizontal);trajectorySliders[j].setTextBoxStyle(juce::Slider::TextBoxRight,false,60,22);add(trajectorySliders[j]);trajectoryAtt[j]=std::make_unique<SA>(p.state,trajectoryKeys[j==4?6:j+1],trajectorySliders[j]);}refreshTrajectoryList();loadTrajectory(p.readTrajectory());
    motionButton.onClick=[this]{motionPage=!motionPage;morphPage=false;trajectoryPage=false;snapshotPage=mutationPage=false;nexusVisibility();};add(motionButton);add(motionGraph);add(motionRec);add(motionStop);add(motionClear);add(motionPlay);add(motionSync);motionRec.onClick=[this]{p.motionCommand.store(1);};motionStop.onClick=[this]{p.motionCommand.store(2);if(auto* par=p.state.getParameter(motionKeys[0]))par->setValueNotifyingHost(0);};motionClear.onClick=[this]{p.resetMotion();};motionPlayAtt=std::make_unique<BA>(p.state,motionKeys[0],motionPlay);motionSyncAtt=std::make_unique<BA>(p.state,motionKeys[1],motionSync);motionLength.addItemList(motionBars,1);add(motionLength);motionLengthAtt=std::make_unique<CA>(p.state,motionKeys[2],motionLength);motionMode.addItemList(motionModes,1);add(motionMode);motionModeAtt=std::make_unique<CA>(p.state,motionKeys[3],motionMode);const char* ml[]={"Vitesse","Intensité","Rotation","Durée libre (s)"};for(int j=0;j<4;++j){label(motionLabels[j],juce::String::fromUTF8(ml[j]),13);motionSliders[j].setSliderStyle(juce::Slider::LinearHorizontal);motionSliders[j].setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);add(motionSliders[j]);motionSliderAtt[j]=std::make_unique<SA>(p.state,motionKeys[4+j],motionSliders[j]);}label(motionStatus,"Aucun mouvement",13);
    morphButton.onClick=[this]{morphPage=!morphPage;motionPage=false;trajectoryPage=false;snapshotPage=mutationPage=false;nexusVisibility();};add(morphButton);const char* morphNames[]={"Activer morphing","Volume / panoramique","Oscillateurs","Granulaire","Filtres","Enveloppes","Protéger accordage","Protéger volume"};for(int j=0;j<8;++j){morphToggles[j].setButtonText(juce::String::fromUTF8(morphNames[j]));add(morphToggles[j]);morphToggleAtt[j]=std::make_unique<BA>(p.state,morphKeys[j==0?0:j+2],morphToggles[j]);}label(morphAmountLabel,"Intensité",13);morphAmount.setSliderStyle(juce::Slider::LinearHorizontal);morphAmount.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);add(morphAmount);morphAmountAtt=std::make_unique<SA>(p.state,morphKeys[1],morphAmount);label(morphCurveLabel,"Courbe",13);morphCurve.addItemList(morphCurves,1);add(morphCurve);morphCurveAtt=std::make_unique<CA>(p.state,morphKeys[2],morphCurve);label(morphInfo,"Les couches originales restent intactes.",14);
    nexusButton.onClick=[this]{showNexus(!nexusPage);};add(nexusButton);add(nexusPad);
    const char* nxNames[]={"Activer NEXUS","Puissance constante","Verrouiller X","Verrouiller Y","Retour au centre","Inverser X","Inverser Y"};const int nxIds[]={0,3,5,6,7,8,9};
    for(int j=0;j<7;++j){nxToggles[j].setButtonText(juce::String::fromUTF8(nxNames[j]));add(nxToggles[j]);nxToggleAtt[j]=std::make_unique<BA>(p.state,nexusKeys[nxIds[j]],nxToggles[j]);}
    for(int j=0;j<3;++j){label(nxSliderLabels[j],j==0?"Axe X":j==1?"Axe Y":"Lissage",13);nxSliders[j].setSliderStyle(juce::Slider::LinearHorizontal);nxSliders[j].setTextBoxStyle(juce::Slider::TextBoxRight,false,60,22);if(j==2)nxSliders[j].setTextValueSuffix(" ms");add(nxSliders[j]);nxSliderAtt[j]=std::make_unique<SA>(p.state,nexusKeys[j==2?4:j+1],nxSliders[j]);}
    for(int j=0;j<2;++j){label(nxMacroLabels[j],j==0?"Source X":"Source Y",13);nxMacros[j].addItemList(nexusMacroChoices,1);add(nxMacros[j]);nxMacroAtt[j]=std::make_unique<CA>(p.state,nexusKeys[10+j],nxMacros[j]);label(nxCCLabels[j],"CC —",12);nxLearn[j].setButtonText("MIDI Learn");add(nxLearn[j]);nxLearn[j].onClick=[this,j]{if(p.learnRow.load()<0&&p.learnResult.load()<0)p.learnRow.store(64+j);};nxClear[j].setButtonText("Effacer");add(nxClear[j]);nxClear[j].onClick=[this,j]{if(p.learnRow.load()==64+j)p.learnRow.store(-1);if(auto* par=p.state.getParameter(nexusKeys[12+j])){par->beginChangeGesture();par->setValueNotifyingHost(par->convertTo0to1(-1.f));par->endChangeGesture();}};}
    nexusPad.onMove=[this](float x,float y,bool begin,bool end){auto nx=p.patch().nexus;for(int j=0;j<2;++j)if(auto* par=p.state.getParameter(nexusKeys[j+1])){bool locked=j?nx.lockY:nx.lockX;if(begin&&!locked){par->beginChangeGesture();nxGesture[j]=true;}if(nxGesture[j]&&!locked)par->setValueNotifyingHost(end&&nx.center?.5f:j?y:x);if(end&&nxGesture[j]){par->endChangeGesture();nxGesture[j]=false;}}};
    selectLayer(0);setOscPage(false);timerCallback();startTimerHz(20);
    setResizable(true,true);setResizeLimits(900,570,1800,1140);if(auto* c=getConstrainer())c->setFixedAspectRatio(1200./760.);setSize(1200,760);
  }
  #ifdef COSMOS_PRESET_QA
  bool qualifyModulationUI(const juce::File& folder){
    p.setCurrentProgram(0);selectLayer(1);setLfoPage(false);
    juce::DragAndDropTarget::SourceDetails drop(juce::var("cosmos-source:155"),&dragChip,{0,0});
    juce::DragAndDropTarget::SourceDetails invalid(juce::var("cosmos-source:999"),&dragChip,{0,0});
    bool ok=knobs[0].isInterestedInDragSource(drop)&&!knobs[0].isInterestedInDragSource(invalid)&&!knobs[4].isInterestedInDragSource(drop);
    knobs[0].itemDropped(drop);auto patch=p.patch();ok&=patch.matrix[0].enabled&&patch.matrix[0].source==155&&patch.matrix[0].destination==3&&patch.matrix[0].layer==2;
    // Use real processor telemetry, not fabricated UI state.
    if(auto* m=p.state.getParameter(macroId(0)))m->setValueNotifyingHost(.8f);
    p.prepareToPlay(48000,128);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);p.processBlock(b,midi);
    timerCallback();ok&=bool(knobs[0].getProperties().getWithDefault("modAssigned",false))&&std::abs(float(knobs[0].getProperties().getWithDefault("modValue",0.f))-.2f)<1e-5f;
    auto file=folder.getChildFile("modulation-halo.png");juce::FileOutputStream out(file);juce::PNGImageFormat png;ok&=out.openedOk()&&png.writeImageToStream(createComponentSnapshot(getLocalBounds()),out);
    knobs[0].itemDropped(drop);patch=p.patch();ok&=patch.matrix[0].enabled&&patch.matrix[1].enabled&&patch.matrix[1].source==155;
    selectLayer(2);setOscPage(true);bindOsc(3);oscKnobs[5].itemDropped(drop);patch=p.patch();ok&=patch.matrix[2].layer==3&&patch.matrix[2].oscillator==4&&patch.matrix[2].destination==6;
    p.setCurrentProgram(0);selectLayer(0);setLfoPage(false);return ok;
  }
  bool qualifyLayout(const juce::File& folder){
    bool ok=true;p.setCurrentProgram(0);auto* cutoff=p.state.getParameter(id(0,"cutoff"));if(!cutoff)return false;for(int j=0;j<16;++j){cutoff->setValueNotifyingHost(.2f+.03f*j);ok&=p.captureSnapshot(j,"QA "+juce::String(j+1),j%6);}
    auto tr=cosmos::factoryTrajectory(7);tr.x[0]=.123f;p.saveTrajectory(tr);juce::MemoryBlock saved;p.getStateInformation(saved);p.setCurrentProgram(1);p.setStateInformation(saved.getData(),int(saved.getSize()));for(int j=0;j<16;++j)ok&=p.snapshotMetadata(j).first=="QA "+juce::String(j+1);ok&=std::abs(p.readTrajectory().x[0]-.123f)<1e-6f;ok&=p.startSnapshot(0);p.advanceSnapshot(1);ok&=std::abs(cutoff->getValue()-.2f)<1e-5f;
    p.nexusView[0].store(1);p.nexusView[1].store(1);p.updateSnapshotCorners();ok&=std::abs(cutoff->getValue()-.23f)<1e-5f;p.setCurrentProgram(0);
    juce::Logger::writeToLog(ok?"NEXUS snapshot bank, recall, XY corners and editable trajectory state: passed":"NEXUS snapshot QA FAILED");
    for(int width:{900,1200,1800})for(int page=0;page<18;++page){
      setSize(width,int(width*760./1200.));grainPage=page==2;filterPage=false;lfoPage=false;matrixPage=false;nexusPage=false;setOscPage(page==1||page==2);if(page==3||page==4){filterModPage=page==4;setFilterPage(true);}if(page>=5){setLfoPage(true);if(page==6||page==10){envPage=true;setOscPage(false);bindEnvelope(0);if(auto* par=p.state.getParameter(eid(0,"freeEnabled")))par->setValueNotifyingHost(page==10?1.f:0.f);}if(page>=7&&page<=9){matrixPage=true;macroPage=page==8;curvePage=page==9;setOscPage(false);bindMatrix(7);}}if(page>=11){showNexus(true);morphPage=page==12;motionPage=page==13;trajectoryPage=page==14;snapshotPage=page==15;mutationPage=page==16;fxPage=page==17;nexusVisibility();}timerCallback();
      for(int i=0;i<canvas.getNumChildComponents();++i){auto* a=canvas.getChildComponent(i);if(!a->isVisible())continue;
        ok&=canvas.getLocalBounds().contains(a->getBounds());
        for(int j=i+1;j<canvas.getNumChildComponents();++j){auto* b=canvas.getChildComponent(j);if(b->isVisible()&&a->getBounds().intersects(b->getBounds())){ok=false;juce::Logger::writeToLog("Layout overlap: width="+juce::String(width)+" page="+juce::String(page)+" "+a->getName()+" ["+a->getBounds().toString()+"] / "+b->getName()+" ["+b->getBounds().toString()+"]");}}
      }
      auto image=createComponentSnapshot(getLocalBounds());auto file=folder.getChildFile(juce::String(width)+"-page-"+juce::String(page)+".png");
      juce::FileOutputStream out(file);juce::PNGImageFormat png;ok&=out.openedOk()&&png.writeImageToStream(image,out);
    }
    if(auto* par=p.state.getParameter(eid(0,"freeEnabled")))par->setValueNotifyingHost(0.f);return ok;
  }
  #endif
  ~Editor()override{p.learnRow.store(-1);stopTimer();browser.setModel(nullptr);setLookAndFeel(nullptr);}
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
    g.setColour(cyan);g.setFont(17);g.drawText("BIBLIOTHÈQUE  /  370",25,162,242,28,juce::Justification::centred);
    g.setColour(juce::Colour(0xff35495e));if(!oscPage&&!filterPage&&!lfoPage)for(int j=0;j<8;++j)g.drawRoundedRectangle(310.f+216.f*(j%4),316.f+180.f*(j/4),204.f,168.f,6.f,1.f);
    if(oscPage)for(int j=0;j<10;++j)g.drawRoundedRectangle(310.f+172.f*(j%5),449.f+132.f*(j/5),164.f,126.f,6.f,1.f);
    g.setColour(juce::Colour(0xff9ab6ce));g.setFont(13);g.drawText("v0.7.5  COSMOS SPECTRAL & CREATIVE COLLECTION  •  4 couches / 20 oscillateurs / 32 voix",18,733,1155,24,juce::Justification::centredRight);
  }
  void resized()override{
    canvas.setTransform(juce::AffineTransform::scale(float(getWidth())/1200.f));canvas.setBounds(0,0,1200,760);
    search.setBounds(26,199,242,30);browser.setBounds(26,239,242,478);nexusButton.setBounds(290,101,118,32);patchLabel.setBounds(414,101,336,32);dragSource.setBounds(755,101,160,30);dragChip.setBounds(920,101,100,30);dragStatus.setBounds(755,78,265,20);
    masterLabel.setBounds(1040,8,140,20);master.setBounds(1045,30,130,102);
    for(int k=0;k<4;++k)tabs[k].setBounds(292+225*k,151,219,52);
    layerTitle.setBounds(312,227,280,32);lfoButton.setBounds(600,230,150,30);envButton.setBounds(760,230,175,30);matrixButton.setBounds(945,230,220,30);macroButton.setBounds(760,230,85,30);curveButton.setBounds(850,230,85,30);customCurve.setBounds(310,310,855,390);
    mutationButton.setBounds(455,230,137,30);mutationMode.setBounds(310,280,410,32);mutationAmountLabel.setBounds(310,328,105,28);mutationAmount.setBounds(420,328,300,28);mutationSeedLabel.setBounds(310,370,105,28);mutationSeed.setBounds(420,370,300,28);for(int j=0;j<10;++j)mutationToggles[j].setBounds(765+200*(j%2),280+55*(j/2),195,35);mutationGenerate.setBounds(310,425,410,42);mutationUndo.setBounds(310,478,200,32);mutationRedo.setBounds(520,478,200,32);mutationAB.setBounds(310,521,410,32);mutationSnapshot.setBounds(310,566,200,32);mutationExport.setBounds(520,566,200,32);mutationInfo.setBounds(310,650,855,48);
    patchLabel.setBounds(414,101,270,32);dragSource.setBounds(689,101,150,30);dragChip.setBounds(844,101,110,30);fxButton.setBounds(959,101,60,30);
    for(int j=0;j<5;++j)fxRackButtons[j].setBounds(310+171*j,270,165,30);for(int j=0;j<8;++j)fxSlotButtons[j].setBounds(310,310+j*32,400,29);fxMeter.setBounds(310,570,400,55);fxAuto.setBounds(310,630,125,25);fxQuality.setBounds(440,630,80,25);fxProfile.setBounds(525,630,185,25);fxMidi.setBounds(310,697,195,28);fxMidiClear.setBounds(515,697,195,28);fxUp.setBounds(310,660,195,30);fxDown.setBounds(515,660,195,30);fxSearch.setBounds(735,310,280,28);fxOnlyFav.setBounds(1020,310,145,28);fxBrowser.setBounds(735,342,300,28);fxLoad.setBounds(1040,342,125,28);fxType.setBounds(735,376,255,28);fxEnabled.setBounds(995,376,85,28);fxParallel.setBounds(1085,376,80,28);for(int j=0;j<6;++j){fxLabels[j].setBounds(735,411+j*35,125,28);fxSliders[j].setBounds(865,411+j*35,300,28);}fxMacro.setBounds(735,626,210,28);fxSync.setBounds(950,626,78,28);fxStereo.setBounds(1033,626,132,28);fxFreeze.setBounds(310,630,88,25);fxEarlyLabel.setBounds(950,626,78,28);fxModLabel.setBounds(399,630,38,25);fxDelayRight.setBounds(1033,626,132,28);fxDelayToneLabel.setBounds(735,697,125,28);fxDelayTone.setBounds(865,697,300,28);fxFavorite.setBounds(735,660,205,30);fxPreset.setBounds(950,660,215,30);fxDelayVisibility(nexusPage&&fxPage,int(p.fxParams[fxRack][fxSlot][0]->load()));
    snapshotButton.setBounds(312,230,137,30);for(int j=0;j<16;++j)snapshotSlots[j].setBounds(310+225*(j%2),270+55*(j/2),219,48);snapshotName.setBounds(775,270,390,30);snapshotColour.setBounds(775,306,390,30);snapshotCapture.setBounds(775,345,190,30);snapshotRecall.setBounds(970,345,195,30);snapshotCopy.setBounds(775,382,125,30);snapshotPaste.setBounds(905,382,125,30);snapshotDelete.setBounds(1035,382,130,30);snapshotAB.setBounds(775,419,390,30);snapshotTimeLabel.setBounds(775,459,95,30);snapshotTime.setBounds(875,459,290,30);snapshotTune.setBounds(775,500,390,26);snapshotGain.setBounds(775,532,390,26);snapshotMidi.setBounds(775,564,390,26);snapshotMacroLabel.setBounds(775,604,95,28);snapshotMacro.setBounds(875,604,290,28);snapshotXY.setBounds(775,637,135,25);snapshotStatus.setBounds(915,637,250,25);for(int j=0;j<4;++j)snapshotCorners[j].setBounds(775+98*j,674,94,28);
    trajectoryButton.setBounds(600,230,150,30);trajectoryEditor.setBounds(775,270,390,115);trajectoryImport.setBounds(775,390,120,28);trajectoryExport.setBounds(900,390,120,28);trajectoryIO.setBounds(1025,390,140,28);trajectorySearch.setBounds(775,430,190,25);trajectoryList.setBounds(970,430,195,25);trajectoryOnlyFav.setBounds(775,459,100,24);trajectoryStar.setBounds(880,459,80,24);trajectoryClose.setBounds(965,459,95,24);trajectoryBezier.setBounds(1065,459,100,24);trajectoryEnable.setBounds(775,487,145,24);trajectoryEase.setBounds(925,487,115,24);trajectoryMacro.setBounds(1045,487,120,24);for(int j=0;j<5;++j){trajectoryLabels[j].setBounds(775,516+j*32,100,26);trajectorySliders[j].setBounds(880,516+j*32,285,26);}trajectoryApply.setBounds(775,682,160,28);trajectoryUndo.setBounds(940,682,110,28);trajectoryRedo.setBounds(1055,682,110,28);
    motionButton.setBounds(760,230,175,30);motionGraph.setBounds(775,270,390,145);motionRec.setBounds(775,422,90,28);motionStop.setBounds(870,422,90,28);motionClear.setBounds(965,422,200,28);motionPlay.setBounds(775,454,190,26);motionSync.setBounds(970,454,195,26);motionLength.setBounds(775,486,190,28);motionMode.setBounds(970,486,195,28);for(int j=0;j<4;++j){motionLabels[j].setBounds(775,521+j*39,115,28);motionSliders[j].setBounds(895,521+j*39,270,28);}motionOverdub.setBounds(775,680,110,28);motionQuant.setBounds(890,680,90,28);motionStatus.setBounds(985,680,180,28);
    morphButton.setBounds(945,230,220,30);for(int j=0;j<8;++j)morphToggles[j].setBounds(775,270+j*35,390,30);morphAmountLabel.setBounds(775,560,85,30);morphAmount.setBounds(865,560,300,30);morphCurveLabel.setBounds(775,605,85,30);morphCurve.setBounds(865,605,300,30);morphInfo.setBounds(775,660,390,30);
    nexusPad.setBounds(310,270,450,445);for(int j=0;j<7;++j)nxToggles[j].setBounds(775+195*(j%2),270+30*(j/2),190,26);for(int j=0;j<3;++j){int y=402+45*j;nxSliderLabels[j].setBounds(775,y,85,28);nxSliders[j].setBounds(865,y,300,28);}for(int j=0;j<2;++j){int y=545+j*78;nxMacroLabels[j].setBounds(775,y,85,28);nxMacros[j].setBounds(865,y,300,28);nxCCLabels[j].setBounds(775,y+32,110,28);nxLearn[j].setBounds(890,y+32,135,28);nxClear[j].setBounds(1030,y+32,135,28);}
    detailLabel.setBounds(310,699,90,25);detailOsc.setBounds(403,699,130,25);detailCurve.setBounds(537,699,150,25);detailSmooth.setBounds(692,699,295,25);detailLive.setBounds(992,699,173,25);
    for(int j=0;j<8;++j){int x=310+216*(j%4),y=300+200*(j/4);macroLabels[j].setBounds(x,y,200,24);macroKnobs[j].setBounds(x+22,y+27,160,145);}
    for(int j=0;j<8;++j)matrixBanks[j].setBounds(310+107*j,270,102,30);
    constexpr int mxX[]={310,349,396,572,770,895,1016},mxW[]={35,43,172,194,121,117,149};
    for(int j=0;j<7;++j)matrixHeaders[j].setBounds(mxX[j],313,mxW[j],24);
    for(int row=0;row<8;++row){int y=344+44*row;matrixNumbers[row].setBounds(mxX[0],y,mxW[0],32);matrixEnabled[row].setBounds(mxX[1],y,mxW[1],32);for(int j=0;j<4;++j)matrixBoxes[row][j].setBounds(mxX[j+2],y,mxW[j+2],32);matrixAmounts[row].setBounds(mxX[6],y,mxW[6],32);}
    for(int j=0;j<12;++j)envTabs[j].setBounds(310+71*j,270,68,30);envView.setBounds(310,308,855,85);freeView.setBounds(310,308,855,152);
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
bool cosmosTestMidiLearn(juce::AudioProcessor& p){auto& x=static_cast<Processor&>(p);x.prepareToPlay(48000,128);x.learnRow.store(63);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(1,74,100),17);x.processBlock(b,midi);return x.consumeLearn()==63*128+74&&int(x.state.getRawParameterValue(mid(63,"source"))->load())==101;}
void cosmosSetReferenceDSP(juce::AudioProcessor& p,bool ref){static_cast<Processor&>(p).referenceDSPForQA=ref;}
bool cosmosTestImport(juce::AudioProcessor& p,const juce::File& f,bool table){auto& x=static_cast<Processor&>(p);return (table?x.loadTable(0,f):x.loadSample(0,f)).isEmpty();}
bool cosmosTestAssets(juce::AudioProcessor& p,bool expected){auto& x=static_cast<Processor&>(p);return (x.samples[0].load()!=nullptr)==expected&&(x.tables[0].load()!=nullptr)==expected;}
bool cosmosTestLayout(juce::AudioProcessor& p,const juce::File& dir){std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());auto& e=static_cast<Editor&>(*editor);bool ui=e.qualifyModulationUI(dir);juce::Logger::writeToLog(ui?"Modulation drop target and halo: passed":"Modulation drop target and halo: FAILED");return e.qualifyLayout(dir)&&ui;}
bool cosmosTestCache(juce::AudioProcessor& p){auto& x=static_cast<Processor&>(p);return x.retainedSamples.size()==1&&x.retainedTables.size()==1;}
#endif
