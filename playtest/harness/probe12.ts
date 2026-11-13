// Trace one town's siege month by month: the ring, the stores, who is inside and outside, and what the besieger's staff could do.
// usage: probe12 ME FROM SEED REGION FIRST UNTIL [PLAN-JSON]
import { campaign, date } from './lib';
import * as T from '@tc/campaign/turn';
import { townHeld, besieged, besiegers } from '@tc/campaign/siege';
const [me, fromS, seedS, name, firstS, untilS, planS] = process.argv.slice(2);
const plan = planS ? JSON.parse(planS) : { minOdds: 1.5 };
let last = '';
campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g) => {
  if (g.turn < Number(firstS)) return;
  const r = g.regions.find((x) => x.name === name)!;
  const v: any = T.townSiege(g, r.id);
  const out = r.units.filter((u) => !u.dead);
  const by: Record<string, number> = {};
  for (const u of out) by[u.nat] = (by[u.nat] ?? 0) + 1;
  const garr = ((r as any).garr ?? []).filter((u: any) => !u.dead);
  const line = `ctrl ${r.ctrl}; field ${Object.entries(by).map(([k, n]) => `${k}×${n}`).join(' ') || 'nobody'}; inside ${garr.map((u: any) => `${u.nat} ${u.name}`).join(', ') || 'guard only'}; ` +
    (v ? `since ${v.since} months ${v.months} stores ${v.stores} left ${v.left} closed ${v.closed} by ${JSON.stringify(v.by)} holder ${v.holder}` : 'no siege view') + `; held ${townHeld(g, r)} besieged ${besieged(g, r)}`;
  if (line !== last) { console.log(date(g.turn), line); last = line; }
} }, { until: Number(untilS) });
