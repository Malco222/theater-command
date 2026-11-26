// What you bring and what you face (the design director's rulings 146, 150 and 152; the Historian's
// anti-tank-and-support-weapons.md). The grand campaign only: a historical scenario is worked out as it always was.
//
// Until now only tanks stopped tanks: one tank corps in a defence switched off the attackers' whole edge, and nothing
// else touched it. Here every defence has an anti-tank strength from 0 to 1, made of its infantry's guns of the day
// (more behind fieldworks), tank destroyers, hand-held weapons, an artillery corps in the line, minefields, fortress
// works and its own tanks, against how good the attacker's armour is for its day. The tanks' edge shrinks by it.
// Machine guns, mortars, assault rifles and snipers each have a job of their own, and a battle's losses fall on what
// the defence can kill: its anti-tank strength on the tanks, its machine guns, guns and snipers on the men on foot.
// The numbers are in data/campaign/support.json. Nothing here throws dice or changes the game.
import data from '../../data/campaign/support.json';
import { turnToDate } from '../core/ticket';
import { isFoot } from './ground';
import { hasTech, techById } from './tech';
import type { Corps, Game, Region } from './types';

export const SUPPORT = data as unknown as {
  antiTank: {
    byYear: Record<string, [number, number]>;
    without: number;
    tankDestroyers: number;
    handHeld: { open: number; close: number; ground: string[] };
    armourStep: number;
    guns: { perDiv: number; most: number };
    mines: number;
    works: number[];
    tanks: number;
    alone: { cut: number; cover: number };
  };
  machineGuns: { defence: number; open: number };
  mortars: { works: number };
  assault: { attack: number };
  snipers: { toll: number; ground: string[] };
  toll: { tanks: [number, number]; machineGuns: number; guns: number; gunsMost: number; shield: number; behind: number };
  staff: { gunned: number; never: number };
  tech: { antiTank: string; tankDestroyers: string; handHeld: string; support: string; armour: string[] };
};

const weight = (c: Corps): number => c.divs * c.str;
const clamp01 = (x: number): number => Math.max(0, Math.min(1, x));
const listAnd = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);

/** True where the rule is in force: the grand campaign. A historical scenario is worked out as it always was (ruling 150). */
export const supportRules = (g: Game): boolean => g.flags.open === 1;

/**
 * True for a corps that carries the infantry's support weapons (its anti-tank guns, machine guns and mortars):
 * infantry, mountain troops, lorried infantry, cavalry and fortress troops. Tanks, guns and engineers do not.
 */
export const carries = (c: Corps): boolean => isFoot(c, true);

/** The infantry's own anti-tank guns of the day: [in the open, behind finished fieldworks]. */
export function gunsOfTheDay(year: number): [number, number] {
  const rows = SUPPORT.antiTank.byYear;
  const years = Object.keys(rows).map(Number).sort((a, b) => a - b);
  let at = years[0];
  for (const y of years) if (y <= year) at = y;
  return rows[String(at)];
}

/** The tank technologies of the day: how many of the tree's steps history had reached by this year. */
export const armourOfTheDay = (year: number): number => SUPPORT.tech.armour.filter((id) => (techById(id)?.year ?? 9999) <= year).length;

/** A defence's anti-tank strength and what it is made of. Each part is the points it adds, before the whole is held between 0 and 1. */
export interface AntiTank {
  /** 0 to 1: the share of the tanks' edge the defence takes away. */
  strength: number;
  /** The infantry's own guns of the day, by how far its fieldworks have got. */
  guns: number;
  /** False where the infantry in the line has not researched Anti-tank guns: its guns count at part of their worth. */
  kit: boolean;
  destroyers: number;
  handHeld: number;
  artillery: number;
  mines: number;
  /** The fortress works that face the attack, and their level (1 the ditch, 2 and more the dragon's teeth). */
  works: number;
  fort: number;
  tanks: number;
  /** What the attacker's armour takes off, when it is ahead of its day (negative when it is behind it). */
  armour: number;
  /** 0 to 1: the guns alone (the infantry's own, tank destroyers, hand-held weapons and guns in the line, against the attacker's armour). */
  gunned: number;
  /** How far the attack's infantry covers its tanks against the guns: 1 where there is a division of infantry to every two of tanks, 0 for tanks alone. */
  cover: number;
  /** The share of their attack the tanks lose to the guns for want of infantry beside them. */
  alone: number;
}

export interface Facing {
  /** How far a corps' fieldworks have got, 0 to 1: nothing for a corps that is not dug in or not fed. */
  works: (d: Corps) => number;
  /** The weight of guns in the defence: its artillery divisions. */
  guns: number;
  /** The level of minefields on the ground, and the weight of engineers in the attack who lift them. */
  mines: number;
  sappers: number;
  /** The level of fortress works that face this attack. */
  fort: number;
  /** The weight of infantry and of tanks in the attack, as the battle counts them beside each other. */
  foot: number;
  tanks: number;
}

/** Works out the anti-tank strength of a defence against this attack. */
export function antiTank(g: Game, attackers: Corps[], defenders: Corps[], target: Region, f: Facing): AntiTank {
  const S = SUPPORT.antiTank, T = SUPPORT.tech;
  const year = turnToDate(Math.max(1, g.turn)).year;
  const day = gunsOfTheDay(year);
  const close = S.handHeld.ground.includes(target.terrain);
  let all = 0, carry = 0, tanks = 0, guns = 0, destroyers = 0, handHeld = 0, kit = true;
  for (const d of defenders) {
    const k = weight(d);
    all += k;
    if (d.kind === 'arm') { tanks += k; continue; }
    if (!carries(d)) continue;
    carry += k;
    const has = hasTech(g, d.nat, T.antiTank);
    if (!has) kit = false;
    guns += k * (day[0] + (day[1] - day[0]) * f.works(d)) * (has ? 1 : S.without);
    if (hasTech(g, d.nat, T.tankDestroyers)) destroyers += k * S.tankDestroyers;
    if (hasTech(g, d.nat, T.handHeld)) handHeld += k * (close ? S.handHeld.close : S.handHeld.open);
  }
  if (carry > 0) { guns /= carry; destroyers /= carry; handHeld /= carry; }
  // Better armour wins part of the edge back, and yesterday's tanks give it away.
  const due = armourOfTheDay(year);
  let armour = 0, tw = 0;
  for (const c of attackers) {
    if (c.kind !== 'arm') continue;
    const k = weight(c);
    tw += k;
    armour += k * (T.armour.filter((id) => hasTech(g, c.nat, id)).length - due);
  }
  armour = tw > 0 ? S.armourStep * (armour / tw) : 0;
  const artillery = Math.min(S.guns.most, S.guns.perDiv * f.guns);
  const mines = S.mines * f.mines * (1 - Math.min(1, f.sappers));
  const fort = Math.max(0, f.fort);
  const works = S.works[Math.min(S.works.length - 1, fort)] ?? 0;
  const own = all > 0 ? S.tanks * (tanks / all) : 0;
  // Tanks alone: what the guns do to tanks that come on with no infantry to deal with them.
  // (A defence with no guns at all has none for the purpose, however old the attacker's tanks.)
  const pieces = guns + destroyers + handHeld + artillery;
  const gunned = pieces > 0 ? clamp01(pieces - armour) : 0;
  const cover = f.tanks > 0 ? clamp01((f.foot * S.alone.cover) / f.tanks) : 1;
  const alone = f.tanks > 0 ? S.alone.cut * gunned * (1 - cover) : 0;
  return { strength: clamp01(guns + destroyers + handHeld + artillery + mines + works + own - armour), guns, kit, destroyers, handHeld, artillery, mines, works, fort, tanks: own, armour, gunned, cover, alone };
}

/** The share of an attack's weight that goes in on foot. */
export function footShare(attackers: Corps[]): number {
  let all = 0, foot = 0;
  for (const c of attackers) { const k = weight(c); all += k; if (isFoot(c)) foot += k; }
  return all > 0 ? foot / all : 0;
}

/**
 * Machine guns: what a defending corps with Submachine guns and mortars gains against men on foot. Nothing against
 * tanks, most in finished fieldworks.
 */
export function machineGuns(g: Game, d: Corps, foot: number, works: number): number {
  if (!carries(d) || !hasTech(g, d.nat, SUPPORT.tech.support)) return 1;
  const M = SUPPORT.machineGuns;
  return 1 + M.defence * foot * (M.open + (1 - M.open) * works);
}

/** Mortars: the share of the worth of fieldworks (never of fortress works) that the attack's own mortars take out. */
export function mortars(g: Game, attackers: Corps[]): number {
  let all = 0, have = 0;
  for (const c of attackers) { const k = weight(c); all += k; if (isFoot(c) && hasTech(g, c.nat, SUPPORT.tech.support)) have += k; }
  return all > 0 ? SUPPORT.mortars.works * (have / all) : 0;
}

/** Assault rifles: what a corps on foot gains in the attack. */
export const assaultFire = (g: Game, c: Corps): number => (isFoot(c) && hasTech(g, c.nat, SUPPORT.tech.handHeld) ? 1 + SUPPORT.assault.attack : 1);

/** Where snipers tell: 'forest' or 'city' by the ground, 'town' in an assault on a town. Null where they do not. */
export type Sniped = 'forest' | 'city' | 'town' | null;
export const snipedIn = (target: Region, town: boolean): Sniped =>
  town ? 'town' : !SUPPORT.snipers.ground.includes(target.terrain) ? null : target.terrain === 'urban' ? 'city' : 'forest';

/** Who pays: what each arm in the attack loses, as a multiple of the usual share, and what the men on foot owe it to. */
export interface Toll {
  tanks: number;
  foot: number;
  guns: number;
  /** What each cause adds to the toll on the men on foot, and what the tanks beside them take off it. */
  why: { machineGuns: number; guns: number; snipers: number; shield: number };
}

/** Works out who pays for an attack on this defence. In an assault on a town (no anti-tank strength, no facing) only the snipers tell. */
export function tollOf(g: Game, attackers: Corps[], defenders: Corps[], at: AntiTank | null, f: Facing | null, sniped: Sniped): Toll {
  const L = SUPPORT.toll, M = SUPPORT.machineGuns;
  const snipers = sniped ? SUPPORT.snipers.toll : 0;
  if (!at || !f) return { tanks: 1, foot: 1 + snipers, guns: 1, why: { machineGuns: 0, guns: 0, snipers, shield: 0 } };
  let all = 0, mg = 0;
  for (const d of defenders) {
    const k = weight(d);
    all += k;
    if (carries(d) && hasTech(g, d.nat, SUPPORT.tech.support)) mg += k * (M.open + (1 - M.open) * f.works(d));
  }
  let mine = 0, armour = 0;
  for (const c of attackers) { const k = weight(c); mine += k; if (c.kind === 'arm') armour += k; }
  // Tanks beside the infantry take part of the toll off it, as far as the anti-tank guns let them work.
  const shield = mine > 0 ? L.shield * Math.min(1, (2 * armour) / mine) * (1 - at.strength) : 0;
  const machine = L.machineGuns * (all > 0 ? mg / all : 0), guns = Math.min(L.gunsMost, L.guns * f.guns);
  return {
    tanks: L.tanks[0] + (L.tanks[1] - L.tanks[0]) * at.strength,
    foot: (1 + machine + guns + snipers) * (1 - shield),
    guns: L.behind,
    why: { machineGuns: machine, guns, snipers, shield },
  };
}

/** What one attacking corps pays, as a multiple of the usual share. */
export const tollFor = (t: Toll | null | undefined, c: Corps): number => (!t ? 1 : c.kind === 'arm' ? t.tanks : c.kind === 'art' ? t.guns : t.foot);

/**
 * What an attack faces, as the battle works it out (the grand campaign): the defence's anti-tank strength, who will
 * pay, and the facts the battle card tells him before he attacks and after.
 */
export interface Faced {
  /** Null in an assault on a town: the streets are the rule there. */
  antiTank: AntiTank | null;
  toll: Toll;
  /** The share of the tanks' shock that is left, 1 in full and 0 nothing. Null where no tanks go in or the ground stops them. */
  shock: number | null;
  /** The share of their attack the tanks lose to the guns for want of infantry beside them. Zero where they are covered, or the ground has stopped them. */
  alone: number;
  /** Machine guns in the defence that men on foot will meet, and whether any of them fire from fieldworks. */
  machineGuns: boolean;
  mgDug: boolean;
  /** The share of the fieldworks' worth the attack takes away, and with what. Zero where nobody is dug in. */
  worksCut: number;
  mortars: boolean;
  gunsIn: boolean;
  snipers: Sniped;
}

// ---- What the cards say ---------------------------------------------------------------------

/** A share in words, as the cards give it: the figures stay in the ticket. */
export function shareWords(x: number): string {
  return x >= 0.9 ? 'in full' : x >= 0.71 ? 'for three quarters' : x >= 0.58 ? 'for two thirds' : x >= 0.42 ? 'for half'
    : x >= 0.29 ? 'for a third' : x >= 0.17 ? 'for a quarter' : x >= 0.05 ? 'for a tenth' : 'for nothing';
}
/** The same for a share taken away: "a third of them". */
const shareOf = (x: number): string => (x >= 0.9 ? 'all' : x >= 0.71 ? 'three quarters' : x >= 0.58 ? 'two thirds' : x >= 0.42 ? 'half'
  : x >= 0.29 ? 'a third' : x >= 0.17 ? 'a quarter' : 'a tenth');

/** What stands against the tanks, biggest first: the names of at most three parts of the anti-tank strength. */
export function antiTankParts(at: AntiTank): string[] {
  const parts: [number, string][] = [
    [at.guns, 'anti-tank guns'], [at.destroyers, 'tank destroyers'], [at.handHeld, 'hand-held weapons'], [at.artillery, 'guns in the line'],
    [at.mines, 'mines'], [at.works, at.fort >= 2 ? 'dragon\'s teeth' : 'a ditch'], [at.tanks, 'tanks of their own'],
  ];
  return parts.filter((p) => p[0] >= 0.03).sort((a, b) => b[0] - a[0]).slice(0, 3).map((p) => p[1]);
}

/** How the card names tanks that go in with no infantry, or too little, beside them. */
export const aloneWords = (at: AntiTank): string => `${at.cover <= 0 ? 'No infantry' : 'Too little infantry'} with your tanks`;
/** The same line where the defence's guns are not known: the most they could take from tanks covered this far. Null where infantry covers the tanks. */
export function aloneUnknown(at: AntiTank): FaceLine | null {
  const most = SUPPORT.antiTank.alone.cut * (1 - at.cover);
  return most >= 0.05 ? { sign: 'tank', text: `${aloneWords(at)}: their guns take as much as ${shareOf(most)} of the tanks' attack.` } : null;
}

export type FaceSign = 'tank' | 'anti-tank' | 'machine-gun' | 'mortar' | 'sniper' | 'gun' | 'scouted';
/** One line of WHAT YOU FACE: a sign and a sentence. 'unknown' marks the line that stands alone when nothing is scouted. */
export interface FaceLine { sign: FaceSign; text: string; unknown?: boolean }

/**
 * WHAT YOU FACE (rulings 146(d)(8), 149(d), 152 and 164(f)): a line for each thing the defence will do to what he brings,
 * four at most and the snipers' the first to go, facts and never a warning. These are the lines for a defence that is identified: what his reports do
 * not know is left out in facing.ts.
 */
export function faceLines(f: Faced): FaceLine[] {
  const out: FaceLine[] = [];
  const at = f.antiTank;
  // First the one thing he can change before he clicks; then guns and obstacles, machine guns, fieldworks, and snipers last (ruling 164(f)).
  if (at && f.alone >= 0.05) out.push({ sign: 'tank', text: `${aloneWords(at)}: their guns take ${shareOf(f.alone)} of the tanks' attack.` });
  if (at && f.shock !== null) {
    const parts = antiTankParts(at);
    out.push({ sign: 'anti-tank', text: at.strength < 0.1 || !parts.length ? 'Few anti-tank guns: your tanks\' shock counts in full.' : `${cap(listAnd(parts))}: your tanks' shock counts ${shareWords(f.shock)}.` });
  }
  if (f.machineGuns) out.push({ sign: 'machine-gun', text: f.mgDug ? 'Machine guns, dug in: most of your losses will be infantry.' : 'Machine guns in the open: more of your losses will be infantry.' });
  if (f.worksCut >= 0.05) out.push({ sign: 'mortar', text: `Fieldworks: your ${f.mortars && f.gunsIn ? 'mortars and guns' : f.mortars ? 'mortars' : 'guns'} take ${shareOf(f.worksCut)} of them away.` });
  if (f.snipers && f.toll.why.snipers > 0) out.push({ sign: 'sniper', text: `Snipers in the ${f.snipers}: more of your losses will be infantry.` });
  return out.slice(0, 4);
}

/** The men an attack lost, by arm. */
export interface Paid { foot: number; tanks: number; guns: number }

/**
 * The result's line (ruling 149 as settled at 21:14Z): who paid most and to what, then the tanks' shock if tanks went
 * in. Two sentences at most, shares in words. 'mine' is true when the reader attacked, false when he defended.
 */
export function whoPaid(f: Faced, lost: Paid, mine: boolean, most?: string, quiet = false): { text: string; sign: FaceSign | null } {
  const all = lost.foot + lost.tanks + lost.guns;
  const who = mine ? 'Your' : 'Their', whose = mine ? 'their' : 'your';
  // A battle fought on the field whose cost fell unevenly names the corps that paid most (ruling 226(6)); fought with
  // one corps, or uneven with no one corps to name (b464(6)), the line goes.
  let first = most ? `${most} paid most of it.` : quiet ? '' : `The cost fell evenly on ${mine ? 'your' : 'their'} corps.`;
  let sign: FaceSign | null = null;
  if (all > 0) {
    const w = f.toll.why;
    if (lost.foot > all / 2 && f.toll.foot > 1.03) {
      const causes = ([[w.machineGuns, 'machine guns'], [w.guns, 'guns'], [w.snipers, 'snipers']] as [number, string][]).filter((c) => c[0] > 0).sort((a, b) => b[0] - a[0]).slice(0, 2).map((c) => c[1]);
      if (causes.length) { first = `${who} infantry paid most of it, to ${whose} ${listAnd(causes)}.`; sign = causes[0] === 'machine guns' ? 'machine-gun' : causes[0] === 'snipers' ? 'sniper' : 'gun'; }
    } else if (lost.tanks > all / 2 && f.toll.tanks > 1.03 && f.antiTank) {
      const causes = antiTankParts(f.antiTank).slice(0, 2);
      if (causes.length) { first = `${who} tanks paid most of it, to ${whose} ${listAnd(causes).replace('tanks of their own', 'tanks')}.`; sign = 'anti-tank'; }
    }
  }
  return { text: f.shock === null ? first : `${first} ${mine ? 'The' : 'Their'} tanks' shock counted ${shareWords(f.shock)}.`.trim(), sign };
}

/** The weapons a corps carries, for the row of signs on its card: null for a corps the rule counts none for, and outside the grand campaign. */
export interface Carried {
  /** 0 to 3: Anti-tank guns, Tank destroyers and the hand-held weapons that come with Assault rifles. */
  antiTank: number;
  machineGuns: boolean;
  mortars: boolean;
  snipers: boolean;
}
export function carried(g: Game, c: Corps): Carried | null {
  if (!supportRules(g) || !carries(c)) return null;
  const T = SUPPORT.tech;
  const has = (id: string): boolean => hasTech(g, c.nat, id);
  return { antiTank: [T.antiTank, T.tankDestroyers, T.handHeld].filter(has).length, machineGuns: has(T.support), mortars: has(T.support), snipers: true };
}
