"""Gate the measured finite/peak/coverage conditions, not subjective listening or deadlines."""
import csv
import math
from pathlib import Path

def read(name):
    with open(name, encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))

def require(condition, message):
    if not condition:
        raise SystemExit(message)

fx = read("cosmos-fx-qualification.csv")
require(len(fx) == 300 and {(int(x["effect"]), int(x["profile"])) for x in fx} == {(i,p) for i in range(1,101) for p in range(1,4)}, "Incomplete FX profiles")
for r in fx:
    require(int(r["finite"]) == 1 and all(math.isfinite(float(r[k])) for k in ("peak","rms","delta_rms","mean_budget_pct","profile_trim")), "Non-finite profile")
    require(0 <= float(r["peak"]) <= .9001 and 0 < float(r["profile_trim"]) <= 1, "Profile outside reference peak/trim gate: " + str(r))
alias = read("cosmos-fx-alias.csv")
require(len(alias)==50 and {(int(x["effect"]),int(x["factor"])) for x in alias} == {(i,f) for i in range(19,29) for f in (1,2,4,8,16)}, "Incomplete alias measurements")
for r in alias:
    require(int(r["finite"]) == 1 and all(math.isfinite(float(r[k])) for k in ("carrier_rms","alias_21000_rms","alias_relative_db","peak")), "Non-finite alias measurement")
bench=read("cosmos-fx-40.csv")
require(len(bench)==4 and {r["scenario"] for r in bench}=={"mixed_serial","fft2048_serial","mixed_parallel","mixed_serial_10min"}, "Missing FX endurance scenarios")
for r in bench:
    require(int(r["finite"])==1 and int(r["slots"])==40 and float(r["rendered_seconds"])>0 and int(r["fx_allocated_bytes"])>0, "Invalid FX benchmark")
long=next(r for r in bench if r["scenario"]=="mixed_serial_10min")
require(float(long["rendered_seconds"])>=600, "Endurance too short")
summary=["300/300 profiles: finite, peak <= 0.9001 on reference signal","50/50 alias probes finite","40 FX, 600 seconds rendered: finite",f"10-minute deadline misses: {long['misses']} (reported, not a pass criterion)",f"FX allocation: {long['fx_allocated_bytes']} bytes","Subjective listening, native DAW PDC and physical display remain manual gates."]
for f in (1,2,4,8,16):
    db=[float(r["alias_relative_db"]) for r in alias if int(r["factor"])==f]
    summary.append(f"{f}x alias probe mean relative level: {sum(db)/len(db):.2f} dB")
Path("cosmos-fx-release-summary.txt").write_text("\n".join(summary)+"\n",encoding="utf-8")
print("\n".join(summary))
