// Support teams on the battle card (release 2; behind `teams`, off).
//
// Brian, 25 November 2026: every infantry force brings its own machine-gun and mortar teams to a battle
// (src/battle/land/own-teams.ts: a share of the force, drawn and fought as team blocks). The fairness gate on the field
// (land-battles/teams-gate2-1125.md) found that the field then leans to the defence by about two points at even odds,
// and why: the teams fire from their reach before the lines meet, the attack is under that fire first, and a force with
// teams reaches its bar sooner. The design director ruled the lean stands (21:40Z) and that the card carries it by its
// causes, in two terms and no fitted one (21:40Z and 21:47Z, ruling 217's rule):
//
//   (a) A team counts at its worth, by side, after the lines meet: 0.59 of the line it was taken from in the attack,
//       0.63 in the defence (G: the field's own tally of harm dealt after contact, 4.91% and 5.19% of a side's harm
//       from 8% of its power; 400 battles on four grounds, teams-gate2-1125.md).
//   (b) Fire before the lines meet, the card's one place for it: the wear on each line is the enemy's teams' power over
//       the line's, at the fire rules' rate and punch, for the seconds it stands in their reach before the lines meet,
//       cut by the line's cover. A weapon on the march counts no seconds. Each line's wear comes off its side as a share
//       of that side's bar (LAND.spent): the odds move by (1 - wear on the attack / 0.13) / (1 - wear on the defence / 0.18).
//
// Every figure is the field's own rule (LAND, OWN_TEAM_SHARE, OWN_TEAM_KINDS) or a measure of the field marked G below
// and pinned by tests/campaign-teamfire.test.ts. Nothing here is fitted to the two points: an all-infantry battle at
// 1.2 to 1 on plains comes to 0.991 of its odds, about 2.2 points, against the field's 2.1.
//
// Teams only, for now (21:47Z, C): the guns' own early fire stays in the card's soften and thicken until it moves into
// this place as its own release 2 item, with its own switch and gate. Nothing is priced twice.
//
// The field is laid at the power BEFORE this term (Reckoning.teamFire, sim.ts): the card's odds are what the field's fight
// then comes to, teams and all. Laid at the odds after it, the field would take the two points off a second time.
import { LAND, type Team } from '../battle/land/rules';
import { OWN_TEAM_KINDS, OWN_TEAM_SHARE } from '../battle/land/own-teams';
import type { Game } from './types';

/**
 * True in a campaign whose battles are fought with every infantry force's own teams (the Battle rules card's `teams`).
 * Its own key, `teamfire`: the campaign's `teams` flag is the buildable machine-gun and mortar brigades' (teams.ts,
 * another switch with its own gate), and the two must never ride one key.
 */
export const teamFireOn = (g: Game): boolean => g.flags.teamfire === 1;

/** Sets the switch: the 3D game as its Battle rules card's row stands, and tests. Off, the card is the card it was. */
export function setTeamFire(g: Game, on: boolean): void {
  if (on) g.flags.teamfire = 1;
  else delete g.flags.teamfire;
}

export const TEAM_FIRE = {
  /** (a) A team's worth of the line it was taken from, after the lines meet (G: the field's tally, above). */
  worth: { att: 0.59, def: 0.63 } as Record<'att' | 'def', number>,
  /**
   * The field as it is laid (G, measured on twelve grounds and pinned by the test): metres between the two lines at the
   * start, metres a side's teams stand behind its own line, and the pace its foot comes on at over a field's mix of
   * open ground and crops, as a share of LAND.speed.inf.
   */
  gap: 767,
  back: 248,
  pace: 0.96,
  /**
   * The staff's rule for a gun or team out of reach (ai.ts gunsUp; the test reads the two figures there): it moves up
   * when the nearest enemy is beyond this share of its reach, to this share of its reach, and stands.
   */
  up: { beyond: 0.9, to: 0.75 },
};

const TEAMS: readonly Team[] = ['mg', 'mor'];
const hold = (x: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, x));

/** The seconds before the lines meet, and the seconds of them each side's teams fire on the other's line. */
export function earlySeconds(): { meet: number; att: Record<Team, number>; def: Record<Team, number> } {
  const G = TEAM_FIRE, pace = LAND.speed.inf * G.pace;
  // The attack's foot comes on until the two lines' own fire meets.
  const meet = Math.max(0, (G.gap - LAND.reach.inf) / pace);
  // A team to the enemy's line at the start.
  const far = G.gap + G.back;
  // The defence's teams stand: each fires from the second the attack's line comes into its reach.
  const def = (t: Team): number => hold(meet - Math.max(0, far - LAND.teams[t].reach) / pace, 0, meet);
  // The attack's teams: in reach where they stand, they fire from the first second; out of it they march up and then
  // fire (the defence's line stands, so the march is theirs alone).
  const att = (t: Team): number => {
    const r = LAND.teams[t].reach;
    return far <= r * G.up.beyond ? meet : hold(meet - (far - r * G.up.to) / LAND.teams[t].speed, 0, meet);
  };
  return { meet, att: { mg: att('mg'), mor: att('mor') }, def: { mg: def('mg'), mor: def('mor') } };
}

/** One side as the field's ticket will share it out: each corps' kind and its weight in the side (ticket.ts sideOf). */
export type TeamCorps = [kind: string, weight: number];

export interface TeamFireIn {
  att: TeamCorps[];
  def: TeamCorps[];
  /** The weight of a town's guard fighting beside the defence (a block of its own on the field: it brings no teams). */
  guard?: number;
  /** A / D as the card stands before this term: the power the field is laid at. */
  odds: number;
  /** The ground, and the level of the fortress works that face the attack (0 for none), for the defence's cover. */
  terrain: string;
  fort?: number;
}

export interface TeamFire {
  /** What the attack's and the defence's strength are multiplied by: (a) and (b) together. */
  att: number;
  def: number;
  /** The share of each side's power in corps that bring teams. */
  share: { att: number; def: number };
  /** (b) The wear on each line before the lines meet, as a share of the line. */
  early: { att: number; def: number };
}

/** The share of a side's weight in corps that bring their own teams, and whether its line is tanks and nothing else (guns stand behind it). */
function sideOf(cs: TeamCorps[], guard = 0): { share: number; armour: boolean } {
  let all = guard, teams = 0, soft = guard;
  for (const [kind, w] of cs) {
    if (!(w > 0)) continue;
    all += w;
    if (OWN_TEAM_KINDS.includes(kind)) teams += w;
    if (kind !== 'arm' && kind !== 'art') soft += w;
  }
  return { share: all > 0 ? teams / all : 0, armour: all > 0 && soft <= 0 };
}

/**
 * What the teams do to a battle's odds, by the two causes ruled. Null where neither side brings any (the card as it was).
 */
export function teamFire(i: TeamFireIn): TeamFire | null {
  const A = sideOf(i.att), D = sideOf(i.def, i.guard ?? 0);
  if (A.share <= 0 && D.share <= 0) return null;
  const S = OWN_TEAM_SHARE, W = TEAM_FIRE.worth, secs = earlySeconds(), odds = Math.max(1e-6, i.odds);
  // (a) After the lines meet: each side's two team blocks count at their worth of the line they were taken from.
  const worthA = 1 - TEAMS.length * S * A.share * (1 - W.att), worthD = 1 - TEAMS.length * S * D.share * (1 - W.def);
  // (b) Before they meet. One side's fire, in its own power times seconds: each team block's share at its punch for its
  // seconds (bullets and light bombs at their rule's share against a line that is all tanks).
  const fire = (who: 'att' | 'def', share: number, onArmour: boolean): number =>
    TEAMS.reduce((s, t) => s + S * share * LAND.teams[t].punch * (onArmour ? LAND.teams[t].armour : 1) * secs[who][t], 0);
  // The line it falls on is the side less its teams (a team's state is its line's: sim.ts wornShare).
  const lineA = 1 - TEAMS.length * S * A.share, lineD = 1 - TEAMS.length * S * D.share;
  // The attack's line comes on over the open (G: no cover counted); the defence stands on its own ground behind its
  // works, at the field's own take (LAND.hold, as sim.ts holdTake reads the place).
  const H = LAND.hold, coverD = (H.take[i.terrain] ?? 1) * H.works[hold(Math.round(i.fort ?? 0), 0, H.works.length - 1)];
  const earlyA = (fire('def', D.share, A.armour) * LAND.rate) / (odds * lineA);
  const earlyD = (fire('att', A.share, D.armour) * LAND.rate * odds * coverD) / lineD;
  // Each line's wear comes off its side as a share of its bar. (A side worn past its bar before the lines meet has
  // nothing left to meet them with: held a hair above nothing, as the card holds any strength.)
  const leftA = Math.max(0.01, 1 - earlyA / LAND.spent.att), leftD = Math.max(0.01, 1 - earlyD / LAND.spent.def);
  return { att: worthA * leftA, def: worthD * leftD, share: { att: A.share, def: D.share }, early: { att: earlyA, def: earlyD } };
}

/** The card's line for it (the UI director's words to follow): null where it moves neither side by a whole point in a hundred. */
export function teamFireLine(f: TeamFire): { label: string; value: string; effect: string } | null {
  const a = Math.round((1 - f.att) * 100), d = Math.round((1 - f.def) * 100);
  if (!a && !d) return null;
  const both = f.share.att > 0 && f.share.def > 0;
  const value = both ? 'Machine guns and mortars on both sides fire before the lines meet'
    : f.share.def > 0 ? 'The defenders\' machine guns and mortars fire as the attack comes on'
    : 'The attack\'s machine guns and mortars fire it in';
  const fx = [a ? `attack down ${a}%` : '', d ? `defence down ${d}%` : ''].filter(Boolean).join(', ');
  return {
    label: 'Support teams', value,
    effect: `${fx[0].toUpperCase()}${fx.slice(1)}${both ? ': the attack is under fire first, and for longer' : ''}`,
  };
}
