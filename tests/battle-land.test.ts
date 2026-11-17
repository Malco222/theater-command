// The land battle (src/battle/land): the field from the province, blocks from divisions balanced to the attack card,
// orders, the end, replay, the fairness test against the dice (ruling 217(3)), and the hand-back to the campaign
// through the rules' own attack order. The design director's rulings 217 and 217(2)(a) of 2026-11-17.
import { describe, expect, it } from 'vitest';
import { diceChance, runOut } from '../src/battle/core';
import { buildField, GROUNDS, LandBattle, blocksFor, handBack, landTicket, sampleTicket, goingIn } from '../src/battle/land';
import { createGame, hostileUnits } from '../src/campaign/game';
import { blocked } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderAttack, previewAttack, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const count = (f: ReturnType<typeof buildField>, k: (typeof GROUNDS)[number]): number => f.ground.filter((g) => GROUNDS[g] === k).length;

describe('the field is built from the province', () => {
  it('the same province and seed give the same field', () => {
    const t = sampleTicket('hills', 1.2, { river: true });
    expect(buildField(t.place, 5)).toEqual(buildField(t.place, 5));
    expect(buildField(t.place, 5).ground).not.toEqual(buildField(t.place, 6).ground);
  });
  it('a river has its bridge, blown when the campaign says the bridges are down', () => {
    const up = buildField(sampleTicket('plains', 1, { river: true }).place, 3);
    expect(count(up, 'water')).toBeGreaterThan(100);
    expect(count(up, 'bridge')).toBeGreaterThan(0);
    const down = buildField(sampleTicket('plains', 1, { river: true, bridgeDown: true }).place, 3);
    expect(count(down, 'bridge')).toBe(0);
    expect(count(down, 'blown')).toBeGreaterThan(0);
  });
  it('the ground follows the terrain: woods in forest, rock in mountains, marsh in marsh, streets in a city, works for a fortress', () => {
    const f = (t: Parameters<typeof sampleTicket>[0], o = {}) => buildField(sampleTicket(t, 1, o).place, 9);
    expect(count(f('forest'), 'woods')).toBeGreaterThan(count(f('plains'), 'woods') * 2);
    expect(count(f('mountain'), 'rock')).toBeGreaterThan(50);
    expect(count(f('marsh'), 'marsh')).toBeGreaterThan(1000);
    expect(count(f('urban'), 'buildings')).toBeGreaterThan(count(f('plains'), 'buildings') * 2);
    expect(count(f('plains', { fort: 2 }), 'bunker')).toBeGreaterThan(0);
  });
});

describe('blocks from divisions', () => {
  it('one a division, at least six a side, and never two divisions folded into one (ruling 226(10))', () => {
    const f = (divs: number[]) => blocksFor(divs.map((d, i) => ({ corps: i, name: '', nat: 'X', kind: 'inf', divs: d, men: 1, str: 1, org: 1, xp: 0, share: 1 / divs.length })));
    expect(f([3, 3, 2])).toEqual([3, 3, 2]);
    expect(f([2]).reduce((a, b) => a + b, 0)).toBeGreaterThanOrEqual(3);
    expect(f([1, 1]).reduce((a, b) => a + b, 0)).toBe(6);
    expect(f([8, 8, 8, 8]).reduce((a, b) => a + b, 0)).toBe(32);
  });
  it('each side\'s blocks add up to the power the card counted, so the odds are the card\'s', () => {
    const b = new LandBattle(sampleTicket('plains', 1.5));
    const sum = (s: string) => b.units.filter((u) => u.side === s).reduce((a, u) => a + u.power0, 0);
    expect(sum('att')).toBeCloseTo(100);
    expect(sum('att') / sum('def')).toBeCloseTo(1.5);
    for (const u of b.units) { expect(u.figures).toBeGreaterThan(0); expect(u.w).toBeGreaterThan(0); expect(u.act).toBe('idle'); }
  });
  it('the field opens with both armies drawn up in their own halves', () => {
    const b = new LandBattle(sampleTicket('forest', 1.2));
    for (const u of b.units) expect(u.side === 'att' ? u.y < b.field.h / 2 : u.y > b.field.h / 2).toBe(true);
  });
});

describe('orders and the end', () => {
  it('a block marches where it is sent, and says what it is doing for the models', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    b.ai.def = false;
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'inf')!;
    expect(b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 200 }, 'att')).toBe(true);
    let marched = false;
    for (let i = 0; i < 600; i++) { b.step(); if (u.act === 'march') marched = true; }
    expect(marched).toBe(true);
    expect(Math.abs(u.y - (u.home[1] + 200))).toBeLessThan(10);
    expect(u.order).toBeNull();
  });
  it('two blocks sent to one spot do not end up standing in each other', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    b.ai.def = false;
    const [u, v] = b.units.filter((x) => x.side === 'att' && x.kind === 'inf');
    b.command({ do: 'move', units: [u.id], x: 1500, y: 700 }, 'att');
    b.command({ do: 'move', units: [v.id], x: 1500, y: 700 }, 'att');
    for (let i = 0; i < 1500; i++) b.step();
    // Side by side or one behind the other, but not on top of each other.
    const lat = Math.abs(u.x - v.x), dep = Math.abs(u.y - v.y);
    expect(lat >= (u.w + v.w) / 2 - 2 || dep >= (u.d + v.d) / 2 - 2).toBe(true);
  });
  it('a player cannot order the other side\'s blocks', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    const e = b.units.find((x) => x.side === 'def')!;
    expect(b.command({ do: 'move', units: [e.id], x: 10, y: 10 }, 'att')).toBe(false);
  });
  it('withdrawing the whole army ends it, and the field is theirs', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    expect(b.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(b.over?.winner).toBe('def');
    expect(b.over?.how).toBe('withdrew');
    expect(b.over!.R).toBeLessThan(1.2);
  });
  it('finish it for me: the dice at the ratio then standing, inside their run', () => {
    for (const odds of [0.8, 1.2, 1.8]) {
      const b = new LandBattle(sampleTicket('hills', odds, { humanAtt: true }));
      b.ai.att = true;
      for (let i = 0; i < 900; i++) b.step();
      b.command({ do: 'dice' }, 'att');
      expect(b.over).not.toBeNull();
      expect(b.over!.R).toBeGreaterThanOrEqual(odds * 0.8 - 1e-6);
      expect(b.over!.R).toBeLessThanOrEqual(odds * 1.2 + 1e-6);
      expect(b.over!.winner).toBe(b.over!.R >= 1.2 ? 'att' : 'def');
    }
  });
  it('the same ticket and the same orders replay the same battle', () => {
    const play = () => { const b = new LandBattle(sampleTicket('urban', 1.3, { seed: 44 })); b.ai.att = b.ai.def = true; runOut(b); return [b.over, b.units.map((u) => [u.x, u.y, u.str, u.mor])]; };
    expect(play()).toEqual(play());
  });
  it('an army that breaks ends the battle, and the result is on the right side of the line', () => {
    const b = new LandBattle(sampleTicket('plains', 2));
    b.ai.att = b.ai.def = true;
    runOut(b);
    expect(b.over).not.toBeNull();
    expect(b.over!.winner === 'att').toBe(b.over!.R >= 1.2);
    expect(b.over!.seconds).toBeLessThan(b.ticket.limit);
  });
});

describe('fair to the dice (ruling 217(3))', () => {
  // Two computer generals carry the ground about as often as the dice would at the same odds, on every ground. The
  // measure: the mean gap between the field's win rate and the dice's chance over a spread of odds, 16 battles each.
  const ODDS = [0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 1.8];
  const gaps: Record<string, number> = {};
  for (const ground of ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const) {
    it(`on ${ground}`, () => {
      let gap = 0;
      for (const odds of ODDS) {
        let won = 0;
        for (let s = 1; s <= 16; s++) { const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13 })); b.ai.att = b.ai.def = true; runOut(b); if (b.over!.winner === 'att') won++; }
        gap += Math.abs(won / 16 - diceChance(odds, [0.8, 1.2]));
      }
      gaps[ground] = gap / ODDS.length;
      expect(gaps[ground]).toBeLessThanOrEqual(0.13);
    }, 120000);
  }
  it('and within a tenth across all grounds together', () => {
    const all = Object.values(gaps);
    expect(all.length).toBe(7);
    expect(all.reduce((a, b) => a + b, 0) / all.length).toBeLessThanOrEqual(0.1);
  });
});

describe('a battle fought to its end costs what the campaign bills for it (the design director, b356)', () => {
  // The campaign charges each side its losses from R alone (resolveBattle in game.ts: the attack 0.14 / R between 2%
  // and 20%; the defence 0.07 + 0.1 R between 6% and 45%, six tenths of that where it holds). Before any corps' own
  // toll, command or works, which the field does not model. The field's own losses must tell the same story: on
  // average within a fifth of the bill for the R it hands back, winners and losers alike, and most battles inside the
  // run of bills the dice could have rolled at the card's odds, give or take a quarter.
  const clamp = (x: number, a: number, b: number): number => Math.max(a, Math.min(b, x));
  const billAtt = (R: number): number => clamp(0.14 / R, 0.02, 0.2);
  const billDef = (R: number): number => clamp(0.07 + 0.1 * R, 0.06, 0.45) * (R >= 1.2 ? 1 : 0.6);
  it('on every ground, computer against computer', () => {
    const cells: Record<string, { a: number; d: number; n: number }> = { attWon: { a: 0, d: 0, n: 0 }, defWon: { a: 0, d: 0, n: 0 } };
    let n = 0, inA = 0, inD = 0;
    for (const ground of ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const) {
      for (const odds of [0.8, 1.0, 1.2, 1.4, 1.8, 2.5]) {
        for (let s = 1; s <= 4; s++) {
          const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13 }));
          b.ai.att = b.ai.def = true;
          runOut(b);
          const o = b.over!, k = b.ticket.reckoning;
          const c = cells[o.winner === 'att' ? 'attWon' : 'defWon'];
          c.a += o.lost.att / billAtt(o.R); c.d += o.lost.def / billDef(o.R); c.n++;
          const lo = k.odds * k.dice[0], hi = k.odds * k.dice[1];
          const ds = [billDef(lo), billDef(hi), billDef(clamp(1.2, lo, hi))];
          if (o.lost.att >= billAtt(hi) * 0.75 && o.lost.att <= billAtt(lo) * 1.25) inA++;
          if (o.lost.def >= Math.min(...ds) * 0.75 && o.lost.def <= Math.max(...ds) * 1.25) inD++;
          n++;
        }
      }
    }
    for (const c of Object.values(cells)) {
      expect(c.n).toBeGreaterThan(20);
      expect(c.a / c.n).toBeGreaterThan(0.8); expect(c.a / c.n).toBeLessThan(1.25);
      expect(c.d / c.n).toBeGreaterThan(0.8); expect(c.d / c.n).toBeLessThan(1.25);
    }
    expect(inA / n).toBeGreaterThanOrEqual(0.7);
    expect(inD / n).toBeGreaterThanOrEqual(0.7);
  }, 120000);
});

describe('a fight, not a blink (the design director, ruling 226)', () => {
  const GROUNDS7 = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const;
  /** Select every block, right-click the enemy nearest them, Space: the order a newcomer gives first (mark 4). */
  const plainAttack = (b: LandBattle): void => {
    const mine = b.units.filter((u) => u.side === 'att' && u.kind !== 'fort');
    const cx = mine.reduce((a, u) => a + u.x, 0) / mine.length, cy = mine.reduce((a, u) => a + u.y, 0) / mine.length;
    const foes = b.units.filter((u) => u.side === 'def');
    const near = foes.reduce((m, f) => (Math.hypot(f.x - cx, f.y - cy) < Math.hypot(m.x - cx, m.y - cy) ? f : m), foes[0]);
    expect(b.command({ do: 'attack', units: mine.map((u) => u.id), target: near.id }, 'att')).toBe(true);
  };

  it('"what you had won stays won" only when the field moved his way by five points or more (the design director, 20:51)', () => {
    let said = 0, quiet = 0;
    for (const ground of GROUNDS7) {
      for (const s of [1, 2, 3]) {
        const b = new LandBattle(sampleTicket(ground, 1.2, { seed: s * 7919 + 13 }));
        const st0 = b.standing(), share0 = st0.att / (st0.att + st0.def);
        b.ai.def = true;
        plainAttack(b);
        for (let i = 0; i < 20000 && !b.over && (b.contactAt === null || b.t < b.contactAt + 20 * s); i++) b.step();
        if (b.over) continue;
        const st = b.standing(), gain = st.att / (st.att + st.def) - share0;
        b.toDice();
        const line = b.over!.lines[0];
        expect(line.includes('what you had won stays won')).toBe(gain >= 0.05);
        if (gain >= 0.05) said++; else quiet++;
      }
    }
    expect(said + quiet).toBeGreaterThan(10);
  });

  it('a close battle lasts minutes from the first contact, and never breaks an army most of which has not fought (mark 2)', () => {
    const fights: number[] = [];
    for (const ground of GROUNDS7) {
      for (const odds of [0.8, 1.0, 1.2, 1.5]) {
        for (let s = 1; s <= 3; s++) {
          const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13 }));
          b.ai.att = b.ai.def = true;
          runOut(b);
          expect(b.contactAt).not.toBeNull();
          fights.push(b.t - b.contactAt!);
          const broke = b.log.find((e) => e.what === 'broken');
          if (broke) {
            // The army that broke: at least half its strength had been in the fight.
            const us = b.units.filter((u) => u.side === broke.side);
            const P = us.reduce((a, u) => a + u.power0, 0);
            const fought = us.filter((u) => u.met || u.str < u.str0 * 0.97 || u.state === 'gone').reduce((a, u) => a + u.power0, 0);
            expect(fought / P).toBeGreaterThanOrEqual(0.5);
            // Routing belongs to the blocks that broke; the rest fall back with their army (after the slice 3 review).
            for (const u of us) expect(u.fellBack ? u.state !== 'routing' : ['routing', 'gone', 'withdrawn'].includes(u.state)).toBe(true);
          }
        }
      }
    }
    fights.sort((a, c) => a - c);
    const at = (p: number): number => fights[Math.floor(p * (fights.length - 1))];
    expect(at(0.05)).toBeGreaterThanOrEqual(90);
    expect(at(0.5)).toBeGreaterThanOrEqual(120);
    expect(at(0.5)).toBeLessThanOrEqual(240);
  }, 120000);

  it('one block running falls back alone: the army bar counts only its share', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { seed: 99 }));
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'inf')!;
    const before = b.breaking('att');
    u.str = u.str0 * 0.5; u.mor = 0;
    for (let i = 0; i < 5; i++) b.step();
    expect(u.state).toBe('routing');
    expect(b.over).toBeNull();
    expect(b.breaking('att')).toBeGreaterThan(before);
    expect(b.breaking('att')).toBeLessThan(0.75);
    expect(b.view().breaking!.att).toBeCloseTo(b.breaking('att'), 6);
  });

  // Over 20 battles at each odds on every ground, the plain attack carries the day within a tenth of the card's chance.
  const gaps: Record<string, { gap: number; bias: number }> = {};
  for (const ground of GROUNDS7) {
    it(`the plain attack honours the card on ${ground} (mark 4)`, () => {
      const ODDS = [0.8, 1.0, 1.2, 1.4, 1.6, 1.8];
      let gap = 0, bias = 0;
      for (const odds of ODDS) {
        let won = 0;
        for (let s = 1; s <= 20; s++) {
          const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13, humanAtt: true }));
          b.ai.att = false; b.ai.def = true;
          plainAttack(b);
          runOut(b);
          if (b.over!.winner === 'att') won++;
        }
        const d = won / 20 - diceChance(odds, [0.8, 1.2]);
        gap += Math.abs(d); bias += d;
      }
      gaps[ground] = { gap: gap / ODDS.length, bias: bias / ODDS.length };
      expect(gaps[ground].bias).toBeGreaterThanOrEqual(-0.1);
      expect(gaps[ground].bias).toBeLessThanOrEqual(0.1);
      expect(gaps[ground].gap).toBeLessThanOrEqual(0.13);
    }, 120000);
  }
  it('and within a tenth across all grounds together (mark 4)', () => {
    const all = Object.values(gaps);
    expect(all.length).toBe(7);
    expect(all.reduce((a, g) => a + g.gap, 0) / all.length).toBeLessThanOrEqual(0.1);
  });
});

describe('the hand-off with a real campaign', () => {
  function playing(seed = 3): Game {
    const g = startOpen(createGame(seed, 'GER'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    return g;
  }
  const anAttack = (g: Game): { c: Corps; r: Region } => {
    for (const c of g.corps.filter((x) => x.nat === 'GER' && !x.dead && x.kind === 'inf' && !x.town && x.ap > 0)) {
      for (const j of g.regions[c.reg].nb) { const r = g.regions[j]; if (hostileUnits(g, r, 'GER').length && !blocked(g, c, r, c.reg) && previewAttack(g, [c], r)) return { c, r }; }
    }
    throw new Error('no attack');
  };
  it('the ticket carries the card\'s own reckoning and the province', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    const t = landTicket(g, [c], r)!;
    const a = previewAttack(g, [c], r)!;
    expect(t.reckoning.odds).toBeCloseTo(a.odds);
    expect(t.reckoning.A).toBeCloseTo(a.A);
    expect(t.place.terrain).toBe(r.terrain);
    expect(t.att.human).toBe(true);
    expect(t.att.forces.map((f) => f.corps)).toEqual(goingIn(g, [c], r).map((x) => x.id));
    expect(t.def.forces.length).toBe(a.defenders.length);
    expect(t.att.forces.reduce((s, f) => s + f.share, 0)).toBeCloseTo(1);
  });
  it('a fought battle comes back through the rules: the report carries the field\'s R, and the ground follows it', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    const t = landTicket(g, [c], r)!;
    const b = new LandBattle(t);
    b.ai.att = true;
    runOut(b);
    const rep = handBack(g, [c], r, b.over!)!;
    expect(rep).not.toBeNull();
    expect(rep.odds).toBeCloseTo(b.over!.R);
    expect(rep.taken || rep.siege === true).toBe(b.over!.R >= 1.2);
  });
  it('the after card\'s headline and its field line speak in one measure, within a fifth (ruling 226(6))', () => {
    let n = 0;
    for (const seed of [3, 5, 7, 9, 11]) {
      const g = playing(seed);
      const { c, r } = anAttack(g);
      const t = landTicket(g, [c], r);
      if (!t) continue;
      const b = new LandBattle(t);
      b.ai.att = true;
      runOut(b);
      const rep = handBack(g, [c], r, b.over!)!;
      const sent = rep.attackers.reduce((a, u) => a + u.men, 0), lost = rep.attackers.reduce((a, u) => a + u.lost, 0);
      const k = lost / sent / Math.max(1e-6, b.over!.lost.att);
      expect(k).toBeGreaterThan(1 / 1.2);
      expect(k).toBeLessThan(1.2);
      n++;
    }
    expect(n).toBeGreaterThanOrEqual(3);
  });
  it('with no battle fought, the rules run exactly as before', () => {
    const g1 = playing(5), g2 = playing(5);
    const a1 = anAttack(g1), a2 = anAttack(g2);
    const r1 = orderAttack(g1, [a1.c], a1.r), r2 = orderAttack(g2, [a2.c], a2.r, undefined);
    expect(JSON.stringify(r1)).toBe(JSON.stringify(r2));
    expect(g1.rng.state).toBe(g2.rng.state);
  });
});
