// Realistic air combat (Brian, 2026-11-18 19:35 to 19:38Z; the design /mnt/project-files/air-battles/realism/
// air-combat-realism.md; the design director's ruling 237). The battle still decides only how many aircraft fall; it
// now also says who shot whom, the pass each fighter flies at a bomber group, which bomber has lost an engine and
// fallen out of formation, each fighter's ammunition and each box's state, so the 3D game can act it out as it was.
// None of that may move a count: these tests fight every battle with it on and off and find the same result.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { SKY } from '../src/battle/air/book';
import { bomberOf, enginesOf, escortRoles, fighterOf, fighterWay, passesOn } from '../src/battle/air/doctrine';
import { airBattle, type AirSim } from '../src/battle/air/sim';
import { airTicket, raidGroups } from '../src/battle/air/ticket';
import type { FieldEvent } from '../src/battle/core/types';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };
const RAIDS: [string, RaidKind, string][] = [['GER', 'bomb', 'Alsace'], ['GER', 'superiority', 'Alsace'], ['FRA', 'bomb', 'Ruhr']];
const RUNS = 8;
/** The events the battle told before any of this was shown. */
const OLD = new Set(['contact', 'wavering', 'routing', 'rallied', 'gone', 'withdrawn', 'end', 'air:lost', 'air:bounce', 'air:scramble', 'air:bingo', 'air:bombs', 'air:spent', 'air:near', 'air:jettison', 'air:rail', 'air:ruin']);

/** The play file's k-th battle of a raid (as tests/battle-air-fair.test.ts). */
function ticketOf(nat: string, kind: RaidKind, name: string, k: number) {
  const g = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name)!;
  const { ticket } = airTicket(g, kind, t.id, raidGroups(g, kind, t.id, nat), nat);
  return ticket!;
}

function fight(nat: string, kind: RaidKind, name: string, k: number, show: boolean, each?: (s: AirSim) => void): AirSim {
  const s = airBattle(ticketOf(nat, kind, name, k), { show });
  s.ai.att = true; s.ai.def = true;
  while (!s.over) { s.step(); each?.(s); }
  return s;
}

const strip = (h: FieldEvent[]) => h.filter((e) => OLD.has(e.what)).map((e) => ({ t: e.t, what: e.what, unit: e.unit, text: e.text, cue: e.cue }));

describe('shown, never counted (ruling 237)', () => {
  it('every battle ends the same with the showing on and off: the counts, the bombs, the verdict, every dispatch', () => {
    for (const [nat, kind, name] of RAIDS) for (let k = 0; k < RUNS; k++) {
      const off = fight(nat, kind, name, k, false), on = fight(nat, kind, name, k, true);
      expect(on.outcome(), `${name} ${kind} ${k}`).toEqual(off.outcome());
      expect(on.over!.level).toBe(off.over!.level);
      expect(on.over!.lines).toEqual(off.over!.lines);
      expect(strip(on.history())).toEqual(strip(off.history()));
      expect(off.history().every((e) => OLD.has(e.what))).toBe(true);
    }
  });

  it('every loss names who brought it down: an enemy group in the fight, or the guns', () => {
    for (const [nat, kind, name] of RAIDS) {
      const s = fight(nat, kind, name, 0, true);
      const lost = s.history().filter((e) => e.what === 'air:lost');
      expect(lost.length).toBeGreaterThan(0);
      for (const e of lost) {
        const by = (e.extra as { by: number | 'flak' | null }).by;
        if (by === 'flak') expect(e.side).toBe('att');
        else { expect(typeof by).toBe('number'); expect(s.unit(by as number)!.side).not.toBe(e.side); }
      }
    }
  });
});

describe('the straggler (Brian, 19:38Z: "it goes slower and falls out of formation which makes it an easier target")', () => {
  it('each bomber that falls out either goes down or limps home, once; and about half the bombers lost were stragglers first', () => {
    let lost = 0, first = 0;
    for (const [nat, kind, name] of RAIDS.filter((r) => r[1] === 'bomb')) for (let k = 0; k < RUNS; k++) {
      const h = fight(nat, kind, name, k, true).history();
      const out = h.filter((e) => e.what === 'air:straggle').map((e) => (e.extra as { id: number }).id);
      const ends = [
        ...h.filter((e) => e.what === 'air:lost').map((e) => (e.extra as { strag?: number }).strag).filter((x) => x !== undefined),
        ...h.filter((e) => e.what === 'air:limped').map((e) => (e.extra as { id: number }).id),
      ];
      expect([...ends].sort((a, b) => a! - b!)).toEqual([...out].sort((a, b) => a - b));
      const bl = h.filter((e) => e.what === 'air:lost' && (e.extra as { air: string }).air === 'bmr');
      lost += bl.length; first += bl.filter((e) => (e.extra as { strag?: number }).strag !== undefined).length;
    }
    expect(lost).toBeGreaterThan(50);
    // Until the historian answers, about half (ruling 237.1).
    expect(first / lost).toBeGreaterThan(0.3);
    expect(first / lost).toBeLessThan(0.7);
  });

  it('falls behind and below its box and never closes up; keeps within reach until the box bombs; a twin has one engine out, feathered', () => {
    const seen = new Map<number, { back: number; down: number }>();
    let any = 0;
    fight('GER', 'bomb', 'Alsace', 1, true, (s) => {
      for (const b of s.units.filter((u) => u.air === 'bmr')) {
        const done = b.dropped > 0 || b.jettisoned;
        for (const st of b.strag) {
          any++;
          const was = seen.get(st.id) ?? { back: 0, down: 0 };
          expect(st.back).toBeGreaterThanOrEqual(was.back);
          expect(st.down).toBeGreaterThanOrEqual(was.down);
          if (!done) expect(st.back).toBeLessThanOrEqual(SKY.strag.held);
          expect(enginesOf(b.model)).toBe(2);
          expect(st.eng.length).toBe(1);
          expect(st.prop).toBe('feather');
          seen.set(st.id, { back: st.back, down: st.down });
        }
        expect(b.strag.length).toBeLessThanOrEqual(Math.min(SKY.strag.most, b.n));
      }
    });
    expect(any).toBeGreaterThan(0);
  });
});

describe('what the 3D game reads each tick', () => {
  it('a fighter group says its aircraft, tactics, ammunition (never rising) and fire; a box its state and stragglers', () => {
    const ammo = new Map<number, number>();
    let fired = 0, passes = 0;
    fight('GER', 'bomb', 'Alsace', 2, true, (s) => {
      for (const u of s.view().units) {
        const x = u.extra as Record<string, unknown>;
        if (u.state === 'gone') continue;
        expect(typeof x.model).toBe('string');
        if (u.air === 'ftr') {
          const a = x.ammo as number;
          expect(a).toBeGreaterThanOrEqual(0); expect(a).toBeLessThanOrEqual(1);
          expect(a).toBeLessThanOrEqual(ammo.get(u.id) ?? 1);
          ammo.set(u.id, a);
          for (const f of x.fire as { on: number; rate: number; pass?: string }[]) { fired++; if (f.pass) passes++; expect(f.rate).toBeGreaterThanOrEqual(0); }
        } else {
          expect(['tight', 'loose', 'broken']).toContain(x.box);
          expect(Array.isArray(x.strag)).toBe(true);
        }
      }
    });
    expect(fired).toBeGreaterThan(0);
    expect(passes).toBeGreaterThan(0);
  });

  it('a fighter on bombers flies a pass its doctrine gives; the contact says how each side flies', () => {
    const s = fight('FRA', 'bomb', 'Ruhr', 0, true);
    const onBmr = s.history().filter((e) => e.what === 'contact' && s.unit(e.unit!)!.air === 'bmr');
    expect(onBmr.length).toBeGreaterThan(0);
    const When = { year: 1939, month: 8 };
    for (const e of onBmr) {
      const x = e.extra as { with: number; pass: string; form: string };
      expect(x.form).toBe('box');
      const f = s.unit(x.with)!;
      expect(passesOn(f.nat, When, s.unit(e.unit!)!.model, s.unit(e.unit!)!.nat).map(([p]) => p)).toContain(x.pass);
    }
    for (const e of s.history().filter((e) => e.what === 'contact' && s.unit(e.unit!)!.air === 'ftr')) {
      const u = s.unit(e.unit!)!;
      expect((e.extra as { style: string }).style).toBe(fighterWay(u.nat, When).style);
    }
  });
});

describe('the aircraft and the doctrine (doctrine.ts)', () => {
  it('draws the aircraft the 3D game draws, by its designers\' year (CampaignAircraft.cpp)', () => {
    expect(fighterOf('GER', 1941)).toBe('Bf109');
    expect(fighterOf('GER', 1942)).toBe('Fw190');
    expect(fighterOf('UK', 1940)).toBe('Hurricane');
    expect(fighterOf('CAN', 1941)).toBe('Spitfire');
    expect(fighterOf('USA', 1942)).toBe('Warhawk');
    expect(fighterOf('USA', 1943)).toBe('Mustang');
    expect(fighterOf('SOV', 1939)).toBe('Yak');
    expect(fighterOf('JAP', 1941)).toBe('Zero');
    expect(fighterOf('FRA', 1939)).toBe('Fighter:FRA');
    expect(bomberOf('UK', 1941)).toBe('Bomber:UK');
    expect(bomberOf('UK', 1942)).toBe('Lancaster');
    expect(bomberOf('USA', 1939)).toBe('B17');
    expect(bomberOf('GER', 1944)).toBe('He111');
    expect(bomberOf('ITA', 1940)).toBe('Bomber:ITA');
    expect([enginesOf('B17'), enginesOf('Lancaster'), enginesOf('He111'), enginesOf('Bomber:FRA')]).toEqual([4, 4, 2, 2]);
    const t = ticketOf('GER', 'bomb', 'Alsace', 0);
    expect(t.place.when).toEqual({ year: 1939, month: 8 });
    expect(t.place.models!.GER).toEqual({ ftr: 'Bf109', bmr: 'He111' });
    expect(t.place.models!.FRA.ftr).toBe('Fighter:FRA');
  });

  it('has a way of flying, passes and escort roles for every nation in every month of the war', () => {
    for (const nat of ['GER', 'UK', 'CAN', 'FRA', 'POL', 'ITA', 'SOV', 'USA', 'JAP', 'CHN', 'ROM'])
      for (let year = 1939; year <= 1945; year++) for (let month = 0; month < 12; month++) {
        const w = { year, month };
        const way = fighterWay(nat, w);
        expect(['vic', 'threes', 'line', 'fours']).toContain(way.form);
        expect(['dive', 'turn']).toContain(way.style);
        for (const b of ['B17', 'Lancaster', 'He111', 'Bomber:FRA']) {
          const rows = passesOn(nat, w, b, b === 'He111' ? 'GER' : 'USA');
          expect(rows.length).toBeGreaterThan(0);
          expect(rows.every(([, k]) => k > 0)).toBe(true);
        }
        expect(escortRoles(nat, w)[0]).toBeDefined();
      }
  });
});
