// Support teams built in the campaign (release 2; Brian, 21 November 2026, 03:57Z: "for release 2 lets have these
// units be separate things to build outside of a normal infantry squad"; 03:49Z: "mortar, artillery, and machine gun
// teams ... units you can place at the begining of a battle on your side and they can then attack from the back").
//
// A machine-gun brigade ('mg') and a mortar brigade ('mor'): one division's worth each, raised as artillery is, never a
// fighting corps on its own (fights(), game.ts), going into an attack only beside the corps that do, and taking a place
// on the roads there as any corps does (limitAttackers; the design director, 25 November 2026 21:33Z). On the card: the machine guns stiffen a defence against men on foot, the mortars break up
// the fieldworks the attack goes in against. On the field each is a team block that fires from the rear (src/battle/
// land, LAND.teams).
//
// Off unless switched on: the grand campaign with the flag 'teams' set (absent means off, so a save without it is the
// game it always was). Release 1 never sets it (TEAMS_RELEASE false); the test kitchen and the tests set it themselves.
// With it off no team can be ordered, so none exists and every rule keyed on their kinds is never reached.

import arsenalData from '../../data/campaign/arsenal.json';
import type { Corps, Game, Kind } from './types';

/** Whether a new grand campaign offers the support teams (release 2 turns this on; release 1 never does). */
export const TEAMS_RELEASE = false;

export type TeamKind = Extract<Kind, 'mg' | 'mor'>;
export const TEAM_KINDS: readonly TeamKind[] = ['mg', 'mor'];
export const isTeam = (c: { kind: string }): boolean => c.kind === 'mg' || c.kind === 'mor';

/** The teams' numbers (data/campaign/arsenal.json "teams"). */
export interface TeamSpec { label: string; divs: number; cost: number; months: number; blurb: string; name: string }
export const TEAMS = (arsenalData as unknown as {
  teams: Record<TeamKind, TeamSpec> & {
    card: { mg: { perDiv: number; most: number }; mor: { perDiv: number; most: number } };
    frontage: number;
  };
}).teams;

/** The support teams are in this game: the grand campaign with the flag set. */
export const teamsOn = (g: Game): boolean => g.flags.open === 1 && g.flags.teams === 1;

/** Turns them on or off in a game; off deletes the flag, so the save is as one that never had them. */
export function setTeams(g: Game, on: boolean): void {
  if (on) g.flags.teams = 1; else delete g.flags.teams;
}

/** Divisions of one team kind among these corps, at their strength, half where cut off from supply (as gunsIn). */
export const teamsIn = (cs: Corps[], k: TeamKind): number => cs.reduce((s, c) => s + (c.kind === k ? c.divs * c.str * (c.supplied ? 1 : 0.5) : 0), 0);

/** A team's name: "1st Machine-Gun Brigade", numbered apart from the corps so no corps' number moves. */
export function teamName(g: Game, nat: string, k: TeamKind): string {
  const key = `team:${k}:${nat}`;
  const no = (g.flags[key] ?? 0) + 1;
  g.flags[key] = no;
  const s = ['th', 'st', 'nd', 'rd'], v = no % 100;
  return `${no}${s[(v - 20) % 10] || s[v] || s[0]} ${TEAMS[k].name}`;
}
