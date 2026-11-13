// Trace one province month by month in a campaign: who holds it, who stands on it, and the headlines that name it.
// usage: probe8 ME FROM SEED REGION UNTIL [PLAN-JSON]
import { campaign, date, atWar } from './lib';
const [me, fromS, seedS, name, untilS, planS] = process.argv.slice(2);
const plan = planS ? JSON.parse(planS) : { minOdds: 1.5 };
let last = '', h0 = 0;
campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g) => {
  const r = g.regions.find((x) => x.name === name)!;
  const nats = [...new Set(r.units.filter((u) => !u.dead).map((u) => u.nat))];
  const line = `held by ${r.ctrl} (owner ${(r as any).owner ?? (r as any).nat ?? '?'}); on it: ${r.units.filter((u) => !u.dead).map((u) => `${u.nat} ${u.name} [${u.kind}]`).join(', ') || 'nobody'}; ${nats.filter((n) => n !== r.ctrl).map((n) => `${n} at war with ${r.ctrl}: ${atWar(g, n, r.ctrl)}`).join('; ')}`;
  const hs = g.headlines.slice(h0).map((h: any) => h.text ?? String(h)).filter((t: string) => t.includes(name));
  h0 = g.headlines.length;
  if (line !== last || hs.length) { console.log(date(g.turn), line); for (const h of hs.slice(-8)) console.log('     news:', h); last = line; }
} }, { until: Number(untilS) });
