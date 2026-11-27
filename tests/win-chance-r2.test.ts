// The honest win chance on land (release 2, auto-resolve fix 1; Brian's yes of 23 November 2026, the design director's
// order of 07:06Z and choice of measure at 07:27Z). Today an attack in the field cannot lose from 1.5 to 1 and cannot win
// under 1.2 to 1, so the attacker carried 93% of the grand campaign's field battles against about two in three in the
// record. With the switch on, an attack in the field is decided by a wider run of luck fitted to the record over combat
// power (data/campaign/winchance.json), and the same throw read in today's run sets what the day costs: the bill at
// given odds is what it was. Towns and guns caught alone keep today's luck. Off, nothing changes.
import { describe, expect, it } from 'vitest';
import {
  DEFENCE, FIELD_DICE, addCorps, assess, battleCost, chanceOn, createGame, evenOdds, luckOf, relocate, resolveBattle, sureWords, winChanceOn,
} from '../src/campaign/game';
import { SIEGE } from '../src/campaign/siege';
import { clampToDice, diceNow } from '../src/battle/core/result';
import { landTicket } from '../src/battle/land/ticket';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { WINCHANCE_STAGE } from '../src/campaign/stage';
import stage from '../data/campaign/stage.json';
import win from '../data/campaign/winchance.json';
import type { Corps, Game, Region } from '../src/campaign/types';

/** The grand campaign of September 1939, with the honest win chance on or off. */
function war(on: boolean, seed = 3): Game {
  const g = startOpen(createGame(seed, null));
  if (on) { g.flags.unitsDecide = 1; g.flags.winChance = 1; }
  return g;
}

/** The same with Germany's general at the table, his first month open, as the game's own screen would have it. */
function playing(on: boolean, seed = 3): Game {
  const g = startOpen(createGame(seed, 'GER'));
  if (on) { g.flags.unitsDecide = 1; g.flags.winChance = 1; }
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/** A Polish line round Lodz with German infantry in front of it, in supply. */
function line(g: Game, defenders = 2, attackers = 2, divs = 3): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  for (const u of [...from.units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
  const def = Array.from({ length: defenders }, (_, i) => addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 3, target.id, {}));
  const att = Array.from({ length: attackers }, (_, i) => addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', divs, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  return { target, from, def, att };
}
function again(g: Game, l: Line): { g: Game; l: Line } {
  const h = loadGame(saveGame(g))!;
  const find = (c: Corps): Corps => h.corps.find((x) => x.id === c.id)!;
  return { g: h, l: { target: h.regions[l.target.id], from: h.regions[l.from.id], def: l.def.map(find), att: l.att.map(find) } };
}
const dig = (cs: Corps[]): void => { for (const c of cs) { c.dug = true; c.dugFor = DEFENCE.entrench.months; } };

describe('the switch', () => {
  it('is off, and a grand campaign opened without it throws today\'s luck', () => {
    expect((stage as { winChance?: boolean }).winChance).toBe(false);
    expect(WINCHANCE_STAGE).toBe(false);
    const g = war(false);
    expect(winChanceOn(g)).toBe(false);
    const l = line(g);
    const a = assess(g, l.att, l.target);
    expect(a.dice).toEqual(FIELD_DICE);
    expect(a.luck).toBeUndefined();
  });

  it('is a step inside Units decide: it needs both flags, and either alone throws today\'s luck', () => {
    for (const [units, win] of [[1, 0], [0, 1]]) {
      const g = war(false);
      if (units) g.flags.unitsDecide = 1;
      if (win) g.flags.winChance = 1;
      expect(winChanceOn(g)).toBe(false);
      const l = line(g);
      expect(assess(g, l.att, l.target).luck).toBeUndefined();
    }
    expect(winChanceOn(war(true))).toBe(true);
  });
});

describe('the record\'s chance on the card', () => {
  it('in the open: 1 to 1 a little over half, 2 to 1 about four in five, 3 to 1 about 85%, and no odds sure', () => {
    const d = luckOf(0);
    expect(d).toEqual(win.field);
    expect(chanceOn(1, d)).toBeCloseTo(0.56, 2);
    expect(chanceOn(1.5, d)).toBeCloseTo(0.71, 2);
    expect(chanceOn(2, d)).toBeCloseTo(0.78, 2);
    expect(chanceOn(3, d)).toBeCloseTo(0.85, 2);
    expect(chanceOn(5, d)).toBeCloseTo(0.91, 2);
    expect(chanceOn(100, d)).toBeLessThan(1);
    expect(chanceOn(1.2 / d[1] - 0.01, d)).toBe(0);
    expect(evenOdds(d)).toBeCloseTo(0.88, 2);
    expect(sureWords(d)).toBe('no odds make it sure');
    expect(sureWords(FIELD_DICE)).toBe('nothing is sure under 1.5 to 1');
  });

  it('against a line dug in and fed, the even chance moves as far as today\'s prepared luck moves it', () => {
    const today = evenOdds(DEFENCE.prepared.dice) / evenOdds(FIELD_DICE);
    expect(evenOdds(luckOf(1)) / evenOdds(luckOf(0))).toBeCloseTo(today, 2);
    expect(luckOf(0.5)[1]).toBeCloseTo(win.field[1] * (1 - win.prepared / 2), 9);
    expect(evenOdds(luckOf(1))).toBeCloseTo(evenOdds(luckOf(0)) / (1 - win.prepared), 9);
  });
});

describe('the assessment', () => {
  it('in the field carries the wide run for the result and today\'s for the bill', () => {
    const g = war(true);
    const l = line(g);
    const a = assess(g, l.att, l.target);
    expect(a.prepared).toBe(0);
    expect(a.dice).toEqual(luckOf(0));
    expect(a.luck).toEqual(FIELD_DICE);
  });

  it('against a line dug in and fed, both runs follow the prepared share, and the ticket gives the chance in words', () => {
    const g = war(true);
    const l = line(g);
    dig(l.def);
    const a = assess(g, l.att, l.target);
    expect(a.prepared).toBeGreaterThan(0);
    expect(a.dice[0]).toBeCloseTo(luckOf(a.prepared)[0], 9);
    expect(a.dice[1]).toBeCloseTo(luckOf(a.prepared)[1], 9);
    expect(a.luck![0]).toBeCloseTo(FIELD_DICE[0] + (DEFENCE.prepared.dice[0] - FIELD_DICE[0]) * a.prepared, 9);
    const dug = a.lines.find((x) => x.label === 'Dug in')!;
    expect(dug.effect).toContain(`an even chance takes about ${evenOdds(a.dice).toFixed(1)} to 1, and no odds make it sure`);
    // Off, the words are today's.
    const h = war(false);
    const m = line(h);
    dig(m.def);
    const b = assess(h, m.att, m.target);
    expect(b.lines.find((x) => x.label === 'Dug in')!.effect).toContain(`nothing is sure under ${(1.2 / b.dice[0]).toFixed(1)} to 1`);
  });

  it('in the streets of a town keeps today\'s luck', () => {
    const g = war(true);
    const l = line(g);
    const a = assess(g, l.att, l.target, { town: true });
    expect(a.dice).toEqual([SIEGE.assault.dice[0], SIEGE.assault.dice[1]]);
    expect(a.luck).toBeUndefined();
  });

  it('the bill before the battle has both ends where either can happen', () => {
    const g = war(true);
    const l = line(g);
    const a = assess(g, l.att, l.target);
    const cost = battleCost(g, a, l.att, l.target, {}, 2);
    expect(cost.chance).toBeCloseTo(chanceOn(2, a.dice), 9);
    expect(cost.won).not.toBeNull();
    expect(cost.lost).not.toBeNull();
    // Off, 2 to 1 in the open cannot lose.
    const h = war(false);
    const m = line(h);
    expect(battleCost(h, assess(h, m.att, m.target), m.att, m.target, {}, 2).lost).toBeNull();
  });
});

describe('the battle', () => {
  // (Two German corps on two Polish ones are a close thing today; four on one cannot lose today, and with the record's
  // luck about one such attack in eight is thrown back.)
  for (const [defenders, attackers] of [[2, 2], [1, 4]] as const) {
    it(`${attackers} corps on ${defenders}: carries the ground as often as the chance says, and the attack pays to the man what it paid before`, () => {
      const on = war(true), off = war(false);
      const lOn = line(on, defenders, attackers), lOff = line(off, defenders, attackers);
      const a = assess(on, lOn.att, lOn.target);
      const p = chanceOn(a.odds, a.dice), today = chanceOn(a.odds, a.luck!);
      let won = 0, wonOff = 0;
      const n = 40;
      for (let k = 0; k < n; k++) {
        const x = again(on, lOn), y = again(off, lOff);
        for (let i = 0; i < k; i++) { x.g.rng.next(); y.g.rng.next(); }
        const rx = resolveBattle(x.g, x.l.att, x.l.target);
        const ry = resolveBattle(y.g, y.l.att, y.l.target);
        // One throw, read in today's run: the attackers lose the same men either way.
        expect(rx.attLost).toBe(ry.attLost);
        // The level says who carried the ground.
        if (rx.taken) expect(rx.level).toMatch(/victory/);
        if (/defeat/.test(rx.level)) expect(rx.taken || !!rx.siege).toBe(false);
        won += rx.taken || rx.siege ? 1 : 0;
        wonOff += ry.taken || ry.siege ? 1 : 0;
      }
      expect(Math.abs(won / n - p)).toBeLessThan(0.2);
      expect(Math.abs(wonOff / n - today)).toBeLessThan(0.2);
      if (attackers === 4) expect(today).toBe(1);
      if (today === 1) { expect(wonOff).toBe(n); expect(p).toBeLessThan(0.95); expect(won).toBeLessThan(n); }
    });
  }

  it('a field fought out is billed at its own result\'s share of the way, read in today\'s run', () => {
    const on = war(true), off = war(false);
    const lOn = line(on, 1, 4), lOff = line(off, 1, 4);
    const a = assess(on, lOn.att, lOn.target);
    for (const share of [0.1, 0.5, 0.9]) {
      const x = again(on, lOn), y = again(off, lOff);
      const rx = resolveBattle(x.g, x.l.att, x.l.target, { fought: a.odds * (a.dice[0] + (a.dice[1] - a.dice[0]) * share) });
      const ry = resolveBattle(y.g, y.l.att, y.l.target, { fought: a.odds * (a.luck![0] + (a.luck![1] - a.luck![0]) * share) });
      expect(rx.attLost).toBe(ry.attLost);
    }
  });
});

describe('the field fought out (the 3D game)', () => {
  it('its ticket carries the wide run for the chance and today\'s for the field\'s fortune; off, today\'s alone', () => {
    const on = playing(true), off = playing(false);
    const l = line(on, 1, 4), m = line(off, 1, 4);
    const a = assess(on, l.att, l.target);
    const t = landTicket(on, l.att, l.target)!;
    expect(t.reckoning.dice).toEqual(a.dice);
    expect(t.reckoning.luck).toEqual(FIELD_DICE);
    expect(t.reckoning.even).toBeCloseTo(evenOdds(a.dice), 9);
    const u = landTicket(off, m.att, m.target)!;
    expect(u.reckoning.dice).toEqual(FIELD_DICE);
    expect('luck' in u.reckoning).toBe(false);
  });

  it('one throw: the field\'s clamp, the staff\'s hand-back and the card bill a result at the same share alike', () => {
    const on = war(true);
    const l = line(on, 1, 4);
    const a = assess(on, l.att, l.target);
    for (const share of [0.02, 0.1, 0.4, 0.8]) {
      const thrown = a.odds * (a.dice[0] + (a.dice[1] - a.dice[0]) * share);
      const winner = thrown >= 1.2 ? 'att' : 'def';
      // Inside the run, the field's clamp and the hand-back to the staff (at the ratio the card set) leave it as it is.
      expect(clampToDice(thrown, a.odds, a.dice, winner)).toBeCloseTo(thrown, 9);
      expect(diceNow(a.odds, a.dice, { att: 1, def: 1 }, share)).toBeCloseTo(thrown, 9);
      // The card's own throw at that share, and the field's result handed back: the same verdict and the same bill.
      const x = again(on, l), y = again(on, l);
      const range = y.g.rng.range.bind(y.g.rng);
      let thrownOnce = false;
      y.g.rng.range = (lo: number, hi: number): number => (!thrownOnce && lo === 0 && hi === 1 ? ((thrownOnce = true), share) : range(lo, hi));
      const fought = resolveBattle(x.g, x.l.att, x.l.target, { fought: thrown });
      const card = resolveBattle(y.g, y.l.att, y.l.target);
      expect(thrownOnce).toBe(true);
      expect(fought.taken).toBe(card.taken);
      expect(fought.level).toBe(card.level);
      expect(fought.attLost).toBe(card.attLost);
      expect(fought.defLost).toBe(card.defLost);
    }
  });
});
