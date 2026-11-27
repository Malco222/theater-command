// An island's room in men, round 2: the new and changed sentences as a player reads them on live boards through the
// 3D game's bridge (TC), each asserted whole: the design director's D4 and D5 (00:20Z, 27 November 2026) and the UI
// director's U1, U3, U5, U6, U7 and U8 (00:19Z, 27 November 2026). (The swap's lines, U2 and U4, have no call of the
// bridge that says them: a march reads the march's line, U3. They are held in far-island-room-r2.test.ts and
// far-island-room-mends.test.ts on the rules, game.ts whyNoSwap and turn.ts swapWhy.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, declareWar, placed, removeCorps } from '../src/campaign/game';
import { fits } from '../src/campaign/islands';
import { saveGame } from '../src/campaign/save';
import { garrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
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
function corps(g: Game, nat: string, name: string, at: Region, divs = 1, str = 1): Corps {
  const c = addCorps(g, nat, name, 'inf', divs, at.id, {});
  c.str = str;
  c.ap = c.apMax;
  return c;
}
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };
/** What the board printed, for the report: one line a sentence. */
const say = (board: string, line: string): void => { process.stderr.write(`\nLIVE ${board} | ${line}`); };

describe('the round 2 sentences on live boards through the bridge', () => {
  it("D5: Singapore with five of Britain's fighting corps refuses a sixth whose men fit, in the sentence any ground that holds five says", () => {
    const g = war('UK');
    const sg = g.byName['Singapore'], jo = g.byName['Johore'];
    for (let k = 1; k <= 4; k++) corps(g, 'UK', `Small ${k}`, sg, 1, 0.2);
    // (Their month spent, so that no corps there changes places with the one sent: the march's own line.)
    for (const u of sg.units) u.ap = 0;
    const there = [...placed(g, sg, 'UK'), ...(sg.garr ?? [])].map((u) => `${u.name} (${u.kind}, ${Math.round(u.divs * u.str * 15000)})`);
    const sixth = corps(g, 'UK', 'Sixth (a test row)', jo, 1, 0.2), seventh = corps(g, 'UK', 'Seventh (a test row)', jo, 1, 0.2);
    TC().load(saveGame(g));
    const one = TC().move([sixth.id], sg.id) as { moved: unknown[]; note: string };
    const two = TC().railTip([sixth.id, seventh.id], sg.id) as { no: string };
    say('D5', `Singapore (room 5, 75,000 men); there: ${there.join('; ')}; sent from Johore: Sixth (a test row), 3,000`);
    say('D5 march', one.note);
    say('D5 rail, two sent', two.no);
    expect(one.moved).toEqual([]);
    expect(one.note).toBe('Singapore has no room for another corps.');
    expect(two.no).toBe('Singapore has no room for 2 more corps.');
  }, 300000);

  it("D4: Pearl Harbor reads the 11,000 of its field division, not its coast artillery's 15,000", () => {
    const g = war('USA');
    const pearl = g.byName['Pearl Harbor'], hon = g.byName['Honolulu'];
    const two = corps(g, 'USA', 'Two divisions (a test row)', hon, 2);
    // (Their month spent, so that no corps there changes places with the one sent: the march's own line.)
    for (const u of pearl.units) u.ap = 0;
    TC().load(saveGame(g));
    const m = TC().move([two.id], pearl.id) as { note: string };
    say('D4 march', m.note);
    expect(m.note).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men, and 11,000 of yours are there: only 19,000 more can go, and you are sending 30,000.');
  }, 300000);

  it('U1: Guam with 750 of his there, by sea', () => {
    const g = war('USA');
    const guam = g.byName['Guam'], manila = g.byName['Manila'];
    clear(g, guam);
    corps(g, 'USA', 'Guam Insular Force (a test row)', guam, 1, 750 / 15000);
    const big = corps(g, 'USA', 'Big', manila, 2);
    TC().load(saveGame(g));
    const w = TC().bySeaWhy([big.id], guam.id) as { why: string };
    say('U1 by sea', w.why);
    expect(w.why).toBe('Guam has room for 2 divisions, about 30,000 men, and 750 of yours are there: only 29,250 more can go, and you are sending 30,000.');
  }, 300000);

  it('U3 and U6: marches onto Pearl Harbor, refused whole and let in in part, the corps that stay behind named', () => {
    const g = war('USA');
    const pearl = g.byName['Pearl Harbor'], hon = g.byName['Honolulu'];
    clear(g, pearl);
    expect(garrison(g, corps(g, 'USA', 'Inside', pearl, 2, 20000 / 30000))).toBe(true);
    const small = corps(g, 'USA', 'Small', hon, 1, 4000 / 15000), d24 = corps(g, 'USA', '24th Division Group', hon), d25 = corps(g, 'USA', '25th Division Group', hon);
    TC().load(saveGame(g));
    const whole = TC().move([d24.id, d25.id], pearl.id) as { moved: unknown[]; note: string };
    say('U3 march refused whole', whole.note);
    expect(whole.moved).toEqual([]);
    expect(whole.note).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men, and 20,000 of yours are there: only 10,000 more can go, and you are sending 30,000.');
    TC().load(saveGame(g));
    const part = TC().move([small.id, d24.id, d25.id], pearl.id) as { note: string };
    say('U6 two stay behind', part.note);
    expect(part.note).toBe('The 24th Division Group and the 25th Division Group stay behind. Pearl Harbor has room for 2 divisions, about 30,000 men, and 24,000 of yours are there: only 6,000 more can go, and you are sending 30,000.');
    // Full once the small one is in: one stays behind on a full island.
    const g2 = war('USA');
    const p2 = g2.byName['Pearl Harbor'], h2 = g2.byName['Honolulu'];
    clear(g2, p2);
    expect(garrison(g2, corps(g2, 'USA', 'Inside', p2, 2, 26000 / 30000))).toBe(true);
    const s2 = corps(g2, 'USA', 'Small', h2, 1, 4000 / 15000), e24 = corps(g2, 'USA', '24th Division Group', h2);
    TC().load(saveGame(g2));
    const full = TC().move([s2.id, e24.id], p2.id) as { note: string };
    say('U6 one stays behind, full', full.note);
    expect(full.note).toBe('The 24th Division Group stays behind. Pearl Harbor is full: it has room for about 30,000 men and 30,000 of yours are there.');
  }, 300000);

  it("U5: the offers page, a formation for a part-full Palau with no other held harbour with room", () => {
    const g = war('JAP');
    declareWar(g, 'JAP', 'USA');
    g.flags.offers = 1;
    g.flags['asked:169'] = g.turn;
    const palau = g.byName['Palau'];
    clear(g, palau);
    corps(g, 'JAP', 'Fill Palau', palau, 2, 0.9);
    const five = { nat: 'JAP', kind: 'inf', divs: 1, str: 1 / 3 } as unknown as Corps;
    for (const r of g.regions) if (r.port && r !== palau && r.ctrl === 'JAP') for (let n = 0; n < 8 && fits(g, r, 'JAP', [five], { rows: r.units.filter((u) => u.kind !== 'eng').length }); n++) corps(g, 'JAP', `Fill ${r.name} ${n}`, r);
    TC().load(saveGame(g));
    const text = JSON.stringify(TC().page('offers'));
    const found = /Palau has room for[^"]*/.exec(text)?.[0] ?? '(none)';
    say('U5 offers page (TC.page offers, the row\'s tip)', found);
    expect(found).toBe('Palau has room for 2 divisions, about 30,000 men, and 27,000 of yours are there: only 3,000 more fit, and this takes 5,000.');
  }, 300000);

  it('U7: a garrison taking the field on Wake, the corps coming out named', () => {
    const g = war('USA');
    const wake = g.byName['Wake'];
    clear(g, wake);
    const inside = corps(g, 'USA', 'Defense Battalion', wake);
    expect(garrison(g, inside)).toBe(true);
    corps(g, 'USA', 'Marines', wake, 1, 0.5);
    TC().load(saveGame(g));
    const w = TC().work(`field:${inside.id}`, wake.id) as { ok: boolean; note: string };
    say('U7 take the field', w.note);
    expect(w.ok).toBe(false);
    expect(w.note).toBe('Wake has room for one division, about 15,000 men, and 8,000 of yours are there: only 7,000 more can go, and the Defense Battalion has 15,000.');
  }, 300000);

  it('U8: the too-large corps named as the news names it, by rail onto Singapore', () => {
    const g = war('UK');
    const sg = g.byName['Singapore'], jo = g.byName['Johore'];
    const huge = corps(g, 'UK', 'XIV Corps', jo, 6, 0.9), more = corps(g, 'UK', 'XV Corps', jo, 6, 1);
    TC().load(saveGame(g));
    const a = TC().railTip([huge.id], sg.id) as { no: string }, b = TC().railTip([huge.id, more.id], sg.id) as { no: string };
    say('U8 rail, too large', a.no);
    say('U8 rail, two too large', b.no);
    expect(a.no).toBe('Singapore has room for 5 divisions, about 75,000 men. The XIV Corps has 81,000: too many for the island.');
    expect(b.no).toBe('Singapore has room for 5 divisions, about 75,000 men. The XIV Corps has 81,000: too many for the island. One more of the corps you are sending is too large for Singapore as well.');
  }, 300000);
});
