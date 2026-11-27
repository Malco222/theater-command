// Brian's battle details (b477, trimmed for the browser by b479): the battle count each unit keeps, counted in
// staff-settled and hand-fought battles alike; which tanks carry a name or rings, as data; and the wrecks that stay on
// the field to the end of the fight, where men on foot take light cover.
import { describe, expect, it } from 'vitest';
import { CREW_NAMES } from '../src/battle/land/crew-names';
import { crewLine, crewMarks, CREWS } from '../src/battle/land/crews';
import { handBack, LAND, landTicket, LandBattle, sampleTicket, type LandUnit } from '../src/battle/land';
import { runOut } from '../src/battle/core';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, candidates, humanUp, orderAttack, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

function playing(seed = 3): Game {
  const g = startOpen(createGame(seed, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

const battlesOf = (c: Corps): number => c.rec?.battles ?? 0;

describe('the battle count each unit keeps (b477)', () => {
  it('a battle counts once for every corps in it, settled by the staff or fought by hand', () => {
    const pick = (g: Game): { cs: Corps[]; r: Region } => {
      const r = g.regions.find((x) => x.name === 'Pomorze')!;
      return { r, cs: candidates(g, r).filter((c) => c.name === 'XIX Corps' || c.name === 'XXI Corps') };
    };
    // SEND THEM IN.
    const g1 = playing(3), p1 = pick(g1);
    const defs1 = g1.corps.filter((c) => c.reg === p1.r.id && c.nat !== 'GER');
    const before1 = [...p1.cs, ...defs1].map(battlesOf);
    expect(orderAttack(g1, p1.cs, p1.r)).toBeTruthy();
    expect([...p1.cs, ...defs1].map(battlesOf)).toEqual(before1.map((n) => n + 1));
    // FIGHT IT YOURSELF, from the same save.
    const g2 = playing(3), p2 = pick(g2);
    const defs2 = g2.corps.filter((c) => c.reg === p2.r.id && c.nat !== 'GER');
    const before2 = [...p2.cs, ...defs2].map(battlesOf);
    const t = landTicket(g2, p2.cs, p2.r)!;
    const b = new LandBattle(t);
    b.ai.att = b.ai.def = true;
    runOut(b);
    expect(handBack(g2, p2.cs, p2.r, b.over!)).toBeTruthy();
    expect([...p2.cs, ...defs2].map(battlesOf)).toEqual(before2.map((n) => n + 1));
    // The field carries the count each corps came with.
    for (const f of [...t.att.forces, ...t.def.forces]) expect(f.battles).toBe(before2[[...p2.cs, ...defs2].findIndex((c) => c.id === f.corps)]);
  });
});

describe('which tanks carry a crew\'s name (b477)', () => {
  it('America, Britain and the Soviet Union name every tank from the third battle', () => {
    for (const nat of ['USA', 'UK', 'SOV', 'CAN']) {
      expect(crewMarks(nat, 'arm', CREWS.veteran - 1, 6, 'x')).toBeNull();
      const m = crewMarks(nat, 'arm', CREWS.veteran, 6, 'x')!;
      expect(m.mark).toBe('name');
      expect(m.names!.every((n) => n)).toBe(true);
      // Six different names on six tanks.
      expect(new Set(m.names!.map((n) => n!.name)).size).toBe(6);
    }
  });
  it('France names every tank from the start; other armies of the record few, and the rest none', () => {
    expect(crewMarks('FRA', 'arm', 0, 5, 'x')!.names!.every((n) => n)).toBe(true);
    let named = 0, tanks = 0;
    for (const nat of ['JAP', 'ITA', 'POL']) {
      expect(crewMarks(nat, 'arm', CREWS.veteran - 1, 8, 'x')).toBeNull();
      for (let i = 0; i < 40; i++) {
        const m = crewMarks(nat, 'arm', CREWS.veteran, 8, `${nat}${i}`);
        tanks += 8;
        named += m?.names?.filter((n) => n).length ?? 0;
      }
    }
    expect(named / tanks).toBeGreaterThan(0.15);
    expect(named / tanks).toBeLessThan(0.35);
    for (const nat of ['HUN', 'ROM', 'FIN']) expect(crewMarks(nat, 'arm', 10, 6, 'x')).toBeNull();
    // Only tanks: an infantry block carries nothing.
    expect(crewMarks('USA', 'inf', 10, 6, 'x')).toBeNull();
  });
  it('Germany names none: its veterans carry one to five rings a tank, more with the battles, varying', () => {
    expect(crewMarks('GER', 'arm', CREWS.veteran - 1, 6, 'x')).toBeNull();
    const at = (n: number): number[] => crewMarks('GER', 'arm', n, 12, 'pz')!.rings!;
    for (const n of [3, 5, 9, 20]) {
      const r = at(n);
      expect(r.every((k) => k >= 1 && k <= CREWS.rings.most)).toBe(true);
      expect(crewMarks('GER', 'arm', n, 12, 'pz')!.names).toBeUndefined();
    }
    const mean = (r: number[]): number => r.reduce((a, b) => a + b, 0) / r.length;
    expect(mean(at(9))).toBeGreaterThan(mean(at(3)));
    expect(new Set(at(7)).size).toBeGreaterThan(1);
    // No count on screen: the hover says nothing of rings.
    expect(crewLine(crewMarks('GER', 'arm', 9, 12, 'pz'))).toBeNull();
  });
  it('names only what the army\'s line tank has room for (the modeller\'s names-fit.json)', () => {
    // The M13/40 takes eight of Italy's twenty; the 7TP seventeen of Poland's.
    expect(CREW_NAMES.ITA.length).toBe(8);
    expect(CREW_NAMES.ITA.map((n) => n.name)).not.toContain('Bella Napoli');
    expect(CREW_NAMES.POL.map((n) => n.name)).not.toContain('Niedźwiedź');
    for (const a of ['USA', 'UK', 'FRA', 'SOV'] as const) expect(CREW_NAMES[a].length).toBe(20);
    // The Type 97 takes the ten whose characters are drawn; the others are never given her.
    expect(CREW_NAMES.JAP.length).toBe(10);
    expect(CREW_NAMES.JAP.map((n) => n.name)).not.toContain('Ume no Hana');
  });
  it('a block names its tanks the same way every battle', () => {
    expect(crewMarks('UK', 'arm', 4, 6, '7:1st Armoured')).toEqual(crewMarks('UK', 'arm', 9, 6, '7:1st Armoured'));
  });
  it('the hover gives the names with their English meaning where the name is not English', () => {
    const by = (n: string) => CREW_NAMES.ITA.find((x) => x.name === n)!;
    const it = { mark: 'name' as const, names: [by('Vesuvio'), null, by('Tuono')] };
    expect(crewLine(it)).toBe('Tanks named by their crews: Vesuvio (Vesuvius), Tuono (Thunder)');
    const us = (n: number) => ({ mark: 'name' as const, names: CREW_NAMES.USA.slice(0, n) });
    expect(crewLine(us(5))).toBe('Tanks named by their crews: Alabama Belle, Ace High, Atlas and 2 more');
    // Exactly four: all four, never "and 1 more".
    expect(crewLine(us(4))).toBe('Tanks named by their crews: Alabama Belle, Ace High, Atlas, Buckeye');
    const za = CREW_NAMES.SOV.find((x) => x.name === 'Za Rodinu')!;
    expect(crewLine({ mark: 'name', names: [za] })).toBe('Tanks named by their crews: Za Rodinu (For the Motherland)');
  });
});

describe('burning wrecks stay (b477)', () => {
  const battle = (): LandBattle => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { seed: 7 }));
    expect(b.units.some((u) => u.kind === 'arm')).toBe(true);
    return b;
  };
  const hurt = (b: LandBattle, u: LandUnit, share: number): void => (b as unknown as { suffer(t: LandUnit, d: number, n: number): void }).suffer(u, u.power0 * share, 1);
  it('every tank a block loses leaves a wreck where it stood, to the end of the fight; men lost leave none', () => {
    const b = battle();
    const tank = b.units.find((u) => u.kind === 'arm')!, foot = b.units.find((u) => u.kind === 'inf')!;
    const had = tank.figures;
    hurt(b, tank, 0.4);
    hurt(b, foot, 0.4);
    const lost = had - tank.figures;
    expect(lost).toBeGreaterThan(0);
    const zones = b.view().zones.filter((z) => z.kind === 'wreck');
    expect(zones.length).toBe(lost);
    for (const z of zones) {
      expect(z.unit).toBe(tank.id);
      expect(z.side).toBe(tank.side);
      expect(z.until).toBe(b.ticket.limit);
      expect(z.r).toBe(LAND.wreck.r);
      // Within the block's frontage and depth where it stood.
      expect(Math.hypot(z.x - tank.x, z.y - tank.y)).toBeLessThanOrEqual(Math.hypot(tank.w, tank.d) / 2);
    }
    // A wreck's place is the block's and the count's, never the battle's dice: the same in the same battle.
    const b2 = battle(), t2 = b2.units.find((u) => u.id === tank.id)!;
    hurt(b2, t2, 0.4);
    expect(b2.view().zones).toEqual(b.view().zones);
  });
  it('men on foot beside a wreck take light cover, either army\'s, never better than their ground', () => {
    const b = battle();
    const foot = b.units.find((u) => u.kind === 'inf')!;
    const share = (): number => (b as unknown as { wreckShare(u: LandUnit): number }).wreckShare(foot);
    expect(share()).toBe(0);
    // An enemy's wreck in its frontage, and one of its own army's beside it: one model's worth each.
    b.wrecks.push({ id: 1, kind: 'wreck', x: foot.x + 5, y: foot.y, r: LAND.wreck.r, until: b.ticket.limit, side: foot.side === 'att' ? 'def' : 'att', at: 0, unit: 0 });
    b.wrecks.push({ id: 2, kind: 'wreck', x: foot.x, y: foot.y + foot.d / 2 + LAND.wreck.beside - 1, r: LAND.wreck.r, until: b.ticket.limit, side: foot.side, at: 0, unit: 0 });
    expect(share()).toBeCloseTo(2 / foot.tray, 6);
    // Far off, none.
    b.wrecks.push({ id: 3, kind: 'wreck', x: foot.x + 2000, y: foot.y, r: LAND.wreck.r, until: b.ticket.limit, side: foot.side, at: 0, unit: 0 });
    expect(share()).toBeCloseTo(2 / foot.tray, 6);
    // The cover is the field's own figure for broken ground.
    expect(LAND.cover[LAND.wreck.cover]).toBeLessThan(LAND.cover.open);
    expect(LAND.cover[LAND.wreck.cover]).toBeGreaterThan(LAND.cover.woods);
  });
});

