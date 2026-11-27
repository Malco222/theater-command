// Ruling 240 (Brian, 21 November 2026, "yes to all 3"; release 2): ships' anti-aircraft strength grows with research,
// proximity shells and damage control are naval researches. Every nation alike: the rules read research only, never a
// nation, a date, which side attacks or who the player is. Each rule has its own switch in carriers.json 'research',
// read every time it is asked, and ALL THREE ARE OFF as shipped: with them off the game is release 1 to the byte,
// and the two new rows of the tree are shelved (no staff picks them, nobody can order them, no card shows them).
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { CARRIERS, assessAir, type AirAssess } from '../src/campaign/air';
import { planRaid } from '../src/campaign/airwar';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { commandView } from '../src/campaign/desks';
import { createGame, setWar } from '../src/campaign/game';
import { booksOf } from '../src/campaign/intel';
import { atSea, sail, zoneOf } from '../src/campaign/navy';
import { PLANS } from '../src/campaign/plans';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  SHIP_RESEARCH, TECH, dcOf, flakMul, grantTech, hasTech, idleTeams, shelved, shipKit, shipRule, staffPick, startResearch, techById, techOf, techRule,
  techTick, techView, whyNotResearch,
} from '../src/campaign/tech';
import { beginHuman, endHuman, humanUp, runTurnAI, stepAI } from '../src/campaign/turn';
import type { AirFleet, Fleet, Game } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const history = (seed = 3): Game => createGame(seed, null);

// The switches are flipped in memory and put back after every test (as tests/wings-abroad.test.ts does with its own).
const R = SHIP_RESEARCH()!;
let was = { stage: false, vt: false, dc: false };
beforeEach(() => { was = { stage: R.stage.on, vt: R.vt.on, dc: R.dc.on }; });
afterEach(() => { R.stage.on = was.stage; R.vt.on = was.vt; R.dc.on = was.dc; });
const set = (stage: boolean, vt: boolean, dc: boolean): void => { R.stage.on = stage; R.vt.on = vt; R.dc.on = dc; };

const FOUR = ['firecontrol', 'taskforces', 'proxfuze', 'damagecontrol'];
/** Puts researches straight onto a nation's list, whatever the switches say (a save made with them on holds the ids). */
const give = (g: Game, nat: string, ...ids: string[]): void => { const s = techOf(g, nat)!; for (const id of ids) if (!s.done.includes(id)) s.done.push(id); };
/** Everything a staff could take up but these: what is left is what the test is about. */
const allBut = (g: Game, nat: string, ...keep: string[]): void => { for (const t of TECH.techs) if (t.branch !== 'intel' && !keep.includes(t.id)) give(g, nat, t.id); };

/** A nation's list without any of the four researches, whatever it began with or its staff has done since. */
const strip = (g: Game, nat: string): void => { const s = techOf(g, nat)!; s.done = s.done.filter((id) => !FOUR.includes(id)); };
/** The nations that hold a research, and those with a team at work on it. */
const holding = (g: Game, id: string): string[] => Object.values(g.nations).filter((n) => n.tech?.done.includes(id)).map((n) => n.id);
const workingOn = (g: Game, id: string): string[] => Object.values(g.nations).filter((n) => n.tech?.work.some((w) => w.id === id)).map((n) => n.id);
/**
 * The names under which a save would write a number that is not one (NaN or an infinity, which a save writes as
 * null): none, in a game whose rules all came out as numbers.
 */
function notNumbers(g: Game): string[] {
  const real = JSON.stringify, out: string[] = [];
  JSON.stringify = ((v: unknown) => real(v, (k: string, x: unknown) => { if (typeof x === 'number' && !Number.isFinite(x)) out.push(k); return x; })) as typeof JSON.stringify;
  try { saveGame(g); } finally { JSON.stringify = real; }
  return out;
}
/**
 * A staff-fought war of thirty months (seed 23), played once for each setting asked for and kept: its save. `on` false
 * is the game as shipped, every switch off; true has all three on from the first month. The switches are left as the
 * caller had them. (Seed 17 until the computer's air forces fought back, airwar.json ai.back: in its war no staff still
 * had a team on Damage control at month 30.)
 */
const wars: { off?: string; on?: string } = {};
function war(on: boolean): string {
  const key = on ? 'on' : 'off';
  if (wars[key] === undefined) {
    const kept = [R.stage.on, R.vt.on, R.dc.on] as const;
    set(on, on, on);
    try {
      const g = grand(null, 23);
      while (g.turn < 30) runTurnAI(g);
      wars[key] = saveGame(g);
    } finally { set(...kept); }
  }
  return wars[key]!;
}
/** What a navy's guns are from the lists as a save holds them, read here without the rule: its own or its senior partner's. */
function stageFrom(g: Game, nat: string): 'early' | 'mid' | 'late' {
  const lists = [nat, PLANS.kin[nat]].filter((n) => n !== undefined && g.nations[n]).map((n) => g.nations[n].tech?.done ?? []);
  return lists.some((l) => l.includes('taskforces')) ? 'late' : lists.some((l) => l.includes('firecontrol')) ? 'mid' : 'early';
}

/** Release 1's tree, counted at fcfb7711. */
const R1 = { total: 51, naval: 8 };
/** Release 1's words on the two cards that set the stage of the guns. */
const OLD_CARD = 'Fleets, raiders and convoy escorts fight 8% harder.';

describe('ruling 240, the data', () => {
  it('ships with all three switches off, at the approved numbers', () => {
    expect(R).toEqual({ stage: { on: false, mid: 'firecontrol', late: 'taskforces' }, vt: { on: false, tech: 'proxfuze' }, dc: { on: false, tech: 'damagecontrol' } });
    expect(CARRIERS.research).toBe(R);
    expect(CARRIERS.flak.stage).toEqual({ early: 0.6, mid: 1, late: 1.5 });
    expect(CARRIERS.flak.vt).toBe(1.3);
    expect(CARRIERS.strike.dc).toBe(1.3);
    const about = (CARRIERS as unknown as { _about: Record<string, string> })._about;
    for (const k of ['research', 'flak', 'strike']) expect(about[k].length, k).toBeGreaterThan(60);
    expect(about.research).toMatch(/OFF as shipped/);
  });

  it('has the four researches in the tree, the two new ones marked with their switch and named in no staff\'s table', () => {
    for (const id of FOUR) expect(techById(id), id).toBeDefined();
    expect(techById('proxfuze')).toMatchObject({ branch: 'naval', tier: 4, year: 1943, needs: ['firecontrol'], name: 'Proximity shells', ships: 'vt' });
    expect(techById('damagecontrol')).toMatchObject({ branch: 'naval', tier: 2, year: 1941, needs: [], name: 'Damage control', ships: 'dc' });
    expect(techById('firecontrol')!.ships).toBeUndefined();
    expect(techById('taskforces')!.ships).toBeUndefined();
    expect(TECH.techs.filter((t) => t.ships !== undefined).map((t) => t.id).sort()).toEqual(['damagecontrol', 'proxfuze']);
    // Every nation alike: no start row, no skip list and no plans table names them.
    for (const id of ['proxfuze', 'damagecontrol']) {
      for (const [nat, ids] of Object.entries(TECH.start)) expect(ids, `start ${nat}`).not.toContain(id);
      for (const [nat, ids] of Object.entries(TECH.ai.skip)) expect(ids, `skip ${nat}`).not.toContain(id);
      expect(PLANS.techs as string[]).not.toContain(id);
    }
  });

  it('says what the two new researches are and do, in the UI director\'s words, with the numbers from the data', () => {
    expect(techById('proxfuze')!.text).toBe('A tiny radio set in the nose of each anti-aircraft shell bursts it as it passes an aircraft, so the gunners no longer need to set the fuze just right.');
    expect(techById('damagecontrol')!.text).toBe('Repair parties, fog nozzles, portable pumps and fuel lines drained before battle: a ship that is hit is far more often saved.');
    // (With no game and whatever the switches say: tests/tech.test.ts asks every row for its rule that way.)
    for (const on of [false, true]) {
      set(on, on, on);
      expect(techRule(techById('proxfuze')!)).toBe("Ships' guns bring down 30% more of the aircraft that attack them.");
      expect(techRule(techById('damagecontrol')!)).toBe('Ships take 30% more hits to sink.');
    }
    // The figure is the data's, not the sentence's own.
    const vt = CARRIERS.flak.vt, dc = CARRIERS.strike.dc;
    try {
      CARRIERS.flak.vt = 1.25; CARRIERS.strike.dc = 1.4;
      expect(techRule(techById('proxfuze')!)).toBe("Ships' guns bring down 25% more of the aircraft that attack them.");
      expect(techRule(techById('damagecontrol')!)).toBe('Ships take 40% more hits to sink.');
    } finally { CARRIERS.flak.vt = vt; CARRIERS.strike.dc = dc; }
  });
});

describe('ruling 240, the switches off (as shipped): release 1', () => {
  it('shows neither row on the tree, counts neither, and lets nobody order or be given one', () => {
    const g = grand('GER');
    for (const nat of ['GER', 'UK', 'SWE', 'CAN']) {
      const v = techView(g, nat)!;
      expect(v.nodes.some((x) => x.id === 'proxfuze' || x.id === 'damagecontrol'), nat).toBe(false);
      expect(v.total, nat).toBe(R1.total);
      expect(v.nodes.length, nat).toBe(R1.total);
      expect(v.branches.find((b) => b.id === 'naval')!.total, nat).toBe(R1.naval);
      expect(v.branches.map((b) => b.total).reduce((a, b) => a + b, 0), nat).toBe(R1.total);
    }
    for (const id of ['proxfuze', 'damagecontrol']) {
      expect(shelved(techById(id)), id).toBe(true);
      expect(whyNotResearch(g, 'GER', id), id).toBe('There is no such technology.');
      expect(startResearch(g, 'GER', id), id).toBe(false);
      expect(grantTech(g, 'GER', id), id).toBe(false);
      expect(hasTech(g, 'GER', id), id).toBe(false);
    }
    for (const id of ['firecontrol', 'taskforces']) expect(shelved(techById(id)), id).toBe(false);
  });

  it('is never picked by a staff, even when nothing else is left', () => {
    const g = grand();
    for (const nat of ['GER', 'UK', 'USA', 'SWE', 'CAN']) {
      allBut(g, nat, 'proxfuze', 'damagecontrol');
      expect(staffPick(g, nat, 60), nat).toBeNull();
    }
    // The player is not told a team of his has work it could begin.
    const mine = grand('GER');
    allBut(mine, 'GER', 'proxfuze', 'damagecontrol');
    expect(idleTeams(mine, 'GER')).toBe(0);
    set(false, false, true);
    expect(idleTeams(mine, 'GER')).toBeGreaterThan(0);
  });

  it('is held and worked on by no staff after thirty months of a staff-fought war', () => {
    const text = war(false);
    const g = loadGame(text)!;
    expect(g.turn).toBe(30);
    for (const n of Object.values(g.nations)) {
      for (const id of ['proxfuze', 'damagecontrol']) {
        expect(n.tech?.done ?? [], `${n.id} holds ${id}`).not.toContain(id);
        expect((n.tech?.work ?? []).map((w) => w.id), `${n.id} works on ${id}`).not.toContain(id);
      }
    }
    expect(text).not.toMatch(/proxfuze|damagecontrol/);
    expect(saveGame(g) === text).toBe(true);
  }, 240000);

  it('reads no kit at all, even for a navy whose list holds all four researches', () => {
    const g = grand();
    for (const nat of ['UK', 'SWE', 'CAN']) {
      give(g, nat, ...FOUR);
      expect(shipKit(g, nat), nat).toEqual({ stage: null, vt: false, dc: false });
      expect(flakMul(g, nat), nat).toBe(1);
      expect(dcOf(g, nat), nat).toBe(1);
    }
    for (const k of ['stage', 'vt', 'dc'] as const) expect(shipRule(g, k), k).toBe(false);
  });

  it('asks the switch before the research: a campaign whose tree is not yet open is not opened, and nothing is written', () => {
    const g = createGame(3, null);
    g.flags.open = 1;
    const before = saveGame(g);
    expect(shipKit(g, 'UK')).toEqual({ stage: null, vt: false, dc: false });
    expect(flakMul(g, 'UK')).toBe(1);
    expect(dcOf(g, 'CAN')).toBe(1);
    expect(g.flags['tech:open']).toBeUndefined();
    expect(saveGame(g)).toBe(before);
  });

  it('leaves the two older cards as they were', () => {
    expect(techRule(techById('firecontrol')!)).toBe(OLD_CARD);
    expect(techRule(techById('taskforces')!)).toBe(OLD_CARD);
    const g = grand('UK');
    expect(techRule(techById('firecontrol')!, g, 'UK')).toBe(OLD_CARD);
    expect(techView(g, 'UK')!.nodes.find((x) => x.id === 'taskforces')!.rule).toBe(OLD_CARD);
  });

  it('frees a team found at work on a shelved row (a save made with the switch on), and announces nothing', () => {
    const g = grand('GER');
    set(false, false, true);
    expect(startResearch(g, 'GER', 'damagecontrol')).toBe(true);
    expect(techView(g, 'GER')!.busy).toBe(1);
    // The research desk's line and the books a spy would read (intel.ts): with the switch on they name the row.
    const desk = (): string => commandView(g)!.desks.find((d) => d.id === 'research')!.state;
    const books = (): { research: { name: string; left: number }[]; researched: number } => booksOf(g, 'GER', 'GER')!;
    const held = techOf(g, 'GER')!.done.length;
    expect(held).toBe(TECH.start.GER.length);
    expect(desk()).toMatch(/^1 of 2 teams at work: Damage control \(\d+ months\)\.$/);
    expect(books().research.map((x) => x.name)).toEqual(['Damage control']);
    expect(books().researched).toBe(held);
    const text = saveGame(g);
    set(false, false, false);
    // The save still holds the id; the tree does not show it, and the team counts as free.
    expect(text).toMatch(/damagecontrol/);
    const v = techView(g, 'GER')!;
    expect(v.busy).toBe(0);
    expect(v.free).toBe(v.teams);
    expect(v.total).toBe(R1.total);
    // Nor does the desk's line name it or count its team, nor the books list it: both read as release 1's.
    expect(desk()).toBe('No research team has work.');
    expect(books().research).toEqual([]);
    expect(books().researched).toBe(held);
    // (A team on a row of release 1's tree beside it is still counted, and alone.)
    expect(startResearch(g, 'GER', 'antitank')).toBe(true);
    expect(desk()).toMatch(/^1 of 2 teams at work: [^,]+ \(\d+ months\)\.$/);
    expect(desk()).not.toMatch(/Damage control/);
    expect(books().research).toHaveLength(1);
    expect(books().research[0].name).not.toBe('Damage control');
    expect(techView(g, 'GER')!.busy).toBe(1);
    techOf(g, 'GER')!.work = techOf(g, 'GER')!.work.filter((w) => w.id !== 'antitank');
    const news = g.headlines.length;
    for (let k = 0; k < 20; k++) techTick(g, 'GER');
    expect(techOf(g, 'GER')!.work.map((w) => w.id)).not.toContain('damagecontrol');
    expect(techOf(g, 'GER')!.done).not.toContain('damagecontrol');
    expect(g.headlines.slice(news).map((h) => h.text).join(' ')).not.toMatch(/Damage control/);
    // A list that holds the id from such a save is not counted either.
    give(g, 'GER', 'damagecontrol', 'proxfuze');
    expect(techView(g, 'GER')!.done).toBe(techOf(g, 'GER')!.done.length - 2);
    expect(books().researched).toBe(techView(g, 'GER')!.done);
    expect(books().researched).toBe(held);
    expect(dcOf(g, 'GER')).toBe(1);
    // Each switch counts its own row again and no other.
    set(false, false, true);
    expect(books().researched).toBe(held + 1);
    set(false, true, true);
    expect(books().researched).toBe(held + 2);
    expect(books().researched).toBe(techOf(g, 'GER')!.done.length);
  });
});

describe('ruling 240, the switches on', () => {
  it('each switch shows its own row and no other', () => {
    const g = grand('GER');
    const rows = (): string[] => techView(g, 'GER')!.nodes.filter((x) => x.id === 'proxfuze' || x.id === 'damagecontrol').map((x) => x.id);
    const naval = (): number => techView(g, 'GER')!.branches.find((b) => b.id === 'naval')!.total;
    set(true, false, false);
    expect(rows()).toEqual([]);
    expect(techView(g, 'GER')!.total).toBe(R1.total);
    set(false, true, false);
    expect(rows()).toEqual(['proxfuze']);
    expect([techView(g, 'GER')!.total, naval()]).toEqual([R1.total + 1, R1.naval + 1]);
    expect(whyNotResearch(g, 'GER', 'proxfuze')).toBe('Needs Radar gunnery first.');
    expect(whyNotResearch(g, 'GER', 'damagecontrol')).toBe('There is no such technology.');
    set(false, false, true);
    expect(rows()).toEqual(['damagecontrol']);
    expect([techView(g, 'GER')!.total, naval()]).toEqual([R1.total + 1, R1.naval + 1]);
    expect(whyNotResearch(g, 'GER', 'damagecontrol')).toBeNull();
    expect(whyNotResearch(g, 'GER', 'proxfuze')).toBe('There is no such technology.');
    set(true, true, true);
    expect(rows()).toEqual(['damagecontrol', 'proxfuze']);
    expect([techView(g, 'GER')!.total, naval()]).toEqual([R1.total + 2, R1.naval + 2]);
    expect(techView(g, 'GER')!.nodes.find((x) => x.id === 'damagecontrol')).toMatchObject({ name: 'Damage control', state: 'ready', rule: 'Ships take 30% more hits to sink.', why: null });
    expect(techView(g, 'GER')!.nodes.find((x) => x.id === 'proxfuze')).toMatchObject({ name: 'Proximity shells', state: 'locked', rule: "Ships' guns bring down 30% more of the aircraft that attack them.", why: 'Needs Radar gunnery first.' });
  });

  it('a staff takes up a row like any naval item, whatever its nation: a great power, a small one, a dominion', () => {
    for (const nat of ['GER', 'UK', 'USA', 'SWE', 'CAN']) {
      const g = grand();
      allBut(g, nat, 'proxfuze', 'damagecontrol');
      set(false, false, false);
      expect(staffPick(g, nat, 60), nat).toBeNull();
      set(false, false, true);
      expect(staffPick(g, nat, 60), nat).toBe('damagecontrol');
      set(false, true, false);
      expect(staffPick(g, nat, 60), nat).toBe('proxfuze');
      // And a player can be given the research like any other.
      expect(grantTech(g, nat, 'proxfuze'), nat).toBe(true);
      expect(hasTech(g, nat, 'proxfuze'), nat).toBe(true);
    }
  });

  it('reads a navy\'s kit from its research alone: the same for every nation on the chart', () => {
    set(true, true, true);
    const g = grand();
    const nats = Object.keys(g.nations);
    expect(nats.length).toBeGreaterThan(40);
    // (Every one of them starts the war with none of the four, so a list holds what the test gives it and no more.)
    for (const id of FOUR) expect(holding(g, id), id).toEqual([]);
    const STEPS: [string, [unknown, number, number]][] = [
      ['', [{ stage: 'early', vt: false, dc: false }, 0.6, 1]],
      ['firecontrol', [{ stage: 'mid', vt: false, dc: false }, 1, 1]],
      ['proxfuze', [{ stage: 'mid', vt: true, dc: false }, 1.3, 1]],
      ['taskforces', [{ stage: 'late', vt: true, dc: false }, 1.5 * 1.3, 1]],
      ['damagecontrol', [{ stage: 'late', vt: true, dc: true }, 1.5 * 1.3, 1.3]],
    ];
    const NONE = STEPS[0][1], ALL = STEPS[4][1];
    const look = (nat: string): unknown[] => [shipKit(g, nat), flakMul(g, nat), dcOf(g, nat)];
    for (const nat of nats) {
      // Its own research, a step at a time: a great power, a small one, a neutral, a dominion, a nation out of the war.
      for (const [id, want] of STEPS) {
        if (id) give(g, nat, id);
        expect(look(nat), `${nat} with ${id || 'none'}`).toEqual(want);
      }
      // No other nation's ships are the better for it, but its kin's (arsenal.json plans.kin), who have all of it.
      for (const other of nats) if (other !== nat) expect(look(other), `${other} beside ${nat}`).toEqual(PLANS.kin[other] === nat ? ALL : NONE);
      strip(g, nat);
      // Damage control alone, on early-war guns: each part is read by itself.
      give(g, nat, 'damagecontrol');
      expect(look(nat), nat).toEqual([{ stage: 'early', vt: false, dc: true }, 0.6, 1.3]);
      techOf(g, nat)!.done = techOf(g, nat)!.done.filter((id) => id !== 'damagecontrol');
      expect(look(nat), nat).toEqual(NONE);
    }
    // (A kin nation and its senior partner are both on this chart, or the kin's half above would hold nothing.)
    expect(nats.filter((k) => PLANS.kin[k] !== undefined && nats.includes(PLANS.kin[k]))).toContain('CAN');
    // Late without the mid row is still late (the tree gives it only after the mid row, but the rule reads what is held).
    give(g, 'ITA', 'taskforces');
    expect(shipKit(g, 'ITA').stage).toBe('late');
  });

  it('each part of the kit stands behind its own switch', () => {
    const g = grand();
    give(g, 'UK', ...FOUR);
    set(true, false, false);
    expect([shipKit(g, 'UK'), flakMul(g, 'UK'), dcOf(g, 'UK')]).toEqual([{ stage: 'late', vt: false, dc: false }, 1.5, 1]);
    expect(['stage', 'vt', 'dc'].map((k) => shipRule(g, k as 'stage'))).toEqual([true, false, false]);
    set(false, true, false);
    expect([shipKit(g, 'UK'), flakMul(g, 'UK'), dcOf(g, 'UK')]).toEqual([{ stage: null, vt: true, dc: false }, 1.3, 1]);
    set(false, false, true);
    expect([shipKit(g, 'UK'), flakMul(g, 'UK'), dcOf(g, 'UK')]).toEqual([{ stage: null, vt: false, dc: true }, 1, 1.3]);
    // A navy at mid with no shells counts exactly as release 1's guns did.
    set(true, true, true);
    give(g, 'ITA', 'firecontrol');
    expect(flakMul(g, 'ITA')).toBe(1);
  });

  it('a kin nation\'s fleet has the better of its own and its senior partner\'s', () => {
    set(true, true, true);
    const g = grand();
    expect(PLANS.kin.CAN).toBe('UK');
    expect(shipKit(g, 'CAN')).toEqual({ stage: 'early', vt: false, dc: false });
    // Britain's research serves Canada's ships; Canada's does not serve Britain's, nor another dominion's.
    give(g, 'UK', 'firecontrol', 'damagecontrol');
    expect(shipKit(g, 'CAN')).toEqual({ stage: 'mid', vt: false, dc: true });
    expect(shipKit(g, 'AUS')).toEqual({ stage: 'mid', vt: false, dc: true });
    give(g, 'CAN', 'taskforces', 'proxfuze');
    expect(shipKit(g, 'CAN')).toEqual({ stage: 'late', vt: true, dc: true });
    expect(shipKit(g, 'UK')).toEqual({ stage: 'mid', vt: false, dc: true });
    expect(shipKit(g, 'AUS')).toEqual({ stage: 'mid', vt: false, dc: true });
    // Nobody else reads a partner's: an ally's research is its own.
    expect(shipKit(g, 'FRA')).toEqual({ stage: 'early', vt: false, dc: false });
    // And behind the same switches.
    set(false, false, false);
    expect(shipKit(g, 'CAN')).toEqual({ stage: null, vt: false, dc: false });
  });

  it('reads nothing in a history campaign, whatever the switches say', () => {
    set(true, true, true);
    const g = history();
    const before = saveGame(g);
    for (const nat of ['UK', 'GER', 'CAN', 'USA']) {
      expect(shipKit(g, nat), nat).toEqual({ stage: null, vt: false, dc: false });
      expect(flakMul(g, nat), nat).toBe(1);
      expect(dcOf(g, nat), nat).toBe(1);
    }
    for (const k of ['stage', 'vt', 'dc'] as const) expect(shipRule(g, k), k).toBe(false);
    expect(saveGame(g)).toBe(before);
  });

  it('the two older cards say what they do for the guns, only while the stage switch is on', () => {
    set(true, false, false);
    expect(techRule(techById('firecontrol')!)).toBe(`${OLD_CARD} Ships' anti-aircraft fire rises from early-war to mid-war strength.`);
    expect(techRule(techById('taskforces')!)).toBe(`${OLD_CARD} Ships' anti-aircraft fire rises from mid-war to late-war strength.`);
    // No other card gains a word.
    const others = TECH.techs.filter((t) => t.id !== 'firecontrol' && t.id !== 'taskforces');
    const on = others.map((t) => techRule(t));
    set(false, true, true);
    expect(techRule(techById('firecontrol')!)).toBe(OLD_CARD);
    expect(techRule(techById('taskforces')!)).toBe(OLD_CARD);
    set(false, false, false);
    expect(others.map((t) => techRule(t))).toEqual(on);
  });
});

// ---- The raid card (the fixture of tests/carriers.test.ts: Britain at the table, at war with Germany) ----------------

const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const wing = (g: Game, f: Fleet, kind: 'ftr' | 'bmr'): AirFleet => wingsOf(g, f.id).find((w) => w.kind === kind)!;

/** A grand campaign with Britain at the table, played until Britain is at war with Germany and it is her month to give orders. */
function britain(seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}
/** Puts a fleet in a sea, on patrol, whatever its reach: the tests place the pieces. */
function put(g: Game, f: Fleet, key: string): void {
  f.zone = zoneOf(g, key); f.stance = 'patrol'; f.lane = undefined;
  expect(atSea(f)).toBe(true);
  syncWings(g);
}
/** The Home Fleet with six carriers, and the German Battle Fleet (2 battleships, 8 cruisers, 20 destroyers) with it in the open Atlantic. */
function duel(seed = 3): { g: Game; home: Fleet; foe: Fleet; strike: AirFleet } {
  const g = britain(seed);
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = 6;
  put(g, home, 'natl');
  put(g, foe, 'natl');
  return { g, home, foe, strike: wing(g, home, 'bmr') };
}
/** The same, with the German Battle Fleet in its harbour and the Home Fleet's carriers off it. */
function raid(seed = 3): { g: Game; foe: Fleet; port: number; strike: AirFleet } {
  const g = britain(seed);
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = 6;
  sail(g, foe, foe.home, 'port');
  expect(atSea(foe)).toBe(false);
  const port = foe.base;
  home.zone = g.seas.find((z) => z.coast.includes(port))!.id; home.stance = 'patrol'; syncWings(g);
  return { g, foe, port, strike: wing(g, home, 'bmr') };
}
/** Brings another navy's fleet into the same harbour, at war with Britain. */
function berth(g: Game, name: string, port: number): Fleet {
  const f = named(g, name);
  setWar(g, 'UK', f.nat);
  f.base = port; f.stance = 'port';
  return f;
}

const GUNS = "Ships' guns";
const gunsLine = (a: AirAssess): { label: string; value: string; effect?: string } | undefined => a.lines.find((l) => l.label === GUNS);
/** A fleet's plain weight of anti-aircraft fire: release 1's sum. */
const weight = (f: Fleet): number => (['bb', 'cv', 'ca', 'dd', 'ss'] as const).reduce((n, c) => n + f.ships[c] * CARRIERS.flak.ships[c], 0);
/** Release 1's line for the German Battle Fleet at sea, to the letter. */
const R1_LINE = { label: GUNS, value: 'Weight of anti-aircraft fire 28', effect: 'About 3% of the bombers brought down: battleships and carriers carry the most guns' };

describe("ruling 240, C1: ships' anti-aircraft strength grows with research", () => {
  it("the switch off: release 1's figure to the digits and release 1's line to the letter, whatever is researched", () => {
    const { g, foe, strike } = duel();
    expect(weight(foe)).toBe(28);
    const off = assessAir(g, [strike], 'fleet', foe.id);
    expect(off.flak).toBe(0.042);
    expect(off.bomberLoss).toBe(0.049600000000000005);
    expect(gunsLine(off)).toEqual(R1_LINE);
    give(g, 'GER', ...FOUR);
    set(false, false, true);
    const held = assessAir(g, [strike], 'fleet', foe.id);
    expect(held.flak).toBe(0.042);
    expect(held.bomberLoss).toBe(off.bomberLoss);
    expect(held.lines).toEqual(off.lines);
  });

  it("counts the same fleet at early, mid and late strength, and at mid gives release 1's figure to the last digit", () => {
    const { g, foe, strike } = duel();
    const off = assessAir(g, [strike], 'fleet', foe.id);
    set(true, false, false);
    const early = assessAir(g, [strike], 'fleet', foe.id);
    give(g, 'GER', 'firecontrol');
    const mid = assessAir(g, [strike], 'fleet', foe.id);
    give(g, 'GER', 'taskforces');
    const late = assessAir(g, [strike], 'fleet', foe.id);
    expect(early.flak).toBeCloseTo(0.042 * CARRIERS.flak.stage.early, 12);
    expect(early.flak).toBeCloseTo(0.0252, 12);
    expect(mid.flak).toBe(0.042);
    expect(mid.flak).toBe(off.flak);
    expect(mid.bomberLoss).toBe(off.bomberLoss);
    expect(mid.gunLoss).toBe(off.gunLoss);
    expect(late.flak).toBeCloseTo(0.042 * CARRIERS.flak.stage.late, 12);
    expect(late.flak).toBeCloseTo(0.063, 12);
    // The bombers pay for it, and nothing else on the card moves.
    const I = CARRIERS.strike.intensity;
    expect(early.bomberLoss - off.bomberLoss).toBeCloseTo((0.0252 - 0.042) * I, 12);
    expect(late.bomberLoss - off.bomberLoss).toBeCloseTo((0.063 - 0.042) * I, 12);
    for (const a of [early, mid, late]) {
      expect(a.spoil).toBe(off.spoil);
      expect(a.naval).toBe(off.naval);
      expect(a.lines.filter((l) => l.label !== GUNS)).toEqual(off.lines.filter((l) => l.label !== GUNS));
    }
    // The line: the staged weight first, then the stage, and what the guns may do. It never names the research.
    expect(gunsLine(early)).toEqual({ label: GUNS, value: 'Weight of anti-aircraft fire 17, early-war guns', effect: 'They may bring down about 2% of the bombers: battleships and carriers carry the most guns' });
    expect(gunsLine(mid)).toEqual({ label: GUNS, value: 'Weight of anti-aircraft fire 28, mid-war guns', effect: 'They may bring down about 3% of the bombers: battleships and carriers carry the most guns' });
    expect(gunsLine(late)).toEqual({ label: GUNS, value: 'Weight of anti-aircraft fire 42, late-war guns', effect: 'They may bring down about 5% of the bombers: battleships and carriers carry the most guns' });
    for (const a of [early, mid, late]) expect(`${gunsLine(a)!.value} ${gunsLine(a)!.effect}`).not.toMatch(/radar|gunnery|task force|research/i);
  });

  it('says "almost none" where the share rounds to nothing (the UI director\'s rider 2), and leaves release 1\'s line alone with the switches off', () => {
    const { g, foe, strike } = duel();
    foe.ships = { bb: 0, cv: 0, ca: 0, dd: 2, ss: 0 };
    const off = assessAir(g, [strike], 'fleet', foe.id);
    expect(Math.round(off.flak * CARRIERS.strike.intensity * 100)).toBe(0);
    // (Release 1's own words for the smallest fleets: logged by the UI director for release 2, and not this switch's to move.)
    expect(gunsLine(off)!.effect).toBe('About 0% of the bombers brought down: battleships and carriers carry the most guns');
    set(true, false, false);
    const early = assessAir(g, [strike], 'fleet', foe.id);
    expect(gunsLine(early)).toEqual({ label: GUNS, value: `Weight of anti-aircraft fire ${Math.round(weight(foe) * CARRIERS.flak.stage.early)}, early-war guns`, effect: 'They may bring down almost none of the bombers' });
    // A share of a half per cent or more is still given as a figure.
    foe.ships = { bb: 0, cv: 0, ca: 8, dd: 8, ss: 0 };
    const more = assessAir(g, [strike], 'fleet', foe.id);
    expect(Math.round(more.flak * CARRIERS.strike.intensity * 100)).toBeGreaterThan(0);
    expect(gunsLine(more)!.effect).toBe(`They may bring down about ${Math.round(more.flak * CARRIERS.strike.intensity * 100)}% of the bombers: battleships and carriers carry the most guns`);
  });

  it('caps a fleet heavy enough to reach the cap at most x its own multiplier', () => {
    const { g, foe, strike } = duel();
    foe.ships.bb = 40;
    expect(weight(foe) * CARRIERS.flak.per).toBeGreaterThan(CARRIERS.flak.most);
    const off = assessAir(g, [strike], 'fleet', foe.id);
    expect(off.flak).toBe(CARRIERS.flak.most);
    set(true, false, false);
    const early = assessAir(g, [strike], 'fleet', foe.id);
    expect(early.flak).toBeCloseTo(CARRIERS.flak.most * 0.6, 12);
    expect(gunsLine(early)!.value).toBe(`Weight of anti-aircraft fire ${Math.round(weight(foe) * 0.6)}, early-war guns`);
    give(g, 'GER', 'firecontrol');
    expect(assessAir(g, [strike], 'fleet', foe.id).flak).toBe(CARRIERS.flak.most);
    give(g, 'GER', 'taskforces');
    const late = assessAir(g, [strike], 'fleet', foe.id);
    expect(late.flak).toBeCloseTo(CARRIERS.flak.most * 1.5, 12);
    expect(late.flak).toBeCloseTo(0.18, 12);
  });

  it('in a harbour of two navies counts each fleet at its own stage, and the cap goes by weight of guns', () => {
    const { g, foe, port, strike } = raid();
    const ally = berth(g, '2nd Fleet', port);
    expect(ally.nat).toBe('ITA');
    const wG = weight(foe), wI = weight(ally);
    expect([wG, wI]).toEqual([28, 23]);
    const off = assessAir(g, [strike], 'harbour', port);
    expect(off.why).toBeNull();
    expect(off.ships).toEqual(expect.arrayContaining([foe, ally]));
    expect(off.ships).toHaveLength(2);
    expect(off.flak).toBeCloseTo((wG + wI) * CARRIERS.flak.per, 12);
    expect(gunsLine(off)!.value).toBe('Weight of anti-aircraft fire 51');
    // Germany late, Italy early: each fleet's own weight at its own stage.
    set(true, false, false);
    give(g, 'GER', 'firecontrol', 'taskforces');
    const mixed = assessAir(g, [strike], 'harbour', port);
    expect(mixed.flak).toBeCloseTo(off.flak * ((wG * 1.5 + wI * 0.6) / (wG + wI)), 12);
    expect(mixed.flak).toBeCloseTo((28 * 1.5 + 23 * 0.6) * CARRIERS.flak.per, 12);
    // The line gives the staged weight and the strongest stage present.
    expect(gunsLine(mixed)).toEqual({ label: GUNS, value: 'Weight of anti-aircraft fire 56, late-war guns', effect: `They may bring down about ${Math.round(mixed.flak * CARRIERS.harbour.intensity * 100)}% of the bombers: battleships and carriers carry the most guns` });
    // The other way round: the same rule, the other navy's research.
    const h = raid();
    const ally2 = berth(h.g, '2nd Fleet', h.port);
    give(h.g, 'ITA', 'firecontrol');
    const other = assessAir(h.g, [h.strike], 'harbour', h.port);
    expect(other.flak).toBeCloseTo((28 * 0.6 + 23 * 1) * CARRIERS.flak.per, 12);
    expect(gunsLine(other)!.value).toBe('Weight of anti-aircraft fire 40, mid-war guns');
    // Over the cap. One late-war destroyer beside a great early-war fleet does not lift the cap for it: the whole
    // harbour's guns bring down `most` in the share their staged weight is of their plain weight.
    h.foe.ships.bb = 40;
    ally2.ships = { bb: 0, cv: 0, ca: 0, dd: 1, ss: 0 };
    give(h.g, 'ITA', 'taskforces');
    const big = weight(h.foe), small = weight(ally2);
    expect(small).toBe(0.5);
    expect((big + small) * CARRIERS.flak.per).toBeGreaterThan(CARRIERS.flak.most);
    const capped = assessAir(h.g, [h.strike], 'harbour', h.port);
    expect(capped.ships).toHaveLength(2);
    expect(capped.flak).toBeCloseTo(CARRIERS.flak.most * ((big * 0.6 + small * 1.5) / (big + small)), 12);
    expect(capped.flak).toBeLessThan(CARRIERS.flak.most * 0.61);
    expect(gunsLine(capped)!.value).toBe(`Weight of anti-aircraft fire ${Math.round(big * 0.6 + small * 1.5)}, late-war guns`);
    // And a great late-war fleet is not held to an early-war neighbour's cap.
    give(h.g, 'GER', 'firecontrol', 'taskforces');
    techOf(h.g, 'ITA')!.done = techOf(h.g, 'ITA')!.done.filter((id) => id !== 'firecontrol' && id !== 'taskforces');
    expect(assessAir(h.g, [h.strike], 'harbour', h.port).flak).toBeCloseTo(CARRIERS.flak.most * ((big * 1.5 + small * 0.6) / (big + small)), 12);
  });

  it('is blind to the nation: the same ships with the same research give the same fire and the same line under every flag on the chart', () => {
    set(true, false, false);
    const { g, foe, strike } = duel();
    const nats = Object.keys(g.nations);
    expect(nats.length).toBeGreaterThan(40);
    for (const nat of nats) strip(g, nat);
    const under = (nat: string): unknown[] => {
      foe.nat = nat;
      const out: unknown[] = [];
      const look = (): void => { const a = assessAir(g, [strike], 'fleet', foe.id); expect(a.why, nat).toBeNull(); out.push([a.flak, a.bomberLoss, gunsLine(a)]); };
      look();
      give(g, nat, 'firecontrol'); look();
      give(g, nat, 'taskforces'); look();
      strip(g, nat);
      return out;
    };
    const ger = under('GER') as [number, number, unknown][];
    expect(ger.map((x) => x[0])).toEqual([expect.closeTo(0.0252, 12), 0.042, expect.closeTo(0.063, 12)]);
    expect(ger.map((x) => (x[2] as { value: string }).value)).toEqual(['Weight of anti-aircraft fire 17, early-war guns', 'Weight of anti-aircraft fire 28, mid-war guns', 'Weight of anti-aircraft fire 42, late-war guns']);
    // A great power, a small one, a neutral, a dominion, the raider's own flag: the fleet is the same fleet.
    for (const nat of nats) expect(under(nat), nat).toEqual(ger);
  });

  it('a harbour caught unready: the guns count harbour.unready of what they would, at every stage', () => {
    const { g, foe, port, strike } = raid();
    set(true, false, false);
    for (const ids of [[], ['firecontrol'], ['taskforces']]) {
      give(g, 'GER', ...ids);
      delete g.flags['war:UK:GER'];
      const ready = assessAir(g, [strike], 'harbour', port);
      expect(ready.surprise).toBe(false);
      // War declared this month.
      g.flags['war:UK:GER'] = g.turn;
      const caught = assessAir(g, [strike], 'harbour', port);
      expect(caught.surprise).toBe(true);
      expect(caught.ships).toEqual([foe]);
      expect(caught.flak).toBeCloseTo(ready.flak * CARRIERS.harbour.unready, 12);
      // The weight on the line is the guns' own: surprise is told on its own line.
      expect(gunsLine(caught)!.value).toBe(gunsLine(ready)!.value);
    }
    expect(gunsLine(assessAir(g, [strike], 'harbour', port))!.value).toBe('Weight of anti-aircraft fire 42, late-war guns');
  });

  it('plans a raid without changing the game', () => {
    const { g, foe, strike } = duel();
    set(true, false, false);
    give(g, 'GER', 'firecontrol');
    const before = saveGame(g);
    assessAir(g, [strike], 'fleet', foe.id);
    const p = planRaid(g, 'fleet', foe.id, [strike.id], 'UK');
    expect(p.lines.map((l) => l.label)).toContain(GUNS);
    expect(saveGame(g)).toBe(before);
  });

  it('reaches no history campaign: there the guns count as they always did, whatever the switch says', () => {
    const g = history();
    const foe = named(g, 'Battle Fleet');
    foe.stance = 'port';
    const bombers = g.airFleets.filter((f) => f.nat === 'UK' && f.name === 'Bomber Command');
    expect(bombers).toHaveLength(1);
    const off = assessAir(g, bombers, 'harbour', foe.base);
    expect(off.why).toBeNull();
    expect(off.flak).toBe(0.042);
    set(true, true, true);
    const on = assessAir(g, bombers, 'harbour', foe.base);
    expect(on.flak).toBe(0.042);
    expect(on.bomberLoss).toBe(off.bomberLoss);
    expect(on.lines).toEqual(off.lines);
    expect(gunsLine(on)).toEqual(R1_LINE);
  });
});

describe('ruling 240, C2: proximity shells', () => {
  it('multiplies a holder\'s share of the bombers by flak.vt, and does nothing for a navy without them', () => {
    const { g, foe, strike } = duel();
    const off = assessAir(g, [strike], 'fleet', foe.id);
    set(false, true, false);
    // Without the shells: release 1's figure to the digit. Only the form of the words is the new one.
    const without = assessAir(g, [strike], 'fleet', foe.id);
    expect(without.flak).toBe(0.042);
    expect(without.bomberLoss).toBe(off.bomberLoss);
    expect(gunsLine(without)).toEqual({ label: GUNS, value: 'Weight of anti-aircraft fire 28', effect: 'They may bring down about 3% of the bombers: battleships and carriers carry the most guns' });
    // With them (the rule reads what is held).
    give(g, 'GER', 'proxfuze');
    const held = assessAir(g, [strike], 'fleet', foe.id);
    expect(held.flak).toBeCloseTo(0.042 * CARRIERS.flak.vt, 12);
    expect(held.flak).toBeCloseTo(0.0546, 12);
    expect(held.bomberLoss - off.bomberLoss).toBeCloseTo((0.0546 - 0.042) * CARRIERS.strike.intensity, 12);
    expect(gunsLine(held)).toEqual({ label: GUNS, value: 'Weight of anti-aircraft fire 36, with proximity shells', effect: 'They may bring down about 4% of the bombers: battleships and carriers carry the most guns' });
    expect(held.lines.filter((l) => l.label !== GUNS)).toEqual(off.lines.filter((l) => l.label !== GUNS));
    // The switch off again: the shells on the list count for nothing, and the line is release 1's.
    set(false, false, false);
    const shelvedAgain = assessAir(g, [strike], 'fleet', foe.id);
    expect(shelvedAgain.flak).toBe(0.042);
    expect(gunsLine(shelvedAgain)).toEqual(R1_LINE);
    // The stage rule alone does not read the shells either.
    set(true, false, false);
    expect(assessAir(g, [strike], 'fleet', foe.id).flak).toBeCloseTo(0.0252, 12);
    expect(gunsLine(assessAir(g, [strike], 'fleet', foe.id))!.value).toBe('Weight of anti-aircraft fire 17, early-war guns');
  });

  it('raises the cap with them, by the same rule of weight', () => {
    const { g, foe, strike } = duel();
    foe.ships.bb = 40;
    set(false, true, false);
    expect(assessAir(g, [strike], 'fleet', foe.id).flak).toBe(CARRIERS.flak.most);
    give(g, 'GER', 'proxfuze');
    expect(assessAir(g, [strike], 'fleet', foe.id).flak).toBeCloseTo(CARRIERS.flak.most * 1.3, 12);
    // With the stages as well: late-war guns with the shells reach most x 1.5 x 1.3.
    set(true, true, false);
    give(g, 'GER', 'firecontrol', 'taskforces');
    const both = assessAir(g, [strike], 'fleet', foe.id);
    expect(both.flak).toBeCloseTo(CARRIERS.flak.most * 1.5 * 1.3, 12);
    expect(both.flak).toBeCloseTo(0.234, 12);
    expect(gunsLine(both)!.value).toBe(`Weight of anti-aircraft fire ${Math.round(weight(foe) * 1.5 * 1.3)}, late-war guns with proximity shells`);
  });

  it('says so on the line, with the stage or without it', () => {
    const { g, foe, strike } = duel();
    const value = (): string => gunsLine(assessAir(g, [strike], 'fleet', foe.id))!.value;
    const effect = (): string => gunsLine(assessAir(g, [strike], 'fleet', foe.id))!.effect!;
    set(true, true, false);
    expect(value()).toBe('Weight of anti-aircraft fire 17, early-war guns');
    give(g, 'GER', 'proxfuze');
    expect(value()).toBe('Weight of anti-aircraft fire 22, early-war guns with proximity shells');
    give(g, 'GER', 'firecontrol');
    expect(value()).toBe('Weight of anti-aircraft fire 36, mid-war guns with proximity shells');
    give(g, 'GER', 'taskforces');
    expect(value()).toBe('Weight of anti-aircraft fire 55, late-war guns with proximity shells');
    expect(effect()).toBe('They may bring down about 7% of the bombers: battleships and carriers carry the most guns');
    expect(assessAir(g, [strike], 'fleet', foe.id).flak).toBeCloseTo(0.042 * 1.5 * 1.3, 12);
    set(false, true, false);
    expect(value()).toBe('Weight of anti-aircraft fire 36, with proximity shells');
    set(true, false, false);
    expect(value()).toBe('Weight of anti-aircraft fire 42, late-war guns');
    set(false, false, true);
    expect(gunsLine(assessAir(g, [strike], 'fleet', foe.id))).toEqual(R1_LINE);
    for (const on of [[true, true], [false, true]]) {
      set(on[0], on[1], false);
      expect(`${value()} ${effect()}`).not.toMatch(/radar|gunnery|task force|research|fuze/i);
    }
  });

  it('in a harbour counts them for the navy that has them and not for the one beside it; a kin navy has its senior partner\'s', () => {
    const { g, foe, port, strike } = raid();
    const ally = berth(g, '2nd Fleet', port);
    set(false, true, false);
    give(g, 'GER', 'proxfuze');
    const a = assessAir(g, [strike], 'harbour', port);
    expect(a.ships).toHaveLength(2);
    expect(a.flak).toBeCloseTo((weight(foe) * 1.3 + weight(ally)) * CARRIERS.flak.per, 12);
    expect(gunsLine(a)!.value).toBe('Weight of anti-aircraft fire 59, with proximity shells');
    // Stages and shells together: Germany early with the shells, Italy late without them.
    set(true, true, false);
    give(g, 'ITA', 'taskforces');
    const b = assessAir(g, [strike], 'harbour', port);
    expect(b.flak).toBeCloseTo((28 * 0.6 * 1.3 + 23 * 1.5) * CARRIERS.flak.per, 12);
    expect(gunsLine(b)!.value).toBe('Weight of anti-aircraft fire 56, late-war guns with proximity shells');
    // A dominion's ships fire Britain's shells; Britain's do not fire a dominion's.
    const k = duel();
    set(false, true, false);
    k.foe.nat = 'CAN';
    expect(assessAir(k.g, [k.strike], 'fleet', k.foe.id).flak).toBe(0.042);
    give(k.g, 'UK', 'proxfuze');
    expect(assessAir(k.g, [k.strike], 'fleet', k.foe.id).flak).toBeCloseTo(0.0546, 12);
    expect(flakMul(k.g, 'CAN')).toBe(1.3);
    const m = grand();
    give(m, 'CAN', 'proxfuze');
    expect([flakMul(m, 'CAN'), flakMul(m, 'UK')]).toEqual([1.3, 1]);
  });

  it('is blind to the nation: the shells count the same for the same ships under every flag on the chart', () => {
    const { g, foe, strike } = duel();
    const nats = Object.keys(g.nations);
    for (const nat of nats) strip(g, nat);
    const under = (nat: string): unknown[] => {
      foe.nat = nat;
      const out: unknown[] = [];
      const look = (): void => { const a = assessAir(g, [strike], 'fleet', foe.id); out.push([a.flak, a.bomberLoss, gunsLine(a)]); };
      // The shells with no stage in force, and then on early, mid and late guns.
      set(false, true, false);
      look();
      give(g, nat, 'proxfuze'); look();
      set(true, true, false);
      look();
      give(g, nat, 'firecontrol'); look();
      give(g, nat, 'taskforces'); look();
      strip(g, nat);
      return out;
    };
    const ger = under('GER') as [number, number, { value: string }][];
    expect(ger.map((x) => x[0])).toEqual([0.042, expect.closeTo(0.0546, 12), expect.closeTo(0.0252 * 1.3, 12), expect.closeTo(0.0546, 12), expect.closeTo(0.042 * 1.5 * 1.3, 12)]);
    expect(ger.map((x) => x[2].value)).toEqual([
      'Weight of anti-aircraft fire 28', 'Weight of anti-aircraft fire 36, with proximity shells', 'Weight of anti-aircraft fire 22, early-war guns with proximity shells',
      'Weight of anti-aircraft fire 36, mid-war guns with proximity shells', 'Weight of anti-aircraft fire 55, late-war guns with proximity shells',
    ]);
    for (const nat of nats) expect(under(nat), nat).toEqual(ger);
  });

  it('is on the tree and open to research only while its switch is on', () => {
    const g = grand('UK');
    give(g, 'UK', 'firecontrol');
    for (const on of [false, true]) {
      set(false, on, false);
      const node = techView(g, 'UK')!.nodes.find((x) => x.id === 'proxfuze');
      expect(node !== undefined).toBe(on);
      expect(whyNotResearch(g, 'UK', 'proxfuze')).toBe(on ? null : 'There is no such technology.');
      if (on) expect(node).toMatchObject({ branch: 'naval', tier: 4, year: 1943, state: 'ready', needs: [{ id: 'firecontrol', name: 'Radar gunnery', done: true }] });
    }
    // The dc switch does not open it, nor the stage switch.
    set(true, false, true);
    expect(techView(g, 'UK')!.nodes.some((x) => x.id === 'proxfuze')).toBe(false);
    set(false, true, false);
    expect(startResearch(g, 'UK', 'proxfuze')).toBe(true);
    expect(techOf(g, 'UK')!.work.find((w) => w.id === 'proxfuze')!.of).toBeGreaterThanOrEqual(TECH.months[4]);
  });

  it('is researched by a staff like any naval item: after Radar gunnery, never before it, and never with the switch off', () => {
    for (const nat of ['UK', 'GER', 'SWE', 'CAN']) {
      const g = grand();
      allBut(g, nat, 'firecontrol', 'proxfuze');
      set(false, true, false);
      expect(staffPick(g, nat, 60), nat).toBe('firecontrol');
      give(g, nat, 'firecontrol');
      expect(staffPick(g, nat, 60), nat).toBe('proxfuze');
      set(false, false, false);
      expect(staffPick(g, nat, 60), nat).toBeNull();
    }
    // The laboratories alone, month after month until nothing is left: every staff comes to the shells, each after its
    // Radar gunnery; with the switch off none ever does.
    for (const on of [true, false]) {
      set(false, on, false);
      const g = grand();
      const nats = ['UK', 'GER', 'ITA', 'SWE', 'CAN'].filter((nat) => g.nations[nat].alive);
      expect(nats.length).toBeGreaterThanOrEqual(4);
      for (let m = 0; m < 900 && nats.some((nat) => staffPick(g, nat) !== null || techOf(g, nat)!.work.some((w) => !w.spy)); m++) { g.turn++; for (const nat of nats) techTick(g, nat); }
      for (const nat of nats) {
        const done = techOf(g, nat)!.done;
        expect(done.includes('proxfuze'), `${nat} ${on}`).toBe(on);
        if (on) expect(done.indexOf('firecontrol'), nat).toBeLessThan(done.indexOf('proxfuze'));
        if (on) expect(done.indexOf('firecontrol'), nat).toBeGreaterThanOrEqual(0);
        expect(done.includes('damagecontrol'), nat).toBe(false);
      }
    }
  });

  it('leaves the guns on land as they were: a bombing raid costs the same with the switches on', () => {
    const g = britain();
    const foe = named(g, 'Battle Fleet');
    sail(g, foe, foe.home, 'port');
    give(g, 'GER', ...FOUR);
    const groups = g.airFleets.filter((f) => f.nat === 'UK' && f.bmr > 0 && f.ship === undefined);
    expect(groups.length).toBeGreaterThan(0);
    // The Ruhr, and the port where the German fleet lies (its ships' guns do not fire at a raid on the factories).
    for (const target of [g.byName.Ruhr.id, foe.base]) {
      g.flags[`wk:aa:${target}`] = 2;
      for (const kind of ['bomb', 'night'] as const) {
        set(false, false, false);
        const off = assessAir(g, groups, kind, target);
        expect(off.why, `${kind} ${target}`).toBeNull();
        expect(off.aa).toBe(2);
        expect(off.gunLoss).toBeGreaterThan(0);
        set(true, true, true);
        const on = assessAir(g, groups, kind, target);
        expect(on.bomberLoss).toBe(off.bomberLoss);
        expect(on.gunLoss).toBe(off.gunLoss);
        expect(on.flak).toBe(0);
        expect(on.lines).toEqual(off.lines);
      }
    }
    // A raid on the harbour itself: the guns round it count as they did, and only the ships' part moves.
    const home = named(g, 'Home Fleet');
    home.ships.cv = 6;
    home.zone = g.seas.find((z) => z.coast.includes(foe.base))!.id; home.stance = 'patrol'; syncWings(g);
    const strike = wing(g, home, 'bmr');
    set(false, false, false);
    const off = assessAir(g, [strike], 'harbour', foe.base);
    set(false, true, false);
    const on = assessAir(g, [strike], 'harbour', foe.base);
    expect(off.aa).toBe(2);
    expect(on.aa).toBe(2);
    expect(on.flak).toBeCloseTo(off.flak * 1.3, 12);
    expect(on.bomberLoss - off.bomberLoss).toBeCloseTo((on.flak - off.flak) * CARRIERS.harbour.intensity, 12);
    expect(on.lines.find((l) => l.label === 'Anti-aircraft guns')).toEqual(off.lines.find((l) => l.label === 'Anti-aircraft guns'));
  });
});

// ---- Months of a war with the switches on, and saves carried across the switches -----------------------------------

describe('ruling 240, a staff-fought war with the switches on, and saves carried across them', () => {
  it('thirty months with all three on: every number in the save is a number, the save loads back to itself, and staffs take up Damage control like any naval item', () => {
    const text = war(true);
    set(true, true, true);
    const g = loadGame(text)!;
    expect(g.turn).toBe(30);
    // (The rules bite: it is not the war the same seed gives with the switches off.)
    expect(text === war(false)).toBe(false);
    expect(notNumbers(g)).toEqual([]);
    expect(saveGame(g) === text).toBe(true);
    // (The helper does find one where there is one.)
    const bad = loadGame(text)!;
    bad.fleets[0].ready = NaN;
    expect(notNumbers(bad)).toEqual(['ready']);
    // R8: no table names the rows, and a staff researches Damage control like any naval item. (Measured, seed 23 with
    // the fight back: Britain and the United States hold it by month 30, and Italy has a team on it.)
    const held = holding(g, 'damagecontrol');
    expect(held.length).toBeGreaterThan(0);
    expect(workingOn(g, 'damagecontrol').length).toBeGreaterThan(0);
    for (const nat of Object.keys(g.nations)) {
      // A navy's kit is its research, or its senior partner's, and nothing else: read here from the lists themselves.
      const lists = [nat, PLANS.kin[nat]].filter((n) => n !== undefined && g.nations[n]).map((n) => techOf(g, n)!.done);
      const dc = lists.some((l) => l.includes('damagecontrol'));
      expect(shipKit(g, nat), nat).toEqual({ stage: stageFrom(g, nat), vt: lists.some((l) => l.includes('proxfuze')), dc });
      expect(dcOf(g, nat), nat).toBe(dc ? 1.3 : 1);
      // Its tree shows both rows, and counts every team at work and every research held.
      const v = techView(g, nat)!;
      expect(v.total, nat).toBe(R1.total + 2);
      expect(v.busy, nat).toBe(v.nodes.filter((x) => x.state === 'work').length);
      expect(v.busy, nat).toBe(techOf(g, nat)!.work.length);
      expect(v.done, nat).toBe(techOf(g, nat)!.done.length);
    }
    expect(dcOf(g, held[0])).toBe(1.3);
  }, 240000);

  it('a save made with the switches on and loaded with them off plays on as release 1 does: the ids stay in the save and count for nothing', () => {
    const text = war(true);
    set(false, false, false);
    const g = loadGame(text)!;
    // Loading rewrites nothing: the lists still hold the id, and a team is still down for the row.
    expect(saveGame(g) === text).toBe(true);
    const alive = (nats: string[]): string[] => nats.filter((nat) => g.nations[nat].alive);
    const held = holding(g, 'damagecontrol'), atWork = alive(workingOn(g, 'damagecontrol'));
    expect(held.length).toBeGreaterThan(0);
    expect(atWork.length).toBeGreaterThan(0);
    // Every nation, every month: no kit, release 1's tree, and the tree, the books and the lists agree.
    const audit = (when: string): void => {
      let books = 0;
      for (const nat of Object.keys(g.nations)) {
        const at = `${when}, ${nat}`;
        expect(shipKit(g, nat), at).toEqual({ stage: null, vt: false, dc: false });
        expect(flakMul(g, nat), at).toBe(1);
        expect(dcOf(g, nat), at).toBe(1);
        const v = techView(g, nat)!, s = techOf(g, nat)!;
        expect(v.total, at).toBe(R1.total);
        expect(v.nodes.some((x) => x.id === 'damagecontrol' || x.id === 'proxfuze'), at).toBe(false);
        expect(v.busy, at).toBe(v.nodes.filter((x) => x.state === 'work').length);
        expect(v.busy, at).toBe(s.work.filter((w) => !shelved(techById(w.id))).length);
        expect(v.done, at).toBe(s.done.filter((id) => !shelved(techById(id))).length);
        const b = booksOf(g, nat, nat);
        if (!b) continue;
        books++;
        expect(b.researched, at).toBe(v.done);
        expect(b.research.map((x) => x.name), at).not.toContain('Damage control');
        expect(b.research, at).toHaveLength(v.busy);
      }
      expect(books, when).toBeGreaterThan(20);
    };
    audit('at the load');
    // The team that is down for the shelved row counts as free.
    for (const nat of atWork) expect(techView(g, nat)!.busy, nat).toBe(techOf(g, nat)!.work.length - 1);
    const news = g.headlines.length;
    for (let m = 1; m <= 3; m++) {
      runTurnAI(g);
      // The work in hand on the row is dropped at the nation's next month of research, and nobody takes it up again.
      expect(alive(workingOn(g, 'damagecontrol')), `month ${m}`).toEqual([]);
      audit(`month ${m} after the load`);
    }
    // The team that was on it has other work, or there is nothing left for it to begin.
    for (const nat of atWork) expect(idleTeams(g, nat), nat).toBe(0);
    for (const nat of atWork) expect(techView(g, nat)!.busy, nat).toBeGreaterThan(0);
    // What was researched stays on the list and in the save, and nothing in the months played speaks of the rules.
    expect(holding(g, 'damagecontrol')).toEqual(held);
    const again = saveGame(g);
    expect(again).toMatch(/damagecontrol/);
    expect(g.headlines.slice(news).map((h) => h.text).join(' ')).not.toMatch(/Damage control|Proximity shells|repair parties/);
    expect(notNumbers(g)).toEqual([]);
    expect(saveGame(loadGame(again)!) === again).toBe(true);
  }, 240000);

  it('a save made with the switches off (release 1\'s) and loaded with all three on plays on: the rows are on the tree, a navy has early-war guns until it researches, and a staff takes Damage control up', () => {
    const text = war(false);
    set(true, true, true);
    const g = loadGame(text)!;
    expect(saveGame(g) === text).toBe(true);
    const nats = Object.keys(g.nations);
    for (const nat of nats) {
      expect(shipKit(g, nat), nat).toEqual({ stage: stageFrom(g, nat), vt: false, dc: false });
      expect(techView(g, nat)!.total, nat).toBe(R1.total + 2);
    }
    // (Measured: no staff has Radar gunnery by month 30 of this war, so every navy on the chart reads early-war guns.)
    expect(nats.filter((nat) => shipKit(g, nat).stage === 'early').length).toBeGreaterThan(40);
    for (let m = 1; m <= 3; m++) runTurnAI(g);
    // (Measured: Britain's staff begins Damage control in the first month after the load, Italy's in the second.)
    expect(workingOn(g, 'damagecontrol').length + holding(g, 'damagecontrol').length).toBeGreaterThan(0);
    for (const nat of nats) {
      const v = techView(g, nat)!;
      expect(v.busy, nat).toBe(v.nodes.filter((x) => x.state === 'work').length);
      expect(v.busy, nat).toBe(techOf(g, nat)!.work.length);
    }
    expect(notNumbers(g)).toEqual([]);
    const again = saveGame(g);
    expect(again).toMatch(/damagecontrol/);
    expect(saveGame(loadGame(again)!) === again).toBe(true);
  }, 240000);
});
