// probe92: the depots month by month for a seat played by hand, and a player who minds them (ruling 167, Version 83.1 on).
//   node out831b/probe92.mjs ME 1 SEED UNTIL PLAN-JSON      (TC_LEVEL sets the level, default colonel)
// PLAN-JSON is the lib's plan with one more key, "mind": a list of what the player does for the depots from month one:
//   "farms"      orders the next level of Farming wherever it can be built on home ground, before anything else is bought
//                ("farmKeep": the treasury kept back, in the page's money, default 0)
//   "synth"      researches Synthetic fuel and rubber, then Hydrogenation plants, ahead of everything else
// Each month, as it opens: what the depots hold, what came in and went out, the share of the need that was met, and the
// count of privation. Output: D lines (January, April, July, October and every month a depot is short), FARM lines once a
// year, then DEPOTS: the first short month of each good, the months short, when privation passed 0.10 and reached its
// cap, what the farms and plants brought in, what was spent on farms, and how the war ended.
import { campaign, date } from './lib';
import { stores, willLost, RES } from '@tc/campaign/resources';
import { civicOffers, orderCivic, civicSum } from '@tc/campaign/buildings';
import { ledger } from '@tc/campaign/economy';
import { reading } from '@tc/campaign/danger';

const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const mind: string[] = plan.mind ?? [];
if (mind.includes('synth')) plan.research = ['synthetics', 'hydrogenation', ...(plan.research ?? ['winterkit', 'mediums', 'tanks', 'motor'])];
const rows: any[] = []; let farmSpent = 0, farmOrders = 0; const errs: string[] = [];
const line = (b: any, keys: RegExp): number => b.income.filter((l: any) => keys.test(l.key)).reduce((s: number, l: any) => s + l.amount, 0);
const first = (g: any): void => {
  const n = g.nations[me];
  if (!n.alive) return;
  try {
    const v: any = stores(g, me);
    if (!v) return;
    const st = n.res ?? {};
    const r: any = reading(g, me);
    const row = {
      t: g.turn, hunger: st.hunger ?? 0, will: willLost(g, me),
      sup: v.sup.stock, supMade: v.sup.made, supNeed: v.sup.need, supMet: st.supMet ?? 1, supCap: v.sup.cap,
      oil: v.oil.stock, oilMade: v.oil.made, oilNeed: v.oil.need, oilMet: st.oilMet ?? 1,
      land: line(v.sup, /^(land|fac|homeland)$/), occ: line(v.sup, /^(occupied|colonies)$/), farms: line(v.sup, /^farms$/), supIn: line(v.sup, /^lane:/),
      fields: line(v.oil, /^(fields|taken|home)$/), plants: line(v.oil, /^tech$/), oilIn: line(v.oil, /^lane:/),
      divs: Math.round(g.corps.filter((c: any) => c.nat === me && !c.dead).reduce((s: number, c: any) => s + c.divs, 0)),
      count: r ? r.score : null, limit: r ? r.limit : n.resilience, lanes: v.lanes.map((l: any) => `${l.id}:${l.state}`).join(' '),
    };
    rows.push(row);
    if (mind.includes('farms')) {
      const L: any = ledger(g, me); const keep = (plan.farmKeep ?? 0) / (L.scale || 1);
      const offers: { region: number; cost: number }[] = [];
      for (const reg of g.regions) {
        if (reg.ctrl !== me || reg.owner !== me) continue;
        const o = civicOffers(g, me, reg.id, true).find((x: any) => x.line === 'farm');
        if (o && o.why === null) offers.push({ region: reg.id, cost: o.cost });
      }
      offers.sort((a, b) => a.cost - b.cost);
      for (const o of offers) { if (n.pp - o.cost < keep) break; const b = orderCivic(g, me, o.region, 'farm'); if (b) { farmSpent += o.cost * (L.scale || 1); farmOrders++; } }
    }
  } catch (e: any) { errs.push(`${date(g.turn)}: ${e.message}`); }
};
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, first }, { until: Number(untilS), level: process.env.TC_LEVEL || 'colonel', quiet: true });
const g: any = run.g; const n = g.nations[me];
const f1 = (x: number) => x.toFixed(1), pc = (x: number) => `${Math.round(x * 100)}%`;
const label = `${me} seed ${seedS}${mind.length ? ` minding ${mind.join('+')}` : ' as before'}`;
for (const r of rows) {
  const m = ((r.t + 7) % 12) + 1, short = r.supMet < 0.995 || r.oilMet < 0.995;
  if (!(m === 1 || m === 4 || m === 7 || m === 10 || short)) continue;
  console.log(`D ${label} ${date(r.t)}: SUPPLIES ${f1(r.sup)} of ${f1(r.supCap)} in the depots, in ${f1(r.supMade)} (home ${f1(r.land)}, farms ${f1(r.farms)}, taken ${f1(r.occ)}, from abroad ${f1(r.supIn)}), out ${f1(r.supNeed)}, met ${pc(r.supMet)} | OIL ${f1(r.oil)}, in ${f1(r.oilMade)} (wells ${f1(r.fields)}, plants ${f1(r.plants)}, from abroad ${f1(r.oilIn)}), out ${f1(r.oilNeed)}, met ${pc(r.oilMet)} | ${r.divs} divisions | privation ${r.hunger.toFixed(2)} = ${pc(r.will)} of the will${r.count !== null ? `, the count ${r.count.toFixed(2)} of ${r.limit}` : ''} | ${r.lanes}`);
}
const firstOf = (p: (r: any) => boolean) => { const r = rows.find(p); return r ? date(r.t) : 'never'; };
const cnt = (p: (r: any) => boolean) => rows.filter(p).length;
const last = rows[rows.length - 1], peak = rows.reduce((a, r) => (r.hunger > a.hunger ? r : a), rows[0]);
console.log(`DEPOTS ${label}: ${n.alive ? `STANDS to ${date(g.turn)}` : `FELL ${date((run as any).fellOn ?? g.turn)}`} | supplies first short ${firstOf((r) => r.supMet < 0.995)}, short in ${cnt((r) => r.supMet < 0.995)} months of ${rows.length}, worst ${pc(Math.min(...rows.map((r) => r.supMet)))} met | oil first short ${firstOf((r) => r.oilMet < 0.995)}, short in ${cnt((r) => r.oilMet < 0.995)} months, worst ${pc(Math.min(...rows.map((r) => r.oilMet)))} met | privation took a tenth of the will ${firstOf((r) => r.will >= 0.1)}, reached its cap ${firstOf((r) => r.will >= RES.will.most - 1e-6)}, months at the cap ${cnt((r) => r.will >= RES.will.most - 1e-6)}, peak ${peak.hunger.toFixed(2)} points (${date(peak.t)}), on the last card ${last.hunger.toFixed(2)} points = ${pc(last.will)} | farms: ${farmOrders} orders for ${Math.round(farmSpent)} in all, bringing ${f1(last.farms)} a month at the end (most ${f1(Math.max(...rows.map((r) => r.farms)))}); plants ${f1(Math.max(...rows.map((r) => r.plants)))} oil a month at most | divisions at most ${Math.max(...rows.map((r) => r.divs))}`);
if (errs.length) console.log(`   ERR ${label}: ${errs.slice(0, 3).join(' ;; ')} (${errs.length})`);
if (run.errors.length) console.log(`   RUNERR ${label}: ${run.errors[0].slice(0, 300)}`);
