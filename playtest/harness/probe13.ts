// A frozen siege looked at closely: strengths outside and inside, who stands next door, and what battles the two sides fight elsewhere.
// usage: probe13 ME FROM SEED REGION FIRST UNTIL EVERY [PLAN-JSON]
import { campaign, date, atWar } from './lib';
import { garrisonOf, guardOf, besieged } from '@tc/campaign/siege';
const [me, fromS, seedS, name, firstS, untilS, everyS, planS] = process.argv.slice(2);
const plan = planS ? JSON.parse(planS) : { minOdds: 1.5 };
let r0 = 0;
campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g) => {
  if (g.turn < Number(firstS)) { r0 = g.reports.length; return; }
  if ((g.turn - Number(firstS)) % Number(everyS)) return;
  const r = g.regions.find((x) => x.name === name)!;
  const str = (cs: any[]) => cs.reduce((s, c) => s + c.divs * c.str, 0);
  const out = r.units.filter((u) => !u.dead), ins = garrisonOf(r);
  const holder = r.ctrl, foe = out.find((u) => atWar(g, u.nat, holder))?.nat ?? '-';
  const nb = r.nb.map((j) => g.regions[j]);
  const beside = (nat: string) => nb.reduce((s, x) => s + x.units.filter((u) => !u.dead && u.nat === nat).length, 0);
  const all = (nat: string) => g.corps.filter((c) => c.nat === nat && !c.dead).length;
  const reps = g.reports.slice(r0); r0 = g.reports.length;
  const ew = reps.filter((b: any) => (b.attNat === foe && b.defNat === holder) || (b.attNat === holder && b.defNat === foe));
  console.log(`${date(g.turn)}: ${name} held by ${holder}, besieged ${besieged(g, r)}; outside ${foe} ${out.length} corps strength ${str(out).toFixed(1)} (org ${out.map((u) => u.org.toFixed(2)).join(' ')}); inside ${ins.length} corps strength ${str(ins).toFixed(1)} + guard ${guardOf(g, r)}; ring needs ${(0.75 * (str(ins) + guardOf(g, r))).toFixed(1)}; next door: ${holder} ${beside(holder)} corps, ${foe} ${beside(foe)} corps; whole armies: ${holder} ${all(holder)}, ${foe} ${all(foe)}; battles between them since last line: ${ew.length} (${ew.filter((b: any) => b.attNat === holder).length} by ${holder})`);
} }, { until: Number(untilS) });
