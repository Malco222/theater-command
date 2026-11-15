// Version 75: what the computer's America does with its army in a war with nobody playing. One line a war: when it
// is at war, when its corps first cross the ocean and where they stand, its landings on the continent, whether it
// would beat the American seat's own par (ruling 29: corps ashore in the west to stay before June 1944 and Germany
// beaten before May 1945), what it lost, and who holds the capitals of the west the month Germany falls.
// node out75/probe68.mjs SEEDS(a,b,c) [UNTIL=80] [NAT=USA]     (build with build70.sh)
import { start, date, T } from './lib';
const [seedsS = '17', untilS = '80', nat = 'USA'] = process.argv.slice(2);
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
const CAPS = ['FRA', 'BEL', 'NED', 'ITA', 'GER', 'DEN', 'NOR', 'GRE', 'POL'];
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', 3);
  const capOf: Record<string, number> = {}; for (const n of CAPS) capOf[n] = g.nations[n]?.capital ?? -1;
  let war = 0, firstOver = 0, firstCont = 0, firstContWhere = '', westFrom = 0, westFirst = 0, gerFell = 0, most = 0, mostAt = 0, raised = 0, err = '';
  let britainMost = 0, britainMonths = 0, idleBritain = 0;
  const seenIds = new Set<number>(); const places: Record<string, number> = {}; let capsAtFall = '', standAtFall = '';
  const own = (r: any): string => (r.owner === nat ? 'home' : r.owner);
  try {
    while (g.turn <= Number(untilS)) {
      const t = g.turn;
      T.runTurnAI(g);
      const N = g.nations[nat]; if (!N?.alive) break;
      if (!war && N.atWar.length) war = t;
      const mine = g.corps.filter((c: any) => c.nat === nat && !c.dead && c.kind !== 'fort');
      for (const c of mine) seenIds.add(c.id);
      raised = seenIds.size;
      const over = mine.filter((c: any) => g.regions[c.reg].owner !== nat && !['CAN', 'MEX', 'CUB'].includes(g.regions[c.reg].owner));
      if (over.length && !firstOver) firstOver = t;
      if (over.length > most) { most = over.length; mostAt = t; }
      const inBritain = over.filter((c: any) => g.regions[c.reg].owner === 'UK' && !g.regions[c.reg].overseas).length;
      if (inBritain > britainMost) britainMost = inBritain;
      if (inBritain) britainMonths++;
      // The continent: ground first owned by a European country other than Britain and Ireland, not an island colony.
      const cont = over.filter((c: any) => { const r = g.regions[c.reg]; return !r.overseas && !['UK', 'IRL'].includes(r.owner) && r.nb.length > 0; });
      if (cont.length && !firstCont) { firstCont = t; firstContWhere = `${g.regions[cont[0].reg].name} (${g.regions[cont[0].reg].owner})`; }
      if (!gerFell) {
        const west = over.filter((c: any) => WEST.has(g.regions[c.reg].owner) && !g.regions[c.reg].overseas && g.regions[c.reg].name !== 'Corsica').length;
        if (west) { if (!westFrom) westFrom = t; if (!westFirst) westFirst = t; } else westFrom = 0;
        for (const c of over) { const k = own(g.regions[c.reg]) + (g.regions[c.reg].overseas ? ' overseas' : ''); places[k] = Math.max(places[k] ?? 0, over.filter((x: any) => own(g.regions[x.reg]) + (g.regions[x.reg].overseas ? ' overseas' : '') === k).length); }
      }
      if (!gerFell && g.nations.GER && !g.nations.GER.alive) {
        gerFell = t;
        capsAtFall = CAPS.map((n) => (capOf[n] >= 0 ? `${g.regions[capOf[n]].name} ${g.regions[capOf[n]].ctrl}` : `${n} ?`)).join(', ') + `, Vienna ${g.byName.Vienna?.ctrl ?? '?'}`;
        const st: Record<string, number> = {}; for (const c of mine) { const k = own(g.regions[c.reg]); st[k] = (st[k] ?? 0) + 1; }
        standAtFall = Object.entries(st).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k} ${v}`).join(', ');
        break;
      }
    }
  } catch (e: any) { err = ` | ERROR in ${date(g.turn)}: ${String(e?.stack ?? e).split('\n').slice(0, 3).join(' / ')}`; }
  const alive = g.corps.filter((c: any) => c.nat === nat && !c.dead && c.kind !== 'fort').length;
  const par = !!westFrom && westFrom < 58 && !!gerFell && gerFell < 69;
  console.log(`ARMY ${nat} seed ${seed}: at war ${war ? date(war) : 'never'} | first across the ocean ${firstOver ? date(firstOver) : 'never'} | most across ${most} corps in ${mostAt ? date(mostAt) : '-'} | in Britain at the most ${britainMost} corps, some there in ${britainMonths} months | first on the continent ${firstCont ? `${date(firstCont)} at ${firstContWhere}` : 'never'} | ashore in the west to stay ${westFrom ? `from ${date(westFrom)}` : 'never'}${westFirst && westFirst !== westFrom ? ` (first ${date(westFirst)})` : ''} | Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'} | the seat's own par ${par ? 'BEATEN' : 'missed'} | corps raised ${raised}, alive at the end ${alive} | the most that stood on ground first owned by: ${Object.entries(places).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k} ${v}`).join(', ') || 'nowhere'} | when Germany fell its corps stood: ${standAtFall || '-'} | capitals then: ${capsAtFall || '-'}${err}`);
}
