import { W, H, x, h, dv, out } from './map.mjs';
for (const f of ['Game_GetWall','Game_GetHole','Game_GetAltar','Game_GetBush']) { let n=0, rcs=new Set(); for (let Y=0;Y<H;Y++) for (let X=0;X<W;X++){ dv().setUint32(out,0,true); const rc=x[f](h,X,Y,out); rcs.add(rc); if (dv().getUint32(out,true)) n++; } console.log(f,n,[...rcs]); }
