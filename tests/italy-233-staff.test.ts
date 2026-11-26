// Ruling 233, riders (j), (k), (q), (r), (s), (u), (v), (w), (x) and (y) (the design director's b506 to b548; the grand campaign, computer staffs only).
// (j) History's list of shores has an order: when the month comes for a later shore of the list, a staff's home army
//     turns to the enemy who holds it nearest the home ports, though its first enemy across the sea still stands.
//     Britain turns to France in 1944 while Italy fights on. (Before it a staff kept one enemy across the sea for as
//     long as his coast was in reach: in the wars where Italy held out, Britain never landed in France.)
// (q) It turns three months before the shore's month (open.json 'descent.lead'), and lands not before the month: the
//     invasion army gathered in Britain through the first half of 1944.
// (k) The computer's Italian fleet keeps its harbours once Mussolini has fallen, and comes out against no landing.
// (r) An army abroad with no lane open from any quay waits at its quay, as the home army does, and does not drop its
//     plan (open.json 'descent.wait'): America's army in Britain, the months the Western Mediterranean is shut.
// (s) The test of strength (b514): only the holder of a beach brings reserves from his own country. An ally's army
//     standing in its own home stays there (open.json 'descent.strength.own'): Italy's, against a landing in Brittany.
// (u) History's list picks the day (b516): a staff that has planned for a shore of the list since the first month of
//     its lead lands on it in the shore's month without the monthly dice, where every test passes (open.json 'descent.day').
// (v) A computer admiral whose own staff has named no crossing covers the named crossing of an ally in the same war
//     (b520; fleets.json 'ai.allies'): America's Atlantic Fleet, the months Britain's way to Sardinia is shut.
// (w) The joint crossing (b532; open.json 'descent.joint'): where a staff's own corps at its quay are fewer than the
//     force, corps of its computer-run allies standing ready there with no orders of their own from that harbour fill
//     the force and sail in it. Torch, Sicily and Salerno were Anglo-American landings under one command. A player's
//     corps are never drawn in, and a player's landing force stays one nation's.
// (x) The home army's quay is chosen afresh the month its staff's target changes, and a home port where its allies'
//     corps leave no room for the force is passed over while another port serves (b532; 'descent.afresh', 'descent.room'):
//     the invasion of France was mounted from every harbour of the south coast, not from one.
// (y) An army abroad names the quay it stands at (b548; 'descent.stands'): of two ports equal in worth and in seas,
//     the one with more of its own corps on the quay, then within the marches. Before, the first port of the list won.
import { describe, expect, it } from 'vitest';
import fleetData from '../data/campaign/fleets.json';
import openData from '../data/campaign/open.json';
import { reckon, type Reckoning } from '../src/campaign/ashore';
import { allied, createGame, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { atSea, closedTo, fleetsOf, keepsHarbour, landCorps, navalAI, sailsWith, sortieAgainst, whyNotLand } from '../src/campaign/navy';
import { fieldCtrl } from '../src/campaign/siege';
import { lodgment, onTheDay, opensNow, opensOn, opensSoon, quayGuests, seaBase, seaFoe, startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const FRANCE_44 = 57;
const DESCENT = openData.descent as unknown as { lead: number; wait: boolean; day: boolean; afresh: boolean; room: boolean; joint: boolean; stands: boolean; force: number };
const LEAD = DESCENT.lead;
/** A grand war nobody plays: Britain at war with Germany and Italy, France fallen, and Britain's staff set on Italy since last year. */
const war = (turn: number, stop = 3, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
  setWar(g, 'GER', 'UK');
  setWar(g, 'ITA', 'UK');
  for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas) r.ctrl = 'GER';
  g.turn = turn;
  const ids = Object.keys(g.nations);
  g.flags['st:foe:UK'] = ids.indexOf('ITA') + 1;
  g.flags['st:foeAt:UK'] = turn - 2;
  for (const e of ['GER', 'ITA']) { g.flags[`war:UK:${e}`] = 1; g.nations.UK.lastFight[e] = 1; g.nations[e].lastFight.UK = 1; }
  return g;
};

describe('history\'s list has an order (233(j))', () => {
  it('Britain\'s home army keeps Italy until three months before France\'s month, and then turns to Germany on the Channel', () => {
    expect(LEAD).toBe(3);
    const before = war(FRANCE_44 - LEAD - 1);
    expect(seaFoe(before, 'UK')?.foe).toBe('ITA');
    const g = war(FRANCE_44 - LEAD);
    expect(seaFoe(g, 'UK')).toMatchObject({ foe: 'GER', second: true });
    // The choice stands like any other: it is kept the month after, and Italy does not win it back.
    g.turn++;
    expect(seaFoe(g, 'UK')?.foe).toBe('GER');
    // It lands not before the month (233(q)): no harbour of the French mainland is on its list until May 1944.
    const beach = g.regions.find((r) => r.owner === 'FRA' && !r.overseas && r.port && r.nb.length > 0 && r.ctrl === 'GER')!;
    expect(opensOn(g, 'UK', beach)).toBe(false);
    g.turn = FRANCE_44;
    expect(opensOn(g, 'UK', beach)).toBe(true);
    expect(seaFoe(g, 'UK')?.foe).toBe('GER');
  });

  it('to the enemy who holds that shore nearest home: Germany on the Channel, not Italy at Marseille', () => {
    const g = war(FRANCE_44);
    g.byName.Marseille.ctrl = 'ITA';
    expect(seaFoe(g, 'UK')?.foe).toBe('GER');
  });

  it('not with men ashore who still have a front to hold, and not at the stops where the list is put away', () => {
    for (const stop of [4, 5]) expect(seaFoe(war(FRANCE_44, stop), 'UK')?.foe, `stop ${stop}`).toBe('ITA');
    for (const stop of [1, 2]) expect(seaFoe(war(FRANCE_44 + 2, stop), 'UK')?.foe, `stop ${stop}`).toBe('GER');
    const g = war(FRANCE_44);
    // A British corps ashore in a harbour of the Italian mainland, with the enemy next door.
    const at = ['Naples', 'Apulia', 'Calabria'].map((nm) => g.byName[nm]).find((r) => r.port)!;
    const c = g.corps.find((x) => x.nat === 'UK' && !x.dead && x.kind === 'inf')!;
    const from = g.regions[c.reg];
    from.units.splice(from.units.indexOf(c), 1);
    c.reg = at.id;
    at.units = [c];
    at.ctrl = 'UK';
    g.flags['st:lodged:UK'] = at.id + 1;
    expect(lodgment(g, 'UK')?.at).toBe(at);
    expect(seaFoe(g, 'UK')?.foe).toBe('ITA');
  });
});

describe('the lead holds for the whole plan (233(q), b513)', () => {
  /** A grand war nobody plays with Italy out of it: Britain at war with Germany alone, France fallen, and no enemy across the sea chosen yet. */
  const alone = (turn: number, stop = 3): Game => {
    const g = startOpen(createGame(3, null));
    if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
    setWar(g, 'GER', 'UK');
    for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas && r.nb.length > 0) r.ctrl = 'GER';
    g.turn = turn;
    g.flags['war:UK:GER'] = 1;
    return g;
  };
  const beachOf = (g: Game): Region => g.regions.find((r) => r.owner === 'FRA' && !r.overseas && r.port && r.seas.length > 0 && r.nb.length > 0 && r.ctrl === 'GER')!;

  it('a mainland harbour of the list opens soon for the three months before its month, at the stops where the list binds', () => {
    for (const [turn, soon, open] of [[FRANCE_44 - LEAD - 1, false, false], [FRANCE_44 - LEAD, true, false], [FRANCE_44 - 1, true, false], [FRANCE_44, false, true]] as const) {
      const g = alone(turn);
      expect(opensSoon(g, 'UK', beachOf(g)), `month ${turn}`).toBe(soon);
      expect(opensOn(g, 'UK', beachOf(g)), `month ${turn}`).toBe(open);
    }
    // Stops 4 and 5 put the list away, and with it the lead; an island and a colony are open always and never 'soon'.
    for (const stop of [4, 5]) { const g = alone(FRANCE_44 - 1, stop); expect(opensSoon(g, 'UK', beachOf(g)), `stop ${stop}`).toBe(false); }
    const g = alone(FRANCE_44 - 1);
    expect(opensSoon(g, 'UK', g.byName.Sardinia)).toBe(false);
    // A player is planned for by nobody.
    const mine = startOpen(createGame(3, 'UK'));
    mine.turn = FRANCE_44 - 1;
    expect(seaFoe(mine, 'UK')).toBeNull();
  });

  it('with Italy out, Britain\'s home army has Germany for its enemy across the sea from February 1944 and not before', () => {
    expect(seaFoe(alone(FRANCE_44 - LEAD - 1), 'UK')).toBeNull();
    const g = alone(FRANCE_44 - LEAD);
    expect(seaFoe(g, 'UK')).toMatchObject({ foe: 'GER', second: true });
  });

  it('and its quay is chosen for the harbours of the list, not for a shore that is no part of it', () => {
    const g = alone(FRANCE_44 - LEAD);
    // Germany holds Norway's harbours too, with nobody in them: no part of Britain's list, however clear.
    for (const r of g.regions) if (r.owner === 'NOR' && !r.overseas) { r.ctrl = 'GER'; r.units = []; }
    const quay = seaBase(g, 'UK')!;
    expect(quay.owner).toBe('UK');
    const list = g.regions.filter((r) => r.port && r.ctrl === 'GER' && (opensOn(g, 'UK', r) || opensSoon(g, 'UK', r)));
    expect(list.length).toBeGreaterThan(0);
    expect(list.every((r) => r.owner === 'FRA')).toBe(true);
    // The quay lies on a sea a harbour of the list lies on, or next to one.
    const seas = new Set(list.flatMap((r) => r.seas));
    expect(quay.seas.some((z) => seas.has(z) || g.seas[z].adj.some((y) => seas.has(y)))).toBe(true);
  });
  it('history\'s list picks the day (233(u)): a staff that planned through the whole lead lands in the shore\'s month without the dice', () => {
    expect(DESCENT.day).toBe(true);
    // The very month the list opens the shore, and no other; nothing at the stops where the list is put away.
    for (const [turn, now] of [[FRANCE_44 - 1, false], [FRANCE_44, true], [FRANCE_44 + 1, false]] as const) { const g = alone(turn); expect(opensNow(g, 'UK', beachOf(g)), `month ${turn}`).toBe(now); }
    for (const stop of [4, 5]) { const g = alone(FRANCE_44, stop); expect(opensNow(g, 'UK', beachOf(g)), `stop ${stop}`).toBe(false); }
    expect(opensNow(alone(FRANCE_44), 'UK', alone(FRANCE_44).byName.Sardinia)).toBe(false);
    // Asked month by month from the first month of the lead: the day is the shore's month, and it is one month long.
    const g = alone(FRANCE_44 - LEAD);
    for (let t = FRANCE_44 - LEAD; t <= FRANCE_44 + 1; t++) { g.turn = t; expect(onTheDay(g, 'UK', 'GER', ''), `month ${t}`).toBe(t === FRANCE_44); }
    expect(g.flags['st:led:UK']).toBeUndefined();
    // A staff that came to the plan a month late has not kept the full lead: the dice decide, as before.
    const late = alone(FRANCE_44 - LEAD + 1);
    for (let t = FRANCE_44 - LEAD + 1; t <= FRANCE_44; t++) { late.turn = t; expect(onTheDay(late, 'UK', 'GER', ''), `late, month ${t}`).toBe(false); }
    // Each army of a staff keeps its own note: the army abroad did not plan because the home army did.
    const two = alone(FRANCE_44 - LEAD);
    expect(onTheDay(two, 'UK', 'GER', '')).toBe(false);
    two.turn = FRANCE_44;
    expect(onTheDay(two, 'UK', 'GER', ':o')).toBe(false);
    expect(onTheDay(two, 'UK', 'GER', '')).toBe(true);
    // At stops 4 and 5 the dice stay, and with the dial off everywhere.
    for (const stop of [4, 5]) {
      const x = alone(FRANCE_44 - LEAD, stop);
      expect(onTheDay(x, 'UK', 'GER', '')).toBe(false);
      x.turn = FRANCE_44;
      expect(onTheDay(x, 'UK', 'GER', ''), `stop ${stop}`).toBe(false);
    }
    const off = alone(FRANCE_44 - LEAD);
    DESCENT.day = false;
    try {
      expect(onTheDay(off, 'UK', 'GER', '')).toBe(false);
      off.turn = FRANCE_44;
      expect(onTheDay(off, 'UK', 'GER', '')).toBe(false);
    } finally { DESCENT.day = true; }
  });
});

describe('the Italian fleet keeps its harbours once Mussolini has fallen (233(k))', () => {
  it('is in the data, for the computer\'s Italy alone', () => {
    expect(fleetData.harbour).toEqual({ ITA: 'event:mussolini-falls' });
    const g = war(48);
    expect(keepsHarbour(g, 'ITA')).toBe(false);
    g.flags['event:mussolini-falls'] = 48;
    expect(keepsHarbour(g, 'ITA')).toBe(true);
    expect(keepsHarbour(g, 'GER')).toBe(false);
    // A player's Italy sails as he likes, and no history campaign is touched.
    const mine = war(48, 3, 'ITA');
    mine.flags['event:mussolini-falls'] = 48;
    expect(keepsHarbour(mine, 'ITA')).toBe(false);
    const old = createGame(1, null);
    old.flags['event:mussolini-falls'] = 48;
    expect(keepsHarbour(old, 'ITA')).toBe(false);
  });

  it('its surface fleets lie in harbour and come out against no landing; its submarines keep the sea', () => {
    const g = war(48);
    navalAI(g, 'ITA');
    const surface = fleetsOf(g, 'ITA').filter((f) => f.duty !== 'raid');
    expect(surface.length).toBeGreaterThan(0);
    expect(surface.some(atSea)).toBe(true);
    const seas = [...new Set(surface.flatMap((f) => (f.base >= 0 ? g.regions[f.base].seas : [f.home])))];
    for (const f of fleetsOf(g, 'ITA')) f.stance = 'port';
    const out = sortieAgainst(g, 'UK', seas);
    expect(out).toBeGreaterThan(0);
    g.flags['event:mussolini-falls'] = 48;
    expect(sortieAgainst(g, 'UK', seas)).toBeLessThan(out);
    navalAI(g, 'ITA');
    expect(surface.some(atSea)).toBe(false);
    expect(fleetsOf(g, 'ITA').filter((f) => f.duty === 'raid').length).toBeGreaterThan(0);
  });
});

describe('an army abroad waits at its quay when no lane is open (233(r))', () => {
  /** America at war beside Britain in spring 1943, four of its corps idle in Cornwall, and every sea off Cornwall shut to its troopships. */
  const shutIn = (): { g: Game; quay: Region } => {
    const g = startOpen(createGame(3, null));
    g.nations.USA.align = g.nations.UK.align;
    expect(allied(g, 'USA', 'UK')).toBe(true);
    for (const [a, b] of [['GER', 'UK'], ['ITA', 'UK'], ['GER', 'USA'], ['ITA', 'USA']]) setWar(g, a, b);
    // (The French mainland is Germany's; Corsica is left to Vichy, so that the islands on America's list are Italy's.)
    for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas && r.nb.length > 0) r.ctrl = 'GER';
    g.turn = 45;
    const quay = g.byName.Cornwall;
    for (const c of g.corps.filter((x) => x.nat === 'USA' && !x.dead && x.kind === 'inf').slice(0, 4)) {
      const from = g.regions[c.reg];
      from.units.splice(from.units.indexOf(c), 1);
      c.reg = quay.id;
      quay.units.push(c);
    }
    // Enemy battle fleets at sea off Cornwall (no such thing happened: they stand for the submarines and aircraft that shut the way).
    const out = [...fleetsOf(g, 'ITA'), ...fleetsOf(g, 'GER')];
    quay.seas.filter((z) => !closedTo(g, z, 'USA')).forEach((z, i) => { const f = out[i]; f.zone = z; f.stance = 'patrol'; f.ships.bb += 400; });
    for (const z of quay.seas) expect(closedTo(g, z, 'USA'), g.seas[z].name).toBe(true);
    return { g, quay };
  };
  const ids = (g: Game): string[] => Object.keys(g.nations);

  it('is in the data', () => {
    expect(DESCENT.wait).toBe(true);
  });

  it('with a plan it keeps the enemy and the quay it had', () => {
    const { g, quay } = shutIn();
    g.flags['st:foe:USA:o'] = ids(g).indexOf('ITA') + 1;
    g.flags['st:quay:USA:o'] = quay.id + 1;
    g.flags['st:quayAt:USA:o'] = g.turn - 5;
    expect(seaFoe(g, 'USA', 1)).toMatchObject({ foe: 'ITA', second: true });
    expect(seaBase(g, 'USA', 1)).toBe(quay);
    expect(g.flags['st:quay:USA:o']).toBe(quay.id + 1);
  });

  it('with none it takes the quay nearest his harbours, whoever holds the water', () => {
    const { g } = shutIn();
    expect(seaFoe(g, 'USA', 1)?.foe).toBe('ITA');
    const at = seaBase(g, 'USA', 1)!;
    expect(at.owner).toBe('UK');
    expect(at.port).toBe(true);
  });

  it('as first built (the dial off) the plan was dropped the month the lane shut', () => {
    const { g, quay } = shutIn();
    g.flags['st:foe:USA:o'] = ids(g).indexOf('ITA') + 1;
    g.flags['st:quay:USA:o'] = quay.id + 1;
    g.flags['st:quayAt:USA:o'] = g.turn - 5;
    DESCENT.wait = false;
    try {
      expect(seaFoe(g, 'USA', 1)).toBeNull();
      expect(g.flags['st:quay:USA:o']).toBeUndefined();
    } finally { DESCENT.wait = true; }
  });

  it('a player\'s army is never planned for', () => {
    const g = startOpen(createGame(3, 'USA'));
    expect(seaFoe(g, 'USA', 1)).toBeNull();
  });
});

describe('only the holder of the beach brings reserves from home (233(s))', () => {
  const STRENGTH = (openData.descent as unknown as { strength: { own?: boolean; reserve: number } }).strength;
  /** The same war with France's army gone from its mainland: Germany's France is in one piece with Germany and with Italy. */
  const fallen = (): Game => {
    const g = war(FRANCE_44);
    for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas) {
      for (const u of r.units) if (u.nat === 'FRA') u.dead = true;
      r.units = r.units.filter((u) => u.nat !== 'FRA');
    }
    return g;
  };
  const atHome = (g: Game, nat: string): number => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && !c.town && g.regions[c.reg].owner === nat && !g.regions[c.reg].overseas).length;
  /** The reckoning as first built (every enemy's corps out of the line on ground joined by rail counted for a share), and as ruled. */
  const both = (g: Game, at: Region, force: Corps[]): { all: Reckoning; R: Reckoning } => {
    STRENGTH.own = false;
    let all: Reckoning;
    try { all = reckon(g, 'UK', at, force)!; } finally { STRENGTH.own = true; }
    return { all, R: reckon(g, 'UK', at, force)! };
  };
  const landing = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'UK' && !c.dead && c.kind === 'inf').slice(0, 3);

  it('is in the data', () => {
    expect(STRENGTH.own).toBe(true);
  });

  it('Italy\'s army standing in Italy is no reserve against a landing in Brittany; Germany\'s own is, and so is an Italian corps in France', () => {
    const g = fallen();
    const at = g.byName.Brittany;
    const force = landing(g);
    const { all, R } = both(g, at, force);
    const italians = atHome(g, 'ITA');
    expect(italians).toBeGreaterThan(5);
    expect(all.behind - R.behind).toBeGreaterThan(5);
    expect(all.behind - R.behind).toBeLessThanOrEqual(italians);
    expect(R.reserve).toBeLessThan(all.reserve);
    expect(R.chance).toBeGreaterThan(all.chance);
    // Germany holds the beach: its corps at home are counted as before.
    expect(R.behind).toBeGreaterThan(0);
    expect(R.near).toBe(all.near);
    // An Italian corps standing in France, outside its own country, can be brought like any other.
    const c = g.corps.find((x) => x.nat === 'ITA' && !x.dead && x.kind === 'inf' && g.regions[x.reg].owner === 'ITA' && !g.regions[x.reg].overseas)!;
    const to = g.byName.Paris;
    const from = g.regions[c.reg];
    from.units.splice(from.units.indexOf(c), 1);
    c.reg = to.id;
    to.units.push(c);
    const moved = reckon(g, 'UK', at, force)!;
    expect(moved.behind + moved.corps).toBe(R.behind + R.corps + 1);
  });

  it('on Italy\'s own shore Italy brings its reserves, and Germany\'s army at home in Germany stays there', () => {
    const g = fallen();
    const at = ['Naples', 'Apulia', 'Calabria'].map((nm) => g.byName[nm]).find((r) => r.port)!;
    const { all, R } = both(g, at, landing(g));
    // What stands within three marches counts as before, whoever's it is.
    expect(R.near).toBe(all.near);
    expect(R.corps).toBe(all.corps);
    expect(R.behind).toBeGreaterThan(0);
    expect(all.behind - R.behind).toBeGreaterThan(0);
    expect(all.behind - R.behind).toBeLessThanOrEqual(atHome(g, 'GER'));
  });
});

describe('a computer admiral covers an ally\'s named crossing (233(v))', () => {
  const AI = fleetData.ai as { allies?: boolean };
  const sea = (g: Game, name: string): number => g.seas.findIndex((z) => z.name === name);
  /** America in the war beside Britain, Britain's home army with its beach named (Sardinia, from Cornwall), and an American patrol fleet based in Cornwall with the Western Approaches for its beat. */
  const allies = (): { g: Game; f: Game['fleets'][number]; off: number; beat: number } => {
    const g = startOpen(createGame(3, null));
    g.nations.USA.align = g.nations.UK.align;
    for (const [a, b] of [['GER', 'UK'], ['ITA', 'UK'], ['GER', 'USA'], ['ITA', 'USA']]) setWar(g, a, b);
    g.turn = 46;
    const quay = g.byName.Cornwall, beat = sea(g, 'Western Approaches'), off = sea(g, 'Western Mediterranean');
    g.flags['st:aim:UK'] = g.byName.Sardinia.id + 1;
    g.flags['st:quay:UK'] = quay.id + 1;
    const f = fleetsOf(g, 'USA').filter((x) => x.duty !== 'raid').sort((a, b) => b.ships.bb + b.ships.ca - a.ships.bb - a.ships.ca)[0];
    Object.assign(f, { base: quay.id, home: beat, station: beat, zone: beat, stance: 'port', duty: 'patrol', tied: false, ready: 1 });
    return { g, f, off, beat };
  };

  it('is in the data', () => {
    expect(AI.allies).toBe(true);
  });

  it('with no crossing of its own staff, the American fleet stands off the beach Britain has named', () => {
    const { g, f, off } = allies();
    expect(g.byName.Sardinia.seas).toContain(off);
    navalAI(g, 'USA');
    expect(atSea(f)).toBe(true);
    expect(f.zone).toBe(off);
    // With the dial off it keeps its own beat, as before.
    const was = allies();
    AI.allies = false;
    try { navalAI(was.g, 'USA'); } finally { AI.allies = true; }
    expect(was.f.zone).toBe(was.beat);
  });

  it('its own staff\'s crossing comes first, and an ally\'s war that is not its own is not covered', () => {
    // America's own army has a beach named too (the Cotentin, from Cornwall): Britain's is left to Britain's fleets.
    const own = allies();
    own.g.byName.Cotentin.ctrl = 'GER';
    own.g.flags['st:aim:USA:o'] = own.g.byName.Cotentin.id + 1;
    own.g.flags['st:quay:USA:o'] = own.g.byName.Cornwall.id + 1;
    navalAI(own.g, 'USA');
    expect(own.f.zone).not.toBe(own.off);
    // America at peace with Italy: the beach is no enemy's of hers.
    const peace = allies();
    peace.g.nations.USA.atWar = peace.g.nations.USA.atWar.filter((e) => e !== 'ITA');
    peace.g.nations.ITA.atWar = peace.g.nations.ITA.atWar.filter((e) => e !== 'USA');
    navalAI(peace.g, 'USA');
    expect(peace.f.zone).not.toBe(peace.off);
    // A player's fleets are his own to order.
    const mine = allies();
    mine.g.human = 'USA';
    navalAI(mine.g, 'USA');
    expect(mine.f.zone).toBe(mine.beat);
  });
});

describe('allies at the same quay sail in one force (233(w))', () => {
  const ONE_PORT = 'A landing force sails from one port.';
  /** Corps of a nation brought to a harbour, rested and ready. */
  const bring = (g: Game, nat: string, at: Region, count: number): Corps[] => {
    const cs = g.corps.filter((c) => c.nat === nat && !c.dead && c.kind === 'inf' && !c.town && c.reg !== at.id).slice(0, count);
    expect(cs.length, nat).toBe(count);
    for (const c of cs) {
      const from = g.regions[c.reg];
      from.units.splice(from.units.indexOf(c), 1);
      c.reg = at.id;
      at.units.push(c);
      Object.assign(c, { str: 1, org: 1, ap: c.apMax, supplied: true, hold: false });
    }
    return cs;
  };
  /** July 1943, nobody playing: America and Canada in the war beside Britain, and Cornwall, Britain's quay for Sardinia, full: two British corps, two American, one Canadian. */
  const cornwall = (human: string | null = null): { g: Game; quay: Region; uk: Corps[]; us: Corps[]; can: Corps[] } => {
    const g = war(47, 3, human);
    for (const id of ['USA', 'CAN']) { g.nations[id].align = g.nations.UK.align; for (const e of ['GER', 'ITA']) setWar(g, e, id); }
    const quay = g.byName.Cornwall;
    quay.units = quay.units.filter((u) => u.kind === 'fort');
    g.flags['st:quay:UK'] = quay.id + 1;
    return { g, quay, uk: bring(g, 'UK', quay, 2), us: bring(g, 'USA', quay, 2), can: bring(g, 'CAN', quay, 1) };
  };

  it('is in the data', () => {
    expect(DESCENT.joint).toBe(true);
  });

  it('the guests at a quay are the allies\' ready corps with no orders of their own from that harbour', () => {
    const { g, quay, us, can } = cornwall();
    expect(allied(g, 'UK', 'USA') && allied(g, 'UK', 'CAN')).toBe(true);
    expect(quayGuests(g, 'UK', quay, 'ITA').map((c) => c.id).sort()).toEqual([...us, ...can].map((c) => c.id).sort());
    // America's own army abroad has named a crossing from Cornwall: its corps there have their orders.
    g.flags['st:quay:USA:o'] = quay.id + 1;
    expect(quayGuests(g, 'UK', quay, 'ITA')).toEqual(can);
    delete g.flags['st:quay:USA:o'];
    // One that has marched this month, one that is worn down, and one told to hold stay behind.
    us[0].ap = 0;
    us[1].str = 0.5;
    can[0].hold = true;
    expect(quayGuests(g, 'UK', quay, 'ITA')).toEqual([]);
    // An ally at peace with the enemy across the water sends nobody.
    const peace = cornwall();
    peace.g.nations.USA.atWar = peace.g.nations.USA.atWar.filter((e) => e !== 'ITA');
    peace.g.nations.ITA.atWar = peace.g.nations.ITA.atWar.filter((e) => e !== 'USA');
    expect(quayGuests(peace.g, 'UK', peace.quay, 'ITA')).toEqual(peace.can);
    // With the dial off nobody joins, as before.
    const off = cornwall();
    DESCENT.joint = false;
    try { expect(quayGuests(off.g, 'UK', off.quay, 'ITA')).toEqual([]); } finally { DESCENT.joint = true; }
  });

  it('a player\'s corps are never drawn in, and a player\'s own landing force stays one nation\'s', () => {
    // The player is America: his corps at Cornwall are his own to order.
    const his = cornwall('USA');
    expect(quayGuests(his.g, 'UK', his.quay, 'ITA')).toEqual(his.can);
    expect(sailsWith(his.g, 'UK', his.us[0])).toBe(false);
    expect(whyNotLand(his.g, [...his.uk, his.us[0]], his.g.byName.Sardinia)).toBe(ONE_PORT);
    // The player is Britain: no ally's corps joins his force, by his staff or by his own hand.
    const mine = cornwall('UK');
    expect(quayGuests(mine.g, 'UK', mine.quay, 'ITA')).toEqual([]);
    expect(whyNotLand(mine.g, [...mine.uk, mine.us[0]], mine.g.byName.Sardinia)).toBe(ONE_PORT);
    // And outside the grand campaign a landing force is one nation's, as it always was.
    const old = cornwall();
    delete old.g.flags.open;
    expect(whyNotLand(old.g, [...old.uk, old.us[0]], old.g.byName.Sardinia)).toBe(ONE_PORT);
  });

  it('two British corps and an American one are a landing force, and the beach falls to the staff that named the crossing', () => {
    const { g, uk, us } = cornwall();
    const beach = g.byName.Sardinia;
    expect(beach.ctrl).toBe('ITA');
    const force = [...uk, us[0]];
    expect(sailsWith(g, 'UK', us[0])).toBe(true);
    // Not against a shore the ally is not at war with.
    const peace = cornwall();
    peace.g.nations.USA.atWar = peace.g.nations.USA.atWar.filter((e) => e !== 'ITA');
    peace.g.nations.ITA.atWar = peace.g.nations.ITA.atWar.filter((e) => e !== 'USA');
    expect(whyNotLand(peace.g, [...peace.uk, peace.us[0]], peace.g.byName.Sardinia)).toBe('Sardinia is not enemy ground.');
    // The island's garrison is taken off and the enemy's fleets lie in harbour: the three go ashore together.
    for (const u of beach.units) u.dead = true;
    beach.units = [];
    for (const f of g.fleets) if (f.nat === 'ITA' || f.nat === 'GER') f.stance = 'port';
    expect(whyNotLand(g, force, beach)).toBeNull();
    const res = landCorps(g, force, beach);
    expect(res.ok).toBe(true);
    expect(force.every((c) => c.dead || c.reg === beach.id)).toBe(true);
    expect(force.filter((c) => !c.dead).map((c) => c.nat).sort()).toEqual(['UK', 'UK', 'USA']);
    expect(fieldCtrl(g, beach)).toBe('UK');
    expect(lodgment(g, 'USA')).toBeNull();
  });
});

describe('the home army\'s quay is chosen afresh, and never a harbour with no room (233(x))', () => {
  /** Italy out of the war, February 1944: Britain's home army turns to France (as `alone` above). */
  const turned = (): Game => {
    const g = startOpen(createGame(3, null));
    setWar(g, 'GER', 'UK');
    for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas && r.nb.length > 0) r.ctrl = 'GER';
    g.turn = FRANCE_44 - LEAD;
    g.flags['war:UK:GER'] = 1;
    g.nations.USA.align = g.nations.UK.align;
    setWar(g, 'GER', 'USA');
    return g;
  };
  const without = <T>(dial: 'afresh' | 'room', f: () => T): T => { DESCENT[dial] = false; try { return f(); } finally { DESCENT[dial] = true; } };
  /** A home port the staff would keep if it had it (it serves the list's shores), other than the one it picks on merit. */
  const another = (best: Region): Region => {
    for (const r of turned().regions) {
      if (r.id === best.id || !r.port || r.owner !== 'UK' || r.overseas) continue;
      const g = turned();
      g.flags['st:quay:UK'] = r.id + 1;
      g.flags['st:quayAt:UK'] = g.turn - 1;
      if (without('afresh', () => seaBase(g, 'UK'))?.id === r.id) return r;
    }
    throw new Error('no second quay serves');
  };
  const crowd = (g: Game, port: Region, count: number): void => {
    const at = g.regions[port.id];
    const cs = g.corps.filter((c) => c.nat === 'USA' && !c.dead && c.kind === 'inf' && !c.town && c.reg !== at.id).slice(0, count);
    expect(cs.length).toBe(count);
    for (const c of cs) { const from = g.regions[c.reg]; from.units.splice(from.units.indexOf(c), 1); c.reg = at.id; at.units.push(c); }
  };

  it('is in the data', () => {
    expect(DESCENT.afresh).toBe(true);
    expect(DESCENT.room).toBe(true);
  });

  it('a quay kept from another target is not kept: the staff chooses for the shore it has turned to', () => {
    const best = seaBase(turned(), 'UK')!;
    const old = another(best);
    // The quay it had against Italy, chosen last month: as first built it was kept for half a year while a lane lay open from it.
    const g = turned();
    g.flags['st:quay:UK'] = old.id + 1;
    g.flags['st:quayAt:UK'] = g.turn - 1;
    g.flags['st:quayFor:UK'] = 1;
    expect(seaBase(g, 'UK')?.id).toBe(best.id);
    expect(g.flags['st:quayAt:UK']).toBe(g.turn);
    // Chosen for this target, it is kept like any other choice.
    g.turn++;
    g.flags['st:quay:UK'] = old.id + 1;
    expect(seaBase(g, 'UK')?.id).toBe(old.id);
  });

  it('a home port its allies fill is passed over while another serves; with none, the full one is the quay as before', () => {
    const best = seaBase(turned(), 'UK')!;
    const g = turned();
    // Three American corps in the best harbour: two places are left of five, and the force is three.
    crowd(g, best, 3);
    const quay = seaBase(g, 'UK')!;
    expect(quay.id).not.toBe(best.id);
    expect(quay.owner).toBe('UK');
    // Two leave room for the force.
    const two = turned();
    crowd(two, best, 2);
    expect(seaBase(two, 'UK')?.id).toBe(best.id);
    // As first built the staff asked no room of its home quay.
    const was = turned();
    crowd(was, best, 3);
    expect(without('room', () => seaBase(was, 'UK'))?.id).toBe(best.id);
    // And a quay already chosen is left the month its allies fill it.
    const kept = turned();
    expect(seaBase(kept, 'UK')?.id).toBe(best.id);
    crowd(kept, best, 3);
    kept.turn++;
    expect(seaBase(kept, 'UK')?.id).not.toBe(best.id);
  });
});

describe('an army abroad names the quay it stands at (233(y))', () => {
  /** America at war beside Britain in May 1944, France in German hands and every sea open: its corps idle in Britain, `west` of them in Cornwall and `east` in Kent. */
  const inBritain = (west: number, east: number): { g: Game; cornwall: Region; kent: Region } => {
    const g = startOpen(createGame(3, null));
    g.nations.USA.align = g.nations.UK.align;
    expect(allied(g, 'USA', 'UK')).toBe(true);
    for (const [a, b] of [['GER', 'UK'], ['GER', 'USA']]) setWar(g, a, b);
    for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas && r.nb.length > 0) r.ctrl = 'GER';
    g.turn = FRANCE_44;
    const cornwall = g.byName.Cornwall, kent = g.byName.Kent;
    const us = g.corps.filter((x) => x.nat === 'USA' && !x.dead && x.kind !== 'fort' && x.kind !== 'eng' && !x.town);
    expect(us.length).toBeGreaterThanOrEqual(west + east);
    us.slice(0, west + east).forEach((c, i) => {
      const from = g.regions[c.reg], at = i < west ? cornwall : kent;
      from.units.splice(from.units.indexOf(c), 1);
      c.reg = at.id;
      at.units.push(c);
    });
    return { g, cornwall, kent };
  };

  it('is in the data', () => {
    expect(DESCENT.stands).toBe(true);
  });

  it('of two quays equal in worth and in seas, the one with more of its corps on it', () => {
    // (Four corps on the quay at Kent; Hampshire, a march away, is as good a port and comes first in the list.)
    const { g, kent } = inBritain(2, 4);
    expect(seaFoe(g, 'USA', 1)).toMatchObject({ foe: 'GER', second: true });
    expect(seaBase(g, 'USA', 1)).toBe(kent);
    // (And the other way round: the army stands in Cornwall, and Cornwall is named.)
    const b = inBritain(4, 2);
    expect(seaFoe(b.g, 'USA', 1)?.foe).toBe('GER');
    expect(seaBase(b.g, 'USA', 1)).toBe(b.cornwall);
  });

  it('as first built (the dial off) the first port of the list won the tie: a quay with none of its corps on it', () => {
    DESCENT.stands = false;
    try {
      const { g, kent } = inBritain(2, 4);
      expect(seaFoe(g, 'USA', 1)?.foe).toBe('GER');
      const first = seaBase(g, 'USA', 1)!;
      expect(first).not.toBe(kent);
      expect(first.units.filter((u) => u.nat === 'USA')).toHaveLength(0);
    } finally { DESCENT.stands = true; }
  });
});
