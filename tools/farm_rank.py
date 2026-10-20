"""Summary of each v83 variant vs v82 and v83 (normal, unlimited): records and us-minus-them farming margins.
usage: python tools/farm_rank.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import farm
B = os.path.join(farm.ROOT, "replays", "bench")
bots = "v83 v83-fuel v83-aggr v83-bank v83-bold0 v83-bold-10 v83-cheese1 v83-cheese2 v83-cheese3 v83-cheese4 v83-cheese6 v83-group v83-hold v83-lock v83-log v83-race v83-split v83-step v83-tp v83-ward".split()
print("%-12s %-9s %-9s %-9s %-9s %6s | per set (us-them): trib/100r  col/100r  camp/100r  lost/game  bank%%" % ("bot","v82 N","v82 U","v83 N","v83 U","total"))
for b in bots:
    rec=[]; w=g=0; d={k:[] for k in ("trib","col","camp","lost","bank")}
    for opp in ("v82","v83"):
        for k in ("normal","unlimited"):
            p=os.path.join(B,"%s-vs-%s-%s"%(b,opp,k))
            if b==opp or not os.path.isdir(p): rec.append("   -   "); continue
            r=farm.folder(p); n=r["rounds"]/100; u,t=r["us"],r["them"]
            rec.append("%2d-%-2d"%(r["wins"],r["games"]-r["wins"])); w+=r["wins"]; g+=r["games"]
            d["trib"].append((u["trib"]-t["trib"])/n); d["col"].append((u["col"]-t["col"])/n)
            d["camp"].append((u["camps"]-t["camps"])/n); d["lost"].append((u["lost"]-t["lost"])/r["games"])
            d["bank"].append(100*u["trib"]/u["col"]-100*t["trib"]/t["col"])
    f=lambda k,fm: " ".join(fm%x for x in d[k])
    print("%-12s %-9s %-9s %-9s %-9s %2d-%-3d| %s | %s | %s | %s | %s"%(b,*rec,w,g-w,f("trib","%+4.0f"),f("col","%+4.0f"),f("camp","%+5.2f"),f("lost","%+4.0f"),f("bank","%+3.0f")))
