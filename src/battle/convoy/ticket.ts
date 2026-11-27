// From the campaign's convoy month to the night's field (release 2; ruling 241 D2 §1-3, the design director's rider of
// 24 November 12:34Z). The rules note each route-month as they are about to fight it (navy.ts ConvoyMeeting); this
// writes the night: the battle sea, the boats raiding it, the convoy's escorts, each navy's kit from its own research and
// its staff's habits, the night's length and moon. Nothing here changes the game.

import { hashString, Rng } from '../../core/rng';
import { turnToDate } from '../../core/ticket';
import { aswKit, NAVY, type ConvoyMeeting } from '../../campaign/navy';
import { allied } from '../../campaign/game';
import { patrolOn } from '../../campaign/seapatrol';
import { airMetWords, routeSeenWords } from '../../campaign/seasight';
import type { Game } from '../../campaign/types';
import seasData from '../../../data/campaign/seas.json';
import type { ForceTicket, Side, SideTicket } from '../core/types';
import { CONVOY, daysIn, habitsOf, moonLit, nightHours } from './numbers';
import type { ConvoyFleet, ConvoyHabits, ConvoyKit, ConvoyLight, ConvoyTicket } from './types';

const SEAS = (seasData as unknown as { zones: { id: string; name: string; at: [number, number] }[] }).zones;

/** Where a sea lies (seas.json's anchor), or the middle of the North Atlantic where the map has none. */
export function seaAt(key: string): [number, number] { return SEAS.find((z) => z.id === key)?.at ?? [55, -20]; }

/**
 * The night's light (form §5; ruling 241 Q6): the day of the month and the weather are drawn on the ticket's own stream,
 * the moon's phase follows from the date, the night's length from the latitude and the day. How far a surfaced boat is
 * seen moves the odds; the sun's and moon's heights and the moon's bearing are looks. 'month' counts from 0 for January,
 * as the sky's sums (numbers.ts) and the campaign's dates (turnToDate) count it.
 */
export function lightOf(seed: number, lat: number, year: number, month: number): ConvoyLight {
  const r = new Rng(hashString(`convoylight:${seed}`));
  const day = 1 + r.int(daysIn(year, month));
  const lit = Math.round(moonLit(year, month, day) * 100) / 100;
  const [w0, w1] = CONVOY.light.weather;
  const weather = r.range(w0, w1);
  const up = lit < 0.1 ? -10 : Math.round(15 + 30 * lit);
  const vis = Math.round((CONVOY.light.dark + CONVOY.light.moonlit * lit * (up > 0 ? 1 : 0)) * weather);
  const hours = Math.round(nightHours(lat, year, month, day) * 100) / 100;
  return { sun: -18, moon: { lit, up, from: Math.round(90 + 180 * r.next()) }, vis, hours, day };
}

/** One side of the night as the core's ticket carries it. */
function sideOf(g: Game | null, nat: string, human: boolean, fleets: ConvoyFleet[]): SideTicket {
  const n = g?.nations[nat];
  const total = fleets.reduce((s, f) => s + f.n, 0);
  const forces: ForceTicket[] = fleets.map((f) => ({
    corps: f.id, name: f.name, nat: f.nat, kind: f.side === 'att' ? 'ss' : 'dd', divs: f.n, men: f.n * (f.side === 'att' ? CONVOY.crew.ss : 150),
    str: 1, org: 1, xp: 0.5, share: f.n / Math.max(1, total),
  }));
  return { nat, adj: n?.adj ?? nat, color: n?.color ?? '#888', human, forces, general: null };
}

/**
 * The night a convoy month brings (form §1, §3): its battle sea is the route's sea with the most boats raiding it; the
 * boats are those raiding it (up to 20, one a piece: the card says how many of all), the escorts the route's guard
 * (up to 16 pieces). 'mine', where given, is the player's nation: he commands only his own pieces.
 */
export function convoyTicket(g: Game, m: ConvoyMeeting, mine: string | null = g.human ?? null): ConvoyTicket {
  const byId = new Map(g.fleets.map((f) => [f.id, f]));
  const bySea = new Map<number, number>();
  for (const r of m.raiders) { const f = byId.get(r.id); if (f && r.ss > 0) bySea.set(f.zone, (bySea.get(f.zone) ?? 0) + r.ss); }
  const zone = [...bySea.entries()].sort((a, b) => b[1] - a[1] || a[0] - b[0])[0]?.[0] ?? m.zones[0];
  const sea = g.seas[zone];
  const [lat, lon] = seaAt(sea?.key ?? '');
  const { year, month } = turnToDate(m.turn);
  const raiders = m.raiders.filter((r) => r.ss > 0 && byId.get(r.id)?.zone === zone);
  const boats = raiders.reduce((s, r) => s + r.ss, 0);
  // The twenty pieces shared over the raiders by their boats, every raider at least one where there is room.
  const fleets: ConvoyFleet[] = [];
  let left = Math.min(20, boats);
  raiders.forEach((r, i) => {
    const want = i === raiders.length - 1 ? left : Math.max(1, Math.round((r.ss / Math.max(1, boats)) * Math.min(20, boats)));
    const n = Math.max(0, Math.min(left, want, r.ss));
    left -= n;
    fleets.push({ id: r.id, name: byId.get(r.id)?.name ?? `Fleet ${r.id}`, nat: r.nat, side: 'att', n, all: r.ss, expect: r.expect });
  });
  for (const e of m.escorts) if (e.dd > 0) fleets.push({ id: e.id, name: byId.get(e.id)?.name ?? `Fleet ${e.id}`, nat: e.nat, side: 'def', n: e.dd, all: e.dd, expect: e.expect });
  const navies = [...new Set(fleets.map((f) => f.nat))];
  const kit: Record<string, ConvoyKit> = {};
  const habits: Record<string, ConvoyHabits> = {};
  for (const nat of navies) { const k = aswKit(g, nat); kit[nat] = { asdic: k.asdic, radar: k.radar, hedgehog: k.hedgehog, groups: k.groups }; habits[nat] = habitsOf(nat); }
  const seed = hashString(`convoyfield:${g.seed}:${m.turn}:${m.route}`);
  const att = fleets.filter((f) => f.side === 'att'), def = fleets.filter((f) => f.side === 'def');
  const attNat = [...att].sort((a, b) => b.n - a.n)[0]?.nat ?? m.raiders[0]?.nat ?? 'GER';
  const route = NAVY.convoys.find((r) => r.id === m.route);
  const light = lightOf(seed, lat, year, month);
  // (Release 2, once the sea patrol is live: what the player's side last saw, in his panel's own words (seasight.ts; the
  // design director's ruling of 24 November 2026 17:54Z, Q4 (a)): on the convoys' side the boats last seen on the
  // route's seas, and where he holds boats the aircraft his side's boats last met over the night's sea.)
  const sight: string[] = [];
  if (mine && patrolOn(g)) {
    if (mine === m.owner || allied(g, mine, m.owner)) sight.push(routeSeenWords(g, mine, m.zones));
    if (att.some((f) => f.nat === mine)) sight.push(airMetWords(g, mine, zone));
  }
  return {
    domain: 'sea', kind: 'field', seed, turn: m.turn, date: turnToDate(m.turn).label,
    title: `A night on ${route?.name ?? 'the convoys'}`,
    place: {
      route: m.route, sea: sea?.key ?? '', seaName: sea?.name ?? '', lat, lon, when: { year, month }, light,
      ships: CONVOY.convoy.ships[m.route] ?? 40, fleets, kit, habits,
      // (Units decide, where the route's convoy count is laid: the month's convoys sailed, sighted and attacked, for the card.)
      card: { raid: m.raid, guard: m.guard, G: m.G, L: m.L, tons: m.tons, ...(m.count ? { count: m.count } : {}), ...(sight.length ? { sight } : {}) },
      surrender: SUB_SURRENDER.on,
      ...(mine ? { mine } : {}),
    },
    att: sideOf(g, attNat, !!mine && att.some((f) => f.nat === mine), att),
    def: sideOf(g, m.owner, !!mine && def.some((f) => f.nat === mine), def),
    reckoning: { A: m.raid, D: m.guard, odds: m.raid / Math.max(1e-6, m.guard), dice: [CONVOY_SWING[0], CONVOY_SWING[1]], even: 1 },
    lines: [],
    limit: Math.round(light.hours * 3600),
  };
}

/** Striking and capture on the field (subSurrender): off until the design director and the historian rule its share. */
export const SUB_SURRENDER = { on: false };

/** The swing's run, for the reckoning's words only (convoybattle.json holds the rule's). */
const CONVOY_SWING: [number, number] = [0.8, 1.2];

/** A plain night with no game behind it (practice and tests): fleets as given, kit and habits by navy. */
export interface PlainConvoy {
  title: string; sea: string; route?: string; year: number; ships?: number;
  /** The month as a card writes it: 1 for January to 12 for December. */
  month: number;
  boats: { name: string; nat: string; n: number; all?: number; expect?: number }[];
  escorts: { name: string; nat: string; n: number; expect?: number }[];
  kit: Record<string, ConvoyKit>; habits?: Record<string, ConvoyHabits>;
}

export function plainConvoyTicket(p: PlainConvoy, seed: number, human: Side | null = 'def'): ConvoyTicket {
  const [lat, lon] = seaAt(p.sea);
  let id = 100;
  const fleets: ConvoyFleet[] = [
    ...p.boats.map((b) => ({ id: id++, name: b.name, nat: b.nat, side: 'att' as const, n: b.n, all: b.all ?? b.n, expect: b.expect ?? 0 })),
    ...p.escorts.map((e) => ({ id: id++, name: e.name, nat: e.nat, side: 'def' as const, n: e.n, all: e.n, expect: e.expect ?? 0 })),
  ];
  const habits: Record<string, ConvoyHabits> = {};
  for (const f of fleets) habits[f.nat] = p.habits?.[f.nat] ?? habitsOf(f.nat);
  // (the card's month counts from 1; the ticket's, like the campaign's, and the sky's sums count from 0)
  const month = p.month - 1;
  const light = lightOf(seed, lat, p.year, month);
  const att = fleets.filter((f) => f.side === 'att'), def = fleets.filter((f) => f.side === 'def');
  return {
    domain: 'sea', kind: 'field', seed, turn: 0, date: 'Practice', title: p.title,
    place: {
      route: p.route ?? 'atlantic', sea: p.sea, seaName: SEAS.find((z) => z.id === p.sea)?.name ?? p.sea, lat, lon, when: { year: p.year, month }, light,
      ships: p.ships ?? CONVOY.convoy.ships[p.route ?? 'atlantic'] ?? 40, fleets, kit: p.kit, habits,
      card: { raid: 0, guard: 0, G: 0, L: 0, tons: 0 }, surrender: SUB_SURRENDER.on,
    },
    att: sideOf(null, att[0]?.nat ?? 'GER', human === 'att', att),
    def: sideOf(null, def[0]?.nat ?? 'UK', human === 'def', def),
    reckoning: { A: 1, D: 1, odds: 1, dice: CONVOY_SWING, even: 1 },
    lines: [],
    limit: Math.round(light.hours * 3600),
  };
}
