// The mender's tests for the island's room in men (26 November 2026): each finding of the sceptics that needed no
// ruling, held by a test that fails without its mend.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, havenTakes, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar, removeCorps, whyNoSwap } from '../src/campaign/game';
import { troopshipsWanted } from '../src/campaign/desks';
import { canRaise, deliver } from '../src/campaign/economy';
import { fits, roomLeft } from '../src/campaign/islands';
import { landingForce } from '../src/campaign/staff';
import { followKey, frontsClear, sailedKey } from '../src/campaign/planfronts';
import { startOpen } from '../src/campaign/staff';
import { saveGame } from '../src/campaign/save';
import { beginHuman, formationOffers, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { garrison, whyNotTakeField } from '../src/campaign/siege';
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
    expect(TC().railTip([b.id, big.id], formosa.id).no).toBe('Formosa has room for 3 divisions, about 45,000 men. The Formosa Big has 60,000: too many for the island.');
    expect(TC().railTip([big.id, big2.id], formosa.id).no).toBe('Formosa has room for 3 divisions, about 45,000 men. The Formosa Big has 60,000: too many for the island. One more of the corps you are sending is too large for Formosa as well.');
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
    type Glob = (pattern: string, options: { eager: true; as: 'raw' }) => Record<string, string>;
    const text = Object.values((import.meta as unknown as { glob: Glob }).glob('../data/campaign/tables/islands-draft.csv', { eager: true, as: 'raw' }))[0];
    // (A small reader of the table's own form: commas, and quoted fields that hold commas.)
    const rows = text.split('\n').filter((l: string) => l.trim()).map((l: string) => {
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

describe("a garrison that would take the field on an island over its room", () => {
  it("is told the island's room in men, not of another corps, the corps coming out named (the UI director, 00:19Z, 27 November 2026, U7)", () => {
    const g = war('USA');
    const wake = at(g, 'Wake');
    clear(g, wake);
    const inside = corps(g, 'USA', 'Defense Battalion', wake);
    expect(garrison(g, inside)).toBe(true);
    // (Drafts and a corps put down by hand put the field over the room: 7,500 men there beside the 15,000 inside.)
    corps(g, 'USA', 'Marines', wake, 1, 0.5);
    expect(whyNotTakeField(g, inside)).toBe('Wake has room for one division, about 15,000 men, and 8,000 of yours are there: only 7,000 more can go, and the Defense Battalion has 15,000.');
    // Ground that is no island says what it always said.
    expect(whyNotTakeField(g, { ...inside, town: undefined } as Corps)).toBe('It is not in a garrison.');
  }, 300000);
});

describe("a swap onto an island with fewer than 1,000 men's room left after it", () => {
  it("says the island would still be full after the swap, counting the men who stay, and promises no room that is not there (the UI director, 00:19Z, 27 November 2026, U2)", () => {
    const g = war('JAP');
    const formosa = at(g, 'Formosa'), taihoku = at(g, 'Taihoku');
    clear(g, formosa);
    const stays = corps(g, 'JAP', 'Stays', formosa, 3, 44500 / 45000);
    const out = corps(g, 'JAP', 'Goes Out', formosa, 1, 400 / 15000);
    const c = corps(g, 'JAP', 'Thousand Men', taihoku, 1, 1000 / 15000);
    expect(whyNoSwap(g, c, out)).toBe('Formosa would still be full after the swap: it has room for about 45,000 men, and 45,000 of yours stay there.'); // (the UI director, 00:19Z, 27 November 2026, U2)
    // With 1,000 or more left after it, the section 3 line with "stay there after the swap", as built.
    stays.str = 43000 / 45000; c.str = 3000 / 15000; out.str = 100 / 15000;
    expect(whyNoSwap(g, c, out)).toBe('Formosa has room for 3 divisions, about 45,000 men, and 43,000 of yours stay there after the swap: only 2,000 more can go, and the Thousand Men has 3,000.');
  }, 300000);
});

describe("an offered formation whose island has no room for it (the offers page, FORMATIONS OFFERED)", () => {
  it("is told the island's room in men, the section 3 line in the offer's words for the row's own men (the UI director, 00:19Z, 27 November 2026, U5)", () => {
    const g = war('JAP');
    declareWar(g, 'JAP', 'USA');
    // (By hand: the play start has no offers, and the row is opened as its month would open it.)
    g.flags.offers = 1;
    g.flags['asked:169'] = g.turn;
    const palau = at(g, 'Palau');
    const before = formationOffers(g).find((o) => o.id === 169)!;
    expect(before.name).toBe('Sakaguchi Detachment (56th Mixed Infantry Group)');
    clear(g, palau);
    corps(g, 'JAP', 'Fill Palau', palau, 2);
    for (const j of palau.nb) { const r = g.regions[j]; if (r.ctrl === 'JAP') corps(g, 'JAP', `Fill ${r.name}`, r, 5); }
    // (A row for a full island goes by sea to the nearest held harbour with room, the design director, 00:20Z, 27
    // November 2026, D1: it forms there and is told nothing. Refused only where no held harbour has room.)
    const sent = formationOffers(g).find((x) => x.id === 169)!;
    expect([sent.why, sent.region === palau.id]).toEqual([null, false]);
    const five = { nat: 'JAP', kind: 'inf', divs: 1, str: 1 / 3 } as unknown as Corps;
    for (const r of g.regions) if (r.port && r !== palau && r.ctrl === 'JAP') for (let n = 0; n < 8 && fits(g, r, 'JAP', [five], { rows: r.units.filter((u) => u.kind !== 'eng').length }); n++) corps(g, 'JAP', `Fill ${r.name} ${n}`, r);
    const o = formationOffers(g).find((x) => x.id === 169)!;
    expect(o.why).toBe('Palau is full: it has room for about 30,000 men and 30,000 of yours are there.');
    // Part full: the room, those there, what more can go, and the row's own men.
    palau.units.find((u) => u.name === 'Fill Palau')!.str = 0.9;
    expect(formationOffers(g).find((x) => x.id === 169)!.why).toBe('Palau has room for 2 divisions, about 30,000 men, and 27,000 of yours are there: only 3,000 more fit, and this takes 5,000.'); // (the UI director, 00:19Z, 27 November 2026, U5)
  }, 300000);
});

describe("a group march onto an island that takes some of it", () => {
  it("tells the corps that stayed for the island's room the island's line for their own men; refused whole, the group's (the UI director, 00:19Z, 27 November 2026, U3 and U6)", () => {
    const g = war('USA');
    const pearl = at(g, 'Pearl Harbor'), hon = at(g, 'Honolulu');
    expect(pearl.nb).toContain(hon.id);
    clear(g, pearl);
    // (26,000 of his there, inside the town, where no corps changes places with them: 4,000 more can go, Pearl Harbor's
    // room being two divisions, 30,000 men.)
    expect(garrison(g, corps(g, 'USA', 'There', pearl, 2, 26000 / 30000))).toBe(true);
    const small = corps(g, 'USA', 'Small First', hon, 1, 4000 / 15000), second = corps(g, 'USA', 'Second', hon), third = corps(g, 'USA', 'Third', hon);
    TC().load(saveGame(g));
    const whole = TC().move([second.id, third.id], pearl.id) as { moved: unknown[]; note: string };
    expect(whole.moved).toEqual([]);
    expect(whole.note).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men, and 26,000 of yours are there: only 4,000 more can go, and you are sending 30,000.');
    TC().load(saveGame(g));
    const part = TC().move([small.id, second.id], pearl.id) as { moved: { id: number }[]; note: string };
    expect(part.moved.map((x) => x.id)).toEqual([small.id]);
    expect(part.note).toBe('The Second stays behind. Pearl Harbor is full: it has room for about 30,000 men and 30,000 of yours are there.'); // (named, the UI director, 00:19Z, 27 November 2026, U6)
  }, 300000);
});

describe("the comments and test titles of the island's room cite the rulings they rest on", () => {
  it("say the design director's 21:37Z rulings as ruled, not read as recommended, and quote no island sentence in corps", () => {
    type Glob = (pattern: string, options: { eager: true; as: 'raw' }) => Record<string, string>;
    const files = {
      ...((import.meta as unknown as { glob: Glob }).glob('../src/**/*.ts', { eager: true, as: 'raw' })),
      ...((import.meta as unknown as { glob: Glob }).glob('../unreal/TheaterCommand/Rules/*.ts', { eager: true, as: 'raw' })),
      ...((import.meta as unknown as { glob: Glob }).glob('./far-island*.test.ts', { eager: true, as: 'raw' })),
    };
    expect(Object.keys(files).length).toBeGreaterThan(100);
    const stale = [/island-room map/, /Q[1-9] of the map/, /\(Q[1-9], read as recommended/, /read as the mapper recommends/, /has room for (one|[0-9]+) corps\.["”]/];
    const bad: string[] = [];
    for (const [f, src] of Object.entries(files)) {
      if (f.endsWith('far-island-room-mends.test.ts')) continue;
      src.split('\n').forEach((line: string, i: number) => { for (const re of stale) if (re.test(line)) bad.push(`${f}:${i + 1}: ${line.trim()}`); });
    }
    expect(bad).toEqual([]);
    // (A lean the UI director has since answered, 00:19Z, 27 November 2026, U1 to U8, is cited as hers, never as
    // laid until she rules: read in every file, this one too, but for the line that names them.)
    const answered = [/lean until the UI director rules/, /laid until the UI director\s*$/, /laid until the UI director rules/, /leans until she rules/];
    const open: string[] = [];
    for (const [f, src] of Object.entries(files)) {
      src.split('\n').forEach((line: string, i: number) => { if (!/const answered/.test(line)) for (const re of answered) if (re.test(line)) open.push(`${f}:${i + 1}: ${line.trim()}`); });
    }
    expect(open).toEqual([]);
    const isl = Object.entries(files).find(([f]) => f.endsWith('/campaign/islands.ts'))![1];
    // (Read across the comment's line breaks.)
    const flat = isl.replace(/\n\s*(\*|\/\/)\s*/g, ' ');
    expect(flat.split('design director, 21:37Z, 26 November 2026').length - 1).toBeGreaterThanOrEqual(5);
  });
});
