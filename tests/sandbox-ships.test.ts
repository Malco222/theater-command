// The sandbox at sea (Brian, 19 November 2026: "a sandbox to play and make your own history, not just play out the
// real one"). Behind one switch (g.flags.sandbox): ships of classes of their own, named and kept on their fleet's list
// (hulls.ts); refits in the yards; the drawing board; and the computer's navies building to their style. With the
// switch off nothing of it is offered and a fleet's numbers are the sums they always were.
import { describe, expect, it } from 'vitest';
import { ARSENAL, offers, orderOffer, type Offer } from '../src/campaign/arsenal';
import { cancelBuild, deliver } from '../src/campaign/economy';
import { detach } from '../src/campaign/fleets';
import { createGame } from '../src/campaign/game';
import { aaCut, decks, listed, loseShips, lossWeight, moveShips, standard, worth, writeFallen } from '../src/campaign/hulls';
import { NAVY, fleetsOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import {
  PRESETS, againstStandard, aiClasses, designCard, drawDesign, nameOrder, orderRefit, refitOffers, shipsMonth, subCard, type Dials, type SubDials,
} from '../src/campaign/sandbox';
import { SANDBOX_SHIPS } from '../src/campaign/sandbox-data';
import { startOpen } from '../src/campaign/staff';
import { techOf } from '../src/campaign/tech';
import { beginHuman, carrierDeck, endHuman, humanUp, planCarrierDeck, runTurnAI, stepAI } from '../src/campaign/turn';
import { airPool, airUpkeep, assessAir, shipHits } from '../src/campaign/air';
import { deckView, setDeckPlan, syncWings, wingsOf } from '../src/campaign/carriers';
import { atSea, sail, zoneOf } from '../src/campaign/navy';
import type { Build, Fleet, Game, ShipClass, ShipHull } from '../src/campaign/types';
import type { ClassRow } from '../src/campaign/classes';

const KINDS: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const sandbox = (seed = 3, hist?: number): Game => { const g = grand(seed); g.flags.sandbox = 1; if (hist !== undefined) g.flags.hist = hist; return g; };
const offer = (g: Game, nat: string, key: string, sel = -1): Offer | undefined => offers(g, nat, sel).find((o) => o.key === key);
const rich = (g: Game, nat: string, pp = 2000): void => { g.nations[nat].pp = pp; };
const learn = (g: Game, nat: string, ...ids: string[]): void => { const t = techOf(g, nat)!; for (const id of ids) if (!t.done.includes(id)) t.done.push(id); };
/** Brings an order due at once and hands it over. */
const arrive = (g: Game, nat: string, b: Build | null): void => { expect(b).not.toBeNull(); b!.eta = g.turn; deliver(g, nat); };
const hullNamed = (g: Game, name: string): { f: Fleet; h: ShipHull } | null => {
  for (const f of g.fleets) for (const h of f.hulls ?? []) if (h.name === name) return { f, h };
  return null;
};
/** A fleet of the nation's in harbour at a home yard big enough for battleships, with one battleship at least. */
const atTheYard = (g: Game, nat: string, yard: string): Fleet => {
  const f = fleetsOf(g, nat).find((x) => x.ships.bb > 0)!;
  f.stance = 'port';
  f.base = g.byName[yard].id;
  return f;
};
const designsOf = (g: Game, nat: string): ClassRow[] | undefined => (g.nations[nat] as unknown as { designs?: ClassRow[] }).designs;
const hull = (over: Partial<ShipHull>): ShipHull => ({ cls: 'raider', kind: 'ca', label: 'Raider cruiser', name: 'Test', n: 1, born: 0, power: 3.5, tough: 0.8, escort: 0.3, raid: 3, ...over });

describe('with the switch off', () => {
  it('offers no class, no refit and no drawing board, and the computer keeps to its programme', () => {
    const g = grand();
    rich(g, 'UK');
    expect(offers(g, 'UK').filter((o) => o.key.split(':').length > 2)).toEqual([]);
    expect(refitOffers(g, 'UK', fleetsOf(g, 'UK')[0])).toEqual([]);
    expect(drawDesign(g, 'UK', PRESETS.ca, 'Test').why).toMatch(/sandbox/);
    const wants = ['ship:ss', 'ship:cv', 'air:ftr'];
    for (let t = 0; t < 24; t++) { g.turn = t; expect(aiClasses(g, 'GER', wants)).toBe(wants); }
    expect(g.fleets.some((f) => f.hulls)).toBe(false);
  });

  it('a fleet with no list is worth exactly the old sums', () => {
    const g = grand();
    for (const f of g.fleets) for (const c of KINDS) {
      for (const k of ['power', 'tough', 'escort', 'raid'] as const) expect(worth(f, c, k)).toBe(f.ships[c] * NAVY.classes[c][k]);
      expect(lossWeight(f, c, 0.37)).toBe(f.ships[c] * 0.37 * NAVY.classes[c].tough);
    }
    for (const f of g.fleets) { expect(decks(f)).toBe(f.ships.cv); expect(aaCut(f, 0.3)).toBe(0); }
  });
});

describe('a fleet\'s list of named ships (hulls.ts)', () => {
  it('counts each ship on the list at her own numbers and the rest at the standard ship\'s', () => {
    const g = grand();
    const f = fleetsOf(g, 'UK').find((x) => x.ships.ca >= 2)!;
    const n = f.ships.ca;
    f.hulls = [hull({ name: 'One' })];
    expect(listed(f, 'ca')).toBe(1);
    expect(standard(f, 'ca')).toBe(n - 1);
    expect(worth(f, 'ca', 'raid')).toBeCloseTo((n - 1) * NAVY.classes.ca.raid + 3, 9);
    expect(lossWeight(f, 'ca', 0.5)).toBeCloseTo(((n - 1) * NAVY.classes.ca.tough + 0.8) * 0.5, 9);
    f.hulls.push(hull({ name: 'Two', cls: 'aacruiser', aa: 0.08 }), hull({ name: 'Three', cls: 'aacruiser', aa: 0.08 }));
    f.ships.ca = Math.max(f.ships.ca, 3);
    expect(aaCut(f, 0.3)).toBeCloseTo(0.16, 9);
    for (let i = 4; i < 10; i++) f.hulls.push(hull({ name: `Extra ${i}`, cls: 'aacruiser', aa: 0.08 }));
    f.ships.ca = 10;
    expect(aaCut(f, 0.3)).toBe(0.3);
  });

  it('loses ships in fair shares, writes the named ones on the memorial and never lists more than the count', () => {
    const g = grand();
    const f = fleetsOf(g, 'UK').find((x) => x.ships.ca >= 2)!;
    f.ships.ca = 6;
    f.hulls = [hull({ name: 'Alpha' }), hull({ name: 'Beta', tough: 0.1 })];
    let fell = 0;
    for (let k = 0; k < 6; k++) {
      loseShips(f, 'ca', 1);
      expect(listed(f, 'ca')).toBeLessThanOrEqual(f.ships.ca);
      fell += writeFallen(g, f, 'in a test').length;
    }
    expect(f.ships.ca).toBe(0);
    expect(f.hulls).toBeUndefined();
    expect(fell).toBe(2);
    const wall = g.nations.UK.memorial!;
    expect(wall.map((m) => m.name).sort()).toEqual(['Alpha', 'Beta']);
    expect(wall.every((m) => m.how === 'in a test' && m.turn === g.turn)).toBe(true);
  });

  it('a fleet split sends the ships the player picked by name, and its standard ships before its own classes', () => {
    const g = grand();
    const f = fleetsOf(g, 'UK').find((x) => x.ships.ca >= 3)!;
    f.stance = 'port';
    f.hulls = [hull({ name: 'Stays' }), hull({ name: 'Goes' })];
    const a = detach(g, f, { ca: 1 }, undefined, ['Goes'])!;
    expect(a).not.toBeNull();
    expect(a.hulls?.map((h) => h.name)).toEqual(['Goes']);
    expect(f.hulls?.map((h) => h.name)).toEqual(['Stays']);
    const b = detach(g, f, { ca: 1 })!;
    expect(b.hulls).toBeUndefined();
    expect(f.hulls?.map((h) => h.name)).toEqual(['Stays']);
    // Ships moved by count with nothing picked: the list follows only when it must.
    moveShips(f, b, 'ca', f.ships.ca - 1);
    expect(f.hulls?.map((h) => h.name)).toEqual(['Stays']);
    moveShips(f, b, 'ca', 1);
    expect(b.hulls?.map((h) => h.name)).toEqual(['Stays']);
    expect(f.hulls).toBeUndefined();
  });
});

describe('the Navy column with the switch on', () => {
  it('lists each class under its kind, and a ship of one is laid down named and joins her fleet by name', () => {
    const g = sandbox();
    rich(g, 'UK');
    // Each class waits on its own technology (tech.json, the naval what-ifs).
    expect(offer(g, 'UK', 'ship:cv:escortcv')!.why).toMatch(/Escort carriers/);
    expect(offer(g, 'UK', 'ship:cv:escortcv')!.blurb).toMatch(/33% of a fleet carrier's air wing/);
    learn(g, 'UK', 'escortcarriers');
    const keys = offers(g, 'UK').map((o) => o.key);
    const cv = keys.indexOf('ship:cv');
    expect(keys.slice(cv + 1, cv + 3).sort()).toEqual(['ship:cv:battlecv', 'ship:cv:escortcv']);
    const o = offer(g, 'UK', 'ship:cv:escortcv')!;
    expect(o.why).toBeNull();
    expect(o.cost).toBeLessThan(offer(g, 'UK', 'ship:cv')!.cost);
    expect(o.note).toMatch(/counts as a carrier/);
    const b = orderOffer(g, 'UK', 'ship:cv:escortcv')!;
    expect(b.cls).toBe('escortcv');
    expect(b.count).toBe(1);
    expect(b.ship).toBeTruthy();
    // Named after one of the nation's own cities.
    expect(g.regions.some((r) => r.name === b.ship && r.ctrl === 'UK')).toBe(true);
    const before = fleetsOf(g, 'UK').reduce((s, f) => s + f.ships.cv, 0);
    arrive(g, 'UK', b);
    expect(fleetsOf(g, 'UK').reduce((s, f) => s + f.ships.cv, 0)).toBe(before + 1);
    const { f, h } = hullNamed(g, b.ship!)!;
    expect(h).toMatchObject({ cls: 'escortcv', kind: 'cv', n: 1, deck: 0.33, born: g.turn });
    expect(decks(f)).toBeCloseTo(f.ships.cv - 1 + 0.33, 9);
  });

  it('takes the player\'s own name for a ship, and never the same name twice', () => {
    const g = sandbox();
    rich(g, 'UK');
    learn(g, 'UK', 'commerceraiders');
    const a = orderOffer(g, 'UK', 'ship:ca:raider', -1, 'Northern Star')!;
    expect(a.ship).toBe('Northern Star');
    const b = orderOffer(g, 'UK', 'ship:ca:raider')!;
    expect(b.ship).not.toBe('Northern Star');
    expect(nameOrder(g, 'UK', b.id, 'Northern Star')).toBe(false);
    expect(nameOrder(g, 'UK', b.id, 'Evening Star')).toBe(true);
    expect(b.ship).toBe('Evening Star');
    expect(b.name).toMatch(/^The Evening Star, raider cruiser, at /);
  });

  it('keeps a small navy\'s battleship and carrier classes behind plans (ruling 235), and its cruisers open', () => {
    const g = sandbox();
    rich(g, 'POL');
    expect(offer(g, 'POL', 'ship:bb:superbb')?.why).toMatch(/great powers/);
    expect(offer(g, 'POL', 'ship:cv:escortcv')?.why).toMatch(/great powers/);
    expect(offer(g, 'POL', 'ship:ca:raider')?.why ?? '').not.toMatch(/great powers/);
  });

  it('keeps a carrier class behind carrier aviation, as the standard carrier is', () => {
    const g = sandbox();
    rich(g, 'GER');
    const std = offer(g, 'GER', 'ship:cv')!;
    expect(std.tech).toBe('carriers');
    learn(g, 'GER', 'escortcarriers');
    expect(offer(g, 'GER', 'ship:cv:escortcv')!.why).toMatch(/Carrier aviation/);
    learn(g, 'GER', 'carriers');
    expect(offer(g, 'GER', 'ship:cv:escortcv')!.why).toBeNull();
  });
});

describe('refits', () => {
  it('a ship goes to the yard, can be called back the month she went, and comes back with her refit', () => {
    const g = sandbox();
    rich(g, 'UK');
    const f = atTheYard(g, 'UK', 'Clydeside');
    const bb = f.ships.bb;
    const key = `refit:${f.id}:std:bb:aa`;
    const line = refitOffers(g, 'UK', f).find((o) => o.key === key)!;
    expect(line.why).toBeNull();
    expect(line.cost).toBeCloseTo(offer(g, 'UK', 'ship:bb')!.cost * 0.1, 1);
    const pp = g.nations.UK.pp;
    let b = orderRefit(g, 'UK', f.id, 'std:bb', 'aa')!;
    expect(b.refit!.hull.name).toBeTruthy();
    expect(f.ships.bb).toBe(bb - 1);
    expect(g.nations.UK.pp).toBeCloseTo(pp - line.cost, 6);
    // Called back: she rejoins as she was, one of the standard ships again, and the money comes back.
    expect(cancelBuild(g, 'UK', b.id)).toBe(true);
    expect(f.ships.bb).toBe(bb);
    expect(f.hulls).toBeUndefined();
    expect(g.nations.UK.pp).toBeCloseTo(pp, 6);
    b = orderRefit(g, 'UK', f.id, 'std:bb', 'aa')!;
    const name = b.refit!.hull.name;
    expect(b.eta - g.turn).toBe(3);
    arrive(g, 'UK', b);
    expect(f.ships.bb).toBe(bb);
    const { h } = hullNamed(g, name)!;
    expect(h).toMatchObject({ kind: 'bb', refits: ['aa'], aa: 0.03 });
    expect(h.escort).toBeCloseTo(NAVY.classes.bb.escort + 0.1, 9);
    expect(aaCut(f, SANDBOX_SHIPS.aaMost)).toBeCloseTo(0.03, 9);
    // Once only; and only in harbour.
    expect(refitOffers(g, 'UK', f).find((o) => o.key === `refit:${f.id}:${name}:aa`)!.why).toMatch(/already/);
    f.stance = 'patrol' as Fleet['stance'];
    expect(refitOffers(g, 'UK', f).find((o) => o.key === `refit:${f.id}:${name}:radar`)!.why).toMatch(/harbour/);
  });

  it('a battleship rebuilt as a carrier counts as a carrier from the day she leaves the yard', () => {
    const g = sandbox();
    rich(g, 'UK');
    const f = atTheYard(g, 'UK', 'Clydeside');
    const bb = f.ships.bb, cv = f.ships.cv;
    const b = orderRefit(g, 'UK', f.id, 'std:bb', 'carrier')!;
    expect(b).not.toBeNull();
    arrive(g, 'UK', b);
    expect(f.ships.bb).toBe(bb - 1);
    expect(f.ships.cv).toBe(cv + 1);
    expect(hullNamed(g, b.refit!.hull.name)!.h).toMatchObject({ kind: 'cv', cls: 'cv', refits: ['carrier'] });
  });
});

describe('the drawing board', () => {
  it('prices the standard dials near the standard ships', () => {
    const g = sandbox();
    for (const k of ['bb', 'cv', 'ca', 'dd'] as const) {
      const card = designCard(g, 'UK', PRESETS[k] as Dials);
      expect(card.kind, k).toBe(k);
      expect(card.why, k).toBeNull();
      const ratio = card.row.cost / ARSENAL.ships[k].cost;
      expect(ratio, `${k} costs ${card.row.cost}`).toBeGreaterThan(0.7);
      expect(ratio, `${k} costs ${card.row.cost}`).toBeLessThan(1.4);
    }
    const sub = subCard(g, 'UK', { size: 1, tubes: 0, range: 1, hangar: 0 });
    expect(sub.row.count).toBe(4);
    expect(sub.row.cost / ARSENAL.ships.ss.cost).toBeGreaterThan(0.7);
    expect(sub.row.cost / ARSENAL.ships.ss.cost).toBeLessThan(1.4);
  });

  it('never lets a design beat the standard ship of her kind for the money by more than a tenth', () => {
    const g = sandbox();
    const b = SANDBOX_SHIPS.board;
    let seen = 0, capped = 0;
    for (let hull = 0; hull < b.hull.length; hull++) for (let guns = 0; guns < b.guns.length; guns++) for (let turrets = 0; turrets <= 4; turrets++)
      for (let armour = 0; armour < 3; armour++) for (let speed = 0; speed < 3; speed++) for (let aa = 0; aa < 3; aa++) for (let air = 0; air < 4; air++) {
        const card = designCard(g, 'UK', { hull, guns, turrets, armour, speed, aa, air });
        if (card.why) continue;
        seen++;
        if (card.capped) capped++;
        expect(againstStandard(card.row), JSON.stringify(card.row.dials)).toBeLessThanOrEqual(b.value + 0.01);
      }
    for (let size = 0; size < 3; size++) for (let tubes = 0; tubes < 3; tubes++) for (let range = 0; range < 3; range++) for (let hangar = 0; hangar < 2; hangar++) {
      const card = subCard(g, 'UK', { size, tubes, range, hangar });
      if (!card.why) expect(againstStandard(card.row)).toBeLessThanOrEqual(b.value + 0.01);
    }
    expect(seen).toBeGreaterThan(300);
    expect(capped).toBeGreaterThan(0);
  });

  it('says why a design will not do', () => {
    const g = sandbox();
    expect(designCard(g, 'UK', { hull: 0, guns: 4, turrets: 4, armour: 2, speed: 2, aa: 2, air: 0 }).why).toMatch(/will not fit/);
    expect(designCard(g, 'UK', { hull: 1, guns: 0, turrets: 0, armour: 0, speed: 1, aa: 0, air: 0 }).why).toMatch(/guns or a flight deck/);
    expect(subCard(g, 'UK', { size: 0, tubes: 0, range: 0, hangar: 1 }).why).toMatch(/cruiser submarine/);
  });

  it('a design signed for is ordered like any class, and a flotilla of boats comes under one name', () => {
    const g = sandbox();
    rich(g, 'UK');
    const { row, why } = drawDesign(g, 'UK', { size: 1, tubes: 2, range: 2, hangar: 0 } as SubDials, 'Long reach');
    expect(why).toBeNull();
    expect(row!.id).toBe('own-uk-1');
    expect(drawDesign(g, 'UK', PRESETS.ca, 'long reach').why).toMatch(/already/);
    const o = offer(g, 'UK', 'ship:ss:own-uk-1')!;
    expect(o.why).toBeNull();
    expect(o.size).toMatch(/flotilla of 4/);
    const b = orderOffer(g, 'UK', 'ship:ss:own-uk-1')!;
    expect(b.count).toBe(4);
    expect(b.ship).toBe('1st Long reach Flotilla');
    arrive(g, 'UK', b);
    expect(hullNamed(g, '1st Long reach Flotilla')!.h).toMatchObject({ kind: 'ss', n: 4, cls: 'own-uk-1' });
    // The save keeps the list, the designs and the memorial.
    const h = loadGame(saveGame(g))!;
    expect(designsOf(h, 'UK')).toEqual(designsOf(g, 'UK'));
    expect(hullNamed(h, '1st Long reach Flotilla')!.h.n).toBe(4);
  });
});

describe('the computer\'s navies', () => {
  it('keep to history\'s ships at the history stops, and build to their style at the free ones', () => {
    const wants = ['ship:ss', 'ship:cv', 'air:ftr'];
    for (const stop of [1, 2]) {
      const g = sandbox(3, stop);
      for (let t = 0; t < 36; t++) { g.turn = t; expect(aiClasses(g, 'GER', wants)).toEqual(wants); }
    }
    const g = sandbox(3, 5);
    learn(g, 'GER', 'commerceraiders', 'aircraftsubs', 'carriers');
    learn(g, 'USA', 'escortcarriers', 'battlecarriers');
    const seen = new Set<string>();
    for (let t = 0; t < 36; t++) { g.turn = t; for (const k of aiClasses(g, 'GER', wants)) seen.add(k); for (const k of aiClasses(g, 'USA', wants)) seen.add(`USA ${k}`); }
    // Germany leans to the trade war (a raider, a scout submarine), the United States to carriers.
    expect([...seen].some((k) => /^ship:(ss:scoutsub|ca:raider)$/.test(k))).toBe(true);
    expect([...seen].some((k) => /^USA ship:cv:(battlecv|escortcv)$/.test(k))).toBe(true);
    expect([...seen].some((k) => k.startsWith('USA') && k.includes('superbb'))).toBe(false);
  });

  it('draw a class of their own once a year at the loosest stops', () => {
    const g = sandbox(3, 5);
    g.turn = 6;
    const n = g.nations.GER;
    if (!n.atWar.length) n.atWar.push('POL');
    shipsMonth(g, 'GER');
    expect(designsOf(g, 'GER')?.length).toBe(1);
    expect(designsOf(g, 'GER')![0]).toMatchObject({ own: true, id: 'own-ger-1' });
    const h = sandbox(3, 3);
    h.turn = 6;
    shipsMonth(h, 'GER');
    expect(designsOf(h, 'GER')).toBeUndefined();
  });

  it('a sandbox war plays on with every list inside its count', () => {
    const g = sandbox(17, 5);
    for (let m = 0; m < 18; m++) {
      runTurnAI(g);
      for (const f of g.fleets) for (const c of KINDS) expect(listed(f, c), `${f.name} ${c}`).toBeLessThanOrEqual(f.ships[c]);
    }
    for (const n of Object.values(g.nations)) for (const m of n.memorial ?? []) expect(m.name).toBeTruthy();
  }, 120000);
});

// ---- Carrier decks (Brian, 19 November 2026: "treat carriers as airfields, you can pick which squadrons to assign to them") ----

/** A grand campaign with Britain at the table, played until she is at war with Germany and it is her month. */
function britain(seed = 3, on = true): Game {
  const g = startOpen(createGame(seed, 'UK'));
  if (on) g.flags.sandbox = 1;
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
const wingOf = (g: Game, f: Fleet, kind: 'ftr' | 'bmr') => wingsOf(g, f.id).find((w) => w.kind === kind);
/** The Home Fleet in its harbour with four carriers and their wings aboard. */
function homeFleet(g: Game): Fleet {
  const f = named(g, 'Home Fleet');
  f.ships.cv = 4;
  sail(g, f, f.home, 'port');
  expect(atSea(f)).toBe(false);
  syncWings(g);
  return f;
}

describe('carrier decks (the sandbox)', () => {
  it('with the switch off there is no deck plan, and a strike is the one it always was', () => {
    const g = britain(3, false);
    const f = homeFleet(g);
    expect(deckView(g, 'UK', f)).toBeNull();
    expect(carrierDeck(g, f.id)).toBeNull();
    expect(setDeckPlan(g, 'UK', f.id, { ftr: 12, dive: 0, torp: 0 })).toBe(false);
    expect(f.deckPlan).toBeUndefined();
  }, 60000);

  it('fills the decks with the navy\'s own mix until the player says otherwise', () => {
    const g = britain();
    const f = homeFleet(g);
    const v = carrierDeck(g, f.id)!;
    // Four British carriers hold 4 x 36 aircraft: twelve squadrons, a third of them fighters.
    expect(v.squadrons).toBe(12);
    expect(v.own).toBe(false);
    expect(v.plan.ftr + v.plan.dive + v.plan.torp).toBe(12);
    expect(v.plan.ftr).toBe(4);
    expect(v.plan.torp).toBeGreaterThan(v.plan.dive);
    expect(v.want).toEqual({ ftr: 48, bmr: 96 });
    expect(v.aboard).toEqual(v.want);
    expect(v.why).toBeNull();
  }, 60000);

  it('changes the decks over in harbour: what no longer fits goes to the reserve, and what is wanted comes from it', () => {
    const g = britain();
    const f = homeFleet(g);
    const p = airPool(g, 'UK');
    p.ftr = 30; p.pilots = 30;
    const bmr = p.bmr;
    // Eight squadrons of fighters, four of dive bombers, no torpedo planes.
    expect(planCarrierDeck(g, f.id, { ftr: 8, dive: 4, torp: 0 })).toBe(true);
    expect(f.deckPlan).toEqual({ ftr: 8 / 12, dive: 4 / 12, torp: 0 });
    // The fighter wing wants 96 and the reserve had 30 to give; the strike wing gives back 48 of its 96.
    expect(wingOf(g, f, 'ftr')!.est.ftr).toBe(96);
    expect(wingOf(g, f, 'ftr')!.ftr).toBe(78);
    expect(p.ftr).toBe(0);
    expect(wingOf(g, f, 'bmr')!.bmr).toBe(48);
    expect(p.bmr).toBe(bmr + 48);
    expect(carrierDeck(g, f.id)!.torp).toBe(0);
    // The rest come as the decks are made up in port.
    p.ftr = 100; p.pilots = 100;
    airUpkeep(g, 'UK');
    expect(wingOf(g, f, 'ftr')!.ftr).toBe(96);
    // A plan that does not add up to the decks is refused.
    expect(planCarrierDeck(g, f.id, { ftr: 5, dive: 5, torp: 5 })).toBe(false);
    expect(planCarrierDeck(g, f.id, { ftr: 12.5, dive: 0, torp: -0.5 })).toBe(false);
  }, 60000);

  it('all fighters: the strike wing goes ashore to the reserve, and comes back when the plan wants it', () => {
    const g = britain();
    const f = homeFleet(g);
    const p = airPool(g, 'UK');
    const bmr = p.bmr;
    p.ftr = 500; p.pilots = 500;
    expect(planCarrierDeck(g, f.id, { ftr: 12, dive: 0, torp: 0 })).toBe(true);
    expect(wingOf(g, f, 'bmr')).toBeUndefined();
    expect(p.bmr).toBe(bmr + 96);
    syncWings(g);
    expect(wingOf(g, f, 'bmr')).toBeUndefined();
    expect(planCarrierDeck(g, f.id, { ftr: 4, dive: 4, torp: 4 })).toBe(true);
    expect(wingOf(g, f, 'bmr')!.bmr).toBe(96);
  }, 60000);

  it('changes nothing at sea, and keeps the plan through a split and a save', () => {
    const g = britain();
    const f = homeFleet(g);
    expect(planCarrierDeck(g, f.id, { ftr: 6, dive: 0, torp: 6 })).toBe(true);
    const half = detach(g, f, { cv: 2 })!;
    expect(half.deckPlan).toEqual(f.deckPlan);
    expect(wingOf(g, half, 'ftr')!.est.ftr).toBe(36);
    f.zone = zoneOf(g, 'natl'); f.stance = 'patrol';
    expect(carrierDeck(g, f.id)!.why).toMatch(/at sea/);
    expect(planCarrierDeck(g, f.id, { ftr: 2, dive: 2, torp: 2 })).toBe(false);
    const h = loadGame(saveGame(g))!;
    expect(named(h, 'Home Fleet').deckPlan).toEqual(f.deckPlan);
  }, 60000);

  it('torpedo planes do more harm to battleships and lose more to the guns; dive bombers go surer for carriers', () => {
    const strikeWith = (sq: { ftr: number; dive: number; torp: number }) => {
      const g = britain();
      const home = homeFleet(g);
      airPool(g, 'UK').bmr = 1000;
      expect(planCarrierDeck(g, home.id, sq)).toBe(true);
      const foe = named(g, 'Battle Fleet');
      foe.ships.cv = Math.max(1, foe.ships.cv);
      for (const x of [home, foe]) { x.zone = zoneOf(g, 'natl'); x.stance = 'patrol'; x.lane = undefined; }
      syncWings(g);
      const a = assessAir(g, [wingOf(g, home, 'bmr')!], 'fleet', foe.id);
      expect(a.why).toBeNull();
      return { a, hit: shipHits(a, 'fleet', 60) };
    };
    const torp = strikeWith({ ftr: 4, dive: 0, torp: 8 });
    const dive = strikeWith({ ftr: 4, dive: 8, torp: 0 });
    expect(torp.a.torp).toBe(1);
    expect(dive.a.torp).toBe(0);
    expect(torp.hit.sunk.bb).toBeGreaterThan(dive.hit.sunk.bb);
    expect(dive.hit.sunk.cv).toBeGreaterThan(torp.hit.sunk.cv);
    expect(torp.a.flak).toBeGreaterThan(dive.a.flak);
    expect(torp.a.lines.some((l) => l.label === 'Strike aircraft' && /100% torpedo planes/.test(String(l.value)))).toBe(true);
  }, 120000);
});
