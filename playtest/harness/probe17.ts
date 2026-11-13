// Which harness option changes the war? usage: probe17 ME FROM SEED UNTIL PLAN-JSON
import { campaign, date } from './lib';
import { createHash } from 'node:crypto';
import * as T from '@tc/campaign/turn';
import { townHeld, besieged, guardOf, garrisonOf, ringClosed } from '@tc/campaign/siege';
import { atWar, collapseScore } from './lib';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS);
const sig = (g: any) => createHash('md5').update(JSON.stringify(g.regions.map((r: any) => [r.ctrl, r.units.length, (r.garr ?? []).length])) + g.corps.filter((c: any) => !c.dead).length).digest('hex').slice(0, 10);
const trace = (label: string, p: any, o: any) => {
  const marks: string[] = [];
  const run = campaign(me, Number(fromS), Number(seedS), p, { until: Number(untilS), ...o, onMonth: (r: any, row: any) => { marks.push(`${row.turn}:${sig(r.g)}`); } } as any);
  const ath = run.g.regions.find((r: any) => r.name === 'Athens');
  return { label, marks, athens: `${ath.ctrl}/${ath.units.map((u: any) => u.nat).join('')}` };
};
const base = trace('play.ts way (level colonel, save check 4 and 40)', { ...plan }, { level: 'colonel', checkSave: [4, 40] });
const all = (f: (g: any, r: any) => void) => (g: any) => { for (const r of g.regions) f(g, r); };
const tests = [
  trace('hook: besieged on every province', { ...plan, each: all((g, r) => besieged(g, r)) }, {}),
  trace('hook: guardOf', { ...plan, each: all((g, r) => guardOf(g, r)) }, {}),
  trace('hook: townHeld', { ...plan, each: all((g, r) => townHeld(g, r)) }, {}),
  trace('hook: ringClosed', { ...plan, each: all((g, r) => { if (besieged(g, r)) ringClosed(g, r); }) }, {}),
  trace('hook: townSiege', { ...plan, each: all((g, r) => T.townSiege(g, r.id)) }, {}),
  trace('hook: collapseScore of every nation', { ...plan, each: (g: any) => { for (const k of Object.keys(g.nations)) collapseScore(g, k); } }, {}),
];
const first = (x: string[], y: string[]) => { for (let i = 0; i < Math.min(x.length, y.length); i++) if (x[i] !== y[i]) return Number(x[i].split(':')[0]); return 0; };
console.log(`${base.label}: Athens at the end ${base.athens}`);
for (const t of tests) { const f = first(base.marks, t.marks); console.log(`${t.label}: ${f ? 'DIFFERENT from turn ' + f + ' (' + date(f) + ')' : 'the same war'}; Athens at the end ${t.athens}`); }
