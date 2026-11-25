// Judges a chart by the count of seas between bases, by the reach of bombers over passed strongholds and by the
// flights history flew: the Pacific planner's tests on paper (data/campaign/far-reach.json), counted and measured
// on the chart as the rules do.
//
//   node tools/check-reach.mjs [--file=world-next.json] [--quiet]
//
// A fleet reaches the seas of its base and two beyond, and a landing crosses three seas at most, the first and the
// last counted (navy.json, range 3). Distance is from province centre to province centre along the map's straight
// line, 2.78 km to a map unit, the short way round the world: the planner's own measure, for its paper was drawn in
// the chart's projection. It is not the rules' on the far side any longer: there they read the globe's distance
// (src/campaign/wrap.ts, chartDist; the design director's rulings of 22 November 2026, item R), which east and west in
// the tropics is half as long again. So this judges the chart against the paper, as before, and not the game's reach:
// a flight in reach here may be out of it in the game (Rabaul to Guadalcanal is 810 km here and 1,048 on the globe).
import fs from 'node:fs';

const arg = (name) => process.argv.find((a) => a.startsWith(`--${name}=`))?.slice(name.length + 3);
const quiet = process.argv.includes('--quiet');
const map = JSON.parse(fs.readFileSync(new URL(`../src/generated/${arg('file') ?? 'world-next.json'}`, import.meta.url)));
const core = JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-map.json', import.meta.url)));
const T = JSON.parse(fs.readFileSync(new URL('../data/campaign/far-reach.json', import.meta.url)));
const KM = 2.78, W = map.world?.width ?? Infinity;

const zones = map.seas.zones;
const zoneOf = new Map(zones.map((z, i) => [z.id, i]));
const regions = [...core.regions, ...map.regions];
const byName = new Map(regions.map((r) => [r.name, r]));
const seasOf = new Map(); // province id -> seas, from the seas' own lists of coasts
zones.forEach((z, i) => { for (const c of z.coast) (seasOf.get(c) ?? seasOf.set(c, []).get(c)).push(i); });

const undrawn = new Set();
/** The seas a place stands on: its province's, or a stand-in until the province is drawn. */
function seasAt(name) {
  const r = byName.get(T.names[name] ?? name);
  if (r && seasOf.get(r.id)?.length) return seasOf.get(r.id);
  undrawn.add(name);
  const s = T.standin[name];
  if (!s) throw new Error(`${name} is not on the chart and has no stand-in`);
  return s.map((id) => { if (!zoneOf.has(id)) throw new Error(`no sea called ${id} (stand-in for ${name})`); return zoneOf.get(id); });
}
/** Seas on the shortest way, the first and the last counted. */
function count(from, to) {
  const goal = new Set(seasAt(to));
  const d = new Map(seasAt(from).map((z) => [z, 1]));
  const q = [...d.keys()];
  for (let i = 0; i < q.length; i++) {
    if (goal.has(q[i])) return d.get(q[i]);
    for (const n of zones[q[i]].adj) if (!d.has(n)) { d.set(n, d.get(q[i]) + 1); q.push(n); }
  }
  return 99;
}
function km(a, b) {
  const A = byName.get(T.names[a] ?? a), B = byName.get(T.names[b] ?? b);
  if (!A || !B) return null;
  let dx = Math.abs(A.x - B.x); if (dx > W / 2) dx = W - dx;
  return Math.hypot(dx, A.y - B.y) * KM;
}

let fails = 0;
const lines = [];
for (const [from, to, , why] of T.within) { const n = count(from, to); if (n > 3) fails++; if (n > 3 || !quiet) lines.push(`${n > 3 ? 'FAIL' : 'ok  '} ${from} to ${to}: ${n} seas (3 or fewer): ${why}`); }
for (const [from, to, , why] of T.beyond) { const n = count(from, to); if (n < 4) fails++; if (n < 4 || !quiet) lines.push(`${n < 4 ? 'FAIL' : 'ok  '} ${from} to ${to}: ${n} seas (4 or more): ${why}`); }
let waiting = 0;
for (const [fort, fields] of T.passed) {
  const ds = fields.map((f) => [f, km(f, fort)]);
  if (ds.some(([, d]) => d === null)) { waiting++; if (!quiet) lines.push(`wait ${fort}: not all of ${[fort, ...fields].join(', ')} are drawn yet`); continue; }
  const ok = ds.some(([, d]) => d <= T.reach);
  if (!ok) fails++;
  if (!ok || !quiet) lines.push(`${ok ? 'ok  ' : 'FAIL'} ${fort} lies ${ds.map(([f, d]) => `${Math.round(d)} km from ${f}`).join(', ')} (one within ${T.reach})`);
}
// Each island and each coast of the roads stands on the seas it must, and on no others.
let coastsJudged = 0;
for (const [name, want] of Object.entries(T.coasts ?? {})) {
  const r = byName.get(T.names[name] ?? name);
  if (!r) { if (!quiet) lines.push(`wait ${name}: not drawn yet`); continue; }
  coastsJudged++;
  const has = (seasOf.get(r.id) ?? []).map((z) => zones[z].id).sort(), must = [...want].sort();
  const ok = has.join() === must.join();
  if (!ok) fails++;
  if (!ok || !quiet) lines.push(`${ok ? 'ok  ' : 'FAIL'} ${r.name} stands on ${has.join(', ') || 'no sea'}${ok ? '' : ` (must be ${must.join(', ')})`}`);
}
// Each flight the planner measured falls on the chart as it fell on paper, at every reach tried (ruling 184).
let flights = 0;
const fall = (d, reach, kind) => (d > reach ? 'out of reach' : kind === 'fighters' && d > 0.6 * reach ? 'in part' : kind === 'fighters' ? 'in full' : 'within reach');
for (const [from, to, paper, kind, reaches] of T.flights ?? []) {
  const d = km(from, to);
  if (d === null) { if (!quiet) lines.push(`wait ${from} to ${to}: not drawn yet`); continue; }
  flights++;
  const off = reaches.filter((r) => fall(d, r, kind) !== fall(paper, r, kind));
  if (off.length) fails++;
  if (off.length || !quiet) lines.push(`${off.length ? 'FAIL' : 'ok  '} ${from} to ${to}: ${Math.round(d)} km on the chart, ${paper} on paper${off.map((r) => `; at ${r} ${fall(d, r, kind)}, on paper ${fall(paper, r, kind)}`).join('')}`);
}
console.log(lines.join('\n'));
console.log(`\n${T.within.length + T.beyond.length} counts of seas, ${T.passed.length - waiting} of ${T.passed.length} passed strongholds, ${coastsJudged} of ${Object.keys(T.coasts ?? {}).length} coasts and ${flights} of ${(T.flights ?? []).length} flights judged: ${fails ? `${fails} FAIL` : 'all pass'}.`);
if (undrawn.size) console.log(`Not drawn yet, judged on a stand-in sea: ${[...undrawn].join(', ')}.`);
process.exitCode = fails ? 1 : 0;
