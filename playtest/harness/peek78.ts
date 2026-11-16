// When is war on WHOM offered to a player in the ME seat? usage: peek78 ME SEED UNTIL WHOM PLAN-JSON
import { campaign, date, T, atWar } from './lib';
const [me, seedS = '25', untilS = '3', whom = 'USA', planS = '{"idle":true}'] = process.argv.slice(2); const X = T as any;
let was = false, first = 0, n = 0, last = 0, war = 0;
campaign(me, 1, Number(seedS), { ...JSON.parse(planS), first: (g: any) => { last = g.turn; const on = X.declarable(g).includes(whom);
  if (on) { n++; if (!first) first = g.turn; } if (!war && atWar(g, me, whom)) war = g.turn;
  if (on !== was) console.log(`${me} seed ${seedS} ${date(g.turn)}: war on ${whom} ${on ? 'is offered' : 'is no longer offered'}${atWar(g, me, whom) ? ' (already at war)' : ''}`); was = on; } } as any, { until: Number(untilS) });
console.log(`OFFER ${me} seed ${seedS}: war on ${whom} first offered ${first ? date(first) : 'never'}, in ${n} months, played to ${date(last)}; at war with ${whom} from ${war ? date(war) : 'never'}`);
