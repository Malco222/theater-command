// Version 74: what another nation does while the player plays. Each month after the player's orders: who the watched
// nation is at war with, which side it stands on, its army, and a line whenever any of that changes.
// usage: probe67 ME FROM SEED UNTIL PLAN-JSON WATCH     (build with build70.sh)
import { campaign, date, bloc } from './lib';
const [me, fromS, seedS, untilS, planS = '{}', watch = 'USA'] = process.argv.slice(2);
let last = '';
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  each: (g: any) => {
    const n = g.nations[watch]; if (!n) return;
    const corps = g.corps.filter((c: any) => c.nat === watch);
    const line = `at war with ${n.atWar.join(',') || 'nobody'}; side ${bloc(n) ?? 'none'}; ${corps.length} corps`;
    const mine = g.nations[me];
    if (line !== last) console.log(`${date(g.turn)}: ${n.name} ${line}${n.alive ? '' : ' (fallen)'} | the player: at war with ${mine.atWar.join(',') || 'nobody'}, side ${bloc(mine) ?? 'none'}`);
    last = line;
  },
} as any, { until: Number(untilS) });
const g: any = run.g; const n = g.nations[watch];
console.log(`WATCHED ${watch} from the ${me} seat, seed ${seedS}: played to ${date(g.turn)}; ${n.name} at war with ${n.atWar.join(',') || 'nobody'}, side ${bloc(n) ?? 'none'}; great powers at war with it: ${['GER', 'ITA', 'UK', 'FRA', 'SOV'].filter((p) => n.atWar.includes(p)).join(',') || 'none'}`);
