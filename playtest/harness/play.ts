// The player who plays: an average commander with a simple plan for each country.
import { campaign, summary, date, T, atWar, allied, type Plan, type Run } from './lib';

const PLANS: Record<string, Plan> = {
  // Germany follows history's timetable: Poland, the West in May 1940, the Balkans and Russia in 1941.
  GER: { minOdds: 1.4, declare: [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED'], [20, 'YUG'], [22, 'SOV']], land: true },
  // Germany that never storms a town: it wins the field and waits for hunger (Version 54).
  GER_STARVE: { minOdds: 1.4, declare: [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED'], [20, 'YUG'], [22, 'SOV']], land: true, noStorm: true } as any,
  // Germany that never attacks Russia: can it finish Britain?
  GER_WEST: { minOdds: 1.4, declare: [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED']], land: true },
  // Germany that gambles on Sea Lion whatever the navy says.
  GER_SEALION: { minOdds: 1.4, declare: [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED']], land: true, landRisk: 0.7 },
  // Britain: fight where there is a front, land when there is none.
  UK: { minOdds: 1.5, land: true, buy: ['corps:arm', 'corps:inf', 'air:ftr', 'ship:dd', 'work:factory'] },
  // France the defender, and France that strikes while Germany is in Poland.
  FRA: { minOdds: 1.6, homeOnly: true },
  FRA_ATTACK: { minOdds: 1.3 },
  // The Soviet Union that waits, and the one that strikes first.
  SOV: { minOdds: 1.5 },
  SOV_FIRST: { minOdds: 1.4, declare: [[10, 'GER']] },
  // Italy that joins when France is falling, and Italy that goes its own way in the Balkans.
  ITA: { minOdds: 1.4, declare: [[10, 'FRA'], [10, 'UK'], [14, 'GRE']], land: true },
  ITA_BALKAN: { minOdds: 1.4, declare: [[2, 'YUG'], [6, 'GRE']] },
  POL: { minOdds: 1.6, homeOnly: true },
  BEL: { minOdds: 1.6, homeOnly: true, path: 'allies' },
  FIN: { minOdds: 1.6, homeOnly: true, path: 'league' },
  TUR: { minOdds: 1.4, path: 'axis' },
  ROM: { minOdds: 1.5, path: 'axis' },
  SWE: { minOdds: 1.5, path: 'league' },
  ESP: { minOdds: 1.4, path: 'axis' },
};

const argv = process.argv.slice(2);
const verbose = argv.includes('-v');
const showLand = argv.includes('-land');
const args = argv.filter((a) => !a.startsWith('-'));
const names = (args[0] ?? 'GER').split(',');
const seeds = (args[1] ?? '7').split(',').map(Number);
const from = Number(args[2] ?? 1);
const until = Number(args[3] ?? 76);
const level = args[4] ?? 'colonel';

for (const name of names) for (const seed of seeds) {
  const me = name.split('_')[0];
  const t = Date.now();
  const run = campaign(me, from, seed, PLANS[name] ?? {}, { until, level, checkSave: [from + 3, 40] });
  console.log(`=== plan ${name} ===`);
  console.log(summary(run));
  console.log(`  wall time ${Math.round((Date.now() - t) / 1000)} s`);
  const notes = run.problems.filter((p) => p.startsWith('[note]') || p.includes('[note]'));
  const probs = run.problems.filter((p) => !p.includes('[note]'));
  for (const p of probs.slice(0, 40)) console.log('   ', p);
  if (probs.length > 40) console.log(`    ... and ${probs.length - 40} more`);
  for (const e of run.errors) console.log('  ERROR', e);
  console.log(`  months (A I attacked, x attacked, l lost ground, g gained, . nothing): ${run.rows.map((r) => (r.attacksOrdered ? 'A' : r.hitByEnemy ? 'x' : r.lost ? 'l' : r.gained ? 'g' : '.')).join('')}`);
  console.log(`  wars over time: ${[...new Set(run.rows.map((r) => r.wars))].join(' | ')}`);
  console.log(`  by year: ${run.rows.filter((r) => (r.turn - 5) % 12 === 0).map((r) => `${r.date}: money ${r.treasury} (${r.net >= 0 ? '+' : ''}${r.net}), ${r.myCorps} corps/${r.myDivs} divs, ${r.myRegions} prov`).join('\n           ')}`);
  if (verbose) for (const r of run.rows) console.log(`   ${r.date}: att ${r.attacksOrdered}/${r.attacksWon} moves ${r.moves} hit ${r.hitByEnemy} +${r.gained}/-${r.lost} opts ${r.attackOptions}/${r.goodOptions} money ${r.treasury} ${r.bought.join(',')} ${r.notes.join('; ')}`);
  else for (const r of run.rows) if (r.notes.length) console.log(`   ${r.date}: ${r.notes.join('; ')}`);
  console.log('  armies each January (divisions):\n     ' + ((run as any).armies ?? []).join('\n     '));
  if (showLand) console.log(((run as any).landLog ?? []).slice(0, 60).join('\n'));
  console.log('');
}
