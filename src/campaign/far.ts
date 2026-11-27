// The far side's countries.
//
// The chart of the far side (the Pacific and the globe) is the chart stage's: world.ts reads it when the far side's
// switch is on (stage.ts). This file is what the countries out there need beyond a row in the data:
//
//   the first day beyond the old chart: who holds what (Japan's ground in China), the capitals that move once a
//     whole homeland is on the map (Tehran, Baghdad), the armies that stand on a client's or a patron's ground, the
//     rows of the older files they replace, and the wars already being fought (China and Japan);
//   a client's tie (the rider to ruling 209(c), the design director, 17 November 2026): Manchukuo is Japan's client
//     and Mongolia the Soviet Union's.
//
// The rows themselves are the seat builders', the planner's and the historian's, laid over the data by far-rows.ts.
// Nothing here happens in a campaign whose chart does not reach the far side (world.ts farOn).
//
// A CLIENT'S TIE. world.json, 'clients': { client: { patron, wars: [neighbours] } }. It is narrower than a dominion's
// and harder. While client and patron are at peace with each other, and the client sits in no alliance that is
// fighting its patron:
//   the client is at war with a neighbour on its row in any month its patron is, in any alliance or none, and whoever
//     leads it (the one place where a player's country goes to war without his say); it is out of that war again the
//     month its patron makes peace;
//   a country that makes war on the client is at war with the patron the same day, and when the patron makes its
//     peace with that country the client's war ends with it;
//   the two stand on each other's ground as allies do, are supplied across it and fight side by side on it
//     (game.ts allied). They are not members of one alliance (game.ts sameSide): the client shares no other war of
//     its patron's, joins none because its patron does, founds and joins no League (league.ts), and may turn on its
//     patron. The day the tie stops reading, the patron's army keeps the ground it stands on.
// What becomes of a client whose patron falls: it lives on with its own ground and its own wars.
// In a campaign of domination the month of the ties is not kept (world.ts worldMonth): the two still stand together.

import './far-rows';
import worldData from '../../data/campaign/world.json';
import { FAR_SETUP } from './far-rows';
import { FAR_STAGE } from './stage';
import { FAR_FIRST, farOn } from './world';
import { addCorps, armySize, atWar, bloc, capture, headline, relocate, removeCorps, sameSide, setWar, theName, roomless } from './game';
import { signPeace } from './diplomacy';
import { chartDist2 } from './wrap';
import { roomAt } from './islands';
import type { Corps, Game, Region } from './types';

interface ClientRow { patron: string; wars: string[] }

/** The clients and their rows. None while the far side's switch is off. */
export const CLIENTS: Record<string, ClientRow> = FAR_STAGE ? ((worldData as unknown as { clients?: Record<string, ClientRow> }).clients ?? {}) : {};
const ANY = Object.keys(CLIENTS).length > 0;

/** The patron of a client, or null. */
export const patronOfClient = (nat: string): string | null => CLIENTS[nat]?.patron ?? null;

/** The client of the two, if they are client and patron. */
function clientOf(a: string, b: string): string | null {
  return CLIENTS[a]?.patron === b ? a : CLIENTS[b]?.patron === a ? b : null;
}

/** True while a client's tie reads: the two at peace with each other, and the client in no alliance that fights its patron. */
function tieHolds(g: Game, client: string, patron: string): boolean {
  const C = g.nations[client], P = g.nations[patron];
  if (!C?.alive || !P?.alive || C.atWar.includes(patron)) return false;
  const bc = bloc(C);
  if (bc === null) return true;
  const bp = bloc(P);
  if (bp !== null && bp !== bc) return false;
  return !P.atWar.some((e) => { const E = g.nations[e]; return !!E?.alive && bloc(E) === bc; });
}

/** True for a client and its patron while their tie holds: each stands on the other's ground as an ally does. */
export function tiedPair(g: Game, a: string, b: string): boolean {
  if (!ANY) return false;
  const c = clientOf(a, b);
  return c !== null && farOn(g) && tieHolds(g, c, c === a ? b : a);
}

/**
 * True for a war a client's row gives it while its tie reads: it makes no separate peace (the third rider to ruling
 * 209(c)). The war ends when the patron's does, and the armistice of a quiet year passes the pair over (staff.ts).
 */
export function tieWar(g: Game, a: string, b: string): boolean {
  if (!ANY || !farOn(g)) return false;
  for (const [c, e] of [[a, b], [b, a]]) {
    const row = CLIENTS[c];
    if (row && row.wars.includes(e) && g.nations[row.patron]?.atWar.includes(e) && tieHolds(g, c, row.patron)) return true;
  }
  return false;
}

/**
 * No sanctuary (T5; the second rider): a corps standing on the ground of its client or its patron strikes from it
 * only into a war that country is in too. A Soviet army in Mongolia does not march on a neighbour Mongolia is at
 * peace with. Read wherever a way between two provinces is asked after (rivers.ts blocked).
 */
export function tieBars(g: Game, c: Corps, to: Region, from = c.reg): boolean {
  if (!ANY) return false;
  const host = g.regions[from]?.ctrl, lord = to.ctrl;
  if (host === undefined || host === c.nat || lord === host || clientOf(host, c.nat) === null) return false;
  return farOn(g) && atWar(g, c.nat, lord) && !atWar(g, host, lord) && tiedPair(g, host, c.nat);
}

/** The plain reason for it, for either interface to show, or null. */
export function whyTieBars(g: Game, c: Corps, to: Region, from = c.reg): string | null {
  if (!tieBars(g, c, to, from)) return null;
  const H = g.nations[g.regions[from].ctrl], L = g.nations[to.ctrl];
  return `${c.name} stands on ${theName(H)}'s ground, and ${theName(H)} is at peace with ${theName(L)}. An army strikes from the ground of its client or its patron only into a war that country is in too.`;
}

// ---- Ground held by agreement (the design director's ruling of 21:08Z of 21 November 2026, form 2) ---------------
//
// Indochina after Japan's demand on Vichy France (pacific.ts `demand`): Japan's troops are stationed in the colony
// and Japan is its holder, but the ground stays its owner's, and it carries the agreement's mark. As built the
// demand made the colony Japan's own, so every enemy of Japan had a front on it: Britain and China took Hanoi and
// Laos in 9 wars of 12, at peace with Vichy France throughout, where in the record no Allied army crossed into the
// colony before 1945. The rule: a country at war with the holder but at peace with the owner forms no front over
// marked ground (staff.ts `fronts`) and orders no ground attack (rivers.ts `blocked`) or landing (navy.ts
// `whyNotLand`) into it. Its aircraft raid the holder's troops, works and ships there as anywhere. A country at war
// with the owner as well is not bound. The mark reads while its holder holds the ground, at peace with the owner.

/** The month a province was marked as held by agreement, for the holder it was marked for. */
const stationKey = (holder: string, id: number): string => `agr:${holder}:${id}`;

/** Marks ground as held by agreement with its owner: its holder's troops are stationed there. */
export function station(g: Game, r: Region, holder: string): void {
  g.flags[stationKey(holder, r.id)] = g.turn;
  g.flags.agr ??= g.turn;
}

/** Ends the mark a holder had on a province, whoever holds it now. True if there was one. */
export function unstation(g: Game, r: Region, holder: string): boolean {
  const key = stationKey(holder, r.id);
  if (g.flags.agr === undefined || g.flags[key] === undefined) return false;
  delete g.flags[key];
  return true;
}

/** True for ground its holder stands on by agreement with its owner. Never with the far side off: nothing is marked there. */
export function stationed(g: Game, r: Region): boolean {
  if (g.flags.agr === undefined || r.ctrl === r.owner) return false;
  return g.flags[stationKey(r.ctrl, r.id)] !== undefined && !atWar(g, r.ctrl, r.owner);
}

/** True where the agreement bars a country's army from a province: at war with its holder, at peace with its owner. */
export function stationBars(g: Game, nat: string, r: Region): boolean {
  if (g.flags.agr === undefined || r.ctrl === r.owner) return false;
  return nat !== r.owner && atWar(g, nat, r.ctrl) && !atWar(g, nat, r.owner) && stationed(g, r);
}

/**
 * True where the agreement shuts ground to a country and no declaration of its own can open it: the owner is no more,
 * or stands on the country's own side. Elsewhere war on the owner frees it (turn.ts `declarable` offers the owner).
 */
export function stationShut(g: Game, nat: string, r: Region): boolean {
  if (!stationBars(g, nat, r)) return false;
  const O = g.nations[r.owner];
  return !O.alive || sameSide(g, nat, O.id);
}

/**
 * The plain reason for it, in the UI director's words (20:58Z of 21 November 2026, and the rider of 21:27Z), or null:
 * one line for an attack and for a landing. It names what frees the country, war on the owner, only where that war
 * can be declared (`stationShut` otherwise).
 */
export function whyStationBars(g: Game, nat: string, r: Region): string | null {
  if (!stationBars(g, nat, r)) return null;
  const O = g.nations[r.owner], H = g.nations[r.ctrl];
  if (stationShut(g, nat, r)) return `${r.name} belongs to ${theName(O)}, and ${theName(H)}'s troops are there by agreement: no army of yours may go in while the agreement stands. Strike them from the air.`;
  return `${r.name} belongs to ${theName(O)}, and you are not at war with it: ${theName(H)}'s troops are there by agreement. Declare war on ${theName(O)} first, or strike them from the air.`;
}

/** A war the client's row made: the month it began. */
const tieFlag =(client: string, e: string): string => `tie:${client}:${e}`;
/** A war the patron took up for its client: the month it began. */
const dutyFlag = (client: string, e: string): string => `duty:${client}:${e}`;
/** Set while a client's tie reads, so that the month it stops reading is known. */
const tiedFlag = (client: string): string => `tied:${client}`;

/** True if the war between the two was made on the client by the other: the later declaration is the other's. */
const madeOn = (g: Game, client: string, e: string): boolean => (g.flags[`war:${e}:${client}`] ?? -1) > (g.flags[`war:${client}:${e}`] ?? -1);

/** The patron's duty: war made on its client is war on its patron. True if the patron went to war. */
function answers(g: Game, client: string, e: string, quiet: boolean): boolean {
  const patron = CLIENTS[client].patron;
  const C = g.nations[client], P = g.nations[patron], E = g.nations[e];
  if (e === patron || !E?.alive || !madeOn(g, client, e) || P.atWar.includes(e) || sameSide(g, patron, e)) return false;
  setWar(g, patron, e);
  g.flags[`war:${e}:${patron}`] = g.turn;
  g.flags[dutyFlag(client, e)] = g.turn;
  g.supplyDirty = true;
  if (!quiet) headline(g, 'war', `${theName(E, true)} is at war with ${theName(P)}: war on ${theName(C)} is war on ${theName(P)}.`, patron);
  return true;
}

/**
 * The month of the clients' ties: each client into its patron's wars with the neighbours on its row and out of them
 * again, each patron into a war somebody has made on its client, and the parting where a tie has stopped reading.
 * 'quiet' on the first day, when nothing is news.
 */
export function clientMonth(g: Game, quiet = false): void {
  if (!ANY || !farOn(g)) return;
  // One client's war can be another's cause (Mongolia in beside the Soviet Union puts Manchukuo at war with Mongolia): round until nothing moves.
  for (let round = 0, moved = true; moved && round < 6; round++) {
    moved = false;
    for (const [client, row] of Object.entries(CLIENTS)) {
      const C = g.nations[client], P = g.nations[row.patron];
      if (!C || !P) continue;
      if (!tieHolds(g, client, row.patron)) {
        // The tie has stopped reading by another road than a declaration (an alliance that fights the patron): the same parting.
        if (g.flags[tiedFlag(client)] !== undefined) { parts(g, client, row.patron); moved = true; }
        continue;
      }
      g.flags[tiedFlag(client)] = 1;
      for (const e of row.wars) {
        const E = g.nations[e];
        if (!E?.alive) continue;
        const patronIn = P.atWar.includes(e), clientIn = C.atWar.includes(e);
        if (patronIn && !clientIn) {
          // The row never sets two members of one alliance at war.
          if (sameSide(g, client, e)) continue;
          setWar(g, client, e);
          g.flags[tieFlag(client, e)] = g.turn;
          g.supplyDirty = true;
          moved = true;
          if (!quiet) headline(g, 'war', `${theName(C, true)} is at war with ${theName(E)}, beside ${theName(P)}.`, client);
        } else if (!patronIn && clientIn && (g.flags[tieFlag(client, e)] !== undefined || g.flags[tieFlag(e, client)] !== undefined)) {
          // A war the tie made ends with the patron's: the client had no say in the one or the other.
          signPeace(g, client, e);
          delete g.flags[tieFlag(client, e)];
          delete g.flags[tieFlag(e, client)];
          g.supplyDirty = true;
          moved = true;
          if (!quiet) headline(g, 'event', `${theName(C, true)} and ${theName(E)} are at peace again: ${theName(P)} has made its peace.`, client);
        }
      }
      for (const e of [...C.atWar]) {
        if (P.atWar.includes(e)) continue;
        const E = g.nations[e];
        if (g.flags[dutyFlag(client, e)] !== undefined) {
          // The patron answered for its client and has made its peace since: the client's war ends with it.
          delete g.flags[dutyFlag(client, e)];
          if (!E?.alive || !signPeace(g, client, e)) continue;
          delete g.flags[`war:${e}:${client}`];
          g.supplyDirty = true;
          moved = true;
          if (!quiet) headline(g, 'event', `${theName(C, true)} and ${theName(E)} are at peace again: ${theName(P)} has made its peace.`, client);
        } else if (answers(g, client, e, quiet)) moved = true;
      }
    }
  }
}

/**
 * A declaration of war as the ties read it (game.ts declareWar, once the war is set; 'tied' is what tiedPair said of
 * the two before it was). Client and patron at war with each other part; war made on a client is war on its patron
 * the same day.
 */
export function clientWar(g: Game, a: string, b: string, tied: boolean): void {
  if (!ANY || !farOn(g)) return;
  if (tied) { const c = clientOf(a, b)!; parts(g, c, c === a ? b : a); return; }
  const p = CLIENTS[b]?.patron;
  if (p && p !== a && tieHolds(g, b, p)) answers(g, b, a, false);
}

/**
 * A client's tie stops reading: the two have gone to war with each other, or the client has joined an alliance that
 * fights its patron. Nobody of the one stays on the other's ground: the patron's army keeps the ground it stands on,
 * the client's corps there fall back to ground their country still holds, and a client's corps on its patron's soil
 * goes home, or is lost where there is nowhere left to go. The wars the tie made are the client's own from now on.
 */
function parts(g: Game, client: string, patron: string): void {
  delete g.flags[tiedFlag(client)];
  for (const k of Object.keys(g.flags)) if (k.startsWith(`tie:${client}:`) || k.startsWith(`duty:${client}:`)) delete g.flags[k];
  const C = g.nations[client], P = g.nations[patron];
  if (!C?.alive || !P?.alive) return;
  const out = (from: Region): void => {
    for (const c of g.corps.filter((x) => x.nat === client && x.reg === from.id && !x.dead)) {
      let best: Region | null = null, bd = Infinity;
      for (const r of g.regions) {
        if (r === from || r.ctrl !== client || r.units.filter((u) => !roomless(g, u)).length >= roomAt(g, r, client) || r.units.some((u) => u.nat !== client)) continue;
        const d = chartDist2(g, r, from);
        if (d < bd) { bd = d; best = r; }
      }
      if (best) relocate(g, c, best); else removeCorps(g, c);
    }
  };
  const taken: Region[] = [];
  for (const r of g.regions) {
    const hasP = r.units.some((u) => u.nat === patron && !u.dead);
    if (r.ctrl === client && hasP) taken.push(r);
    else if (r.ctrl === patron && g.corps.some((x) => x.nat === client && x.reg === r.id && !x.dead)) out(r);
  }
  // The client's corps leave the ground the patron stands on before it changes hands, so none is caught on it.
  for (const r of taken) out(r);
  // The client's own capital goes last, and without the day's reckoning of a fall (game.ts capture): the patron's
  // garrison was there already, and whether the country gives in is the month's ordinary question.
  for (const r of taken) if (C.alive && r.ctrl === client && r.id !== C.capital) capture(g, r, patron);
  const seat = g.regions[C.capital];
  if (C.alive && seat && taken.includes(seat) && seat.ctrl === client) { seat.prev = client; seat.ctrl = patron; seat.since = g.turn; g.flags[`fellP:${seat.id}`] = g.phase; }
  const kept = taken.filter((r) => r.ctrl === patron).map((r) => r.name);
  if (kept.length) headline(g, 'event', `${theName(P, true)} keeps the ground its army stands on in ${theName(C)}: ${kept.length > 4 ? `${kept.slice(0, 4).join(', ')} and ${kept.length - 4} more` : kept.length > 1 ? `${kept.slice(0, -1).join(', ')} and ${kept[kept.length - 1]}` : kept[0]}.`, client, seat && kept.includes(seat.name) ? seat.id : undefined);
  g.supplyDirty = true;
}

// ---- The first day ------------------------------------------------------------------------------------

/**
 * Who held a province on the first day: the country the far side's first day gives it to where that was not its
 * owner (`farGround`: Japan's ground in China), and otherwise its owner of that day. Read from the chart and the
 * setup, never from the war: it says the same in every month and after every load.
 */
export const heldFirst = (r: Region): string => FAR_SETUP.held[r.name] ?? r.first ?? r.owner;

/**
 * The far side's ground as it stood on 1 September 1939, set as the chart is attached and before its countries are
 * made (world.ts): the provinces another than their owner holds, the capitals that move, and the oil marks the old
 * chart gives up to the far side.
 */
export function farGround(g: Game): void {
  if (!farOn(g)) return;
  for (const [name, by] of Object.entries(FAR_SETUP.held)) {
    const r = g.byName[name];
    if (r && r.id >= FAR_FIRST) r.ctrl = by;
  }
  for (const [id, name] of Object.entries(FAR_SETUP.capitals)) {
    const to = g.byName[name], n = g.nations[id];
    if (!to || !n || to.owner !== id) continue;
    for (const r of g.regions) if (r.owner === id && r.cap && r !== to) r.cap = false;
    to.cap = true;
    n.capital = to.id;
  }
  // The oil the old chart marked one province short of where it lay, because the place was off its edge (ruling 231,
  // 1(b): Kirovabad stood for Baku). The mark is the chart's and no save keeps it, so it comes off on a load as well.
  for (const name of FAR_SETUP.unmark) {
    const r = g.byName[name];
    if (r && r.id < FAR_FIRST) r.oil = false;
  }
  // The depots a far file lays: the map's own mark (game.ts computeSupply), put on a province of the far side by
  // name. (The design director's 13:43Z of 22 November 2026: Italian East Africa was cut off from Italy from June
  // 1940 and lived on its own stocks until each seat was taken; as laid, with no depot on its ground and the sea to
  // its ports shut, its whole army starved in the fourth month of its war.)
  for (const name of FAR_SETUP.depots) {
    const r = g.byName[name];
    if (r && r.id >= FAR_FIRST) g.flags['depot:' + r.id] = 1;
  }
}

/**
 * The far side's countries on their first day, once the chart's own first day is laid (world.ts): the rows of the
 * older files that the far side's replace are taken away, the armies that stand on a client's or a patron's ground
 * are put down, and the wars already being fought are set. Nothing of it on a load: the save brings its own.
 */
export function farCountries(g: Game, fresh: boolean): void {
  if (!farOn(g) || !fresh) return;
  for (const [nat, name] of FAR_SETUP.drop) {
    const c = g.corps.find((x) => x.nat === nat && x.name === name && !x.dead);
    if (c) removeCorps(g, c);
  }
  firstDay(g);
}

/**
 * A campaign saved before the chart reached the far side is laid over today's (world.ts settleNewGround; 'from' is
 * the first province the save never knew): the far side's countries begin there as on their first day. The armies
 * the save has of the older rows stay as they are.
 */
export function farSettle(g: Game, from: number): void {
  if (farOn(g) && from <= FAR_FIRST) firstDay(g);
}

function firstDay(g: Game): void {
  const W = worldData as unknown as { corps: [string, string, string, number, string, { str?: number }][] };
  for (const [nat, name, kind, divs, at, o] of W.corps) {
    const r = g.byName[at];
    if (!r || r.id < FAR_FIRST || r.ctrl === nat || !g.nations[nat]?.alive || !tiedPair(g, nat, r.ctrl)) continue;
    if (g.corps.some((x: Corps) => x.nat === nat && x.name === name)) continue;
    const c = addCorps(g, nat, name, kind as Corps['kind'], divs, at, o);
    if (o.str !== undefined) c.str = o.str;
  }
  for (const [a, b] of FAR_SETUP.wars) if (g.nations[a]?.alive && g.nations[b]?.alive && !atWar(g, a, b)) setWar(g, a, b);
  // A client's first-day war beside its patron is the tie's: it ends when the patron's does.
  for (const [client, row] of Object.entries(CLIENTS)) {
    const C = g.nations[client], P = g.nations[row.patron];
    if (!C?.alive || !P?.alive) continue;
    for (const e of row.wars) if (C.atWar.includes(e) && P.atWar.includes(e)) g.flags[tieFlag(client, e)] = g.turn;
  }
  clientMonth(g, true);
  for (const n of Object.values(g.nations)) if (n.alive) n.armyPeak = Math.max(n.armyPeak, armySize(g, n.id));
  g.supplyDirty = true;
}
