// The computer general's styles on the land field (the tactics panel, item 1; the design director, ruling 239): bold,
// methodical and mass, read only from the ticket; the nations' leanings at history dial stops 1 to 3, grown by
// experience or research, never by date; the situation's pick at stops 4 and 5, never the nation; never the player's.
import { describe, expect, it } from 'vitest';
import lean from '../data/battle/lean.json';
import styles from '../data/battle/land-styles.json';
import groundCard from '../data/campaign/ground.json';
import supportCard from '../data/campaign/support.json';
import { LAND, LandBattle, LEAN, PATH, STYLE, STYLES, coverAt, coveredTanks, pickFor, sampleTicket, styleFor, type LandUnit, type StyleId } from '../src/battle/land';
import type { FieldTicket, SideTicket } from '../src/battle/core';
import type { LandPlace } from '../src/battle/land';

const G7 = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const;
/** A ticket with these nations, this experience and this stop; the attack's corps as the practice attack's. */
const at = (nat: { att?: string; def?: string }, o: { hist?: number; xp?: number; doctrine?: string[]; terrain?: (typeof G7)[number]; odds?: number } = {}): FieldTicket<LandPlace> => {
  const t = sampleTicket(o.terrain ?? 'plains', o.odds ?? 1.3, { nat, ...(o.hist !== undefined ? { hist: o.hist } : {}) });
  const side = (s: SideTicket): SideTicket => ({ ...s, forces: s.forces.map((f) => ({ ...f, xp: o.xp ?? f.xp })), ...(o.doctrine ? { doctrine: o.doctrine } : {}) });
  return { ...t, att: side(t.att), def: side(t.def) };
};
const id = (t: FieldTicket<LandPlace>, side: 'att' | 'def'): StyleId | undefined => styleFor(t, side)?.id;

describe('styles: the numbers on one plan (the tactics panel, item 1)', () => {
  it('three styles; a methodical defence is the one the field always had', () => {
    expect(Object.keys(STYLES).sort()).toEqual(['bold', 'mass', 'methodical']);
    expect(STYLES.methodical).toMatchObject({ defLeash: 300, defFinish: 150 });
    expect(STYLES.mass).toMatchObject({ plan: false, hook: null });
    expect(STYLES.bold.wait).toBeLessThan(STYLES.methodical.wait);
    expect(STYLES.bold.deep && !STYLES.methodical.deep).toBe(true);
  });
  it('one lean table, a column per arm; the land column names only styles or a stand-in (ruling 239(1), rider 20:58Z)', () => {
    const L = lean as unknown as { lean: Record<string, Record<string, string | { att: string; def: string }>>; standIn: Record<string, string> };
    for (const [nat, row] of Object.entries(L.lean)) {
      expect(Object.keys(row).sort(), nat).toEqual(['air', 'land', 'sea']);
      const land = row.land;
      for (const s of typeof land === 'string' ? [land] : [land.att, land.def]) expect(s in STYLES || L.standIn[s] in STYLES, `${nat} ${s}`).toBe(true);
    }
    expect(LEAN.GER).toBe('bold');
    for (const n of ['UK', 'USA', 'ITA', 'FRA']) expect(LEAN[n]).toBe('methodical');
    expect(LEAN.SOV).toBe('mass');
    expect(LEAN.JAP).toEqual({ att: 'night', def: 'last-stand' });
    expect(L.standIn.night).toBe('bold');
    expect(L.standIn['last-stand']).toBe('bold');
    // Nations are named in the lean table and the growth paths and nowhere else in the styles (Brian, 19 Nov: a sandbox).
    expect(JSON.stringify((styles as Record<string, unknown>).styles)).not.toMatch(/GER|SOV|USA|UK|ITA|FRA|JAP/);
  });
  it('never the player: a side the player commands has no style, on any ticket', () => {
    for (const hist of [1, 3, 5]) {
      const t = at({ att: 'GER', def: 'SOV' }, { hist });
      expect(styleFor({ ...t, att: { ...t.att, human: true } }, 'att')).toBeNull();
      expect(styleFor({ ...t, def: { ...t.def, human: true } }, 'def')).toBeNull();
      expect(styleFor({ ...t, att: { ...t.att, human: true, style: 'bold' } }, 'att')).toBeNull();
    }
  });
  it('a style fixed on the ticket by hand is the one fought (tests, a sandbox pairing)', () => {
    for (const s of ['bold', 'methodical', 'mass'] as const) expect(styleFor(sampleTicket('forest', 1.2, { style: { att: s, def: s } }), 'def')?.id).toBe(s);
  });
  it('the practice armies, no nation of their own, lean methodical', () => {
    expect(id(sampleTicket('plains', 1.3), 'att')).toBe('methodical');
    expect(id(sampleTicket('plains', 1.3), 'def')).toBe('methodical');
  });
});

describe('the dial (ruling 239): stops 1 to 3 lean, grown; 4 and 5 the situation', () => {
  it('stops 1 to 3: every listed nation gets its leaning', () => {
    for (const hist of [1, 2, 3, undefined]) {
      const t = (n: string): FieldTicket<LandPlace> => at({ att: n, def: n }, { hist, xp: 0.15 });
      expect(id(t('GER'), 'att')).toBe('bold');
      for (const n of ['UK', 'USA', 'ITA', 'FRA']) expect(id(t(n), 'att')).toBe('methodical');
      expect(id(t('SOV'), 'att')).toBe('mass');
      // Japan attacks by night and defends to the last, fought as bold until item 6.
      expect(id(t('JAP'), 'att')).toBe('bold');
      expect(id(t('JAP'), 'def')).toBe('bold');
      // Any other nation methodical, or a mass while its army is raw.
      expect(id(t('GRE'), 'att')).toBe('methodical');
      expect(id(at({ att: 'GRE' }, { hist, xp: 0.05 }), 'att')).toBe('mass');
    }
  });
  it('growth by experience or research, never by date (ruling 239(2), (8))', () => {
    const sov = (o: { xp?: number; doctrine?: string[] }): StyleId | undefined => id(at({ att: 'SOV' }, { hist: 3, ...o }), 'att');
    expect(sov({ xp: 0.2 })).toBe('mass');
    expect(sov({ xp: 0.25 })).toBe('methodical');
    expect(sov({ xp: 0.45 })).toBe('bold');
    expect(sov({ xp: 0.15, doctrine: ['combined'] })).toBe('methodical');
    expect(sov({ xp: 0.15, doctrine: ['combined', 'deep'] })).toBe('bold');
    // America grows from methodical to bold at the second step (the rider, 20:58Z).
    const usa = (o: { xp?: number; doctrine?: string[] }): StyleId | undefined => id(at({ att: 'USA' }, { hist: 2, ...o }), 'att');
    expect(usa({ xp: 0.3 })).toBe('methodical');
    expect(usa({ xp: 0.15, doctrine: ['combined'] })).toBe('methodical');
    expect(usa({ xp: 0.45 })).toBe('bold');
    expect(usa({ xp: 0.15, doctrine: ['combined', 'deep'] })).toBe('bold');
    expect(PATH.SOV).toEqual(['mass', 'methodical', 'bold']);
    // The date never moves it: the same army a year later fights the same way.
    const t = at({ att: 'SOV' }, { hist: 3, xp: 0.2 });
    expect(id({ ...t, turn: t.turn + 40, date: 'June 1943' }, 'att')).toBe('mass');
  });
  it('stops 4 and 5: swapping a side\'s nation never changes its style', () => {
    let n = 0;
    for (const hist of [4, 5]) for (const g of G7) for (const odds of [0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 1.8]) for (let s = 1; s <= 24; s++) {
      const base = sampleTicket(g, odds, { seed: s * 7919 + 13, hist });
      for (const side of ['att', 'def'] as const) {
        const want = styleFor(base, side)?.id;
        for (const nat of ['GER', 'SOV', 'USA', 'JAP', 'GRE']) { expect(styleFor({ ...base, [side]: { ...base[side], nat } }, side)?.id).toBe(want); n++; }
      }
    }
    expect(n).toBe(2 * 7 * 8 * 24 * 2 * 5);
  });
  it('stops 4 and 5: the situation picks from the ticket\'s facts', () => {
    const t = (terrain: (typeof G7)[number], odds: number, o: { fort?: number; river?: boolean; xp?: number } = {}): FieldTicket<LandPlace> => {
      const s = sampleTicket(terrain, odds, { hist: 5, fort: o.fort, river: o.river });
      return o.xp === undefined ? s : { ...s, att: { ...s.att, forces: s.att.forces.map((f) => ({ ...f, xp: o.xp! })) }, def: { ...s.def, forces: s.def.forces.map((f) => ({ ...f, xp: o.xp! })) } };
    };
    // The practice attack is 28 in 100 tanks: bold on open ground at telling odds, methodical in the forest, at short
    // odds, at works and over a river its tanks must cross; raw, a mass.
    expect(pickFor(t('plains', 1.4), 'att')).toBe('bold');
    expect(pickFor(t('desert', 1.6), 'att')).toBe('bold');
    expect(pickFor(t('forest', 1.6), 'att')).toBe('methodical');
    expect(pickFor(t('plains', 1.2), 'att')).toBe('methodical');
    expect(pickFor(t('plains', 1.6, { fort: 1 }), 'att')).toBe('methodical');
    expect(pickFor(t('plains', 1.6, { river: true }), 'att')).toBe('methodical');
    expect(pickFor(t('plains', 1.6, { xp: 0.05 }), 'att')).toBe('mass');
    // The defence: methodical against tanks on open ground, bold at close odds, else methodical; raw, a mass.
    expect(pickFor(t('plains', 1.1), 'def')).toBe('methodical');
    expect(pickFor(t('hills', 1.1), 'def')).toBe('bold');
    expect(pickFor(t('hills', 1.6), 'def')).toBe('methodical');
    expect(pickFor(t('hills', 1.1, { xp: 0.05 }), 'def')).toBe('mass');
  });
});

describe('the defence by its style (item 1b)', () => {
  const held = (s: StyleId): number => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { style: { def: s } }));
    return b.units.filter((u) => u.side === 'def' && u.reserve === 'held').length;
  };
  it('every style keeps the fitted quarter back, a mass too (ruling 239\'s rider, 21:57Z)', () => {
    const line = new LandBattle(sampleTicket('plains', 1.2)).units.filter((u) => u.side === 'def' && u.kind !== 'art').length;
    expect(held('methodical')).toBe(Math.max(1, Math.floor(line / 4)));
    expect(held('bold')).toBe(held('methodical'));
    expect(held('mass')).toBe(held('methodical'));
  });
  it('no leash leaves the held ground; a bold defence finishes a wavering attacker further out (ruling 239\'s rider)', () => {
    expect(STYLES.methodical.defLeash).toBe(300);
    for (const s of Object.values(STYLES)) expect(s.defLeash, s.id).toBeLessThanOrEqual(LAND.hold.within);
    expect(STYLES.bold.defFinish).toBeGreaterThan(STYLES.methodical.defFinish);
    expect(LAND.limit).toBe(360);
  });
});

describe('a bold defence\'s reserve follows the approach (ruling 239\'s rider, 03:20Z)', () => {
  /** The plain order against this defence style on plains: how far its held blocks have walked by first contact, and their worst while held. */
  const run = (ds: StyleId, seed: number) => {
    const t0 = sampleTicket('plains', 1.3, { seed, style: { def: ds } });
    const b = new LandBattle({ ...t0, att: { ...t0.att, human: true } });
    b.ai.def = true;
    const mine = b.units.filter((u) => u.side === 'att');
    const foe = b.units.filter((u) => u.side === 'def').reduce((m, f) => (f.y < m.y ? f : m));
    b.command({ do: 'attack', units: mine.map((u) => u.id), target: foe.id }, 'att');
    const held = b.units.filter((u) => u.side === 'def' && u.reserve === 'held');
    const str = held.map((u) => u.str);
    let walked: number | null = null, far = 0, hit = 0, phase = false;
    for (let i = 0; i < 100000 && !b.over; i++) {
      b.step();
      if (b.view().plan?.def?.phase === 'shift') phase = true;
      held.forEach((u, k) => {
        if (u.reserve !== 'held') return;
        far = Math.max(far, Math.hypot(u.x - u.home[0], u.y - u.home[1]));
        if (u.str < str[k] || u.engaged !== undefined) hit++;
      });
      if (walked === null && b.contactAt !== null) walked = Math.max(0, ...held.map((u) => Math.abs(u.x - u.home[0])));
    }
    const said = b.log.filter((e) => e.what === 'land:reserve' && e.text);
    // The player's own block reports what it sees (b464's fog rule; the UI director's words, 20 Nov 05:02).
    for (const e of said) {
      const u = b.units.find((x) => x.id === e.unit)!;
      expect(u.side).toBe('att');
      expect(e.text).toBe(`Your ${u.name} reports movement behind the enemy line, shifting toward it.`);
    }
    return { held: held.length, walked: walked ?? 0, far, hit, phase, told: said.length };
  };
  it('walks behind its line toward the attack before the line is struck, within its held ground, striking and struck by nothing', () => {
    const rs = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map((s) => run('bold', s * 7919 + 13));
    expect(rs.every((r) => r.held > 0)).toBe(true);
    expect(rs.filter((r) => r.walked >= 100).length).toBeGreaterThanOrEqual(6);
    for (const r of rs) { expect(r.far).toBeLessThanOrEqual(LAND.hold.within); expect(r.hit).toBe(0); expect(r.told).toBeLessThanOrEqual(1); }
    expect(rs.some((r) => r.phase && r.told === 1)).toBe(true);
  });
  it('a methodical or mass defence\'s reserve stands in the middle until its line is struck, as it always did', () => {
    for (const ds of ['methodical', 'mass'] as const) for (const s of [1, 2, 3]) {
      const r = run(ds, s * 7919 + 13);
      expect(r.far, `${ds} ${s}`).toBe(0);
      expect(r.phase || r.told > 0, `${ds} ${s}`).toBe(false);
    }
    expect(STYLES.methodical.defShift).toBe(0);
    expect(STYLES.mass.defShift).toBe(0);
  });
});

describe('tanks that leave their infantry pay on close ground (item 7; ruling 239(5))', () => {
  it('the figures are the card\'s own, never more (b418)', () => {
    const arms = (groundCard as unknown as { arms: { foot: string[]; alone: Record<string, number>; defence: { alone: Record<string, number> } } }).arms;
    const A = LAND.alone;
    expect(A.foot).toEqual(arms.foot);
    expect(A.take.att.woods).toBeCloseTo(1 / arms.alone.forest, 9);
    expect(A.take.att.streets).toBeCloseTo(1 / arms.alone.urban, 9);
    expect(A.take.def.woods).toBeCloseTo(1 / arms.defence.alone.forest, 9);
    expect(A.take.def.streets).toBeCloseTo(1 / arms.defence.alone.urban, 9);
    for (const s of ['att', 'def'] as const) for (const v of Object.values(A.take[s])) expect(v).toBeLessThanOrEqual(1 / Math.min(arms.alone.urban, arms.defence.alone.urban) + 1e-9);
    expect(A.cover.open).toBe((supportCard as unknown as { antiTank: { alone: { cover: number } } }).antiTank.alone.cover);
  });
  it('only the tank divisions the card counted as covered can be bitten; none where a side has no foot', () => {
    const t = sampleTicket('forest', 1.3);
    const b = new LandBattle(t);
    const tanks = b.units.filter((u) => u.side === 'att' && u.kind === 'arm');
    expect([...coveredTanks(t, b.units)].filter((id) => tanks.some((u) => u.id === id)).length).toBe(tanks.length);
    // Tanks alone: no foot at all, none covered.
    const bare = { ...t, att: { ...t.att, forces: t.att.forces.filter((f) => f.kind === 'arm').map((f) => ({ ...f, share: 1 })) } };
    const bb = new LandBattle(bare);
    expect(coveredTanks(bare, bb.units).size).toBe(0);
    // Four tank divisions beside one of foot in a forest: a quarter of them covered (ground.ts, one each).
    const heavy = { ...t, att: { ...t.att, forces: [{ ...t.att.forces[0], divs: 1, str: 1, share: 0.2 }, { ...t.att.forces[2], divs: 4, str: 1, share: 0.8 }] } };
    const hb = new LandBattle(heavy);
    const hs = hb.units.filter((u) => u.side === 'att' && u.kind === 'arm');
    expect(coveredTanks(heavy, hb.units).size).toBe(Math.floor(hs.length / 4));
  });
  it('a covered tank alone in the woods takes the card\'s figure from the foot it fights, and none beside its foot', () => {
    const t = sampleTicket('forest', 1.3, { seed: 11 });
    const fight = (alone: boolean, friend: boolean): number => {
      const b = new LandBattle(t);
      const tank = b.units.find((u) => u.side === 'att' && u.kind === 'arm')!;
      const foe = b.units.find((u) => u.side === 'def' && u.kind === 'inf')!;
      // The tank on a wood cell before the enemy foot, its own foot far off or right beside it.
      const f = b.field;
      let at: [number, number] | null = null;
      for (let y = foe.y - 150; y > foe.y - 400 && !at; y -= f.cell) for (let x = foe.x - 300; x <= foe.x + 300 && !at; x += f.cell) if (coverAt(f, x, y) === 'woods') at = [x, y];
      expect(at).not.toBeNull();
      tank.x = at![0]; tank.y = at![1];
      for (const u of b.units) if (u.side === 'att' && u !== tank) { u.x = friend ? tank.x + 160 : 100; u.y = friend ? tank.y : 100; }
      for (const u of b.units) u.engaged = undefined;
      const was = LAND.alone.on;
      LAND.alone.on = alone;
      try { return (b as unknown as { blow: (u: LandUnit, t: LandUnit) => { dmg: number } }).blow(foe, tank).dmg; } finally { LAND.alone.on = was; }
    };
    expect(fight(true, false) / fight(false, false)).toBeCloseTo(LAND.alone.take.att.woods, 6);
    expect(fight(true, true) / fight(false, true)).toBeCloseTo(1, 9);
  });
});

describe('the attack on its plan (item 1)', () => {
  it('holds nothing back: every block is in the main effort or pins the line (ruling 239\'s rider: the card prices the whole army)', () => {
    for (const st of ['bold', 'methodical'] as const) for (const g of G7) {
      const b = new LandBattle(sampleTicket(g, 1.3, { seed: 11, style: { att: st } }));
      b.ai.att = b.ai.def = true;
      for (let i = 0; i < 30 && !b.plan.att; i++) b.step();
      const P = b.plan.att!;
      for (const u of b.units.filter((f) => f.side === 'att' && f.kind !== 'art' && f.kind !== 'fort')) expect(['main', 'pin'], `${st} ${g} ${u.id}`).toContain(P.role[u.id]);
      expect(P.main.length, `${st} ${g}`).toBeGreaterThanOrEqual(1);
      expect(P.main.length, `${st} ${g}`).toBeLessThanOrEqual(STYLES[st].stretch);
    }
  });
});

describe('going in together (item 1; 7b)', () => {
  it('the player\'s group attack keeps its tanks within the lead of its slowest foot until they meet the enemy', () => {
    const was = LAND.together.player;
    LAND.together.player = true;
    try {
      const b = new LandBattle(sampleTicket('plains', 1.3, { seed: 5, humanAtt: true }));
      b.ai.def = true;
      const mine = b.units.filter((u) => u.side === 'att');
      const foe = b.units.filter((u) => u.side === 'def').reduce((m, f) => (f.y < m.y ? f : m));
      expect(b.command({ do: 'attack', units: mine.map((u) => u.id), target: foe.id }, 'att')).toBe(true);
      let worst = -Infinity;
      for (let i = 0; i < 3000 && !b.over && b.contactAt === null; i++) {
        b.step();
        const foot = mine.filter((u) => u.kind === 'inf' && u.state !== 'gone'), slow = Math.min(...foot.map((u) => u.y));
        for (const u of mine) if (u.kind === 'arm' && !u.met) worst = Math.max(worst, u.y - slow);
      }
      expect(worst).toBeLessThanOrEqual(STYLE.lead + 12);
    } finally { LAND.together.player = was; }
  });
});
