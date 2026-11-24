// Print a map as text. usage: node map.mjs maps/NAME.map26a   (#=wall, o=hole, "=bush, A=altar, digits = camp type, .=floor)
// camp types: W wolf, E elk, G golem, c gem cluster, g gold cluster, M monolith (letters from the camp list)
import fs from 'fs';
const wasi = new Proxy({}, { get: () => () => 0 });
const mod = await WebAssembly.instantiate(fs.readFileSync(new URL('./assets/v3_0_0.wasm', import.meta.url)), { wasi_snapshot_preview1: wasi });
const x = mod.instance.exports; x._initialize();
const bytes = fs.readFileSync(process.argv[2]);
const ptr = x.Engine_Malloc(bytes.length); new Uint8Array(x.memory.buffer).set(bytes, ptr);
const out = x.Engine_Malloc(64);
const isReplay = process.argv[2].endsWith('.repl26a');
const rc = isReplay ? x.Game_FromReplayFileBytes(ptr, bytes.length, out) : x.Game_FromMapFileBytes(ptr, bytes.length, out);
const dv = () => new DataView(x.memory.buffer);
if (rc) { console.error('load failed', rc); process.exit(1); }
const h = dv().getBigUint64(out, true);
x.Game_GetBitboardMetadata(h, out);
const W = dv().getInt32(out, true), H = dv().getInt32(out + 4, true);
const get = (fn, X, Y) => { fn(h, X, Y, out); return dv().getUint32(out, true) & 255; };
const grid = [];
for (let Y = 0; Y < H; Y++) { const row = []; for (let X = 0; X < W; X++) row.push(get(x.Game_GetWall, X, Y) ? '#' : get(x.Game_GetHole, X, Y) ? 'o' : get(x.Game_GetAltar, X, Y) ? 'A' : get(x.Game_GetBush, X, Y) ? '"' : '.'); grid.push(row); }
x.Game_GetNumCamps(h, out); const n = dv().getUint32(out, true);
x.Game_GetTeamASpawnArea(h, out); const sa = [0, 4, 8, 12].map(o => dv().getInt32(out + o, true));
console.log(JSON.stringify({ W, H, camps: n, spawnA: sa }));
export { grid, W, H, x, h, dv, out };
if (process.argv[1].endsWith('map.mjs')) {
  console.log('    ' + Array.from({ length: W }, (_, i) => i % 10).join(''));
  grid.forEach((r, i) => console.log(String(i).padStart(3) + ' ' + r.join('')));
}
