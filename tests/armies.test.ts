// An army opened up. A force in a province is a stack of corps, and each corps is so many divisions of one kind:
// both interfaces can ask what an army is made of arm by arm, and get a card for every division with its number and
// name, its strength, and how seasoned it is. The divisions always add up to exactly the corps, and nothing about
// them changes a battle.
import { describe, expect, it } from 'vitest';
import data from '../data/campaign/divisions.json';
import { addCorps, createGame, men, removeCorps } from '../src/campaign/game';
import { DIVISIONS, armyCard, corpsCard, divisionWords, divisionsOf, mixLine, mixOf, numberDivisions, ordinal } from '../src/campaign/armies';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { armiesAt, armyAt, corpsDetail, orderFog, runTurnAI, sight } from '../src/campaign/turn';
import { KIND, type Corps, type Game, type Kind } from '../src/campaign/types';

const open = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const mean = (xs: number[]): number => xs.reduce((s, x) => s + x, 0) / xs.length;

describe('the division data', () => {
  it('names every kind of corps, and lists every kind in the order armies are summed up', () => {
    for (const k of Object.keys(KIND) as Kind[]) {
      expect(data.kinds[k].word, k).toBeTruthy();
      expect(data.kinds[k].name, k).toMatch(/Division|Brigade/);
      expect(DIVISIONS.order).toContain(k);
    }
    expect(new Set(DIVISIONS.order).size).toBe(DIVISIONS.order.length);
    expect(data.spread.str).toBeLessThanOrEqual(1);
    expect(data.spread.xp).toBeLessThanOrEqual(1);
  });

  it('counts in English', () => {
    expect([1, 2, 3, 4, 11, 12, 13, 21, 22, 23, 101, 111, 112, 123].map(ordinal)).toEqual(['1st', '2nd', '3rd', '4th', '11th', '12th', '13th', '21st', '22nd', '23rd', '101st', '111th', '112th', '123rd']);
  });

  it('lets a nation call its own divisions what it called them', () => {
    expect(divisionWords('GER', 'arm')).toEqual({ name: 'Panzer Division', short: 'Panzer' });
    expect(divisionWords('SOV', 'inf').name).toBe('Rifle Division');
    expect(divisionWords('SOV', 'arm').name).toBe('Tank Division');
    expect(divisionWords('USA', 'arm').name).toBe('Armored Division');
    expect(divisionWords('UK', 'arm').name).toBe('Armoured Division');
    expect(divisionWords('FRA', 'inf').name).toBe('Infantry Division');
  });
});

describe('the divisions of a corps', () => {
  it('are as many as the corps has, each with a number, a name and a card', () => {
    const g = open();
    const c = g.corps.find((x) => x.nat === 'GER' && x.kind === 'arm')!;
    const ds = divisionsOf(g, c);
    expect(ds).toHaveLength(c.divs);
    ds.forEach((d, i) => {
      expect(d.slot).toBe(i);
      expect(d.no).toBe(c.dn! + i);
      expect(d.name).toBe(`${ordinal(d.no)} Panzer Division`);
      expect(d.short).toBe(`${ordinal(d.no)} Panzer`);
      expect(d.kind).toBe('arm');
      expect(d.nat).toBe('GER');
      expect(d.grade).toBeTruthy();
      expect(d.stripes).toBeGreaterThanOrEqual(0);
      expect(d.stripes).toBeLessThanOrEqual(4);
    });
  });

  it('add up to exactly the corps, in strength and in experience, however battered or seasoned it is', () => {
    const g = open();
    const cs = g.corps.filter((x) => x.divs >= 2).slice(0, 40);
    for (const [str, xp] of [[1, 0.15], [0.83, 0.3], [0.5, 0.5], [0.12, 0.9], [0.99, 0.02], [0.6, 1]] as const) {
      for (const c of cs) {
        c.str = str; c.xp = xp;
        const ds = divisionsOf(g, c);
        expect(mean(ds.map((d) => d.str))).toBeCloseTo(str, 9);
        expect(mean(ds.map((d) => d.xp))).toBeCloseTo(xp, 9);
        for (const d of ds) { expect(d.str).toBeGreaterThanOrEqual(0); expect(d.str).toBeLessThanOrEqual(1); expect(d.xp).toBeGreaterThanOrEqual(0); expect(d.xp).toBeLessThanOrEqual(1); }
        expect(Math.abs(ds.reduce((s, d) => s + d.men, 0) - men(c))).toBeLessThanOrEqual(ds.length);
      }
    }
  });

  it('differ from one another once the corps is worn, and stand level when it is whole', () => {
    const g = open();
    const c = g.corps.find((x) => x.divs >= 3)!;
    c.str = 1; c.xp = 0;
    expect(new Set(divisionsOf(g, c).map((d) => d.str))).toEqual(new Set([1]));
    expect(new Set(divisionsOf(g, c).map((d) => d.xp))).toEqual(new Set([0]));
    c.str = 0.6; c.xp = 0.4;
    const ds = divisionsOf(g, c);
    expect(Math.max(...ds.map((d) => d.str)) - Math.min(...ds.map((d) => d.str))).toBeGreaterThan(0.1);
    expect(Math.max(...ds.map((d) => d.xp)) - Math.min(...ds.map((d) => d.xp))).toBeGreaterThan(0.1);
    // The same division is the most worn every time it is asked, and the corps is none the worse for the asking.
    expect(divisionsOf(g, c)).toEqual(ds);
    expect(c.str).toBe(0.6);
  });

  it('count as battle-hardened once they are seasoned', () => {
    const g = open();
    const c = g.corps.find((x) => x.divs >= 3)!;
    c.xp = 0.05;
    expect(divisionsOf(g, c).every((d) => !d.hardened && d.grade === 'Green')).toBe(true);
    expect(corpsCard(g, c).hardened).toBe(false);
    expect(corpsCard(g, c).service).toBe('Green · untested');
    c.xp = 0.9;
    c.rec = { battles: 6, won: 5 };
    expect(divisionsOf(g, c).every((d) => d.hardened && d.stripes >= 3)).toBe(true);
    const card = corpsCard(g, c);
    expect(card.hardened).toBe(true);
    expect(card.battles).toBe(6);
    expect(card.won).toBe(5);
    expect(card.service).toBe('Elite, battle-hardened · 6 battles');
  });
});

describe('the numbers divisions carry', () => {
  it('are each nation\'s own, one run for each arm, with no number used twice', () => {
    const g = open();
    const seen = new Map<string, Set<number>>();
    for (const c of g.corps) {
      expect(c.dn, c.name).toBeGreaterThanOrEqual(1);
      const key = `${c.nat}:${c.kind}`;
      const set = seen.get(key) ?? seen.set(key, new Set()).get(key)!;
      for (const d of divisionsOf(g, c)) { expect(set.has(d.no), `${key} ${d.no}`).toBe(false); set.add(d.no); }
    }
    // Each run starts at 1 and has no gaps.
    for (const [key, set] of seen) expect(Math.max(...set), key).toBe(set.size);
  });

  it('go on from the last one raised, and are never handed out again when a corps is lost', () => {
    const g = open();
    const before = Math.max(...g.corps.filter((c) => c.nat === 'GER' && c.kind === 'inf').flatMap((c) => divisionsOf(g, c).map((d) => d.no)));
    const lost = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf')!;
    const kept = g.corps.filter((c) => c.nat === 'GER' && c.kind === 'inf' && c !== lost).map((c) => [c.id, divisionsOf(g, c).map((d) => d.no)] as const);
    removeCorps(g, lost);
    const c = addCorps(g, 'GER', 'New Corps', 'inf', 3, g.byName['Berlin'].id, {});
    expect(divisionsOf(g, c).map((d) => d.no)).toEqual([before + 1, before + 2, before + 3]);
    for (const [id, nos] of kept) expect(divisionsOf(g, g.corps.find((x) => x.id === id)!).map((d) => d.no)).toEqual(nos);
  });

  it('are kept by a save, and an older save of a grand campaign is numbered when it is loaded', () => {
    const g = open();
    for (let k = 0; k < 2; k++) runTurnAI(g);
    const back = loadGame(saveGame(g))!;
    for (const c of g.corps) expect(back.corps.find((x) => x.id === c.id)!.dn).toBe(c.dn);
    const old = JSON.parse(saveGame(g));
    for (const x of old.corps) delete x.dn;
    for (const k of Object.keys(old.flags)) if (k.startsWith('dn:')) delete old.flags[k];
    const lifted = loadGame(JSON.stringify(old))!;
    expect(lifted.corps.every((x) => x.dn !== undefined)).toBe(true);
    const nos = lifted.corps.filter((c) => c.nat === 'SOV' && c.kind === 'inf').flatMap((c) => divisionsOf(lifted, c).map((d) => d.no));
    expect(new Set(nos).size).toBe(nos.length);
    // Numbering twice changes nothing.
    const snap = lifted.corps.map((c) => c.dn);
    numberDivisions(lifted);
    expect(lifted.corps.map((c) => c.dn)).toEqual(snap);
  });

  it('are worked out as the armies stand in a historical scenario, which keeps none', () => {
    const g = createGame(1, null);
    for (let k = 0; k < 2; k++) runTurnAI(g);
    expect(g.corps.every((c) => c.dn === undefined)).toBe(true);
    expect(Object.keys(g.flags).some((k) => k.startsWith('dn:'))).toBe(false);
    const nos = g.corps.filter((c) => c.nat === 'GER' && c.kind === 'inf').flatMap((c) => divisionsOf(g, c).map((d) => d.no));
    expect(new Set(nos).size).toBe(nos.length);
    expect(Math.min(...nos)).toBe(1);
    expect(g.corps.every((c) => c.dn === undefined)).toBe(true);
  });
});

describe('an army as the player reads it', () => {
  /** A province emptied and filled with a German army of several arms. */
  const staged = (g: Game): { reg: number; corps: Corps[] } => {
    const r = g.byName['Berlin'];
    for (const c of [...r.units]) removeCorps(g, c);
    const corps = [
      addCorps(g, 'GER', 'A Corps', 'inf', 3, r.id, {}), addCorps(g, 'GER', 'B Corps', 'inf', 2, r.id, {}),
      addCorps(g, 'GER', 'C Corps', 'arm', 2, r.id, {}), addCorps(g, 'GER', 'D Corps', 'arm', 2, r.id, {}),
      addCorps(g, 'GER', 'E Corps', 'art', 2, r.id, {}),
    ];
    return { reg: r.id, corps };
  };

  it('says what it is made of, arm by arm, in the words the player asked for', () => {
    const g = open('GER');
    const { reg, corps } = staged(g);
    const a = armyAt(g, reg, 'GER')!;
    expect(a.known).toBe('full');
    expect(a.name).toBe('German army at Berlin');
    expect(a.line).toBe('4 armoured, 5 infantry and 2 artillery divisions');
    expect(a.mix.map((m) => [m.kind, m.divs])).toEqual([['arm', 4], ['inf', 5], ['art', 2]]);
    expect(a.divs).toBe(11);
    expect(a.corpsCount).toBe(5);
    expect(a.men).toBe(corps.reduce((s, c) => s + men(c), 0));
    expect(a.str).toBeCloseTo(1, 9);
    expect(a.morale).not.toBeNull();
    expect(a.moraleWord).toBeTruthy();
    expect(a.rest).toBeCloseTo(1, 9);
    expect(a.restWord).toBe('Fresh');
    // Tanks first, then the infantry, then the guns.
    expect(a.corps.map((c) => c.kind)).toEqual(['arm', 'arm', 'inf', 'inf', 'art']);
    expect(a.corps.reduce((s, c) => s + c.divisions.length, 0)).toBe(11);
    expect(a.corps[0].divisions[0].name).toMatch(/Panzer Division$/);
    expect(a.corps[0].mood.morale).not.toBeNull();
    expect(corpsDetail(g, corps[0]).divisions).toEqual(a.corps.find((c) => c.id === corps[0].id)!.divisions);
  });

  it('weighs strength, rest and morale by the divisions that carry them', () => {
    const g = open('GER');
    const { reg, corps } = staged(g);
    corps[0].str = 0.4; corps[0].org = 0.2; corps[0].mor = 0.2;
    const a = armyAt(g, reg, 'GER')!;
    expect(a.str).toBeCloseTo((0.4 * 3 + 8) / 11, 9);
    expect(a.rest).toBeCloseTo((0.2 * 3 + 8) / 11, 9);
    expect(a.morale!).toBeCloseTo((0.2 * 3 + corps[1].mor! * 8) / 11, 9);
  });

  it('counts in the singular, and counts engineers in brigades', () => {
    const g = open();
    const one = [g.corps.find((c) => c.kind === 'inf' && c.divs === 1) ?? Object.assign({}, g.corps.find((c) => c.kind === 'inf')!, { divs: 1 })];
    expect(mixLine(mixOf(one as Corps[]))).toBe('1 infantry division');
    const eng = [Object.assign({}, g.corps[0], { kind: 'eng' as Kind, divs: 2 })];
    expect(mixLine(mixOf(eng))).toBe('2 engineer brigades');
    expect(mixLine([])).toBe('No troops');
  });

  it('is known in full to its own side, by its flag alone at a glimpse, and not at all in the dark', () => {
    const g = open('UK');
    orderFog(g, true);
    // An army deep in country Britain cannot see is not known at all.
    const seenAs = sight(g);
    const far = g.regions.find((r) => seenAs[r.id] === 0 && r.units.length > 0)!;
    const who = far.units[0].nat;
    expect(armyAt(g, far.id, who)).toBeNull();
    expect(armiesAt(g, far.id)).toEqual([]);
    // Britain's own are always known, and so are its allies'.
    const mine = g.regions.find((r) => r.units.some((c) => c.nat === 'UK'))!;
    expect(armyAt(g, mine.id, 'UK')!.known).toBe('full');
    const french = g.regions.find((r) => r.units.some((c) => c.nat === 'FRA'))!;
    expect(armyAt(g, french.id, 'FRA')!.known).toBe('full');
    // With the fog lifted, everything is.
    orderFog(g, false);
    const seen = armyAt(g, far.id, who)!;
    expect(seen.known).toBe('full');
    expect(seen.corps.length).toBe(far.units.filter((c) => c.nat === who).length);
    // A glimpse gives the flag and the count and nothing else.
    orderFog(g, true);
    const dim = g.regions.find((r) => seenAs[r.id] === 1 && r.units.length > 0)!;
    const glimpsed = armyAt(g, dim.id, dim.units[0].nat)!;
    expect(glimpsed.known).toBe('count');
    expect(glimpsed.corpsCount).toBe(dim.units.filter((c) => c.nat === dim.units[0].nat).length);
    expect(glimpsed.corps).toEqual([]);
    expect(glimpsed.mix).toEqual([]);
    expect(glimpsed.line).toBe('');
    expect(glimpsed.morale).toBeNull();
    // Nobody at the controls sees everything.
    expect(armyCard(g, far.id, who, null)!.known).toBe('full');
  });

  it('lists the armies in a province with the player\'s own first', () => {
    const g = open('GER');
    const { reg } = staged(g);
    addCorps(g, 'ITA', 'Italian Corps', 'inf', 3, reg, {});
    const all = armiesAt(g, reg);
    expect(all.map((a) => a.nat)).toEqual(['GER', 'ITA']);
    expect(all[1].line).toBe('3 infantry divisions');
    expect(armyAt(g, reg, 'FRA')).toBeNull();
    expect(armyAt(g, 99999, 'GER')).toBeNull();
  });

  it('opens up in a historical scenario too, with rest and no morale', () => {
    const g = createGame(1, 'GER');
    const r = g.regions.find((x) => x.units.some((c) => c.nat === 'GER'))!;
    const a = armyAt(g, r.id, 'GER')!;
    expect(a.known).toBe('full');
    expect(a.morale).toBeNull();
    expect(a.moraleWord).toBe('');
    expect(a.restWord).toBeTruthy();
    expect(a.corps[0].mood.morale).toBeNull();
    expect(a.corps[0].divisions.length).toBe(a.corps[0].divs);
    expect(a.line).toMatch(/divisions?$/);
  });
});
