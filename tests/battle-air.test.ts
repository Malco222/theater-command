// The air battle (src/battle/air): a raid fought out as air groups on a board instead of settled by one throw of the
// dice. Counted, not flown: groups are blocks with a count, a morale meter, fuel and one of three heights, and while
// in contact they wear each other down at rates taken from the raid card's own forecast. The rulings it is held to
// (the design director, 2026-11-17 17:55Z): the computer against the computer averages the raid card within a tenth;
// what a fought raid hands back to the rules stays inside the dice's own run (fly()'s luck, plus or minus 15 per cent).
import { describe, expect, it } from 'vitest';
import { settleFought } from '../src/campaign/airfought';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, Game } from '../src/campaign/types';
import { runOut } from '../src/battle/core/clock';
import { SKY } from '../src/battle/air/book';
import { AirSim, airBattle, peopleOf } from '../src/battle/air/sim';
import { airHandBack, airTicket, foughtAir } from '../src/battle/air/ticket';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

function game(nat: string): Game {
  const g = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  return g;
}

/** The staff's raid on a named province, with every fighter group that can go with it by day. */
function raid(nat: string, kind: RaidKind, name: string) {
  const g = game(nat);
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = kind === 'night' ? undefined : [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  const { ticket, why } = airTicket(g, kind, t.id, ids, nat);
  if (!ticket) throw new Error(why ?? 'no ticket');
  return { g, target: t.id, ids: ids ?? day.picked, ticket };
}

/** Fight the raid `runs` times with the computer on both sides, and average what each side lost. */
function average(tk: ReturnType<typeof raid>['ticket'], runs: number, orders?: (s: AirSim) => void) {
  let foe = 0, ftr = 0, bmr = 0, dropped = 0;
  for (let k = 0; k < runs; k++) {
    const sim = airBattle({ ...tk, seed: tk.seed + k });
    sim.ai.att = true; sim.ai.def = true;
    orders?.(sim);
    runOut(sim);
    const o = sim.outcome();
    for (const gr of o.groups) if (gr.side === 'def') foe += gr.lostFtr; else { ftr += gr.lostFtr; bmr += gr.lostBmr; }
    dropped += o.dropped;
  }
  return { foe: foe / runs, ftr: ftr / runs, bmr: bmr / runs, dropped: dropped / runs };
}

describe('the air battle against the raid card', () => {
  const CASES: [string, RaidKind, string, string][] = [
    ['GER', 'bomb', 'Alsace', 'an escorted day raid'],
    ['FRA', 'bomb', 'Saar', 'a raid with a big escort'],
    ['UK', 'bomb', 'Bremen', 'a raid with a small escort against many fighters'],
    ['GER', 'bomb', 'Kent', 'an unescorted raid into radar'],
    ['GER', 'superiority', 'Alsace', 'a fight for the sky'],
    ['UK', 'night', 'Bremen', 'a night raid'],
    ['GER', 'night', 'Kent', 'a night raid into radar'],
  ];
  for (const [nat, kind, name, what] of CASES) {
    it(`costs what the card forecast, within a tenth, with the computer on both sides: ${what} (${nat} on ${name})`, () => {
      const { ticket } = raid(nat, kind, name);
      const f = ticket.place.forecast;
      const got = average(ticket, 24);
      for (const [k, want] of [['foe', f.foe], ['ftr', f.ftr], ['bmr', f.bmr]] as const) {
        if (want < 3) continue;
        expect(Math.abs(got[k] / want - 1), `${k}: ${got[k].toFixed(1)} against ${want.toFixed(1)}`).toBeLessThan(0.1);
      }
    });
  }

  it('is the same battle every time from the same ticket', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const play = () => { const s = airBattle(ticket); s.ai.att = true; s.ai.def = true; runOut(s); return s.history().map((e) => `${e.t} ${e.what} ${e.unit ?? ''} ${e.text}`); };
    expect(play()).toEqual(play());
  });

  it('ends on its own: the bombs dropped, the fight over, the raid gone home', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    runOut(s);
    expect(s.over).not.toBeNull();
    expect(s.over!.how).toBe('objective');
    expect(s.over!.seconds).toBeLessThan(SKY.limit);
    expect(s.outcome().dropped).toBeGreaterThan(0);
  });
});

describe('what the player decides', () => {
  it('bombers sent on without their escort lose more', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const staff = average(ticket, 12);
    const alone = average(ticket, 12, (s) => {
      s.ai.att = false;
      s.command({ do: 'withdraw', units: s.units.filter((u) => u.side === 'att' && u.air === 'ftr').map((u) => u.id) }, 'att');
    });
    expect(alone.bmr).toBeGreaterThan(staff.bmr * 1.5);
    expect(alone.foe).toBe(0);
  });

  it('fighters sent above the enemy dive on it and come off better', () => {
    const { ticket } = raid('GER', 'superiority', 'Alsace');
    const staff = average(ticket, 12);
    const high = average(ticket, 12, (s) => {
      s.ai.att = false;
      s.command({ do: 'air:band', band: 2, units: s.units.filter((u) => u.side === 'att').map((u) => u.id) }, 'att');
    });
    expect(high.foe).toBeGreaterThan(staff.foe);
    expect(high.ftr).toBeLessThan(staff.ftr);
  });

  it('bombers that jettison drop nothing on the target', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    s.command({ do: 'air:jettison', units: s.units.filter((u) => u.air === 'bmr').map((u) => u.id) }, 'att');
    runOut(s);
    expect(s.outcome().dropped).toBe(0);
    expect(s.outcome().jettisoned).toBeGreaterThan(0);
  });

  it('withdrawing everything gives the battle to the other side, and orders the enemy cannot give are refused', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket);
    const theirs = s.units.find((u) => u.side === 'def')!;
    expect(s.command({ do: 'air:band', band: 2, units: [theirs.id] }, 'att')).toBe(false);
    expect(s.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(s.over?.winner).toBe('def');
    expect(s.over?.how).toBe('withdrew');
  });

  it('one press hands the rest to the staff: settled at once from where each side stands, and what was fought stays fought', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    while (!s.over && s.history().filter((e) => e.what === 'air:lost').length < 10) s.step();
    const sofar = new Map(s.units.map((u) => [u.id, u.n0 - u.n]));
    expect(s.command({ do: 'dice' }, 'att')).toBe(true);
    expect(s.over).not.toBeNull();
    for (const u of s.units) expect(u.n0 - u.n).toBeGreaterThanOrEqual(sofar.get(u.id)!);
    expect(s.over!.lines[0]).toContain('The staff settled the rest of the raid on Alsace');
    // One bill, the one the campaign books, then the part of it that came before the press (the UI director, 18:45Z).
    expect(s.over!.lines.filter((l) => l.startsWith('The raid cost you'))).toHaveLength(1);
    expect(s.over!.lines[s.over!.lines.length - 1]).toMatch(/^Before you handed it to the staff, you had lost .+ and the French .+\.$/);
    expect(s.over!.lines.join(' ')).not.toMatch(/\b0 (fighters|bombers)|::|dice/);
    expect(s.outcome().dropped).toBeGreaterThan(0);
  });

  it('fought to the end, the after card has the one bill, the player\'s own side first', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    runOut(s);
    const L = s.over!.lines;
    expect(L[0]).toMatch(/^\d+ of your \d+ bombers dropped their bombs on Alsace/);
    expect(L[1]).toMatch(/^The raid cost you .+, and the French lost .+\.$/);
    expect(L.some((l) => l.startsWith('Before you handed') || l.startsWith('The staff settled'))).toBe(false);
    expect(L.join(' ')).not.toMatch(/\b0 (fighters|bombers)|::|dice/);
  });

  it('when he defends, the card names the raiders\' bombers and his own losses first', () => {
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    const t = raidTargets(g, 'bomb', 'GER').find((x) => planRaid(g, 'bomb', x.id, undefined, 'GER').place === 'Alsace')!;
    const { ticket } = airTicket(g, 'bomb', t.id, undefined, 'GER');
    expect(ticket!.def.human).toBe(true);
    const s = airBattle(ticket!); s.ai.att = true; s.ai.def = true;
    runOut(s);
    expect(s.over!.lines[0]).toMatch(/^\d+ of \d+ German bombers dropped their bombs on Alsace/);
    expect(s.over!.lines[1]).toMatch(/^The raid cost you .+, and the Germans lost .+\.$/);
  });

  it('names a people from its adjective', () => {
    expect(['French', 'German', 'British', 'Swiss', 'Soviet', 'Japanese', 'Iraqi', 'Luxembourg'].map(peopleOf))
      .toEqual(['French', 'Germans', 'British', 'Swiss', 'Soviets', 'Japanese', 'Iraqis', 'Luxembourgers']);
  });

  it('handed to the staff at first contact, the raid still costs about what the card forecast', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const f = ticket.place.forecast;
    const got = average(ticket, 24, (s) => {
      const step = s.step.bind(s);
      s.step = () => { step(); if (!s.over && s.units.some((u) => u.contacts.length)) s.command({ do: 'dice' }, 'att'); };
    });
    for (const [k, want] of [['foe', f.foe], ['ftr', f.ftr], ['bmr', f.bmr]] as const) expect(Math.abs(got[k] / want - 1), `${k}: ${got[k].toFixed(1)} against ${want.toFixed(1)}`).toBeLessThan(0.15);
  });

  it('by night the defender sees only what comes close', () => {
    const { ticket } = raid('UK', 'night', 'Bremen');
    const s = airBattle(ticket);
    expect(s.view('def').units.some((u) => u.side === 'att')).toBe(false);
    expect(s.view('att').units.some((u) => u.side === 'att')).toBe(true);
  });
});

describe('what the 3D game draws from', () => {
  it('each group says how many models it shows, how it is drawn up, what it is doing and whom it fights', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    const acts = new Set<string>();
    let fought = false;
    while (!s.over) {
      s.step();
      for (const u of s.view().units) {
        acts.add(u.act);
        expect(u.figures).toBeLessThanOrEqual(u.figures0);
        expect(u.figures0).toBeLessThanOrEqual(SKY.look.most[u.air]);
        expect(u.w).toBeGreaterThan(0);
        if (u.act === 'fight') { fought = true; expect(u.engaged).toBeDefined(); }
        if (u.state === 'gone' || u.state === 'withdrawn') expect(u.act).toBe('down');
      }
    }
    expect(fought).toBe(true);
    for (const a of ['march', 'fight']) expect([...acts], a).toContain(a);
    // Every event that makes a sound names one from the sound director's list, and says where it happened.
    for (const e of s.history().filter((x) => x.cue)) { expect(typeof e.cue).toBe('string'); expect(typeof e.x).toBe('number'); }
  });
});

describe('an aircraft going down (Brian, 17 November)', () => {
  it('explodes, burns or spins; fighter pilots seen under a parachute are the ones who fly again', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    let falls = 0;
    for (let k = 0; k < 6; k++) {
      const s = airBattle({ ...ticket, seed: ticket.seed + k }); s.ai.att = true; s.ai.def = true;
      runOut(s);
      const lost = s.history().filter((e) => e.what === 'air:lost');
      falls += lost.length;
      const out = s.outcome();
      // Every aircraft lost is one fall; every pilot the rules give back was seen to get out.
      expect(lost.length).toBe(out.groups.reduce((n, gr) => n + gr.lostFtr + gr.lostBmr, 0));
      expect(lost.filter((e) => e.extra!.back).length).toBe(out.groups.reduce((n, gr) => n + gr.back, 0));
      for (const e of lost) {
        const x = e.extra as { kind: string; chutes: number; back: boolean; air: string; over: string };
        expect(['explode', 'burn', 'spin']).toContain(x.kind);
        if (x.back) { expect(x.kind).not.toBe('explode'); expect(x.chutes).toBeGreaterThan(0); }
        if (x.air === 'bmr') expect(x.chutes).toBeLessThanOrEqual(SKY.fall.crew);
        expect(x.over).toBe(e.side === 'att' ? 'enemy' : 'own');
      }
    }
    expect(falls).toBeGreaterThan(50);
  });
});

describe('the result back to the rules (ruling of 17:55Z: inside the dice\'s own run)', () => {
  const fleet = (id: number, ftr: number, bmr: number): AirFleet => ({ id, ftr, bmr } as AirFleet);

  it('keeps what the battle reports when it is inside the run, and holds it at the edge when it is not', () => {
    const a = fleet(1, 100, 0), b = fleet(2, 100, 0), e = fleet(3, 50, 0), bm = fleet(4, 0, 80);
    const groups = { def: [{ f: a, share: 0.6 }, { f: b, share: 0.4 }], esc: [{ f: e, share: 1 }], bmr: [{ f: bm, share: 1 }] };
    const inside = settleFought({ lost: { 1: { ftr: 12, bmr: 0, back: 5 }, 2: { ftr: 8, bmr: 0, back: 3 }, 3: { ftr: 10, bmr: 0, back: 1 }, 4: { ftr: 0, bmr: 16, back: 0 } }, dropped: 62 },
      { foe: 20, ftr: 10, bmr: 16 }, groups, 80);
    expect([inside.ftr(a), inside.ftr(b), inside.ftr(e), inside.bmr(bm)]).toEqual([12, 8, 10, 16]);
    expect(inside.back(a, 12)).toBe(5);
    expect(inside.through(64)).toBe(62);
    // A battle far luckier for the defenders than the dice allow: held to the top of the run, spread by fly()'s shares.
    const wild = settleFought({ lost: { 1: { ftr: 1, bmr: 0, back: 0 }, 2: { ftr: 0, bmr: 0, back: 0 }, 3: { ftr: 40, bmr: 0, back: 0 }, 4: { ftr: 0, bmr: 50, back: 0 } }, dropped: 10 },
      { foe: 20, ftr: 10, bmr: 16 }, groups, 80);
    expect(wild.ftr(a) + wild.ftr(b)).toBe(Math.floor(20 * 0.85));
    expect(wild.ftr(a)).toBeGreaterThan(wild.ftr(b));
    expect(wild.ftr(e)).toBe(Math.ceil(10 * 1.15));
    expect(wild.bmr(bm)).toBe(Math.ceil(16 * 1.15));
    expect(wild.through(80 - wild.bmr(bm))).toBe(80 - Math.ceil(16 * 1.15));
  });

  it('a fought raid lands on the campaign as the battle went, inside the run', () => {
    const { g, target, ids, ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    runOut(s);
    const before = new Map(g.airFleets.map((f) => [f.id, { ftr: f.ftr, bmr: f.bmr }]));
    const rep = airHandBack(g, 'bomb', target, ids, s.over!);
    expect(rep).not.toBeNull();
    const f = ticket.place.forecast;
    const lost = (side: 'att' | 'def', k: 'ftr' | 'bmr'): number => ticket[side].forces.filter((x, i, xs) => xs.findIndex((y) => y.corps === x.corps) === i)
      .reduce((n, x) => n + before.get(x.corps)![k] - g.airFleets.find((y) => y.id === x.corps)![k], 0);
    expect(lost('def', 'ftr')).toBeGreaterThanOrEqual(Math.floor(f.foe * 0.85));
    expect(lost('def', 'ftr')).toBeLessThanOrEqual(Math.ceil(f.foe * 1.15));
    expect(lost('att', 'bmr')).toBeGreaterThanOrEqual(Math.floor(f.bmr * 0.85));
    expect(lost('att', 'bmr')).toBeLessThanOrEqual(Math.ceil(f.bmr * 1.15));
    // Inside the run, the battle's own figure stands.
    const battle = s.outcome().groups.filter((x) => x.side === 'def').reduce((n, x) => n + x.lostFtr, 0);
    if (battle >= Math.floor(f.foe * 0.85) && battle <= Math.ceil(f.foe * 1.15)) expect(lost('def', 'ftr')).toBe(battle);
  });
});
