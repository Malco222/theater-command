// Sniper teams on the battle card (release 2, behind `snipers`, off; src/campaign/snipercard.ts).
//
// With sniper pairs on the field (withSnipers: each side's foot gives its doctrine's share to one to three pairs), the
// design director ruled that the card carries what they do by its causes and no fitted figure (26 November 2026, 01:49Z
// and 02:32Z): (a) after the lines meet a side's pairs count at their worth of the foot they were taken from, 0.07 in
// the attack and 0.10 in the defence (G); (b) the pin, where it lands, after the lines meet: the share of a side's foot
// the other's pairs reach fights at x (1 - pin.pace x bite) for its pinned part of the fight, LAND.pin.most seconds over
// the mean fight (G stand-ins: the card has no clock). Off, the card is the card it was; its key is its own; and the
// field is laid at the power before the term, so it is never taken off twice.
import { describe, expect, it } from 'vitest';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LAND, sniperShare } from '../src/battle/land/rules';
import { LandBattle } from '../src/battle/land/sim';
import { sampleTicket } from '../src/battle/land/sample';
import { withSnipers } from '../src/battle/land/snipers';
import type { FieldTicket, SideTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';
import { addCorps, assess, createGame, relocate } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { SNIPER_CARD, setSniperCard, sniperCard, sniperCardLine, sniperCardOn, type SniperCorps } from '../src/campaign/snipercard';
import { startOpen } from '../src/campaign/staff';
import { setTeamFire } from '../src/campaign/teamfire';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

type Ticket = FieldTicket<LandPlace>;

/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/** A Polish line in the country round Lodz and German corps in front of it, all test corps, in supply. */
function line(g: Game, defKind: Corps['kind'] = 'inf', attKind: Corps['kind'] = 'inf'): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  const def = [1, 2].map((i) => addCorps(g, 'POL', `Line ${i}`, defKind, 3, target.id, {}));
  const att = [1, 2].map((i) => addCorps(g, 'GER', `Storm ${i}`, attKind, 3, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  g.nations.GER.offStart.POL = g.turn - 6;
  g.nations.GER.lastFight.POL = g.turn - 1;
  return { target, from, def, att };
}

/** A set piece's ticket as a campaign battle's: without the piece's own battalions, its shares whole again. */
function campaign(key: string, seed = 11): Ticket {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, seed, null);
  const side = (s: SideTicket): SideTicket => ({ ...s, forces: s.forces.filter((f) => f.kind !== 'mg' && f.kind !== 'mor').map((f) => ({ ...f, share: f.share / 0.92 })) });
  return { ...t, att: side(t.att), def: side(t.def) };
}

const foot = (nat: string): SniperCorps[] => [['inf', 1, nat]];

describe('the switch', () => {
  it('is off in a new campaign, and off the card is the card it was', () => {
    const g = playing();
    const l = line(g);
    expect(sniperCardOn(g)).toBe(false);
    const before = assess(g, l.att, l.target);
    expect(before.snipers).toBeUndefined();
    setSniperCard(g, true);
    expect(sniperCardOn(g)).toBe(true);
    expect(assess(g, l.att, l.target).snipers).toBeDefined();
    setSniperCard(g, false);
    expect(g.flags.snipers).toBeUndefined();
    // on and off again: the same card, to the last figure and the last line
    expect(JSON.stringify(assess(g, l.att, l.target))).toBe(JSON.stringify(before));
  });

  it('has a key of its own: the whole save differs by it alone, and neither the teams\' key nor the brigades\' moves it', () => {
    const g = playing();
    const l = line(g);
    const before = saveGame(g);
    setSniperCard(g, true);
    const on = JSON.parse(saveGame(g));
    expect(on.flags.snipers).toBe(1);
    expect(on.flags.teamfire).toBeUndefined();
    expect(on.flags.teams).toBeUndefined();
    setSniperCard(g, false);
    expect(saveGame(g)).toBe(before);
    setTeamFire(g, true);
    g.flags.teams = 1;
    expect(sniperCardOn(g)).toBe(false);
    expect(assess(g, l.att, l.target).snipers).toBeUndefined();
  });
});

describe('the card with the switch on', () => {
  it('multiplies each side by its term and nothing else, and the odds by their ratio', () => {
    const g = playing();
    const l = line(g);
    const off = assess(g, l.att, l.target);
    setSniperCard(g, true);
    const on = assess(g, l.att, l.target);
    const k = on.snipers!;
    expect(on.A).toBeCloseTo(off.A * k.att, 9);
    expect(on.D).toBeCloseTo(off.D * k.def, 9);
    expect(on.odds).toBeCloseTo((off.odds * k.att) / k.def, 9);
    // German foot against Polish foot: each side's pairs by its own doctrine
    expect(k.pairs.att).toBeCloseTo(sniperShare('GER'), 12);
    expect(k.pairs.def).toBeCloseTo(sniperShare('POL'), 12);
    expect(k).toEqual(sniperCard({ att: foot('GER'), def: foot('POL') }));
    expect(on.dice).toEqual(off.dice);
    expect(on.defenders.map((c) => c.id)).toEqual(off.defenders.map((c) => c.id));
    expect(on.lines.filter((x) => x.label !== 'Snipers')).toEqual(off.lines);
  });

  it('prices nothing where the fight is not a field battle: a town stormed, a landing, a sortie', () => {
    const g = playing();
    const l = line(g);
    setSniperCard(g, true);
    expect(assess(g, l.att, l.target, { town: true }).snipers).toBeUndefined();
    expect(assess(g, l.att, l.target, { sea: 0.5 }).snipers).toBeUndefined();
    expect(assess(g, l.att, l.target, { sally: true }).snipers).toBeUndefined();
  });

  it('tanks against tanks give no pairs: the card as it was', () => {
    const g = playing();
    const l = line(g, 'arm', 'arm');
    const off = assess(g, l.att, l.target);
    setSniperCard(g, true);
    const on = assess(g, l.att, l.target);
    expect(on.snipers).toBeUndefined();
    expect(JSON.stringify(on)).toBe(JSON.stringify(off));
  });
});

describe('the term by its two causes', () => {
  it('(a) counts each side\'s pairs at their worth of the foot they were taken from', () => {
    const W = SNIPER_CARD.worth;
    const k = sniperCard({ att: foot('SOV'), def: foot('USA') })!;
    expect(k.pairs).toEqual({ att: 0.03, def: 0.012 });
    expect(k.att).toBeCloseTo((1 - 0.03 * (1 - W.att)) * (1 - k.pinned.att), 12);
    expect(k.def).toBeCloseTo((1 - 0.012 * (1 - W.def)) * (1 - k.pinned.def), 12);
  });

  it('(b) slows the foot the other side\'s pairs reach, at pin.pace x bite, for pin.most of the mean fight', () => {
    const C = SNIPER_CARD, part = LAND.pin.most / C.fight;
    const k = sniperCard({ att: foot('GER'), def: foot('GER') })!;
    expect(k.pinned.att).toBeCloseTo(C.reached.att * LAND.snipers.pin.pace * C.bite.att * part, 12);
    expect(k.pinned.def).toBeCloseTo(C.reached.def * LAND.snipers.pin.pace * C.bite.def * part, 12);
    // one rule both ways; it falls on the attack, the side that walks
    expect(k.pinned.att).toBeGreaterThan(k.pinned.def);
    // only the foot is pinned, and only by the other side's pairs
    const half = sniperCard({ att: [['inf', 1, 'GER'], ['arm', 1, 'GER']], def: foot('GER') })!;
    expect(half.foot.att).toBe(0.5);
    expect(half.pinned.att).toBeCloseTo(k.pinned.att / 2, 12);
    const noPairs = sniperCard({ att: foot('GER'), def: [['arm', 1, 'GER']] })!;
    expect(noPairs.pinned.att).toBe(0);
    expect(noPairs.pairs.def).toBe(0);
  });

  it('takes nothing from a town\'s guard: a block of its own that gives no pairs', () => {
    const k = sniperCard({ att: foot('GER'), def: [['inf', 3, 'POL']], guard: 1 })!;
    expect(k.pairs.def).toBeCloseTo(0.75 * sniperShare('POL'), 12);
    expect(k.foot.def).toBeCloseTo(0.75, 12);
  });

  it('says so on the card in whole points, and says nothing under one', () => {
    const k = sniperCard({ att: foot('SOV'), def: foot('SOV') })!;
    expect(sniperCardLine({ ...k, att: 0.999, def: 0.998 })).toBeNull();
    expect(sniperCardLine({ ...k, att: 0.97, def: 0.99 })).toEqual({ label: 'Snipers', value: 'Sniper pairs pin men on the move and pick off their leaders', effect: 'Attack down 3%, defence down 1%' });
  });
});

describe('the figures marked G are the field\'s own', () => {
  it('a pair\'s bite on the foot it pins is its weight over theirs, as the field lays them (sim.ts bite)', () => {
    // The field's trace read the bite on the blocks it pinned, on the gate's own fields (F: seven grounds, sampleTicket);
    // here, on every pair and every block of the other side's foot as those fields are laid, at full strength.
    const grounds = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const;
    const bites: Record<'att' | 'def', number[]> = { att: [], def: [] };
    for (const g of grounds) for (const odds of [0.8, 1.2, 1.8]) for (const seed of [1, 2]) {
      const s = new LandBattle(withSnipers(sampleTicket(g, odds, { seed: seed * 7919 + 13 })) as never);
      for (const side of ['att', 'def'] as const) {
        const pairs = s.units.filter((u) => u.side !== side && u.team === 'snp');
        const blocks = s.units.filter((u) => u.side === side && !u.team && (LAND.snipers.foot as string[]).includes(u.kind));
        for (const p of pairs) for (const t of blocks) bites[side].push(Math.min(1, p.power0 / t.power0 / LAND.snipers.bite));
      }
    }
    for (const side of ['att', 'def'] as const) {
      expect(bites[side].length).toBeGreaterThan(100);
      const mean = bites[side].reduce((a, b) => a + b, 0) / bites[side].length;
      expect(Math.abs(mean - SNIPER_CARD.bite[side]) / SNIPER_CARD.bite[side]).toBeLessThan(0.15);
    }
  });
});

describe('the field is laid at the power before the term', () => {
  const power = (s: LandBattle): { att: number; def: number } => (s as unknown as { power: { att: number; def: number } }).power;

  it('with the term on the ticket and the pairs on the field, the blocks share out the power before it', () => {
    const t = campaign('normandy-1944');
    const k = { att: 0.99, def: 0.995 };
    const priced: Ticket = { ...t, reckoning: { ...t.reckoning, snipers: k } };
    const plain = new LandBattle(withSnipers(t) as never), on = new LandBattle(withSnipers(priced) as never);
    expect(power(on).att).toBeCloseTo(power(plain).att / k.att, 9);
    expect(power(on).def).toBeCloseTo(power(plain).def / k.def, 9);
    expect(withSnipers(priced).reckoning.odds).toBe(t.reckoning.odds);
  });

  it('priced but fought without the pairs on the field, it is laid at the card\'s odds as it always was', () => {
    const t = campaign('normandy-1944');
    const priced: Ticket = { ...t, reckoning: { ...t.reckoning, snipers: { att: 0.99, def: 0.995 } } };
    expect(power(new LandBattle(priced as never))).toEqual(power(new LandBattle(t as never)));
  });
});
