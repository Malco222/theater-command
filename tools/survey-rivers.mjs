// Reads the rivers of the Natural Earth survey (public domain, naturalearthdata.com: the file
// ne_10m_rivers_lake_centerlines.geojson, given by path) and writes data/campaign/more-rivers.mjs:
//  - REAL_COURSES: the real course of each of the 60 rivers handed over with the chart, by name;
//  - MORE_RIVERS: the rivers and streams outside the map of Europe that the chart did not have.
// What is decided by hand (names, which to leave out, how great each is) stands in data/campaign/detail.mjs.
//
//   node tools/survey-rivers.mjs <survey.geojson> [--list] [--fresh]
//
// --list prints every river taken, by part of the world. Without --fresh the rivers already in more-rivers.mjs keep
// their order (a river keeps its number for good once the chart is handed over) and new ones follow them.
import fs from 'node:fs';
import { SURVEY_FIX, SURVEY_SPLIT, LAKE_NAMES, RIVER_ALIASES, RIVER_TRIM, RIVER_LEAVE, RIVER_FOOT, RIVER_HAND, RIVER_NAMES, RIVER_SKIP, RIVER_PICK, RIVER_GREAT, RIVER_RIVERS, RIVER_STREAMS, RIVER_INTO, RIVER_SEASONAL, RIVER_FLIP, RIVER_BAR } from '../data/campaign/detail.mjs';

const src = process.argv[2];
if (!src || src.startsWith('--')) { console.error('give the path of ne_10m_rivers_lake_centerlines.geojson'); process.exit(1); }
const list = process.argv.includes('--list'), fresh = process.argv.includes('--fresh');
const read = (name) => JSON.parse(fs.readFileSync(new URL(`../src/generated/${name}.json`, import.meta.url), 'utf8'));
const ne = JSON.parse(fs.readFileSync(src, 'utf8'));
const have = read('world-next-rivers').rivers;
const europe = read('campaign-map');
const chart = read('world-cities');

// A flat sheet to measure on: a unit is a unit of the chart (40 to a degree of latitude).
const K = Math.cos((50 * Math.PI) / 180) * 40;
const uv = ([lon, lat]) => [lon * K, -lat * 40];
const turn = (lon) => (lon > 180 ? lon - 360 : lon < -180 ? lon + 360 : lon);
// The map of Europe: no river is drawn on it from here.
const SHEET = { w: -11, e: -11 + europe.width / K, s: 71.4 - europe.height / 40 };
const inSheet = ([lon, lat]) => lon > SHEET.w && lon < SHEET.e && lat > SHEET.s;
const fold = (s) => (s ?? '').normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[’`]/g, "'").replace(/\s+/g, ' ').trim();
const len = (p) => { let t = 0; for (let i = 1; i < p.length; i++) t += Math.hypot(p[i][0] - p[i - 1][0], p[i][1] - p[i - 1][1]); return t; };
/** The nearest point of a line to p: how far, and how far along the line it lies. */
function nearest(p, line) {
  let best = { d: Infinity, s: 0 }, run = 0;
  for (let i = 0; i + 1 < line.length; i++) {
    const a = line[i], b = line[i + 1], dx = b[0] - a[0], dy = b[1] - a[1], L2 = dx * dx + dy * dy, L = Math.sqrt(L2);
    const t = L2 ? Math.max(0, Math.min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / L2)) : 0;
    const d = Math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy);
    if (d < best.d) best = { d, s: run + t * L };
    run += L;
  }
  return best;
}

// ---- The survey's lines --------------------------------------------------------------------------------------

const lines = [];
for (const ft of ne.features) {
  const g = ft.geometry, p = ft.properties;
  if (!g) continue;
  const raw = fold(p.name_en || p.name) || `#${p.rivernum}`;
  // A line that is two rivers is cut where the lists say.
  const parts = [];
  for (const l of g.type === 'LineString' ? [g.coordinates] : g.coordinates) {
    const cut = SURVEY_SPLIT.find(([n, lat, lon]) => fold(n) === raw && l.some((c) => Math.abs(c[1] - lat) < 0.4 && Math.abs(turn(c[0] - lon)) < 0.4));
    if (!cut) { parts.push(l); continue; }
    let at = 0, best = Infinity;
    l.forEach((c, i) => { const d = Math.hypot(c[1] - cut[1], turn(c[0] - cut[2])); if (d < best) { best = d; at = i; } });
    parts.push(l.slice(0, at + 1), l.slice(at));
  }
  for (const l of parts) {
    if (l.length < 2) continue;
    const pts = l.map(uv);
    const mid = l[Math.floor(l.length / 2)];
    let name = raw;
    // The survey misnames a reach here and there: the lists say what it is.
    const fix = SURVEY_FIX.find(([n, lat, lon, , span = 4]) => fold(n) === name && Math.abs(lat - mid[1]) < span && Math.abs(turn(lon - mid[0])) < span * 1.5);
    if (fix) name = fix[3];
    lines.push({ id: lines.length, name, rank: p.scalerank, ll: l, p: pts, len: len(pts) });
  }
}

/**
 * Joins lines into rivers: lines that meet end to end are one river, and so are lines whose ends lie within 'gap'
 * of each other (a lake the survey left blank). Each river is given as its longest run, end to end.
 */
function chain(ls, gap) {
  const ends = ls.flatMap((l, i) => [{ i, at: 0, p: l.p[0] }, { i, at: 1, p: l.p[l.p.length - 1] }]);
  const node = ends.map((_, k) => k);
  const find = (k) => { while (node[k] !== k) k = node[k] = node[node[k]]; return k; };
  for (let a = 0; a < ends.length; a++) for (let b = a + 1; b < ends.length; b++) if (Math.hypot(ends[a].p[0] - ends[b].p[0], ends[a].p[1] - ends[b].p[1]) < 1) node[find(a)] = find(b);
  const nodeOf = (i, at) => find(i * 2 + at);
  const edges = ls.map((l, i) => ({ a: nodeOf(i, 0), b: nodeOf(i, 1), w: l.len, line: i }));
  // The gaps: ends of lines that do not meet, near each other.
  const place = new Map();
  ends.forEach((e, k) => place.set(find(k), e.p));
  const nodes = [...place.keys()];
  for (let a = 0; a < nodes.length; a++) for (let b = a + 1; b < nodes.length; b++) {
    const p = place.get(nodes[a]), q = place.get(nodes[b]), d = Math.hypot(p[0] - q[0], p[1] - q[1]);
    if (d < gap) edges.push({ a: nodes[a], b: nodes[b], w: d * 3, line: -1 });
  }
  const adj = new Map(nodes.map((n) => [n, []]));
  for (const e of edges) { adj.get(e.a).push(e); adj.get(e.b).push(e); }
  const far = (from) => {
    const d = new Map([[from, 0]]), prev = new Map(), done = new Set();
    for (;;) {
      let u = null;
      for (const [n, v] of d) if (!done.has(n) && (u === null || v < d.get(u))) u = n;
      if (u === null) break;
      done.add(u);
      for (const e of adj.get(u)) { const v = e.a === u ? e.b : e.a, w = d.get(u) + e.w; if (w < (d.get(v) ?? Infinity)) { d.set(v, w); prev.set(v, { u, e }); } }
    }
    let end = from;
    for (const [n, v] of d) if (v > d.get(end)) end = n;
    return { d, prev, end };
  };
  const seen = new Set(), out = [];
  for (const start of nodes) {
    if (seen.has(start)) continue;
    const a = far(start);
    for (const n of a.d.keys()) seen.add(n);
    const b = far(a.end);
    // From b.end back to a.end: the lines on the way, each turned to run with the walk.
    const ll = [], used = [];
    let fwd = 0, back = 0;
    for (let v = b.end; b.prev.has(v); v = b.prev.get(v).u) {
      const { u, e } = b.prev.get(v);
      if (e.line < 0) continue;
      const l = ls[e.line], runs = nodeOf(e.line, 0) === v && !(nodeOf(e.line, 1) === v && used.includes(l));
      ll.push(...(runs ? l.ll : [...l.ll].reverse()));
      if (runs) fwd += l.len; else back += l.len;
      used.push(l);
      void u;
    }
    if (ll.length < 2) continue;
    // The survey draws a river downstream: the walk is turned to run the way most of its lines do.
    if (back > fwd) ll.reverse();
    out.push({ ll, used, rank: Math.min(...used.map((l) => l.rank)) });
  }
  return out;
}
/**
 * A walk that comes back on itself is cut where it turns back. Where a river parts into arms (a delta, a side
 * channel) the longest walk can run down one arm and back up another to where they parted; the river is the walk
 * as far as the point of that loop that lies furthest from the parting.
 */
const unlooped = [];
function unloop(ll, name) {
  for (let pass = 0; pass < 4; pass++) {
    let cut = false;
    for (const end of ['foot', 'head']) {
      const p = ll.map(uv), n = p.length - 1;
      const along = [0];
      for (let i = 1; i <= n; i++) along.push(along[i - 1] + Math.hypot(p[i][0] - p[i - 1][0], p[i][1] - p[i - 1][1]));
      const e = end === 'foot' ? n : 0;
      let at = -1, bd = 2;
      for (let j = 0; j < n; j++) {
        if (Math.abs(along[j] - along[e]) < 25 || Math.abs(along[j + 1] - along[e]) < 25) continue;
        const d = nearest(p[e], [p[j], p[j + 1]]).d;
        if (d < bd) { bd = d; at = j; }
      }
      if (at < 0) continue;
      // The loop runs from the parting (at) to the end: the point of it furthest from the end.
      let far = e, fd = 0;
      for (let i = Math.min(at, e); i <= Math.max(at, e); i++) { const d = Math.hypot(p[i][0] - p[e][0], p[i][1] - p[e][1]); if (d > fd) { fd = d; far = i; } }
      unlooped.push(`${name} (${end}: ${Math.round(Math.abs(along[e] - along[far]))} units of an arm walked back, cut at ${ll[far][1].toFixed(2)}, ${ll[far][0].toFixed(2)})`);
      ll = end === 'foot' ? ll.slice(0, far + 1) : ll.slice(far);
      cut = true;
    }
    if (!cut) break;
  }
  return ll;
}
/** A course without the points the survey repeats, cut to what lies outside the map of Europe (its longest run). */
function outside(ll) {
  const runs = [[]];
  for (const p of ll) { if (inSheet(p)) { if (runs[runs.length - 1].length) runs.push([]); } else runs[runs.length - 1].push(p); }
  return runs.sort((a, b) => len(b.map(uv)) - len(a.map(uv)))[0];
}

// ---- The 60 rivers of the chart on their real courses -------------------------------------------------------------

const NEAR = 30;
const courses = have.map((rv) => rv.ll.map(([lat, lon]) => uv([turn(lon), lat])));
const aliases = have.map((rv) => new Set((RIVER_ALIASES[rv.name] ?? [rv.name]).map(fold)));
for (const l of lines) {
  let best = null;
  for (let k = 0; k < have.length; k++) {
    if (!aliases[k].has(l.name)) continue;
    let near = 0;
    for (let i = 0; i + 1 < l.p.length; i++) if (nearest(l.p[i], courses[k]).d <= NEAR) near += Math.hypot(l.p[i + 1][0] - l.p[i][0], l.p[i + 1][1] - l.p[i][1]);
    const share = near / l.len;
    if (share >= 0.35 && (!best || share > best.share)) best = { k, share };
  }
  if (best) l.river = best.k;
}
// A line of the same name that runs on from one of a river's lines is the same river (its upper course, beyond
// where the course handed over begins), unless the river of the chart is one reach of a longer one.
for (let again = true; again; ) {
  again = false;
  for (const l of lines) {
    if (l.river !== undefined) continue;
    for (const o of lines) {
      if (o.river === undefined || !aliases[o.river].has(l.name) || RIVER_TRIM.includes(have[o.river].name)) continue;
      const ends = [l.p[0], l.p[l.p.length - 1]], theirs = [o.p[0], o.p[o.p.length - 1]];
      if (ends.some((a) => theirs.some((b) => Math.hypot(a[0] - b[0], a[1] - b[1]) < 1.5))) { l.river = o.river; again = true; break; }
    }
  }
}
/**
 * A foot laid by the lists (RIVER_FOOT): places by hand, and reaches of the survey's lines by name, each followed
 * from where the course stands to the place named. The lines followed are that river's and no other's.
 */
function footOf(items, from) {
  const out = [], used = [];
  let last = from;
  for (const it of items) {
    if (typeof it[0] === 'number') { last = [it[1], it[0]]; out.push(last); continue; }
    const [name, lat, lon] = it;
    let best = null;
    for (const c of chain(lines.filter((l) => l.name === fold(name)), 5)) {
      const idx = (pt) => { let at = 0, d = Infinity; c.ll.forEach((q, i) => { const e = Math.hypot((q[1] - pt[1]) * 40, turn(q[0] - pt[0]) * K); if (e < d) { d = e; at = i; } }); return { at, d }; };
      const a = idx(last), b = idx([lon, lat]);
      if (!best || a.d + b.d < best.a.d + best.b.d) best = { c, a, b };
    }
    if (!best || best.a.d + best.b.d > 40) throw new Error(`RIVER_FOOT: the survey has no line called ${name} from ${last[1].toFixed(2)}, ${last[0].toFixed(2)} to ${lat}, ${lon}`);
    const run = best.a.at <= best.b.at ? best.c.ll.slice(best.a.at, best.b.at + 1) : best.c.ll.slice(best.b.at, best.a.at + 1).reverse();
    out.push(...run);
    used.push(...best.c.used);
    last = run[run.length - 1];
  }
  return { ll: out, used };
}
const real = {}, realNotes = [];
have.forEach((rv, k) => {
  const mine = lines.filter((l) => l.river === k);
  const hand = courses[k], L = len(hand);
  const pick = chain(mine, 40).sort((a, b) => len(b.ll.map(uv)) - len(a.ll.map(uv)))[0];
  if (!pick) { realNotes.push(`${rv.name}: not in the survey, drawn as handed over`); return; }
  let ll = unloop(pick.ll, rv.name);
  // The same way round as the course handed over.
  if (nearest(uv(ll[0]), hand).s > nearest(uv(ll[ll.length - 1]), hand).s) ll = [...ll].reverse();
  if (RIVER_TRIM.includes(rv.name)) {
    let a = 0, b = ll.length - 1, da = Infinity, db = Infinity;
    ll.forEach((p, i) => { const q = uv(p), d0 = Math.hypot(q[0] - hand[0][0], q[1] - hand[0][1]), d1 = Math.hypot(q[0] - hand[hand.length - 1][0], q[1] - hand[hand.length - 1][1]); if (d0 < da) { da = d0; a = i; } if (d1 < db) { db = d1; b = i; } });
    if (b > a) ll = ll.slice(a, b + 1);
  }
  if (RIVER_LEAVE[rv.name]) {
    const [lat, lon] = RIVER_LEAVE[rv.name];
    let at = 0, best = Infinity;
    ll.forEach((p, i) => { const d = Math.hypot(p[1] - lat, turn(p[0] - lon)); if (d < best) { best = d; at = i; } });
    ll = ll.slice(0, at + 1);
  }
  ll = outside(ll);
  if (!ll || ll.length < 2) { realNotes.push(`${rv.name}: its real course lies on the map of Europe, drawn as handed over`); return; }
  // Where the survey stops short of the course handed over, the course handed over runs on. (Where the survey
  // leaves it for good, as the Yellow River's bed of today leaves the flood of 1938, the survey's own way is dropped.)
  {
    const end = nearest(uv(ll[ll.length - 1]), hand), start = nearest(uv(ll[0]), hand);
    if (L - end.s > 25 && end.d > 20) { let i = ll.length - 1; while (i > 1 && nearest(uv(ll[i]), hand).d > 20) i--; ll = ll.slice(0, i + 1); }
    if (start.s > 25 && start.d > 20) { let i = 0; while (i < ll.length - 2 && nearest(uv(ll[i]), hand).d > 20) i++; ll = ll.slice(i); }
  }
  const s0 = nearest(uv(ll[0]), hand).s, s1 = nearest(uv(ll[ll.length - 1]), hand).s;
  const handLL = rv.ll.map(([lat, lon]) => [turn(lon), lat]);
  let run = 0;
  const along = hand.map((p, i) => (i ? (run += Math.hypot(p[0] - hand[i - 1][0], p[1] - hand[i - 1][1])) : 0));
  const head = s0 > 25 ? handLL.filter((_, i) => along[i] < s0 - 8) : [];
  let tail = L - s1 > 25 ? handLL.filter((_, i) => along[i] > s1 + 8) : [];
  // (Where the course handed over is not the river's course of 1939 either, the lists lay the foot.)
  const foot = RIVER_FOOT[rv.name] ? footOf(RIVER_FOOT[rv.name], ll[ll.length - 1]) : null;
  if (foot) { tail = foot.ll; realNotes.push(`${rv.name}: its foot is laid by the lists (${foot.ll.length} points)`); }
  if (!foot && (head.length || tail.length)) realNotes.push(`${rv.name}: the survey stops short (${head.length ? `${Math.round(s0)} units at the head` : ''}${head.length && tail.length ? ', ' : ''}${tail.length ? `${Math.round(L - s1)} at the foot` : ''}): the course handed over runs on there`);
  real[rv.name] = { ll: [...head, ...ll, ...tail], was: Math.round(L), now: Math.round(len([...head, ...ll, ...tail].map(uv))), used: new Set([...pick.used, ...(foot?.used ?? [])]) };
});

// ---- The rivers the chart did not have -----------------------------------------------------------------------------

const taken = new Set(Object.values(real).flatMap((r) => [...r.used]));
// A line of one of the chart's rivers that was not on its course (an arm of a delta, a side channel) is not a
// river of its own.
const spare = lines.filter((l) => !taken.has(l) && l.river === undefined && !l.name.startsWith('#'));
const byName = new Map();
for (const l of spare) (byName.get(l.name) ?? byName.set(l.name, []).get(l.name)).push(l);
const at = (list, name, lat, lon) => list.some((row) => fold(row[0]) === name && (row.length < 3 || typeof row[1] !== 'number' || (Math.abs(row[1] - lat) < 7 && Math.abs(turn(row[2] - lon)) < 9)));
const found = [];
for (const [name, ls] of byName) {
  for (const c of chain(ls, 25)) {
    const ll = outside(unloop(c.ll, name));
    if (!ll || ll.length < 2) continue;
    const p = ll.map(uv), L = len(p), mid = ll[Math.floor(ll.length / 2)];
    const picked = at(RIVER_PICK, name, mid[1], mid[0]);
    if (at(RIVER_SKIP, name, mid[1], mid[0])) continue;
    found.push({ ne: name, rank: c.rank, ll, len: L, mid, picked });
  }
}
// The rivers the survey does not have, drawn by hand (RIVER_HAND): they are what the lists say they are.
for (const row of RIVER_HAND) {
  const ll = row[3].map(([lat, lon]) => [lon, lat]);
  found.push({ ne: row[0], rank: 0, ll, len: len(ll.map(uv)), mid: ll[Math.floor(ll.length / 2)], picked: true, hand: row });
}
// Names: the chart's own (a 1939 atlas's) where the lists give one.
for (const r of found) {
  const row = RIVER_NAMES.find((n) => fold(n[0]) === r.ne && (n.length === 2 || (Math.abs(n[1] - r.mid[1]) < 7 && Math.abs(turn(n[2] - r.mid[0])) < 9)));
  r.name = row ? row[row.length - 1] : r.ne;
}
// The part of the world a river runs in: that of the province whose chief place is nearest its middle.
const cx = (lon) => ((lon < -141 ? lon + 360 : lon) + 11) * K, cy = (lat) => (71.4 - lat) * 40, W = 360 * K;
const byId = new Map(chart.regions.map((r) => [r.id, r]));
function provinceAt(lon, lat) {
  const x = cx(lon), y = cy(lat);
  let best = null, bd = Infinity;
  for (const pr of chart.regions) { const d = Math.min(Math.hypot(pr.x - x, pr.y - y), Math.hypot(pr.x - x - W, pr.y - y), Math.hypot(pr.x - x + W, pr.y - y)); if (d < bd) { bd = d; best = pr; } }
  return best;
}
const SOUTH = new Set(['COL', 'VEN', 'ECU', 'PER', 'BOL', 'PAR', 'URU', 'CHL', 'ARG', 'BRA']);
const where = (la, lo) => {
  const pr = provinceAt(lo, la), r = pr.of !== undefined ? byId.get(pr.of) : pr, o = r.owner;
  if (r.id < 576) return o === 'USA' && la > 24 ? 'United States' : o === 'CAN' || o === 'UK' && la > 45 ? 'Canada' : SOUTH.has(o) || la < 9 ? 'South America' : 'Mexico, Central America, Caribbean';
  if (r.id < 888) {
    if (o === 'USA' && la > 50) return 'Alaska';
    if (o === 'JAP' && la > 30 && lo > 128 && lo < 146 && la < 46 && !(lo < 131 && la > 33 && la < 43.5 && lo < 129.6)) return 'Japan';
    if (o === 'JAP' && lo > 124 && lo < 131 && la > 33 && la < 43.5) return 'Korea';
    if (['CHN', 'MAN', 'MGL'].includes(o) || (o === 'JAP' && lo > 100 && lo < 124.5 && la > 18)) return 'China, Manchuria, Mongolia';
    if (o === 'SOV') return 'Soviet Asia';
    if ((o === 'AUS' && la < -9 && lo > 110 && lo < 155) || o === 'NZL') return 'Australia and New Zealand';
    if (lo > 91 && lo < 142 && la < 29 && la > -12) return 'South-East Asia';
    return 'Pacific islands';
  }
  if (o === 'SOV') return 'Soviet Asia';
  if (la < 38 && lo > 60 && lo < 98 && la > 5 && !['AFG', 'IRN'].includes(o)) return 'India';
  if (lo > 34 && la > 12 && (['IRN', 'IRQ', 'SAU', 'YEM', 'AFG', 'TUR'].includes(o) || (lo > 42 && lo < 62))) return 'Persia, Arabia, Afghanistan';
  return 'Africa';
};
const PARTS = ['United States', 'Canada', 'Alaska', 'Mexico, Central America, Caribbean', 'South America', 'China, Manchuria, Mongolia', 'Korea', 'Japan', 'Soviet Asia', 'South-East Asia', 'India', 'Persia, Arabia, Afghanistan', 'Australia and New Zealand', 'Pacific islands', 'Africa'];
for (const r of found) r.part = where(r.mid[1], r.mid[0]);
// The bar: how long a river of each rank of the survey must be to be drawn. In empty country (the tundra and the
// taiga) only the survey's first ranks are drawn.
const wild = (r) => r.part === 'Alaska' || (r.part === 'Canada' && (r.mid[1] > 55 || (r.mid[1] > 50 && r.mid[0] > -96))) || (r.part === 'Soviet Asia' && (r.mid[1] > 60 || (r.mid[0] > 105 && r.mid[1] > 56)));
const bar = (r) => (wild(r) ? (r.rank <= 6 ? 150 : Infinity) : r.rank <= 6 ? 80 : r.rank === 7 ? 120 : r.rank === 8 ? 180 : Infinity);
// --under=north,south,west,east lists what the survey has in a box that is under the bar (to pick from by hand).
const under = process.argv.find((v) => v.startsWith('--under='))?.slice(8).split(',').map(Number);
if (under) for (const r of found.filter((r) => r.len < bar(r) && !r.picked && r.mid[1] <= under[0] && r.mid[1] >= under[1] && r.mid[0] >= under[2] && r.mid[0] <= under[3] && r.len >= 25).sort((a, b) => b.len - a.len)) console.log(`  under the bar: r${r.rank} ${r.ne.padEnd(22)} ${String(Math.round(r.len)).padStart(4)}u  ${r.ll[0][1].toFixed(1)},${r.ll[0][0].toFixed(1)} -> ${r.ll[r.ll.length - 1][1].toFixed(1)},${r.ll[r.ll.length - 1][0].toFixed(1)}`);
for (let i = found.length - 1; i >= 0; i--) if (found[i].len < bar(found[i]) && !found[i].picked) found.splice(i, 1);
found.sort((a, b) => PARTS.indexOf(a.part) - PARTS.indexOf(b.part) || b.len - a.len);
// No two rivers of the chart share a name.
const names = new Map([...read('campaign-rivers').rivers.map((rv) => [rv.name, 'the map of Europe']), ...have.map((rv) => [rv.name, 'the chart'])]);
const clash = [];
for (const r of found) { if (names.has(r.name)) clash.push(`${r.name} (${r.part}, ${r.mid[1].toFixed(0)},${r.mid[0].toFixed(0)}; survey name ${r.ne}) and ${names.get(r.name)}`); else names.set(r.name, `${r.part} ${r.mid[1].toFixed(0)},${r.mid[0].toFixed(0)}`); }

// ---- How great, and into what ---------------------------------------------------------------------------------------

const great = new Set(RIVER_GREAT), rivers = new Set(RIVER_RIVERS), streams = new Set(RIVER_STREAMS), seasonal = new Set(RIVER_SEASONAL);
// A river in the rules is one the size of Europe's own (the Meuse, the Prut, the Berezina); the rest are streams.
const classOf = (name, rank, L, empty) => (great.has(name) ? 'great' : streams.has(name) ? 'small' : rivers.has(name) ? 'middle' : !empty && ((rank <= 5 && L >= RIVER_BAR[0]) || (rank === 6 && L >= RIVER_BAR[1]) || (rank === 7 && L >= RIVER_BAR[2]) || (rank === 8 && L >= RIVER_BAR[3])) ? 'middle' : 'small');
for (const r of found) r.class = r.hand ? r.hand[1] : classOf(r.name, r.rank, r.len, wild(r));
// Thinned: no point of a course is dropped that stands more than a unit from the line drawn without it.
function thin(ll, tol) {
  const p = ll.map(uv), keep = new Uint8Array(p.length);
  keep[0] = keep[p.length - 1] = 1;
  const stack = [[0, p.length - 1]];
  while (stack.length) {
    const [a, b] = stack.pop();
    let far = -1, fd = tol;
    const dx = p[b][0] - p[a][0], dy = p[b][1] - p[a][1], L2 = dx * dx + dy * dy;
    for (let i = a + 1; i < b; i++) {
      const t = L2 ? Math.max(0, Math.min(1, ((p[i][0] - p[a][0]) * dx + (p[i][1] - p[a][1]) * dy) / L2)) : 0;
      const d = Math.hypot(p[i][0] - p[a][0] - t * dx, p[i][1] - p[a][1] - t * dy);
      if (d > fd) { fd = d; far = i; }
    }
    if (far > 0) { keep[far] = 1; stack.push([a, far], [far, b]); }
  }
  return ll.filter((_, i) => keep[i]);
}
/**
 * A sharp turn is cut across, so that a river bends and does not break: the corner is cut the same distance back
 * on both sides, which halves the turn, until no turn is over 'most' degrees. Points that crowd one another are
 * dropped first.
 */
function ease(ll, most = 27, rounds = 12) {
  const turnAt = (i) => {
    const a = uv(ll[i - 1]), b = uv(ll[i]), c = uv(ll[i + 1]);
    return { t: Math.abs(Math.abs(((Math.atan2(c[1] - b[1], c[0] - b[0]) - Math.atan2(b[1] - a[1], b[0] - a[0])) * 180) / Math.PI + 540) % 360 - 180), la: Math.hypot(b[0] - a[0], b[1] - a[1]), lc: Math.hypot(c[0] - b[0], c[1] - b[1]) };
  };
  const near = [ll[0]];
  for (let i = 1; i + 1 < ll.length; i++) { const a = uv(near[near.length - 1]), b = uv(ll[i]), z = uv(ll[ll.length - 1]); if (Math.hypot(b[0] - a[0], b[1] - a[1]) >= 0.5 && Math.hypot(b[0] - z[0], b[1] - z[1]) >= 0.5) near.push(ll[i]); }
  near.push(ll[ll.length - 1]);
  ll = near;
  for (let pass = 0; pass < 3; pass++) {
    // A hairpin too tight to round (a sharp turn with a short reach beside it), or a course that doubles back on
    // itself where two of the survey's lines meet, loses its tip.
    for (let again = true; again; ) {
      again = false;
      for (let i = 1; i + 1 < ll.length; i++) { const { t, la, lc } = turnAt(i); if (t > 150 || (t > 60 && Math.min(la, lc) < 1)) { ll.splice(i, 1); again = true; i--; } }
    }
    for (let n = 0; n < rounds; n++) {
      const out = [ll[0]];
      let cut = 0;
      for (let i = 1; i + 1 < ll.length; i++) {
        const { t, la, lc } = turnAt(i);
        if (t > most && la > 0 && lc > 0) {
          const d = Math.min(la, lc) * 0.4, fa = d / la, fc = d / lc;
          out.push([ll[i][0] + (ll[i - 1][0] - ll[i][0]) * fa, ll[i][1] + (ll[i - 1][1] - ll[i][1]) * fa], [ll[i][0] + (ll[i + 1][0] - ll[i][0]) * fc, ll[i][1] + (ll[i + 1][1] - ll[i][1]) * fc]);
          cut++;
        } else out.push(ll[i]);
      }
      out.push(ll[ll.length - 1]);
      ll = out;
      if (!cut) break;
    }
    // (Two points that the cutting left on one spot are one point.)
    ll = ll.filter((p, i) => i === 0 || i === ll.length - 1 || Math.hypot(uv(p)[0] - uv(ll[i - 1])[0], uv(p)[1] - uv(ll[i - 1])[1]) > 0.03);
  }
  return ll;
}
const TOL = 1.2;
// (A course is drawn to the top of the chart and no further: the Lena's delta lies beyond it.)
const TOP = 71.4;
const onChart = (ll) => { let a = 0, b = ll.length; while (a < b - 2 && ll[a][1] > TOP) a++; while (b > a + 2 && ll[b - 1][1] > TOP) b--; return ll.slice(a, b); };
// (And when it has been eased it is thinned again, so that a point stands only where the course bends: the line
// is drawn as a curve through its points, and none of them is further than FINE from the eased course.)
const FINE = 0.6;
const draw = (ll) => onChart(thin(ease(thin(ll, TOL)), FINE));

// The survey's lakes (ne_10m_lakes.geojson, a second path), for a river that ends in one: the lakes that were
// there in 1939 (a reservoir only if its dam was built by then).
const lakesFile = process.argv.slice(3).find((v) => !v.startsWith('--'));
const lakes = [];
if (lakesFile) for (const ft of JSON.parse(fs.readFileSync(lakesFile, 'utf8')).features) {
  const g = ft.geometry, pr = ft.properties, name = fold(pr.name);
  if (!g || !name || (pr.featurecla === 'Reservoir' && !(pr.year > 0 && pr.year <= 1939))) continue;
  for (const poly of g.type === 'Polygon' ? [g.coordinates] : g.coordinates) lakes.push({ name, ring: poly[0].map(uv) });
}
const inRing = (ring, x, y) => { let inside = false; for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) { const [xi, yi] = ring[i], [xj, yj] = ring[j]; if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside; } return inside; };
function lakeAt(end) {
  // The chart's own lakes first, by their own names.
  const x = cx(end[0]), y = cy(end[1]);
  for (const l of chart.lakes) for (const w of [0, W, -W]) if (inRing(l.outline, x + w, y) || nearest([x + w, y], l.outline).d < 5) return l.name;
  const p = uv(end);
  let best = null;
  for (const l of lakes) { const d = inRing(l.ring, p[0], p[1]) ? 0 : nearest(p, l.ring).d; if (d < 4 && (!best || d < best.d)) best = { d, name: l.name }; }
  if (!best) return null;
  const row = LAKE_NAMES.find((n) => fold(n[0]) === best.name);
  return row ? row[row.length - 1] : best.name;
}
// How far a place is from the coast of the chart (the sea's, and the shores of the lakes cut out of its land).
const CELL = 24, shore = new Map();
for (const poly of chart.land) for (const ring of poly) for (let i = 0; i < ring.length; i++) {
  const a = ring[i], b = ring[(i + 1) % ring.length];
  for (const k of new Set([`${Math.floor(a[0] / CELL)}:${Math.floor(a[1] / CELL)}`, `${Math.floor(b[0] / CELL)}:${Math.floor(b[1] / CELL)}`])) (shore.get(k) ?? shore.set(k, []).get(k)).push([a, b]);
}
function coast(end) {
  let best = Infinity;
  for (const w of [0, W, -W]) {
    const x = cx(end[0]) + w, y = cy(end[1]), gx = Math.floor(x / CELL), gy = Math.floor(y / CELL);
    for (let i = gx - 1; i <= gx + 1; i++) for (let j = gy - 1; j <= gy + 1; j++) for (const [a, b] of shore.get(`${i}:${j}`) ?? []) best = Math.min(best, nearest([x, y], [a, b]).d);
  }
  return best;
}
const wet = (end) => coast(end) < 8 || lakeAt(end) !== null;
// What a river runs into: the river, old or new, that its foot touches; else the lake or the sea it ends in.
// (A river handed over the wrong way round, foot first, is turned to run from its head: RIVER_FLIP.)
const all = [...have.map((rv) => { const ll = real[rv.name]?.ll ?? rv.ll.map(([lat, lon]) => [turn(lon), lat]); return { name: rv.name, ll: RIVER_FLIP.includes(rv.name) ? [...ll].reverse() : ll, old: true }; }), ...found];
for (const r of all) r.p = r.ll.map(uv);
const zones = chart.seas.zones;
function into(r) {
  // The rivers a point lies on: in their course (a tributary's foot), at their head (the river runs on under
  // another name) or at their foot (two rivers that end together).
  const touch = (pt, reach) => {
    const hits = [];
    for (const o of all) {
      if (o === r) continue;
      const n = nearest(pt, o.p);
      if (n.d >= reach) continue;
      const L = len(o.p);
      hits.push({ o, d: n.d, kind: n.s > 8 && n.s < L - 8 ? 0 : n.s <= 8 ? 1 : 2 });
    }
    return hits.sort((a, b) => a.kind - b.kind || a.d - b.d)[0];
  };
  const reach = r.old ? 12 : 5;
  let f = touch(r.p[r.p.length - 1], reach), h = touch(r.p[0], reach);
  // Which end is the foot: the end that lies in another river's course; else the end at the sea or a lake; else the
  // survey's own word, or the lists'. (The chart's own 60 run as they were handed over.)
  if (!r.old && !r.hand) {
    const fIn = f && f.kind === 0, hIn = h && h.kind === 0;
    let flip = false;
    if (fIn !== hIn) flip = hIn;
    else if (!fIn) { const fw = wet(r.ll[r.ll.length - 1]), hw = wet(r.ll[0]); if (fw !== hw) flip = hw; }
    if (RIVER_FLIP.includes(r.name)) flip = !flip;
    if (flip) { r.ll.reverse(); r.p.reverse(); r.turned = true; [f, h] = [h, f]; }
  }
  const named = r.hand ? r.hand[2] : RIVER_INTO[r.name];
  if (named) {
    // Named by the lists: where that is a river whose course lies near, the foot is still set on its line.
    const o = all.find((o) => o !== r && o.name === named);
    if (o && nearest(r.p[r.p.length - 1], o.p).d < 25) r.joins = o;
    return named;
  }
  if (f && f.kind === 0) { r.joins = f.o; return f.o.name; }
  const end = r.ll[r.ll.length - 1];
  const lake = lakeAt(end);
  if (lake) return lake;
  if (f && f.kind === 1) { r.joins = f.o; return f.o.name; }
  if (coast(end) < 12) {
    const pr = provinceAt(end[0], end[1]);
    if (pr && pr.seas.length) {
      const x = cx(end[0]), y = cy(end[1]);
      return pr.seas.map((s) => zones[s]).sort((a, b) => Math.hypot(a.x - x, a.y - y) - Math.hypot(b.x - x, b.y - y))[0].name;
    }
  }
  if (f) { r.joins = f.o; return f.o.name; }
  return null;
}
for (const r of all) r.into = into(r);

// Each course as it is drawn; and where a river runs into another, its foot is set on the line of the other as
// drawn, so that no gap shows between them.
for (const r of all) r.drawn = r.old && !real[r.name] ? null : draw(r.ll);
for (const r of all) {
  if (!r.drawn || !r.joins?.drawn) continue;
  const o = r.joins.drawn.map(uv), foot = uv(r.drawn[r.drawn.length - 1]);
  let best = null;
  for (let i = 0; i + 1 < o.length; i++) {
    const a = o[i], b = o[i + 1], dx = b[0] - a[0], dy = b[1] - a[1], L2 = dx * dx + dy * dy;
    const t = L2 ? Math.max(0, Math.min(1, ((foot[0] - a[0]) * dx + (foot[1] - a[1]) * dy) / L2)) : 0;
    const p = [a[0] + t * dx, a[1] + t * dy], d = Math.hypot(p[0] - foot[0], p[1] - foot[1]);
    if (!best || d < best.d) best = { d, p };
  }
  if (best && best.d < 40) {
    const at = [best.p[0] / K, -best.p[1] / 40], n = r.drawn.length;
    // (The foot is moved there if it is close, or if running on to it would break the course's last bend.)
    const a = uv(r.drawn[n - 2]), b = uv(r.drawn[n - 1]);
    const t = Math.abs(Math.abs(((Math.atan2(best.p[1] - b[1], best.p[0] - b[0]) - Math.atan2(b[1] - a[1], b[0] - a[0])) * 180) / Math.PI + 540) % 360 - 180);
    if (best.d < 1.5 || (t > 35 && best.d < 6)) r.drawn[n - 1] = at; else r.drawn.push(at);
  }
}
const written = (r) => r.drawn.map(([lon, lat]) => [Math.round(lat * 10000) / 10000, Math.round(lon * 10000) / 10000]);

// ---- Written out ---------------------------------------------------------------------------------------------------------

const outFile = new URL('../data/campaign/more-rivers.mjs', import.meta.url);
let order = [];
if (!fresh && fs.existsSync(outFile)) order = (await import(outFile.href)).MORE_RIVERS.map((r) => r[0]);
const rank = new Map(order.map((n, i) => [n, i]));
const more = [...found].sort((a, b) => (rank.get(a.name) ?? 1e9) - (rank.get(b.name) ?? 1e9));
const gone = order.filter((n) => !found.some((r) => r.name === n));
const q = (v) => JSON.stringify(v);
const text = `// Written by tools/survey-rivers.mjs from the rivers of the Natural Earth survey (public domain,
// naturalearthdata.com) and the lists in detail.mjs. Not to be changed by hand: change the lists and run the tool.
// A course is [lat, lon] points from the head of the river to its foot.

/** The real courses of the 60 rivers handed over with the chart: name, how great, into what, the course. */
export const REAL_COURSES = [
${have.map((rv) => { const r = all.find((o) => o.old && o.name === rv.name); return `  [${q(rv.name)}, ${q(great.has(rv.name) ? 'great' : 'middle')}, ${q(r.into)}, ${r.drawn ? q(written(r)) : 'null'}${seasonal.has(rv.name) ? ', { seasonal: true }' : ''}],`; }).join('\n')}
];

/** The rivers and streams the chart did not have, in the order of their numbers (the first is river 60). */
export const MORE_RIVERS = [
${more.map((r) => `  [${q(r.name)}, ${q(r.class)}, ${q(r.into)}, ${q(written(r))}${seasonal.has(r.name) ? ', { seasonal: true }' : ''}],`).join('\n')}
];
`;
fs.writeFileSync(outFile, text);

const count = (c) => more.filter((r) => r.class === c).length;
console.log(`the chart's 60 rivers: ${Object.keys(real).length} on their real courses${realNotes.length ? `\n  ${realNotes.join('\n  ')}` : ''}`);
console.log(`new: ${more.length} (${count('great')} great, ${count('middle')} middle, ${count('small')} streams); ${(fs.statSync(outFile).size / 1024).toFixed(0)} KB`);
if (unlooped.length) console.log(`walks that came back on themselves, cut where they turned back (${unlooped.length}):\n  ${unlooped.join('\n  ')}`);
if (clash.length) console.log(`! two rivers share a name:\n  ${clash.join('\n  ')}`);
if (gone.length) console.log(`! rivers that were in more-rivers.mjs and are not found now: ${gone.join(', ')}`);
const noInto = all.filter((r) => !r.into).map((r) => r.name);
if (noInto.length) console.log(`run into nothing the tool can name (${noInto.length}): ${noInto.join(', ')}`);
if (list) {
  for (const part of PARTS) {
    const rows = more.filter((r) => r.part === part);
    console.log(`\n${part}: ${rows.length}`);
    for (const r of rows) console.log(`  ${r.class.padEnd(6)} r${r.rank} ${r.name.padEnd(24)} ${String(Math.round(r.len)).padStart(5)}u  ${r.ll[0][1].toFixed(1)},${r.ll[0][0].toFixed(1)} -> ${r.ll[r.ll.length - 1][1].toFixed(1)},${r.ll[r.ll.length - 1][0].toFixed(1)}  into ${r.into ?? '?'}${r.turned ? ' (turned)' : ''}${r.name !== r.ne ? `  [survey: ${r.ne}]` : ''}`);
  }
  console.log('\nThe 60 on their real courses (units as handed over -> now, into):');
  for (const rv of have) { const r = all.find((o) => o.old && o.name === rv.name); console.log(`  ${rv.name.padEnd(16)} ${real[rv.name] ? `${String(real[rv.name].was).padStart(5)} -> ${String(real[rv.name].now).padStart(5)}` : '   as handed over'}  into ${r.into ?? '?'}`); }
}
