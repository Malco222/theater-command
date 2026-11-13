// Wars with nobody at the controls: how the computer plays against itself.
import { start, date, T, check, armySize, atWar, allied, hostileUnits, collapseScore } from './lib';
import { ledger } from '@tc/campaign/economy';
import { besieged, siegeRules } from '@tc/campaign/siege';
const args = process.argv.slice(2);
const seeds = (args[0] ?? '1,2,3,4,5,6,7,8,9,10,11,12').split(',').map(Number);
const until = Number(args[1] ?? 88);
const rows: string[] = [];
const agg: Record<string, number> = {};
const bump = (k: string, n = 1): void => { agg[k] = (agg[k] ?? 0) + n; };
for (const seed of seeds) {
  const g = start(null, 1, seed);
  const problems: string[] = []; const seen = new Set<string>();
  const battlesByYear: Record<number, number> = {};
  let lastRep = 0, lastHead = 0;
  const repeats: Record<string, number> = {};
  const failRun: Record<string, number> = {}; let worstRun = 0, worstRunAt = '';
  const t0 = Date.now();
  let maxStack = 0, maxStackAt = '';
  const hoard: Record<string, number> = {};
  const sieges = new Map<number, { since: number; holder: string }>(); const siegeDone: { name: string; months: number; fell: boolean }[] = [];
  while (g.turn <= until) {
    const t = g.turn;
    T.runTurnAI(g);
    check(g, 'month end', problems, seen);
    const year = 1939 + Math.floor((t + 7) / 12);
    for (const r of g.reports.slice(lastRep)) {
      battlesByYear[year] = (battlesByYear[year] ?? 0) + 1;
      const lostIt = !r.taken && !(r as any).siege && /defeat|repuls|costly|failed/i.test(String(r.level));
      bump('battles'); if (lostIt) bump('attacks failed');
      if ((r as any).town) { bump('assaults on a town'); if (r.taken) bump('assaults that took the town'); }
      if ((r as any).siege) bump('field wins that began a siege');
      if ((r as any).relief) bump('relief attacks');
      if ((r as any).sally) bump('sorties by a garrison');
      bump('battle level: ' + r.level);
      if (r.odds < 0.8) bump('attacks at under 0.8 odds');
      const k = `${r.attNat}>${r.regionName}`;
      if (lostIt) { failRun[k] = (failRun[k] ?? 0) + 1; if (failRun[k] > worstRun) { worstRun = failRun[k]; worstRunAt = `${k} by ${date(t)}`; } } else delete failRun[k];
    }
    lastRep = g.reports.length;
    for (const h of g.headlines.slice(lastHead)) {
      const key = h.text.replace(/\d[\d,.]*/g, '#');
      repeats[key] = (repeats[key] ?? 0) + 1;
      if (/0 \w+ aircraft destroyed, 0 aircraft lost/.test(h.text)) bump('sky fights with nobody there');
    }
    lastHead = g.headlines.length;
    for (const r of g.regions) { const by: Record<string, number> = {}; for (const u of r.units) if (u.kind !== 'eng' && u.kind !== 'fort') by[u.nat] = (by[u.nat] ?? 0) + 1; for (const [n, k] of Object.entries(by)) if (k > maxStack) { maxStack = k; maxStackAt = `${k} ${n} corps in ${r.name}, ${date(t)}`; } }
    for (const r of g.regions) {
      const on = besieged(g, r), was = sieges.get(r.id);
      if (on && !was) sieges.set(r.id, { since: t, holder: r.ctrl });
      else if (was && (!on || r.ctrl !== was.holder)) { const m = t - was.since; siegeDone.push({ name: r.name, months: m, fell: r.ctrl !== was.holder }); sieges.delete(r.id); if (on) sieges.set(r.id, { since: t, holder: r.ctrl }); }
    }
    if (t % 12 === 4) for (const n of Object.values(g.nations)) if (n.alive && n.pp > (hoard[n.id] ?? 0)) hoard[n.id] = n.pp;
  }
  const fell = (id: string): string => { const f = g.nations[id].fellOn; return f ? `${date(f.turn).replace(/(\w{3})\w* (\d{2})(\d{2})/, '$1 $3')}${g.nations[id].alive ? '*' : ''}` : '-'; };
  const top = Object.entries(repeats).sort((a, b) => b[1] - a[1]).slice(0, 3).map(([k, v]) => `x${v} "${k.slice(0, 70)}"`).join(' | ');
  rows.push(`seed ${String(seed).padStart(2)}: FRA ${fell('FRA').padEnd(7)} SOV ${fell('SOV').padEnd(7)} GER ${fell('GER').padEnd(7)} ITA ${fell('ITA').padEnd(7)} UK ${fell('UK').padEnd(6)} | east war ${g.flags['war:GER:SOV'] ? 'GER ' + date(g.flags['war:GER:SOV']) : g.flags['war:SOV:GER'] ? 'SOV ' + date(g.flags['war:SOV:GER']) : 'never'} | battles/yr ${[1939, 1940, 1941, 1942, 1943, 1944, 1945, 1946].map((y) => battlesByYear[y] ?? 0).join(' ')} | worst fail run ${worstRun} (${worstRunAt}) | max stack ${maxStackAt} | problems ${problems.filter((p) => !p.includes('[note]')).length} | ${Math.round((Date.now() - t0) / 1000)}s`);
  rows.push(`         hoards: ${Object.entries(hoard).filter(([, v]) => v > 300).sort((a, b) => b[1] - a[1]).slice(0, 5).map(([k, v]) => `${k} ${Math.round(v * 100)}`).join(', ')} | repeats: ${top}`);
  if (siegeRules(g)) {
    const long = [...siegeDone].sort((a, b) => b.months - a.months).slice(0, 3).map((x) => `${x.name} ${x.months}mo ${x.fell ? 'fell' : 'raised'}`).join(', ');
    const open = [...sieges].map(([id, x]) => `${g.regions[id].name} ${until + 1 - x.since}mo`).sort((a, b) => Number(b.match(/(\d+)mo/)![1]) - Number(a.match(/(\d+)mo/)![1]));
    rows.push(`         sieges: ${siegeDone.length} ended, ${siegeDone.filter((x) => x.fell).length} fell, mean ${siegeDone.length ? (siegeDone.reduce((s2, x) => s2 + x.months, 0) / siegeDone.length).toFixed(1) : '-'} months; longest ${long || '-'}; still open at the end ${open.length}: ${open.slice(0, 5).join(', ')} | tallies ${Object.keys(g.flags).filter((k) => k.startsWith('sgn:')).map((k) => `${k.slice(4)} ${g.flags[k]}`).join(', ')}`);
    bump('sieges ended', siegeDone.length); bump('sieges where the town fell', siegeDone.filter((x) => x.fell).length); bump('sieges still open at the end', open.length); bump('sieges longer than 12 months', siegeDone.filter((x) => x.months > 12).length + open.filter((x) => Number(x.match(/(\d+)mo/)![1]) > 12).length);
  }
  for (const p of problems.filter((p) => !p.includes('[note]'))) { const k = p.replace(/^\[[^\]]*\] /, '').replace(/ in [A-Z][\w -]*?,/, ' in X,').replace(/\(.*$/, '').replace(/[A-Z][a-z]+( [A-Z][a-z]+)* (is held|holds)/, 'X $2').slice(0, 60); bump('problem: ' + k); }
  if (seed === seeds[0]) console.log(problems.filter((p) => !p.includes('[note]')).slice(0, 12).join('\n'));
}
console.log(rows.join('\n'));
console.log(Object.entries(agg).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${v}  ${k}`).join('\n'));
