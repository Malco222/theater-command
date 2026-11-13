// Version 60: can a player make a promise read bolder by standing his troops back on the day he gives it?
// usage: probe35 ME FROM SEED AT-TURN PLAN-JSON asis|back [MONTHS]
// Plays the plan to AT-TURN. At the start of that month it lists the promises on offer, then (back) marches or rails
// every corps within two provinces of the best town away from it, lists the offers again, gives the promise for the
// longest time, and plays on for the months allowed with the player going for the town.
import { campaign, date, T, allied } from './lib';
import type { Game } from './lib';
const [me, fromS, seedS, atS, planS, mode, monthsS, townS] = process.argv.slice(2);
const at = Number(atS), months = Number(monthsS ?? 6);
const X = T as any;
let done = false, target = -1; const seenNotes = new Set<string>();
const hops = (g: Game, from: number): Map<number, number> => {
  const d = new Map<number, number>([[from, 0]]); let ring = [from];
  for (let k = 1; k < 12 && ring.length; k++) { const next: number[] = []; for (const i of ring) for (const j of g.regions[i].nb) if (!d.has(j)) { d.set(j, k); next.push(j); } ring = next; }
  return d;
};
const show = (g: Game, label: string, only = -1): void => {
  const rows = X.promiseTargetsOf(g, 12) as any[];
  if (!rows.length) console.log(`${label}: no promise on offer (${X.whyNotOrderPromise(g)})`);
  for (const t of rows) if (only < 0 || t.region === only) console.log(`${label}: ${t.town} (${t.size} of ${t.holder}, ${t.depth} province${t.depth > 1 ? 's' : ''} off) odds ${t.odds.toFixed(2)}: ${t.terms.map((x: any) => `${x.months}m ${x.band} +${x.trust}/-${x.price} trust, ${x.capital} capital, ${Math.round(x.money)} money`).join(' | ')}`);
};
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), vow: 'none', first: (g) => {
  if (g.turn !== at || done) return;
  done = true;
  console.log(`${date(g.turn)}: ${me} holds ${g.regions.filter((r) => r.ctrl === me).length} provinces; why not promise: ${X.whyNotOrderPromise(g)}`);
  show(g, 'ON OFFER');
  const rows = X.promiseTargetsOf(g, 12) as any[];
  const best = (townS ? rows.find((t) => t.town === townS) : null) ?? rows.filter((t) => t.depth === 1).sort((a, b) => b.odds - a.odds)[0] ?? rows[0];
  if (!best) return;
  target = best.region;
  if (mode === 'back') {
    const d = hops(g, target); let moved = 0, railed = 0, stuck = 0, near = 0;
    for (const c of g.corps) {
      if (c.nat !== me || c.dead || c.reg < 0 || (d.get(c.reg) ?? 99) > 2) continue;
      near++;
      // by rail first, to the farthest of the player's own ground; then on foot, as far from the town as the corps can get
      const own = g.regions.filter((r) => (r.ctrl === me || allied(g, r.ctrl, me)) && (d.get(r.id) ?? 99) >= 4).sort((a, b) => (d.get(a.id) ?? 99) - (d.get(b.id) ?? 99));
      let went = false;
      for (const r of own.slice(0, 12)) { if (X.orderRail(g, c, r.id)) { railed++; went = true; break; } }
      if (!went) {
        const m = X.reach(g, c) as Map<number, any>;
        const opts = [...m.keys()].filter((id) => !m.get(id).attack && (g.regions[id].ctrl === me || allied(g, g.regions[id].ctrl, me)) && (d.get(id) ?? 99) > (d.get(c.reg) ?? 0)).sort((a, b) => (d.get(b) ?? 0) - (d.get(a) ?? 0));
        if (opts.length && X.orderMove(g, c, opts[0])) { moved++; went = true; }
      }
      if (!went) { stuck++; if (stuck <= 4) { const m = X.reach(g, c) as Map<number, any>; console.log(`   stuck: ${c.name} ${c.kind} at ${g.regions[c.reg].name} (${d.get(c.reg)} off) ap ${c.ap} org ${c.org?.toFixed(2)} reach ${m.size}: ${[...m.keys()].map((id) => `${g.regions[id].name}:${g.regions[id].ctrl}:${d.get(id)}${m.get(id).attack ? ':attack' : ''}`).join(', ').slice(0, 300)}; why ${X.whyNotOrder?.(g, c) ?? ''}`); } }
    }
    console.log(`STOOD BACK from ${best.town}: ${near} corps within two provinces; ${railed} went by rail, ${moved} marched, ${stuck} could not move`);
    show(g, 'AFTER STANDING BACK', target);
  }
  const now = (X.promiseTargetsOf(g, 12) as any[]).find((t) => t.region === target);
  const term = now?.terms.find((x: any) => x.months === months) ?? now?.terms[now.terms.length - 1];
  if (!term) { console.log(`${best.town} is no longer on offer`); return; }
  const ok = X.orderPromise(g, target, term.months);
  console.log(`PROMISED ${term.town} in ${term.months} months: ${term.band} (odds read ${term.odds.toFixed(2)}); kept +${term.trust} trust, ${term.capital} capital, ${Math.round(term.money)} money; broken -${term.price}; accepted ${ok}`);
}, each: (g) => {
  const v = X.governmentOf(g); if (!v) return;
  for (const e of v.log as any[]) { const k = `${e.turn}|${e.text}`; if (e.turn < at || seenNotes.has(k)) continue; seenNotes.add(k); if (/romise/.test(e.text)) console.log(`   ${date(e.turn)}  ${e.text} (trust now ${v.trust})`); }
} } as any, { until: at + months + 1 });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 400));
