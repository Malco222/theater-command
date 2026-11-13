import { campaign, date, atWar, allied, T, hostileUnits, collapseScore, armySize, ledger } from './lib';
const plan = { minOdds: 1.4, declare: [[10, 'FRA'], [10, 'UK'], [14, 'GRE']] as [number, string][], land: true };
const run = campaign('ITA', 1, 7, plan, { until: 76, onMonth: (run, row) => {
  const g = run.g;
  if (row.turn % 6 === 0 || row.turn >= 66) {
    const n = g.nations.ITA; const L = ledger(g, 'ITA');
    const d = T.depots(g) as any;
    console.log(`${row.date}: collapse ${collapseScore(g, 'ITA').toFixed(2)}/${n.resilience} hunger ${n.res?.hunger ?? 0} oil ${n.res?.oil.toFixed(0)} (met ${n.res?.oilMet.toFixed(2)}) sup ${n.res?.sup.toFixed(0)} (met ${n.res?.supMet.toFixed(2)}) | income ${Math.round(L.gross * L.scale)} upkeep ${Math.round(L.upkeepTotal * L.scale)} net ${Math.round(L.net * L.scale)} debt ${L.debt.toFixed(2)} | corps ${g.corps.filter((c) => c.nat === 'ITA').length} prov ${g.regions.filter((r) => r.ctrl === 'ITA').length} | offers: ${T.arsenalOffers(g, n.capital).filter((o) => o.group === 'army').map((o) => `${o.key} ${Math.round(o.cost * L.scale)}${o.why ? ' (' + o.why.slice(0, 40) + ')' : ''}`).slice(0, 3).join('; ')}`);
  }
} });
const g = run.g;
console.log(g.headlines.filter((h) => h.turn >= 66 && (/Ital|Rome/.test(h.text))).slice(-25).map((h) => `${date(h.turn)}: [${h.kind}] ${h.text}`).join('\n'));
console.log('changes:', JSON.stringify(ledger(g, 'ITA').changes));
