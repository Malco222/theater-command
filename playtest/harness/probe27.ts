// Version 57: the "bring the army up to strength" directive as the player reads it, against the army the player sees.
// usage: probe27 ME FROM SEED UNTIL PLAN-JSON     prints each raise directive with the game's count, the count of all
// divisions, those overseas, the price of an infantry corps and how many months of income it is.
import { campaign, date, T, ledger } from './lib';
import { corpsCost } from '@tc/campaign/economy';
import { armySize } from '@tc/campaign/game';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
let last = -1;
campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), each: (g: any) => {
  const d = T.governmentOf(g)?.directive;
  if (!d || d.kind !== 'raise' || d.id === last) return;
  last = d.id;
  const mine = g.corps.filter((c: any) => c.nat === me && !c.dead);
  const all = mine.reduce((s: number, c: any) => s + c.divs, 0), over = mine.filter((c: any) => g.regions[c.reg].overseas).reduce((s: number, c: any) => s + c.divs, 0);
  const L = ledger(g, me) as any, price = corpsCost(g, me, 'inf') * L.scale, net = L.net * L.scale;
  console.log(`${me} ${date(g.turn)}: "${d.text}" / "${d.progress}" | divisions on the map ${all} (${over} overseas), counted by the directive ${armySize(g, me).toFixed(1)}; an infantry corps (3 divisions) costs ${Math.round(price)}, income left a month ${Math.round(net)}, treasury ${Math.round(L.treasury * L.scale)}: ${(price / Math.max(1, net)).toFixed(1)} months of income for one corps; needs ${Math.ceil((d.target - armySize(g, me)) / 3)} corps`);
} }, { until: Number(untilS), quiet: true });
