// Version 75: why the computer's Americans wait in Britain beside a British player. Month by month from FROMT: where
// the American corps stand, the second front their staff sees (staff.ts frontAshore: its anchor and harbours), the
// player's own corps in the west and the ports they hold there, and for an American corps on a British quay whether a
// way by sea to each harbour of that front exists and how dangerous the staff holds it (world.json overseas.USA.risk).
// It reads only: frontAshore, lodgment, seaPath and crossingRisk set no kept flags.
// usage: probe71 ME FROM SEED UNTIL PLAN-JSON [FROMT=52] [NAT=USA]     (build with build70.sh)
import { campaign, date } from './lib';
import { frontAshore, lodgment } from '@tc/campaign/staff';
import { seaPath, crossingRisk, liftLeft } from '@tc/campaign/navy';
import { overseasOf } from '@tc/campaign/world';
import { friendlyUnits, allied } from '@tc/campaign/game';
const [me, fromS, seedS, untilS, planS = '{}', fromT = '52', nat = 'USA'] = process.argv.slice(2);
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  each: (g: any) => {
    const t = g.turn; if (t < Number(fromT)) return;
    const N = g.nations[nat]; if (!N?.alive || !g.nations.GER?.alive) return;
    const O: any = overseasOf(g, nat);
    const mine = g.corps.filter((c: any) => c.nat === nat && !c.dead && c.kind !== 'fort');
    const inBritain = mine.filter((c: any) => g.regions[c.reg].owner === 'UK' && !g.regions[c.reg].overseas);
    const west = mine.filter((c: any) => WEST.has(g.regions[c.reg].owner) && !g.regions[c.reg].overseas);
    const my = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && WEST.has(g.regions[c.reg].owner) && !g.regions[c.reg].overseas);
    const myPorts = g.regions.filter((r: any) => WEST.has(r.owner) && !r.overseas && r.port && r.ctrl === me).map((r: any) => r.name);
    const alliedPorts = g.regions.filter((r: any) => WEST.has(r.owner) && !r.overseas && r.port && allied(g, nat, r.ctrl)).map((r: any) => `${r.name}:${r.ctrl}`);
    const lod = [0, 1].map((s) => lodgment(g, me, s)).map((l: any) => (l ? `${l.at.name}${l.short ? ' (short)' : ''} ports ${l.ports.map((p: any) => p.name).join('/') || 'none'}` : 'none'));
    const lodFlag = [`st:lodged:${me}`].map((k) => (g.flags[k] ? g.regions[g.flags[k] - 1].name : 'unset'));
    const f: any = frontAshore(g, nat);
    let ways = '';
    if (f) {
      const quay = inBritain.find((c: any) => g.regions[c.reg].port && c.supplied && c.str >= 0.7 && c.ap >= c.apMax) ?? inBritain.find((c: any) => g.regions[c.reg].port);
      if (quay) ways = f.ports.map((p: any) => { const path = seaPath(g, nat, g.regions[quay.reg], p); return `${p.name}: ${path ? `risk ${crossingRisk(g, nat, path).toFixed(2)}` : 'no way by sea'}, ${friendlyUnits(g, p, nat).length} corps in it`; }).join('; ');
      else ways = 'no American corps on a British quay';
    }
    const fit = inBritain.filter((c: any) => c.supplied && c.str >= 0.7 && c.ap >= c.apMax && c.kind !== 'eng' && !c.airborne && !c.town && c.joint === undefined);
    const onQuay = fit.filter((c: any) => g.regions[c.reg].port);
    console.log(`WAIT ${nat} beside ${me} seed ${seedS} ${date(t)}: Americans in Britain ${inBritain.length} (fit to sail ${fit.length}, of them on a quay ${onQuay.length}, held by their staff ${inBritain.filter((c: any) => c.hold).length}), in the west ${west.length}; lift left ${liftLeft(g, nat)}; the staff's risk limit ${O?.risk}; the player's corps in the west ${my.length}, the ports there in the player's hands: ${myPorts.join(', ') || 'none'}; in the Allies' hands: ${alliedPorts.join(', ') || 'none'}; the player's recorded landing: ${lodFlag.join(',')}; as a front: ${lod.join(' | ')}; the front the American staff sees: ${f ? `${f.at.name}, harbours ${f.ports.map((p: any) => p.name).join('/')}` : 'NONE'}${ways ? `; from a British quay: ${ways}` : ''}`);
  },
}, { until: Number(untilS) });
if (run.errors.length) console.log('ERROR ' + run.errors.join(' / '));
