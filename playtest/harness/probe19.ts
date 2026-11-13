// How an army melts: month by month, a nation's corps and divisions, where the corps that vanished were last seen
// (in a town, shut in, in the field), and what the battles against it cost.
// usage: probe19 ME FROM SEED NATION FIRST UNTIL [PLAN-JSON] [LEVEL]
import { campaign, date } from './lib';
import { besieged } from '@tc/campaign/siege';
const [me, fromS, seedS, nat, firstS, untilS, planS, level] = process.argv.slice(2);
const plan = planS ? JSON.parse(planS) : { minOdds: 1.5 };
let seen = new Map<number, { town: boolean; shut: boolean; divs: number; reg: string }>();
let r0 = 0;
campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g) => {
  const mine = g.corps.filter((c) => c.nat === nat && !c.dead);
  const now = new Map<number, { town: boolean; shut: boolean; divs: number; reg: string }>();
  for (const c of mine) now.set(c.id, { town: !!(c as any).town, shut: besieged(g, g.regions[c.reg]), divs: c.divs, reg: g.regions[c.reg].name });
  const gone = [...seen.entries()].filter(([id]) => !now.has(id)).map(([, v]) => v);
  const reps = g.reports.slice(r0) as any[]; r0 = g.reports.length;
  if (g.turn >= Number(firstS)) {
    const def = reps.filter((b) => b.defNat === nat), att = reps.filter((b) => b.attNat === nat);
    const sum = (xs: any[], k: string) => xs.reduce((s, b) => s + (b[k] ?? 0), 0);
    const div = (cs: any[]) => cs.reduce((s, c) => s + c.divs, 0);
    const sov = g.corps.filter((c) => c.nat === 'SOV' && !c.dead);
    console.log(`${date(g.turn)}: ${nat} ${mine.length} corps ${div(mine)} divs (mean strength ${(mine.reduce((s, c) => s + c.str, 0) / Math.max(1, mine.length)).toFixed(2)}, in towns ${mine.filter((c) => (c as any).town).length}); SOV ${div(sov)} divs; gone since last month ${gone.length} corps ${div(gone)} divs: ${gone.filter((v) => v.town).length} from inside a town, ${gone.filter((v) => !v.town && v.shut).length} outside a besieged town, ${gone.filter((v) => !v.town && !v.shut).length} in the open; attacked ${def.length} times (lost the field ${def.filter((b) => b.taken || b.siege || b.gates).length}, of which assaults on a town ${def.filter((b) => b.town).length}), men lost defending ${sum(def, 'defLost')}, prisoners ${sum(def, 'prisoners')}; own attacks ${att.length}, men lost attacking ${sum(att, 'attLost')}`);
  }
  seen = now;
} }, { until: Number(untilS), level: level ?? 'colonel' } as any);
