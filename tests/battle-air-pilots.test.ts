// What the 3D game needs to fly its fighter pilots well (Brian, 2026-11-20 20:42Z: "lets work on the ai for the
// fighter pilots. they were kinda just flying around doing nothing"; 20:43Z: "lets make this super realistic and
// meaningful and everyone has high AI iq to do so"). The battle still only counts: the scene's pilots act the count
// out. These are the facts the scene reads to do it with purpose: whom each fighter group is making for before the
// first shot, which bombers an escort guards, and how good its pilots are. Shown, never counted (ruling 237).
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { SKY } from '../src/battle/air/book';
import { airBattle, type AirSim } from '../src/battle/air/sim';
import { airTicket, raidGroups } from '../src/battle/air/ticket';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };
const RAIDS: [string, RaidKind, string][] = [['GER', 'bomb', 'Alsace'], ['GER', 'superiority', 'Alsace'], ['FRA', 'bomb', 'Ruhr']];
const RUNS = 6;

/** The play file's k-th battle of a raid (as tests/battle-air-realism.test.ts). */
function ticketOf(nat: string, kind: RaidKind, name: string, k: number) {
  const g = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name)!;
  const { ticket } = airTicket(g, kind, t.id, raidGroups(g, kind, t.id, nat), nat);
  return ticket!;
}

type Pilots = { skill?: number; with?: number; quarry?: number; role?: string };

function fight(nat: string, kind: RaidKind, name: string, k: number, show: boolean, each?: (s: AirSim) => void): AirSim {
  const s = airBattle(ticketOf(nat, kind, name, k), { show });
  s.ai.att = true; s.ai.def = true;
  while (!s.over) { s.step(); each?.(s); }
  return s;
}

describe('what the scene\'s fighter pilots are told (shown, never counted)', () => {
  it('every fighter group carries its pilots\' skill, the same all battle, within the book\'s range', () => {
    const top = SKY.pilots.base + SKY.pilots.leader + SKY.pilots.ready;
    for (const [nat, kind, name] of RAIDS) {
      const seen = new Map<number, number>();
      let frames = 0;
      fight(nat, kind, name, 0, true, (s) => {
        if (Math.round(s.t / 0.1) % 10) return;
        for (const v of s.view().units) {
          const u = s.unit(v.id)!;
          const x = (v.extra ?? {}) as Pilots;
          if (u.air !== 'ftr') { expect(x.skill, `${name} ${kind}: only fighters are graded`).toBeUndefined(); continue; }
          frames++;
          expect(typeof x.skill).toBe('number');
          expect(x.skill!).toBeGreaterThanOrEqual(SKY.pilots.base - 1e-9);
          expect(x.skill!).toBeLessThanOrEqual(Math.min(1, top) + 1e-9);
          if (seen.has(v.id)) expect(x.skill).toBe(seen.get(v.id));
          seen.set(v.id, x.skill!);
        }
      });
      expect(frames, `${name} ${kind}`).toBeGreaterThan(0);
    }
  });

  it('an escort names the bombers it guards; nobody else does', () => {
    let escorts = 0;
    for (const [nat, kind, name] of RAIDS) fight(nat, kind, name, 0, true, (s) => {
      if (Math.round(s.t / 0.1) % 10) return;
      for (const v of s.view().units) {
        const u = s.unit(v.id)!;
        const x = (v.extra ?? {}) as Pilots;
        if (u.air === 'ftr' && u.job.do === 'escort') {
          escorts++;
          expect(x.with).toBe(u.job.on);
          const b = s.unit(x.with!)!;
          expect(b.side).toBe(u.side);
          expect(b.air).toBe('bmr');
        } else expect(x.with).toBeUndefined();
      }
    });
    expect(escorts).toBeGreaterThan(0);
  });

  it('a fighter group making for the enemy names it before the first shot: a living enemy group, and the one it then meets', () => {
    let named = 0, met = 0, early = 0;
    for (const [nat, kind, name] of RAIDS) for (let k = 0; k < RUNS; k++) {
      const last = new Map<number, number>();
      const before = new Set<number>();
      const s = fight(nat, kind, name, k, true, (sim) => {
        for (const v of sim.view().units) {
          const u = sim.unit(v.id)!;
          const x = (v.extra ?? {}) as Pilots;
          if (x.quarry === undefined) continue;
          expect(u.air).toBe('ftr');
          const e = sim.unit(x.quarry)!;
          expect(e, `${name} ${kind} ${k}: the quarry is a group in this battle`).toBeDefined();
          expect(e.side).not.toBe(u.side);
          named++;
          last.set(v.id, x.quarry);
          if (!u.contacts.length) before.add(v.id);
        }
      });
      for (const e of s.history().filter((h) => h.what === 'contact' && h.unit !== undefined)) {
        const u = s.unit(e.unit!)!;
        if (u.air !== 'ftr' || !last.has(u.id)) continue;
        met++;
        if (before.has(u.id)) early++;
      }
    }
    expect(named).toBeGreaterThan(0);
    expect(met).toBeGreaterThan(0);
    // Most fighter groups that come to blows were already flying at somebody: the scene can set them up for the pass.
    expect(early / met).toBeGreaterThan(0.6);
  });

  it('none of it moves a count: the same battle, shown or not', () => {
    for (const [nat, kind, name] of RAIDS) for (let k = 0; k < RUNS; k++) {
      const off = fight(nat, kind, name, k, false), on = fight(nat, kind, name, k, true);
      expect(on.outcome(), `${name} ${kind} ${k}`).toEqual(off.outcome());
      expect(on.over!.level).toBe(off.over!.level);
      expect(on.over!.lines).toEqual(off.over!.lines);
      expect(on.t).toBe(off.t);
    }
  });
});
