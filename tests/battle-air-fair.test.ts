// Ruling 226 on the air battle (the design director, 19:42Z; the playtest's b384, 20:15Z; b397, 20:51Z): the first thing
// a newcomer tries is never a trap, the break can be seen coming, and the staff's question says what the staff would do
// now. Mark 4 (b397): what a newcomer does (everything picked up, Space, the enemy group right-clicked at the first
// contact) and Space alone, with the computer flying the other side, each win within a tenth of the raid card's chance
// on every raid the play file offers, over the play file's own battles: the September opening, each with its own luck,
// and the groups it sends. The battles are fixed, so the test is exact from run to run; the tolerance allows for the
// luck of 30 raids.
import { describe, expect, it } from 'vitest';
import { launchRaid, planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { railChanceOf, ruinChanceOf, staffHow } from '../src/campaign/airfought';
import { createGame, setWar } from '../src/campaign/game';
import { railBombChance, ruinChance } from '../src/campaign/rails';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { SKY } from '../src/battle/air/book';
import { airBattle, type AirSim } from '../src/battle/air/sim';
import { airTicket, escortPair, foughtAir, raidGroups } from '../src/battle/air/ticket';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };
const RUNS = 30;
// The play file's raids (b401, as the design director ruled at 07:38Z on 22 November and re-ruled at 08:48Z and 08:50Z:
// the third France's, on the nearest industrial target its escort reaches, offered alone and escorted, both flown).
const RAIDS: [string, RaidKind, string, 'alone'?][] = [['GER', 'bomb', 'Alsace'], ['GER', 'superiority', 'Alsace'], ['FRA', 'bomb', 'Saar'], ['FRA', 'bomb', 'Saar', 'alone']];

/** The play file's k-th battle: the September 1939 opening, with that battle's own luck (air-page.ts raidOf). */
function game(k: number, nat: string): Game {
  const g = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  return g;
}

/**
 * The raid on a named province with the groups the play file sends (raidGroups: from the nation's own airfields); flown
 * 'alone', the pair's alone card (escortPair: the same bombers and no fighter group).
 */
function raid(g: Game, nat: string, kind: RaidKind, name: string, alone?: 'alone') {
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  if (alone) { const p = escortPair(g, kind, t.id, nat); if (!p) throw new Error(`no pair on ${name}`); return { target: t.id, ...p.alone }; }
  const ids = raidGroups(g, kind, t.id, nat);
  const { ticket, why } = airTicket(g, kind, t.id, ids, nat);
  if (!ticket) throw new Error(why ?? 'no ticket');
  return { target: t.id, ids, ticket };
}

/** What a newcomer does: box-select everything, Space, and right-click the enemy group his own first meet. */
function newcomer(s: AirSim): void {
  const mine = s.units.filter((u) => u.side === 'att').map((u) => u.id);
  let given = false;
  while (!s.over) {
    if (!given && s.fired) {
      const foe = s.units.filter((u) => u.side === 'att').flatMap((u) => u.contacts).find((id) => s.unit(id)?.side === 'def');
      if (foe !== undefined) { s.command({ do: 'attack', units: mine, target: foe }, 'att'); given = true; }
    }
    s.step();
  }
}

/** Space, and nothing else. */
function space(s: AirSim): void {
  while (!s.over) s.step();
}

describe('the plain order honours the raid card (ruling 226, mark 4; b397)', () => {
  for (const [nat, kind, name, alone] of RAIDS) {
    for (const [how, play] of [['what a newcomer does', newcomer], ['Space alone', space]] as [string, (s: AirSim) => void][]) {
      it(`${kind === 'superiority' ? 'the fight for the sky over' : 'the day raid on'} ${name}${alone ? ', flown alone' : ''}: ${how}`, () => {
        let won = 0, chance = 0;
        for (let k = 0; k < RUNS; k++) {
          const g = game(k, nat);
          const r = raid(g, nat, kind, name, alone);
          chance = r.ticket.place.chance;
          const s = airBattle(r.ticket);
          s.ai.def = true;
          play(s);
          const rep = launchRaid(g, kind, r.target, r.ids, nat, foughtAir(s.outcome()))!;
          // The stamp is the field's (mark 6): the battle's own verdict and the campaign's agree.
          expect(rep.won, `${name} battle ${k}`).toBe(s.over!.level.includes('victory'));
          if (rep.won) won++;
        }
        expect(Math.abs(won / RUNS - chance), `${name}: won ${won} of ${RUNS}, the card gives ${chance.toFixed(2)}`).toBeLessThanOrEqual(0.1);
      }, 240_000);
    }
  }

  it('the third raid is the same raid alone and escorted, both cards from one reckoning, the escort worth the difference (b401)', () => {
    // France's fighters do not reach the Ruhr from any French field in September 1939: its raid there flies alone, so
    // the third raid is on the nearest industrial target its escort reaches, the Saar.
    expect(raid(game(0, 'FRA'), 'FRA', 'bomb', 'Ruhr').ticket.att.forces.every((f) => f.kind === 'bmr')).toBe(true);
    for (let k = 0; k < 4; k++) {
      const g = game(k, 'FRA');
      const t = raidTargets(g, 'bomb', 'FRA').find((x) => planRaid(g, 'bomb', x.id, undefined, 'FRA').place === 'Saar')!;
      const before = JSON.stringify(g);
      const p = escortPair(g, 'bomb', t.id, 'FRA')!;
      // Both cards present: the same bombers, alone and with every fighter group that can go with them.
      expect(p, `battle ${k}`).not.toBeNull();
      const bmr = (ts: typeof p.alone.ticket) => ts.att.forces.filter((f) => f.kind === 'bmr').map((f) => `${f.corps}:${f.men}`).join(' ');
      expect(p.alone.ticket.att.forces.every((f) => f.kind === 'bmr')).toBe(true);
      expect(p.escorted.ticket.att.forces.some((f) => f.kind !== 'bmr')).toBe(true);
      expect(bmr(p.alone.ticket)).toBe(bmr(p.escorted.ticket));
      expect(p.escorted.ids).toEqual(raidGroups(g, 'bomb', t.id, 'FRA'));
      // From one reckoning: the game is not touched, and each card is the raid card's own (airTicket on the same game,
      // the same month and luck), so the two differ only by the escort.
      expect(JSON.stringify(g)).toBe(before);
      expect(p.alone.ticket).toEqual(airTicket(g, 'bomb', t.id, p.alone.ids, 'FRA').ticket);
      expect(p.escorted.ticket).toEqual(airTicket(g, 'bomb', t.id, p.escorted.ids, 'FRA').ticket);
      expect(p.alone.ticket.seed).toBe(p.escorted.ticket.seed);
      expect(p.alone.ticket.def.forces.length).toBeGreaterThan(0);
      // No new number: the escort's worth is the one card less the other, and nothing of its own.
      expect(p.worth).toEqual({
        chance: p.escorted.ticket.place.chance - p.alone.ticket.place.chance,
        bmr: p.alone.ticket.place.forecast.bmr - p.escorted.ticket.place.forecast.bmr,
      });
      expect(Object.keys(p).sort()).toEqual(['alone', 'escorted', 'worth']);
      expect(Object.keys(p.worth).sort()).toEqual(['bmr', 'chance']);
    }
    // No band. Struck, not deleted: b401 re-ruled 22 Nov by the DD: the band was set for an unescorted raid; the third
    // raid's choice is now the escort, shown as the same raid alone and escorted. Both cards are foregone, on purpose,
    // and nobody should move the raid toward the middle: it is the record's lesson of 1939 and 1940, day bombers without
    // an escort cut to pieces (alone about 1 in 100, 60 of its 100 bombers lost, the 0.6 day cap) and fighters overhead
    // changing that (escorted about 99 in 100, 4.5 lost). The uncertain outcome is carried by the battles before it,
    // the raid on Alsace about 4 in 10. (The figures inside, the 0.6 day cap and the escort's 18 lost, stand pending the
    // count-switch rulings.)
    //   expect(r.ticket.place.chance).toBeGreaterThanOrEqual(0.3);
    //   expect(r.ticket.place.chance).toBeLessThanOrEqual(0.7);
  });

  it("a raid's groups fly from the nation's own airfields: Bremen would be the RAF's, not the Home Fleet's (b397)", () => {
    const r = raid(game(0, 'UK'), 'UK', 'bomb', 'Bremen');
    expect(r.ticket.place.from).toBe('land');
    expect(r.ticket.att.forces.map((f) => f.name).join(' ')).not.toMatch(/Home Fleet/);
    expect(r.ticket.att.forces.some((f) => f.kind === 'bmr')).toBe(true);
  });
});

describe('the plain order honours the raid card when radar warns the defence (the tactics panel, item 5)', () => {
  // None of the play file's three raids is warned, so the climb (ruling 239: drawn, never priced) would go unmeasured
  // against mark 4. Kent: radar warns the computer's defence, which climbs above the raid before it meets it. This holds
  // the card's odds; that the climb's own dive goes unpriced is held in battle-air-styles.test.ts.
  for (const [how, play] of [['what a newcomer does', newcomer], ['Space alone', space]] as [string, (s: AirSim) => void][]) {
    it(`the day raid on Kent: ${how}`, () => {
      let won = 0, chance = 0;
      for (let k = 0; k < RUNS; k++) {
        const g = game(k, 'GER');
        const r = raid(g, 'GER', 'bomb', 'Kent');
        expect(r.ticket.place.radar && !r.ticket.place.night).toBe(true);
        chance = r.ticket.place.chance;
        const s = airBattle(r.ticket);
        s.ai.def = true;
        play(s);
        const rep = launchRaid(g, 'bomb', r.target, r.ids, 'GER', foughtAir(s.outcome()))!;
        expect(rep.won, `Kent battle ${k}`).toBe(s.over!.level.includes('victory'));
        if (rep.won) won++;
      }
      expect(Math.abs(won / RUNS - chance), `Kent: won ${won} of ${RUNS}, the card gives ${chance.toFixed(2)}`).toBeLessThanOrEqual(0.1);
    }, 240_000);
  }
});

describe('what the bombs do is decided on the field, by the campaign\'s law (b397)', () => {
  it('the law weighs the bombs as the rules do', () => {
    for (const [nat, name] of [['GER', 'Alsace'], ['UK', 'Bremen'], ['FRA', 'Ruhr']] as [string, string][]) {
      const g = game(0, nat);
      const r = raid(g, nat, 'bomb', name);
      const L = r.ticket.place.bombs!;
      expect(L).toBeTruthy();
      for (const n of [0, 1, 40, 140, 300, 900, 3000]) {
        expect(railChanceOf(L, n), `${name} rail at ${n}`).toBeCloseTo(railBombChance(g, r.target, n * L.hit, L.rail ? L.rail.stop : 0), 12);
        expect(ruinChanceOf(L, n), `${name} ruin at ${n}`).toBeCloseTo(ruinChance(g, g.regions[r.target], Math.min(L.out.most, (n * L.hit) / L.out.scale) * L.out.keep), 12);
      }
    }
  });

  it('the line the field shows cut is the line the campaign cuts, and the dispatch says so as it happens', () => {
    let cut = 0;
    for (let k = 0; k < 20; k++) {
      const g = game(k, 'GER');
      const r = raid(g, 'GER', 'bomb', 'Alsace');
      const s = airBattle(r.ticket);
      s.ai.def = true;
      space(s);
      const told = s.history().filter((e) => e.what === 'air:rail');
      expect(told.length).toBe(s.railCut ? 1 : 0);
      const rep = launchRaid(g, 'bomb', r.target, r.ids, 'GER', foughtAir(s.outcome()))!;
      expect(/railway through Alsace is cut/.test(rep.result ?? ''), `battle ${k}`).toBe(s.railCut);
      if (s.railCut) cut++;
    }
    // Some cut and some not: the throw is the battle's own.
    expect(cut).toBeGreaterThan(0);
    expect(cut).toBeLessThan(20);
  });
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

  it("what you had won stays won only when the fight has moved the player's way by 5 points or more", () => {
    expect(staffHow('att', { chance: 0.7, fired: true }, 0.56)).toBe('won');
    expect(staffHow('att', { chance: 0.61, fired: true }, 0.56)).toBe('won');
    expect(staffHow('att', { chance: 0.6, fired: true }, 0.56)).toBe('rest');
    expect(staffHow('att', { chance: 0.38, fired: true }, 0.37)).toBe('rest');
    expect(staffHow('att', { chance: 0.4, fired: true }, 0.56)).toBe('rest');
    expect(staffHow('def', { chance: 0.4, fired: true }, 0.56)).toBe('won');
    expect(staffHow('def', { chance: 0.7, fired: true }, 0.56)).toBe('rest');
  });

  it("the raid's bar counts its escort and its losses, not fresh bombers, and the enemy's line names the cause", () => {
    const g = game(0, 'GER');
    const s = airBattle(raid(g, 'GER', 'bomb', 'Alsace').ticket);
    s.ai.def = true;
    let most = 0;
    while (!s.over) { s.step(); most = Math.max(most, s.nearBreak('att')); }
    // 300 bombers that have barely been touched no longer hold the raid's bar near half: its escort fights itself down.
    expect(most).toBeGreaterThanOrEqual(SKY.warn);
    const lines = s.history().filter((e) => e.what === 'air:near').map((e) => e.text!);
    expect(lines).toContain('Your raid is near breaking off.');
    expect(lines.find((l) => l.startsWith('The French'))).toMatch(/^The French fighters are near breaking off: (short of ammunition|shaken)\.$/);
  });

  it("the score on the field is the stamp's: the aim slip's counts give the verdict the campaign books", () => {
    for (let k = 0; k < 6; k++) {
      const g = game(k, 'FRA');
      const r = raid(g, 'FRA', 'bomb', 'Ruhr');
      const s = airBattle(r.ticket);
      s.ai.def = true;
      space(s);
      const sc = s.score();
      const rep = launchRaid(g, 'bomb', r.target, r.ids, 'FRA', foughtAir(s.outcome()))!;
      expect(sc.ratio >= 1).toBe(rep.won);
      const lost = rep.att.reduce((n, x) => n + x.lostFtr + x.lostBmr, 0);
      expect(lost).toBe(sc.lost.att);
      expect(rep.def.reduce((n, x) => n + x.lostFtr + x.lostBmr, 0)).toBe(sc.lost.def);
    }
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

  it('each ground carries true north on the turned board: a raid flying east has it on the left, one flying north has it ahead', () => {
    const north = (nat: string, name: string) => raid(game(7, nat), nat, 'bomb', name).ticket.place.ground!.north;
    const bremen = north('UK', 'Bremen'), rhineland = north('FRA', 'Rhineland'), kent = north('GER', 'Kent');
    for (const n of [bremen, rhineland, kent]) expect(Math.hypot(n[0], n[1])).toBeCloseTo(1, 5);
    // The board always has the raid come on from its south edge, so the map is turned under it.
    expect(bremen[0]).toBeLessThan(-0.9); // from England to Bremen the raid flies east
    expect(rhineland[1]).toBeGreaterThan(0.9); // from France to the Rhineland it flies north
    expect(kent[0]).toBeGreaterThan(0.9); // across the Channel to Kent it flies west
  });
});
