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
import { bomberGuns, fighterGuns, type Loadout } from '../src/battle/air/guns';
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
  it('the two army bars in land\'s shape: how near each side is to breaking off, and when the sides first met', () => {
    let met: number | null = null, sawNull = false;
    const s = fight('GER', 'bomb', 'Alsace', 1, false, (s) => {
      const v = s.view();
      for (const side of ['att', 'def'] as const) { expect(v.breaking[side]).toBeGreaterThanOrEqual(0); expect(v.breaking[side]).toBeLessThanOrEqual(1); expect(v.breaking[side]).toBe(s.nearBreak(side)); }
      if (v.contact === null) { sawNull = true; expect(met).toBeNull(); }
      else { if (met === null) met = v.contact; expect(v.contact).toBe(met); }
    });
    const first = s.history().find((e) => e.what === 'contact')!;
    expect(sawNull).toBe(true);
    expect(met).toBe(first.t);
  });

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
    expect(fighterOf('UK', 1939)).toBe('Hurricane');
    expect(fighterOf('UK', 1940)).toBe('Spitfire');
    expect(fighterOf('CAN', 1941)).toBe('Spitfire');
    expect(fighterOf('USA', 1943)).toBe('Warhawk');
    expect(fighterOf('USA', 1944)).toBe('Mustang');
    expect(fighterOf('SOV', 1941)).toBe('Fighter:SOV');
    expect(fighterOf('SOV', 1942)).toBe('Yak');
    expect(fighterOf('JAP', 1941)).toBe('Zero');
    expect(fighterOf('ITA', 1941)).toBe('Fighter:ITA');
    expect(fighterOf('ITA', 1942)).toBe('C202');
    expect(fighterOf('FRA', 1939)).toBe('Fighter:FRA');
    expect(bomberOf('UK', 1941)).toBe('Wellington');
    expect(fighterOf('FRA', 1940)).toBe('D520');
    expect([bomberOf('JAP', 1940), bomberOf('JAP', 1941), bomberOf('SOV', 1940), bomberOf('SOV', 1941), bomberOf('FRA', 1939), bomberOf('FRA', 1940)]).toEqual(['Nell', 'Betty', 'Bomber:SOV', 'Pe2', 'Bomber:FRA', 'LeO451']);
    expect([enginesOf('SM79'), enginesOf('Betty'), enginesOf('Wellington')]).toEqual([3, 2, 2]);
    expect(bomberGuns('SM79', 'ITA', { year: 1941, month: 3 }).mark).toBe('SM.79');
    expect(fighterGuns('D520', 'FRA', { year: 1940, month: 0 }).mark).toBe('D.520');
    expect(bomberOf('UK', 1942)).toBe('Lancaster');
    expect(bomberOf('USA', 1939)).toBe('B17');
    expect(bomberOf('GER', 1944)).toBe('He111');
    expect(bomberOf('ITA', 1940)).toBe('SM79');
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

describe('the historian\'s corrections (review-historian.md, 18 November)', () => {
  it('escorts: Germany hunts free ahead until September 1940; the Americans are freed in January 1944; Britain and Italy', () => {
    expect(escortRoles('GER', { year: 1939, month: 8 })[0]).toBe('sweep');
    expect(escortRoles('GER', { year: 1940, month: 8 })[0]).toBe('close');
    expect(escortRoles('USA', { year: 1943, month: 11 })).toEqual(['close', 'top']);
    expect(escortRoles('USA', { year: 1944, month: 0 })[0]).toBe('sweep');
    expect(escortRoles('UK', { year: 1940, month: 4 })).toEqual(['close', 'top']);
    expect(escortRoles('ITA', { year: 1941, month: 5 })).toEqual(['close', 'top']);
    expect(escortRoles('JAP', { year: 1942, month: 1 })).toEqual(['close', 'top', 'sweep']);
  });

  it('passes: France from astern, the Soviets from below, Japan head-on only at the four-engined; finger fours from spring 1941', () => {
    const w = { year: 1942, month: 8 };
    const top = (rows: [string, number][]) => rows.filter(([, k]) => k === Math.max(...rows.map(([, x]) => x))).map(([p]) => p);
    expect(top(passesOn('FRA', w, 'He111', 'GER'))).toEqual(['astern', 'quarter', 'beam']);
    expect(passesOn('SOV', w, 'He111', 'GER').map(([p]) => p)).toContain('below');
    expect(top(passesOn('JAP', w, 'B17', 'USA'))).toEqual(['headon']);
    expect(passesOn('JAP', w, 'Bomber:UK', 'UK').find(([p]) => p === 'headon')![1]).toBe(0.1);
    expect(fighterWay('UK', { year: 1941, month: 1 }).form).toBe('line');
    expect(fighterWay('UK', { year: 1941, month: 2 }).form).toBe('fours');
  });
});

/** Each drawn model's muzzles, from its model file (/mnt/project-files/battle-models/air/<Model>.json, 18 November). */
const SOCKETS: Record<string, string[]> = {
  Bf109: ['gun.nose', 'gun.1', 'gun.2'], Fw190: ['gun.nose', 'gun.1', 'gun.2', 'gun.3', 'gun.4'], Zero: ['gun.nose', 'gun.1', 'gun.2'],
  Hurricane: Array.from({ length: 8 }, (_, i) => `gun.${i + 1}`), Spitfire: Array.from({ length: 8 }, (_, i) => `gun.${i + 1}`),
  Warhawk: Array.from({ length: 6 }, (_, i) => `gun.${i + 1}`), Mustang: Array.from({ length: 6 }, (_, i) => `gun.${i + 1}`),
  Yak: ['gun.nose'], Fighter: ['gun.nose', 'gun.1', 'gun.2'],
  He111: ['turret.1', 'turret.2', 'turret.3', 'turret.4'], B17: Array.from({ length: 13 }, (_, i) => `turret.${i + 1}`),
  Lancaster: Array.from({ length: 8 }, (_, i) => `turret.${i + 1}`), Bomber: ['turret.1', 'turret.2'],
};
const NATS = ['GER', 'UK', 'CAN', 'FRA', 'POL', 'ITA', 'SOV', 'USA', 'JAP', 'CHN', 'ROM'];
const FTR = ['Bf109', 'Fw190', 'Hurricane', 'Spitfire', 'Warhawk', 'Mustang', 'Yak', 'Zero', 'Fighter'];
const BMR = ['He111', 'B17', 'Lancaster', 'Bomber'];
const count = (l: Loadout) => l.guns.reduce((s, g) => s + g.n, 0);

describe('the guns (guns.ts; the design\'s section 3 with the historian\'s review)', () => {
  it('every drawn aircraft has guns in every month of the war, each from a muzzle its model has, never one it silences', () => {
    for (const nat of NATS) for (let year = 1939; year <= 1945; year++) for (let month = 0; month < 12; month++) {
      const w = { year, month };
      for (const [kind, models] of [['ftr', FTR], ['bmr', BMR]] as const) for (const model of models) {
        const l = kind === 'ftr' ? fighterGuns(model === 'Fighter' ? `Fighter:${nat}` : model, nat, w) : bomberGuns(model === 'Bomber' ? `Bomber:${nat}` : model, nat, w);
        expect(l.guns.length).toBeGreaterThan(0);
        for (const g of l.guns) {
          expect(['rifle', 'hmg', 'cannon20']).toContain(g.kind);
          expect(g.rate).toBeGreaterThan(0); expect(g.mv).toBeGreaterThan(400); expect(g.rounds).toBeGreaterThan(0);
          expect(g.n).toBeGreaterThanOrEqual(g.at.length);
          for (const a of g.at) { expect(SOCKETS[model]).toContain(a); expect(l.silent).not.toContain(a); }
          if (kind === 'ftr') expect(g.mount).toBe('fixed');
          if (g.mount === 'hand') expect(g.arc).toBeTruthy();
        }
        for (const a of l.silent) expect(SOCKETS[model]).toContain(a);
        if (kind === 'bmr') expect(l.reach!.rifle).toBeGreaterThan(0);
      }
    }
  });

  it('one gunner to an opening: no gunner\'s socket fires two guns, and a gun with no socket says which side\'s window it fires from (Brian, 02:57Z)', () => {
    // Brian on his reference clip: "The video is off with the 2 gunners at the same window". A twin mount is one entry
    // (one opening); two gunners' entries never share a socket. A gun the model has no socket for (the He 111's beam
    // guns, a French bomber's ventral gun) is hand-held with an arc whose middle is on one side, so the 3D game fires it
    // from that side's own window, never from another gun's socket. (A fighter's fixed guns may share gun.nose: the hub
    // cannon and the cowl guns, no gunner, drawn apart by the kit.)
    for (const nat of NATS) for (let year = 1939; year <= 1945; year++) for (let month = 0; month < 12; month++) {
      const w = { year, month };
      for (const [kind, models] of [['ftr', FTR], ['bmr', BMR]] as const) for (const model of models) {
        const l = kind === 'ftr' ? fighterGuns(model === 'Fighter' ? `Fighter:${nat}` : model, nat, w) : bomberGuns(model === 'Bomber' ? `Bomber:${nat}` : model, nat, w);
        const used = l.guns.filter((g) => g.mount !== 'fixed').flatMap((g) => g.at);
        expect(new Set(used).size, `${l.mark} ${year}-${month + 1}: ${used.join(' ')}`).toBe(used.length);
        for (const g of l.guns.filter((x) => !x.at.length)) {
          expect(g.mount).toBe('hand');
          expect(g.arc).toBeTruthy();
        }
      }
    }
    const beams = bomberGuns('He111', 'GER', { year: 1943, month: 6 }).guns.filter((g) => !g.at.length);
    const mid = (g: (typeof beams)[number]): number => { const [a, b] = g.arc!.yaw; return ((a + ((b - a + 360) % 360) / 2) % 360); };
    expect(beams.map(mid).sort((a, b) => a - b)).toEqual([90, 270]);
  });

  it('flies the marks the historian gave: the 109 by month in 1940, the RAF\'s convergence, the four-gun Mustang, the B-17 by year', () => {
    const e1 = fighterGuns('Bf109', 'GER', { year: 1939, month: 8 });
    expect(e1.mark).toBe('Bf 109E-1');
    expect(e1.guns.map((g) => [g.gun, g.n, g.rounds])).toEqual([['MG 17', 2, 1_000], ['MG 17', 2, 500]]);
    expect(fighterGuns('Bf109', 'GER', { year: 1940, month: 4 }).mark).toBe('Bf 109E-3');
    expect(fighterGuns('Bf109', 'GER', { year: 1940, month: 6 }).mark).toBe('Bf 109E-4');
    expect(fighterGuns('Bf109', 'GER', { year: 1941, month: 3 }).silent).toEqual(['gun.1', 'gun.2']);
    expect(fighterGuns('Hurricane', 'UK', { year: 1940, month: 4 }).conv).toBe(366);
    expect(fighterGuns('Hurricane', 'UK', { year: 1940, month: 5 }).conv).toBe(229);
    expect(count(fighterGuns('Mustang', 'USA', { year: 1944, month: 4 }))).toBe(4);
    expect(count(fighterGuns('Mustang', 'USA', { year: 1944, month: 5 }))).toBe(6);
    expect(count(bomberGuns('He111', 'GER', { year: 1939, month: 8 }))).toBe(3);
    expect(count(bomberGuns('B17', 'USA', { year: 1942, month: 7 }))).toBe(10);
    const g = bomberGuns('B17', 'USA', { year: 1943, month: 9 });
    expect(count(g)).toBe(13);
    // The B-17G as the modeller laid her out again on 19 November: the tail pair at turret.12 and .13, the radio gun at .11.
    expect(g.guns.find((x) => x.arc?.yaw[0] === 150)!.at).toEqual(['turret.12', 'turret.13']);
    expect(bomberGuns('B17', 'USA', { year: 1944, month: 6 }).silent).toEqual(['turret.11']);
    expect(count(bomberGuns('Bomber:FRA', 'FRA', { year: 1939, month: 8 }))).toBe(3);
    expect(fighterGuns('Fighter:ITA', 'ITA', { year: 1940, month: 5 }).guns.map((g) => g.kind)).toEqual(['hmg']);
  });

  it('rides on the ticket, so the 3D game fires the guns of the month', () => {
    const t = ticketOf('GER', 'bomb', 'Alsace', 0);
    expect(t.place.guns!.GER.ftr.mark).toBe('Bf 109E-1');
    expect(t.place.guns!.GER.bmr.mark).toBe('He 111P');
    expect(t.place.guns!.FRA.ftr.mark).toBe('MS.406');
  });
});

/** The Alsace raid's escort split into `parts` groups, fought in a given month (as the escort roles' parity runs). */
function splitEscort(parts: number, when: { year: number; month: number }) {
  const t = structuredClone(ticketOf('GER', 'bomb', 'Alsace', 0));
  t.place.when = when;
  const f = t.att.forces.find((x) => x.kind !== 'bmr')!, each = Math.floor(f.men / parts);
  for (let i = 1; i < parts; i++) { t.att.forces.push({ ...f, corps: 90_000 + i, men: each }); t.place.starts[90_000 + i] = { ...t.place.starts[f.corps] }; }
  f.men -= each * (parts - 1);
  return t;
}

describe('escorts doing their job (Brian, 19:38Z; ruling 237.2)', () => {
  const roles = (s: AirSim) => s.units.filter((u) => u.side === 'att' && u.air === 'ftr').map((u) => (u.job.do === 'escort' ? (u.job.close ? 'close' : u.job.top ? 'top' : 'sweep') : u.job.do));

  it('deals each escort group its role in its doctrine\'s order, top cover only from the third group', () => {
    const at = (parts: number, when: { year: number; month: number }) => roles(airBattle(splitEscort(parts, when)));
    expect(at(1, { year: 1939, month: 8 })).toEqual(['sweep']);
    expect(at(1, { year: 1940, month: 8 })).toEqual(['close']);
    expect(at(2, { year: 1939, month: 8 })).toEqual(['sweep', 'close']);
    expect(at(3, { year: 1940, month: 8 })).toEqual(['close', 'sweep', 'top']);
  });

  it('top cover keeps a band above its box and behind it, dives on the attackers first, and climbs back; close escort keeps its box\'s band', () => {
    const s = airBattle(splitEscort(3, { year: 1940, month: 8 }), { show: true }); s.ai.att = true; s.ai.def = true;
    const top = s.units.find((u) => u.job.do === 'escort' && u.job.top)!;
    let held = 0;
    while (!s.over) {
      s.step();
      for (const u of s.units) {
        const j = u.job;
        if (j.do !== 'escort' || u.n <= 0 || u.contacts.length || u.state !== 'steady' || u.to !== u.band) continue;
        const b = s.unit(j.on)!;
        if (j.top) { held++; expect(u.band).toBe(Math.min(2, b.band + 1)); expect((s.view().units.find((v) => v.id === u.id)!.extra as { role: string }).role).toBe('top'); }
        if (j.close) expect(u.band).toBe(b.band);
      }
    }
    expect(held).toBeGreaterThan(0);
    expect(s.history().find((e) => e.what === 'air:bounce' && e.unit === top.id)).toBeDefined();
  });
});

describe('the words the player reads (the UI director, 21:19Z)', () => {
  it('a bomber falling out, a straggler turning for home, an escort breaking away once; its return untold', () => {
    const s = airBattle(splitEscort(3, { year: 1940, month: 8 }), { show: true }); s.ai.att = true; s.ai.def = true;
    while (!s.over) s.step();
    const h = s.history();
    const out = h.filter((e) => e.what === 'air:straggle');
    expect(out.length).toBeGreaterThan(0);
    for (const e of out) expect(e.text).toMatch(/^(A bomber of .+ has lost an engine and|Flak has knocked out an engine on a bomber of .+, and it) is falling out of formation\.$/);
    for (const e of h.filter((e) => e.what === 'air:limped' && e.text)) expect(e.text).toMatch(/^A damaged bomber of .+ has turned for home on its own\.$/);
    const told = h.filter((e) => e.what === 'air:engage' && e.text);
    expect(told.length).toBeGreaterThan(0);
    expect(new Set(told.map((e) => e.unit)).size).toBe(told.length);
    for (const e of told) expect(e.text).toMatch(/ broken away from .+ to take on the attackers\.$/);
    expect(h.filter((e) => e.what === 'air:rejoin').every((e) => e.text === '')).toBe(true);
    for (const e of h) expect(e.text).not.toMatch(/[[\]{}]/);
  });
});

describe('flak round a box (the art director\'s flak brief)', () => {
  it('fires only inside the ring on the way in, by the guns that reach the box\'s height, and never once it turns for home', () => {
    let seen = 0;
    fight('GER', 'bomb', 'Alsace', 0, true, (s) => {
      for (const v of s.view().units) {
        if (v.air !== 'bmr' || v.state === 'gone') continue;
        const u = s.unit(v.id)!, flak = (v.extra as { flak: string[] }).flak;
        const inside = Math.hypot(u.x - SKY.target.x, u.y - SKY.target.y) <= SKY.flakR;
        if (flak.length) { seen++; expect(inside).toBe(true); expect(u.dropped > 0 || u.jettisoned).toBe(false); expect(flak).toEqual(SKY.flak.kinds[u.band]); }
        if (u.dropped > 0 || u.jettisoned) expect(flak).toEqual([]);
      }
    });
    expect(seen).toBeGreaterThan(0);
    expect(SKY.flak.kinds[2]).toEqual(['heavy']);
  });
});
