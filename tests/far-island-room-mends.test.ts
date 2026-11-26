// The mender's tests for the island's room in men (26 November 2026): each finding of the sceptics that needed no
// ruling, held by a test that fails without its mend.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { readFileSync } from 'node:fs';
import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, havenTakes, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar, removeCorps } from '../src/campaign/game';
import { troopshipsWanted } from '../src/campaign/desks';
import { canRaise, deliver } from '../src/campaign/economy';
import { roomLeft } from '../src/campaign/islands';
import { landingForce } from '../src/campaign/staff';
import { followKey, frontsClear, sailedKey } from '../src/campaign/planfronts';
import { startOpen } from '../src/campaign/staff';
import { saveGame } from '../src/campaign/save';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** A whole-world war with a player, at his first month (seed 3). */
function war(human: string): Game {
  const g = startOpen(createGame(3, human));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** A corps of infantry put down for the test, with its month in hand. */
function corps(g: Game, nat: string, name: string, where: Region, divs = 1, str = 1): Corps {
  const c = addCorps(g, nat, name, 'inf', divs, where.id, {});
  c.str = str;
  c.ap = c.apMax;
  return c;
}
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };

const at = (g: Game, name: string): Region => g.byName[name];
type Told = { what: string; force: string[]; to: string; why?: string };
const told: Told[] = [];
const listen = (): void => { told.length = 0; frontWatch.tell = (e) => { told.push({ what: e.what, force: e.force.map((c) => c.name), to: e.to.name, why: e.why }); }; };
afterEach(() => { frontWatch.tell = null; frontsClear(); });

describe("a computer staff's shore that takes none of its force (the design director, 21:37Z, 26 November 2026, 2)", () => {
  it("is read as full, and the staff goes on to the plan's next shore instead of waiting on it every month", () => {
    const g = startOpen(createGame(3, null));
    g.turn = 29;
    g.flags['st:done:JAP:south'] = 28;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    const guam = at(g, 'Guam');
    guam.ctrl = 'JAP'; guam.units = guam.units.filter((u) => u.nat === 'JAP');
    const rabaul = at(g, 'Rabaul');
    rabaul.units = rabaul.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${rabaul.id}`] = 100;
    // (The South Seas Detachment made five divisions for the test: 75,000 men against Rabaul's room of four, 60,000.)
    const ss: Corps = addCorps(g, 'JAP', 'South Seas Detachment', 'inf', 5, guam.id, { xp: 0.3, hold: true });
    g.flags[sailedKey(ss)] = 28;
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force[0] === 'South Seas Detachment').map((t) => [t.what, t.to, t.why])).toEqual([['wait', 'Rabaul', 'no room on the shore: Rabaul is full']]);
    // The shore is ended, not made: the row is free of it, and no hop behind it reads Rabaul as taken.
    expect(g.flags[followKey('JAP', 5, 0)]).toBe(-29);
    expect([g.regions[ss.reg].name, rabaul.ctrl === 'JAP']).toEqual(['Guam', false]);
    // The next month it does not wait on Rabaul again.
    g.turn = 30;
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force[0] === 'South Seas Detachment' && t.to === 'Rabaul')).toEqual([]);
  }, 300000);
});

describe("the player's building desk and a crossing to an island", () => {
  it('lays down troopships ahead of the troops: an empty quay wants shipping for the room, as it did when the room was counted in corps', () => {
    const g = startOpen(createGame(3, null));
    const wake = at(g, 'Wake'), saipan = at(g, 'Saipan'), quay = at(g, 'Kwajalein'), pearl = at(g, 'Pearl Harbor');
    for (const r of [wake, saipan]) for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u);
    for (const u of [...quay.units]) removeCorps(g, u);
    expect(landingForce()).toBeGreaterThanOrEqual(2);
    // The quay empty: Wake's room of one division, Saipan's of two.
    expect(troopshipsWanted(g, 'JAP', { from: quay, to: wake })).toBe(1);
    expect(troopshipsWanted(g, 'USA', { from: pearl.units.length ? { ...pearl, units: [] } : pearl, to: saipan })).toBe(2);
    // One corps of one division on the quay: it and a division's worth more for Saipan.
    addCorps(g, 'JAP', 'One', 'inf', 1, quay.id, {});
    expect(troopshipsWanted(g, 'JAP', { from: quay, to: saipan })).toBe(2);
    // A corps of two divisions on the quay takes Saipan's whole room: shipping for it alone.
    for (const u of [...quay.units]) removeCorps(g, u);
    addCorps(g, 'JAP', 'Two', 'inf', 2, quay.id, {});
    expect(troopshipsWanted(g, 'JAP', { from: quay, to: saipan })).toBe(1);
    // Ground that is no island: the quay's corps, never fewer than a landing force, as it always was.
    expect(troopshipsWanted(g, 'JAP', { from: quay, to: at(g, 'Manila') })).toBe(Math.min(5, Math.max(landingForce(), 1)));
  }, 300000);
});

describe('a new corps raised on an island (the design director, 21:37Z, 26 November 2026, 4: the island refuses only new corps)', () => {
  it('is raised there only where it fits in men at the strength it forms at, and elsewhere where it does not', () => {
    const g = startOpen(createGame(3, null));
    const isle = ['Okinawa', 'Formosa', 'Taihoku', 'Kyushu'].map((nm) => at(g, nm)).find((r) => r && canRaise(g, 'JAP', r) && roomLeft(g, r, 'JAP') < 5)!;
    expect(isle).toBeDefined();
    for (const u of [...isle.units, ...(isle.garr ?? [])]) removeCorps(g, u);
    // (One division's worth of room left: the island's room less one division.)
    const room = roomLeft(g, isle, 'JAP');
    const there = addCorps(g, 'JAP', 'There', 'inf', room - 1, isle.id, {});
    there.str = 1;
    expect(roomLeft(g, isle, 'JAP')).toBeCloseTo(1, 9);
    const n = g.nations.JAP;
    n.queue.push({ id: 9901, what: 'corps', kind: 'inf', divs: 2, name: 'Two Divisions', region: isle.id, ordered: g.turn - 3, eta: g.turn, cost: 0 } as any);
    n.queue.push({ id: 9902, what: 'corps', kind: 'inf', divs: 1, name: 'One Division', region: isle.id, ordered: g.turn - 3, eta: g.turn, cost: 0 } as any);
    deliver(g, 'JAP');
    const two = g.corps.find((c) => c.name === 'Two Divisions' && !c.dead)!, one = g.corps.find((c) => c.name === 'One Division' && !c.dead)!;
    expect(two).toBeDefined();
    expect(g.regions[two.reg].name, isle.name).not.toBe(isle.name);
    expect(g.regions[one.reg].name).toBe(isle.name);
    expect(roomLeft(g, isle, 'JAP')).toBeCloseTo(0, 9);
  }, 300000);
});

describe("a group sent by rail onto an island that has no room for it (the UI director, 15:02Z, section 3, and 21:37Z 26 November 2026)", () => {
  it("is told the island's line once for the whole group: the group's men, the too-large corps first, the rest counted, and no rail reason before it", () => {
    const g = war('JAP');
    const formosa = at(g, 'Formosa'), taihoku = at(g, 'Taihoku');
    clear(g, formosa);
    corps(g, 'JAP', 'Garrison', formosa, 1, 20000 / 15000);
    const a = corps(g, 'JAP', 'Taihoku First', taihoku, 2), b = corps(g, 'JAP', 'Taihoku Second', taihoku, 2);
    const big = corps(g, 'JAP', 'Formosa Big', taihoku, 4), big2 = corps(g, 'JAP', 'Formosa Bigger', taihoku, 4);
    TC().load(saveGame(g));
    expect(TC().railTip([a.id], formosa.id).no).toBe('Formosa has room for 3 divisions, about 45,000 men, and 20,000 of yours are there: only 25,000 more can go, and you are sending 30,000.');
    expect(TC().railTip([a.id, b.id], formosa.id).no).toBe('Formosa has room for 3 divisions, about 45,000 men, and 20,000 of yours are there: only 25,000 more can go, and you are sending 60,000.');
    expect(TC().railTip([b.id, big.id], formosa.id).no).toBe('Formosa has room for 3 divisions, about 45,000 men. Formosa Big has 60,000: too many for the island.');
    expect(TC().railTip([big.id, big2.id], formosa.id).no).toBe('Formosa has room for 3 divisions, about 45,000 men. Formosa Big has 60,000: too many for the island. One more of the corps you are sending is too large for Formosa as well.');
    // A rail reason is still a rail reason, and is said as it always was: a corps with its month spent.
    b.ap = 0;
    TC().load(saveGame(g));
    expect(TC().railTip([b.id, big.id], formosa.id).no).toMatch(/^No train for Taihoku Second this month/);
    const small = corps(g, 'JAP', 'Small', taihoku);
    TC().load(saveGame(g));
    expect(TC().railTip([b.id, small.id], formosa.id).no).toMatch(/^Taihoku Second cannot go by rail, so nobody rides\. No train for Taihoku Second this month/);
  }, 300000);
});

describe('a staging island for a hop', () => {
  it("is picked for no more of the corps brought up than fit there in men; other ground counts them against its room in corps", () => {
    const g = startOpen(createGame(3, null));
    const saipan = at(g, 'Saipan'), tokyo = at(g, 'Tokyo');
    clear(g, saipan);
    // (Half a division's worth there: a division and a half of Saipan's two still left.)
    corps(g, 'JAP', 'There', saipan, 1, 0.5);
    expect(roomLeft(g, saipan, 'JAP')).toBeCloseTo(1.5, 9);
    const first = corps(g, 'JAP', 'First', tokyo), second = corps(g, 'JAP', 'Second', tokyo), half = corps(g, 'JAP', 'Half', tokyo, 1, 0.5);
    const space = roomLeft(g, saipan, 'JAP');
    expect(havenTakes(g, saipan, 'JAP', [], first, space)).toBe(true);
    expect(havenTakes(g, saipan, 'JAP', [first], second, space)).toBe(false);
    expect(havenTakes(g, saipan, 'JAP', [first], half, space)).toBe(true);
    // Ground that is no island: the count of those brought against the room left, as it always was.
    const osaka = at(g, 'Osaka');
    expect([havenTakes(g, osaka, 'JAP', [first], second, 2), havenTakes(g, osaka, 'JAP', [first, second], half, 2)]).toEqual([true, false]);
  }, 300000);
});

describe("the islands' table and its stand-ins flagged G (the design director, 14:28Z, 25 November 2026, (c))", () => {
  it("rates a stand-in that is a guess as low in its confidence column, never high, while its G stands", () => {
    const text = readFileSync(new URL('../data/campaign/tables/islands-draft.csv', import.meta.url), 'utf8');
    // (A small reader of the table's own form: commas, and quoted fields that hold commas.)
    const rows = text.split('\n').filter((l) => l.trim()).map((l) => {
      const out: string[] = []; let cur = '', q = false;
      for (let i = 0; i < l.length; i++) {
        const ch = l[i];
        if (q) { if (ch === '"' && l[i + 1] === '"') { cur += '"'; i++; } else if (ch === '"') q = false; else cur += ch; }
        else if (ch === '"') q = true; else if (ch === ',') { out.push(cur); cur = ''; } else cur += ch;
      }
      out.push(cur);
      return out;
    });
    const [head, ...body] = rows, col = (nm: string): number => head.indexOf(nm);
    const flagged = body.filter((r) => r[col('check')].startsWith('G ')).map((r) => [r[col('name')], r[col('confidence')]]);
    expect(flagged.map((r) => r[0]).sort()).toEqual(['Formosa', 'Singapore']);
    for (const [name, conf] of flagged) expect(conf, name).not.toBe('high');
    expect(Object.fromEntries(flagged)).toEqual({ Formosa: 'medium', Singapore: 'low' });
  });
});
