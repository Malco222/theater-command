// Version 78 (ruling 58a), the war nobody plays: an Axis country declares war on the United States early.
//   ger39  Germany declares in September 1939 (the design director's case)
//   ita39  Italy alone declares in September 1939 (ruling 50's third clause: December 1941 must still fire)
//   bra40  a computer-led Brazil turns to the Axis and declares in September 1940
//   base   nobody declares early
// usage: probe80 SEEDS(a,b,c) UNTIL MODE     (build with build70.sh)   One EARLY line a war.
import { start, date, T, atWar } from './lib';
import { bloc, declareWar } from '@tc/campaign/game';
import { beyondEurope, warWorkLeft } from '@tc/campaign/world';
const [seedsS = '17', untilS = '80', mode = 'ger39'] = process.argv.slice(2);
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', 3);
  const o: any = { declared: 0, ally: 0, ger: 0, ita: 0, full: 0, across: 0, first: '', firstT: 0, west: 0, gerFell: 0, itaFell: 0, cub: 0, mex: 0, bra: 0, braFell: 0, last: 0, err: '' };
  const field = (): any[] => g.corps.filter((c: any) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
  const island = (to: any): boolean => { const seen = new Set<number>([to.id]); const q = [to.id];
    for (let i = 0; i < q.length && seen.size <= 8; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j)) { seen.add(j); q.push(j); }
    return seen.size <= 8; };
  try {
    while (g.turn <= Number(untilS)) {
      const t = g.turn; o.last = t;
      if (!o.declared && mode === 'ger39' && t === 1) { declareWar(g, 'GER', 'USA'); o.declared = t; }
      if (!o.declared && mode === 'ita39' && t === 1) { declareWar(g, 'ITA', 'USA'); o.declared = t; }
      if (!o.declared && mode === 'bra40' && t === 13) { g.nations.BRA.align = 'axis'; declareWar(g, 'BRA', 'USA'); o.declared = t; }
      (T as any).runTurnAI(g);
      const U = g.nations.USA;
      if (!o.ally && bloc(U) === 'allies') o.ally = t;
      if (!o.ger && atWar(g, 'USA', 'GER')) o.ger = t;
      if (!o.ita && atWar(g, 'USA', 'ITA')) o.ita = t;
      if (!o.cub && g.nations.CUB && atWar(g, 'CUB', 'GER')) o.cub = t;
      if (!o.mex && g.nations.MEX && atWar(g, 'MEX', 'GER')) o.mex = t;
      if (!o.bra && g.nations.BRA && atWar(g, 'BRA', 'GER')) o.bra = t;
      if (!o.braFell && !g.nations.BRA.alive) o.braFell = t;
      if (!o.full && U.atWar.length && warWorkLeft(g, 'USA') <= 0) o.full = t;
      if (!o.itaFell && !g.nations.ITA.alive) o.itaFell = t;
      let west = false;
      for (const c of field()) { const r = g.regions[c.reg];
        if (beyondEurope(r) || r.overseas) continue;
        if (!o.across) o.across = t;
        if (!['UK', 'IRE', 'ICE'].includes(r.owner) && !island(r) && !o.first) { o.first = r.owner; o.firstT = t; }
        if (WEST.has(r.owner) && !island(r)) west = true; }
      if (west) { if (!o.west) o.west = t; } else o.west = 0;
      if (!g.nations.GER.alive) { o.gerFell = t; break; }
    }
  } catch (e: any) { o.err = String(e?.message ?? e).slice(0, 120); }
  const d = (t: number, no = 'never'): string => (t ? date(t) : no);
  const U = g.nations.USA;
  console.log(`EARLY ${mode} seed ${seed}: declared ${d(o.declared)}; America among the Allies ${d(o.ally)}; at war with Germany ${d(o.ger, 'NEVER')}, with Italy ${d(o.ita, 'NEVER')}; December 1941 event ${g.flags['event:pearl-harbor-world'] ? date(g.flags['event:pearl-harbor-world']) : 'never fired'}; all on war work ${d(o.full)}; first corps across the ocean ${d(o.across)}; first ground on the continent ${o.first || 'none'} ${o.firstT ? date(o.firstT) : ''}; ashore in the west to stay ${d(o.west)}; Cuba at war with Germany ${d(o.cub)}, Mexico ${d(o.mex)}, Brazil ${d(o.bra)}; Brazil ${o.braFell ? `fell ${date(o.braFell)}` : 'stands'}; Germany ${o.gerFell ? `fell ${date(o.gerFell)}` : 'STANDS'}; Italy ${o.itaFell ? `fell ${date(o.itaFell)}` : 'stands'}; played to ${date(o.last)}; America at the end: bloc ${bloc(U) ?? 'none'}, at war with ${U.atWar.filter((x: string) => g.nations[x]?.alive).join(' ') || 'nobody'}${o.err ? ` | ERROR ${o.err}` : ''}`);
}
