import { campaign, date, atWar, allied, T, hostileUnits, collapseScore, armySize, bloc } from './lib';
const plan = { minOdds: 1.4, declare: [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED'], [20, 'YUG'], [22, 'SOV']] as [number, string][], land: true };
for (const seed of [7, 11, 23]) {
  const run = campaign('GER', 1, seed, plan, { until: 50, onMonth: (run, row) => {
    const g = run.g;
    if ([21, 22, 23, 26, 34, 50].includes(row.turn)) {
      console.log(`seed ${seed} ${row.date}: at war with SOV: ${g.nations.SOV.atWar.join(',')} | axis bloc: ${Object.values(g.nations).filter((n) => n.alive && bloc(n) === 'axis').map((n) => `${n.id}(${n.align})`).join(' ')} | ITA at war with ${g.nations.ITA.atWar.join(',')}`);
    }
  } });
  const g = run.g;
  console.log(g.headlines.filter((h) => h.kind === 'war' && h.turn >= 18 && h.turn <= 30).map((h) => `  ${date(h.turn)}: ${h.text}`).join('\n'));
}
