// The globe's eight seats (ruling 209): South Africa, Iran, Iraq, Saudi Arabia, Afghanistan, Yemen, Nepal and
// Liberia, each on the door's second row from September 1939. Their rows wait in data/campaign/far-seats-globe.json,
// keyed by the file each block is lifted into when the globe's stage of the map is attached. Nothing in the game
// reads that file yet.
//
// The first half runs today: the rows are in the forms the game's own files use, and it holds each to Brian's rule
// (a smaller country is never balanced toward the great powers) and to the door's sheet (45 words, a true path).
// The second half runs by itself once the rows are in world.json and the wider map is attached: it opens a campaign
// from each seat. Until then the proving rig on the builder's branch plays a year from each (tools/trial/).
import { describe, expect, it } from 'vitest';
import seats from '../data/campaign/far-seats-globe.json';
import grandData from '../data/campaign/grand.json';
import leagueData from '../data/campaign/league.json';
import nationsData from '../data/campaign/nations.json';
import worldData from '../data/campaign/world.json';
import { income } from '../src/campaign/economy';
import { atWar, bloc, createGame } from '../src/campaign/game';
import { partName, pathOptions } from '../src/campaign/league';
import { startOpen } from '../src/campaign/staff';
import { traitRule } from '../src/campaign/traits';
import { KIND, type Game } from '../src/campaign/types';

const EIGHT = ['SAF', 'IRN', 'IRQ', 'SAU', 'AFG', 'YEM', 'NEP', 'LBR'];
/** Iran and Iraq are countries of the old chart already (nations.json): the other six are new. */
const NEW = ['SAF', 'SAU', 'AFG', 'YEM', 'NEP', 'LBR'];
const S = seats as any;
const W = worldData as any;
/** True once the rows have been lifted into the game's own files (the real merge, or the trial's overlay). */
const merged = W.nations.SAF !== undefined;
type Row = [string, string, string, number, string, { xp?: number; str?: number; hold?: number }];
const corps = S.world.corps as Row[];
const divs = (id: string): number => corps.filter((c) => c[0] === id).reduce((s, c) => s + c[3], 0);
const NUMBERS = ['attack', 'defence', 'income', 'incomeMul', 'oil', 'relocate', 'drafts', 'cost', 'sealift', 'alert', 'partisans', 'fleet', 'seaLoss'];

describe('the rows as they wait', () => {
  it('has the eight seats and no other, each small, from 1939 only, in 45 words or fewer', () => {
    expect(S.grand.nations.map((n: any) => n.id)).toEqual(EIGHT);
    for (const n of S.grand.nations) {
      expect(n.small, n.id).toBe(true);
      expect(Object.keys(n.years), n.id).toEqual(['1939']);
      // The door's own floor, and the 45 words a seat's own text is held to on the new door (sheet 33).
      expect(n.years['1939'].length, n.id).toBeGreaterThan(40);
      expect(n.years['1939'].split(/\s+/).length, n.id).toBeLessThanOrEqual(45);
    }
    for (const id of EIGHT) expect((leagueData as any).never, id).not.toContain(id);
    if (!merged) { const door = (grandData as any).nations.map((n: any) => n.id); for (const id of EIGHT) expect(door, id).not.toContain(id); }
  });

  it('says a true path on the door for every seat', () => {
    expect(Object.keys(S.door.path)).toEqual(EIGHT);
    // The two that follow Britain are in the Allies for good: neither is offered the four choices.
    for (const id of ['SAF', 'NEP']) { expect(S.door.path[id], id).toMatch(/a member of the Allies for good/); expect(S.door.path[id], id).not.toMatch(/Join the Axis/); }
    // Liberia has no free neighbour in Africa: it is not told it can found a League.
    expect(S.door.path.LBR).not.toMatch(/found a League/);
    for (const id of ['IRN', 'IRQ', 'SAU', 'AFG', 'YEM']) expect(S.door.path[id], id).toMatch(/Join the Axis, join the Allies, stand alone, or found a League with its neighbours\./);
    expect(S.door.path.IRQ).toMatch(/^Neutral, and leaning toward the Allies/);
  });

  it('writes each new nation in the form world.json has, weaker than any great power, with a part of the world', () => {
    const keys = new Set(['name', 'adj', 'color', 'quality', 'air', 'resilience', 'align', 'seaSupply', 'sealift']);
    expect(Object.keys(S.world.nations)).toEqual(NEW);
    expect(S.world.order).toEqual(NEW);
    for (const id of NEW) {
      const n = S.world.nations[id];
      for (const k of Object.keys(n)) expect(keys.has(k), `${id}.${k}`).toBe(true);
      expect(n.color, id).toMatch(/^#[0-9a-f]{6}$/);
      expect(n.quality, id).toBeGreaterThan(0); expect(n.quality, id).toBeLessThan(0.9);
      expect(n.resilience, id).toBeGreaterThan(0); expect(n.resilience, id).toBeLessThan(1);
      // An air force by the record: none has as many points as Canada's 8.
      expect(n.air, id).toBeLessThan(W.nations.CAN.air);
      expect(['allies', 'independent'], id).toContain(n.align);
    }
    // Iran and Iraq keep their rows of the old chart: a row in world.json for either would not be read.
    for (const id of ['IRN', 'IRQ']) { expect(S.world.nations[id], id).toBeUndefined(); expect((nationsData as any)[id] ?? (nationsData as any).nations?.[id], id).toBeDefined(); }
    // The League's parts as ruled (15:30Z). A new country falls into 'the Americas' unless its part is named.
    expect(S.world.parts.of).toEqual({ SAF: 'africa', LBR: 'africa', IRN: 'mideast', IRQ: 'mideast', SAU: 'mideast', YEM: 'mideast', AFG: 'mideast', NEP: 'asia' });
    expect(S.world.parts.names).toEqual({ africa: 'Africa', mideast: 'the Middle East', asia: 'Asia' });
  });

  it('gives every seat an army by the record: thin, small, and never more than three corps to a province', () => {
    const at: Record<string, number> = {};
    for (const [nat, name, kind, n, place, o] of corps) {
      expect(EIGHT, name).toContain(nat);
      expect(name.length, name).toBeGreaterThan(3);
      expect((KIND as any)[kind], name).toBeDefined();
      expect(n, name).toBeGreaterThan(0);
      // Not one corps of the eight stands at full strength on the first day.
      expect(o.str ?? 1, name).toBeGreaterThan(0); expect(o.str ?? 1, name).toBeLessThan(1);
      at[place] = (at[place] ?? 0) + 1;
    }
    for (const [place, k] of Object.entries(at)) expect(k, place).toBeLessThanOrEqual(3);
    expect(Object.fromEntries(EIGHT.map((id) => [id, divs(id)]))).toEqual({ SAF: 2, IRN: 9, IRQ: 4, SAU: 3, AFG: 4, YEM: 1, NEP: 1, LBR: 1 });
    // A computer-run staff at peace raises no more than the army the country had.
    for (const id of EIGHT) expect(S.world.idle[id], id).toBe(corps.filter((c) => c[0] === id).length);
    // No armour, no motor troops: the record has none worth a row.
    for (const c of corps) expect(['inf', 'cav'], c[1]).toContain(c[2]);
  });

  it('gives Iran alone a fleet, of two sloops, in the form navy.json has', () => {
    expect(S.world.fleets.map((f: any) => f.nat)).toEqual(['IRN']);
    const f = S.world.fleets[0];
    expect(f.name).toMatch(/Fleet$/);
    expect(f.ships.dd).toBe(2);
    expect((f.ships.bb ?? 0) + (f.ships.cv ?? 0) + (f.ships.ca ?? 0) + (f.ships.ss ?? 0)).toBe(0);
    expect(f.admiral.length).toBeGreaterThan(3);
    // South Africa had no naval vessels in 1939, and no shipping of its own to carry an army: by the record.
    expect(S.world.nations.SAF.sealift).toBeUndefined();
    expect(S.world.nations.SAF.seaSupply).toBe(true);
  });

  it('adds no number in a seat\'s favour beyond the record\'s one ability, and none at all for four seats', () => {
    const live = (id: string): any[] => (S.traits.nations[id]?.traits ?? []).filter((t: any) => !t.builtin);
    expect(Object.fromEntries(EIGHT.map((id) => [id, live(id).map((t: any) => t.id)]))).toEqual({
      SAF: ['gold'], IRN: [], IRQ: [], SAU: [], AFG: ['passes'], YEM: ['highlands'], NEP: ['hillmen'], LBR: [],
    });
    for (const id of EIGHT) expect(S.traits.nations[id]?.unit, id).toBeUndefined();
    // None is stronger than the row of Europe it was measured against (Canada's wheat, Turkey's Anatolia).
    expect(live('SAF')[0].income).toBeLessThanOrEqual(0.8);
    for (const id of ['AFG', 'YEM']) { expect(live(id)[0].defence, id).toBeLessThanOrEqual(1.2); expect(live(id)[0].when.home, id).toBe(true); }
    expect(live('NEP')[0].attack).toBeLessThanOrEqual(1.15);
  });

  it('writes every ability row well formed, and every row of words as a rule the game already has', () => {
    for (const id of EIGHT) {
      const row = S.traits.nations[id];
      // The door's first gate (traits.test.ts): a row for every seat.
      expect(row.traits.length, id).toBeGreaterThan(0);
      expect(new Set(row.traits.map((t: any) => t.id)).size, id).toBe(row.traits.length);
      for (const t of row.traits) {
        expect(t.name.length, `${id}.${t.id}`).toBeGreaterThan(3);
        expect(t.text.length, `${id}.${t.id}`).toBeGreaterThan(20);
        for (const m of [t.attack, t.defence, t.fleet, t.drafts, t.incomeMul]) if (m !== undefined) { expect(m).toBeGreaterThan(1); expect(m).toBeLessThanOrEqual(1.5); }
        const rules = NUMBERS.filter((k) => t[k] !== undefined);
        expect(t.builtin ? rules.length === 0 && !!t.rule : rules.length > 0 && !t.rule, `${id}.${t.id}`).toBe(true);
        expect(traitRule(t).length, `${id}.${t.id}`).toBeGreaterThan(10);
      }
    }
    // The door's second gate asks two entries of every country. Five seats reach it. Saudi Arabia, Yemen and Liberia
    // have one, because nothing more is true of them: the gate or the rows wait for the design director's ruling.
    const count = (id: string): number => S.traits.nations[id].traits.length;
    for (const id of ['SAF', 'IRN', 'IRQ', 'AFG', 'NEP']) expect(count(id), id).toBe(2);
    for (const id of ['SAU', 'YEM', 'LBR']) expect(count(id), id).toBe(1);
  });

  it('binds South Africa and Nepal as Canada is bound, and nobody else', () => {
    expect(W.follow.CAN).toBe('UK');
    expect(S.world.follow).toEqual({ SAF: 'UK', NEP: 'UK' });
    for (const id of ['SAF', 'NEP']) expect(S.world.nations[id].align, id).toBe(W.nations.CAN.align);
    for (const id of ['SAU', 'AFG', 'YEM', 'LBR']) expect(S.world.nations[id].align, id).toBe('independent');
    // The Saadabad circle holds the three signers of the Middle East; Turkey, in Europe's part, is each one's friend
    // by a pair, so that no computer-run League is founded across two parts of the world (ruling 58).
    expect(S.relations.groups.map((g: any) => g.nations)).toEqual([['IRN', 'IRQ', 'AFG']]);
    for (const g of S.relations.groups) expect(new Set(g.nations.map((id: string) => S.world.parts.of[id])).size, g.id).toBe(1);
    for (const id of ['IRN', 'IRQ', 'AFG']) expect(S.relations.pairs.some((p: string[]) => p[0] === 'TUR' && p[1] === id), id).toBe(true);
    // The computer's path by the book: Iraq joins the Allies from January 1943, Liberia from January 1944.
    expect(S.diplomacy.events.map((e: any) => [e.id, e.from])).toEqual([['iraq-war', 41], ['liberia-war', 53]]);
    for (const e of S.diplomacy.events) { expect(e.mode).toBe('open'); expect(e.enter.align).toBe('allies'); }
  });

  it("gives every seat the clerk's two name lists for its officers, in the form commanders.json has", () => {
    expect(Object.keys(S.commanders.names)).toEqual(EIGHT);
    for (const id of EIGHT) {
      const { first, last } = S.commanders.names[id];
      expect(first.length, id).toBeGreaterThanOrEqual(6);
      expect(last.length, id).toBeGreaterThanOrEqual(6);
    }
  });
});

// ---- The globe's stage: the rows are in the game's own files and the wider map is attached ----------------------

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));

describe.runIf(merged)('the seats on the map (the globe\'s stage)', () => {
  it('opens a campaign from each seat with its whole army on its own ground and its capital in its hands', () => {
    for (const id of EIGHT) {
      const g = grand(id);
      const n = g.nations[id];
      expect(n.alive, id).toBe(true);
      expect(g.regions[n.capital].ctrl, id).toBe(id);
      const mine = g.corps.filter((c) => c.nat === id && !c.dead);
      expect(mine.length, id).toBe(corps.filter((c) => c[0] === id).length);
      for (const c of mine) expect(g.regions[c.reg].ctrl, `${id} ${c.name}`).toBe(id);
      expect(income(g, id), id).toBeGreaterThan(0);
    }
    // The capitals the record names, not the old chart's stand-ins.
    for (const [id, name] of [['IRN', 'Tehran'], ['IRQ', 'Baghdad'], ['SAF', 'Pretoria']]) { const g = grand(id); expect(g.regions[g.nations[id].capital].name, id).toBe(name); }
  });

  it('puts South Africa and Nepal at war with Germany on the first day, and leaves the other six at peace in no alliance', () => {
    const g = grand();
    for (const id of ['SAF', 'NEP']) { expect(atWar(g, id, 'GER'), id).toBe(true); expect(bloc(g.nations[id]), id).toBe('allies'); }
    for (const id of ['IRN', 'IRQ', 'SAU', 'AFG', 'YEM', 'LBR']) { expect(g.nations[id].atWar, id).toEqual([]); expect(bloc(g.nations[id]), id).toBeNull(); }
  });

  it('files each under its own part of the world, and offers each the paths the door says it has', () => {
    expect(Object.fromEntries(EIGHT.map((id) => [id, partName(id)]))).toEqual({
      SAF: 'Africa', LBR: 'Africa', IRN: 'the Middle East', IRQ: 'the Middle East', SAU: 'the Middle East', YEM: 'the Middle East',
      AFG: 'the Middle East', NEP: 'Asia',
    });
    const open = (id: string): string[] => pathOptions(grand(id), id).filter((o) => o.ok && !o.now).map((o) => o.id).sort();
    // An alliance, once joined, is not left.
    for (const id of ['SAF', 'NEP']) expect(open(id), id).toEqual([]);
    for (const id of ['IRN', 'SAU', 'AFG', 'YEM']) expect(open(id), id).toEqual(['allies', 'axis', 'found']);
    expect(open('IRQ')).toEqual(['alone', 'allies', 'axis', 'found'].sort());
    // Africa holds Liberia and a dominion: there is nobody to found a League with.
    expect(open('LBR')).toEqual(['allies', 'axis']);
  });

  it('keeps the smaller powers small beside Germany on the first day: in money, and far more so in men under arms', () => {
    const g = grand();
    const ger = income(g, 'GER');
    const army = (id: string): number => g.corps.filter((c) => c.nat === id && !c.dead).reduce((s, c) => s + c.divs * c.str, 0);
    // South Africa, a dominion at war with gold and two factories, earns under a fifth of what Germany does (about
    // an eighth once ruling 214's land factor is in); no other seat earns a tenth, and the five poorest earn less
    // than a twentieth, with or without the factor (with it, Saudi Arabia joins them).
    for (const id of EIGHT) expect(income(g, id) / ger, id).toBeLessThan(id === 'SAF' ? 0.25 : 0.1);
    for (const id of ['IRQ', 'AFG', 'YEM', 'NEP', 'LBR']) expect(income(g, id) / ger, id).toBeLessThan(0.05);
    // Iran's nine thin divisions are the largest army of the eight: a sixteenth of Germany's. The rest are under a fortieth.
    for (const id of EIGHT) expect(army(id) / army('GER'), id).toBeLessThan(id === 'IRN' ? 0.07 : 0.03);
  });
});
