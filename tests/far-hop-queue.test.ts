// The hop staff's queue with the far side's switch on (rider b599 to ruling 234, (ii)): an aim stuck over the risk
// limit gives its place to the next and comes back after, and a shore beside a nation's own army is the army's only
// if that army can attack it this month; a corps brought up for a shore keeps it as its aim (the Wake ruling's (b)).
// States are set by hand at stop 3 (no book) in a made-up 1942; the crossing's
// risk is held over the staff's limit by hand where the queue is read, so no landing goes in.
import { vi } from 'vitest';
const risky = vi.hoisted(() => { (globalThis as any).TC_FAR = 1; return { on: false }; });
vi.mock('../src/campaign/navy', async (importOriginal) => {
  const m = await importOriginal<typeof import('../src/campaign/navy')>();
  return { ...m, planLanding: (g: Parameters<typeof m.planLanding>[0], cs: Parameters<typeof m.planLanding>[1], to: Parameters<typeof m.planLanding>[2]) => {
    const p = m.planLanding(g, cs, to);
    return risky.on ? { ...p, why: undefined, risk: 0.5 } : p;
  } };
});

import { describe, expect, it } from 'vitest';
import { armysShore, hop } from '../src/campaign/ai';
import { addCorps, assess, atWar, createGame, declareWar, limitAttackers, relocate } from '../src/campaign/game';
import { hopAims, hopShores } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

/** Japan at war with America, Britain, the Dutch and the dominions in a month of 1942, with no book (stop 3). */
const pacificWar = (turn: number): Game => {
  const g = startOpen(createGame(3, null));
  g.turn = turn;
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  g.turn = turn;
  return g;
};
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
/** A corps put down by hand where it is wanted, whole, supplied, in order and with all its steps. */
const ready = (g: Game, c: Corps, at: Region, str = c.full ?? 1): Corps => {
  relocate(g, c, at);
  c.hold = false; c.supplied = true; c.org = 1; c.ap = c.apMax; c.str = str;
  return c;
};
const aimed = (g: Game): string[] => hopAims(g, 'JAP').map((a) => a.to.name);

describe("the hop staff's queue (rider b599 to ruling 234, (ii))", () => {
  it('gives a shore stuck over the risk limit three months running a rest of three months, the next shore taking its force, and then takes it up again', () => {
    const g = pacificWar(30);
    expect(hopShores(g, 'JAP')!.book).toBe(false);
    // Every other Japanese corps is held: one army at Truk is the whole force the ships have.
    for (const c of g.corps) if (c.nat === 'JAP') c.hold = true;
    ready(g, corps(g, '21st Army (Canton)'), g.byName['Truk']);
    risky.on = true;
    try {
      const months: string[][] = [], off: (number | undefined)[] = [], stuck: (number | undefined)[] = [];
      let X: Region | null = null;
      for (let t = 30; t <= 37; t++) {
        g.turn = t;
        hop(g, 'JAP', null);
        months.push(aimed(g));
        X ??= g.byName[months[0][0]];
        off.push(g.flags[`st:hopOff:JAP:${X.id}`]);
        stuck.push(g.flags[`st:hopStuck:JAP:${X.id}`]);
      }
      // The army is named for one shore, the same three months running, while its crossing stays too risky.
      expect(months[0].length).toBe(1);
      const x = X!.name;
      expect(months.slice(0, 3)).toEqual([[x], [x], [x]]);
      expect(stuck.slice(0, 3)).toEqual([1, 2, 3]);
      // The fourth month that shore gives its place up: the army is named for the next shore instead.
      expect(months[3].length).toBe(1);
      expect(months[3]).not.toContain(x);
      expect(off[3]).toBe(33);
      const y = months[3][0];
      // It stays aside three months, and the next shore keeps the army meanwhile.
      expect(months.slice(4, 6)).toEqual([[y], [y]]);
      expect(off.slice(4, 6)).toEqual([33, 33]);
      // Then it comes back first in the queue, its count begun again.
      expect(months.slice(6, 8)).toEqual([[x], [x]]);
      expect(off.slice(6, 8)).toEqual([undefined, undefined]);
      expect(stuck.slice(6, 8)).toEqual([1, 2]);
    } finally { risky.on = false; }
  });

  it('counts only months running, and only a crossing over the risk limit: a month the shore is not named begins the count again', () => {
    const g = pacificWar(30);
    for (const c of g.corps) if (c.nat === 'JAP') c.hold = true;
    const a = ready(g, corps(g, '21st Army (Canton)'), g.byName['Truk']);
    risky.on = true;
    try {
      g.turn = 30; hop(g, 'JAP', null);
      const x = aimed(g)[0], X = g.byName[x];
      g.turn = 31; hop(g, 'JAP', null);
      expect(g.flags[`st:hopStuck:JAP:${X.id}`]).toBe(2);
      // A month with the army held: nobody is named, so the run is broken.
      a.hold = true;
      g.turn = 32; hop(g, 'JAP', null);
      expect(aimed(g)).toEqual([]);
      a.hold = false;
      g.turn = 33; hop(g, 'JAP', null);
      expect(aimed(g)).toEqual([x]);
      expect(g.flags[`st:hopStuck:JAP:${X.id}`]).toBe(1);
      expect(g.flags[`st:hopOff:JAP:${X.id}`]).toBeUndefined();
    } finally { risky.on = false; }
    // With the crossing within the limit the count is dropped.
    g.turn = 34; hop(g, 'JAP', null);
    expect(g.flags[`st:hopStuck:JAP:${g.byName['Truk'].id}`]).toBeUndefined();
  });

  it("leaves a shore beside its own army to the ships unless corps beside it can attack it this month", () => {
    const g = pacificWar(30);
    const rabaul = g.byName['Rabaul'], cape = g.byName['Cape Gloucester'];
    expect(rabaul.nb).toEqual([cape.id]);
    cape.ctrl = 'JAP';
    const aus = g.corps.find((c) => c.nat === 'AUS' && !c.dead && c.kind === 'inf')!;
    ready(g, aus, rabaul);
    const jap = ready(g, corps(g, '13th Army (22nd Division, Hangchow)'), cape, 0.2);
    // Too weak to go in against the Australians: the ships' shore (before b599 it was the army's for standing there).
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
    // Whole, against a broken garrison: the army's.
    jap.str = jap.full ?? 1;
    aus.str = 0.1;
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(true);
    // The phase may ask more of its attacks.
    expect(armysShore(g, 'JAP', rabaul, { minOdds: 99 } as never)).toBe(false);
    // On hold or out of order, it cannot go in this month: the ships' again.
    jap.hold = true;
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
    jap.hold = false; jap.org = 0.2;
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
    jap.org = 1;
    // An empty shore with a corps beside that can walk in is the army's.
    relocate(g, aus, g.byName['Port Moresby']);
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(true);
    // Nobody of its own beside it: the ships'.
    relocate(g, jap, g.byName['Truk']);
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
  });

  it("reads a corps' steps as its month gives them: one that has spent them this month beside the shore still makes it the army's", () => {
    const g = pacificWar(30);
    const rabaul = g.byName['Rabaul'], cape = g.byName['Cape Gloucester'];
    cape.ctrl = 'JAP';
    const aus = ready(g, g.corps.find((c) => c.nat === 'AUS' && !c.dead && c.kind === 'inf')!, rabaul);
    aus.str = 0.1;
    const jap = ready(g, corps(g, '13th Army (22nd Division, Hangchow)'), cape);
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(true);
    // The month's dated attack on it has been made and thrown back, or the corps has just landed or walked in beside it:
    // its steps are spent, and the shore is the army's still (the hops are planned after the month's attacks).
    jap.ap = 0;
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(true);
    // Broken by it, the corps is out of the fight: the ships'.
    jap.org = 0.2;
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
  });

  it("counts the guns beside in the odds, as the army's own attacks bring them, but never guns alone", () => {
    const g = pacificWar(30);
    const rabaul = g.byName['Rabaul'], cape = g.byName['Cape Gloucester'];
    cape.ctrl = 'JAP';
    const aus = ready(g, g.corps.find((c) => c.nat === 'AUS' && !c.dead && c.kind === 'inf')!, rabaul, 0.3);
    const jap = ready(g, corps(g, '13th Army (22nd Division, Hangchow)'), cape, 0.6);
    const guns = addCorps(g, 'JAP', 'Test guns', 'art', 1, cape.id, {});
    // The infantry alone is short of the staff's 1.3; with the guns it is over.
    expect(assess(g, limitAttackers(g, [jap], rabaul), rabaul).odds).toBeLessThan(1.3);
    expect(assess(g, limitAttackers(g, [jap, guns], rabaul), rabaul).odds).toBeGreaterThanOrEqual(1.3);
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(true);
    guns.hold = true;
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
    guns.hold = false;
    // Guns with no infantry beside them do not take a shore, defended or empty.
    relocate(g, jap, g.byName['Truk']);
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
    relocate(g, aus, g.byName['Port Moresby']);
    expect(armysShore(g, 'JAP', rabaul, null)).toBe(false);
  });
});

describe('a corps brought up for a shore keeps it as its aim (rider b599 to ruling 234, the Wake ruling\'s (b))', () => {
  /** One army at Palau, every other Japanese corps held; with the crossing too risky nothing lands and the aims stand. */
  const palau = (): { g: Game; a: Corps } => {
    const g = pacificWar(30);
    for (const c of g.corps) if (c.nat === 'JAP') c.hold = true;
    return { g, a: ready(g, corps(g, '21st Army (Canton)'), g.byName['Palau']) };
  };
  /** The shore the army is named for first, and the one it is named for with that shore set aside. */
  const firstTwo = (): [Region, Region] => {
    const { g } = palau();
    risky.on = true;
    try {
      hop(g, 'JAP', null);
      const x = g.byName[aimed(g)[0]];
      g.flags[`st:hopOff:JAP:${x.id}`] = 30;
      hop(g, 'JAP', null);
      return [x, g.byName[aimed(g)[0]]];
    } finally { risky.on = false; }
  };
  /** The army as the ships left it at Palau a month ago for a shore: waiting there, held, with the shore it came for. */
  const broughtFor = (g: Game, a: Corps, r: Region, at = 29): void => {
    a.hold = true;
    g.flags[`st:hopUp:${a.id}`] = at;
    g.flags[`st:hopFor:${a.id}`] = r.id + 1;
  };

  it('three months: no other shore takes it, though the staff would name another first; then the army has it back', () => {
    const [x, y] = firstTwo();
    expect(x.id).not.toBe(y.id);
    const { g, a } = palau();
    broughtFor(g, a, y);
    risky.on = true;
    try {
      for (const t of [30, 31, 32]) {
        g.turn = t;
        hop(g, 'JAP', null);
        expect(aimed(g), `month ${t}`).toEqual([y.name]);
        expect(g.flags[`st:hopFor:${a.id}`]).toBe(y.id + 1);
        expect(a.hold).toBe(true);
      }
      // The quarter is up: it is off hold, and free for the shore the staff names first.
      g.turn = 33;
      hop(g, 'JAP', null);
      expect(g.flags[`st:hopUp:${a.id}`]).toBeUndefined();
      expect(g.flags[`st:hopFor:${a.id}`]).toBeUndefined();
      expect(a.hold).toBe(false);
      expect(aimed(g)).toEqual([x.name]);
    } finally { risky.on = false; }
  });

  it('unless the shore is struck off the queue first: set aside, or taken; it is then free for any shore while it waits', () => {
    const [x, y] = firstTwo();
    const { g, a } = palau();
    broughtFor(g, a, y);
    g.flags[`st:hopOff:JAP:${y.id}`] = 30;
    risky.on = true;
    try {
      hop(g, 'JAP', null);
      expect(g.flags[`st:hopFor:${a.id}`]).toBeUndefined();
      expect(g.flags[`st:hopUp:${a.id}`]).toBe(29);
      expect(aimed(g)).toEqual([x.name]);
      const { g: h, a: b } = palau();
      broughtFor(h, b, y);
      h.byName[y.name].ctrl = 'JAP';
      hop(h, 'JAP', null);
      expect(h.flags[`st:hopFor:${b.id}`]).toBeUndefined();
      expect(aimed(h)).toEqual([x.name]);
    } finally { risky.on = false; }
  });

  it('lands on the shore it came for when the crossing will do, and forgets it', () => {
    const [x, y] = firstTwo();
    const { g, a } = palau();
    broughtFor(g, a, y);
    hop(g, 'JAP', null);
    expect(a.reg).toBe(y.id);
    expect(a.reg).not.toBe(x.id);
    expect(g.flags[`st:hopUp:${a.id}`]).toBeUndefined();
    expect(g.flags[`st:hopFor:${a.id}`]).toBeUndefined();
    expect(a.hold).toBe(false);
  });

  it("is not struck off by a month the shore is the army's: the corps waits on for it, and is named for it once it is the ships' again", () => {
    const [x] = firstTwo();
    expect(x.name).toBe('Rabaul');
    const { g, a } = palau();
    broughtFor(g, a, x);
    const cape = g.byName['Cape Gloucester'];
    cape.ctrl = 'JAP';
    const jap = ready(g, corps(g, '13th Army (22nd Division, Hangchow)'), cape);
    for (const c of x.units) if (!c.dead && c.nat !== 'JAP') c.str = 0.05;
    expect(armysShore(g, 'JAP', x, null)).toBe(true);
    risky.on = true;
    try {
      hop(g, 'JAP', null);
      expect(g.flags[`st:hopFor:${a.id}`]).toBe(x.id + 1);
      expect(aimed(g)).toEqual([]);
      jap.hold = true;
      g.turn = 31;
      hop(g, 'JAP', null);
      expect(aimed(g)).toEqual([x.name]);
      expect(g.flags[`st:hopFor:${a.id}`]).toBe(x.id + 1);
    } finally { risky.on = false; }
  });

  it('is freed the month another force takes its shore, in time for the shores after it', () => {
    const { g, a } = palau();
    // The corps waits at Kwajalein for Guam, made the staff's first shore. Another army one sea from Guam, at Saipan,
    // goes in there this month.
    const guam = g.byName['Guam'], kwajalein = g.byName['Kwajalein'];
    guam.vp = 99;
    relocate(g, a, kwajalein);
    broughtFor(g, a, guam);
    const b = ready(g, corps(g, '13th Army (22nd Division, Hangchow)'), g.byName['Saipan']);
    hop(g, 'JAP', null);
    expect(b.reg).toBe(guam.id);
    expect(guam.ctrl).toBe('JAP');
    expect(g.flags[`st:hopFor:${a.id}`]).toBeUndefined();
    // Free in the same month: named for a shore of its own from Kwajalein.
    expect(hopAims(g, 'JAP').some((x) => x.from.id === kwajalein.id)).toBe(true);
  });
});
