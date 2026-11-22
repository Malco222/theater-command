// Strikes on ships fought on the field (ruling 238) under ruling 240's switches, joined on release 2 (step 9 of the
// rules join; the design director, 22 November 13:38Z): with damage control on, the field sinks each fleet at its own
// eased share, as fly()'s dice do. The switches are flipped in memory and put back after every test.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { shipHitsBy } from '../src/campaign/air';
import { afterCard, keptLines } from '../src/campaign/airfought';
import { launchRaid, planRaid } from '../src/campaign/airwar';
import { syncWings } from '../src/campaign/carriers';
import { createGame, setWar } from '../src/campaign/game';
import { afloatWatch, keptAfloat, sail, zoneOf, type AfloatTold } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { SHIP_RESEARCH, dcOf, techOf } from '../src/campaign/tech';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, SeaSide, ShipClass, Ships } from '../src/campaign/types';
import { airBattle, type AirSim } from '../src/battle/air/sim';
import { ShipBook, type ShipPlace } from '../src/battle/air/ships';
import { airTicket, foughtAir, raidGroups } from '../src/battle/air/ticket';
import { Rng } from '../src/core/rng';

const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const R = SHIP_RESEARCH()!;
let was = { stage: false, vt: false, dc: false };
beforeEach(() => { was = { stage: R.stage.on, vt: R.vt.on, dc: R.dc.on }; });
afterEach(() => { R.stage.on = was.stage; R.vt.on = was.vt; R.dc.on = was.dc; afloatWatch.tell = null; });
const set = (stage: boolean, vt: boolean, dc: boolean): void => { R.stage.on = stage; R.vt.on = vt; R.dc.on = dc; };
/** Puts researches straight onto a nation's list, whatever the switches say (tests/ship-aa-240.test.ts). */
const give = (g: Game, nat: string, ...ids: string[]): void => { const s = techOf(g, nat)!; for (const id of ids) if (!s.done.includes(id)) s.done.push(id); };

/** A grand campaign with Britain at the table, played until Britain is at war with Germany (tests/battle-air-ships.test.ts). */
function britain(seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;

/**
 * The Home Fleet with six carriers off the German Battle Fleet's harbour, and the Italian 2nd Fleet berthed beside the
 * Germans (Italy at war with Britain): a harbour of two navies (tests/ship-aa-240.test.ts raid and berth).
 */
const TWO = (() => {
  const g = britain();
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet'), ally = named(g, '2nd Fleet');
  home.ships.cv = 6;
  sail(g, foe, foe.home, 'port');
  setWar(g, 'UK', ally.nat);
  ally.base = foe.base; ally.stance = 'port';
  home.zone = g.seas.find((z) => z.coast.includes(foe.base))!.id; home.stance = 'patrol'; syncWings(g);
  return saveGame(g);
})();

/** The raid on that harbour, ticketed as a strike on ships ({ ships: true }), on the game as set and then as `prep` leaves it. */
function harbourRaid(prep?: (g: Game) => void): { g: Game; P: ShipPlace; port: number } {
  const g = loadGame(TWO)!;
  prep?.(g);
  const port = named(g, 'Battle Fleet').base;
  const { ticket, why } = airTicket(g, 'harbour', port, raidGroups(g, 'harbour', port, 'UK'), 'UK', { ships: true });
  if (!ticket) throw new Error(why ?? 'no ticket');
  return { g, P: ticket.place.ships!, port };
}

/** What the field sinks of each fleet and class on these throws, by ShipBook's own rule, from expectations by fleet. */
function onThrows(P: ShipPlace, seed: number, e: (i: number, c: ShipClass) => number): Record<number, Partial<Record<ShipClass, number>>> {
  const rng = new Rng(seed), out: Record<number, Partial<Record<ShipClass, number>>> = {};
  P.law.fleets.forEach((f, i) => {
    for (const c of CLASSES) {
      if (f.ships[c] <= 0) continue;
      const u = rng.next(), x = e(i, c);
      (out[f.id] ??= {})[c] = x > u ? Math.min(f.ships[c], Math.floor(x - u) + 1) : 0;
    }
  });
  return out;
}

describe('ruling 240 on the field: a harbour of two navies, damage control on (the design director, 13:38Z)', () => {
  it("sinks each fleet at its own eased share, shipHitsBy's `by`, as fly()'s dice do; not a head count's part of the eased total", () => {
    set(false, false, true);
    // Germany holds Damage control, Italy does not.
    const { g, P: again } = harbourRaid((h) => give(h, 'GER', 'damagecontrol'));
    const L = again.law, ger = L.fleets.findIndex((f) => f.nat === 'GER'), ita = L.fleets.findIndex((f) => f.nat === 'ITA');
    expect(ger).toBeGreaterThanOrEqual(0);
    expect(ita).toBeGreaterThanOrEqual(0);
    expect(L.tough![ger]).toBe(dcOf(g, 'GER'));
    expect(L.tough![ger]).toBeGreaterThan(1);
    expect(L.tough![ita]).toBe(1);
    // A class both navies have: the Germans' own share is less than a head count's part of the eased total, the
    // Italians' more (theirs is the plain law's, uneased).
    const big = shipHitsBy(L, 400 * again.mul);
    const both = CLASSES.filter((c) => c !== 'ss' && L.fleets[ger].ships[c] > 0 && L.fleets[ita].ships[c] > 0);
    expect(both.length).toBeGreaterThan(0);
    for (const c of both) {
      const head = (i: number): number => big.sunk[c] * (L.fleets[i].ships[c] / L.have[c]);
      expect(big.by![ger][c]).toBeLessThan(head(ger));
      expect(big.by![ita][c]).toBeGreaterThan(head(ita));
      expect(big.by![ita][c]).toBeCloseTo(big.plain![c] * (L.fleets[ita].ships[c] / L.have[c]), 12);
    }
    // Box by box, on several sets of throws, the field's sinkings are the ones each fleet's own share gives.
    let sank = 0;
    for (let k = 0; k < 12; k++) {
      const book = new ShipBook(again, new Rng(1000 + k), new Rng(2000 + k), 400);
      for (let d = 25; d <= 400; d += 25) {
        book.drop(d, 'bomb', d);
        const exp = shipHitsBy(L, d * again.mul);
        expect(book.sunk(), `throws ${k}, ${d} dropped`).toEqual(onThrows(again, 1000 + k, (i, c) => exp.by ? exp.by[i][c] : exp.sunk[c] * (L.fleets[i].ships[c] / L.have[c])));
      }
      sank += Object.values(book.sunk()).reduce((t, x) => t + CLASSES.reduce((n, c) => n + (x[c] ?? 0), 0), 0);
    }
    expect(sank).toBeGreaterThan(0);
  });

  it('with the switch off, or no fleet holding the research, the head count as before: no `by`, the plain law', () => {
    for (const [dc, held] of [[false, true], [true, false]] as const) {
      set(false, false, dc);
      const { P } = harbourRaid((h) => { if (held) give(h, 'GER', 'damagecontrol'); });
      expect(P.law.tough).toBeUndefined();
      const book = new ShipBook(P, new Rng(77), new Rng(78), 400);
      book.drop(400, 'bomb', 1);
      const exp = shipHitsBy(P.law, 400 * P.mul);
      expect(exp.by).toBeUndefined();
      expect(book.sunk()).toEqual(onThrows(P, 77, (i, c) => exp.sunk[c] * (P.law.fleets[i].ships[c] / P.law.have[c])));
    }
  });
});

/** The k-th battle of that harbour raid, with that battle's own luck, fought to the end by the computer on both sides (or until `stop`). */
function fought(k: number, prep?: (g: Game) => void, stop?: (s: AirSim) => boolean) {
  const r = harbourRaid(prep);
  r.g.seed = (7 + 7919 * k) >>> 0;
  r.g.flags.airBattles = k;
  const ids = raidGroups(r.g, 'harbour', r.port, 'UK');
  const ticket = airTicket(r.g, 'harbour', r.port, ids, 'UK', { ships: true }).ticket!;
  const s = airBattle(ticket, { show: false });
  s.ai.att = true; s.ai.def = true;
  while (!s.over && !stop?.(s)) s.step();
  return { ...r, ids, ticket, s };
}
const total = (x: Partial<Record<ShipClass, number>> | undefined): number => CLASSES.reduce((t, c) => t + (x?.[c] ?? 0), 0);
/** The field's own book, its throws and the bombers it last weighed (white box: what the battle drew at its start). */
const bookOf = (s: AirSim): { book: ShipBook; throws: Map<string, number>; dropped: number } => {
  const book = (s as unknown as { sea: ShipBook }).sea;
  const inner = book as unknown as { throws: Map<string, number>; dropped: number };
  return { book, throws: inner.throws, dropped: inner.dropped };
};

describe('ruling 240 on the field: ships kept afloat in a fought strike (the design director, 13:38Z)', () => {
  it("with damage control on, the field's sinkings and the ships kept afloat make the plain law's count on the same throw, by fleet and class", () => {
    set(false, false, true);
    let keptAll = 0, sunkAll = 0;
    for (let k = 0; k < 10; k++) {
      const { s, ticket } = fought(k, (h) => give(h, 'GER', 'damagecontrol'));
      const o = s.outcome(), P = ticket.place.ships!, L = P.law;
      expect(o.kept, `battle ${k}`).toBeDefined();
      const { book, throws, dropped } = bookOf(s);
      expect(book.kept()).toEqual(o.kept);
      const exp = shipHitsBy(L, dropped * P.mul);
      L.fleets.forEach((f, i) => {
        for (const c of CLASSES) {
          if (f.ships[c] <= 0) continue;
          const u = throws.get(`${f.id}:${c}`)!;
          const e0 = exp.plain ? exp.plain[c] * (f.ships[c] / L.have[c]) : 0;
          const plain = e0 > u ? Math.min(f.ships[c], Math.floor(e0 - u) + 1) : 0;
          expect(o.sunk![f.id][c]! + o.kept![f.id][c]!, `battle ${k} ${f.nat} ${c}`).toBe(plain);
          expect(o.kept![f.id][c]).toBeGreaterThanOrEqual(0);
          // The Italians have no damage control: nothing of theirs is kept afloat.
          if (f.nat === 'ITA') expect(o.kept![f.id][c]).toBe(0);
        }
      });
      keptAll += Object.values(o.kept!).reduce((t, x) => t + total(x), 0);
      sunkAll += Object.values(o.sunk!).reduce((t, x) => t + total(x), 0);
    }
    expect(sunkAll).toBeGreaterThan(0);
    expect(keptAll).toBeGreaterThan(0);
  });

  it("fought to the end, the campaign tells the field's ships kept afloat: the report's rows, afloatWatch, the news and the after card", () => {
    set(false, false, true);
    let saw = { kept: 0, none: 0 };
    for (let k = 0; k < 10; k++) {
      const { g, s, port, ids } = fought(k, (h) => give(h, 'GER', 'damagecontrol'));
      const o = s.outcome(), told: AfloatTold[] = [];
      afloatWatch.tell = (e) => told.push(e);
      const fa = foughtAir(o);
      expect(fa.kept).toEqual(o.kept);
      const rep = launchRaid(g, 'harbour', port, ids, 'UK', fa)!;
      const card = afterCard(g, rep, fa);
      const any = rep.ships!.some((x) => x.kept);
      for (const row of rep.ships!) {
        const f = g.fleets.find((x) => x.name === row.fleet)!;
        for (const c of CLASSES) {
          expect(row.sunk[c]).toBe(o.sunk![f.id][c] ?? 0);
          expect(row.kept?.[c] ?? 0, `${k} ${row.fleet} ${c}`).toBe(o.kept![f.id][c] ?? 0);
        }
        const heard = told.find((e) => e.nat === row.nat)!;
        expect(heard.saved).toEqual({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...Object.fromEntries(CLASSES.map((c) => [c, o.kept![f.id][c] ?? 0])) });
      }
      const sunk = rep.ships!.some((x) => total(x.sunk) > 0);
      if (any) {
        saw.kept++;
        // The news, as the dice's path says it (present beside ships sunk; rider 2's past sentence where none was).
        if (sunk) for (const row of rep.ships!.filter((x) => x.kept)) expect(rep.result).toContain(keptAfloat({ name: row.fleet }, row.kept!, rep.ships!.length === 1 ? 'its' : 'fleet'));
        else expect(rep.result).toMatch(/were hit, but none was sunk: the repair parties kept .* afloat\.$/);
        // The after card, once, in the past, after the bombers line.
        const lines = keptLines(rep.ships!, null);
        expect(lines.length).toBeGreaterThan(0);
        expect(card.lines.slice(1, 1 + lines.length)).toEqual(lines);
        for (const l of lines) expect(l).toMatch(sunk ? / (was|were) badly hit, but the repair parties kept the (ship|ships) afloat\.$/ : /^Ships of .* were hit, but none was sunk: the repair parties kept .* afloat\.$/);
      } else {
        saw.none++;
        expect(card.lines.join(' ')).not.toContain('repair parties');
      }
    }
    expect(saw.kept).toBeGreaterThan(0);
  });

  it("handed to the staff part way: the ships kept afloat come from whichever sinkings stand, the field's or the dice's", () => {
    set(false, false, true);
    let field = 0;
    for (let k = 0; k < 10; k++) {
      const { g, s, port, ids } = fought(k, (h) => give(h, 'GER', 'damagecontrol'), (x) => x.history().some((e) => e.what === 'air:ship-sunk'));
      if (s.over) continue;
      const at = s.outcome();
      expect(s.command({ do: 'dice' }, 'att')).toBe(true);
      const o = s.outcome();
      expect(o.sunk).toEqual(at.sunk);
      expect(o.kept).toEqual(at.kept);
      const rep = launchRaid(g, 'harbour', port, ids, 'UK', foughtAir(o))!;
      for (const row of rep.ships!) {
        const f = g.fleets.find((x) => x.name === row.fleet)!;
        for (const c of CLASSES) {
          const was = o.sunk![f.id][c] ?? 0, kept = row.kept?.[c] ?? 0;
          expect(row.sunk[c]).toBeGreaterThanOrEqual(was);
          expect(kept).toBeGreaterThanOrEqual(0);
          expect(row.sunk[c] + kept).toBeLessThanOrEqual(row.ships[c]);
          // Where what the field had sunk stands, so do the ships it had kept afloat.
          if (row.sunk[c] === was) { expect(kept, `${k} ${row.fleet} ${c}`).toBe(o.kept![f.id][c] ?? 0); field++; }
        }
      }
    }
    expect(field).toBeGreaterThan(0);
  });

  it('with the switch off, or no fleet holding the research: no ships kept afloat anywhere, and nothing said', () => {
    for (const [dc, held] of [[false, true], [true, false]] as const) {
      set(false, false, dc);
      for (let k = 0; k < 4; k++) {
        const { g, s, port, ids } = fought(k, (h) => { if (held) give(h, 'GER', 'damagecontrol'); });
        const o = s.outcome();
        expect(o.kept).toBeUndefined();
        expect(bookOf(s).book.kept()).toBeUndefined();
        const fa = foughtAir(o);
        expect('kept' in fa).toBe(false);
        const rep = launchRaid(g, 'harbour', port, ids, 'UK', fa)!;
        expect(rep.ships!.every((x) => !('kept' in x))).toBe(true);
        expect(rep.result).not.toContain('repair parties');
        expect(afterCard(g, rep, fa).lines.join(' ')).not.toContain('repair parties');
      }
    }
  });
});

/** The Home Fleet with six carriers and the German Battle Fleet together in the open Atlantic (tests/battle-air-ships.test.ts). */
const SEA = (() => {
  const g = britain();
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = 6;
  for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; }
  syncWings(g);
  return saveGame(g);
})();
/** The German harbour alone (no Italians), as tests/battle-air-ships.test.ts has it. */
const PORT = (() => {
  const g = britain();
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = 6;
  sail(g, foe, foe.home, 'port');
  home.zone = g.seas.find((z) => z.coast.includes(foe.base))!.id; home.stance = 'patrol'; syncWings(g);
  return saveGame(g);
})();

describe("ruling 240 on the field: the ships' guns with the stage and proximity-shell switches on", () => {
  for (const kind of ['harbour', 'fleet'] as const) {
    it(`the field fires the card's own guns, and costs what the card forecast inside 15 per cent: a ${kind === 'harbour' ? 'raid on a harbour' : 'strike at sea'}`, () => {
      // The Germans with late-war guns and proximity shells.
      const ticketOf = () => {
        const g = loadGame(kind === 'harbour' ? PORT : SEA)!;
        give(g, 'GER', 'firecontrol', 'taskforces', 'proxfuze');
        const foe = named(g, 'Battle Fleet'), target = kind === 'harbour' ? foe.base : foe.id;
        const ids = raidGroups(g, kind, target, 'UK');
        return { g, target, ids, ticket: airTicket(g, kind, target, ids, 'UK', { ships: true }).ticket! };
      };
      set(false, false, false);
      const off = ticketOf();
      set(true, true, false);
      const on = ticketOf();
      // The card's guns, as the raid card shows them (planRaid: "about N to the ships' guns"), are the ones the field fires.
      expect(on.ticket.place.gunLoss).toBe(planRaid(on.g, kind, on.target, on.ids, 'UK').guns.loss);
      expect(on.ticket.place.gunLoss).toBeGreaterThan(off.ticket.place.gunLoss * 1.5);
      // Many battles with the computer on both sides: the bombers lost against the card's forecast with the switches on,
      // and the bombers the guns bring down against the same battles with the switches off.
      const RUNS = 48;
      const fly = (t: typeof on.ticket, k: number): { bmr: number; flak: number } => {
        const s = airBattle({ ...t, seed: t.seed + k }, { show: false });
        s.ai.att = true; s.ai.def = true;
        while (!s.over) s.step();
        const bmr = s.outcome().groups.reduce((n, gr) => n + (gr.side === 'att' ? gr.lostBmr : 0), 0);
        const flak = s.history().filter((e) => e.what === 'air:lost' && e.extra?.air === 'bmr' && e.extra?.by === 'flak').length;
        return { bmr, flak };
      };
      let bmr = 0, flakOn = 0, flakOff = 0;
      for (let k = 0; k < RUNS; k++) {
        const x = fly(on.ticket, k);
        bmr += x.bmr;
        if (k < RUNS / 2) { flakOn += x.flak; set(false, false, false); flakOff += fly(off.ticket, k).flak; set(true, true, false); }
      }
      const want = on.ticket.place.forecast.bmr;
      expect(Math.abs(bmr / RUNS - want), `bombers lost ${(bmr / RUNS).toFixed(1)} against the card's ${want.toFixed(1)}`).toBeLessThan(Math.max(0.15 * want, 1));
      expect(flakOn, `the guns brought down ${flakOn} with the switches on, ${flakOff} off`).toBeGreaterThan(flakOff);
    }, 300_000);
  }
});

describe("the after card's words for ships kept afloat (the UI director's rider 6, 13:47Z)", () => {
  const ships = (x: Partial<Ships>): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...x });
  const row = (fleet: string, nat: string, sunk: Partial<Ships>, kept: Partial<Ships>): SeaSide =>
    ({ nat, fleet, admiral: null, ships: ships({ bb: 2, ca: 8, dd: 20 }), sunk: ships(sunk), ...(Object.keys(kept).length ? { kept: ships(kept) } : {}) });

  it('read by the raider, and by any side but the owner: the fleet named, as built', () => {
    expect(keptLines([row('Battle Fleet', 'GER', { ca: 1 }, { dd: 1 })], null)).toEqual(['A destroyer of the Battle Fleet was badly hit, but the repair parties kept the ship afloat.']);
    expect(keptLines([row('Battle Fleet', 'GER', { ca: 1 }, { ca: 1, dd: 1 })], 'UK')).toEqual(['A cruiser and a destroyer of the Battle Fleet were badly hit, but the repair parties kept the ships afloat.']);
    expect(keptLines([row('Battle Fleet', 'GER', {}, { dd: 1 })], null)).toEqual(['Ships of the Battle Fleet were hit, but none was sunk: the repair parties kept a badly hit destroyer afloat.']);
    expect(keptLines([row('Battle Fleet', 'GER', {}, { dd: 1 }), row('2nd Fleet', 'ITA', {}, {})], null)).toEqual(['Ships of the Battle Fleet and the 2nd Fleet were hit, but none was sunk: the repair parties kept a badly hit destroyer afloat.']);
    // A line for each fleet that kept any; none at all where none was kept.
    expect(keptLines([row('Battle Fleet', 'GER', { dd: 2 }, { dd: 1 }), row('2nd Fleet', 'ITA', { dd: 1 }, {})], null)).toEqual(['A destroyer of the Battle Fleet was badly hit, but the repair parties kept the ship afloat.']);
    expect(keptLines([row('Battle Fleet', 'GER', { dd: 2 }, {})], null)).toEqual([]);
  });

  it("read by the owner: one of his fleets his own; two or more each by name, 'your' and no 'the'; none sunk 'Your ships'", () => {
    // One of his fleets struck: passes as built.
    expect(keptLines([row('Battle Fleet', 'GER', { ca: 1 }, { dd: 1 })], 'GER')).toEqual(['One of your destroyers was badly hit, but the repair parties kept the ship afloat.']);
    // Mend (a): two or more of his fleets struck, each named.
    expect(keptLines([row('Battle Fleet', 'GER', { ca: 1 }, { dd: 1 }), row('2nd Fleet', 'GER', {}, { ca: 1, dd: 2 })], 'GER')).toEqual([
      'A destroyer of your Battle Fleet was badly hit, but the repair parties kept the ship afloat.',
      'A cruiser and 2 destroyers of your 2nd Fleet were badly hit, but the repair parties kept the ships afloat.',
    ]);
    // Mend (b): none sunk.
    expect(keptLines([row('Battle Fleet', 'GER', {}, { dd: 1 })], 'GER')).toEqual(['Your ships were hit, but none was sunk: the repair parties kept a badly hit destroyer afloat.']);
    expect(keptLines([row('Battle Fleet', 'GER', {}, { dd: 1 }), row('2nd Fleet', 'GER', {}, { ca: 1 })], 'GER')).toEqual(['Ships of your Battle Fleet and your 2nd Fleet were hit, but none was sunk: the repair parties kept a badly hit cruiser and a destroyer afloat.']);
    // His fleet beside an ally's: his own, and the other by its name.
    expect(keptLines([row('Battle Fleet', 'GER', { ca: 1 }, { dd: 1 }), row('2nd Fleet', 'ITA', {}, { dd: 1 })], 'GER')).toEqual([
      'One of your destroyers was badly hit, but the repair parties kept the ship afloat.',
      'A destroyer of the 2nd Fleet was badly hit, but the repair parties kept the ship afloat.',
    ]);
    expect(keptLines([row('Battle Fleet', 'GER', {}, { dd: 1 }), row('2nd Fleet', 'ITA', {}, {})], 'GER')).toEqual(['Ships of your Battle Fleet and the 2nd Fleet were hit, but none was sunk: the repair parties kept a badly hit destroyer afloat.']);
  });

  it("a played strike's after card, read by the fleet's owner: his own form, after the bombers line", () => {
    set(false, false, true);
    let saw = 0;
    for (let k = 0; k < 12 && saw < 2; k++) {
      const g = loadGame(PORT)!;
      give(g, 'GER', 'damagecontrol');
      g.seed = (7 + 7919 * k) >>> 0; g.flags.airBattles = k;
      const port = named(g, 'Battle Fleet').base, ids = raidGroups(g, 'harbour', port, 'UK');
      const s = airBattle(airTicket(g, 'harbour', port, ids, 'UK', { ships: true }).ticket!, { show: false });
      s.ai.att = true; s.ai.def = true;
      while (!s.over) s.step();
      const fa = foughtAir(s.outcome());
      const rep = launchRaid(g, 'harbour', port, ids, 'UK', fa)!;
      if (!rep.ships!.some((x) => x.kept)) continue;
      saw++;
      expect(rep.defNat).toBe('GER');
      g.human = 'GER';
      const lines = afterCard(g, rep, fa).lines;
      expect(lines[1]).toMatch(/^(One of your .+ (was|were) badly hit, but the repair parties kept the (ship|ships) afloat\.|Your ships were hit, but none was sunk: the repair parties kept .* afloat\.)$/);
      expect(lines.slice(1, 2)).toEqual(keptLines(rep.ships!, 'GER'));
    }
    expect(saw).toBeGreaterThan(0);
  });
});
