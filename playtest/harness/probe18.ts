// A long siege of a port looked at closely: surrender score of the holder, ring, sea, the besieger's odds for an assault.
// usage: probe18 ME FROM SEED REGION FIRST UNTIL EVERY [PLAN-JSON]
import { campaign, date, atWar } from './lib';
import { garrisonOf, guardOf, besieged, ringClosed, fedBySea, stormForce, monthsShut, investedOn } from '@tc/campaign/siege';
import { collapseScore, homeLost, assess } from '@tc/campaign/game';
import { closedTo } from '@tc/campaign/navy';
const [me, fromS, seedS, name, firstS, untilS, everyS, planS] = process.argv.slice(2);
const plan = planS ? JSON.parse(planS) : { minOdds: 1.5 };
campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g) => {
  if (g.turn < Number(firstS)) return;
  if ((g.turn - Number(firstS)) % Number(everyS)) return;
  const r = g.regions.find((x) => x.name === name)!;
  if (!besieged(g, r)) { console.log(`${date(g.turn)}: ${name} held by ${r.ctrl}, not besieged`); return; }
  const holder = r.ctrl, n = g.nations[holder];
  const out = r.units.filter((u) => !u.dead && atWar(g, u.nat, holder));
  const foe = out[0]?.nat ?? '-';
  const pool = stormForce(g, out.filter((c) => c.org >= 0.35 && c.kind !== 'fort'), r);
  const a = pool.length ? assess(g, pool, r, { town: true }) : null;
  const seas = r.seas.map((z) => `${g.seas[z]?.name ?? z}:${closedTo(g, z, holder) ? 'closed' : 'open'}`).join(' ');
  const ships = (nat: string) => (g as any).fleets ? (g as any).fleets.filter((f: any) => f.nat === nat && !f.dead).length : '?';
  console.log(`${date(g.turn)}: ${name} (${holder}) besieged by ${foe} since ${date(investedOn(g, r))}, ${out.length} corps outside, ${garrisonOf(r).length} inside + guard ${guardOf(g, r).toFixed(2)}; ring closed ${ringClosed(g, r)}, fed by sea ${fedBySea(g, r)}, months shut ${monthsShut(g, r)}; seas ${seas}; assault odds ${a ? a.odds.toFixed(2) : '-'} with ${pool.length} corps; ${holder} score ${collapseScore(g, holder).toFixed(2)} of ${n.resilience}, home lost ${(100 * homeLost(g, holder)).toFixed(0)}%, alive ${n.alive}; fleets ${foe} ${ships(foe)}, ${holder} ${ships(holder)}`);
} }, { until: Number(untilS) });
