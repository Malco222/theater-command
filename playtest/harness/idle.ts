// The player who does nothing: picks a country and presses End Turn every month.
import { campaign, summary, date } from './lib';

const args = process.argv.slice(2);
const nats = (args[0] ?? 'GER,UK,FRA,SOV,ITA,POL').split(',');
const seed = Number(args[1] ?? 7);
const from = Number(args[2] ?? 1);
const until = Number(args[3] ?? 76);

for (const me of nats) {
  const t = Date.now();
  const run = campaign(me, from, seed, { idle: true }, { until, checkSave: [from + 2, 30, 60] });
  console.log(summary(run));
  console.log(`  wall time ${Math.round((Date.now() - t) / 1000)} s`);
  for (const p of run.problems.slice(0, 40)) console.log('   ', p);
  if (run.problems.length > 40) console.log(`    ... and ${run.problems.length - 40} more`);
  for (const e of run.errors) console.log('  ERROR', e);
  // The player's year at a glance.
  const line = run.rows.map((r) => (r.hitByEnemy ? 'x' : r.lost ? 'l' : r.gained ? 'g' : '.')).join('');
  console.log(`  months (x attacked, l lost ground, g gained, . nothing): ${line}`);
  console.log(`  wars over time: ${[...new Set(run.rows.map((r) => r.wars))].join(' | ')}`);
  console.log(`  treasury by year: ${run.rows.filter((r) => (r.turn - 5) % 12 === 0).map((r) => `${r.date}: ${r.treasury} (${r.net >= 0 ? '+' : ''}${r.net})`).join('; ')}`);
  console.log('');
}
