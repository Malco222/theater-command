// Version 78, ruling 80(c): with Germany at war with the United States from September 1939 and nobody playing, how
// many of the American corps that cross the ocean by June 1940 are dead by the end of 1940, and where they went.
// usage: probe81 SEEDS(a,b,c) [BY=10 (June 1940)] [END=17 (January 1941 opens)]     One EARLYCORPS line a war.
import { start, date, T } from './lib';
import { declareWar } from '@tc/campaign/game';
import { beyondEurope } from '@tc/campaign/world';
const [seedsS = '17', byS = '10', endS = '17'] = process.argv.slice(2);
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', 3);
  const crossed = new Map<number, { t: number; where: Set<string> }>();
  declareWar(g, 'GER', 'USA');
  while (g.turn < Number(endS)) {
    for (const c of g.corps) { if (c.nat !== 'USA' || c.dead || c.kind === 'fort' || c.kind === 'eng') continue;
      const r = g.regions[c.reg]; if (beyondEurope(r) || r.overseas) continue;
      if (!crossed.has(c.id)) { if (g.turn > Number(byS)) continue; crossed.set(c.id, { t: g.turn, where: new Set() }); }
      crossed.get(c.id)!.where.add(r.owner); }
    (T as any).runTurnAI(g);
  }
  const alive = new Set(g.corps.filter((c: any) => c.nat === 'USA' && !c.dead).map((c: any) => c.id));
  const all = [...crossed.entries()]; const dead = all.filter(([id]) => !alive.has(id));
  const went = (xs: [number, any][]): string => { const n: Record<string, number> = {}; for (const [, v] of xs) for (const o of v.where) if (o !== 'UK') n[o] = (n[o] ?? 0) + 1; return Object.entries(n).map(([k, v]) => `${k} ${v}`).join(', ') || 'Britain only'; };
  const raised = g.corps.filter((c: any) => c.nat === 'USA' && c.kind !== 'fort' && c.kind !== 'eng').length;
  console.log(`EARLYCORPS seed ${seed}: American corps across the ocean by ${date(Number(byS))}: ${all.length}; dead as ${date(g.turn)} opens: ${dead.length}; ground the crossers stood on: ${went(all)}; ground the dead stood on: ${went(dead)}; France ${g.nations.FRA.alive ? 'stands' : 'fallen'}; American field corps in all ${raised}`);
}
