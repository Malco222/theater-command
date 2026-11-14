// Version 62: what the cabinet remembers of a town's odds. usage: probe40 ME FROM SEED AT-TURN PLAN-JSON TOWN
// At the start of the player's month AT-TURN it prints, for TOWN, the reading the cabinet noted as each of the last
// months opened (nation.gov.reach) beside the reading on offer today (promiseTargetsOf).
import { campaign, date, T } from './lib';
const [me, fromS, seedS, atS, planS, town] = process.argv.slice(2);
const at = Number(atS), X = T as any;
campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), first: (g: any) => {
  if (g.turn < at - 3 || g.turn > at) return;
  const r = g.regions.find((x: any) => x.name === town || X.townNameOf?.(g, x.id) === town);
  const offer = (X.promiseTargetsOf(g, 40) as any[]).find((t) => t.town === town || t.region === r?.id);
  const mem = (g.nations[me].gov?.reach ?? []).map((m: any) => `${date(m.t)}: ${m.odds[offer?.region ?? r?.id] ?? '-'} (depth ${m.depth?.[offer?.region ?? r?.id] ?? '-'})`).join(' | ');
  console.log(`${date(g.turn)} ${town}: on offer today ${offer ? `${offer.terms?.[0]?.odds} (${offer.terms?.map((t: any) => `${t.months}m ${t.band}`).join(', ')})` : 'not on offer'} | the cabinet's notes: ${mem}`);
} } as any, { until: at, quiet: true });
