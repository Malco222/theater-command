// Release 1.5.2: an air arm too small to hold any sky fights over its army (the design director, 25 November 2026,
// 03:54Z, and the raid reading of 04:07Z; airwar.json ai.back.small). Brian, 24 November: "im attacking other nations
// that have airforces but they arent using them". An arm whose land fighters number under air.json sky.least can never
// win a fight for the sky, so with its own ground fought over every fit fighter group in reach flies ground support,
// except those held to meet the enemy's raids on that ground; no staff sends it on a fight for the sky; and a player
// who sends too few fighters on one is told so on his card, and told when nobody came up. Arms at or above sky.least
// are untouched (tests/small-air-identity-152.test.ts). Every rule goes by the size of the force, never by nation.
import { afterEach, describe, expect, it } from 'vitest';
import {
  AIR, AIRWAR, SMALL_SEEN, airAI, airFleetsOf, airRange, airWeight, assessAir, brokeOff, cover, defendBattles, fly, foundAirFleet, kmTo, landFighters, meetRaids, smallArm, smallOn,
} from '../src/campaign/air';
import { campaignOrders, openCampaign } from '../src/campaign/aircampaigns';
import { airPage } from '../src/campaign/airpages';
import { launchRaid, planRaid, raidTargets } from '../src/campaign/airwar';
import { setDesk } from '../src/campaign/depth';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, AirReport, BattleReport, Game, PlanPhase } from '../src/campaign/types';

const SM = AIRWAR.ai.back!.small!;
afterEach(() => { SM.on = true; });
const esc = (x: string): string => x.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const copy = (g: Game): Game => loadGame(saveGame(g))!;
/** The groups that flew a raid, as its report's line names them: "A", "A and B", "A, B and C". */
const names = (r: AirReport): string => { const xs = r.att.map((x) => x.fleet); return xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`; };
/** A grand war nobody plays, with the air campaigns on, as the gate plays it. */
const war = (seed: number): Game => { const g = startOpen(createGame(seed, null)); g.flags.ac = 1; return g; };
const landGroups = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat && f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined);
/** The staff's air month for one nation on a copy, with the mend off or on. */
function month(g: Game, nat: string, on: boolean, fix?: (h: Game) => void): Game {
  SM.on = on;
  const h = copy(g);
  fix?.(h);
  airAI(h, nat, null);
  SM.on = true;
  return h;
}
/** An enemy raid since last month on a province, as the reports file it: `n` aircraft from `by`. */
const raidOn = (g: Game, region: number, by: string, on: string, n: number): AirReport => ({
  id: 900000 + g.airReports.length, turn: g.turn, mission: 'ground', region, zone: -1, place: g.regions[region].name, attNat: by, defNat: on,
  att: [{ nat: by, fleet: 'raiders', leader: null, ftr: n, bmr: 0, lostFtr: 0, lostBmr: 0 }], def: [], lines: [], odds: 1, level: 'Close victory', won: true, sky: false, hold: 0, result: '',
}) as AirReport;

describe('release 1.5.2: a small air arm fights over its army (seed 17, a war nobody plays)', () => {
  // The state the tests below share: the month the war comes to a country whose air arm is a single fighter group,
  // with its own ground fought over, where the staff of release 1.5.1 kept that group on patrol.
  let g: Game;
  let nat = '';
  let battles: number[] = [];

  it('the war comes to a country with one fighter group of under sky.least, and release 1.5.1 keeps it home', () => {
    g = war(17);
    for (let t = 0; t < 14 && !nat; t++) {
      runTurnAI(g);
      for (const n of Object.keys(g.nations)) {
        if (!g.nations[n].alive || !g.nations[n].atWar.length || landGroups(g, n).length !== 1 || !smallArm(g, n)) continue;
        const held = defendBattles(g, n);
        if (!held.length) continue;
        const off = month(g, n, false), on = month(g, n, true);
        if (landGroups(off, n).some((f) => f.close) || !landGroups(on, n).some((f) => f.close)) continue;
        nat = n; battles = held;
        break;
      }
    }
    expect(nat).not.toBe('');
    expect(landFighters(g, nat)).toBeGreaterThan(0);
    expect(landFighters(g, nat)).toBeLessThan(AIR.sky.least);
    // The share of release 1.5.1 (0.34 of its fighters) is less than its one group: it never went.
    expect(AIRWAR.ai.back!.close * landFighters(g, nat)).toBeLessThan(landGroups(g, nat)[0].ftr);
  }, 30_000);

  it('switched off, nobody is a small arm and the staff is release 1.5.1\'s: the group stays on patrol', () => {
    SM.on = false;
    expect(smallOn(g)).toBe(false);
    expect(Object.keys(g.nations).some((n) => smallArm(g, n))).toBe(false);
    SM.on = true;
    const off = month(g, nat, false);
    expect(landGroups(off, nat).map((f) => [f.mission, !!f.close])).toEqual([['support', false]]);
  });

  it('switched on, its one group flies ground support over the battles, though no fighter is left on patrol there', () => {
    expect(smallArm(g, nat)).toBe(true);
    const on = month(g, nat, true);
    const [f] = landGroups(on, nat);
    expect(f.close).toBe(true);
    expect(f.mission).toBe('support');
    // (the patrol-first rule of release 1.5.1 would have kept as many on patrol over each battle: here none is)
    expect(landGroups(on, nat).filter((x) => !x.close)).toEqual([]);
  });

  it('a raid on the fought-over ground since last month holds the group to meet it; one only behind the front does not', () => {
    const [f] = landGroups(g, nat);
    const enemy = g.nations[nat].atWar[0];
    // A province of the fought-over ground that it holds and its group guards from where it stands.
    const front = battles.find((b) => g.regions[b].ctrl === nat && cover(kmTo(g, f, b), airRange(f, g).ftr) > 0);
    expect(front).toBeDefined();
    // On the fought-over ground: the army's fight, met by the group that guards it.
    const atFront = month(g, nat, true, (h) => { h.airReports.push(raidOn(h, front!, enemy, nat, 30)); });
    expect(landGroups(atFront, nat)[0].close).toBeFalsy();
    expect(meetRaids(atFront, nat, landGroups(atFront, nat), battles).has(f.id)).toBe(true);
    // Behind the front: a place it holds that is not fought over. The army comes first.
    const behind = g.regions.find((r) => r.ctrl === nat && !battles.includes(r.id) && (r.fac > 0 || r.vp > 0))!;
    expect(behind).toBeTruthy();
    const atHome = month(g, nat, true, (h) => { h.airReports.push(raidOn(h, behind.id, enemy, nat, 30)); });
    expect(landGroups(atHome, nat)[0].close).toBe(true);
  });

  it('it holds back the nearest groups first, only until they at least match the aircraft the enemy sent', () => {
    const enemy = g.nations[nat].atWar[0];
    const [f] = landGroups(g, nat);
    const front = battles.find((b) => g.regions[b].ctrl === nat && cover(kmTo(g, f, b), airRange(f, g).ftr) > 0)!;
    // A second group of 10 fighters on the same field: of two as near, the lower id is held first.
    const second = (h: Game): AirFleet => { const x = foundAirFleet(h, nat, h.regions[f.base], 'Second', 'ftr', 10); x.ftr = 10; x.est = { ftr: 10, bmr: 0 }; return x; };
    const w = airWeight(g, enemy) ?? 1;
    // The enemy sent fewer than the first group's fighters: the first is held, the second flies ground support.
    let id2 = -1;
    const few = month(g, nat, true, (h) => { id2 = second(h).id; h.airReports.push(raidOn(h, front, enemy, nat, Math.floor((f.ftr - 1) / w))); });
    expect(few.airFleets.find((x) => x.id === f.id)!.close).toBeFalsy();
    expect(few.airFleets.find((x) => x.id === id2)!.close).toBe(true);
    // More than the first group's: both are held to meet it.
    const many = month(g, nat, true, (h) => { id2 = second(h).id; h.airReports.push(raidOn(h, front, enemy, nat, Math.ceil((f.ftr + 1) / w))); });
    expect(many.airFleets.find((x) => x.id === f.id)!.close).toBeFalsy();
    expect(many.airFleets.find((x) => x.id === id2)!.close).toBeFalsy();
  });

  it('of two groups on two fields, the one nearer the raided place is held first, whatever their ids', () => {
    // (The case above is a tie: both on one field. Here the second group stands on a field found for it by hand across
    // the Sound, in Scania, farther from Zealand but in its fighters' full reach, and it is given the lower id, so that
    // only the distance can put the nearer one first.)
    const enemy = g.nations[nat].atWar[0];
    const [f] = landGroups(g, nat);
    const zealand = g.byName.Zealand.id, scania = g.byName.Scania.id;
    expect(f.base).toBe(zealand);
    const h = copy(g);
    const far = foundAirFleet(h, nat, h.regions[scania], 'Far', 'ftr', 10);
    far.ftr = 10; far.est = { ftr: 10, bmr: 0 };
    far.id = Math.min(...h.airFleets.map((x) => x.id)) - 1;
    expect(far.id).toBeLessThan(f.id);
    expect(kmTo(h, far, zealand)).toBeGreaterThan(kmTo(h, f, zealand));
    expect(cover(kmTo(h, far, zealand), airRange(far, h).ftr)).toBeGreaterThan(0);
    // A raid on its troops on Zealand smaller than either group: one is enough, and it is the nearer.
    h.airReports.push(raidOn(h, zealand, enemy, nat, 5 / (airWeight(h, enemy) ?? 1)));
    const held = meetRaids(h, nat, landGroups(h, nat), defendBattles(h, nat));
    expect([...held]).toEqual([f.id]);
  });

  // (The design director, 04:55Z: the cause is its own army, not who controls the ground. A raid on its own troops
  // counts wherever they stand, a province an ally controls included. Here Zealand, where its corps stand and its
  // group is based, is set British-controlled, and the enemy raids it with 30 aircraft; a second group of 10 stands
  // on a field found for it by hand in Scania, in reach of Zealand but across the sea from it. With 20 + 10 fighters,
  // a raid of 30 asks for both.)
  const alliedZealand = (mission: AirReport['mission']): { h: Game; near: AirFleet; far: AirFleet; held: Set<number> } => {
    const enemy = g.nations[nat].atWar[0];
    const zealand = g.byName.Zealand.id;
    const h = copy(g);
    h.regions[zealand].ctrl = 'UK';
    const far = foundAirFleet(h, nat, h.regions[g.byName.Scania.id], 'Far', 'ftr', 10);
    far.ftr = 10; far.est = { ftr: 10, bmr: 0 };
    h.airReports.push({ ...raidOn(h, zealand, enemy, 'UK', 30 / (airWeight(h, enemy) ?? 1)), mission });
    const near = landGroups(h, nat).find((x) => x.base === zealand)!;
    return { h, near, far, held: meetRaids(h, nat, landGroups(h, nat), defendBattles(h, nat)) };
  };

  it('a raid on its own troops, or the fieldworks they hold, in a province an ally controls holds its groups to meet it', () => {
    for (const mission of ['ground', 'line'] as const) {
      const { h, near, far, held } = alliedZealand(mission);
      expect(h.regions[g.byName.Zealand.id].ctrl).toBe('UK');
      expect(defendBattles(h, nat)).toContain(g.byName.Zealand.id);
      // (every fighter in reach comes up against a raid on troops, whoever holds the ground: both are held)
      expect([...held].sort()).toEqual([near.id, far.id].sort());
      // and in the staff's month neither flies ground support
      const after = month(h, nat, true);
      expect(after.airFleets.filter((x) => x.id === near.id || x.id === far.id).map((x) => !!x.close)).toEqual([false, false]);
    }
  });

  it('a raid on a place there that is not its troops, the British-held town, is not its army\'s fight: nothing is held', () => {
    const { held, near, h } = alliedZealand('bomb');
    expect(held.size).toBe(0);
    expect(month(h, nat, true).airFleets.find((x) => x.id === near.id)!.close).toBe(true);
  });

  it('a raid on the town\'s defences there holds only a group that would come up over the ally\'s ground, not one that would not', () => {
    // (A raid on a town shakes the troops inside it, so it counts; but it is met only by the groups that guard that
    // ground for whoever holds it, guardsLand with the province's real holder: the group on the British-held field
    // does, the one across the sea in Scania does not, and holding it back would keep it from the army for nothing.)
    const { held, near, far } = alliedZealand('town');
    expect([...held]).toEqual([near.id]);
    expect(held.has(far.id)).toBe(false);
  });

  it('a battle an ally fights with none of its corps in or beside it is not its army\'s fight', () => {
    // (The design director, 04:55Z: a battle an ally defends with none of its corps in it is not its army's fight. In
    // this month Holland and Oslo are fought over and defended by allies, with none of its corps there or next to it;
    // its own battle is for Jutland, beside its corps on Zealand. Set that one aside and only the allies' are left.)
    const mine = (h: Game, id: number): boolean => h.regions[id].units.some((u) => !u.dead && u.nat === nat && u.kind !== 'fort');
    const allyOnly = (h: Game): void => { h.reports = h.reports.filter((r) => r.defNat !== nat); };
    const h0 = copy(g); allyOnly(h0);
    const theirs = defendBattles(h0, nat);
    expect(theirs.length).toBeGreaterThan(0);
    for (const b of theirs) {
      expect(h0.regions[b].ctrl).not.toBe(nat);
      expect(mine(h0, b) || h0.regions[b].nb.some((j) => mine(h0, j))).toBe(false);
    }
    // A heavy raid on the ally's troops there holds nothing, and its own battle (Zealand) is flown over as before.
    const enemy = g.nations[nat].atWar[0];
    const oslo = g.byName.Oslo.id;
    expect(defendBattles(g, nat)).toContain(oslo);
    let seen = null as number[] | null;
    SMALL_SEEN.fn = (_h, n, held) => { if (n === nat) seen = held; };
    try {
      const raided = month(g, nat, true, (h) => { h.airReports.push(raidOn(h, oslo, enemy, h.regions[oslo].ctrl, 500)); });
      expect(seen).toEqual([]);
      expect(landGroups(raided, nat)[0].close).toBe(true);
      // With only the allies' battles fought over, the small-arm rule stands aside: nobody is brought up to them, and
      // the month is the switch-off month to the last group.
      seen = null;
      const on = month(g, nat, true, allyOnly), off = month(g, nat, false, allyOnly);
      expect(seen).toBeNull();
      expect(landGroups(on, nat).map((f) => [f.id, !!f.close, f.base])).toEqual(landGroups(g, nat).map((f) => [f.id, false, f.base]));
      expect(saveGame(on)).toBe(saveGame(off));
    } finally { SMALL_SEEN.fn = undefined; }
  });

  // (The design director, 05:38Z, after the review: "read the battle itself, not the map. A battle is its army's when
  // its own corps take part in it, attacking or defending, plus any battle on its home ground. Where the battle lists
  // its attackers, use that list. A field corps standing next to an enemy province whose battle it is not in is a
  // neighbour, not a fighter. The adjacency test stands in only where the rules do not record who attacked." The
  // staff's air month comes before its army's orders, so this month's blow is not yet on record: last month's is.
  // Here its army is on the attack and nothing of its own is being defended: last month's battle reports are set
  // aside, so the only ground fought over is the attack's.)
  /** The battle its side fought for a province last month, as the reports file it: `by` attacking with corps of `corps`' nations. */
  const battleFor = (h: Game, region: number, by: string, corps: string[], guard = false): BattleReport => ({
    id: 900000 + h.reports.length, turn: h.turn - 1, week: 3, region, regionName: h.regions[region].name, attNat: by, defNat: h.regions[region].ctrl,
    attackers: corps.map((n, i) => ({ name: `${i + 1}. Corps`, nat: n, kind: 'inf', divs: 2, men: 24000, lost: 900, fate: '', ...(guard ? { guard: true } : {}) })),
    defenders: [], lines: [], aPower: 1, dPower: 1, odds: 1, level: 'Close victory', taken: false, attLost: 900, defLost: 900, prisoners: 0,
  }) as BattleReport;
  /** Its staff's month with its army attacking `place`: whether its group flies ground support, and over the place. */
  const flies = (place: string, on: boolean, fix?: (h: Game) => void): boolean => {
    SM.on = on;
    const h = copy(g);
    try {
      h.reports = [];
      fix?.(h);
      airAI(h, nat, { from: 0, to: 9999, posture: 'attack', main: [place] } as PlanPhase);
    } finally { SM.on = true; }
    const [f] = landGroups(h, nat);
    if (f.close) expect(cover(kmTo(h, f, h.byName[place].id), airRange(f, h).ftr)).toBeGreaterThan(0);
    return !!f.close;
  };

  it('attacking, a battle whose report lists its own corps among the attackers is its army\'s fight, and its group flies over it', () => {
    // Schleswig-Holstein: German ground in its fighters' reach, with none of its corps beside it.
    const sh = g.byName['Schleswig-Holstein'].id;
    const [f] = landGroups(g, nat);
    expect(g.nations[nat].atWar).toContain(g.regions[sh].ctrl);
    expect(cover(kmTo(g, f, sh), airRange(f, g).ftr)).toBeGreaterThan(0);
    expect(g.regions[sh].nb.some((j) => g.regions[j].units.some((u) => !u.dead && u.nat === nat))).toBe(false);
    // Its corps attacked it last month, on its own or beside an ally's: its army's fight.
    expect(flies('Schleswig-Holstein', true, (h) => { h.reports.push(battleFor(h, sh, nat, [nat])); })).toBe(true);
    expect(flies('Schleswig-Holstein', true, (h) => { h.reports.push(battleFor(h, sh, 'UK', ['UK', nat])); })).toBe(true);
    // An ally's corps alone attacked it, or only a town guard of its own: not its army's fight (and the ordinary rule's
    // share, 0.34 of its fighters, is less than its one group: it stays home, as in 1.5.1).
    expect(flies('Schleswig-Holstein', true, (h) => { h.reports.push(battleFor(h, sh, 'UK', ['UK'])); })).toBe(false);
    expect(flies('Schleswig-Holstein', true, (h) => { h.reports.push(battleFor(h, sh, nat, [nat], true)); })).toBe(false);
    // A battle older than last month is not read.
    expect(flies('Schleswig-Holstein', true, (h) => { const b = battleFor(h, sh, nat, [nat]); b.turn = h.turn - 2; h.reports.push(b); })).toBe(false);
    // No battle on record and none of its corps beside it: not its army's fight.
    expect(flies('Schleswig-Holstein', true)).toBe(false);
    // Switched off, release 1.5.1 keeps it home whatever the report says.
    expect(flies('Schleswig-Holstein', false, (h) => { h.reports.push(battleFor(h, sh, nat, [nat])); })).toBe(false);
  });

  it('a field corps beside a battle it is not in is a neighbour, not a fighter; beside enemy ground with no battle yet on record, the blow is its own', () => {
    // Jutland, German-held, is set German-owned too, so that it is not its home ground; its corps stand on Zealand
    // beside it.
    const jut = g.byName.Jutland.id;
    const enemy = g.regions[jut].ctrl;
    expect(g.nations[nat].atWar).toContain(enemy);
    const german = (h: Game): void => { h.regions[jut].owner = enemy; };
    expect(g.regions[jut].nb.some((j) => g.regions[j].units.some((u) => !u.dead && u.nat === nat && u.kind !== 'fort'))).toBe(true);
    expect(g.regions[jut].units.some((u) => !u.dead && u.nat === nat)).toBe(false);
    // No battle for it on record yet: the corps beside it stand for the blow about to fall. Its group flies.
    expect(flies('Jutland', true, german)).toBe(true);
    // Its own corps on last month's report: its group flies.
    expect(flies('Jutland', true, (h) => { german(h); h.reports.push(battleFor(h, jut, nat, [nat])); })).toBe(true);
    // Last month's battle for it was an ally's, its own corps not among the attackers: a neighbour, not a fighter.
    expect(flies('Jutland', true, (h) => { german(h); h.reports.push(battleFor(h, jut, 'UK', ['UK'])); })).toBe(false);
    // No corps of its own beside it, and no battle on record: not its army's fight.
    expect(flies('Jutland', true, (h) => { german(h); for (const r of h.regions) for (const u of r.units) if (u.nat === nat) u.dead = true; })).toBe(false);
  });

  it('where no battle is its own army\'s, the staff\'s month is the switch-off month: never less than 1.5.1', () => {
    // (The design director, 05:38Z: "where no battle is its army's, the small-arm rule stands aside and 1.5.1's
    // ordinary staff rule runs, so no case is ever worse than 1.5.1. The rule only adds; it never takes away what
    // shipped." A second group of 5 fighters on Zealand is under the ordinary rule's share, 0.34 of its 25, so 1.5.1
    // sends it on ground support over Jutland, which an ally's corps alone attacked last month.)
    const jut = g.byName.Jutland.id;
    const fix = (h: Game): void => {
      h.regions[jut].owner = h.regions[jut].ctrl;
      const x = foundAirFleet(h, nat, h.regions[g.byName.Zealand.id], 'Small', 'ftr', 5); x.ftr = 5; x.est = { ftr: 5, bmr: 0 };
      h.reports = [battleFor(h, jut, 'UK', ['UK'])];
    };
    const run = (on: boolean): Game => {
      SM.on = on;
      const h = copy(g);
      try { fix(h); airAI(h, nat, { from: 0, to: 9999, posture: 'attack', main: ['Jutland'] } as PlanPhase); } finally { SM.on = true; }
      return h;
    };
    const on = run(true), off = run(false);
    const orders = (h: Game): unknown[] => landGroups(h, nat).map((f) => [f.id, f.ftr, !!f.close, f.base, f.mission]);
    expect(orders(on)).toEqual(orders(off));
    // (1.5.1 sends the small group, so the comparison is not of two empty months)
    expect(landGroups(off, nat).filter((f) => f.close).map((f) => f.ftr)).toEqual([5]);
    expect(saveGame(on)).toBe(saveGame(off));
  });

  it('binds the player\'s own staff the same: a player of this country who leaves his staff the air war gets the same orders', () => {
    const computer = month(g, nat, true);
    const player = month(g, nat, true, (h) => { h.human = nat; expect(setDesk(h, 'air', true)).toBe(true); });
    expect(landGroups(player, nat).map((f) => [f.id, !!f.close, f.base])).toEqual(landGroups(computer, nat).map((f) => [f.id, !!f.close, f.base]));
    expect(landGroups(player, nat).every((f) => f.close)).toBe(true);
    // Flying his own air war, he orders it himself: the staff touches nothing.
    const byHand = month(g, nat, true, (h) => { h.human = nat; });
    expect(landGroups(byHand, nat).map((f) => [f.id, f.close, f.base, f.mission])).toEqual(landGroups(g, nat).map((f) => [f.id, f.close, f.base, f.mission]));
  });

  it('his raid card says when the fighters he sends on a fight for the sky are too few to hold it, and words the chance for the fight', () => {
    const h = copy(g);
    h.human = nat;
    const t = raidTargets(h, 'superiority', nat)[0];
    expect(t).toBeTruthy();
    const p = planRaid(h, 'superiority', t.id, undefined, nat);
    expect(p.why).toBeNull();
    const a = assessAir(h, p.picked.map((id) => h.airFleets.find((x) => x.id === id)!), 'superiority', t.id);
    expect(a.E).toBeLessThan(AIR.sky.least);
    const sent = Math.round(p.fighters), over = Math.floor(a.E);
    expect(p.effects[0]).toMatch(/^Fighter odds /);
    expect(p.effects.some((e) => e.startsWith('Too few'))).toBe(false);
    // (The UI director ruled these words at 04:55Z: the short line where every fighter sent can be over the target.)
    expect(p.tooFew).toBe(`Too few fighters to hold this sky: it takes at least ${AIR.sky.least} fighters still over the target at the end, and you are sending ${sent > over ? `${sent}, of which only ${over} can be over it` : over}.`);
    expect(['Very likely to win the air fight', 'Likely to win the air fight', 'The air fight could go either way', 'Likely to lose the air fight', 'Very likely to lose the air fight']).toContain(p.verdict);
    // The bridge's page for the plan carries both: the headline and the note.
    const page = airPage(h, `plan:superiority:${t.id}:${p.picked.join('.')}`)!;
    expect(page.headline).toContain(p.verdict);
    expect(page.note).toContain(p.tooFew!);
    // Switched off, the card is release 1.5.1's.
    SM.on = false;
    const q = planRaid(h, 'superiority', t.id, undefined, nat);
    SM.on = true;
    expect(q.effects[0]).toMatch(/^Fighter odds /);
    expect(q.tooFew).toBeUndefined();
    expect(['Very likely to succeed', 'Likely to succeed', 'It could go either way', 'Likely to fail', 'Very likely to fail']).toContain(q.verdict);
    expect(q.success).toBe(p.success);
  });

  it('a player who sends sky.least fighters or more is told nothing new; with fewer, whoever he is, he is', () => {
    const h = copy(g);
    h.human = 'GER';
    expect(landFighters(h, 'GER')).toBeGreaterThanOrEqual(AIR.sky.least);
    const fleets = (ids: number[]): AirFleet[] => ids.map((id) => h.airFleets.find((x) => x.id === id)!);
    let big: { t: number; ids: number[] } | null = null;
    for (const t of raidTargets(h, 'superiority', 'GER')) {
      const ids = planRaid(h, 'superiority', t.id, undefined, 'GER').seats.filter((s) => !s.why && s.kind === 'ftr').map((s) => s.id);
      if (ids.length && assessAir(h, fleets(ids), 'superiority', t.id).E >= AIR.sky.least) { big = { t: t.id, ids }; break; }
    }
    expect(big).not.toBeNull();
    const P = planRaid(h, 'superiority', big!.t, big!.ids, 'GER');
    expect(P.why).toBeNull();
    expect(P.tooFew).toBeUndefined();
    expect(P.effects[0]).toMatch(/^Fighter odds /);
    expect(['Very likely to succeed', 'Likely to succeed', 'It could go either way', 'Likely to fail', 'Very likely to fail']).toContain(P.verdict);
    // The same air force sending one group cut down to 40 fighters: the card says so, great power or not.
    const one = fleets([big!.ids[0]])[0];
    one.ftr = 40;
    const Q = planRaid(h, 'superiority', big!.t, [one.id], 'GER');
    expect(Q.why).toBeNull();
    expect(Q.tooFew).toMatch(/^Too few fighters to hold this sky: it takes at least 50 fighters still over the target at the end, and you are sending 40(\.|, of which only \d+ can be over it\.)$/);
  });

  it('his own fight for the sky that nobody came up to meet is told to him in one plain sentence', () => {
    const sweep = (on: boolean): { h: Game; rep: AirReport | null; news: string[] } => {
      SM.on = on;
      const h = copy(g);
      h.human = nat;
      const t = raidTargets(h, 'superiority', nat)[0];
      // (the enemy's fighter groups stand down, resting: nobody comes up)
      for (const e of h.airFleets) if (e.nat !== nat && e.ftr > 0 && h.nations[nat].atWar.includes(e.nat)) e.mission = 'rest';
      const p = planRaid(h, 'superiority', t.id, undefined, nat);
      expect(p.meets).toEqual([]);
      const n0 = h.headlines.length;
      const rep = launchRaid(h, 'superiority', t.id, p.picked, nat);
      SM.on = true;
      return { h, rep, news: h.headlines.slice(n0).map((x) => x.text) };
    };
    const off = sweep(false);
    expect(off.rep).not.toBeNull();
    expect(off.rep!.def).toEqual([]);
    expect(off.news).toEqual([]);
    // (With the switch off, his sweep that met nobody has the UI director's line of 05:45Z, findings 8 and 30 of 1.5.1.)
    expect(off.rep!.result).toBe(`${names(off.rep!)} flew over ${off.rep!.place} and met no enemy fighters.`);
    const on = sweep(true);
    const r = on.rep!;
    const who = r.att.length === 1 ? r.att[0].fleet : '';
    expect(who).not.toBe('');
    const there = Math.floor(on.h.airFleets.filter((f) => r.att.some((x) => x.id === f.id)).reduce((s, f) => s + f.ftr, 0) + 1e-9);
    // (The UI director ruled these words at 04:55Z: the same two figures as the plan card's own line.)
    expect(r.result).toMatch(new RegExp(`^${esc(who)} flies over ${esc(r.place)}, but no enemy fighters came up to meet it, and the sky is not won: it takes at least 50 fighters still over the target at the end, and only \\d+ (?:(?:was|were) there|of the \\d+ sent could be over it)\\.$`));
    expect(Number(r.result.match(/and only (\d+)/)![1])).toBeLessThanOrEqual(there);
    // Where fewer could be over the target than were sent, the line gives the fighters sent; where it does not, every
    // fighter sent was over it, and the one figure is theirs.
    const flew = r.result.match(/of the (\d+) sent/);
    expect(Number(flew ? flew[1] : r.result.match(/and only (\d+) (?:was|were) there/)![1])).toBe(there);
    expect(on.news).toEqual([r.result]);
    // Nobody met anybody, so the news item is set out as neither side's: no won, no lost.
    const item = on.h.headlines[on.h.headlines.length - 1];
    expect(item.text).toBe(r.result);
    expect(item.arm).toBe('air');
    expect(item.won).toBeUndefined();
    expect(item.lost).toBeUndefined();
  });

  it('his own fight for the sky that the enemy did come up to meet is still set out in the news as won and lost', () => {
    const h = copy(g);
    h.human = nat;
    const t = raidTargets(h, 'superiority', nat).find((x) => planRaid(h, 'superiority', x.id, undefined, nat).meets.length > 0)!;
    expect(t).toBeTruthy();
    const p = planRaid(h, 'superiority', t.id, undefined, nat);
    const n0 = h.headlines.length;
    const rep = launchRaid(h, 'superiority', t.id, p.picked, nat)!;
    expect(rep.def.length).toBeGreaterThan(0);
    const lost = rep.att.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0) + rep.def.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
    expect(lost).toBeGreaterThan(0);
    const items = h.headlines.slice(n0);
    expect(items.length).toBe(1);
    expect([items[0].won, items[0].lost].sort()).toEqual([nat, rep.defNat].sort());
  });

  it('the report of a fight for the sky broken off before it met the enemy reads as broken off, and an ordinary one does not', () => {
    // (The browser's raid report card, ui/main.ts airReportCard, gives "the raid was broken off" as the reason a sky
    // was not won where brokeOff reads so. The card itself is drawn inside the browser client and cannot be reached
    // from here; brokeOff, the one thing it reads for it, can.)
    const t = raidTargets(g, 'superiority', nat).find((x) => planRaid(g, 'superiority', x.id, undefined, nat).meets.length > 0)!;
    const sweep = (gaveUp?: 'att'): AirReport => {
      const h = copy(g);
      h.human = nat;
      const p = planRaid(h, 'superiority', t.id, undefined, nat);
      return fly(h, nat, 'superiority', t.id, p.picked.map((id) => h.airFleets.find((x) => x.id === id)!), ':own', gaveUp ? { lost: {}, dropped: 0, gaveUp } : undefined, true)!;
    };
    const broke = sweep('att');
    expect(broke.result).toBe('The fighters broke off and turned for home before they met the enemy.');
    expect(broke.sky).toBe(false);
    expect(brokeOff(broke)).toBe(true);
    const fought = sweep();
    expect(fought.def.length).toBeGreaterThan(0);
    expect(brokeOff(fought)).toBe(false);
  });

  it('a fight for the sky with only bombers sent is a raid on the airfields: no too-few line, and the chance in the ordinary words', () => {
    const h = copy(g);
    h.human = 'GER';
    let p = null as null | ReturnType<typeof planRaid>;
    for (const t of raidTargets(h, 'superiority', 'GER')) {
      const bmr = planRaid(h, 'superiority', t.id, undefined, 'GER').seats.find((s) => !s.why && s.kind === 'bmr' && h.airFleets.find((x) => x.id === s.id)!.ftr === 0);
      if (bmr) { p = planRaid(h, 'superiority', t.id, [bmr.id], 'GER'); break; }
    }
    expect(p).not.toBeNull();
    expect(p!.why).toBeNull();
    expect(p!.fighters).toBe(0);
    expect(p!.tooFew).toBeUndefined();
    expect(['Very likely to succeed', 'Likely to succeed', 'It could go either way', 'Likely to fail', 'Very likely to fail']).toContain(p!.verdict);
  });

  it('with every enemy fighter group resting, his card says no fighters will come up, and still that the sky cannot be held', () => {
    const h = copy(g);
    h.human = nat;
    for (const e of h.airFleets) if (e.nat !== nat && e.ftr > 0 && h.nations[nat].atWar.includes(e.nat)) e.mission = 'rest';
    const t = raidTargets(h, 'superiority', nat)[0];
    const p = planRaid(h, 'superiority', t.id, undefined, nat);
    expect(p.why).toBeNull();
    expect(p.meets).toEqual([]);
    expect(p.fighters).toBeGreaterThan(0);
    expect(p.verdict).toBe('No fighters will come up');
    expect(p.tooFew).toMatch(/^Too few fighters to hold this sky: it takes at least 50 fighters still over the target at the end, and you are sending \d+(\.|, of which only \d+ can be over it\.)$/);
  });

  it('a staff\'s fight for the sky that met nobody is still not news', () => {
    const h = copy(g);
    h.human = nat;
    const t = raidTargets(h, 'superiority', nat)[0];
    for (const e of h.airFleets) if (e.nat !== nat && e.ftr > 0 && h.nations[nat].atWar.includes(e.nat)) e.mission = 'rest';
    const p = planRaid(h, 'superiority', t.id, undefined, nat);
    const n0 = h.headlines.length;
    // (flown as a staff flies it, not as his own order)
    const rep = fly(h, nat, 'superiority', t.id, p.picked.map((id) => h.airFleets.find((x) => x.id === id)!), ':staff0');
    expect(rep).not.toBeNull();
    expect(rep!.def).toEqual([]);
    expect(h.headlines.length).toBe(n0);
    // (It is still no headline; but the report, his, says in the UI director's words of 05:45Z that nobody came up.)
    expect(rep!.result).toBe(`${names(rep!)} flew over ${rep!.place} and met no enemy fighters.`);
  });

  it('his staff, holding his air war, does not fly his support campaign as a fight for the sky with too few fighters', () => {
    const orders = (on: boolean): string[] => {
      SM.on = on;
      const h = copy(g);
      h.human = nat;
      expect(setDesk(h, 'air', true)).toBe(true);
      const [f] = landGroups(h, nat);
      f.close = undefined;
      const t = raidTargets(h, 'superiority', nat)[0];
      expect(openCampaign(h, nat, 'support', [t.id], [f.id])).not.toBeNull();
      campaignOrders(h, nat);
      SM.on = true;
      return landGroups(h, nat).map((x) => x.mission);
    };
    expect(orders(false)).toEqual(['superiority']);
    expect(orders(true)).toEqual(['support']);
  });

  it('a saved war from before the mend loads and plays on with it', () => {
    SM.on = false;
    const h = copy(g);
    runTurnAI(h);
    const old = saveGame(h);
    SM.on = true;
    const back = loadGame(old)!;
    expect(saveGame(back)).toBe(old);
    runTurnAI(back);
    runTurnAI(back);
    expect(back.turn).toBe(h.turn + 2);
    const again = loadGame(saveGame(back))!;
    expect(saveGame(again)).toBe(saveGame(back));
  }, 30_000);
});

describe('release 1.5.2: no staff sends an arm under sky.least on a fight for the sky', () => {
  // (A guard against going back, not a proof of the mend: release 1.5.1's staffs already hold to this, since a staff's
  // own sweeps go only where the fighters over the target can win it, air.ts raidStaff. Wars nobody plays were looked
  // through with the mend off for a small arm's fight for the sky, and none was found: seeds 23, 5 and 17 for 16
  // months, with and without the historian's campaigns (setStop 1), and seeds 1, 2, 3 and 7 for 24 months with them.
  // The mend's own case, the historian's campaign flown by an arm too small, is the next test, built by hand.)
  it('regression guard, holding in 1.5.1 too: in a war nobody plays, every fight for the sky a staff flies has sky.least fighters or more', () => {
    const g = war(23);
    let sweeps = 0;
    for (let t = 0; t < 16; t++) {
      const n0 = g.airReports.length;
      runTurnAI(g);
      for (const r of g.airReports.slice(n0)) {
        if (r.mission !== 'superiority') continue;
        sweeps++;
        expect(r.att.reduce((s, x) => s + x.ftr, 0)).toBeGreaterThanOrEqual(AIR.sky.least);
      }
    }
    expect(sweeps).toBeGreaterThan(0);
  }, 30_000);

  it('a campaign of the historian\'s for the sky is not flown by an arm too small to hold one', () => {
    // By the book (stop 1): the Luftwaffe's battle for the sky over southern England opens in its month.
    const g = war(5);
    expect(setStop(g, 1, true)).toBe(true);
    let c = null as null | { id: number; groups: number[] };
    for (let t = 0; t < 14 && !c; t++) {
      runTurnAI(g);
      c = g.ac?.campaigns.find((x) => x.nat === 'GER' && x.kind === 'sky' && x.book !== undefined && x.off === undefined && x.targets.length > 0) ?? null;
    }
    expect(c).not.toBeNull();
    const shrink = (h: Game): void => {
      // (the same war, with Germany's fighters cut to one each in the campaign's groups and none elsewhere)
      for (const f of airFleetsOf(h, 'GER')) if (f.kind === 'ftr') f.ftr = c!.groups.includes(f.id) ? 1 : 0;
      for (const f of airFleetsOf(h, 'GER')) if (f.mission === 'superiority') { f.mission = 'support'; f.target = -1; }
    };
    const orders = (on: boolean): string[] => {
      SM.on = on;
      const h = copy(g);
      shrink(h);
      expect(landFighters(h, 'GER')).toBeLessThan(AIR.sky.least);
      campaignOrders(h, 'GER');
      SM.on = true;
      return airFleetsOf(h, 'GER').filter((f) => f.mission === 'superiority').map((f) => f.name);
    };
    expect(orders(false).length).toBeGreaterThan(0);
    expect(orders(true)).toEqual([]);
  }, 30_000);
});
