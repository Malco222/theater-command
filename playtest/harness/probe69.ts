// Version 75: what the computer's America does with its army beside a player (probe68 is the same reading for a war
// with nobody playing). One line a war: when its corps cross, how many wait in Britain, its landings on the
// continent, whether it would beat the American seat's own par, and where its corps stood the month Germany fell;
// and beside it when the player's own corps were ashore in the west.
// usage: probe69 ME FROM SEED UNTIL PLAN-JSON [NAT=USA]     (build with build70.sh)
import { campaign, date } from './lib';
const [me, fromS, seedS, untilS, planS = '{}', nat = 'USA'] = process.argv.slice(2);
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
let war = 0, firstOver = 0, firstCont = 0, firstContWhere = '', westFrom = 0, westFirst = 0, gerFell = 0, most = 0, mostAt = 0, britainMost = 0, britainMonths = 0, britainAt58 = -1, westAt58 = -1;
let myWestFrom = 0, myWestFirst = 0, standAtFall = '', lastTurn = 0, landings = 0, prevCont = new Set<number>();
const seen = new Set<number>();
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  each: (g: any) => {
    lastTurn = g.turn;
    const N = g.nations[nat]; if (!N?.alive) return;
    const t = g.turn;
    if (!war && N.atWar.length) war = t;
    const mine = g.corps.filter((c: any) => c.nat === nat && !c.dead && c.kind !== 'fort');
    for (const c of mine) seen.add(c.id);
    const over = mine.filter((c: any) => g.regions[c.reg].owner !== nat && !['CAN', 'MEX', 'CUB'].includes(g.regions[c.reg].owner));
    if (over.length && !firstOver) firstOver = t;
    if (over.length > most) { most = over.length; mostAt = t; }
    const inBritain = over.filter((c: any) => g.regions[c.reg].owner === 'UK' && !g.regions[c.reg].overseas).length;
    if (inBritain > britainMost) britainMost = inBritain;
    if (inBritain) britainMonths++;
    const cont = over.filter((c: any) => { const r = g.regions[c.reg]; return !r.overseas && !['UK', 'IRL'].includes(r.owner) && r.nb.length > 0; });
    if (cont.length && !firstCont) { firstCont = t; firstContWhere = `${g.regions[cont[0].reg].name} (${g.regions[cont[0].reg].owner})`; }
    const west = over.filter((c: any) => WEST.has(g.regions[c.reg].owner) && !g.regions[c.reg].overseas && g.regions[c.reg].name !== 'Corsica').length;
    if (t === 58) { britainAt58 = inBritain; westAt58 = west; }
    if (!gerFell) {
      if (west) { if (!westFrom) westFrom = t; if (!westFirst) westFirst = t; } else westFrom = 0;
      const myWest = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && g.regions[c.reg].owner !== me && WEST.has(g.regions[c.reg].owner) && !g.regions[c.reg].overseas && g.regions[c.reg].name !== 'Corsica').length;
      if (myWest) { if (!myWestFrom) myWestFrom = t; if (!myWestFirst) myWestFirst = t; } else myWestFrom = 0;
    }
    if (!gerFell && g.nations.GER && !g.nations.GER.alive) {
      gerFell = t;
      const st: Record<string, number> = {}; for (const c of mine) { const k = g.regions[c.reg].owner === nat ? 'home' : g.regions[c.reg].owner; st[k] = (st[k] ?? 0) + 1; }
      standAtFall = Object.entries(st).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k} ${v}`).join(', ');
    }
  },
} as any, { until: Number(untilS), quiet: true } as any);
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 400));
const par = !!westFrom && westFrom < 58 && !!gerFell && gerFell < 69;
console.log(`ARMY ${nat} beside ${me} seed ${seedS}: at war ${war ? date(war) : 'never'} | first across the ocean ${firstOver ? date(firstOver) : 'never'} | most across ${most} corps in ${mostAt ? date(mostAt) : '-'} | in Britain at the most ${britainMost} corps, some there in ${britainMonths} months; as June 1944 opens ${britainAt58} in Britain and ${westAt58} in the west | first on the continent ${firstCont ? `${date(firstCont)} at ${firstContWhere}` : 'never'} | ashore in the west to stay ${westFrom ? `from ${date(westFrom)}` : 'never'}${westFirst && westFirst !== westFrom ? ` (first ${date(westFirst)})` : ''} | Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'} | the American seat's par ${par ? 'BEATEN' : 'missed'} | corps raised ${seen.size} | when Germany fell its corps stood: ${standAtFall || '-'} | the player's own corps ashore in the west to stay ${myWestFrom ? `from ${date(myWestFrom)}` : 'never'}${myWestFirst && myWestFirst !== myWestFrom ? ` (first ${date(myWestFirst)})` : ''} | played to ${date(lastTurn)}`);
