// Named aces (Brian, 22 November 2026: "a fighter group with a strong record earns a named ace; if he's shot down, the
// group's spirit drops for a month"; the design director's rulings of 22 November, 17:44Z and 17:45Z). The grand
// campaign only, behind a switch: data/campaign/aces.json 'on', kept with each campaign in g.flags.aces.
//
// A fighter group's leading pilot downs a share of what the group downs ('share', a knob until the historian sources
// it). Once that share of the aircraft the group has brought down since his count began reaches five, the record's own
// word for an ace, he is named: an invented man of his nation and service, never one of history's aces. His tally is
// that share, rounded, from then on, and he climbs a rank for each fifteen. After every fight the group is in he may
// fall: his chance is the group's own loss share that day. Then the next man's count begins.
//
// The stories' one rule holds (stories.ts, ruling R1): every nation's aces are named, so the whole war has them, but
// only the player's change a number: while his ace flies the player's group is worth a little more, inside the stories'
// edge cap as an honour is ('edge'), and after he falls ... the 'grief' knob, held at 1 (nothing) until the historian
// sources it. Who is named and who falls are drawn from streams of the group's own, never from the dice of the war, so
// a war with no player is the same war with the switch on or off.
//
// With the switch off nothing here is read or written.

import data from '../../data/campaign/aces.json';
import { Rng, hashString } from '../core/rng';
import { headline } from './game';
import type { AceRecord, AceState, AirAce, AirFleet, AirSide, Game, Storied } from './types';

interface Names { first: string[]; last: string[]; ranks: string[]; familyFirst?: boolean }
export const ACES = data as unknown as {
  on: boolean; at: number; share: number; risk: number; edge: number; grief: number; griefMonths: number; promote: number;
  lang: Record<string, string>; names: Record<string, Names>; guard: string[];
  words: { named: string; fell: string; theirs: string };
};

/** True in a campaign played with named aces: the grand campaign, begun with the switch on. */
export const acesOn = (g: Game): boolean => g.flags.open === 1 && g.flags.aces === 1;

/** True when a new grand campaign opens with named aces (aces.json 'on'). The interfaces read it as a campaign starts. */
export const acesShipped = (): boolean => ACES.on === true;

/** Turns named aces on or off (a new grand campaign as shipped, or the test kitchen). The bridge calls it by name. */
export function setAces(g: Game, on: boolean): void {
  if (on) g.flags.aces = 1;
  else delete g.flags.aces;
}

const namesOf = (nat: string): Names => ACES.names[ACES.lang[nat] ?? ACES.lang._default] ?? ACES.names.uk;

/** The aircraft a group has brought down in the war: those it shot down attacking and those it brought down defending (the stories' records). */
export const downedOf = (f: Storied): number => (f.rec?.kills ?? 0) + (f.rec?.saved ?? 0);

/** A group that can have an ace: one that flies fighters. */
const fighters = (f: AirFleet): boolean => f.kind !== 'bmr';

/** His rank today: the one he was named at, a step up for each 'promote' victories, to the top of his nation's list. */
export function aceRank(nat: string, a: AirAce): string {
  const r = namesOf(nat).ranks;
  return r[Math.min(r.length - 1, a.rank + Math.floor(a.kills / ACES.promote))];
}

/** "Flight Lieutenant Tom Harland": his rank today and his name. */
export const aceTitle = (nat: string, a: AirAce): string => `${aceRank(nat, a)} ${a.name}`;

/** A new ace for a group: a name of his nation's, drawn from the group's own stream; never a famous ace, never a name the nation already has. */
function newAce(g: Game, f: AirFleet, st: AceState, kills: number): AirAce {
  const N = namesOf(f.nat);
  const rng = new Rng(hashString(`acename:${g.seed}:${f.id}:${st.n}`));
  const taken = new Set(g.airFleets.filter((x) => x.nat === f.nat && x.aces?.now).map((x) => x.aces!.now!.name));
  let name = '';
  for (let k = 0; k < 8; k++) {
    const first = rng.pick(N.first), last = rng.pick(N.last);
    name = N.familyFirst ? `${last} ${first}` : `${first} ${last}`;
    if (!ACES.guard.includes(name) && !taken.has(name)) break;
  }
  return { name, rank: rng.int(2), kills, from: st.from, since: g.turn };
}

const fill = (s: string, v: Record<string, string | number>): string => s.replace(/\{(\w+)\}/g, (_, k: string) => String(v[k] ?? ''));

/** Keeps an ace on the war's record: the player's own, and the enemy's his fighters brought down. */
function onRecord(g: Game, f: AirFleet, a: AirAce, fell?: number, at?: number): void {
  const list = (g.stories.aces ??= []);
  const title = aceTitle(f.nat, a);
  const was = list.find((x) => x.group === f.id && x.since === a.since && x.nat === f.nat);
  const row: AceRecord = was ?? { nat: f.nat, group: f.id, unit: f.name, name: title, kills: a.kills, since: a.since };
  row.name = title; row.kills = a.kills;
  if (fell !== undefined) row.fell = fell;
  if (at !== undefined && at >= 0) row.at = at;
  if (!was) list.push(row);
}

/**
 * After an air fight: every fighter group in it may name its ace, or add to his tally, and he may fall. `att` and `def`
 * are the sides as the fight booked them (the aircraft each group flew and lost), beside the groups themselves. The
 * stories' records (storyAir) are already written. `where` is the province fought over (or -1), `place` its name.
 */
export function acesFought(g: Game, att: AirSide[], attF: AirFleet[], def: AirSide[], defF: AirFleet[], where: number, place: string): void {
  if (!acesOn(g)) return;
  const me = g.human;
  const rows: [AirSide, AirFleet, boolean][] = [...att.map((s, i): [AirSide, AirFleet, boolean] => [s, attF[i], true]), ...def.map((s, i): [AirSide, AirFleet, boolean] => [s, defF[i], false])];
  const over = place ? `over ${place}` : 'in action';
  for (const [side, f, attacking] of rows) {
    if (!f || !fighters(f) || side.ftr <= 0) continue;
    const st = (f.aces ??= { from: 0, n: 0 });
    const down = downedOf(f);
    const tally = Math.round(ACES.share * (down - st.from));
    if (!st.now) {
      if (ACES.share * (down - st.from) < ACES.at) continue;
      st.now = newAce(g, f, st, Math.max(ACES.at, tally));
      st.n++;
      if (f.nat === me) {
        const a = st.now;
        onRecord(g, f, a);
        headline(g, 'battle', fill(ACES.words.named, { group: f.name, title: aceRank(f.nat, a), name: a.name, kills: a.kills }), f.nat, where >= 0 ? where : undefined);
        g.stories.mail.push({ w: 5, m: 'ace', id: -f.id, unit: f.name, reg: f.base, x: aceTitle(f.nat, a) });
      }
      continue;
    }
    const a = st.now;
    a.kills = Math.max(a.kills, tally);
    if (f.nat === me) onRecord(g, f, a);
    // Did he come home? The group's own loss share that day (ruling R2), from his own stream.
    const p = Math.min(1, ACES.risk * (side.lostFtr / side.ftr));
    if (p <= 0) continue;
    const roll = new Rng(hashString(`acefell:${g.seed}:${f.id}:${st.n}:${g.turn}:${g.flags.airBattles ?? 0}`)).next();
    if (roll >= p) continue;
    // He fell. The news names him for the player's own, and for an enemy's the player's fighters brought down.
    const foes = attacking ? def : att;
    const words = { group: f.name, title: aceRank(f.nat, a), name: a.name, kills: a.kills, where: over };
    if (f.nat === me) {
      onRecord(g, f, a, g.turn, where);
      headline(g, 'battle', fill(ACES.words.fell, words), f.nat, where >= 0 ? where : undefined);
      g.stories.mail.push({ w: 7, m: 'acefell', id: -f.id, unit: f.name, reg: f.base, x: aceTitle(f.nat, a) });
    } else if (me && foes.some((s) => s.nat === me && s.ftr > 0)) {
      onRecord(g, f, a, g.turn, where);
      headline(g, 'battle', fill(ACES.words.theirs, words), me, where >= 0 ? where : undefined);
    }
    st.now = undefined;
    st.from = down;
    st.fell = g.turn;
  }
}

/** What the player's ace adds to his group, inside the stories' edge cap (stories.ts edge): 0 with no ace, or for a computer-run nation. */
export function aceEdge(g: Game, u: Storied & { nat?: string }): number {
  if (!acesOn(g) || u.nat !== g.human) return 0;
  return (u as AirFleet).aces?.now ? ACES.edge : 0;
}

/** The player's group after its ace has fallen, for 'griefMonths' months counting the month he fell: 1 otherwise, and 1 as shipped (the knob is held). */
export function aceGrief(g: Game, u: Storied & { nat?: string }): number {
  if (!acesOn(g) || u.nat !== g.human) return 1;
  const fell = (u as AirFleet).aces?.fell;
  return fell !== undefined && g.turn - fell < ACES.griefMonths ? ACES.grief : 1;
}

/** The aces of the groups in an air battle, for its ticket (battle/air AirPlace.aces): shown, never counted. Null with none. */
export function aceSeats(g: Game, groups: AirFleet[]): Record<number, { name: string; kills: number }> | null {
  if (!acesOn(g)) return null;
  const out: Record<number, { name: string; kills: number }> = {};
  for (const f of groups) if (f.aces?.now) out[f.id] = { name: aceTitle(f.nat, f.aces.now), kills: f.aces.now.kills };
  return Object.keys(out).length ? out : null;
}

/** The line on a group's card: "Its ace: Flight Lieutenant Tom Harland, 23 victories." Null with none. */
export function aceLine(g: Game, f: AirFleet): string | null {
  if (!acesOn(g) || !f.aces?.now) return null;
  return `Its ace: ${aceTitle(f.nat, f.aces.now)}, ${f.aces.now.kills} victories.`;
}
