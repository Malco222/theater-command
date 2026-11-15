// A fair shot for every major (Version 69), the grand campaign only. The computer's staffs open a second front by
// sea on every level: a nation a year at war with an enemy its home army does not touch keeps a force by a quay
// with an open lane to his harbours, lands where a harbour is weakly held, stays ashore and is fed while the men
// there are too few. An army standing idle on a far shore does the same from there. Against it, a staff keeps a
// corps of its field army in each harbour of its homeland the enemy's ships can reach, and a computer-run nation
// that has run out of men puts its idle money into fortifications. A staff whose army is being worn away raises men
// for the line before guns, and its builders wait. History's scenarios use none of it.
import { describe, expect, it } from 'vitest';
import arsenalData from '../data/campaign/arsenal.json';
import openData from '../data/campaign/open.json';
import { runAI } from '../src/campaign/ai';
import { aiFortify } from '../src/campaign/arsenal';
import { PRODUCTION, aiProduction, armyWorn, corpsCost, fortCost, gunsWanted, isHome, orderFort } from '../src/campaign/economy';
import { addCorps, armySize, atWar, createGame, removeCorps } from '../src/campaign/game';
import { menSpare } from '../src/campaign/manpower';
import { landingTargets } from '../src/campaign/navy';
import { pocketMonth } from '../src/campaign/siege';
import { coastRules, coastWatch, lodgment, seaBase, seaFoe, staffPhase, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const OPEN = openData as unknown as {
  coast: { share: number; least: number; each: number; works: number; dig: { orders: number; top: number; capital: number; keep: number } };
  descent: { after: number; abroad: number; hold: number; feed: number; force: number; every: number; chance: number };
};
const D = OPEN.descent;

/** A war the computer plays on every side, as it stands once Poland has fallen. */
const opened = (human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
  return g;
};
/** The same war after a quiet year and more in the west: the calendar moved on, every corps rested. */
const quietYear = (human: string | null = null): Game => {
  const g = opened(human);
  g.turn += D.after + 1;
  for (const c of g.corps) c.ap = c.apMax;
  return g;
};
const corps = (g: Game, nat: string, at: Region, divs = 3): Corps => addCorps(g, nat, `Test Corps ${g.nextCorps}`, 'inf', divs, at.id, {});
const clear = (r: Region): Region => { for (const c of r.units) c.dead = true; r.units = []; return r; };
/** Runs a nation's month with the staff's coin for a descent fixed. */
const monthWith = (g: Game, nat: string, chance: number): void => {
  const was = D.chance;
  D.chance = chance;
  try { runAI(g, nat); } finally { D.chance = was; }
};
/** Britain with a landing force by its quay, and every German harbour in reach emptied, with the country behind it. */
const descent = (): { g: Game; quay: Region; force: Corps[]; shores: Region[] } => {
  const g = quietYear();
  g.fleets = g.fleets.filter((f) => f.nat !== 'GER');
  g.airFleets = g.airFleets.filter((f) => f.nat !== 'GER');
  const quay = seaBase(g, 'UK')!;
  const force = [corps(g, 'UK', quay), corps(g, 'UK', quay), corps(g, 'UK', quay)];
  const shores = landingTargets(g, force).filter((r) => r.ctrl === 'GER' && r.port);
  for (const r of shores) { clear(r); for (const j of r.nb) if (g.regions[j].ctrl === 'GER') clear(g.regions[j]); }
  return { g, quay, force, shores };
};

describe('the second front', () => {
  it('is opened by a staff with ships after a year at war with an enemy its home army does not touch', () => {
    const g = opened();
    expect(g.nations.UK.atWar).toContain('GER');
    expect(seaFoe(g, 'UK')).toBeNull();
    expect(seaBase(g, 'UK')).toBeNull();
    g.turn += D.after + 1;
    const sea = seaFoe(g, 'UK')!;
    expect(sea.foe).toBe('GER');
    expect(sea.second).toBe(true);
    expect(sea.L.force).toBe(D.force);
    // The quay is a home port of its own, and the staff keeps the landing force there.
    const quay = seaBase(g, 'UK')!;
    expect(quay.owner).toBe('UK');
    expect(quay.port).toBeTruthy();
    expect(quay.overseas).toBeFalsy();
    expect(staffPhase(g, 'UK')!.staging![quay.name]).toBeGreaterThanOrEqual(D.force);
    // France has the German army on its own frontier and sends nobody; the Soviet Union is at war with no one.
    expect(seaFoe(g, 'FRA')).toBeNull();
    expect(seaFoe(g, 'SOV')).toBeNull();
    // A player's own country is his to order.
    const mine = quietYear('UK');
    expect(seaFoe(mine, 'UK')).toBeNull();
    expect(seaBase(mine, 'UK')).toBeNull();
  });

  it('lands on a harbour left unguarded, and not when the coin falls the other way', () => {
    const { g, quay, force, shores } = descent();
    expect(shores.length).toBeGreaterThan(0);
    monthWith(g, 'UK', 1);
    const at = g.regions[force[0].reg];
    expect(shores).toContain(at);
    expect(force.every((c) => !c.dead && c.reg === at.id)).toBe(true);
    expect(g.headlines.some((h) => h.text === `British troops land at ${at.name} from the sea.`)).toBe(true);
    expect(g.flags['st:lodged:UK']).toBe(at.id + 1);
    expect(g.flags['st:seaNext:UK']).toBe(g.turn + D.every);
    // The corps of a second front are marked: they are not shipped home again as an expedition would be.
    for (const c of force) expect(g.flags[`sf:${c.id}`]).toBe(1);
    const calm = descent();
    monthWith(calm.g, 'UK', 0);
    expect(calm.force.every((c) => c.reg === calm.quay.id)).toBe(true);
    expect(calm.shores.every((r) => r.ctrl === 'GER' && !r.units.some((u) => u.nat === 'UK'))).toBe(true);
    expect(quay.owner).toBe('UK');
  });

  it('stays ashore, and is fed over the beach while the men there are too few and the harbour town holds out', () => {
    const { g, force } = descent();
    monthWith(g, 'UK', 1);
    const at = g.regions[force[0].reg];
    const lod = lodgment(g, 'UK')!;
    expect(lod.at).toBe(at);
    // Nine divisions ashore are fewer than the front needs.
    expect(force.reduce((s, c) => s + c.divs * c.str, 0)).toBeLessThan(D.hold);
    expect(lod.short).toBe(true);
    // While the town still flies the enemy's flag no ship can dock: there is no friendly quay on that ground yet.
    if (at.ctrl !== 'UK') expect(lod.ports).toEqual([]);
    else expect(lod.ports[0]).toBe(at);
    // Next month: the quay is still the staff's, and whoever is ready there goes over, coin or no coin.
    g.turn += 1;
    for (const c of force) { c.ap = c.apMax; c.supplied = true; }
    const quay = seaBase(g, 'UK')!;
    expect(staffPhase(g, 'UK')!.staging![quay.name]).toBeGreaterThanOrEqual(Math.max(D.force, Math.min(g.nations.UK.sealift, 5)));
    const wave = [corps(g, 'UK', quay), corps(g, 'UK', quay)];
    monthWith(g, 'UK', 0);
    expect(wave.every((c) => !c.dead && c.reg === at.id)).toBe(true);
    for (const c of wave) expect(g.flags[`sf:${c.id}`]).toBe(1);
    // And the first wave has not been taken off: every corps of it is still on the Continent.
    expect(force.every((c) => !c.dead && g.regions[c.reg].owner !== 'UK')).toBe(true);
  });
});

describe('the coast watch', () => {
  it('names the harbours of the homeland an enemy with ships can reach, once the two have not fought for a season', () => {
    const g = opened();
    // Too soon: the war is a month or two old.
    expect(coastWatch(g, 'GER')).toEqual({});
    g.turn += D.after + 1;
    const watch = coastWatch(g, 'GER');
    const names = Object.keys(watch);
    expect(names.length).toBeGreaterThanOrEqual(2);
    expect(names).toContain('Hamburg');
    const field = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng').length;
    expect(names.length).toBeLessThanOrEqual(Math.max(OPEN.coast.least, Math.floor(OPEN.coast.share * field)));
    for (const nm of names) {
      const r = g.byName[nm];
      expect(r.owner).toBe('GER');
      expect(r.ctrl).toBe('GER');
      expect(r.port).toBeTruthy();
      expect(watch[nm]).toBe(OPEN.coast.each);
    }
    // The staff's orders carry it: a place to keep a force at, and marked as a watch.
    const ph = staffPhase(g, 'GER')!;
    expect(ph.watch).toEqual(names);
    for (const nm of names) expect(ph.staging![nm]).toBeGreaterThanOrEqual(1);
    // Britain watches its own harbours against Germany's ships in the same way.
    const british = Object.keys(coastWatch(g, 'UK'));
    expect(british.length).toBeGreaterThan(0);
    for (const nm of british) expect(g.byName[nm].owner).toBe('UK');
    // France has the German army at its frontier, not at sea; the Soviet Union is at peace.
    expect(coastWatch(g, 'FRA')).toEqual({});
    expect(coastWatch(g, 'SOV')).toEqual({});
  });

  it('is kept by a corps of the field army: a town garrison does not count, and a corps already there stays', () => {
    const set = (real: boolean) => {
      const g = quietYear();
      const quay = seaBase(g, 'GER');
      const name = Object.keys(coastWatch(g, 'GER')).find((nm) => nm !== quay?.name)!;
      const r = clear(g.byName[name]);
      const first = corps(g, 'GER', r, real ? 3 : 1);
      if (!real) first.hold = true;
      return { g, r, first };
    };
    // A one-division garrison that never leaves its town: the staff sends a real corps to stand beside it.
    const thin = set(false);
    runAI(thin.g, 'GER');
    expect(thin.first.reg).toBe(thin.r.id);
    expect(thin.r.units.some((u) => u.nat === 'GER' && !u.hold && u.kind !== 'fort' && u.divs >= 2)).toBe(true);
    // A corps of three divisions already in the harbour is the watch, and is not marched off to the front.
    const held = set(true);
    runAI(held.g, 'GER');
    expect(held.first.reg).toBe(held.r.id);
  });

  it('is fed by rail when nobody stands within three marches of the harbour', () => {
    const g = quietYear();
    const quay = seaBase(g, 'GER');
    const name = Object.keys(coastWatch(g, 'GER')).find((nm) => nm !== quay?.name)!;
    const r = g.byName[name];
    const d = new Int16Array(g.regions.length).fill(-1);
    const q = [r.id];
    d[r.id] = 0;
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0) { d[j] = d[q[i]] + 1; q.push(j); }
    for (const x of g.regions) if (d[x.id] <= 3 && x.ctrl === 'GER') clear(x);
    runAI(g, 'GER');
    expect(r.units.some((u) => u.nat === 'GER' && !u.hold && u.kind === 'inf' && u.divs >= 2)).toBe(true);
  });
});

describe('fortifications from idle money', () => {
  const forts = (g: Game): Region[] => g.nations.GER.queue.filter((b) => b.what === 'fort').map((b) => g.regions[b.region!]);
  const ready = (human: string | null = null): Game => {
    const g = quietYear(human);
    g.nations.GER.queue = g.nations.GER.queue.filter((b) => b.what !== 'fort');
    g.nations.GER.pp = 300;
    return g;
  };

  it('a computer-run nation with men to raise corps keeps its money for them', () => {
    const g = ready();
    expect(menSpare(g, 'GER')).toBeGreaterThan(3);
    aiFortify(g, 'GER', [], coastRules());
    expect(forts(g)).toEqual([]);
    expect(g.nations.GER.pp).toBe(300);
  });

  it('out of men, it digs in at home behind the front, so many works a month, and keeps something back', () => {
    const g = ready();
    g.flags['mp:GER'] = 0;
    expect(menSpare(g, 'GER')).toBeLessThan(1);
    aiFortify(g, 'GER', [], coastRules());
    const got = forts(g);
    expect(got.length).toBe(OPEN.coast.dig.orders);
    expect(g.nations.GER.pp).toBeLessThan(300);
    for (const r of got) {
      expect(isHome(g, 'GER', r)).toBe(true);
      expect(r.overseas).toBeFalsy();
      // Not in a province the enemy already stands next to: those are fought for, not built in.
      expect(r.nb.some((j) => atWar(g, 'GER', g.regions[j].ctrl))).toBe(false);
    }
    // With little in the treasury nothing is ordered: 'keep' is held back for the drafts.
    const poor = ready();
    poor.flags['mp:GER'] = 0;
    poor.nations.GER.pp = OPEN.coast.dig.keep + 5;
    aiFortify(poor, 'GER', [], coastRules());
    expect(forts(poor)).toEqual([]);
  });

  it('a watched harbour is fortified first, one order at a time, whatever the reserve of men', () => {
    const g = ready();
    aiFortify(g, 'GER', ['Hamburg'], coastRules());
    expect(forts(g).map((r) => r.name)).toEqual(['Hamburg']);
    aiFortify(g, 'GER', ['Hamburg'], coastRules());
    expect(forts(g).map((r) => r.name)).toEqual(['Hamburg']);
    // Idle money only: while the treasury is still saving for a corps of infantry, the works wait.
    const saving = ready();
    const hamburg = saving.byName.Hamburg;
    saving.nations.GER.pp = corpsCost(saving, 'GER', 'inf') + PRODUCTION.aiReserve + fortCost(saving, 'GER', hamburg)! - 1;
    aiFortify(saving, 'GER', ['Hamburg'], coastRules());
    expect(forts(saving)).toEqual([]);
    saving.nations.GER.pp += 1;
    aiFortify(saving, 'GER', ['Hamburg'], coastRules());
    expect(forts(saving).map((r) => r.name)).toEqual(['Hamburg']);
  });

  it('works on a computer staff\'s order book do not hold up the raising of corps', () => {
    const g = ready();
    const n = g.nations.GER;
    // (Late enough that history's own mobilisation has run its course and the staff orders for itself.)
    g.turn = 70;
    n.queue = [];
    n.pp = 2000;
    const homes = g.regions.filter((r) => isHome(g, 'GER', r) && !r.overseas && !(r.fort?.lv ?? 0)).slice(0, 8);
    for (const r of homes) expect(orderFort(g, 'GER', r.id)).toBeTruthy();
    expect(forts(g).length).toBe(8);
    n.pp = 2000;
    aiProduction(g, 'GER');
    expect(n.queue.some((b) => b.what === 'corps')).toBe(true);
  });

  it('the player\'s fortifications are his own to order, and history\'s scenarios have none of this', () => {
    const g = ready('GER');
    g.flags['mp:GER'] = 0;
    aiFortify(g, 'GER', ['Hamburg'], coastRules());
    expect(forts(g)).toEqual([]);
    const h = createGame(3, null);
    expect(coastWatch(h, 'GER')).toEqual({});
    expect(seaFoe(h, 'UK')).toBeNull();
    h.nations.GER.pp = 300;
    aiFortify(h, 'GER', ['Hamburg'], coastRules());
    expect(h.nations.GER.queue.filter((b) => b.what === 'fort')).toEqual([]);
  });
});

describe('a pocket fed from an ally\'s town', () => {
  it('does not stop the month: the news of it is written from the pocket\'s own count of its stores', () => {
    const g = quietYear();
    // A British corps on French ground with the Germans all round it, and a French-held harbour beside it.
    const a = g.regions.find((r) => r.owner === 'FRA' && !r.overseas && r.port && r.ctrl === 'FRA' && r.nb.some((j) => g.regions[j].owner === 'FRA' && !g.regions[j].port))!;
    const b = g.regions[a.nb.find((j) => g.regions[j].owner === 'FRA' && !g.regions[j].port)!];
    for (const r of [a, b]) for (const j of r.nb) if (j !== a.id && j !== b.id) { clear(g.regions[j]).ctrl = 'GER'; }
    clear(b).ctrl = 'UK';
    corps(g, 'UK', b);
    expect(() => { for (let k = 0; k < 4; k++) { pocketMonth(g, 'UK'); g.turn++; } }).not.toThrow();
  });
});

describe('men before guns', () => {
  const G = (arsenalData as unknown as { ai: { guns: { per: number; most: number; facing: number; fit: number } } }).ai.guns;
  /** Germany after Poland, with no guns of its own or on order, facing French corps that have dug in. */
  const facing = (): Game => {
    const g = opened();
    const n = g.nations.GER;
    for (const c of g.corps.filter((x) => x.nat === 'GER' && x.kind === 'art')) removeCorps(g, c);
    n.queue = n.queue.filter((b) => !(b.what === 'corps' && b.kind === 'art'));
    const over = g.corps.filter((c) => c.nat === 'FRA' && !c.dead && c.kind !== 'fort' && g.regions[c.reg].nb.some((j) => g.regions[j].ctrl === 'GER'));
    if (over.length < G.facing) throw new Error('too few French corps stand on the German frontier');
    for (const c of over) { c.dug = true; c.dugFor = 3; }
    n.armyPeak = armySize(g, 'GER');
    return g;
  };

  it('a staff at full strength that faces dug-in troops without guns asks for them', () => {
    const g = facing();
    expect(armyWorn(g, 'GER')).toBe(false);
    expect(gunsWanted(g, 'GER')).toBe(true);
  });

  it('an army worn under its fit share of the largest it has been asks for none, and says so to its builders', () => {
    const g = facing();
    const n = g.nations.GER;
    n.armyPeak = armySize(g, 'GER') / (G.fit - 0.02);
    expect(armyWorn(g, 'GER')).toBe(true);
    expect(gunsWanted(g, 'GER')).toBe(false);
    // The next corps its factories take in hand is not artillery.
    n.pp = 400; g.turn = 70;
    const had = new Set(n.queue);
    aiProduction(g, 'GER');
    const fresh = n.queue.filter((b) => !had.has(b) && b.what === 'corps');
    expect(fresh.length).toBeGreaterThan(0);
    expect(fresh.some((b) => b.kind === 'art')).toBe(false);
    // Back near its peak, it asks again.
    n.armyPeak = armySize(g, 'GER');
    expect(armyWorn(g, 'GER')).toBe(false);
  });

  it('a nation at peace is not worn, and history\'s scenarios know nothing of it', () => {
    const g = opened();
    const quiet = Object.values(g.nations).find((x) => x.alive && !x.atWar.length && armySize(g, x.id) > 0)!;
    quiet.armyPeak = armySize(g, quiet.id) * 2;
    expect(armyWorn(g, quiet.id)).toBe(false);
    const h = createGame(3, 'GER');
    h.nations.POL.armyPeak = armySize(h, 'POL') * 2;
    expect(armyWorn(h, 'POL')).toBe(false);
  });
});
