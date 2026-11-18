// Ruling 233, riders (j), (k), (q) and (r) (the design director's b506 and b510; the grand campaign, computer staffs only).
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
import { describe, expect, it } from 'vitest';
import fleetData from '../data/campaign/fleets.json';
import openData from '../data/campaign/open.json';
import { reckon, type Reckoning } from '../src/campaign/ashore';
import { allied, createGame, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { atSea, closedTo, fleetsOf, keepsHarbour, navalAI, sortieAgainst } from '../src/campaign/navy';
import { lodgment, opensOn, opensSoon, seaBase, seaFoe, startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const FRANCE_44 = 57;
const DESCENT = openData.descent as unknown as { lead: number; wait: boolean };
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
