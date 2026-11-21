// Ruling 240 (Brian, 21 November 2026, "yes to all 3"; release 2): ships' anti-aircraft strength grows with research,
// proximity shells and damage control are naval researches. Every nation alike: the rules read research only, never a
// nation, a date, which side attacks or who the player is. Each rule has its own switch in carriers.json 'research',
// read every time it is asked, and ALL THREE ARE OFF as shipped: with them off the game is release 1 to the byte,
// and the two new rows of the tree are shelved (no staff picks them, nobody can order them, no card shows them).
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { CARRIERS } from '../src/campaign/air';
import { createGame } from '../src/campaign/game';
import { PLANS } from '../src/campaign/plans';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  SHIP_RESEARCH, TECH, dcOf, flakMul, grantTech, hasTech, idleTeams, shelved, shipKit, shipRule, staffPick, startResearch, techById, techOf, techRule,
  techTick, techView, whyNotResearch,
} from '../src/campaign/tech';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

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
    const g = grand(null, 17);
    while (g.turn < 30) runTurnAI(g);
    for (const n of Object.values(g.nations)) {
      for (const id of ['proxfuze', 'damagecontrol']) {
        expect(n.tech?.done ?? [], `${n.id} holds ${id}`).not.toContain(id);
        expect((n.tech?.work ?? []).map((w) => w.id), `${n.id} works on ${id}`).not.toContain(id);
      }
    }
    expect(saveGame(g)).not.toMatch(/proxfuze|damagecontrol/);
  }, 120000);

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
    const text = saveGame(g);
    set(false, false, false);
    // The save still holds the id; the tree does not show it, and the team counts as free.
    expect(text).toMatch(/damagecontrol/);
    const v = techView(g, 'GER')!;
    expect(v.busy).toBe(0);
    expect(v.free).toBe(v.teams);
    expect(v.total).toBe(R1.total);
    const news = g.headlines.length;
    for (let k = 0; k < 20; k++) techTick(g, 'GER');
    expect(techOf(g, 'GER')!.work.map((w) => w.id)).not.toContain('damagecontrol');
    expect(techOf(g, 'GER')!.done).not.toContain('damagecontrol');
    expect(g.headlines.slice(news).map((h) => h.text).join(' ')).not.toMatch(/Damage control/);
    // A list that holds the id from such a save is not counted either.
    give(g, 'GER', 'damagecontrol');
    expect(techView(g, 'GER')!.done).toBe(techOf(g, 'GER')!.done.length - 1);
    expect(dcOf(g, 'GER')).toBe(1);
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

  it('reads a navy\'s kit from its research alone: the same for a great power, a small power and a dominion', () => {
    set(true, true, true);
    // (Every one of them starts the war with none of the four.)
    const kits = ['GER', 'SWE', 'CAN'].map((nat) => {
      const g = grand();
      const seen: unknown[] = [];
      const look = (): void => { seen.push([shipKit(g, nat), flakMul(g, nat), dcOf(g, nat)]); };
      look();
      give(g, nat, 'firecontrol'); look();
      give(g, nat, 'proxfuze'); look();
      give(g, nat, 'taskforces'); look();
      give(g, nat, 'damagecontrol'); look();
      return seen;
    });
    expect(kits[0]).toEqual([
      [{ stage: 'early', vt: false, dc: false }, 0.6, 1],
      [{ stage: 'mid', vt: false, dc: false }, 1, 1],
      [{ stage: 'mid', vt: true, dc: false }, 1.3, 1],
      [{ stage: 'late', vt: true, dc: false }, 1.5 * 1.3, 1],
      [{ stage: 'late', vt: true, dc: true }, 1.5 * 1.3, 1.3],
    ]);
    expect(kits[1]).toEqual(kits[0]);
    expect(kits[2]).toEqual(kits[0]);
    // Late without the mid row is still late (the tree gives it only after the mid row, but the rule reads what is held).
    const g = grand();
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
