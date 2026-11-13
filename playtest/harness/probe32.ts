// Version 59: the player's men. usage: probe32 ME FROM SEED UNTIL PLAN-JSON [OTHER-NATION]
// Every third month: the reserve (divisions' worth of men left to call up), the month's class, the player's corps and divisions,
// how full the corps are on average, how many men the corps are short of, provinces held; and the same for OTHER (GER for a Soviet
// player, SOV otherwise). At the end: the month the reserve first fell under half, under a quarter, and to nothing.
import { campaign, date } from './lib';
import { menLeft, classNow, poolAtStart } from '@tc/campaign/manpower';
const [me, fromS, seedS, untilS, planS, otherS] = process.argv.slice(2);
const other = otherS || (me === 'SOV' ? 'GER' : 'SOV');
const plan = JSON.parse(planS || '{"minOdds":1.5}');
const first: Record<string, number> = {};
let lastTurn = 0, attacks = 0, attacksYear: Record<number, number> = {};
const line = (g: any, nat: string): string => {
  const cs = g.corps.filter((c: any) => c.nat === nat && !c.dead && c.kind !== 'fort');
  const divs = cs.reduce((s: number, c: any) => s + c.divs, 0), full = cs.reduce((s: number, c: any) => s + c.divs * c.str, 0);
  return `${nat}: reserve ${menLeft(g, nat).toFixed(1)} of ${poolAtStart(g, nat).toFixed(0)}, class ${classNow(g, nat).toFixed(1)} a month, corps ${cs.length}, divisions ${divs}, filled ${divs ? Math.round((100 * full) / divs) : 0}%, short ${(divs - full).toFixed(1)}, provinces ${g.regions.filter((x: any) => x.ctrl === nat).length}`;
};
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, onAttack: (g: any) => { attacks++; const y = 1939 + Math.floor((g.turn + 7) / 12); attacksYear[y] = (attacksYear[y] ?? 0) + 1; }, each: (g: any) => {
  lastTurn = g.turn;
  const share = menLeft(g, me) / Math.max(1e-9, poolAtStart(g, me));
  if (share < 0.5 && !first.half) first.half = g.turn;
  if (share < 0.25 && !first.quarter) first.quarter = g.turn;
  if (share < 0.01 && !first.spent) first.spent = g.turn;
  if ((g.turn - 2) % 3 === 0) console.log(`${date(g.turn).padEnd(15)} ${line(g, me)} | ${line(g, other)}`);
} }, { until: Number(untilS), quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
console.log(`TOTAL ${me} seed ${seedS}: played to ${date(lastTurn)}${run.fellOn ? ` (fell ${run.fellOn})` : ''}; reserve under half ${first.half ? date(first.half) : 'never'}, under a quarter ${first.quarter ? date(first.quarter) : 'never'}, spent ${first.spent ? date(first.spent) : 'never'}; attacks ordered ${attacks}: ${Object.entries(attacksYear).map(([y, n]) => `${y} ${n}`).join(', ')}`);
