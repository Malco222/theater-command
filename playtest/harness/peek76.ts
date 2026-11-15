// Version 76: what a Latin player's desks offer in a given month. usage: peek76 ME SEED MONTH [MODE]
import { campaign, date, T } from './lib';
const [me, seedS, mS = '5', mode = 'look'] = process.argv.slice(2);
const X = T as any;
const plan: any = { idle: true, each: (g: any) => {
  if (g.turn !== Number(mS)) return;
  const n = g.nations[me];
  console.log(`== ${me} ${date(g.turn)} treasury ${n.pp.toFixed(1)} capital ${n.pc}`);
  for (const o of X.arsenalOffers(g, n.capital)) console.log(`ARS ${o.key} cost ${o.cost ?? o.price ?? '?'} ${o.why === null ? 'CAN' : 'no: ' + o.why}`);
  const book = X.tradeLedger(g);
  console.log(`TRADE used ${book.used} houses ${book.houses} most ${book.most} income ${book.income}`);
  for (const d of book.deals) console.log(`DEAL ${JSON.stringify(d).slice(0, 300)}`);
  for (const o of book.offers.slice(0, 12)) console.log(`OFFER ${o.nat} pays ${o.pays} ${o.why === null ? 'CAN' : 'no: ' + o.why}`);
  for (const r of g.regions) if (r.ctrl === me) {
    const tb = X.townBuildings?.(g, r.id);
    if (tb) console.log(`TOWN ${r.name}: ${JSON.stringify(tb).slice(0, 900)}`);
  }
  console.log(`CORPS ${g.corps.filter((c: any) => c.nat === me && !c.dead).map((c: any) => `${c.name}/${c.kind}/${c.divs}d/${c.str.toFixed(2)}@${g.regions[c.reg].name}`).join('; ')}`);
  console.log(`FLEETS ${JSON.stringify((X.myFleetCards?.(g) ?? []).map((f: any) => f.name ?? f.id)).slice(0, 300)}`);
  const tree = X.techTree(g); console.log(`TECH teams ${tree?.teams} busy ${tree?.busy} ready ${tree?.nodes.filter((x: any) => x.state === 'ready' && x.why === null).length}`);
  console.log(`CALLUP ${JSON.stringify(X.callUpOf?.(g) ?? null).slice(0, 400)}`);
} };
campaign(me, 1, Number(seedS), plan, { until: Number(mS) });
