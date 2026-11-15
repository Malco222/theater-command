// Version 74: a seat's diary. One line for every month in which the scripted player did something (notes, attacks,
// provinces won or lost, what was bought), and the run of quiet months between, so a campaign can be read as a player
// would live it. usage: probe66 ME FROM SEED UNTIL PLAN-JSON     (build with build70.sh)
import { campaign, date } from './lib';
const [me, fromS, seedS, untilS, planS = '{}'] = process.argv.slice(2);
const run = campaign(me, Number(fromS), Number(seedS), JSON.parse(planS), { until: Number(untilS) });
let quiet = 0, from = '';
const where = (g: any): string => { const by: Record<string, number> = {}; for (const c of g.corps) if (c.nat === me) { const r = g.regions[c.reg]; const k = r.owner === me ? 'home' : `${g.nations[r.owner]?.name ?? r.owner}`; by[k] = (by[k] || 0) + 1; } return Object.entries(by).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k} ${v}`).join(', '); };
for (const r of run.rows) {
  const busy = r.attacksOrdered || r.gained || r.lost || r.hitByEnemy || r.notes.length;
  if (!busy) { if (!quiet) from = r.date; quiet++; continue; }
  if (quiet) console.log(`   (${quiet} quiet month${quiet === 1 ? '' : 's'} from ${from})`);
  quiet = 0;
  console.log(`${r.date}: at war with ${r.wars || 'nobody'}; ${r.myCorps} corps; attacks ${r.attacksWon} won of ${r.attacksOrdered}; provinces +${r.gained} -${r.lost}${r.hitByEnemy ? `; attacked ${r.hitByEnemy} times` : ''}${r.bought.length ? `; bought ${r.bought.join(', ')}` : ''}${r.notes.length ? `; ${r.notes.join('; ')}` : ''}`);
}
if (quiet) console.log(`   (${quiet} quiet month${quiet === 1 ? '' : 's'} from ${from})`);
console.log(`DIARY ${me} seed ${seedS}: played to ${date(run.g.turn)}; the army at the end stands in: ${where(run.g)}; errors ${run.errors.length}${run.errors.length ? ': ' + run.errors.slice(0, 3).join(' | ') : ''}`);
