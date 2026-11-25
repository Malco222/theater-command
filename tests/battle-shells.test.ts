// Where the shells land (Brian, 18 November: "very dramatic and life like"; the art director's explosions sheet): what
// each block's guns throw, read from the model the 3D game draws it with; a shell for every model a gun or a tank costs a
// block, at that model's place, the same every time the battle is played; the craters they leave, kept to the end; and
// the fight itself, to the last figure, as it was before any of it (208ab5b).
import { describe, expect, it } from 'vitest';
import table from '../unreal/TheaterCommand/Content/Data/Campaign/unit-table.json';
import { battleCalls, unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { groundAt, landTicket, LandBattle, PRACTICE_GROUNDS, practiceTicket, sampleTicket, type LandUnit } from '../src/battle/land';
import { calFor, CRATER, DRAWN, MODEL_CAL, modelOf, REACH, SHELLS_FROM } from '../src/battle/land/shells';
import { runOut } from '../src/battle/core';
import { hashString } from '../src/core/rng';
import type { FieldEvent, Zone } from '../src/battle/core/types';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { attackOutlook, candidates, orderAttack } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { armourYear } from '../src/campaign/tech';

/** The first month of a year, as a campaign turn (turn 1 is September 1939). */
const turnOf = (year: number): number => (year <= 1939 ? 1 : 12 * (year - 1939) - 7);

const plainAttack = (b: LandBattle): void => {
  const mine = b.units.filter((u) => u.side === 'att' && u.kind !== 'fort');
  const cx = mine.reduce((a, u) => a + u.x, 0) / mine.length, cy = mine.reduce((a, u) => a + u.y, 0) / mine.length;
  const foes = b.units.filter((u) => u.side === 'def');
  const near = foes.reduce((m, f) => (Math.hypot(f.x - cx, f.y - cy) < Math.hypot(m.x - cx, m.y - cy) ? f : m), foes[0]);
  expect(b.command({ do: 'attack', units: mine.map((u) => u.id), target: near.id }, 'att')).toBe(true);
};
const shellsOf = (es: FieldEvent[]): FieldEvent[] => es.filter((e) => e.what === 'shell');
const cratersOf = (zs: Zone[]): Zone[] => zs.filter((z) => z.kind === 'crater');
const hurt = (b: LandBattle, u: LandUnit, share: number, by?: LandUnit): void =>
  (b as unknown as { suffer(t: LandUnit, d: number, n: number, from?: [LandUnit, number][]): void }).suffer(u, u.power0 * share, 1, by ? [[by, u.power0 * share]] : []);

describe('what each block throws, by the model the 3D game draws it with', () => {
  type Row = { gun?: { model: string }; tank?: { model: string } };
  const T = table as unknown as { _default: Row; countries: Record<string, Row> };
  /**
   * A row's models as the unit table writes them: names, each after the first with the year it comes in, or the plain
   * medium tank's "once medium tanks are researched", which the campaign gives the year 1942.
   */
  const parse = (s: string): [string, number][] => s.split(', ').map((x) => {
    const [name, rest] = [x.split(' ')[0], x.slice(x.indexOf(' ') + 1)];
    if (!x.includes(' ')) return [name, 0];
    return [name, /^\d{4}$/.test(rest) ? +rest : 1942];
  });
  const pick = (ms: [string, number][], year: number): string => ms.reduce((m, [n, from]) => (year >= from ? n : m), ms[0][0]);

  it('follows unit-table.json, nation by nation and year by year, and every model it names has a class', () => {
    const nats = [...Object.keys(T.countries), 'XXX'];
    for (const nat of nats) {
      const row = T.countries[nat] ?? T._default;
      const gun = row.gun?.model ?? T._default.gun!.model, tank = row.tank?.model ?? T._default.tank!.model;
      for (let year = 1939; year <= 1945; year++) {
        expect(modelOf('art', nat, year), `${nat} guns ${year}`).toBe(pick(parse(gun), year));
        expect(modelOf('arm', nat, year), `${nat} tanks ${year}`).toBe(pick(parse(tank), year));
        expect(MODEL_CAL[modelOf('art', nat, year)!]).toBeDefined();
        expect(MODEL_CAL[modelOf('arm', nat, year)!]).toBeDefined();
      }
    }
    for (const d of Object.values(DRAWN)) for (const m of [d.gun, ...d.tank.map((x) => x[0])]) expect(MODEL_CAL[m], m).toBeDefined();
  });
  it('guns by their bore, tanks by their gun and year; foot divisions their mortars; cavalry, engineers and works none', () => {
    expect(calFor('art', 'GER', 1)).toBe('howitzer');
    expect(calFor('art', 'USA', turnOf(1943))).toBe('howitzer');
    for (const nat of ['UK', 'SOV', 'FRA', 'POL', 'ITA', 'XXX']) expect(calFor('art', nat, 1), nat).toBe('field');
    expect(calFor('arm', 'GER', 1)).toBe('field');
    expect(calFor('arm', 'POL', 1)).toBe('light');
    expect(calFor('arm', 'UK', turnOf(1941))).toBe('light');
    expect(calFor('arm', 'UK', turnOf(1943))).toBe('field');
    expect(calFor('arm', 'SOV', turnOf(1940))).toBe('light');
    expect(calFor('arm', 'SOV', turnOf(1941))).toBe('field');
    expect(calFor('arm', 'XXX', 1)).toBe('light');
    expect(calFor('arm', 'XXX', turnOf(1942))).toBe('field');
    for (const k of ['inf', 'mtn', 'mot'] as const) expect(calFor(k, 'GER', 1)).toBe('mortar');
    for (const k of ['cav', 'eng', 'fort'] as const) expect(calFor(k, 'GER', 1)).toBeUndefined();
    // The blast reaches less far than the camera shakes (the sheet: 10, 25, 30, 40 and 60 m), and grows with the class.
    expect(Object.values(REACH)).toEqual([...Object.values(REACH)].sort((a, b) => a - b));
    expect(REACH.howitzer).toBe(15);
    // A fighter's bomb, the 250 kg row (air support, b533(f)): only a strike drops one, never a block's gun.
    expect(CRATER).toEqual({ light: 0.5, mortar: 0.8, field: 1, howitzer: 1.5, heavy: 3, bomb: 7 });
    expect(REACH.bomb).toBe(45);
  });
  it("a tank throws the gun it is drawn with: its nation's design year where the campaign keeps one, not the calendar (the verifier, explosions)", () => {
    // A Soviet army ahead on armour in 1940 is drawn with the T-34; one held back in 1943 with its light tank still.
    expect(calFor('arm', 'SOV', turnOf(1940), 1941)).toBe(MODEL_CAL.T34);
    expect(calFor('arm', 'SOV', turnOf(1943), 1939)).toBe(MODEL_CAL.LightTank);
    expect(calFor('arm', 'XXX', turnOf(1943), 1939)).toBe(MODEL_CAL.LightTank);
    expect(calFor('arm', 'SOV', turnOf(1943), null)).toBe(calFor('arm', 'SOV', turnOf(1943)));
    // Guns and foot are not drawn by the design year.
    expect(calFor('art', 'USA', 1, 1944)).toBe(calFor('art', 'USA', 1));
    expect(calFor('inf', 'GER', 1, 1944)).toBe('mortar');
    // As campaign-bridge.ts reckons it. The grand campaign: no medium tanks 1939, medium 1942, heavy 1943 or later.
    const g = newCampaign();
    const flags = g.flags.open;
    try {
      g.flags.open = 1;
      const n = g.nations.SOV;
      const tech0 = n.tech;
      n.tech = { done: [], work: [] };
      expect(armourYear(g, 'SOV')).toBe(1939);
      n.tech = { done: ['mediums'], work: [] };
      expect(armourYear(g, 'SOV')).toBe(1942);
      n.tech = { done: ['mediums', 'heavies'], work: [] };
      expect(armourYear(g, 'SOV')).toBe(1943);
      n.tech = tech0;
    } finally { g.flags.open = flags; }
    // Elsewhere by the design teams' clock, where a nation keeps one; else none, and the calendar draws it.
    const lab = Object.values(g.nations).find((x) => x.alive && x.lab);
    if (lab) expect(armourYear(g, lab.id)).toBe(1939 + Math.floor((Math.max(1, g.turn + Math.round(lab.lab!.clock.armour - lab.lab!.months)) + 7) / 12));
    for (const x of Object.values(g.nations)) if (!x.lab && g.flags.open !== 1) expect(armourYear(g, x.id)).toBeNull();
  });
  it('every block carries it to the 3D game, and a block the player has not met carries none (b464(2))', () => {
    const b = new LandBattle(practiceTicket(PRACTICE_GROUNDS[6], { seed: 2, humanAtt: true }));
    for (const u of b.units) {
      expect(u.cal).toBe(calFor(u.kind, u.nat, b.ticket.turn));
      expect(unitOut(u).cal).toBe(u.cal);
    }
    expect(b.units.filter((u) => u.kind === 'art').every((u) => u.cal === 'field')).toBe(true);
    const g = newCampaign();
    const TC = battleCalls({ game: () => g, attack: (ids, to, f) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f) }) });
    const o = offers(g).find((x) => attackOutlook(g, x.corps, x.target)?.exact === false)!;
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id)!;
    const units = st.view.units as { side: string; known?: boolean; kind?: string; cal?: string }[];
    for (const u of units.filter((x) => x.known === false)) expect(u.cal).toBeUndefined();
    expect(units.filter((x) => x.known !== false && (x.kind === 'inf' || x.kind === 'arm')).every((x) => !!x.cal)).toBe(true);
  });
});

describe('where the shells land', () => {
  /**
   * A fight played as the screen plays it, a view after every tick, with every blow watched (never changed): the shells
   * each tick sent, set against the models each block lost that tick and whose they were by the share rule (the design
   * director, b507): each striker's harm is kept on the block it struck, and each model lost goes to the one of that
   * tick's strikers owed most, the first of equals, which pays a model's worth.
   */
  function watch(b: LandBattle): { shells: FieldEvent[]; craters: Zone[][] } {
    const P = LandBattle.prototype as unknown as { blow(u: LandUnit, t: LandUnit): { dmg: number } };
    const blow0 = P.blow;
    let seen: [LandUnit, LandUnit, number][] = [];
    P.blow = function (this: LandBattle, u: LandUnit, t: LandUnit) { const r = blow0.call(this, u, t); seen.push([u, t, r.dmg]); return r; };
    const shells: FieldEvent[] = [], craters: Zone[][] = [];
    const owed = new Map<number, Map<number, number>>();
    try {
      for (let i = 0; i < 20000 && !b.over; i++) {
        const was = new Map(b.units.map((u) => [u.id, { x: u.x, y: u.y, face: u.face, figures: u.figures }]));
        const at = Math.round(b.t * 10) / 10;
        seen = [];
        b.step();
        const v = b.view();
        const now = shellsOf(v.events);
        let due = 0;
        for (const u of b.units) {
          const lost = was.get(u.id)!.figures - u.figures;
          const on = now.filter((e) => e.on === u.id);
          const from = seen.filter((x) => x[1] === u), mine = owed.get(u.id) ?? new Map<number, number>();
          owed.set(u.id, mine);
          for (const [f, , d] of from) mine.set(f.id, (mine.get(f.id) ?? 0) + d);
          if (lost <= 0) { expect(on).toEqual([]); continue; }
          // Each model lost to the striker of this tick owed most, the first of equals; guns' and tanks' land shells.
          const model = u.power0 / (u.tray * u.str0), firers: LandUnit[] = [];
          for (let k = 0; k < lost; k++) {
            const by = from.reduce<LandUnit | null>((m, [f]) => (!m || mine.get(f.id)! > mine.get(m.id)! ? f : m), null)!;
            mine.set(by.id, mine.get(by.id)! - model);
            if (SHELLS_FROM.includes(by.kind)) firers.push(by);
          }
          // One shell for each model a gun or a tank cost it, from that block, of its class, now.
          expect(on.map((e) => e.by)).toEqual(firers.map((f) => f.id));
          due += firers.length;
          const w = was.get(u.id)!, c = Math.cos(w.face), s = Math.sin(w.face);
          for (const [n, e] of on.entries()) {
            const top = firers[n];
            expect([e.by, e.cal, e.t, e.reach, e.text, e.cue]).toEqual([top.id, top.cal, at, REACH[top.cal!], '', undefined]);
            expect(e.unit).toBeUndefined();
            expect(e.armour).toBe(u.kind === 'arm');
            expect(e.ground).toBe(groundAt(b.field, e.x!, e.y!));
            // At the lost model's place within the block's frontage and depth where it stood; a tank's, at her wreck.
            const dx = e.x! - w.x, dy = e.y! - w.y;
            expect(Math.abs(dx * c - dy * s)).toBeLessThanOrEqual(u.w * 0.45 + 0.1);
            expect(Math.abs(dx * s + dy * c)).toBeLessThanOrEqual(u.d * 0.4 + 0.1);
            if (e.armour) expect(b.wrecks.some((z) => z.unit === u.id && z.at === e.t && z.x === e.x && z.y === e.y)).toBe(true);
          }
        }
        expect(now.length).toBe(due);
        shells.push(...now);
        craters.push(cratersOf(v.zones));
      }
    } finally {
      P.blow = blow0;
    }
    expect(b.over).not.toBeNull();
    return { shells, craters };
  }
  const play = (i: number, seed: number): LandBattle => {
    const b = new LandBattle(practiceTicket(PRACTICE_GROUNDS[i], { seed, humanAtt: true }));
    b.ai.def = true;
    plainAttack(b);
    return b;
  };

  it('only from guns and tanks, one for each model they cost a block, at its place, and never in the log', () => {
    let all = 0;
    for (const [i, seed] of [[0, 1], [6, 2], [7, 1]]) {
      const b = play(i, seed);
      const { shells } = watch(b);
      all += shells.length;
      for (const e of shells) {
        expect(SHELLS_FROM).toContain(b.unit(e.by!)!.kind);
        expect(b.unit(e.by!)!.side).not.toBe(b.unit(e.on!)!.side);
      }
      expect(b.log.some((e) => e.what === 'shell')).toBe(false);
    }
    expect(all).toBeGreaterThan(0);
  });
  it('the same battle lands the same shells in the same places, and its craters stay to the end', () => {
    const one = play(6, 2), two = play(6, 2);
    const a = watch(one), b = watch(two);
    expect(a.shells.length).toBeGreaterThan(0);
    expect(b.shells).toEqual(a.shells);
    expect(cratersOf(two.view().zones)).toEqual(cratersOf(one.view().zones));
    // One crater for each shell on the ground, from its second to the end of the fight, never taken away.
    const ground = a.shells.filter((e) => !e.armour && e.ground !== 'water' && e.ground !== 'blown');
    const end = cratersOf(one.view().zones);
    expect(end.map((z) => [z.x, z.y, z.at, z.cal])).toEqual(ground.map((e) => [e.x, e.y, e.t, e.cal]));
    end.forEach((z, k) => {
      expect(z.id).toBe(-(k + 1));
      expect(z.r).toBe(CRATER[z.cal!] / 2);
      expect(z.until).toBe(one.ticket.limit);
    });
    for (let k = 1; k < a.craters.length; k++) expect(a.craters[k].slice(0, a.craters[k - 1].length)).toEqual(a.craters[k - 1]);
    expect(a.craters.at(-1)).toEqual(end);
    // The wrecks keep their own ids, and their own place in the zones, first.
    expect(one.view().zones.slice(0, one.wrecks.length)).toEqual(one.wrecks);
  });
  it('a gun\'s shell on men leaves a crater; one on tanks lands at her wreck, with none; men\'s blows land none', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { seed: 7, humanAtt: true }));
    const gun = b.units.find((u) => u.kind === 'art')!, tank = b.units.find((u) => u.kind === 'arm')!;
    const foot = b.units.find((u) => u.kind === 'inf' && u.side === 'att')!, theirs = b.units.find((u) => u.kind === 'inf' && u.side === 'def')!;
    hurt(b, foot, 0.25, gun);
    const lost = foot.figures0 - foot.figures;
    let es = shellsOf(b.view().events);
    expect(es.length).toBe(lost);
    for (const e of es) expect([e.by, e.on, e.cal, e.armour, e.reach]).toEqual([gun.id, foot.id, gun.cal, false, REACH[gun.cal!]]);
    expect(cratersOf(b.view().zones).length).toBe(lost);
    // Tanks struck by tanks: at each wreck, on armour, and no crater.
    const enemyTank = b.units.find((u) => u.kind === 'arm' && u.side !== tank.side) ?? tank;
    hurt(b, tank, 0.4, enemyTank === tank ? gun : enemyTank);
    es = shellsOf(b.view().events);
    expect(es.length).toBe(b.wrecks.length);
    es.forEach((e, k) => expect([e.x, e.y, e.t, e.armour, e.on]).toEqual([b.wrecks[k].x, b.wrecks[k].y, b.wrecks[k].at, true, tank.id]));
    expect(cratersOf(b.view().zones).length).toBe(lost);
    // Men struck by men: no shell at all.
    hurt(b, foot, 0.25, theirs);
    hurt(b, theirs, 0.25);
    expect(shellsOf(b.view().events)).toEqual([]);
  });
  it('a gun the player has not met lands its shells on his blocks unnamed (b464(2))', () => {
    const f = new LandBattle(sampleTicket('plains', 1.2, { seed: 7, humanAtt: true }));
    f.fog = true;
    const fg = f.units.find((u) => u.kind === 'art')!, ff = f.units.find((u) => u.kind === 'inf' && u.side === 'att')!;
    hurt(f, ff, 0.25, fg);
    const es = shellsOf(f.view().events);
    expect(es.length).toBeGreaterThan(0);
    for (const e of es) { expect(e.by).toBeUndefined(); expect(e.cal).toBe(fg.cal); }
  });
});

describe('the fight is not moved by a hair', () => {
  /**
   * Each fight's whole result and every block's end state, as the tree gave them before shells were landed (c5a9794,
   * the gate pass, with the UI director's 16:37Z words; run out there with no view at all; here with a view after every tick, as the 3D game asks): the result's R, length and men
   * for the reader, and a digest of all of it, to the last figure (two runs of core/rng.ts hashString, 64 bits).
   */
  // Every digest re-recorded at the general's join (22 November 20:10Z) for the after-battle card's words alone (the UI
  // director, 18:26Z: a rout's when and why in one line, the strength line "By the day's end, counting the rout"): each
  // R, length and men the same, viewed and not alike; with the old words every digest is as it was.
  const BEFORE: Record<string, [number, number, number, number, string]> = {
    'practice 0 seed 1': [1.484, 262, 13150, 21612, 'bd251698b75ddc1d'],
    'practice 6 seed 2': [1.539, 222, 9404, 25997, '641b7449be431eb8'],
    'practice 7 seed 5': [1.129, 241, 19498, 14269, '31e0bac3c4c096ca'],
    // Re-recorded at the join with the city glance's rules base (22 November): the campaign gives these corps their own
    // historic divisions and Katowice its town guard (ruling 232), so the tickets differ; viewed and not still alike.
    Pomorze: [5.264, 142, 134, 9946, '0ac731b51ccad330'],
    // Re-recorded on the city's streets (b511: Katowice is a city, fought in its streets now), viewed and not alike; and
    // again with the defence a block inside its core (b529): a shorter fight, one shell fewer; and on Katowice's own
    // street plan (b618: a real city's plan from its name, no longer the battle's seed), its kept plan 3 (b625). And with
    // the tactics panel's item 7 on (b647, ruling 239): one German tank, alone in Katowice's streets at 166.5 s, takes the
    // card's figure; with LAND.alone off it is still aecea9823b67b851, and the shells are as many.
    // Re-recorded with Katowice's town guard drawn on the field as a defending block (ruling 232, b464: the coordinator,
    // 22 November 06:40Z): the same odds, a longer fight, and the guard's men booked with the field's; viewed and not alike.
    'Polish Silesia': [1.865, 233, 4659, 16171, '46c3aefdbb74086a'],
  };
  const check = (key: string, b: LandBattle, viewed: boolean): number => {
    b.ai.def = true;
    plainAttack(b);
    let shells = 0;
    if (viewed) for (let i = 0; i < 1e6 && !b.over; i++) { b.step(); shells += shellsOf(b.view().events).length; }
    else runOut(b);
    const o = b.over!;
    const all = JSON.stringify({ over: o, units: b.units.map((u) => [u.id, u.str, u.mor, u.state, u.figures, u.x, u.y]), wrecks: b.wrecks.length });
    const digest = hashString(all).toString(16).padStart(8, '0') + hashString('#' + all).toString(16).padStart(8, '0');
    expect([o.R, o.seconds, o.men.att, o.men.def, digest], key).toEqual(BEFORE[key]);
    return shells;
  };
  it('practice grounds with guns and tanks: the same R, booked men and every figure as before', () => {
    for (const [i, seed] of [[0, 1], [6, 2], [7, 5]]) {
      const key = `practice ${i} seed ${seed}`;
      check(key, new LandBattle(practiceTicket(PRACTICE_GROUNDS[i], { seed, humanAtt: true })), true);
      check(key, new LandBattle(practiceTicket(PRACTICE_GROUNDS[i], { seed, humanAtt: true })), false);
    }
  });
  it('the campaign\'s own attacks, Pomorze and Polish Silesia (shells from the German tanks): the same as before', () => {
    const g: Game = newCampaign();
    let shells = 0;
    for (const [name, corps] of [['Pomorze', ['XIX Corps', 'XXI Corps']], ['Polish Silesia', ['XVI Corps', 'XV Corps', 'IV Corps']]] as [string, string[]][]) {
      const r = g.regions.find((x) => x.name === name)!;
      const cs = candidates(g, r).filter((c) => corps.includes(c.name));
      shells += check(name, new LandBattle(landTicket(g, cs, r)!), true);
      check(name, new LandBattle(landTicket(g, cs, r)!), false);
    }
    // Katowice on its own kept plan (b618, b625): two shells more than on the battle's plan; and two more with its town
    // guard on the field (ruling 232, b464), the fight in its streets lasting longer.
    expect(shells).toBe(11);
  });
});
