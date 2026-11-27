"""Compare actual PCM16 preset artifacts on the same OS, not subjective audition."""
import array, csv, math, pathlib, sys, wave
def pcm(path):
    with wave.open(str(path),'rb') as w:
        fmt=(w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())
        if fmt[1]!=2: raise ValueError(f"Unexpected format: {path}")
        a=array.array('h',w.readframes(w.getnframes()))
        if sys.byteorder!='little': a.byteswap()
        return fmt,a
old=pathlib.Path("cosmos-baseline-v055/cosmos-preset-audio")
new=pathlib.Path("cosmos-preset-audio")
fail=0
with open("cosmos-cross-version.csv","w",newline="") as f:
    w=csv.writer(f);w.writerow(["preset","max_pcm16_difference","rms_difference","passed"])
    for i in range(1,271):
        name=f"{i:03}.wav"
        a,x=pcm(old/name);b,y=pcm(new/name)
        if a!=b or len(x)!=len(y): raise ValueError(name)
        peak=0;energy=0
        for u,v in zip(x,y):
            d=u-v;peak=max(peak,abs(d));energy+=d*d
        ok=peak<=4
        w.writerow([i,peak,math.sqrt(energy/max(1,len(x)))/32768,ok]);fail+=not ok
print(f"Cross-version PCM16 regression: {270-fail}/270 passed (max 4 PCM counts)")
sys.exit(bool(fail))
