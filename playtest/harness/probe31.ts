// Version 58: writes a save for the page at a moment worth looking at: the first month (from FIRST on), before the
// scripted player attacks, in which an attack on a line that is dug in and fed is on offer at odds between 1.15 and 1.7 to 1 and with its works finished (the even-chance mark at 1.38 or more).
// Prints the province to attack and the province most of the attackers stand in, for ui18.cjs.
// usage: probe31 ME FROM SEED UNTIL PLAN-JSON OUT-PREFIX [FIRST]
import { writeFileSync } from 'node:fs';
import { campaign, date, saveGame } from './lib';
const [me, fromS, seedS, untilS, planS, out, firstS = '30'] = process.argv.slice(2);
let done = false;
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), onLook: (g: any, options: any[], pass: number) => {
  if (done || pass !== 0 || g.turn < Number(firstS)) return;
  const fit = options.filter((o) => o.out && o.out.even >= 1.38 && o.odds >= 1.15 && o.odds <= 1.7).sort((a, b) => b.odds - a.odds)[0];
  if (!fit) return;
  done = true;
  const by: Record<number, number> = {};
  for (const c of fit.cands) by[c.reg] = (by[c.reg] ?? 0) + 1;
  const from = Number(Object.keys(by).sort((a, b) => by[Number(b)] - by[Number(a)])[0]);
  writeFileSync(`${out}-prepared.json`, saveGame(g));
  writeFileSync(`${out}-prepared.txt`, JSON.stringify({ turn: g.turn, date: date(g.turn), target: fit.r.id, targetName: fit.r.name, from, fromName: g.regions[from].name, odds: fit.odds, chance: fit.out.chance, even: fit.out.even, attackers: fit.cands.map((c: any) => c.id) }));
  console.log(`${date(g.turn)}: saved. Attack ${fit.r.name} (${fit.r.id}) from ${g.regions[from].name} (${from}) with ${fit.cands.length} corps: odds ${fit.odds.toFixed(2)}, chance ${Math.round(fit.out.chance * 100)}%, even at ${fit.out.even.toFixed(2)}`);
} }, { until: Number(untilS), quiet: true });
for (const e of (run as any).errors ?? []) console.log('ERROR', e);
if (!done) console.log('no such month found');
