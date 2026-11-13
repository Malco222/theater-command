// Computer-only wars, stepped nation by nation: for every great power that gives in, how its capital and its home
// ground stood just before the move that ended it (Version 55 rule: no surrender with the capital free and under 60% of home lost).
// usage: probe15 SEEDS UNTIL
import { start, date, T, allied, collapseScore } from './lib';
import { besieged, ringClosed, guardOf, garrisonOf, townHeld } from '@tc/campaign/siege';
import { homeLost } from '@tc/campaign/game';
const seeds = (process.argv[2] ?? '1,2,3').split(',').map(Number);
const until = Number(process.argv[3] ?? 88);
const GREAT = ['GER', 'FRA', 'SOV', 'ITA', 'UK'];
let n = 0, free = 0;
for (const seed of seeds) {
  const g = start(null, 1, seed);
  let p0 = g.passings.length, guard = 0;
  while (g.turn <= until && guard++ < 6000) {
    const snap = GREAT.filter((k) => g.nations[k].alive).map((k) => {
      const seat = g.regions[g.nations[k].capital];
      return { k, seat: seat.name, own: allied(g, k, seat.ctrl), sieged: besieged(g, seat), ring: besieged(g, seat) && ringClosed(g, seat), since: g.flags[`siege:${seat.id}`], lost: homeLost(g, k), id: seat.id, guard: guardOf(g, seat), garr: garrisonOf(seat).length, held: townHeld(g, seat), field: seat.units.filter((u) => !u.dead && u.nat === k).length, army: g.corps.filter((c) => c.nat === k && !c.dead).length, score: collapseScore(g, k), res: g.nations[k].resilience };
    });
    const r0 = g.reports.length, turn = g.turn;
    const mover = T.stepAI(g);
    for (const p of g.passings.slice(p0) as any[]) {
      if (p.kind !== 'fell' || !GREAT.includes(p.nat)) continue;
      const s = snap.find((x) => x.k === p.nat);
      if (!s) continue;
      n++;
      const fought = (g.reports.slice(r0) as any[]).filter((r) => r.region === s.id).map((r) => `${r.attNat} ${r.town ? 'assault' : 'battle'}${r.taken ? ' took it' : r.siege ? ' closed round it' : ''}`);
      const wasFree = s.own && !s.sieged;
      if (wasFree && s.lost < 0.6) free++;
      console.log(`seed ${seed} ${date(turn)}: ${p.nat} fell to ${p.by} during ${mover}'s move; before it ${s.seat} was ${s.own ? 'its own' : 'lost'}${s.sieged ? (s.ring ? ', ringed' : ', besieged with the ring open') + ` for ${s.since === undefined ? '?' : turn - s.since} months` : ', free'}; guard ${s.guard.toFixed(1)} div, ${s.garr} corps inside, ${s.field} of its ${s.army} corps outside, town held ${s.held}; home lost ${(100 * s.lost).toFixed(0)}%, surrender score ${s.score.toFixed(2)} of ${s.res}; in that move at ${s.seat}: ${fought.join(', ') || 'no battle'}${wasFree && s.lost < 0.6 ? '   <== capital free and under 60% lost before the move' : ''}`);
    }
    p0 = g.passings.length;
  }
}
console.log(`${n} great-power falls; ${free} with the capital free and under 60% of home lost just before the move that ended it`);
