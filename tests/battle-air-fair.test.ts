// Ruling 226 on the air battle (the design director, 19:42Z; the playtest's b384, 20:15Z): the first thing a newcomer
// tries is never a trap, the break can be seen coming, and the staff's question says what the staff would do now.
// Mark 4: everything picked up, the nearest enemy right-clicked as soon as one is seen, and the computer flying the
// other side; over 30 fixed seeds that order wins within a tenth of the raid card's chance, on every raid the play
// file offers. The seeds are fixed, so the test is exact from run to run; the tolerance allows for the luck of 30 raids.
import { describe, expect, it } from 'vitest';
import { launchRaid, planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { staffHow } from '../src/campaign/airfought';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { SKY } from '../src/battle/air/book';
import { airBattle, type AirSim } from '../src/battle/air/sim';
import { airTicket, foughtAir } from '../src/battle/air/ticket';
import { dist } from '../src/battle/core/geometry';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };
const RUNS = 30;

function game(seed: number, nat: string): Game {
  const g = startOpen(createGame(seed, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  return g;
}

/** The staff's raid on a named province with every fighter group that can go by day, as the play file sets it out. */
function raid(g: Game, nat: string, kind: RaidKind, name: string) {
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  const { ticket, why } = airTicket(g, kind, t.id, ids, nat);
  if (!ticket) throw new Error(why ?? 'no ticket');
  return { target: t.id, ids, ticket };
}

/** The obvious order: every group of his picked up, and the nearest enemy he can see right-clicked once one shows. */
function obvious(s: AirSim): void {
  const mine = s.units.filter((u) => u.side === 'att').map((u) => u.id);
  let given = false;
  while (!s.over) {
    if (!given) {
      const foes = s.units.filter((u) => u.side === 'def' && u.job.do !== 'wait' && u.state !== 'gone' && u.state !== 'withdrawn' && s.sees('att', u));
      if (foes.length) {
        const us = s.units.filter((u) => u.side === 'att' && u.state !== 'gone');
        const cx = us.reduce((t, u) => t + u.x, 0) / us.length, cy = us.reduce((t, u) => t + u.y, 0) / us.length;
        const near = foes.sort((p, q) => dist(cx, cy, p.x, p.y) - dist(cx, cy, q.x, q.y))[0];
        given = s.command({ do: 'attack', units: mine, target: near.id }, 'att');
      }
    }
    s.step();
  }
}

describe('the obvious order honours the raid card (ruling 226, mark 4)', () => {
  for (const [nat, kind, name] of [['GER', 'bomb', 'Alsace'], ['GER', 'superiority', 'Alsace'], ['UK', 'bomb', 'Bremen']] as [string, RaidKind, string][]) {
    it(`${kind === 'superiority' ? 'the fight for the sky over' : 'the day raid on'} ${name}`, () => {
      let won = 0, chance = 0;
      for (let k = 0; k < RUNS; k++) {
        const g = game(100 + k * 13, nat);
        const r = raid(g, nat, kind, name);
        chance = r.ticket.place.chance;
        const s = airBattle(r.ticket);
        s.ai.def = true;
        obvious(s);
        const rep = launchRaid(g, kind, r.target, r.ids, nat, foughtAir(s.outcome()))!;
        if (rep.won) won++;
      }
      expect(Math.abs(won / RUNS - chance), `${name}: won ${won} of ${RUNS}, the card gives ${chance.toFixed(2)}`).toBeLessThanOrEqual(0.1);
    }, 240_000);
  }
});

describe('the break can be seen coming, and the staff says its chance now (ruling 226, marks 3 and 5)', () => {
  it('both bars start empty, only ever fill, and the warning comes once a side is three quarters of the way', () => {
    const g = game(7, 'GER');
    const s = airBattle(raid(g, 'GER', 'bomb', 'Alsace').ticket);
    s.ai.att = true; s.ai.def = true;
    expect(s.nearBreak('att')).toBe(0);
    expect(s.nearBreak('def')).toBe(0);
    let warned = 0;
    while (!s.over) {
      s.step();
      for (const side of ['att', 'def'] as const) expect(s.nearBreak(side)).toBeGreaterThanOrEqual(0);
      for (const e of s.view().events) {
        if (e.what !== 'air:near') continue;
        warned++;
        expect(s.nearBreak((e.extra as { side: 'att' | 'def' }).side)).toBeGreaterThanOrEqual(SKY.warn);
      }
    }
    expect(warned).toBeLessThanOrEqual(2);
  });

  it('before any contact the staff give the raid card its own chance, and say no shot has been fired', () => {
    const g = game(7, 'UK');
    const r = raid(g, 'UK', 'bomb', 'Bremen');
    const s = airBattle(r.ticket);
    expect(Math.abs(s.staffChance() - r.ticket.place.chance)).toBeLessThanOrEqual(0.05);
    expect(staffHow('att', { chance: s.staffChance(), fired: false }, r.ticket.place.chance)).toBe('whole');
  });

  it("what you had won stays won only when the fight has moved the player's way", () => {
    expect(staffHow('att', { chance: 0.7, fired: true }, 0.56)).toBe('won');
    expect(staffHow('att', { chance: 0.4, fired: true }, 0.56)).toBe('rest');
    expect(staffHow('def', { chance: 0.4, fired: true }, 0.56)).toBe('won');
    expect(staffHow('def', { chance: 0.7, fired: true }, 0.56)).toBe('rest');
  });

  it('the fight winds down only once nobody is fighting and every bomber has bombed or turned back', () => {
    const g = game(7, 'GER');
    const s = airBattle(raid(g, 'GER', 'bomb', 'Alsace').ticket);
    s.ai.att = true; s.ai.def = true;
    expect(s.winding()).toBe(false);
    while (!s.over && !s.winding()) s.step();
    const bombers = s.units.filter((u) => u.side === 'att' && u.air === 'bmr');
    expect(bombers.every((u) => u.state === 'gone' || u.state === 'withdrawn' || u.dropped > 0 || u.jettisoned || u.job.do === 'home' || u.state === 'routing')).toBe(true);
  });
});

describe("each raid's ground is its own province (b384(j))", () => {
  it('Bremen has its coast on the board, Alsace is inland with its rivers, and the two do not share a target outline', () => {
    const bremen = raid(game(7, 'UK'), 'UK', 'bomb', 'Bremen').ticket.place.ground!;
    const alsace = raid(game(7, 'GER'), 'GER', 'bomb', 'Alsace').ticket.place.ground!;
    expect(bremen.inland).toBe(false);
    expect(bremen.land.length).toBeGreaterThan(0);
    expect(alsace.inland).toBe(true);
    expect(alsace.rivers.length).toBeGreaterThan(0);
    expect(bremen.target.length).toBeGreaterThan(0);
    expect(alsace.target.length).toBeGreaterThan(0);
    expect(JSON.stringify(bremen.target)).not.toBe(JSON.stringify(alsace.target));
  });
});
