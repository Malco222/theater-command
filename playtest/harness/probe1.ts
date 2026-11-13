import { campaign, date, atWar, allied, T } from './lib';
const seen: string[] = [];
const run = campaign('ITA', 1, 7, { idle: true }, { until: 87, onMonth: (run, row) => {
  const g = run.g;
  for (const c of g.corps) {
    if (c.nat !== 'BUL' && c.nat !== 'FIN') continue;
    const r = g.regions[c.reg];
    if (r.owner !== c.nat) seen.push(`${row.date}: ${c.nat} ${c.name} at ${r.name} (ctrl ${r.ctrl}, owner ${r.owner}) joint=${c.joint} atWar(${c.nat},${r.ctrl})=${atWar(g, c.nat, r.ctrl)} allied=${allied(g, c.nat, r.ctrl)}`);
  }
  if (row.turn >= 70) {
    const n = g.nations;
    console.log(row.date, 'BUL align', n.BUL.align, 'atWar', n.BUL.atWar.join(','), '| FIN align', n.FIN.align, 'alive', n.FIN.alive, 'atWar', n.FIN.atWar.join(','), '| GER alive', n.GER.alive, 'Berlin', g.byName['Berlin'].ctrl, 'Paris', g.byName['Paris'].ctrl, g.byName['Paris'].units.map((u) => u.nat + ':' + u.name).join(', '));
  }
} });
console.log(seen.slice(-60).join('\n'));
const g = run.g;
console.log(g.headlines.filter((h) => h.turn >= 76 && /Bulgaria|Finland|Finnish|Bulgarian|Germany|German/.test(h.text)).map((h) => `${date(h.turn)}: ${h.text}`).join('\n'));
