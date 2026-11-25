// Squad life (release 2; Brian, 25 November 2026, 13:40Z: "remember i dont want all the soldiers of a squad doing the
// exact same thing at the same time. I want them to work together and look real as individuals apart of a squad").
// Looks only (ruling 217): Drill.life lays each man's own part over his squad's drill (drill.ts, DR.life): buddy teams
// that rush one after another in their squad's turn, each man's own moment to take up a new order, mixed stances and
// passing sentry duty on the watch, each man's own arc to look over, his own spells off his sights, his own gait and
// his own drift in the line on the move. These tests hold that it changes nothing in the fight, that no squad does one
// thing as one man for long or changes as one man often, that a squad still works together (some down firing while
// others move), and that the 3D game is sent nothing it cannot draw.
import { describe, expect, it } from 'vitest';
import { LandBattle, sampleTicket, withTeams, type MenOut } from '../src/battle/land';

const GAITS = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee']);
const POSES = new Set([...GAITS, 'drive', 'reverse', 'towed', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire', 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);
const FIRES = new Set(['fire_prone', 'fire_kneel', 'fire_stand', 'lmg_burst', 'throw_grenade']);
const NOT_FOOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
// What the 3D game draws a pose with (LandMen.cpp StanceOf): a man lying still at his rifle as one firing prone, one
// kneeling and watching as one firing kneeling. Two men do the same thing only where the game would draw them the same.
const CLIP: Record<string, string> = { hide: 'prone', fire_prone: 'prone', wait: 'kneel', fire_kneel: 'kneel', throw_grenade: 'stand', fire_stand: 'stand' };
const seen = (m: MenOut): string => `${m.p === 'lmg_burst' ? (m.r === 'mg' ? 'gun' : 'prone') : CLIP[m.p] ?? m.p}${FIRES.has(m.p) && m.a ? '*' : ''}`;

const ticket = () => withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } }));
const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });

interface Seen { samples: number; alike: number; mass: number; mins: number; kinds: number; watch: number; faceAlike: number; bound: number; both: number; badPose: number; out: number; sig: string }

/** Fights the battle with the drill as given, reading every half second what each section of five or more men on foot is doing. */
function watch(opts: { sections?: boolean; platoons?: boolean; book?: boolean; life?: boolean }): Seen {
  const b = new LandBattle(ticket()), d = b.drillOn(opts);
  const s: Seen = { samples: 0, alike: 0, mass: 0, mins: 0, kinds: 0, watch: 0, faceAlike: 0, bound: 0, both: 0, badPose: 0, out: 0, sig: '' };
  const last = new Map<string, string>();
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    if (k % 5) continue;
    for (const blk of d.view()) {
      const u = b.unit(blk.id)!;
      for (const m of blk.men) {
        if (m.d === undefined && !POSES.has(m.p)) s.badPose++;
        // Never beyond the ring round his block's box (the block is the truth).
        if (m.d === undefined && Math.hypot(m.x - u.x, m.y - u.y) > Math.hypot(u.w / 2 + 70, u.d / 2 + 70) + 130) s.out++;
      }
      if (u.kind !== 'inf') continue;
      const groups = new Map<number, MenOut[]>();
      for (const m of blk.men) {
        if (m.d !== undefined || NOT_FOOT.has(m.r) || m.wd !== undefined || m.br) continue;
        let g = groups.get(m.k);
        if (!g) groups.set(m.k, (g = []));
        g.push(m);
      }
      for (const [g, men] of groups) {
        if (men.length < 5) continue;
        const count = new Map<string, number>(), to = new Map<string, number>();
        for (const m of men) {
          const now = seen(m), key = `${blk.id}:${g}:${m.j ?? -1}`, was = last.get(key);
          count.set(now, (count.get(now) ?? 0) + 1);
          if (was !== undefined && was !== now) to.set(now, (to.get(now) ?? 0) + 1);
          last.set(key, now);
        }
        s.samples++; s.mins += 0.5 / 60; s.kinds += count.size;
        if (Math.max(...count.values()) / men.length >= 0.9) s.alike++;
        if (to.size && Math.max(...to.values()) / men.length >= 0.6) s.mass++;
        // Holding or waiting, how alike its men face: the share within 6 degrees of the section's middle facing.
        if (blk.ph === 'hold' || blk.ph === 'startline') {
          const sx = men.reduce((n, m) => n + Math.sin(m.f), 0), sy = men.reduce((n, m) => n + Math.cos(m.f), 0), mid = Math.atan2(sx, sy);
          s.watch++; s.faceAlike += men.filter((m) => Math.abs(Math.atan2(Math.sin(m.f - mid), Math.cos(m.f - mid))) < 0.105).length / men.length;
        }
        // Coming on under fire: some of the section down and firing while others of it are up and moving.
        if (blk.ph === 'bound') {
          s.bound++;
          if (men.some((m) => GAITS.has(m.p)) && men.some((m) => FIRES.has(m.p) && m.a)) s.both++;
        }
      }
    }
  }
  s.sig = sig(b);
  return s;
}

describe('squad life: each man his own part of his squad\'s drill (looks only, ruling 217)', () => {
  const plain = watch({ sections: true }), book = watch({ sections: true, book: true }), life = watch({ sections: true, life: true });

  it('a battle ends exactly the same with squad life as with no men drawn at all (drawn as sections, and as platoons)', () => {
    const off = new LandBattle(ticket());
    for (let k = 0; !off.over && k < 30000; k++) off.step();
    expect(life.sig).toBe(sig(off));
    expect(plain.sig).toBe(sig(off));
    const big = new LandBattle(ticket());
    big.drillOn({ sections: true, platoons: true, life: true });
    for (let k = 0; !big.over && k < 30000; k++) big.step();
    expect(sig(big)).toBe(sig(off));
  }, 600_000);

  it('squad life brings the drill book with it, and is off unless asked for', () => {
    const b = new LandBattle(ticket());
    expect(b.drillOn({ sections: true }).life).toBe(false);
    const c = new LandBattle(ticket()), d = c.drillOn({ sections: true, life: true });
    expect(d.life).toBe(true);
    expect(d.byBook).toBe(true);
  });

  it('no section does one thing as one man for long: all alike a small share of the time, a third of the old drill\'s at most', () => {
    expect(plain.samples).toBeGreaterThan(2000);
    expect(life.samples).toBeGreaterThan(2000);
    expect(life.alike / life.samples).toBeLessThan(0.08);
    expect(life.alike / life.samples).toBeLessThan((plain.alike / plain.samples) / 3);
    expect(life.alike / life.samples).toBeLessThan((book.alike / book.samples) / 3);
    // More different things going on in a section at any moment.
    expect(life.kinds / life.samples).toBeGreaterThan(plain.kinds / plain.samples + 0.3);
  });

  it('no section changes as one man often: three in five of it going to one same thing in one half second, under once in two minutes', () => {
    expect(life.mass / life.mins).toBeLessThan(0.5);
    expect(life.mass / life.mins).toBeLessThan((plain.mass / plain.mins) / 3);
    expect(life.mass / life.mins).toBeLessThan((book.mass / book.mins) / 3);
  });

  it('a line holding or waiting looks about it: its men face their own ways, where the old line stared one way', () => {
    expect(life.watch).toBeGreaterThan(200);
    expect(plain.faceAlike / Math.max(1, plain.watch)).toBeGreaterThan(0.8);
    expect(life.faceAlike / life.watch).toBeLessThan(0.6);
  });

  it('a squad still works together: coming on under fire, some of a section are down firing while others of it move, more often than before', () => {
    expect(life.bound).toBeGreaterThan(300);
    expect(life.both / life.bound).toBeGreaterThan(0.3);
    expect(life.both / life.bound).toBeGreaterThan(plain.both / Math.max(1, plain.bound));
  });

  it('only poses the 3D game has, and every man within reach of his block', () => {
    expect(life.badPose).toBe(0);
    expect(life.out).toBe(0);
  });
});
