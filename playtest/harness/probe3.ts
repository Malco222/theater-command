import { campaign, date, atWar, allied, T, hostileUnits, collapseScore, armySize } from './lib';
const plan = { minOdds: 1.4, declare: [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED'], [20, 'YUG'], [22, 'SOV']] as [number, string][], land: true };
const snap = (g: any, label: string) => {
  const sov = g.corps.filter((c: any) => c.nat === 'SOV');
  const by: Record<string, number> = {};
  for (const c of sov) by[g.regions[c.reg].name] = (by[g.regions[c.reg].name] ?? 0) + 1;
  const top = Object.entries(by).sort((a, b) => b[1] - a[1]).slice(0, 8).map(([k, v]) => `${k} ${v}`).join(', ');
  const mos = g.byName['Moscow'];
  console.log(`${label}: SOV ${sov.length} corps ${Math.round(armySize(g, 'SOV'))} divs in ${Object.keys(by).length} provinces; biggest stacks: ${top}`);
  console.log(`   Moscow: ctrl ${mos.ctrl}, ${mos.units.length} corps; neighbours: ${mos.nb.map((j: number) => `${g.regions[j].name}=${g.regions[j].ctrl}`).join(' ')}`);
  console.log(`   SOV collapse ${collapseScore(g, 'SOV').toFixed(2)}/${g.nations.SOV.resilience}; SOV treasury ${Math.round(g.nations.SOV.pp)}, queue ${g.nations.SOV.queue.length}; SOV provinces ${g.regions.filter((r: any) => r.ctrl === 'SOV').length}; with supply ${sov.filter((c: any) => c.supplied).length}/${sov.length}`);
};
const run = campaign('GER', 1, 7, plan, { until: 76, onMonth: (run, row) => { if ([28, 34, 40, 46, 52, 64, 76].includes(row.turn)) snap(run.g, row.date); } });
const g = run.g;
console.log(run.problems.filter((p) => p.includes('crowd')).slice(0, 30).join('\n'));
// Soviet attacks on the player after 1942
const sovAtt = g.reports.filter((r) => r.attNat === 'SOV');
console.log('SOV attacks in the last 200 battles kept:', sovAtt.length, sovAtt.slice(-10).map((r) => `${date(r.turn)} ${r.regionName} odds ${r.odds.toFixed(2)} ${r.taken ? 'won' : 'lost'}`).join('; '));
const mine = g.reports.filter((r) => r.attNat === 'GER' && r.turn > 40);
console.log('GER attacks after 1942:', mine.length, 'won', mine.filter((r) => r.taken).length, mine.slice(-14).map((r) => `${date(r.turn)} ${r.regionName} odds ${r.odds.toFixed(2)} ${r.taken ? 'won' : 'lost'} lost ${r.attLost}/${r.defLost}`).join('; '));
for (const r of g.reports.filter((r) => r.attNat === 'GER' && r.regionName === 'Vladimir').slice(-1)) {
  console.log(`\n${date(r.turn)} wk${r.week} ${r.regionName}: odds ${r.odds.toFixed(2)} ${r.level} att ${r.attackers.length} corps ${r.attackers.reduce((s, u) => s + u.divs, 0)} divs (${r.attackers.map((u) => u.kind + u.divs).join(' ')}) vs ${r.defenders.length} corps ${r.defenders.reduce((s, u) => s + u.divs, 0)} divs`);
  for (const l of r.lines) console.log(`     ${l.label}: ${l.value}${l.effect ? ' → ' + l.effect : ''}`);
}
{
  const me = 'GER';
  // What the player would be shown now for Vladimir and Gorky
  for (const nm of ['Vladimir', 'Gorky', 'Stalingrad', 'Kalach']) {
    const r = g.byName[nm]; const cands = T.candidates(g, r); const out = cands.length ? T.attackOutlook(g, cands, r) : null;
    console.log(nm, 'ctrl', r.ctrl, r.terrain, r.fort ? 'fort' + r.fort.lv : '', 'defenders', r.units.length, 'cands', cands.length, out ? `estimate ${out.odds.toFixed(2)} band ${out.lo.toFixed(2)}-${out.hi.toFixed(2)} exact=${out.exact} unidentified ${out.unidentified} stale ${out.stale}` : '');
    const a = cands.length ? T.previewAttack(g, cands, r) : null; if (a) console.log('    true odds', a.odds.toFixed(2));
  }
  const eng = g.corps.filter((c) => c.nat === me && c.kind === 'eng');
  console.log('GER engineers', eng.length, eng.map((c) => `${g.regions[c.reg].name}${c.job ? ':' + c.job.what : ''}`).join(', '));
  const tree = T.techTree(g)!; console.log('tech done', tree.done, '/', tree.total, 'winterkit', tree.nodes.find((n) => n.id === 'winterkit')?.state, 'teams', tree.teams, 'busy', tree.busy);
  const d = T.depots(g) as any; console.log('oil', JSON.stringify(d?.oil ? { have: d.oil.have, met: d.oil.met, months: d.oil.months } : d).slice(0, 300));
}
