// Sniper teams (release 2; Brian, 25 November 2026, 16:07Z: "should we add snipers? how would that impact the
// battle?", then "yes"): the field's sniper teams, carved out of each side's own foot (LAND.snipers). Kept apart from
// sample.ts so the 3D game's bundle (live.ts) needs nothing of the campaign's.

import type { FieldTicket, ForceTicket, SideTicket } from '../core/types';
import type { LandPlace } from './field';
import { LAND, sniperShare } from './rules';
import { partsFor } from './units';

/**
 * A ticket with sniper teams (release 2; LAND.snipers, the switch 'snipers'; the design director's ruling of 25 November
 * 16:30Z, Q1 and Q2): each side's foot (the card's own arms on foot, LAND.snipers.foot) gives its army's share of its power
 * (sniperShare: by doctrine, never the year), pro rata, to one team for every LAND.snipers.per of its foot blocks, at
 * least one and at most LAND.snipers.most, so each side's power sums to the card's and the card's odds stand (b418).
 * Never from tanks, guns, engineers, fortress troops, teams, or a corps still to come up (joiners are left as they are).
 * Each team carries the corps of the foot that gave most, so its losses are billed to that corps; its men are picked
 * (its foot's power-weighted experience plus LAND.snipers.xp). A side with no foot gets none.
 */
export function withSnipers(t: FieldTicket<LandPlace>): FieldTicket<LandPlace> {
  const S = LAND.snipers;
  const add = (s: SideTicket): SideTicket => {
    const foot = s.forces.filter((f) => S.foot.includes(f.kind) && f.share > 0);
    if (!foot.length) return s;
    const give = foot.map((f) => f.share * sniperShare(f.nat)), total = give.reduce((a, b) => a + b, 0);
    if (total <= 0) return s;
    // Its foot blocks, as the field will cut them (units.ts partsFor).
    const parts = partsFor(s.forces), blocks = s.forces.reduce((a, f, i) => a + (foot.includes(f) ? parts[i].length : 0), 0);
    const n = Math.max(1, Math.min(S.most, Math.ceil(blocks / S.per)));
    // The corps that gives most (the first of equals): its corps, nation and men's name the teams.
    let top = 0;
    for (let i = 1; i < foot.length; i++) if (give[i] > give[top]) top = i;
    const from = foot[top], weight = foot.reduce((a, f) => a + f.share, 0);
    const xp = Math.min(S.xpTop, foot.reduce((a, f) => a + f.xp * f.share, 0) / weight + S.xp);
    const men = foot.reduce((a, f) => a + f.men, 0);
    const forces = s.forces.map((f): ForceTicket => { const i = foot.indexOf(f); return i < 0 ? f : { ...f, share: f.share - give[i] }; });
    for (let k = 0; k < n; k++) {
      forces.push({
        corps: from.corps, name: 'Sniper Detachment', nat: from.nat, kind: 'snp', divs: 1, men: Math.max(8, Math.round((men * sniperShare(from.nat)) / 4 / n)),
        str: from.str, org: from.org, xp: Math.round(xp * 1000) / 1000, mor: from.mor, share: total / n, first: k + 1,
      });
    }
    return { ...s, forces };
  };
  return { ...t, att: add(t.att), def: add(t.def) };
}
