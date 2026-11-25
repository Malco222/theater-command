// True reach (audit item 41; the design director's ruling of 13:58Z, "Withering garrisons" 1, and 14:26Z, "Ports fed
// by an ally's paper reach: the old-chart count", 25 November 2026; src/campaign/game.ts trueReach).
//
// "A sea the shutting power holds closed to the nation fed is closed to every ship carrying it stores, whatever the
// flag: an ally's, a neutral's, or one at war with nobody there. An ally feeds only where its way is open."
//
// The supply picture let an ally's ships feed a port for another nation (an ally's port overseas for the guest's corps,
// the side theatre's shared shipping) wherever the ally's own lanes reached it: Norway, at war with Germany and not
// with Italy, kept a British Malta fed through a sea Italy had shut to Britain. With the rule on, the ally's lanes are
// walked as if every sea closed to the nation fed were closed to the ally too. The old chart, the rule's own sub-flag
// under Units decide; the far switch in tests/true-reach-far-r2.test.ts. Seas are closed by real fleets; no war is played.
//
// Every picture in this file is drawn under TC_SUPPLY_CHECK, which asks every kept answer afresh and throws on a difference.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_SUPPLY_CHECK = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, computeSupply, createGame, declareWar, portKeep, setTrueReach, trueReach } from '../src/campaign/game';
import { SeaPicture, openSeas, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { TRUEREACH_STAGE, UNITS_STAGE } from '../src/campaign/stage';
import { setUnitsDecide } from '../src/campaign/units';
import type { Game } from '../src/campaign/types';

/** A war nobody plays, the old chart's first day, with Units decide; true reach's sub-flag as asked. */
const war = (on: boolean): Game => {
  const g = startOpen(createGame(1, null));
  g.flags.unitsDecide = 1;
  if (on) g.flags.trueReach = 1;
  g.supplyDirty = true;
  return g;
};
/** One nation's fleets all patrolling one sea. */
const patrol = (g: Game, nat: string, key: string): void => { const z = zoneOf(g, key); for (const f of g.fleets) if (f.nat === nat) { f.zone = z; f.stance = 'patrol'; } };
const fed = (g: Game, nat: string, name: string): boolean => !!g.supply[nat][g.byName[name].id];
const reaches = (g: Game, nat: string, key: string): number => openSeas(g, nat, new SeaPicture(g))[zoneOf(g, key)];

/** Malta held by Britain, Italy at war with Britain and her fleet across the central Mediterranean, Norway in the war with Germany only. */
const malta = (on: boolean) => {
  const g = war(on);
  declareWar(g, 'ITA', 'UK');
  declareWar(g, 'GER', 'NOR');
  patrol(g, 'ITA', 'central');
  const c = addCorps(g, 'UK', 'Malta test', 'inf', 1, 'Malta', {});
  computeSupply(g);
  return { g, c, r: g.byName['Malta'] };
};

describe('an ally\'s ships feed a port only where they could truly sail there with its stores (true reach, audit item 41)', () => {
  it('Malta by Norway: off, Norway\'s paper reach feeds the British corps for ever; on, the fortress lives on its stores and then goes hungry', () => {
    // The ground both readings share: the central Mediterranean is shut to Britain by Italy, and open to Norway, whose
    // own lanes reach it; Britain's own do not.
    const off = malta(false);
    const pic = new SeaPicture(off.g), central = zoneOf(off.g, 'central');
    expect([pic.closed(central, 'UK'), pic.closed(central, 'NOR')]).toEqual([true, false]);
    expect([reaches(off.g, 'UK', 'central'), reaches(off.g, 'NOR', 'central')]).toEqual([0, 1]);
    expect(off.r.seas).toEqual([central]);
    expect(trueReach(off.g)).toBe(false);
    // Off (today's answer, pinned): Malta is a source for Britain month after month, its stores mark kept fresh.
    for (let k = 0; k < 5; k++) {
      if (k) { off.g.turn++; computeSupply(off.g); }
      expect([off.g.turn, fed(off.g, 'UK', 'Malta'), off.c.supplied, off.g.flags[`stores:${off.r.id}`]]).toEqual([1 + k, true, true, 1 + k]);
    }
    // On: no ship reaches Malta with Britain's stores. The fortified port keeps its stores portKeep months from the
    // last month a ship came in (the first day, before the war with Italy), then the corps is out of supply.
    const on = malta(true);
    expect(trueReach(on.g)).toBe(true);
    const keep = portKeep(on.g, on.r);
    expect(keep).toBeGreaterThan(0);
    for (let k = 0; k <= keep + 2; k++) {
      if (k) { on.g.turn++; computeSupply(on.g); }
      const left = on.g.turn - 1 <= keep;
      expect([on.g.turn, fed(on.g, 'UK', 'Malta'), on.c.supplied, on.g.flags[`stores:${on.r.id}`]]).toEqual([1 + k, left, left, 1]);
    }
    expect(on.c.supplied).toBe(false);
    // Norway's own ships still reach Malta with Norway's stores (it is a quay of Norway's own picture), and that does
    // not fill the British garrison's stores: they are filled only by ships that could carry them there for the holder.
    expect(fed(on.g, 'NOR', 'Malta')).toBe(true);
  });

  it('does not change an ally at war with the same shutter whose own way is open: Italy feeds a German corps in Tripoli, on and off', () => {
    for (const on of [false, true]) {
      const g = war(on);
      declareWar(g, 'ITA', 'UK');
      const c = addCorps(g, 'GER', 'Tripoli test', 'inf', 1, 'Tripoli', {});
      computeSupply(g);
      const r = g.byName['Tripoli'], pic = new SeaPicture(g);
      // Germany's own ships reach no sea of Tripoli's; Italy's do, and some sea of Italy's lanes is closed to Germany,
      // so the walk as if carrying Germany's stores is really taken; it still reaches Tripoli.
      const ger = openSeas(g, 'GER', pic), ita = openSeas(g, 'ITA', pic), carrying = openSeas(g, 'ITA', pic, undefined, undefined, undefined, 'GER');
      expect(r.seas.some((z) => ger[z])).toBe(false);
      expect(r.seas.some((z) => ita[z])).toBe(true);
      expect([...ita.keys()].some((z) => ita[z] && pic.closed(z, 'GER'))).toBe(true);
      expect(r.seas.some((z) => carrying[z])).toBe(true);
      expect([on, fed(g, 'GER', 'Tripoli'), c.supplied]).toEqual([on, true, true]);
    }
  });

  it('still lets a neutral to the shutter feed by a way no sea of which is closed to the nation fed: the rule closes seas, it bans no flag', () => {
    for (const on of [false, true]) {
      // Italy at peace with Britain, Britain's fleet at home: the seas Britain shuts to Germany lie outside Italy's way to Tripoli.
      const g = war(on);
      const c = addCorps(g, 'GER', 'Tripoli test', 'inf', 1, 'Tripoli', {});
      computeSupply(g);
      const r = g.byName['Tripoli'], pic = new SeaPicture(g);
      const ita = openSeas(g, 'ITA', pic), carrying = openSeas(g, 'ITA', pic, undefined, undefined, undefined, 'GER');
      expect([...ita.keys()].filter((z) => ita[z] && pic.closed(z, 'GER')).length).toBeGreaterThan(0);
      expect(r.seas.some((z) => carrying[z])).toBe(true);
      expect([on, fed(g, 'GER', 'Tripoli'), c.supplied]).toEqual([on, true, true]);
    }
  });

  it('reads the holder\'s own port overseas the same way: Italy\'s Rhodes does not feed a German corps through a sea Britain shuts to Germany', () => {
    for (const on of [false, true]) {
      // Italy at peace with Britain, Britain's fleet across the Aegean. Rhodes lies in no side theatre: only the
      // holder's quay (line 2) can feed a guest there.
      const g = war(on);
      const r = g.byName['Rhodes'];
      patrol(g, 'UK', 'aegean');
      const c = addCorps(g, 'GER', 'Rhodes test', 'inf', 1, 'Rhodes', {});
      computeSupply(g);
      const pic = new SeaPicture(g), aegean = zoneOf(g, 'aegean');
      expect([r.ctrl, r.overseas, r.theatre, r.seas]).toEqual(['ITA', true, -1, [aegean]]);
      expect([pic.closed(aegean, 'GER'), pic.closed(aegean, 'ITA')]).toEqual([true, false]);
      expect([reaches(g, 'GER', 'aegean'), reaches(g, 'ITA', 'aegean')]).toEqual([0, 1]);
      // Off: fed by Italy's paper reach. On: not, at once (no fortress, no stores).
      expect([on, fed(g, 'GER', 'Rhodes'), c.supplied]).toEqual([on, !on, !on]);
    }
  });

  it('is off on the old chart with Units decide alone, on with its sub-flag, and set by hand only under Units decide', () => {
    // Bench only: stage.json keeps it off, and a campaign opens without the flag.
    expect([UNITS_STAGE, TRUEREACH_STAGE]).toEqual([false, false]);
    const g = startOpen(createGame(1, null));
    expect([trueReach(g), g.flags.trueReach]).toEqual([false, undefined]);
    // Refused without Units decide.
    expect(setTrueReach(g, true)).toBe(false);
    expect([trueReach(g), g.flags.trueReach]).toEqual([false, undefined]);
    expect(setUnitsDecide(g, true)).toBe(true);
    expect(trueReach(g)).toBe(false);
    g.supplyDirty = false;
    expect(setTrueReach(g, true)).toBe(true);
    expect([trueReach(g), g.flags.trueReach, g.supplyDirty]).toEqual([true, 1, true]);
    g.supplyDirty = false;
    expect(setTrueReach(g, false)).toBe(true);
    expect([trueReach(g), g.flags.trueReach, g.supplyDirty]).toEqual([false, undefined, true]);
    // The reading follows the gate: Malta by Norway is fed under Units decide alone, and not once the sub-flag is set
    // (the fortress then lives on the stores of the last month a ship came in, and goes hungry).
    const m = malta(false);
    for (let k = 0; k < 4; k++) { m.g.turn++; computeSupply(m.g); }
    expect([m.g.turn, m.c.supplied, m.g.flags[`stores:${m.r.id}`]]).toEqual([5, true, 5]);
    expect(setTrueReach(m.g, true)).toBe(true);
    const keep = portKeep(m.g, m.r);
    for (let k = 1; k <= keep + 1; k++) {
      m.g.turn++; computeSupply(m.g);
      expect([m.g.turn, m.c.supplied, m.g.flags[`stores:${m.r.id}`]]).toEqual([5 + k, k <= keep, 5]);
    }
  });

  it('gives the same answers from kept pictures as from fresh ones while a sea closes and opens again to the nation fed alone', () => {
    // Britain's fleet in port, across the Aegean, and in port again: the Aegean closes to Germany and not to Italy, so
    // Italy's own lanes are kept from call to call while what its ships can carry for Germany changes.
    const stage = (g: Game): void => { addCorps(g, 'GER', 'Rhodes test', 'inf', 1, 'Rhodes', {}); };
    const g = war(true);
    stage(g);
    const home = g.fleets.filter((f) => f.nat === 'UK').map((f) => [f.zone, f.stance] as const);
    const at = (h: Game, out: boolean): void => {
      if (out) patrol(h, 'UK', 'aegean');
      else h.fleets.filter((f) => f.nat === 'UK').forEach((f, i) => { f.zone = home[i][0]; f.stance = home[i][1]; });
    };
    const fresh = (out: boolean): Game => { const h = war(true); stage(h); at(h, out); computeSupply(h); return h; };
    for (const out of [false, true, false, true, false]) {
      at(g, out);
      computeSupply(g);
      expect([out, fed(g, 'GER', 'Rhodes')]).toEqual([out, !out]);
      const f = fresh(out);
      for (const nat of ['GER', 'ITA', 'UK']) expect([out, nat, [...g.supply[nat]]]).toEqual([out, nat, [...f.supply[nat]]]);
    }
  });
});
