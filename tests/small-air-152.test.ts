// Release 1.5.2: an air arm too small to hold any sky fights over its army (the design director, 25 November 2026,
// 03:54Z, and the raid reading of 04:07Z; airwar.json ai.back.small). Brian, 24 November: "im attacking other nations
// that have airforces but they arent using them". An arm whose land fighters number under air.json sky.least can never
// win a fight for the sky, so with its own ground fought over every fit fighter group in reach flies ground support,
// except those held to meet the enemy's raids on that ground; no staff sends it on a fight for the sky; and a player
// who sends too few fighters on one is told so on his card, and told when nobody came up. Arms at or above sky.least
// are untouched (tests/small-air-identity-152.test.ts). Every rule goes by the size of the force, never by nation.
import { afterEach, describe, expect, it } from 'vitest';
import {
  AIR, AIRWAR, airAI, airFleetsOf, airRange, airWeight, assessAir, cover, defendBattles, fly, foundAirFleet, kmTo, landFighters, meetRaids, smallArm, smallOn,
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
import type { AirFleet, AirReport, Game } from '../src/campaign/types';

const SM = AIRWAR.ai.back!.small!;
afterEach(() => { SM.on = true; });
const esc = (x: string): string => x.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const copy = (g: Game): Game => loadGame(saveGame(g))!;
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
    expect(p.tooFew).toBe(`Too few fighters to hold this sky: it takes ${AIR.sky.least} still over it at the end, and you are sending ${sent > over ? `${sent}, of which only ${over} can be over it` : over}.`);
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
    expect(Q.tooFew).toMatch(/^Too few fighters to hold this sky: it takes 50 still over it at the end, and you are sending 40(\.|, of which only \d+ can be over it\.)$/);
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
    expect(off.rep!.result).toBe('');
    const on = sweep(true);
    const r = on.rep!;
    const who = r.att.length === 1 ? r.att[0].fleet : '';
    expect(who).not.toBe('');
    const there = Math.floor(on.h.airFleets.filter((f) => r.att.some((x) => x.id === f.id)).reduce((s, f) => s + f.ftr, 0) + 1e-9);
    // (The UI director ruled these words at 04:55Z: the same two figures as the plan card's own line.)
    expect(r.result).toMatch(new RegExp(`^${esc(who)} flies over ${esc(r.place)}, but no enemy fighters came up to meet it, and the sky is not won: it takes 50 still over it at the end, and only \\d+ (?:(?:was|were) there|of the \\d+ sent could be over it)\\.$`));
    expect(Number(r.result.match(/and only (\d+)/)![1])).toBeLessThanOrEqual(there);
    const flew = r.result.match(/of the (\d+) sent/);
    if (flew) expect(Number(flew[1])).toBe(there);
    expect(on.news).toEqual([r.result]);
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
    expect(rep!.result).toBe('');
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
  it('in a war nobody plays, every fight for the sky a staff flies has sky.least fighters or more', () => {
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
