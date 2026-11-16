// probe93: the computer's Germany and Italy at the depots (rider to ruling 168, Version 83.1 on).
//   node out831c/probe93.mjs ME 1 SEED UNTIL PLAN-JSON      (TC_LEVEL sets the level, default colonel; TC_NATS=GER,ITA)
// A seat is played by the lib's plan; each month, as it opens, the stores of every nation in TC_NATS that the player
// does not play are read: oil and supplies met, privation, and which fuel technologies its staff has finished.
// One STAFF line a nation at the end.
import { campaign, date } from './lib';
import { stores, willLost, RES } from '@tc/campaign/resources';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const nats = (process.env.TC_NATS || 'GER,ITA').split(',').filter((n) => n !== me);
const rows: Record<string, any[]> = {}; const fell: Record<string, number> = {}; const tech: Record<string, Record<string, number>> = {};
for (const n of nats) { rows[n] = []; tech[n] = {}; }
const first = (g: any): void => {
  for (const nat of nats) {
    const n = g.nations[nat];
    if (!n) continue;
    if (!n.alive) { if (fell[nat] === undefined) fell[nat] = g.turn; continue; }
    const st = n.res ?? {};
    let v: any = null; try { v = stores(g, nat); } catch { v = null; }
    for (const id of ['synthetics', 'hydrogenation']) if (n.tech?.done?.includes(id) && tech[nat][id] === undefined) tech[nat][id] = g.turn;
    rows[nat].push({ t: g.turn, hunger: st.hunger ?? 0, will: willLost(g, nat), oilMet: st.oilMet ?? 1, supMet: st.supMet ?? 1, oil: v ? v.oil.stock : 0, need: v ? v.oil.need : 0, made: v ? v.oil.made : 0 });
  }
};
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, first }, { until: Number(untilS), level: process.env.TC_LEVEL || 'colonel', quiet: true });
const g: any = run.g;
const pc = (x: number): string => `${Math.round(x * 100)}%`;
for (const nat of nats) {
  const R = rows[nat];
  if (!R.length) { console.log(`STAFF ${nat} (the player is ${me}, seed ${seedS}): no reading`); continue; }
  const firstOf = (p: (r: any) => boolean) => { const r = R.find(p); return r ? date(r.t) : 'never'; };
  const cnt = (p: (r: any) => boolean) => R.filter(p).length;
  const last = R[R.length - 1];
  const when = (id: string) => tech[nat][id] === undefined ? 'never' : (tech[nat][id] <= Number(fromS) + 1 ? 'at the start' : date(tech[nat][id]));
  console.log(`STAFF ${nat} (the player is ${me}, seed ${seedS}): ${fell[nat] !== undefined ? `FELL ${date(fell[nat])}` : `STANDS to ${date(g.turn)}`} | supplies short in ${cnt((r) => r.supMet < 0.995)} months of ${R.length} | oil first short ${firstOf((r) => r.oilMet < 0.995)}, short in ${cnt((r) => r.oilMet < 0.995)} months, worst ${pc(Math.min(...R.map((r) => r.oilMet)))} met | privation took a tenth of the will ${firstOf((r) => r.will >= 0.1)}, reached its cap ${firstOf((r) => r.will >= RES.will.most - 1e-6)}, months at the cap ${cnt((r) => r.will >= RES.will.most - 1e-6)}, on the last reading ${last.hunger.toFixed(2)} points = ${pc(last.will)} | synthetic fuel finished ${when('synthetics')}, hydrogenation ${when('hydrogenation')}`);
}
if (run.errors.length) console.log(`   RUNERR: ${run.errors[0].slice(0, 300)}`);
