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
import { addCorps, capture, computeSupply, createGame, declareWar, portKeep, setTrueReach, trueReach } from '../src/campaign/game';
import { SeaPicture, openSeas, zoneOf } from '../src/campaign/navy';
import { besieged, fedBySea, feeders, feedingSeas, garrison, garrisonFed, garrisonOf, monthsShut, ringClosed, siegeMonth, siegeView, storesOf } from '../src/campaign/siege';
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

  // A besieged port town (the design director's ruling of 17:35Z, 25 November 2026): one town, one larder, one reach,
  // and the reach is the holder's. The town is fed when the ships of one of its feeders, carrying the holder's stores,
  // reach a sea off the harbour by the supply picture's own lanes (siege.ts fedBySea): the whole voyage, not the sea
  // off the quay alone. The harbours of Tobruk and Bordeaux lie inside the walls, so ships may still use them under siege.
  /** A town of the old chart closely invested: the corps inside (a corps sent into a town with the enemy round it goes in at once), and corps of the besieger's put round it. */
  const besiege = (g: Game, town: string, inside: string[], by: string, ring = 1) => {
    const r = g.byName[town];
    const cs = inside.map((nat) => { const c = addCorps(g, nat, `${town} test`, 'inf', 1, town, {}); if (!c.town) expect(garrison(g, c)).toBe(true); return c; });
    for (let i = 0; i < ring; i++) {
      const s = addCorps(g, by, 'Besieger test', 'inf', 1, g.regions[g.nations[by].capital].name, {});
      g.regions[s.reg].units.splice(g.regions[s.reg].units.indexOf(s), 1);
      s.reg = r.id; r.units.push(s);
    }
    g.flags[`siege:${r.id}`] = g.turn;
    g.supplyDirty = true;
    computeSupply(g);
    expect([besieged(g, r), garrisonOf(r).map((x) => x.nat)]).toEqual([true, inside]);
    return { g, r, cs, pic: new SeaPicture(g) };
  };
  /** True where the siege card says ships still reach the harbour. */
  const card = (g: Game, id: number): boolean => siegeView(g, id)!.lines.some((l) => l.startsWith('Ships still reach the harbour'));

  /** Italy's Tobruk with a German corps inside and no other, a Soviet corps round it, its stores long eaten; the Libyan Sea shut by one nation's fleets. */
  const tobruk = (on: boolean, shutter: string, foe: string) => {
    const g = war(on);
    const libyan = zoneOf(g, 'libyan');
    if (foe !== 'none') declareWar(g, shutter, foe);
    patrol(g, shutter, 'libyan');
    if (shutter !== 'SOV') declareWar(g, 'ITA', 'SOV');
    const { r, cs: [c] } = besiege(g, 'Tobruk', ['GER'], 'SOV');
    // Its stores long eaten: only ships keep the garrison fed.
    g.flags[`sgm:${r.id}`] = 99;
    computeSupply(g);
    expect([r.ctrl, r.seas, feeders(g, r)]).toEqual(['ITA', [libyan], ['ITA', 'GER']]);
    return { g, r, c, libyan, pic: new SeaPicture(g) };
  };

  /**
   * France's Bordeaux with a British corps inside, three German corps round it: the war as it opens, no side's
   * alignment touched. France has no ships of her own to send, so Britain is the town's one feeder. Germany's fleets
   * lie across the Western Approaches (one) and the Iberian Coast (two), the Allied fleets far off in the eastern
   * Mediterranean: the Bay of Biscay off the harbour is open to Britain and France, and no British ship can sail there.
   */
  const bordeaux = (on: boolean) => {
    const g = war(on);
    const ger = g.fleets.filter((f) => f.nat === 'GER');
    ger.forEach((f, i) => { f.zone = zoneOf(g, i === 0 ? 'wapp' : 'iberia'); f.stance = 'patrol'; });
    for (const f of g.fleets) if (f.nat === 'UK' || f.nat === 'FRA') { f.zone = zoneOf(g, 'emed'); f.stance = 'patrol'; }
    const { r, cs: [c], pic } = besiege(g, 'Bordeaux', ['UK'], 'GER', 3);
    return { g, r, c, pic };
  };

  it('feeds a besieged town by its holder\'s reach, whoever is inside: Italy\'s Tobruk held by a German corps alone eats from Italy\'s larder', () => {
    for (const on of [false, true]) {
      // Italy at peace with Britain, whose fleet lies across the Libyan Sea: closed to Germany and not to Italy.
      const { g, r, c, libyan, pic } = tobruk(on, 'UK', 'none');
      expect([pic.closed(libyan, 'GER'), pic.closed(libyan, 'ITA')]).toEqual([true, false]);
      // The supply picture has no source at Tobruk for Germany, on or off: the garrison is fed by the town's reckoning.
      expect(fed(g, 'GER', 'Tobruk')).toBe(false);
      // On and off the town is fed: ships that can carry stores there for its holder come in, and the guest eats from
      // them. The holder has no corps of its own inside and needs none: the town and its quay are the holder's.
      expect([on, fedBySea(g, r), feedingSeas(g, r), garrisonFed(g, r), c.supplied]).toEqual([on, true, [libyan], true, true]);
      expect([on, card(g, r.id)]).toEqual([on, true]);
    }
  });

  it('under true reach never feeds a besieged town through a sea shut to its holder, though a guest\'s ships could sail it: the same Tobruk with the sea shut to Italy and open to Germany (off: fed by paper, pinned)', () => {
    for (const on of [false, true]) {
      // The Soviet Union at war with Italy alone, its fleets across the Libyan Sea: closed to Italy, the holder, and
      // open to Germany, whose corps is inside and whose ships could sail it.
      const { g, r, c, libyan, pic } = tobruk(on, 'SOV', 'ITA');
      expect([pic.closed(libyan, 'ITA'), pic.closed(libyan, 'GER')]).toEqual([true, false]);
      // Off (today's answer, pinned): Germany's ships feed the town by paper. On: a sea shut to the holder is shut to
      // every ship carrying the town its stores, whatever the flag; the town starves, and its card says so.
      expect([on, fedBySea(g, r), feedingSeas(g, r), garrisonFed(g, r), c.supplied]).toEqual([on, !on, on ? [] : [libyan], !on, !on]);
      expect([on, card(g, r.id)]).toEqual([on, !on]);
    }
  });

  it('under true reach feeds a besieged port town only where the feeder\'s ships can sail all the way to it: Bordeaux by Britain, with every way into the Bay of Biscay shut (off: fed by paper, pinned)', () => {
    for (const on of [false, true]) {
      const { g, r, c, pic } = bordeaux(on);
      const biscay = zoneOf(g, 'biscay'), wapp = zoneOf(g, 'wapp'), iberia = zoneOf(g, 'iberia');
      // The ground both readings share: the sea off the harbour is open to the feeder and the holder, and the only
      // seas it can be reached by are shut to them. Britain's own lanes do not reach it, nor her lanes carrying
      // France's stores; France sends no ships.
      expect([r.ctrl, r.owner, r.seas, feeders(g, r), g.seas[biscay].adj]).toEqual(['FRA', 'FRA', [biscay], ['UK'], [wapp, iberia]]);
      expect([pic.closed(biscay, 'UK'), pic.closed(biscay, 'FRA'), pic.closed(wapp, 'UK'), pic.closed(iberia, 'UK')]).toEqual([false, false, true, true]);
      expect([reaches(g, 'UK', 'biscay'), openSeas(g, 'UK', pic, undefined, undefined, undefined, 'FRA')[biscay]]).toEqual([0, 0]);
      expect(ringClosed(g, r)).toBe(true);
      // The months of the siege, each opening France's month (siegeMonth) and drawing the picture: off, Britain's ships
      // feed the town by paper and its stores never run down; on, no ship comes in, the stores are eaten, and then the
      // British corps inside starves.
      const stores = storesOf(g, r);
      expect(stores).toBeGreaterThan(0);
      for (let k = 1; k <= stores + 2; k++) {
        g.turn++;
        siegeMonth(g, 'FRA');
        computeSupply(g);
        const eating = !on || k <= stores;
        expect([on, k, monthsShut(g, r), fedBySea(g, r), feedingSeas(g, r), garrisonFed(g, r), c.supplied, card(g, r.id)]).toEqual([on, k, on ? k : 0, !on, on ? [] : [biscay], eating, eating, !on]);
      }
    }
  });

  /**
   * Italy's Tobruk taken by Britain at war with Italy (capture), Italy's own corps still round it in the country
   * outside the walls, a British corps gone in; Britain's fleets across the Libyan Sea, which is shut to Italy, the
   * owner, and open to Britain, the holder. Its stores long eaten.
   */
  const taken = (on: boolean) => {
    const g = war(on);
    declareWar(g, 'UK', 'ITA');
    patrol(g, 'UK', 'libyan');
    capture(g, g.byName['Tobruk'], 'UK');
    const { r, cs: [c] } = besiege(g, 'Tobruk', ['UK'], 'ITA', 0);
    g.flags[`sgm:${r.id}`] = 99;
    computeSupply(g);
    return { g, r, c, libyan: zoneOf(g, 'libyan'), pic: new SeaPicture(g) };
  };

  it('reads the reach of the town\'s holder, not of its owner: Italy\'s Tobruk taken by Britain is fed by British ships through a sea shut to Italy', () => {
    for (const on of [false, true]) {
      const { g, r, c, libyan, pic } = taken(on);
      expect([r.ctrl, r.owner, r.seas, feeders(g, r)]).toEqual(['UK', 'ITA', [libyan], ['UK']]);
      expect([pic.closed(libyan, 'UK'), pic.closed(libyan, 'ITA')]).toEqual([false, true]);
      // Britain's ships reach the sea carrying Britain's stores, and not carrying Italy's.
      expect([reaches(g, 'UK', 'libyan'), openSeas(g, 'UK', pic, undefined, undefined, undefined, 'ITA')[libyan]]).toEqual([1, 0]);
      expect([on, fedBySea(g, r), feedingSeas(g, r), garrisonFed(g, r), c.supplied, card(g, r.id)]).toEqual([on, true, [libyan], true, true, true]);
    }
  });

  it('keeps the siege card, the garrison\'s eating and every corps inside in step, on and off, month after month, from kept lanes and fresh ones alike', () => {
    // Each town above, drawn under TC_SUPPLY_CHECK (which throws where a besieged town reads differently from the kept
    // lanes than from a fresh walk), over three further months of the supply picture.
    const cases: [string, (on: boolean) => { g: Game; r: Game['regions'][number] }][] = [
      ['Tobruk, the sea shut to the guest', (on) => tobruk(on, 'UK', 'none')],
      ['Tobruk, the sea shut to the holder', (on) => tobruk(on, 'SOV', 'ITA')],
      ['Tobruk taken by Britain', taken],
      ['Bordeaux, the way shut', (on) => { const b = bordeaux(on); b.g.flags[`sgm:${b.r.id}`] = 99; computeSupply(b.g); return b; }],
    ];
    for (const [name, make] of cases) for (const on of [false, true]) {
      const { g, r } = make(on);
      for (let k = 0; k <= 3; k++) {
        if (k) { g.turn++; computeSupply(g); }
        const eats = garrisonFed(g, r);
        expect([name, on, k, card(g, r.id), garrisonOf(r).map((x) => x.supplied)]).toEqual([name, on, k, fedBySea(g, r), garrisonOf(r).map(() => eats)]);
      }
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
