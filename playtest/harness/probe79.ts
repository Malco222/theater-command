// Version 78 (ruling 58a): a great Axis power led by hand declares war on the United States early. "America early,
// not never": does America join the Allies, is it at war with the other Axis powers by December 1941, does its army
// cross, and what does the early war cost the player.
// usage: probe79 ME SEED UNTIL T PLAN-JSON    (T: the month the player declares on the United States, 0 never;
//        the plan's "force":true declares by the rules' own call where the page does not offer the war;
//        build with build70.sh)
// One U line each January and at the end, one END line a war.
import { campaign, date, T, atWar, goalState } from './lib';
import { bloc, declareWar } from '@tc/campaign/game';
import { beyondEurope, warWorkLeft } from '@tc/campaign/world';
const [me, seedS, untilS = '76', tS = '1', planS = '{}'] = process.argv.slice(2);
const X = T as any; const at = Number(tS); const force = !!JSON.parse(planS).force;
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
const o: any = { declared: 0, ally: 0, ger: 0, ita: 0, full: 0, across: 0, first: '', firstT: 0, west: 0, gerFell: 0, itaFell: 0, cub: 0, mex: 0, bra: 0, last: 0 };
const field = (g: any): any[] => g.corps.filter((c: any) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
const island = (g: any, to: any): boolean => { const seen = new Set<number>([to.id]); const q = [to.id];
  for (let i = 0; i < q.length && seen.size <= 8; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j)) { seen.add(j); q.push(j); }
  return seen.size <= 8; };
const read = (g: any, tag: string): void => {
  const u = g.nations.USA; const f = field(g);
  console.log(`U ${me} T${at} seed ${seedS} ${tag} | the United States: bloc ${bloc(u) ?? 'none'}, at war with ${u.atWar.filter((x: string) => g.nations[x]?.alive).join(' ') || 'nobody'}, war work left ${warWorkLeft(g, 'USA').toFixed(2)}, field corps ${f.length} (across the ocean ${f.filter((c: any) => { const r = g.regions[c.reg]; return !beyondEurope(r) && !r.overseas; }).length}) | Germany ${g.nations.GER.alive ? 'stands' : 'fallen'}, Italy ${g.nations.ITA.alive ? 'stands' : 'fallen'}`);
};
const plan: any = { ...JSON.parse(planS),
  first: (g: any) => {
    const t = g.turn; o.last = t; const U = g.nations.USA;
    if (at && !o.declared && t >= at && g.nations[me]?.alive && !atWar(g, me, 'USA') && (force ? (declareWar(g, me, 'USA'), true) : X.declarable(g).includes('USA') && X.orderDeclare(g, 'USA'))) o.declared = t;
    if (!o.ally && bloc(U) === 'allies') o.ally = t;
    if (!o.ger && atWar(g, 'USA', 'GER')) o.ger = t;
    if (!o.ita && atWar(g, 'USA', 'ITA')) o.ita = t;
    if (!o.cub && g.nations.CUB && atWar(g, 'CUB', 'GER')) o.cub = t;
    if (!o.mex && g.nations.MEX && atWar(g, 'MEX', 'GER')) o.mex = t;
    if (!o.bra && g.nations.BRA && atWar(g, 'BRA', 'GER')) o.bra = t;
    if (!o.full && U.atWar.length && warWorkLeft(g, 'USA') <= 0) o.full = t;
    if (!o.gerFell && !g.nations.GER.alive) o.gerFell = t;
    if (!o.itaFell && !g.nations.ITA.alive) o.itaFell = t;
    let west = false;
    for (const c of field(g)) { const r = g.regions[c.reg];
      if (beyondEurope(r) || r.overseas) continue;
      if (!o.across) o.across = t;
      if (!['UK', 'IRE', 'ICE'].includes(r.owner) && !island(g, r) && !o.first) { o.first = r.owner; o.firstT = t; }
      if (WEST.has(r.owner) && !island(g, r)) west = true; }
    if (west) { if (!o.west) o.west = t; } else o.west = 0;
    if (t === 1 || (t - 5) % 12 === 0) read(g, date(t));
  },
};
const run = campaign(me, 1, Number(seedS), plan, { until: Number(untilS) });
const g: any = run.g; read(g, `the end (${date(o.last)})`);
const d = (t: number, no = 'never'): string => (t ? date(t) : no);
let goal = ''; try { const s: any = goalState(g); goal = ` | goals: ${JSON.stringify(s).slice(0, 160)}`; } catch { /* no goals */ }
console.log(`END ${me} T${at} seed ${seedS}: declared on the United States ${d(o.declared)}; America among the Allies ${d(o.ally)}; at war with Germany ${d(o.ger, 'NEVER')}, with Italy ${d(o.ita, 'NEVER')}; December 1941 event ${g.flags['event:pearl-harbor-world'] ? date(g.flags['event:pearl-harbor-world']) : 'never fired'}; all on war work ${d(o.full)}; first corps across the ocean ${d(o.across)}; first ground on the continent ${o.first || 'none'} ${o.firstT ? date(o.firstT) : ''}; ashore in the west to stay ${d(o.west)}; Cuba at war with Germany ${d(o.cub)}, Mexico ${d(o.mex)}, Brazil ${d(o.bra)}; Germany ${o.gerFell ? `fell ${date(o.gerFell)}` : 'STANDS'}; Italy ${o.itaFell ? `fell ${date(o.itaFell)}` : 'stands'}; ${me} ${g.nations[me].alive ? 'stands' : 'FALLEN'} at ${date(o.last)}${goal}`);
