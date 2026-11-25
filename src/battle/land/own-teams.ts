// Every infantry force with its own machine gun and mortar teams, in a campaign battle (release 2, switched, off).
//
// Brian, 25 November 2026, 14:14Z and 14:15Z: "Where are the machine gun teams and mortar teams in the infantry
// squad"; "There should be machine gun teams that lay down suppressing fire, such as the German MG 42 and the US machine
// gun". Asked how the teams should reach his battles, he chose both (14:42Z): every infantry force brings its own, and
// teams are also units he builds in the campaign (src/campaign/teams.ts, whose brigades come on the ticket as forces of
// kind 'mg' and 'mor' and are priced on the attack card).
//
// The set pieces have always had a machine-gun and a mortar battalion a side (pieces.ts withTeams). A campaign battle
// had none unless the campaign had built some. With this on, each force of infantry, mountain troops or lorried
// infantry on a campaign ticket gives up a small share of itself (LAND-side numbers below) to its machine guns and its
// mortars, each a team block of its own: the same men and the same weight, drawn and fought as the teams they always had.
//
//   - Nothing is added to a side. The parent force's share of its side's power, its men and its divisions' men each
//     go down by what its two team blocks take, so the side's shares still add up to what they did, and so do its men
//     ("on-screen numbers reflect actual units brought", Brian's standing rule of 22 November).
//   - Each team block is its parent's own: its nation, strength, cohesion, experience and morale, dug in where it is,
//     over the river or not as it is, worn as it is.
//   - Their ids are their own, far below any the campaign uses (a corps' id is positive; a town's guard is -1 - its
//     region, siege.ts guardForce): the result names them among its forces, and the campaign's bill, which looks its
//     own corps up by id (game.ts splitWeights), passes over them. What they lost is in their side's share lost, which
//     the bill does read.
//   - The attack card's fog: a team block is known to the other side where its parent corps is (ownTeams' seen).
//
// It changes what is on the field, so it changes how a battle goes: off until the design director's ruling and the
// fairness tables pass it. Off, the ticket is the one the campaign made, untouched (the same object).
//
// A first reading, 25 November (the two set pieces' tickets without their own battalions, the computer on both sides,
// seeds 1 to 200, each fought with none, with the set piece's battalions and with these): teams tilt a battle toward
// the attack. Mean R, none / the set piece's / these: Normandy 1.247 / 1.273 / 1.277, Prokhorovka 1.325 / 1.373 /
// 1.374; the attack won 86 / 100 / 96 and 101 / 119 / 121 of 200. So these do what the set pieces' own always did, no
// more; and at Normandy 30 of 200 ran to the time limit (1 with none, 11 with the set piece's), battles a sixth
// longer. A team block's dug, strength, experience and morale made no difference to any of it. The tilt is LAND.teams'
// own (rules.ts), which is the design director's to rule on.

import type { FieldTicket, ForceTicket, SideTicket } from '../core/types';
import type { LandPlace } from './field';
import type { Team } from './rules';

/** The share of its parent force each team block takes: of its power, and of its men (pieces.ts withTeams takes the same of a side). */
export const OWN_TEAM_SHARE = 0.04;
/** The kinds of force that bring their own teams: men who fight on foot in companies with a weapons platoon behind them. */
export const OWN_TEAM_KINDS: readonly string[] = ['inf', 'mtn', 'mot'];
/** The first of the team blocks' ids, and downward from it: two a force, the attack's then the defence's. */
export const OWN_TEAM_ID = -1_000_001;

/** Their words on a block's card: "VII Corps machine guns" (all of a corps' machine-gun companies together, more men than one team block). */
const WORD: Record<Team, [string, string]> = { mg: ['machine guns', 'MGs'], mor: ['mortars', 'mortars'] };

/** Whether a force is one of these team blocks (by its id). */
export const isOwnTeam = (f: { corps: number }): boolean => f.corps <= OWN_TEAM_ID;

/** Whether a force brings its own teams: a campaign's own corps (never a town's guard, whose id is below zero) of a kind that has them. */
const brings = (f: ForceTicket): boolean => f.corps >= 0 && OWN_TEAM_KINDS.includes(f.kind) && f.share > 0 && f.men > 0;

/**
 * One side with each such force's two team blocks after its forces, in the forces' order (machine guns, then mortars).
 * next: the id the first of them takes; each after it one lower. parents: each team block's id to its parent's.
 */
function sideWith(s: SideTicket, next: number, parents: Map<number, number>): SideTicket {
  const made: ForceTicket[] = [];
  const forces = s.forces.map((f): ForceTicket => {
    if (!brings(f)) return f;
    const keep = 1 - 2 * OWN_TEAM_SHARE;
    for (const kind of ['mg', 'mor'] as const) {
      const id = next - made.length, men = Math.max(1, Math.round(f.men * OWN_TEAM_SHARE));
      const name = `${f.name} ${WORD[kind][0]}`, short = `${f.name} ${WORD[kind][1]}`;
      parents.set(id, f.corps);
      made.push({
        corps: id, name: f.name, nat: f.nat, kind, divs: 1, men, str: f.str, org: f.org, xp: f.xp,
        ...(f.mor !== undefined ? { mor: f.mor } : {}), ...(f.dug ? { dug: true } : {}), ...(f.cross === false ? { cross: false } : {}),
        ...(f.wear !== undefined ? { wear: f.wear } : {}), ...(f.fresh !== undefined ? { fresh: f.fresh } : {}),
        divisions: [{ name, short, str: f.str, xp: f.xp, men }],
        share: f.share * OWN_TEAM_SHARE, battles: f.battles ?? 0,
      });
    }
    return {
      ...f, share: f.share * keep, men: Math.round(f.men * keep),
      ...(f.divisions ? { divisions: f.divisions.map((d) => ({ ...d, men: Math.round(d.men * keep) })) } : {}),
    };
  });
  return made.length ? { ...s, forces: [...forces, ...made] } : s;
}

/**
 * The ticket with every infantry force's own machine-gun and mortar team blocks on it, and the corps the other side's
 * card knows with the team blocks of those it knows added. A ticket that has them already (asked twice), or no force
 * that brings any, comes back as it is.
 */
export function ownTeams(t: FieldTicket<LandPlace>, seen: readonly number[] = []): { ticket: FieldTicket<LandPlace>; seen: number[] } {
  if ([...t.att.forces, ...t.def.forces].some(isOwnTeam)) return { ticket: t, seen: [...seen] };
  const parents = new Map<number, number>();
  const att = sideWith(t.att, OWN_TEAM_ID, parents);
  const def = sideWith(t.def, OWN_TEAM_ID - parents.size, parents);
  if (!parents.size) return { ticket: t, seen: [...seen] };
  const known = new Set(seen);
  return { ticket: { ...t, att, def }, seen: [...seen, ...[...parents].filter(([, parent]) => known.has(parent)).map(([id]) => id)] };
}

/**
 * Each own team block's parent on a ticket that has them: the team force's id to the id of the force it was taken
 * from, read back from the order ownTeams lays them in (a side's team blocks after its forces, two a parent, the
 * parents in the forces' order). Empty on a ticket with none; a side whose team blocks are not laid so gives none
 * (no parent is guessed).
 */
export function ownTeamParents(t: { att: SideTicket; def: SideTicket }): Map<number, number> {
  const out = new Map<number, number>();
  for (const s of [t.att, t.def]) {
    const teams = s.forces.filter(isOwnTeam), parents = s.forces.filter((f) => !isOwnTeam(f) && brings(f));
    if (!teams.length || teams.length !== 2 * parents.length) continue;
    teams.forEach((f, i) => out.set(f.corps, parents[i >> 1].corps));
  }
  return out;
}

/**
 * The same for what the 3D game hands the live battle: the campaign's battleLook answer ({ ticket, live: { seen } })
 * or a bare ticket, in the shape it came.
 */
export function ownTeamsLook<L extends { ticket: FieldTicket<LandPlace>; live?: { seen?: number[] } }>(look: FieldTicket<LandPlace> | L): FieldTicket<LandPlace> | L {
  if ('ticket' in look && look.ticket) {
    const o = ownTeams(look.ticket, look.live?.seen ?? []);
    return o.ticket === look.ticket ? look : { ...look, ticket: o.ticket, live: { ...(look.live ?? {}), seen: o.seen } };
  }
  return ownTeams(look as FieldTicket<LandPlace>).ticket;
}
