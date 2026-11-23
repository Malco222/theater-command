// A computer's fighter defence and its style (the tactics panel's air brief, item 5; the design director's ruling 239,
// its riders of 20:58Z and 21:06Z, and the pool of 21:10Z). A style is a temperament, not a weapon: it moves where the
// defence's weight falls, never how much (217, b418). Bold, methodical and mass; mass is today's battle event for event.
// A split defence sends whole groups at the raid's escort and the rest at its bombers, and fights from one pool: what
// the whole defence has left, drawn at the whole defence's rate by whichever of its groups are in contact, each by its
// weight. A warned defence climbs above the raid first; the climb is drawn, never priced. Nothing reads the date: a
// nation leans by the one lean table at stops 1 to 3 and grows by its leaders and its research, and at stops 4 and 5
// the style comes from the ticket's facts alone (Brian, 19 November: a sandbox to make your own history).
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { runOut } from '../src/battle/core/clock';
import type { FieldTicket, ForceTicket } from '../src/battle/core/types';
import { SKY } from '../src/battle/air/book';
import { airBattle, type AirSim } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';
import type { AirPlace } from '../src/battle/air/types';
import { airStyles, defenceOf, experience, holds, leanOf, pickOf, rolesOf, STYLES, styleOf, type DefenceFacts, type DefenceStyle } from '../src/battle/air/styles';

const facts = (o: Partial<DefenceFacts> = {}): DefenceFacts => ({ xp: 0.5, warning: false, gci: false, escort: 10, defence: 20, ...o });
const force = (nat: string, kind: 'ftr' | 'bmr', men = 30, xp = 0.33): ForceTicket => ({ corps: 1, name: `${nat} ${kind}`, nat, kind, divs: 1, men, str: 1, org: 1, xp, share: 1 });

describe("a computer defence's style: the dial (ruling 239, A5)", () => {
  it("leans at stops 1 to 3 by the air column of the one lean table (239.1 as amended 20:58Z)", () => {
    for (const nat of ['GER', 'UK', 'USA', 'ITA', 'FRA', 'CAN', 'AUS', 'NZL', 'SAF', 'IND']) expect(leanOf(nat, facts()), nat).toBe('methodical');
    expect(leanOf('JAP', facts())).toBe('bold');
    expect(leanOf('JPN', facts())).toBe('bold');
    expect(leanOf('SOV', facts({ xp: 0.33 }))).toBe('mass');
    for (const stop of [1, 2, 3]) expect(styleOf(stop, 'JAP', facts())).toBe('bold');
  });

  it('grows the Soviet lean by its leaders and its research, never by date', () => {
    expect(leanOf('SOV', facts({ xp: 0.33 }))).toBe('mass');
    expect(leanOf('SOV', facts({ xp: 0.6 }))).toBe('methodical');
    expect(leanOf('SOV', facts({ xp: 0.33, warning: true }))).toBe('methodical');
    expect(leanOf('SOV', facts({ xp: 0.9, warning: true }))).toBe('methodical');
    expect(leanOf('SOV', facts({ xp: 0.6, warning: true, gci: true }))).toBe('methodical');
    expect(leanOf('SOV', facts({ xp: 0.9, warning: true, gci: true }))).toBe('bold');
  });

  it('flies a nation not in the table methodical, or mass while raw', () => {
    expect(leanOf('SWE', facts({ xp: 0 }))).toBe('mass');
    expect(leanOf('SWE', facts({ xp: 0, warning: true }))).toBe('methodical');
    expect(leanOf('SWE', facts({ xp: 0.33 }))).toBe('methodical');
  });

  it('picks at stops 4 and 5 from the ticket alone: the same facts fly the same style for every nation', () => {
    const cases: [DefenceFacts, DefenceStyle][] = [
      [facts({ xp: 0 }), 'mass'],
      [facts({ xp: 0, warning: true }), 'methodical'],
      [facts({ escort: 20, defence: 20 }), 'bold'],
      [facts({ escort: 19, defence: 20 }), 'methodical'],
    ];
    for (const [f, want] of cases) {
      expect(pickOf(f)).toBe(want);
      for (const stop of [4, 5]) for (const nat of ['GER', 'JAP', 'SOV', 'UK', 'SWE']) expect(styleOf(stop, nat, f), `${nat} at stop ${stop}`).toBe(want);
    }
  });

  it('mass neither splits nor climbs; bold and methodical do both', () => {
    expect(defenceOf('mass')).toEqual({ style: 'mass', escort: 0, climb: 0 });
    expect(defenceOf('methodical')).toEqual({ style: 'methodical', escort: 0.35, climb: 1 });
    expect(defenceOf('bold')).toEqual({ style: 'bold', escort: 0.5, climb: 1 });
  });

  it("reads research as the grand campaign holds it, and none outside it", () => {
    const g = startOpen(createGame(7, 'GER'));
    expect(holds(g, 'UK', STYLES.grow.first.tech)).toBe(true);
    expect(holds(g, 'UK', STYLES.grow.second.tech)).toBe(false);
    expect(holds(g, 'XXX', STYLES.grow.first.tech)).toBe(false);
    expect(holds(createGame(7, 'GER'), 'UK', STYLES.grow.first.tech)).toBe(false);
  });

  it("weighs leaders' experience by aircraft", () => {
    expect(experience([force('GER', 'ftr', 30, 1 / 3), force('GER', 'ftr', 10, 1)])).toBeCloseTo((30 / 3 + 10) / 40, 12);
    expect(experience([])).toBe(0);
  });

  it("gives a human defender no style, and the escort's roles only to a computer raid at stops 4 and 5", () => {
    const g = startOpen(createGame(7, 'GER'));
    const ger = { nat: 'GER', forces: [force('GER', 'ftr'), force('GER', 'bmr')] };
    const fra = { nat: 'FRA', forces: [force('FRA', 'ftr'), force('FRA', 'bmr')] };
    const light = { escort: 10, defence: 20 }, heavy = { escort: 20, defence: 20 };
    // Stop 3: the computer's defence leans; nobody's escort is touched.
    expect(airStyles(g, ger, fra, light)).toEqual({ defence: defenceOf('methodical') });
    expect(airStyles(g, fra, ger, light)).toEqual({});
    // Stop 5: the computer's raid orders its roles by its weight; the human's raid and the human's defence keep today's.
    g.flags.hist = 5;
    expect(airStyles(g, fra, ger, light)).toEqual({ roles: { FRA: ['close', 'top', 'sweep'] } });
    expect(airStyles(g, fra, ger, heavy)).toEqual({ roles: { FRA: ['sweep', 'top', 'close'] } });
    expect(airStyles(g, ger, fra, heavy)).toEqual({ defence: defenceOf('bold') });
    // An unescorted computer raid has no escort to order.
    expect(airStyles(g, { nat: 'FRA', forces: [force('FRA', 'bmr')] }, ger, heavy)).toEqual({});
  });

  it("orders the escort's roles by its weight against the defence, never by date", () => {
    expect(rolesOf(20, 20)).toEqual(['sweep', 'top', 'close']);
    expect(rolesOf(19, 20)).toEqual(['close', 'top', 'sweep']);
  });
});

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

/** The staff's raid on a named province with every fighter group that can go by day, at a turn, against more foes. */
function raid(nat: string, kind: RaidKind, name: string, turn = 0, foes: string[] = []) {
  const g: Game = startOpen(createGame(7, nat));
  for (const w of [...WARS[nat], ...foes]) setWar(g, nat, w);
  if (turn) g.turn = turn;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  const { ticket, why } = airTicket(g, kind, t.id, ids, nat);
  if (!ticket) throw new Error(why ?? 'no ticket');
  return ticket;
}

type Ticket = FieldTicket<AirPlace>;
const as = (tk: Ticket, style: DefenceStyle): Ticket => ({ ...tk, place: { ...tk.place, defence: defenceOf(style) } });
const plain = (tk: Ticket): Ticket => { const { defence: _, ...place } = tk.place; return { ...tk, place }; };

function fight(tk: Ticket, k: number, each?: (s: AirSim) => void): AirSim {
  const s = airBattle({ ...tk, seed: tk.seed + k });
  s.ai.att = true; s.ai.def = true;
  if (each) while (!s.over) { s.step(); each(s); } else runOut(s);
  return s;
}

function average(tk: Ticket, runs: number) {
  let foe = 0, ftr = 0, bmr = 0;
  for (let k = 0; k < runs; k++) for (const gr of fight(tk, k).outcome().groups) if (gr.side === 'def') foe += gr.lostFtr; else { ftr += gr.lostFtr; bmr += gr.lostBmr; }
  return { foe: foe / runs, ftr: ftr / runs, bmr: bmr / runs };
}

function holdsCard(tk: Ticket, runs: number, what: string): void {
  const f = tk.place.forecast, got = average(tk, runs);
  for (const [k, want] of [['foe', f.foe], ['ftr', f.ftr], ['bmr', f.bmr]] as const) {
    if (want < 3) continue;
    expect(Math.abs(got[k] - want), `${what} ${k}: ${got[k].toFixed(1)} against ${want.toFixed(1)}`).toBeLessThan(Math.max(0.1 * want, 1));
  }
}

/** The defending fighter groups, and the escort role's share of their weight (aircraft x worth). */
const defenders = (s: AirSim) => s.units.filter((u) => u.side === 'def' && u.air === 'ftr');

describe('a split defence holds the card (the 21:06Z rider: escorted raids with two defending groups, 48 battles)', () => {
  const SPLIT: [string, RaidKind, string, number][] = [['FRA', 'bomb', 'Rhineland', 0], ['FRA', 'bomb', 'Rhineland', 30], ['UK', 'bomb', 'Ruhr', 30]];
  for (const [nat, kind, name, turn] of SPLIT) for (const style of ['bold', 'methodical'] as const) {
    it(`${nat} on ${name} at turn ${turn}, the defence ${style}`, () => {
      holdsCard(as(raid(nat, kind, name, turn), style), 48, `${name}@${turn} ${style}`);
    }, 120_000);
  }

  it('an invented war holds it too: France bombs Kent, its two defending groups and two escorts, warned', () => {
    const tk = raid('FRA', 'bomb', 'Kent', 0, ['UK']);
    expect(tk.place.radar).toBe(true);
    // Ruling 225: Kent is defended by the two groups of its own command, enough for a split.
    expect(tk.def.forces.length).toBe(2);
    expect(tk.att.forces.filter((f) => f.kind === 'ftr').length).toBe(2);
    expect(tk.place.defence?.style).toBe('methodical');
    holdsCard(tk, 24, 'FRA on Kent');
  }, 120_000);
});

describe('what a style changes on the field, and what it never does', () => {
  it('A1: mass is today\'s battle, event for event', () => {
    for (const [nat, name, turn] of [['GER', 'Alsace', 0], ['FRA', 'Rhineland', 30], ['UK', 'Ruhr', 30], ['GER', 'Kent', 0]] as const) {
      const tk = raid(nat, 'bomb', name, turn);
      for (let k = 0; k < 3; k++) expect(fight(as(tk, 'mass'), k).history(), `${name} battle ${k}`).toEqual(fight(plain(tk), k).history());
    }
  }, 120_000);

  it('A2: the escort role takes the whole groups whose weight comes nearest the style\'s share, each role at least one', () => {
    for (const [nat, name, turn] of [['FRA', 'Rhineland', 0], ['FRA', 'Rhineland', 30], ['UK', 'Ruhr', 30]] as const) for (const style of ['bold', 'methodical'] as const) {
      const s = airBattle(as(raid(nat, 'bomb', name, turn), style));
      const def = defenders(s), w = def.map((u) => u.n0 * u.q), all = w.reduce((t, x) => t + x, 0);
      const share = def.reduce((t, u, i) => t + (u.def === 'escort' ? w[i] : 0), 0) / all;
      let best = Infinity;
      for (let m = 1; m < (1 << def.length) - 1; m++) best = Math.min(best, Math.abs(def.reduce((t, _, i) => t + (m & (1 << i) ? w[i] : 0), 0) / all - defenceOf(style).escort));
      expect(Math.abs(share - defenceOf(style).escort), `${name}@${turn} ${style}`).toBeCloseTo(best, 12);
      expect(def.some((u) => u.def === 'escort') && def.some((u) => u.def === 'bombers')).toBe(true);
    }
  });

  it('A2: the escort-takers fight the escort, on a raid fighter within 30 seconds of first contact (239, 21:24Z)', () => {
    // Which group they touch first is the formation's geometry, not the role: a close escort flies 1.5 km beside its box
    // and contact opens at 3 km, so escort-takers coming from the box's side meet the box a few seconds before its
    // escort. The at-tick figure is given beside.
    for (const [nat, name, turn] of [['FRA', 'Rhineland', 0], ['FRA', 'Rhineland', 30], ['UK', 'Ruhr', 30]] as const) for (const style of ['bold', 'methodical'] as const) {
      let n = 0, soon = 0, atTick = 0;
      // And what they go for, not only what they touch (the fourth review: the escort flies beside its box, so any
      // group that reaches the box touches it): the share of their ticks in a fight spent chasing a raid fighter.
      const chase = { escort: [0, 0], bombers: [0, 0] };
      const tk = as(raid(nat, 'bomb', name, turn), style);
      for (let k = 0; k < 12; k++) {
        const first = new Map<number, number>(), on = new Set<number>();
        fight(tk, k, (s) => {
          for (const u of defenders(s).filter((x) => x.def === 'escort')) {
            if (!u.contacts.length && !first.has(u.id)) continue;
            const ftr = u.contacts.some((i) => s.unit(i)?.air === 'ftr');
            if (!first.has(u.id)) { first.set(u.id, s.t); n++; if (ftr) atTick++; }
            if (ftr && !on.has(u.id) && s.t - first.get(u.id)! <= 30) { on.add(u.id); soon++; }
          }
          for (const u of defenders(s)) {
            if (!u.def || !u.contacts.length) continue;
            chase[u.def][1]++;
            if (u.job.do === 'intercept' && s.unit(u.job.on)?.air === 'ftr') chase[u.def][0]++;
          }
        });
      }
      expect(soon / n, `${name}@${turn} ${style}: ${soon} of ${n} within 30 s, ${atTick} at the first tick`).toBeGreaterThanOrEqual(0.8);
      const [e, b] = [chase.escort[0] / chase.escort[1], chase.bombers[0] / chase.bombers[1]];
      expect(e, `${name}@${turn} ${style}: escort-takers chase a fighter ${e.toFixed(2)} of their fighting ticks, bomber-killers ${b.toFixed(2)}`).toBeGreaterThanOrEqual(0.5);
      expect(b).toBeLessThan(e);
    }
  }, 120_000);

  it('A4: a style deals the same kits as today, and a split defence keeps every group at its weight\'s share of the pool', () => {
    for (const [nat, name, turn] of [['FRA', 'Rhineland', 0], ['UK', 'Ruhr', 30], ['GER', 'Kent', 0]] as const) {
      const tk = raid(nat, 'bomb', name, turn);
      const kits = (s: AirSim) => defenders(s).map((u) => u.kit0);
      const today = kits(airBattle(plain(tk)));
      for (const style of ['bold', 'methodical', 'mass'] as const) {
        const got = kits(airBattle(as(tk, style)));
        expect(got.reduce((t, x) => t + x, 0)).toBeCloseTo(today.reduce((t, x) => t + x, 0), 9);
        got.forEach((x, i) => expect(x).toBeCloseTo(today[i], 9));
      }
    }
    // Through a split battle, what each group holds is its weight's share of what the defence still in the fight has left.
    const s = fight(as(raid('FRA', 'bomb', 'Rhineland', 30), 'methodical'), 0, (sim) => {
      const def = defenders(sim).filter((u) => u.state !== 'gone' && u.state !== 'routing' && u.job.do !== 'home'), D = def.reduce((t, u) => t + u.n0 * u.q, 0);
      const left = def.reduce((t, u) => t + u.kit.ftr + u.kit.bmr, 0), left0 = def.reduce((t, u) => t + u.kit0, 0);
      for (const u of def) expect(u.kit.ftr + u.kit.bmr).toBeCloseTo((left * u.n0 * u.q) / D, 9);
      for (const u of def) if (u.job.do !== 'wait') expect(u.vig).toBeCloseTo(left / left0, 9);
    });
    expect(s.over).not.toBeNull();
  }, 120_000);

  it('a group that breaks off from a split defence takes its share of the pool with it, as any group takes its fight', () => {
    // The review's case (21:40Z): the staff's reckoning (expect) counts only groups still in the fight, so the board must
    // not hand an out group's share to the rest.
    const tk = as(raid('FRA', 'bomb', 'Rhineland', 30), 'methodical');
    for (let k = 0; k < 3; k++) {
      let out: { id: number; kit: number; t: number } | null = null;
      const s = fight(tk, k, (sim) => {
        const def = defenders(sim);
        if (!out && def.some((u) => u.contacts.length)) {
          const u = def.find((x) => x.def === 'bombers')!;
          u.job = { do: 'home' };
          out = { id: u.id, kit: u.kit.ftr + u.kit.bmr, t: sim.t };
        } else if (out) {
          const u = sim.unit(out.id)!;
          expect(u.kit.ftr + u.kit.bmr, `battle ${k} at ${sim.t}`).toBe(out.kit);
          const rest = def.filter((x) => x.id !== out!.id && x.state !== 'gone' && x.state !== 'routing' && x.job.do !== 'home');
          const W = rest.reduce((t, x) => t + x.n0 * x.q, 0), left = rest.reduce((t, x) => t + x.kit.ftr + x.kit.bmr, 0);
          for (const x of rest) expect(x.kit.ftr + x.kit.bmr).toBeCloseTo((left * x.n0 * x.q) / W, 9);
        }
      });
      expect(out).not.toBeNull();
      expect(s.over).not.toBeNull();
    }
  }, 120_000);

  it('climbers follow their box down: a raid that dives a band after the scramble is still met (the review, 21:40Z)', () => {
    // Germany bombs Kent; the computer's methodical defence climbs to the top band. Once it is up, the player takes his
    // boxes down to the lowest band: the climbers must come down within a band of them, or they could never meet them.
    const kent = as(raid('GER', 'bomb', 'Kent'), 'methodical');
    for (let k = 0; k < 3; k++) {
      const s = airBattle({ ...kent, seed: kent.seed + k });
      s.ai.att = false; s.ai.def = true;
      let ordered: number | null = null, met: number | null = null;
      while (!s.over) {
        s.step();
        const boxes = s.units.filter((u) => u.side === 'att' && u.air === 'bmr');
        if (ordered === null && s.t >= 115) {
          expect(defenders(s).some((u) => u.rise && u.band === 2)).toBe(true);
          expect(s.command({ do: 'air:band', band: 0, units: boxes.map((u) => u.id) }, 'att')).toBe(true);
          ordered = s.t;
        }
        if (ordered !== null && met === null && defenders(s).some((u) => u.contacts.some((i) => boxes.some((b) => b.id === i)))) met = s.t;
      }
      expect(met, `battle ${k}: the climbers never met the boxes`).not.toBeNull();
      expect(met! - ordered!, `battle ${k}`).toBeLessThan(150);
    }
  }, 120_000);

  it('A3: a warned defence climbs above the raid once and dives on the boxes; mass and an unwarned defence never climb', () => {
    const kent = raid('GER', 'bomb', 'Kent');
    expect(kent.place.radar && !kent.place.night).toBe(true);
    let met = 0, above = 0;
    for (let k = 0; k < 12; k++) {
      const seen = new Set<number>();
      const s = fight(as(kent, 'methodical'), k, (sim) => {
        const log = sim.history();
        for (const u of defenders(sim)) {
          if (seen.has(u.id) || !u.contacts.some((i) => sim.unit(i)?.air === 'bmr')) continue;
          seen.add(u.id); met++;
          if (log.some((e) => e.what === 'air:bounce' && e.unit === u.id && e.t === sim.t)) above++;
        }
      });
      const climbs = s.history().filter((e) => e.what === 'air:climb');
      for (const u of defenders(s)) expect(climbs.filter((e) => e.unit === u.id).length, `group ${u.id}`).toBeLessThanOrEqual(1);
      expect(climbs.length).toBeGreaterThan(0);
    }
    expect(above / met, `above the box at first contact ${above} of ${met}`).toBeGreaterThanOrEqual(0.9);
    for (const tk of [as(kent, 'mass'), as(raid('GER', 'bomb', 'Alsace'), 'methodical')]) {
      const h = fight(tk, 0).history();
      expect(h.filter((e) => e.what === 'air:climb')).toEqual([]);
    }
  }, 120_000);

  it("the climb ends with its dive, or once the climbers are back at every staff's height: the player's height orders keep today's price (239.15)", () => {
    // The review's case (21:58Z): the player takes his boxes to the lowest band before the climbers are up. Brought back
    // to band 1, the climbers dive from where a mass defence would, and that dive is priced as mass's is.
    const kent = raid('GER', 'bomb', 'Kent');
    const lost = (style: DefenceStyle) => {
      let bmr = 0;
      for (let k = 0; k < 12; k++) {
        const s = airBattle({ ...as(kent, style), seed: kent.seed + k });
        s.ai.att = false; s.ai.def = true;
        let ordered = false;
        while (!s.over) {
          s.step();
          if (!ordered && s.t >= 30) { s.command({ do: 'air:band', band: 0, units: s.units.filter((u) => u.side === 'att' && u.air === 'bmr').map((u) => u.id) }, 'att'); ordered = true; }
        }
        for (const gr of s.outcome().groups) if (gr.side === 'att') bmr += gr.lostBmr;
      }
      return bmr / 12;
    };
    const mass = lost('mass'), methodical = lost('methodical');
    expect(Math.abs(methodical - mass) / mass, `bombers lost: methodical ${methodical.toFixed(1)}, mass ${mass.toFixed(1)}`).toBeLessThan(0.1);
    // Left alone, the climbers dive from above unpriced, and are priced as any other group once that dive has run out.
    const dived = new Map<number, number>();
    fight(as(kent, 'methodical'), 0, (s) => {
      for (const e of s.history().filter((x) => x.what === 'air:bounce' && x.t === s.t)) if (!dived.has(e.unit!)) dived.set(e.unit!, s.t);
      for (const [id, t] of dived) if (s.t > t + 61) expect(s.unit(id)!.rise, `group ${id}`).toBeFalsy();
    });
    expect(dived.size).toBeGreaterThan(0);
  }, 120_000);

  it("only the climb's own dive is drawn: a dive from the climb's height always, a second dive never (the third review)", () => {
    // The third review's cases: the player takes his box or his escorts down once the climbers are up. Ordered back
    // to band 1 (keep), a group still at band 2 dives from the climb's height: drawn. A group whose dive is still
    // running dives again on a raider below, or one back at band 1 dives: its own, priced as mass's would be.
    const cases = [
      ['FRA', 'bmr', 170], ['FRA', 'bmr', 175], ['FRA', 'bmr', 180], ['FRA', 'ftr', 100], ['GER', 'bmr', 1],
    ] as const;
    let high = 0, again = 0;
    for (const [nat, kind, at] of cases) {
      const tk = nat === 'FRA' ? raid('FRA', 'bomb', 'Kent', 0, ['UK']) : raid('GER', 'bomb', 'Kent');
      for (const style of ['methodical', 'bold'] as const) for (let k = 0; k < 12; k++) {
        const s = airBattle({ ...as(tk, style), seed: tk.seed + k });
        s.ai.att = false; s.ai.def = true;
        // Each group as it stands when contacts are made, after this tick's flying (so a group that has just come
        // down to band 1 is seen there).
        // Each group's nerve either side of the contacts: a dive's only price that touch() itself pays (the fourth review).
        let was = new Map<number, { band: number; rise: boolean; bounce: number }>();
        let mor = new Map<number, [number, number]>();
        const touch = (s as unknown as { touch: () => void }).touch.bind(s);
        (s as unknown as { touch: () => void }).touch = () => {
          was = new Map(defenders(s).map((u) => [u.id, { band: u.band, rise: !!u.rise, bounce: u.bounce }]));
          const before = new Map(s.units.map((u) => [u.id, u.mor]));
          touch();
          mor = new Map(s.units.map((u) => [u.id, [before.get(u.id)!, u.mor]]));
        };
        const drawn = new Map<number, number>();
        let ordered = false;
        while (!s.over) {
          s.step();
          if (!ordered && s.t >= at) {
            const ours = s.units.filter((u) => u.side === 'att' && u.air === kind);
            s.command({ do: 'air:band', band: 0, units: (nat === 'GER' ? ours.slice(-1) : ours).map((u) => u.id) }, 'att');
            ordered = true;
          }
          const what = `${nat} ${kind} at ${at}, ${style} battle ${k}, t=${s.t}`;
          const dives = new Map<number, number>(), priced = new Map<number, number>();
          for (const e of s.history().filter((x) => x.what === 'air:bounce' && x.t === s.t)) {
            const w = was.get(e.unit!), k = dives.get(e.unit!) ?? 0, lo = (e.extra as { on: number }).on;
            dives.set(e.unit!, k + 1);
            if (!(k === 0 && w && w.rise && w.bounce <= 0 && w.band === 2)) priced.set(lo, (priced.get(lo) ?? 0) + 1);
            else priced.set(lo, priced.get(lo) ?? 0);
          }
          // The climb's own dive knocks no nerve; every other dive knocks the group below as any dive does.
          for (const [lo, p] of priced) {
            const [m0, m1] = mor.get(lo)!;
            expect(m0 - m1, `${what}: group ${lo} dived on, ${p} priced`).toBeCloseTo(Math.min(m0, p * SKY.morale.bounced), 9);
          }
          for (const [id, n] of dives) {
            const u = s.unit(id)!, w = was.get(id);
            if (u.side !== 'def' || !w) continue;
            // The tick's first dive is drawn only on the climb, from the height it gave, and only if it is the climb's
            // first; any more are priced.
            const first = w.rise && w.bounce <= 0 && w.band === 2;
            if (w.band === 2) { high++; expect(w.rise, `${what}: group ${id} dives from the climb's height`).toBe(true); }
            if (w.bounce > 0 || n > 1) again++;
            expect(!!u.rise, `${what}: group ${id} after ${n} dive(s), ${JSON.stringify(w)}`).toBe(first && n === 1);
            if (first) drawn.set(id, (drawn.get(id) ?? 0) + 1);
          }
        }
        for (const [id, n] of drawn) expect(n, `${nat} ${kind} at ${at}, ${style} battle ${k}: group ${id}'s drawn dives`).toBe(1);
      }
    }
    expect(high).toBeGreaterThan(0);
    expect(again).toBeGreaterThan(0);
  }, 300_000);

  it('A6: a bomber box never dives on fighters (the raids where it did at the head, every battle)', () => {
    for (const [nat, name] of [['GER', 'Aube'], ['FRA', 'Bremen']] as const) {
      const tk = raid(nat, 'bomb', name);
      for (const t of [tk, plain(tk)]) for (let k = 0; k < 4; k++) {
        const s = fight(t, k);
        const box = new Set(s.units.filter((u) => u.air === 'bmr').map((u) => u.id));
        expect(s.history().filter((e) => e.what === 'air:bounce' && box.has(e.unit!)), `${name} battle ${k}`).toEqual([]);
      }
    }
  }, 120_000);

  it("the split and the climb are told in the dispatches, and the escort-takers' extra says their role", () => {
    const s = fight(as(raid('FRA', 'bomb', 'Rhineland', 30), 'methodical'), 0);
    const h = s.history();
    const split = h.filter((e) => e.what === 'air:split');
    expect(split.length).toBe(defenders(s).filter((u) => u.def === 'escort').length);
    for (const e of split) expect(e.text).toMatch(/ (is|are) sent against the escort\.$/);
    for (const e of h.filter((x) => x.what === 'air:climb')) expect(e.text).toMatch(/^Warned by radar, .+ (climbs|climb) to get above the raid\.$/);
    // The UI director, 21:26Z: the climb says why; a name that takes "the" keeps it small after the comma. The names
    // are the game's own: the German divisions over the Rhineland take "the"; Kent's groups do not.
    const climbs = h.filter((e) => e.what === 'air:climb').map((e) => e.text);
    for (const u of defenders(s)) expect(climbs).toContain(`Warned by radar, the German ${u.name} climbs to get above the raid.`);
    // Ruling 225: Kent is defended by its own two groups, and no carrier fighters climb over it.
    const over = fight(as(raid('GER', 'bomb', 'Kent'), 'methodical'), 0);
    const kent = over.history().filter((e) => e.what === 'air:climb').map((e) => e.text);
    expect(defenders(over).length).toBe(2);
    for (const u of defenders(over)) expect(kent).toContain(`Warned by radar, British ${u.name} climbs to get above the raid.`);
    expect(kent.some((t) => /carrier/.test(t))).toBe(false);
  });
});

describe("the escort's roles at stops 4 and 5 (the escortRoles hook)", () => {
  it("a computer raid's escort flies close when outweighed and sweeps when it outweighs the defence", () => {
    for (const [nat, name, first] of [['GER', 'Alsace', 'close'], ['FRA', 'Saar', 'sweep']] as const) {
      const g = startOpen(createGame(7, nat));
      for (const w of WARS[nat]) setWar(g, nat, w);
      g.flags.hist = 5;
      g.human = 'USA';
      const t = raidTargets(g, 'bomb', nat).find((x) => planRaid(g, 'bomb', x.id, undefined, nat).place === name)!;
      const day = planRaid(g, 'bomb', t.id, undefined, nat);
      const ids = [...day.picked, ...day.seats.filter((x) => !x.why && !x.picked && x.kind === 'ftr').map((x) => x.id)];
      const { ticket } = airTicket(g, 'bomb', t.id, ids, nat);
      expect(ticket!.place.roles?.[nat]?.[0], name).toBe(first);
      const s = airBattle(ticket!); s.ai.att = true; s.ai.def = true;
      s.step();
      const esc = s.units.find((u) => u.side === 'att' && u.air === 'ftr')!;
      expect(esc.job.do).toBe('escort');
      expect(esc.job.do === 'escort' && esc.job.close, name).toBe(first === 'close');
    }
  });
});
