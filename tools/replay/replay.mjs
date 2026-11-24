// Decode a .repl26a (or the zip from `cambc matches download`) into one JSON line per round.
// usage: node replay.mjs FILE.repl26a [every=1] > out.jsonl
// Each line: {r, ch:[[idx,team,type,x,y,hp,maxhp,alive,gold,gems,kills,stunned]], camps:[[type,x,y,hp]], groups:[[reward,capTeam]]}
// Champion type: 0 Ryth 1 Vesper 2 Vendetta 3 Cassi+Issac(?) 4 Stellar 5 Prism 6 Marcella 7 Willow 8 Ren 9 Varg 10 Tavi 11 HoR
import fs from 'fs';
import { I as decode } from './assets/PanelChrome-DRtW3oWL.js';
const file = process.argv[2], every = +(process.argv[3] || 1);
const wasi = new Proxy({}, { get: (_, n) => (...a) => (n === 'proc_exit' ? (() => { throw new Error('exit'); })() : 0) });
const mod = await WebAssembly.instantiate(fs.readFileSync(new URL('./assets/v3_0_0.wasm', import.meta.url)), { wasi_snapshot_preview1: wasi });
const x = mod.instance.exports;
x._initialize();
const bytes = fs.readFileSync(file);
const ptr = x.Engine_Malloc(bytes.length);
new Uint8Array(x.memory.buffer).set(bytes, ptr);
const out = x.Engine_Malloc(16);
const rc = x.Game_FromReplayFileBytes(ptr, bytes.length, out);
const dv = () => new DataView(x.memory.buffer);
if (rc) { console.error('load failed', rc); process.exit(1); }
const h = dv().getBigUint64(out, true);
const snapSize = x.Game_Visualiser_TimelineSnapshotSize();
const snap = x.Engine_Malloc(snapSize), args = x.Engine_Malloc(256);
let last = -1;
const lines = [];
for (;;) {
  const end = x.Game_Visualiser_Timeline_ReplayNext(h, 1);
  x.Game_Runner_GetState(h, args);
  const round = dv().getUint32(args + 8, true);
  if (round !== last && (round % every === 0 || end)) {
    x.Game_Visualiser_ReadTimelineState(h, snap);
    const s = decode(dv(), snap);
    lines.push(JSON.stringify({ r: round,
      ch: s.champions.map(c => [c.entity.index, c.team, c.type, c.position ? c.position.x : -1, c.position ? c.position.y : -1, Math.round(c.health), Math.round(c.maxHealth), +c.alive, c.gold, c.gems, c.kills, c.stunSource ? 1 : 0]),
      camps: s.camps.map(c => [c.type, c.position.x, c.position.y, Math.round(c.health)]),
      groups: s.groups.map(g => [+g.rewardAvailable, g.captureTeam]) }));
  }
  last = round;
  if (end) break;
}
fs.writeSync(1, lines.join('\n') + '\n');
