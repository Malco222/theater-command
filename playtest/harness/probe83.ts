// probe83 SEEDS [UNTIL=80] [FOE=COL] [T=13]: nobody playing; a computer-led FOE turns to the Axis and declares war on the
// United States in month T (13 = September 1940); FOE 'none' is the same war without it. One FOE line a war: when the
// first American corps stands on its soil, when it falls, Americans across the ocean and ashore in the west to stay,
// Germany's fall, American corps at home when Germany fell and at the end. Version 81's measure (rulings 75, 76, 80b).
// Build with build70.sh.
import { start, date, T, atWar } from './lib';
import { bloc, declareWar } from '@tc/campaign/game';
import { beyondEurope, warWorkLeft } from '@tc/campaign/world';
const [seedsS = '17', untilS = '80', foe = 'COL', tS = '13'] = process.argv.slice(2);
const mode = foe, T0 = Number(tS);
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', 3);
  const o: any = { declared: 0, ally: 0, ger: 0, ita: 0, full: 0, across: 0, first: '', firstT: 0, west: 0, gerFell: 0, itaFell: 0, cub: 0, mex: 0, bra: 0, braFell: 0, last: 0, err: '', onFoe: 0, foeFell: 0, home: 0, onFoeEnd: 0, homeFall: 0, n: 0, nFall: 0 };
  const field = (): any[] => g.corps.filter((c: any) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
  const island = (to: any): boolean => { const seen = new Set<number>([to.id]); const q = [to.id];
    for (let i = 0; i < q.length && seen.size <= 8; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j)) { seen.add(j); q.push(j); }
    return seen.size <= 8; };
  const observe = (t: number): void => {
    const U = g.nations.USA;
    if (!o.ally && bloc(U) === 'allies') o.ally = t;
    if (!o.ger && atWar(g, 'USA', 'GER')) o.ger = t;
    if (!o.ita && atWar(g, 'USA', 'ITA')) o.ita = t;
    if (!o.cub && g.nations.CUB && atWar(g, 'CUB', 'GER')) o.cub = t;
    if (!o.mex && g.nations.MEX && atWar(g, 'MEX', 'GER')) o.mex = t;
    if (!o.bra && g.nations.BRA && atWar(g, 'BRA', 'GER')) o.bra = t;
    if (!o.braFell && !g.nations.BRA.alive) o.braFell = t;
    if (foe !== 'none' && !o.foeFell && !g.nations[foe].alive) o.foeFell = g.nations[foe].fellOn?.turn ?? t;
    o.home = 0; o.onFoeEnd = 0; o.n = 0;
    for (const c of field()) { const r = g.regions[c.reg]; o.n++; if (r.owner === 'USA' && !r.overseas) o.home++; if (r.owner === foe) { o.onFoeEnd++; if (!o.onFoe) o.onFoe = t; } }
    if (!o.full && U.atWar.length && warWorkLeft(g, 'USA') <= 0) o.full = t;
    if (!o.itaFell && !g.nations.ITA.alive) o.itaFell = t;
    let west = false;
    for (const c of field()) { const r = g.regions[c.reg];
      if (beyondEurope(r) || r.overseas) continue;
      if (!o.across) o.across = t;
      if (!['UK', 'IRE', 'ICE'].includes(r.owner) && !island(r) && !o.first) { o.first = r.owner; o.firstT = t; }
      if (WEST.has(r.owner) && !island(r)) west = true; }
    if (west) { if (!o.west) o.west = t; } else o.west = 0;
  };
  try {
    while (g.turn <= Number(untilS)) {
      const t = g.turn; o.last = t;
      if (!o.declared && foe !== 'none' && t === T0) { g.nations[foe].align = 'axis'; declareWar(g, foe, 'USA'); o.declared = t; }
      observe(t);
      (T as any).runTurnAI(g);
      if (!o.gerFell && !g.nations.GER.alive) { o.gerFell = g.nations.GER.fellOn?.turn ?? g.turn; observe(g.turn); o.homeFall = o.home; o.nFall = o.n; o.westF = o.west; o.acrossF = o.across; if (foe === 'none' || o.foeFell || g.turn >= Number(untilS)) break; }
      if (o.gerFell && o.foeFell) { observe(g.turn); break; }
    }
  } catch (e: any) { o.err = String(e?.message ?? e).slice(0, 120); }
  const d = (t: number, no = 'never'): string => (t ? date(t) : no);
  const U = g.nations.USA;
  console.log(`FOE ${foe} seed ${seed}: declared ${d(o.declared)}; ` + (foe === 'none' ? '' : `first American corps on its soil ${d(o.onFoe)}; ${foe} ${o.foeFell ? `fell ${date(o.foeFell)} (${o.foeFell - o.declared} months after declaring)` : 'STANDS'}; `) + `America at war with Germany ${d(o.ger, 'NEVER')}; first corps across the ocean ${d(o.across)}; first ground on the continent ${o.first || 'none'} ${o.firstT ? date(o.firstT) : ''}; ashore in the west to stay ${d(o.gerFell ? o.westF : o.west)}; Germany ${o.gerFell ? `fell ${date(o.gerFell)}` : 'STANDS'}; American field corps the month Germany fell ${o.nFall}, at home ${o.homeFall}; at the end (${date(o.last)}) ${o.n}, at home ${o.home}, on ${foe}'s soil ${o.onFoeEnd}` + (o.err ? ` | ERROR ${o.err}` : ''));
}
