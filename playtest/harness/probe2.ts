import { campaign, date, atWar, allied, T, hostileUnits, collapseScore, armySize } from './lib';
const plan = { minOdds: 1.4, declare: [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED'], [20, 'YUG'], [22, 'SOV']] as [number, string][], land: true };
const run = campaign('GER', 1, 7, plan, { until: 76, onMonth: (run, row) => {
  const g = run.g;
  if (row.turn === 4 || row.turn === 5) {
    // the Maginot battles
  }
  if (row.turn === 52 || row.turn === 40) {
    console.log(`--- ${row.date}: the front as the German player sees it at the start of next month ---`);
    const me = 'GER';
    const targets = new Set<number>();
    for (const c of g.corps) if (c.nat === me && !c.dead) for (const j of g.regions[c.reg].nb) if (hostileUnits(g, g.regions[j], me).length) targets.add(j);
    for (const j of targets) {
      const r = g.regions[j];
      const cands = T.candidates(g, r);
      const out = cands.length ? T.attackOutlook(g, cands, r) : null;
      const def = hostileUnits(g, r, me);
      console.log(`  ${r.name} (${r.terrain}${r.fort ? ' fort' + r.fort.lv : ''}, ${r.ctrl}): ${def.length} corps ${def.reduce((s, u) => s + u.divs * u.str, 0).toFixed(1)} divs [${def.map((u) => `${u.kind}${u.divs}x${u.str.toFixed(2)}o${u.org.toFixed(2)}${u.dug ? 'D' : ''}`).join(' ')}] vs my ${cands.length} corps ${cands.reduce((s, u) => s + u.divs * u.str, 0).toFixed(1)} divs → odds ${out ? out.odds.toFixed(2) : '-'} (${out?.exact ? 'exact' : `band ${out?.lo.toFixed(2)}-${out?.hi.toFixed(2)}`})`);
    }
    // One ticket in full
    const first = [...targets][0];
    const out = T.attackOutlook(g, T.candidates(g, g.regions[first]), g.regions[first]);
    console.log('  ticket for', g.regions[first].name); for (const l of out?.lines ?? []) console.log(`     ${l.label}: ${l.value}${l.effect ? ' → ' + l.effect : ''}`);
    console.log('  SOV collapse score', collapseScore(g, 'SOV').toFixed(2), 'resilience', g.nations.SOV.resilience, 'cities:', ['Moscow', 'Leningrad', 'Stalingrad', 'Kiev', 'Gorky', 'Baku', 'Rostov'].map((n) => `${n}=${g.byName[n]?.ctrl}`).join(' '));
    console.log('  my corps by distance: next to enemy', g.corps.filter((c) => c.nat === me && g.regions[c.reg].nb.some((j) => hostileUnits(g, g.regions[j], me).length)).length, 'of', g.corps.filter((c) => c.nat === me).length);
    console.log('  supplied', g.corps.filter((c) => c.nat === me && c.supplied).length, 'unsupplied', g.corps.filter((c) => c.nat === me && !c.supplied).length);
  }
} });
const g = run.g;
// The fall of France: every German attack on French soil before May 1940
for (const r of g.reports.filter((r) => r.attNat === 'GER' && r.defNat === 'FRA' && r.turn <= 8).slice(0, 14)) {
  console.log(`${date(r.turn)} wk${r.week} ${r.regionName}: odds ${r.odds.toFixed(2)} ${r.level} taken=${r.taken} att ${r.attackers.length} corps ${r.attackers.reduce((s, u) => s + u.divs, 0)} divs vs ${r.defenders.length} corps ${r.defenders.reduce((s, u) => s + u.divs, 0)} divs; lost ${r.attLost}/${r.defLost}`);
  for (const l of r.lines) console.log(`     ${l.label}: ${l.value}${l.effect ? ' → ' + l.effect : ''}`);
}
