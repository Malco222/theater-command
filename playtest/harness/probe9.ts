// What brings the Soviet Union down, month by month: capital, ground, army, privation. usage: probe9 ME SEED LEVEL UNTIL [PLAN-JSON]
import { campaign, date, allied, armySize, collapseScore } from './lib';
import { willLost } from '@tc/campaign/resources';
const [me, seedS, level, untilS, planS] = process.argv.slice(2);
const plan = planS ? JSON.parse(planS) : { minOdds: 1.5, land: true };
const who = process.env.WHO ?? 'SOV';
campaign(me, 1, Number(seedS), { ...plan, each: (g) => {
  if (g.turn < 20 || (g.turn % 2)) return;
  const n = g.nations[who]; if (!n.alive) return;
  let vp = 0, held = 0; for (const r of g.regions) if (r.owner === who && !r.overseas) { vp += r.vp; if (allied(g, who, r.ctrl)) held += r.vp; }
  const cap = !allied(g, who, g.regions[n.capital].ctrl) ? 1 : 0;
  const army = armySize(g, who) / n.armyPeak;
  console.log(`${date(g.turn)}: score ${collapseScore(g, who).toFixed(2)} of ${n.resilience}; capital lost ${cap} (0.45), ground lost ${(1 - held / vp).toFixed(2)} (x0.35 = ${(0.35 * (1 - held / vp)).toFixed(2)}), army ${Math.round(armySize(g, who))} of peak ${Math.round(n.armyPeak)} (x0.2 = ${(0.2 * (1 - Math.min(1, army))).toFixed(2)}), privation ${willLost(g, who).toFixed(2)} hunger ${(n as any).res?.hunger ?? '-'} oil ${(n as any).res?.oilMet?.toFixed?.(2)} sup ${(n as any).res?.supMet?.toFixed?.(2)}`);
} }, { until: Number(untilS), level });
