// The air battle (src/battle/air): a raid fought out as air groups on a board instead of settled by one throw of the
// dice. Counted, not flown: groups are blocks with a count, a morale meter, fuel and one of three heights, and while
// in contact they wear each other down at rates taken from the raid card's own forecast. The rulings it is held to
// (the design director, 2026-11-17 17:55Z): the computer against the computer averages the raid card within a tenth;
// what a fought raid hands back to the rules is booked as the field saw it (b397).
import { describe, expect, it } from 'vitest';
import { afterCard, afterWords, rosterWords, settleFought } from '../src/campaign/airfought';
import { launchRaid, planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { airAct } from '../src/campaign/airpages';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, AirReport, Game } from '../src/campaign/types';
import { runOut } from '../src/battle/core/clock';
import { SKY } from '../src/battle/air/book';
import { AirSim, airBattle, peopleOf, subjectOf, told } from '../src/battle/air/sim';
import { airTicket, foughtAir } from '../src/battle/air/ticket';
import { defenceOf, type DefenceStyle } from '../src/battle/air/styles';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

function game(nat: string): Game {
  const g = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  return g;
}

/** The staff's raid on a named province, with every fighter group that can go with it by day (`set`: the game first). */
function raid(nat: string, kind: RaidKind, name: string, set?: (g: Game) => void) {
  const g = game(nat);
  set?.(g);
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
  ];
  for (const [nat, kind, name, what] of CASES) {
    it(`costs what the card forecast, within a tenth, with the computer on both sides: ${what} (${nat} on ${name})`, () => {
      const { ticket } = raid(nat, kind, name);
      const f = ticket.place.forecast;
      const got = average(ticket, 24);
      // Within a tenth; where the card forecasts only a handful, within one aircraft. Losses fall by chance (AirSim.fall),
      // so 24 raids that lose 3 or 4 aircraft each carry about a tenth of luck on their own (Bremen's 3.4: 3.7 over 200).
      for (const [k, want] of [['foe', f.foe], ['ftr', f.ftr], ['bmr', f.bmr]] as const) {
        if (want < 3) continue;
        expect(Math.abs(got[k] - want), `${k}: ${got[k].toFixed(1)} against ${want.toFixed(1)}`).toBeLessThan(Math.max(0.1 * want, 1));
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

describe("the air battle against the raid card, whatever the computer defence's style (ruling 239)", () => {
  const CASES: [string, RaidKind, string][] = [['GER', 'bomb', 'Alsace'], ['FRA', 'bomb', 'Saar'], ['UK', 'bomb', 'Bremen'], ['GER', 'bomb', 'Kent'], ['GER', 'superiority', 'Alsace']];
  // At stop 5 with the computer flying the raid too, the escort's roles come from its weight (the escortRoles hook);
  // otherwise each style forced on the defence.
  const HOW: [string, (g: Game) => void, DefenceStyle?][] = [
    ['at stop 5, the computer flying both sides', (g) => { g.flags.hist = 5; g.human = 'USA'; }],
    ['the defence bold', () => {}, 'bold'],
    ['the defence methodical', () => {}, 'methodical'],
    ['the defence mass', () => {}, 'mass'],
  ];
  for (const [nat, kind, name] of CASES) for (const [how, set, style] of HOW) {
    it(`costs what the card forecast, within a tenth: ${nat} on ${name} (${kind}), ${how}`, () => {
      const { ticket } = raid(nat, kind, name, set);
      const tk = style ? { ...ticket, place: { ...ticket.place, defence: defenceOf(style) } } : ticket;
      const f = tk.place.forecast;
      const got = average(tk, 24);
      for (const [k, want] of [['foe', f.foe], ['ftr', f.ftr], ['bmr', f.bmr]] as const) {
        if (want < 3) continue;
        expect(Math.abs(got[k] - want), `${k}: ${got[k].toFixed(1)} against ${want.toFixed(1)}`).toBeLessThan(Math.max(0.1 * want, 1));
      }
    });
  }
});

describe('a stream wider than the target ring (the tactics panel\'s air brief, item 7; the design director, 20 November)', () => {
  /** The staff's raid on a named province with its bomber groups copied, at the same airfield, until `n` boxes go. */
  function stream(nat: string, name: string, n: number) {
    const g = game(nat);
    const t = raidTargets(g, 'bomb', nat).find((x) => planRaid(g, 'bomb', x.id, undefined, nat).place === name)!;
    const going = g.airFleets.filter((f) => planRaid(g, 'bomb', t.id, undefined, nat).picked.includes(f.id) && f.bmr > 0);
    let id = Math.max(...g.airFleets.map((f) => f.id)) + 1;
    for (let k = 0; going.length + k < n; k++) {
      const c = JSON.parse(JSON.stringify(going[k % going.length])) as AirFleet;
      g.airFleets.push({ ...c, id: id++, name: `${c.name} ${k + 2}`, escorts: undefined });
    }
    const day = planRaid(g, 'bomb', t.id, undefined, nat);
    const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
    const { ticket } = airTicket(g, 'bomb', t.id, ids, nat);
    expect(ticket!.att.forces.filter((f) => f.kind === 'bmr').length).toBe(n);
    return ticket!;
  }

  it('every box that reaches the run drops: the outer lanes, beyond the ring, run in where their lane ends', () => {
    for (const n of [10, 12]) {
      const tk = stream('GER', 'Kent', n);
      for (let k = 0; k < 6; k++) {
        const s = airBattle({ ...tk, seed: tk.seed + k });
        s.ai.att = true; s.ai.def = true;
        runOut(s);
        const boxes = s.units.filter((u) => u.air === 'bmr');
        expect(boxes.filter((u) => Math.abs(u.lane) > SKY.target.r).length).toBe(n - 8);
        for (const u of boxes) expect(u.dropped > 0 || u.jettisoned || u.n <= 0, `${n} boxes, battle ${k}: box ${u.id} on lane ${u.lane}`).toBe(true);
      }
    }
  });

  it('ten and twelve boxes into radar cost the bombers the card forecast and put its bombs on the target, within a tenth', () => {
    for (const n of [10, 12]) {
      const tk = stream('GER', 'Kent', n), f = tk.place.forecast, got = average(tk, 24);
      expect(Math.abs(got.bmr - f.bmr), `${n} boxes: bombers lost ${got.bmr.toFixed(1)} against ${f.bmr.toFixed(1)}`).toBeLessThan(0.1 * f.bmr);
      expect(Math.abs(got.dropped - f.through), `${n} boxes: bombers over the target ${got.dropped.toFixed(1)} against ${f.through.toFixed(1)}`).toBeLessThan(0.1 * f.through);
    }
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
    // Without its escort only the bombers' gunners bring fighters down (ruling 227): some, but far fewer than the escort does.
    expect(alone.foe).toBeGreaterThan(0);
    expect(alone.foe).toBeLessThan(staff.foe / 4);
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

  it('one press hands the rest to the staff: the battle ends at once, and what was fought stays fought', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    while (!s.over && s.history().filter((e) => e.what === 'air:lost').length < 10) s.step();
    const sofar = new Map(s.units.map((u) => [u.id, u.n0 - u.n]));
    expect(s.command({ do: 'dice' }, 'att')).toBe(true);
    expect(s.over).not.toBeNull();
    // The battle adds nothing of its own after the press: the rest is the campaign's dice.
    for (const u of s.units) expect(u.n0 - u.n).toBe(sofar.get(u.id)!);
    const st = s.outcome().staff!;
    for (const k of ['foe', 'ftr', 'bmr'] as const) { expect(st.rest[k]).toBeGreaterThanOrEqual(0); expect(st.rest[k]).toBeLessThan(1); }
    expect(s.over!.lines[0]).toContain('The staff settled the rest of the raid on Alsace');
    // Its own card leaves the bill to the campaign, and says which part came before the press.
    expect(s.over!.lines.some((l) => l.startsWith('The raid cost you'))).toBe(false);
    expect(s.over!.lines[s.over!.lines.length - 1]).toMatch(/^Before you handed it to the staff, you had lost .+ and the French .+\.$/);
    expect(s.over!.lines.join(' ')).not.toMatch(/\b0 (fighters|bombers)|::|dice/);
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

  it('a night raid is not fought out: the staff settles it (b384(c))', () => {
    const g = game('UK');
    const t = raidTargets(g, 'night', 'UK').find((x) => planRaid(g, 'night', x.id, undefined, 'UK').place === 'Bremen')!;
    const { ticket, why } = airTicket(g, 'night', t.id, undefined, 'UK');
    expect(ticket).toBeNull();
    expect(why).toMatch(/the staff settles a night raid/);
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
        // (Brian, 21 November 2026: the numbers proportionate. One scale for the whole battle replaces the old cap of
        // 12 models a group: every model stands for the same number of aircraft, and no group is cut to a size.)
        expect(u.figures0).toBe(Math.max(1, Math.ceil(u.men0 / u.per)));
        expect(u.w).toBeGreaterThan(0);
        if (u.act === 'fight') { fought = true; expect(u.engaged).toBeDefined(); }
        if (u.state === 'gone' || u.state === 'withdrawn') expect(u.act).toBe('down');
      }
    }
    expect(fought).toBe(true);
    for (const a of ['march', 'fight']) expect([...acts], a).toContain(a);
    // One scale: the same 'per' for every group, the smallest that keeps the models at the start to about SKY.look.all.
    const groups = s.view().units;
    const sent = groups.reduce((n, u) => n + u.men0, 0), per = groups[0].per;
    for (const u of groups) expect(u.per).toBe(per);
    expect(per).toBe(Math.max(1, Math.ceil(sent / SKY.look.all)));
    expect(groups.reduce((n, u) => n + u.figures0, 0)).toBeLessThanOrEqual(SKY.look.all + groups.length);
    // Every event that makes a sound names one from the sound director's list, and says where it happened.
    for (const e of s.history().filter((x) => x.cue)) { expect(typeof e.cue).toBe('string'); expect(typeof e.x).toBe('number'); }
    // A bomber group's first contact names the fighters on it, so the 3D game can play their own pass.
    const units = new Map(s.view().units.map((u) => [u.id, u]));
    const onBmr = s.history().filter((e) => e.what === 'contact' && units.get(e.unit!)?.air === 'bmr');
    expect(onBmr.length).toBeGreaterThan(0);
    for (const e of onBmr) { expect(e.cue).toBe(SKY.cues.onBombers); expect(units.get((e.extra as { with: number }).with)?.air).toBe('ftr'); }
  });

  it('names none of the made-up sounds the mix renamed (the sound director, 18 November)', () => {
    const names: string[] = [];
    const walk = (v: unknown): void => {
      if (typeof v === 'string') names.push(v);
      else if (Array.isArray(v)) v.forEach(walk);
      else if (v && typeof v === 'object') Object.values(v).forEach(walk);
    };
    // 'engines', 'calibre' and 'mix' name entries in the sound director's mix.json, not sounds.
    const { engines, calibre, mix, ...sounds } = SKY.cues;
    walk(sounds);
    expect(names.length).toBeGreaterThan(12);
    expect(Object.keys(engines)).toEqual(['bmr', 'ftr']);
    expect(Object.values(calibre)).toEqual(['rifle', 'heavy', 'cannon']);
    for (const path of Object.values(mix)) expect(path.startsWith('air.')).toBe(true);
    // The made-up names the mix renamed (mix.json "rename", air) are gone.
    for (const old of ['explosion', 'fighter_loop', 'fighter_pass_02', 'flak_barrage_loop', 'parachutes']) expect(names).not.toContain(old);
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

describe('handed to the staff (the design director, log b368): the rules\' own dice settle the rest', () => {
  /** Two copies of the same campaign: one to roll the raid with the dice, one to fight it and press at once. */
  function twin(seed: number, nat: string, kind: RaidKind, name: string) {
    const make = (): Game => { const g = startOpen(createGame(seed, nat)); for (const w of WARS[nat]) setWar(g, nat, w); return g; };
    const a = make(), b = make();
    const t = raidTargets(a, kind, nat).find((x) => planRaid(a, kind, x.id, undefined, nat).place === name)!;
    const day = planRaid(a, kind, t.id, undefined, nat);
    const ids = kind === 'night' ? day.picked : [...day.picked, ...day.seats.filter((x) => !x.why && !x.picked && x.kind === 'ftr').map((x) => x.id)];
    return { a, b, target: t.id, ids };
  }
  const booked = (r: AirReport) => [...r.att, ...r.def].map((x) => `${x.id}:${x.lostFtr}/${x.lostBmr}`).join(' ');
  const CASES: [string, RaidKind, string][] = [['GER', 'bomb', 'Alsace'], ['UK', 'bomb', 'Bremen'], ['GER', 'bomb', 'Kent'], ['GER', 'superiority', 'Alsace']];

  for (const [nat, kind, name] of CASES) {
    it(`pressed at once, ${nat} ${kind} on ${name} books exactly what the campaign would have rolled, at several seeds`, () => {
      for (const seed of [7, 19, 31, 58]) {
        const { a, b, target, ids } = twin(seed, nat, kind, name);
        const dice = launchRaid(a, kind, target, ids, nat)!;
        const { ticket } = airTicket(b, kind, target, ids, nat);
        const s = airBattle(ticket!);
        s.command({ do: 'dice' }, 'att');
        const st = s.outcome().staff!;
        expect(st.rest).toEqual({ foe: 1, ftr: 1, bmr: 1 });
        const rep = launchRaid(b, kind, target, ids, nat, foughtAir(s.outcome()))!;
        expect(booked(rep), `seed ${seed}`).toBe(booked(dice));
        expect(rep.result).toBe(dice.result);
        expect(rep.level).toBe(dice.level);
        // Nothing had fallen before the press: the sea card's form (the UI director, air review 2).
        const card = afterCard(b, rep, foughtAir(s.outcome()));
        expect(card.lines[card.lines.length - 1]).toBe('Before you handed it to the staff, no aircraft had fallen on either side.');
      }
    });
  }

  it('pressed partway, it never books fewer losses than had already happened, and the card gives the campaign\'s bill', () => {
    for (const [nat, kind, name] of CASES.slice(0, 4)) {
      for (const seed of [7, 31]) {
        const { b, target, ids } = twin(seed, nat, kind, name);
        const { ticket } = airTicket(b, kind, target, ids, nat);
        const s = airBattle(ticket!); s.ai.att = true; s.ai.def = true;
        while (!s.over && s.history().filter((e) => e.what === 'air:lost').length < 12) s.step();
        if (s.over) continue;
        s.command({ do: 'dice' }, 'att');
        const fought = foughtAir(s.outcome());
        // The raid order the 3D game's bridge goes through; where it is not his turn in the test, the order beneath it.
        const act = nat === 'GER' ? airAct(b, `raid:${kind}:${target}:${ids.join('.')}`, fought)! : null;
        const rep = act ? b.airReports[b.airReports.length - 1] : launchRaid(b, kind, target, ids, nat, fought)!;
        if (act) expect(act.ok).toBe(true);
        for (const x of [...rep.att, ...rep.def]) {
          const had = fought.lost[x.id!] ?? { ftr: 0, bmr: 0 };
          expect(x.lostFtr, `${nat} ${kind} seed ${seed} ${x.fleet}`).toBeGreaterThanOrEqual(had.ftr);
          expect(x.lostBmr).toBeGreaterThanOrEqual(had.bmr);
        }
        // The card's bill is the report's, his own side first, and the part before the press is the battle's own.
        const card = act ? act.after! : afterCard(b, rep, fought);
        const mine = rep.att.reduce((t, x) => t + x.lostFtr + x.lostBmr, 0);
        expect(card.lines[0]).toContain('The staff settled the rest');
        const bill = card.lines.find((l) => / cost you /.test(l))!;
        expect(bill).toBeTruthy();
        const myPart = bill.split(', and the ')[0], f = rep.att.reduce((t, x) => t + x.lostFtr, 0), k = rep.att.reduce((t, x) => t + x.lostBmr, 0);
        if (mine === 0) expect(myPart).toContain('cost you nothing');
        if (f) expect(myPart).toContain(`${f} fighter`);
        if (k) expect(myPart).toContain(`${k} bomber`);
        expect(card.lines[card.lines.length - 1]).toMatch(/^Before you handed it to the staff, you had lost /);
        expect(card.lines.join(' ')).not.toMatch(/\b0 (fighters|bombers)|::|dice/);
        expect(card.groups.length).toBe(rep.att.length + rep.def.length);
        // The roster counts what flew, as the top bar did, in one form on both sides (the design director, 19:29Z).
        for (const x of card.groups) {
          const row = rep[x.side].find((y) => y.fleet === x.name)!;
          const flew = s.units.filter((u) => u.corps === row.id).reduce((n, u) => n + u.n0, 0);
          expect(x.flew).toBe(flew);
          if (flew > 1) expect(x.words).toBe(`lost ${x.lostFtr + x.lostBmr || 'none'} of the ${flew} that flew`);
        }
        // One reckoning line, against the chance the raid card gave, in the land and sea cards' words.
        expect(card.reckoning).toMatch(/^(As the staff expected|As the staff feared|You did better than the staff expected|You did worse than the staff expected|Better than the staff expected|Worse than the staff expected): they /);
        expect(card.reckoning).not.toMatch(/dice/);
      }
    }
  });
});

describe('the dispatches\' grammar (the UI director, air review 10)', () => {
  it('a group named as a plural takes a plural verb; a carrier\'s fighters are the fleet\'s', () => {
    expect(subjectOf('Home Fleet carrier fighters')).toEqual({ who: "The Home Fleet's carrier fighters", whom: "the Home Fleet's carrier fighters", many: true, craft: true });
    expect(told('Home Fleet carrier fighters', '{N} [is|are] in a fight.')).toBe("The Home Fleet's carrier fighters are in a fight.");
    expect(told('Jagdgeschwader 4', '{N} [is|are] in a fight.')).toBe('Jagdgeschwader 4 is in a fight.');
    expect(told('No. 11 Group', '{N} [breaks|break]: [it jettisons its|they jettison their] bombs and [turns|turn] for home.')).toBe('No. 11 Group breaks: it jettisons its bombs and turns for home.');
    // A strike wing is its fleet's too, and singular (the UI director's own form, words-pacific-news.md: "The Mobile Fleet's strike wing").
    expect(told('Home Fleet strike wing', '{N} [lands|land].')).toBe("The Home Fleet's strike wing lands.");
  });

  it('a roster row spells a lone aircraft', () => {
    expect(rosterWords(0, 1)).toBe('lost none of the one that flew');
    expect(rosterWords(1, 1)).toBe('lost the one that flew');
    expect(rosterWords(0, 175)).toBe('lost none of the 175 that flew');
    expect(rosterWords(25, 175)).toBe('lost 25 of the 175 that flew');
  });

  it('every dispatch in a battle reads without a bracket or a stray brace', () => {
    const { ticket } = raid('GER', 'bomb', 'Alsace');
    const s = airBattle(ticket); s.ai.att = true; s.ai.def = true;
    runOut(s);
    for (const e of s.history()) if (e.text) expect(e.text).not.toMatch(/[[\]{}|]/);
    expect(s.outcome().chance).toBe(ticket.place.chance);
    expect(ticket.place.chance).toBeGreaterThan(0);
    expect(ticket.place.chance).toBeLessThan(1);
  });
});

describe('the result back to the rules (b397: the card takes the field\'s losses)', () => {
  const fleet = (id: number, ftr: number, bmr: number): AirFleet => ({ id, ftr, bmr } as AirFleet);

  it('books what the battle reports, however far from the card, never past a group\'s strength', () => {
    const a = fleet(1, 100, 0), b = fleet(2, 100, 0), e = fleet(3, 50, 0), bm = fleet(4, 0, 80);
    const inside = settleFought({ lost: { 1: { ftr: 12, bmr: 0, back: 5 }, 2: { ftr: 8, bmr: 0, back: 3 }, 3: { ftr: 10, bmr: 0, back: 1 }, 4: { ftr: 0, bmr: 16, back: 0 } }, dropped: 62 }, 80);
    expect([inside.ftr(a), inside.ftr(b), inside.ftr(e), inside.bmr(bm)]).toEqual([12, 8, 10, 16]);
    expect(inside.back(a, 12)).toBe(5);
    expect(inside.through()).toBe(62);
    // A battle far luckier for the defenders than the card: it stands as fought (the playtester's Bremen, b397(3)).
    const wild = settleFought({ lost: { 1: { ftr: 1, bmr: 0, back: 0 }, 2: { ftr: 0, bmr: 0, back: 0 }, 3: { ftr: 40, bmr: 0, back: 0 }, 4: { ftr: 0, bmr: 90, back: 0 } }, dropped: 10 }, 80);
    expect([wild.ftr(a), wild.ftr(b), wild.ftr(e), wild.bmr(bm)]).toEqual([1, 0, 40, 80]);
    expect(wild.through()).toBe(10);
  });

  it('the after card never says the opposite of the field, and counts each bomber once (b384(a))', () => {
    let lines = 0;
    for (const [nat, kind, name] of [['GER', 'bomb', 'Alsace'], ['UK', 'bomb', 'Bremen'], ['GER', 'bomb', 'Kent'], ['GER', 'superiority', 'Alsace']] as [string, RaidKind, string][]) {
      for (const k of [0, 1, 2, 3]) {
        const { g, target, ids, ticket } = raid(nat, kind, name);
        const s = airBattle({ ...ticket, seed: ticket.seed + k }); s.ai.att = true; s.ai.def = true;
        // Half of them break off everything partway, as the playtester did at Bremen.
        if (k % 2) { while (!s.over && s.history().filter((e) => e.what === 'air:lost').length < 8) s.step(); if (!s.over) s.command({ do: 'withdrawAll', side: 'att' }, 'att'); }
        runOut(s);
        const out = s.outcome();
        const fought = foughtAir(out);
        const rep = launchRaid(g, kind, target, ids, nat, fought)!;
        const card = afterCard(g, rep, fought);
        // The field's verdict: the campaign's measure on the losses the field saw, with what the bombs did.
        const seen = (side: 'att' | 'def'): number => out.groups.filter((x) => x.side === side).reduce((n, x) => n + x.lostFtr + x.lostBmr, 0);
        const fieldWon = out.gaveUp ? out.gaveUp === 'def' : rep.sky || (seen('def') + (rep.worth ?? 0) + 10) / (seen('att') + 10) >= 1;
        expect(rep.won, `${nat} ${kind} ${name} ${k}`).toBe(fieldWon);
        expect(card.level.includes('victory'), `${nat} ${kind} ${name} ${k}: card ${card.level}`).toBe(fieldWon);
        if (k % 2) expect(fieldWon).toBe(false);
        const B = ticket.att.forces.filter((f) => f.kind === 'bmr').reduce((n, f) => n + f.men, 0);
        if (B > 0) {
          expect(rep.dropped).toBe(out.dropped);
          const line = card.lines.find((l) => / bombers dropped their bombs on /.test(l))!;
          const nums = (line.match(/\d+/g) ?? []).map(Number);
          // "36 of your 48 ...; 8 jettisoned theirs, and 4 were lost before the target": every bomber once.
          const first = line.startsWith('None') ? 0 : nums[0];
          const rest = nums.slice(line.startsWith('None') ? 1 : 2).reduce((a, b) => a + b, 0);
          expect(first + rest, line).toBe(B);
          expect(line).not.toMatch(/\b0 /);
          lines++;
        }
      }
    }
    expect(lines).toBeGreaterThan(8);
  });

  it('the bombers line in its forms (the UI director, newcomer review 5)', () => {
    const w = { place: 'Bremen', sky: false, me: 'att' as const, adj: { att: 'British', def: 'German' }, staff: false, bill: null, before: null };
    expect(afterWords({ ...w, bombers: 48, dropped: 36, jettisoned: 8, lostBefore: 4 })[0]).toBe('36 of your 48 bombers dropped their bombs on Bremen; 8 jettisoned theirs, and 4 were lost before the target.');
    expect(afterWords({ ...w, bombers: 48, dropped: 0, jettisoned: 41, lostBefore: 7 })[0]).toBe('None of your 48 bombers dropped their bombs on Bremen: 41 jettisoned theirs, and 7 were lost before the target.');
    expect(afterWords({ ...w, bombers: 48, dropped: 48, jettisoned: 0, lostBefore: 0 })[0]).toBe('48 of your 48 bombers dropped their bombs on Bremen.');
    expect(afterWords({ ...w, bombers: 48, dropped: 47, jettisoned: 0, lostBefore: 1 })[0]).toBe('47 of your 48 bombers dropped their bombs on Bremen; 1 was lost before the target.');
    expect(afterWords({ ...w, me: 'def', bombers: 48, dropped: 0, jettisoned: 48, lostBefore: 0 })[0]).toBe('None of the 48 British bombers dropped their bombs on Bremen: 48 jettisoned theirs.');
    // "36 of 48 dropped and 41 jettisoned" can never print: a count beyond the bombers that flew is cut to what is left.
    expect(afterWords({ ...w, bombers: 48, dropped: 36, jettisoned: 41, lostBefore: 0 })[0]).toBe('36 of your 48 bombers dropped their bombs on Bremen; 12 jettisoned theirs.');
    // A strike on a fleet at sea: the bombs fell on the fleet, and only the bombers line names it.
    expect(afterWords({ ...w, target: 'the Mobile Force', bombers: 72, dropped: 45, jettisoned: 0, lostBefore: 27 })[0]).toBe('45 of your 72 bombers dropped their bombs on the Mobile Force; 27 were lost before the target.');
  });

  it('a fought raid lands on the campaign exactly as the battle went (b397(3))', () => {
    for (const [nat, kind, name] of [['GER', 'bomb', 'Alsace'], ['UK', 'bomb', 'Bremen']] as [string, RaidKind, string][]) {
      for (const k of [0, 1, 2]) {
        const { g, target, ids, ticket } = raid(nat, kind, name);
        const s = airBattle({ ...ticket, seed: ticket.seed + k }); s.ai.att = true; s.ai.def = true;
        runOut(s);
        const before = new Map(g.airFleets.map((f) => [f.id, { ftr: f.ftr, bmr: f.bmr }]));
        const out = s.outcome();
        const rep = launchRaid(g, kind, target, ids, nat, foughtAir(out));
        expect(rep, `${name} ${k}`).not.toBeNull();
        for (const gr of out.groups) {
          const f = g.airFleets.find((y) => y.id === gr.id)!;
          expect(before.get(gr.id)!.ftr - f.ftr, `${name} ${k} group ${gr.id} fighters`).toBe(gr.lostFtr);
          expect(before.get(gr.id)!.bmr - f.bmr, `${name} ${k} group ${gr.id} bombers`).toBe(gr.lostBmr);
        }
      }
    }
  });
});
