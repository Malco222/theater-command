// The drill book (release 2; Brian, 22 November 2026, 00:23Z: "i want the troop AI to be smart and move like a platoon
// with covering fire and historical tactics. have the historian and general make sure they move and play like a real
// world war 2 unit would and with combines arms"). Looks only (ruling 217): how each army's platoons fight, by its
// school (src/battle/land/tactics.ts; the book and both reviews in /mnt/project-files/battles/drill-book/). These tests
// hold that it changes nothing in the fight, that every nation fights by a school, that the schools are the historian's,
// that fire and movement goes by turns, that suppression and the guns' missions work as the general set them, and that
// a battle shows them.
import { describe, expect, it } from 'vitest';
import { LandBattle, sampleTicket, withTeams, type MenOut } from '../src/battle/land';
import { BOOK, Suppression, boundOf, missionStep, schoolOf, schoolNums, turnOf, turns, type Mission, type Shoot } from '../src/battle/land/tactics';

const NATIONS = ['AFG', 'ARG', 'AUS', 'BEL', 'BOL', 'BRA', 'BUL', 'CAN', 'CHL', 'CHN', 'COL', 'COS', 'CUB', 'DEN', 'DOM', 'ECU', 'ELS', 'ESP', 'EST', 'FIN', 'FRA', 'GER', 'GRE', 'GUA', 'HAI', 'HON', 'HUN', 'IRL', 'IRN', 'IRQ', 'ITA', 'JAP', 'LAT', 'LIT', 'LUX', 'MAN', 'MEX', 'MGL', 'NED', 'NIC', 'NOR', 'NZL', 'PAN', 'PAR', 'PER', 'POL', 'POR', 'ROM', 'SAF', 'SAU', 'SOV', 'SUI', 'SVK', 'SWE', 'THA', 'TUR', 'URU', 'USA', 'VEN', 'VIC', 'YUG'];
const MOVING = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
const POSES = new Set([...MOVING, 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire', 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);

describe('every nation fights by its army\'s school (the historian\'s map)', () => {
  it('every playable nation has a school, its own or the principles as written', () => {
    for (const n of NATIONS) expect(BOOK.schools[schoolOf(n)], n).toBeDefined();
    expect(schoolOf('XYZ')).toBe('_');
  });
  it('the schools are the historian\'s: German squads one body round the gun, British sections a gun group and a rifle group, the American squad with its BAR, the Soviet chain', () => {
    expect(boundOf('GER', 1, false)).toBe('body');
    expect(boundOf('UK', 1, false)).toBe('both');
    expect(boundOf('USA', 1, false)).toBe('squad');
    expect(boundOf('SOV', 1, false)).toBe('squad');
    expect(boundOf('SOV', 0, false)).toBe('wave'); // a raw Soviet chain goes up as one
    expect(boundOf('GER', 2, true)).toBe('wave'); // a mass general's style is a wave, whatever the army
    // The Japanese crawl close and go in with grenades: the banzai charge was no drill (only of the broken and cut off).
    expect(schoolNums('JAP').crawl).toBeGreaterThanOrEqual(0.5);
    expect(schoolNums('JAP').final).toBe('grenade');
    expect(schoolNums('JAP').hold).toBeLessThanOrEqual(50);
    expect(schoolNums('USA').final).toBe('marching');
    // The map's fixes: Romania and Slovakia French-model armies; Spain, Argentina, Chile and Turkey German-trained;
    // Norway, the Netherlands, Mexico and Thailand by the principles.
    for (const n of ['ROM', 'SVK']) expect(schoolOf(n)).toBe('FRA');
    for (const n of ['ESP', 'ARG', 'CHL', 'TUR']) expect(schoolOf(n)).toBe('GER');
    for (const n of ['NOR', 'NED', 'MEX', 'THA']) expect(schoolOf(n)).toBe('_');
  });
});

describe('fire and movement goes by turns: one fires, one moves, one follows', () => {
  it('three turns (a wave one): a German squad moves as one body, its gun with it; the squads leapfrog', () => {
    expect(turns('body')).toBe(3); expect(turns('wave')).toBe(1);
    for (const s of [0, 1, 2]) expect(turnOf('body', true, s, false)).toBe(turnOf('body', false, s, false));
    expect(new Set([0, 1, 2].map((s) => turnOf('body', false, s, false))).size).toBe(3);
    // The headquarters goes with the leading squad.
    expect(turnOf('squad', false, 3, true)).toBe(turnOf('squad', false, 0, false));
    for (const s of [0, 1, 2]) expect(turnOf('wave', s === 1, s, false)).toBe(0);
  });
  it('a British section\'s gun group goes a turn after its rifle group, covering it; at most half a platoon is up at once', () => {
    for (const s of [0, 1, 2]) expect((turnOf('both', true, s, false) - turnOf('both', false, s, false) + 3) % 3).toBe(1);
    // An American platoon (three squads of 12 and five at its head) and a British one (three of 10, a gun group of the
    // gunner and his two, and 7 at its head): the men up in any one turn.
    for (const [bound, squad, gun, hq] of [['squad', 12, 0, 5], ['both', 10, 3, 7], ['body', 10, 3, 7]] as const) {
      const up = [0, 0, 0];
      for (let s = 0; s < 3; s++) { up[turnOf(bound, false, s, false)] += squad - gun; up[turnOf(bound, true, s, false)] += gun; }
      up[turnOf(bound, false, 3, true)] += hq;
      expect(Math.max(...up) / (3 * squad + hq), bound).toBeLessThanOrEqual(0.5);
    }
  });
});

describe('suppression (the general\'s P4)', () => {
  const groups = new Map<number, [number, number]>([[1, [100, 100]], [2, [140, 100]]]);
  it('a round landing near a group puts its heads down, the heavier the more; one in the air counts once it lands', () => {
    const S = new Suppression();
    S.feel(7, 10, 1, [{ n: 1, on: 7, l: 11, w: 'mortar', x: 103, y: 100 }], groups);
    expect(S.level(7, 1, 10)).toBe(0); // still in the air
    S.feel(7, 11, 1, [], groups);
    expect(S.level(7, 1, 11)).toBeGreaterThan(BOOK.suppress.down);
    expect(S.level(7, 2, 11)).toBe(0); // 37 m off: out of its reach
    const R = new Suppression();
    R.feel(7, 11, 1, [{ n: 1, on: 7, l: 11, w: 'rifle', x: 103, y: 100 }], groups);
    expect(R.level(7, 1, 11)).toBeLessThan(S.level(7, 1, 11) / 5);
  });
  it('it wears off by halves, raw men slower than veterans', () => {
    const at = (band: 0 | 1 | 2): number => {
      const S = new Suppression();
      S.feel(3, 0, band, [{ n: 1, on: 3, l: 0, w: 'gun', x: 100, y: 100 }], groups);
      return S.level(3, 1, 6) / S.level(3, 1, 0);
    };
    expect(at(1)).toBeCloseTo(0.5 ** (6 / BOOK.suppress.half[1]), 6);
    expect(at(0)).toBeGreaterThan(at(1));
    expect(at(2)).toBeLessThan(at(1));
  });
});

describe('the guns\' fire missions (the general\'s section 5)', () => {
  const run = (k: number, until: number, on = (t: number) => 9 + 0 * t): { t: number; s: Shoot; call: number }[] => {
    let ms: Mission | undefined;
    const out: { t: number; s: Shoot; call: number }[] = [];
    for (let t = 0; t <= until; t += 0.5) {
      const [m, s] = missionStep(ms, on(t), t, k, () => 0.5);
      if (s || !ms || m.call !== ms.call) out.push({ t, s, call: m.call });
      ms = m;
    }
    return out;
  };
  it('a call, one ranging round over, a call, one short (the bracket), a call, then fire for effect: salvos, quiet, and again', () => {
    const ev = run(1, 90), fired = ev.filter((e) => e.s).map((e) => e.s);
    expect(fired.slice(0, 2)).toEqual(['over', 'short']);
    const ffe = fired.slice(2, 2 + Math.round((BOOK.mission.salvos[0] + BOOK.mission.salvos[1]) / 2));
    expect(ffe.every((s) => s === 'salvo')).toBe(true);
    // Three calls before the first salvo; quiet after the fire for effect, then a short call and salvos again.
    const first = ev.find((e) => e.s === 'salvo')!.t;
    expect(new Set(ev.filter((e) => e.t <= first).map((e) => e.call)).size).toBe(3);
    const salvos = ev.filter((e) => e.s === 'salvo').map((e) => e.t);
    const quiet = Math.max(...salvos.slice(1).map((t, i) => t - salvos[i]));
    expect(quiet).toBeGreaterThanOrEqual(BOOK.mission.quiet[0]);
    expect(fired.filter((s) => s === 'over' || s === 'short').length).toBe(2); // no ranging again on the same target
  });
  it('a mortar team is quicker; a new target starts the mission afresh', () => {
    const gun = run(1, 60).find((e) => e.s === 'salvo')!.t, mortar = run(BOOK.mission.mortar, 60).find((e) => e.s === 'salvo')!.t;
    expect(mortar).toBeLessThan(gun * 0.7);
    const moved = run(1, 120, (t) => (t < 60 ? 9 : 12)).filter((e) => e.s && e.t >= 60).map((e) => e.s);
    expect(moved.slice(0, 2)).toEqual(['over', 'short']);
  });
});

describe('in a battle (looks only, ruling 217)', () => {
  const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
  const ticket = () => withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } }));
  const b = new LandBattle(ticket()), d = b.drillOn({ sections: true, platoons: true });
  const t = { fights: 0, both: 0, upright: 0, assault: 0, badPose: 0, badEl: 0, badCall: 0 };
  const fire = new Map<number, number[]>(), calls = new Map<number, number[]>();
  // The platoon cam: how many blocks carry the leading squad's mark at once (by side), marks on fallen men, how often
  // the lead goes from one block to another, how far the attack's lead is behind its foremost man (both measured to
  // the nearest enemy man), and how often the lead is seen on the move.
  const cam = { samples: 0, twoBlocks: 0, onFallen: 0, none: 0, switches: 0, fronts: 0, behind: 0, moving: 0, men: 0 };
  const leadBlock = new Map<string, number>();
  let last = 0;
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    for (const r of d.rounds()) {
      if (r.n <= last) continue;
      last = r.n;
      if ((r.w === 'gun' || r.w === 'mortar') && r.by !== undefined && r.r !== 'kill') { const l = fire.get(r.by) ?? []; l.push(r.t); fire.set(r.by, l); }
    }
    if (k % 5) continue;
    const view = d.view();
    if (b.t > 30) {
      cam.samples++;
      for (const side of ['att', 'def']) {
        const mine = view.filter((x) => b.unit(x.id)!.side === side);
        const marked = mine.filter((x) => x.men.some((m) => m.ld));
        if (marked.length > 1) cam.twoBlocks++;
        if (!marked.length && mine.some((x) => b.unit(x.id)!.kind === 'inf')) cam.none++;
        for (const x of marked) cam.onFallen += x.men.filter((m) => m.ld && m.d !== undefined).length;
        if (marked.length === 1) {
          if (leadBlock.has(side) && leadBlock.get(side) !== marked[0].id) cam.switches++;
          leadBlock.set(side, marked[0].id);
          if (side === 'att') cam.men += marked[0].men.filter((m) => m.ld).length;
        }
        if (side === 'att' && marked.length === 1) {
          const foes = view.filter((x) => b.unit(x.id)!.side !== side).flatMap((x) => x.men.filter((m) => m.d === undefined));
          const gap = (m: MenOut): number => foes.reduce((g, f) => Math.min(g, Math.hypot(f.x - m.x, f.y - m.y)), Infinity);
          const foot = mine.filter((x) => b.unit(x.id)!.kind === 'inf').flatMap((x) => x.men.filter((m) => m.d === undefined && m.r !== 'tank'));
          if (foes.length && foot.length) {
            cam.fronts++;
            const lead = Math.min(...marked[0].men.filter((m) => m.ld).map(gap)), first = Math.min(...foot.map(gap));
            cam.behind += lead - first;
          }
          if (marked[0].men.some((m) => m.ld && m.e === 2)) cam.moving++;
        }
      }
    }
    for (const blk of view) {
      const u = b.unit(blk.id)!;
      for (const m of [...blk.men, ...(blk.fo ?? [])] as MenOut[]) {
        if (m.d === undefined && !POSES.has(m.p)) t.badPose++;
        if (m.e !== undefined && m.e !== 1 && m.e !== 2) t.badEl++;
        if (m.cl !== undefined && m.r !== 'fo') t.badCall++;
        if (m.cl !== undefined) { const l = calls.get(blk.id) ?? []; if (!l.includes(m.cl)) l.push(m.cl); calls.set(blk.id, l); }
      }
      if (blk.ph !== 'fight' || u.side !== 'att' || u.kind !== 'inf') continue;
      const live = blk.men.filter((m) => m.d === undefined), e2 = live.filter((m) => m.e === 2);
      t.fights++;
      if (e2.length && live.some((m) => m.e === 1)) t.both++;
      t.assault += e2.length; t.upright += e2.filter((m) => m.p === 'advance' || m.p === 'run' || m.p === 'walk').length;
    }
  }

  it('a battle ends exactly the same with the drill book as with no men drawn at all (drawn as sections)', () => {
    const off = new LandBattle(ticket()), on = new LandBattle(ticket());
    on.drillOn({ sections: true, book: true });
    for (let k = 0; !off.over && k < 30000; k++) off.step();
    for (let k = 0; !on.over && k < 30000; k++) on.step();
    expect(sig(on)).toBe(sig(off));
    expect(sig(b)).toBe(sig(off));
  }, 300_000);

  it('the attack fights with a base of fire and an assault at once; most of the assault is down firing at any moment', () => {
    expect(t.fights).toBeGreaterThan(100);
    expect(t.both / t.fights).toBeGreaterThan(0.9);
    expect(t.upright / t.assault).toBeLessThan(0.5);
    expect(t.upright).toBeGreaterThan(0);
  });

  it('a battery\'s or a mortar team\'s spotter calls each shoot on the radio: its first rounds one at a time (ranging), then the salvos', () => {
    let missions = 0;
    for (const [id, cl] of calls) {
      const shots = fire.get(id) ?? [];
      const after = shots.filter((s) => s >= cl[0]);
      if (after.length < 4 || cl.length < 3) continue;
      missions++;
      // The two ranging rounds come singly, each well apart from the next round; the salvos after them.
      expect(after[1] - after[0], `block ${id}`).toBeGreaterThan(2);
      expect(after[2] - after[1], `block ${id}`).toBeGreaterThan(2);
    }
    expect(missions).toBeGreaterThan(0);
  });

  it('the platoon cam: one squad of each side carries the lead mark, a living squad on the move, and the camera is not thrown about', () => {
    expect(cam.samples).toBeGreaterThan(100);
    expect(cam.twoBlocks).toBe(0);
    expect(cam.onFallen).toBe(0);
    expect(cam.none / cam.samples).toBeLessThan(0.05);
    // A squad, not a platoon or one man: on average a handful of men up to a dozen.
    const per = cam.men / Math.max(1, cam.samples - cam.none);
    expect(per).toBeGreaterThan(3);
    expect(per).toBeLessThan(16);
    // The attack's lead is up with the foremost of its men (on average within 40 m of the nearest to the enemy), and it
    // is an assaulting squad: seen on the move in a good share of the looks (it rushes in turns, and lies firing between).
    expect(cam.fronts).toBeGreaterThan(50);
    expect(cam.behind / cam.fronts).toBeLessThan(40);
    expect(cam.moving / cam.fronts).toBeGreaterThan(0.2);
    // Each side's lead goes to another block at most about once every 30 seconds of battle, on average.
    expect(cam.switches / Math.max(1, b.t - 30)).toBeLessThan(2 / 30);
  });

  it('only poses the 3D game has; the element is 1 or 2; only the spotter\'s pair carry a call', () => {
    expect(t.badPose).toBe(0); expect(t.badEl).toBe(0); expect(t.badCall).toBe(0);
  });
});

describe('smoke on the flank (on: the 3D scene draws it)', () => {
  const ticket = () => withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'UK', def: 'GER' }, style: { att: 'methodical', def: 'bold' } }));
  const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.figures]) });
  const run = (on: boolean) => {
    const was = BOOK.smoke.on;
    BOOK.smoke.on = on;
    try {
      const b = new LandBattle(ticket()), d = b.drillOn({ sections: true, platoons: true });
      const smoke: { by: number; on: number; t: number; l: number; off: number }[] = [];
      let last = 0;
      for (let k = 0; !b.over && k < 30000; k++) {
        b.step();
        for (const r of d.rounds()) { if (r.n <= last) continue; last = r.n; if (r.w !== 'smoke') continue;
          // How far off the enemy's ground it lands, as he stands when it is fired (a light mortar's scatter).
          const e = b.unit(r.on)!, dx = r.x - e.x, dy = r.y - e.y, a = Math.abs(dx * Math.cos(e.face) - dy * Math.sin(e.face)), l = Math.abs(dx * Math.sin(e.face) + dy * Math.cos(e.face));
          smoke.push({ by: r.by!, on: r.on, t: r.t, l: r.l, off: Math.hypot(Math.max(0, a - e.w / 2), Math.max(0, l - e.d / 2)) });
        }
      }
      return { b, smoke };
    } finally { BOOK.smoke.on = was; }
  };
  const off = run(false), on = run(true);
  it('none while it is off', () => { expect(off.smoke.length).toBe(0); });
  it('a platoon lays its light mortar\'s smoke on the enemy\'s front as its assault goes out, landing after it is fired, and nothing else changes', () => {
    expect(on.smoke.filter((x) => on.b.unit(x.by)!.side === 'att').length).toBeGreaterThanOrEqual(BOOK.schools.UK.smoke);
    for (const s of on.smoke) {
      const by = on.b.unit(s.by)!, e = on.b.unit(s.on)!;
      // By a foot block attacking (the defence's too, where it goes forward off its own ground), at its enemy.
      expect(by.kind).toBe('inf');
      expect(e.side).not.toBe(by.side);
      expect(s.l).toBeGreaterThan(s.t);
      expect(s.off).toBeLessThan(50);
    }
    expect(sig(on.b)).toBe(sig(off.b));
  });
});
