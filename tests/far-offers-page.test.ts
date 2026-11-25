// The offers page, on the UI director's marks of 25 November 2026, 13:39Z (pacific-japan/wip/offers-list-UI-MARKS.md,
// word for word), for the formations offered out of the player's own corps (the design director's ruling B8 of that
// day, and his rulings of 13:39Z on the batch 1 readings, design-director/pacific-japan-1125.md).
//
// A page of the bridge's own page system, id 'offers' (campaign-bridge.ts offersPage; the words in
// Rules/offers-look.ts): the head and its note, ACCEPT ALL and DECLINE ALL asking first in place on the head row, one
// row for each offer with FORM IT and DECLINE (SEND IT HOME and KEEP IT at a loan's end), and two muted sections the
// design director asked for that the marks do not word. The month card's one row, and the reports page's. A player's
// Japan, seed 5, the months walked by hand as tests/far-dated-rows-b7.test.ts walks them.
import { vi } from 'vitest';
// (The offers rule has its own switch, off until its screen passes the Mac: stage.ts OFFERS_STAGE. On for this file.)
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; (globalThis as any).TC_OFFERS = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, placed, relocate, rowOffers, setWar, startPhase } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { STACK, type Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { askForm, cardRowOf, offersLook } from '../unreal/TheaterCommand/Rules/offers-look';
import { orderDeclineFormation, orderTakeFormation } from '../src/campaign/turn';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const war = (): Game => startOpen(createGame(5, 'JAP'));
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) { g.turn = t; startPhase(g, 'JAP'); } };
/** The war handed to the 3D game in Japan's own month (or another's), and the offers page as it reads it. */
const open = (g: Game, nat = 'JAP'): any => { g.phase = g.order.indexOf(nat); expect(TC().load(saveGame(g))).not.toBeNull(); return TC().page('offers'); };
/** A row of the page as words: title, each line with its tone, and each button with whether it is lit and its tip. */
const words = (r: any): unknown[] => [r.title, r.lines.map((l: string, i: number) => `${r.tones[i] || 'plain'}: ${l}`), r.buttons.map((b: any) => `${b.label} ${b.ok ? 'lit' : 'grey'}${b.tip ? ` (${b.tip})` : ''} -> ${b.act}`)];
/** Okinawa full: five corps of Japan's on the island, so the 14th Army's 16th Division cannot form there. */
const fillOkinawa = (g: Game): void => { for (let k = 0; placed(g, g.byName.Okinawa, 'JAP').filter((u) => u.kind !== 'eng').length < STACK; k++) addCorps(g, 'JAP', `Test garrison ${k}`, 'inf', 1, 'Okinawa', {}); };
/** A player's Japan in December 1941 (month 28), at war with Britain and with Singapore his: two loans have ended. */
const singapore = (): Game => {
  const g = war();
  months(g, 1, 27);
  setWar(g, 'JAP', 'UK');
  const r = g.byName.Singapore;
  for (const u of [...r.units]) relocate(g, u, g.byName.Calcutta);
  r.ctrl = 'JAP';
  months(g, 28, 28);
  return g;
};

const FIFTH = '21st Army (5th Division)';
const NOV41 = ['25th Army (18th Division)', '14th Army (48th Division)', '14th Army (16th Division)', '16th Army (2nd Division)', '4th Tank Regiment'];
const GARRISON = 'Tientsin-Pukow railway garrison (21st Division, less its Bataan detachment)';
const ONE_NOTE = 'It would be formed out of corps you have, as history formed it. It waits for your answer until its last month, or until a corps it needs is gone.';
const MANY_NOTE = 'Each would be formed out of corps you have, as history formed it. Each waits for your answer until its last month, or until a corps it needs is gone.';
const HEAD_BUTTONS = [{ label: 'ACCEPT ALL', act: 'offers:ask:take', ok: true, tip: '' }, { label: 'DECLINE ALL', act: 'offers:ask:decline', ok: true, tip: '' }];
const UK_WAITS = [
  '2nd Tank Regiment waits for its war: it forms once you are at war with the United Kingdom.',
  '15th Army (33rd and 55th Divisions) waits for its war: it forms once you are at war with the United Kingdom.',
  '2nd Raiding Regiment (paratroops of the 1st Raiding Group) waits for its war: it forms once you are at war with the United Kingdom.',
  '16th Army troops (landing with the 2nd Division) waits for its war: it forms once you are at war with the United Kingdom.',
];

describe('the offers page', () => {
  it('October 1939, one offer: the whole page, word for word', () => {
    const g = war();
    months(g, 1, 2);
    expect(open(g)).toEqual({
      ok: true, id: 'offers', title: 'FORMATIONS OFFERED', headline: '1 formation offered', note: ONE_NOTE, top: HEAD_BUTTONS,
      sections: [
        { heading: '', text: '', rows: [{
          nat: 'JAP', title: FIFTH,
          // (13,500 men and the corps' 14,142 read the same in round figures: no line for the rest.)
          lines: [
            'Infantry, 14,000 men, to form at Taihoku.',
            'Takes your 12th Army (5th Division) at Tsinan, 14,000 men.',
            'Declining it also gives up 25th Army (5th Division), which would later be formed out of it.',
            'Offered in October 1939, open until April 1940.',
          ],
          tones: ['', '', '', 'muted'],
          buttons: [{ label: 'FORM IT', act: `offers:take:${index(FIFTH)}`, ok: true, tip: '' }, { label: 'DECLINE', act: `offers:decline:${index(FIFTH)}`, ok: true, tip: '' }],
        }] },
        // (The design director's point 7, a row waiting for its war: muted, unworded by the marks. UI director to mark.)
        { heading: 'WAITING FOR THEIR WAR', text: UK_WAITS.join('\n'), rows: [] },
      ],
    });
  }, 600000);

  it('November 1941 and after (month 30), six offers: every row, its lines and its buttons; "on" for an island; the loans that end by themselves', () => {
    const g = war();
    months(g, 1, 30);
    const p = open(g);
    expect([p.headline, p.note, p.top]).toEqual(['6 formations offered', MANY_NOTE, HEAD_BUTTONS]);
    expect(p.sections.map((x: any) => x.heading)).toEqual(['', 'LENT TO YOU, AND LEAVING BY THEMSELVES', 'WAITING FOR THEIR WAR']);
    const on = (name: string): string => `offers:take:${index(name)}`, off = (name: string): string => `offers:decline:${index(name)}`;
    const lit = (name: string): string[] => [`FORM IT lit -> ${on(name)}`, `DECLINE lit -> ${off(name)}`];
    expect(p.sections[0].rows.map(words)).toEqual([
      [NOV41[0], ['plain: Infantry, 10,000 men, to form at Hainan.', 'plain: Takes your 21st Army (18th Division) at Tungkun, 15,000 men.', 'plain: The other 5,000 are lost with the old corps.',
        'plain: Declining it also gives up 18th Division, which would later be formed out of it.', 'muted: Offered in November 1941, open until May 1942.'], lit(NOV41[0])],
      [NOV41[1], ['plain: Infantry, 12,000 men, to form at Formosa.', 'plain: Takes your Formosa Army at Taihoku, 15,000 men.', 'plain: The other 3,000 are lost with the old corps.',
        'muted: Offered in November 1941, open until May 1942.'], lit(NOV41[1])],
      // (Okinawa has no land neighbour: "on", the design director's island test.)
      [NOV41[2], ['plain: Infantry, 8,000 men, to form on Okinawa.', 'plain: Takes your General reserve (16th Division, home from China) at Osaka, 15,000 men.', 'plain: The other 8,000 are lost with the old corps.',
        'muted: Offered in November 1941, open until May 1942.'], lit(NOV41[2])],
      [NOV41[3], ['plain: Infantry, 15,000 men, to form at Sendai.', 'plain: Takes your 3rd Army (2nd Division) at Mutankiang, 15,000 men.',
        'plain: Declining it also gives up 2nd Division, which would later be formed out of it.', 'muted: Offered in November 1941, open until May 1942.'], lit(NOV41[3])],
      // ("Armoured", not "Armor": the division words.)
      [NOV41[4], ['plain: Armoured, 4,000 men, to form at Formosa.', 'plain: Takes your 4th Tank Regiment at Szepingkai, 4,000 men.', 'muted: Offered in November 1941, open until May 1942.'], lit(NOV41[4])],
      [GARRISON, ['plain: Infantry, 10,000 men, to form at Hsuchow.', 'plain: Takes your Tientsin-Pukow railway garrison (21st Division) at Hsuchow, 15,000 men.', 'plain: The other 5,000 are lost with the old corps.',
        'plain: Declining it also gives up 21st Division detachment (for Bataan), which would later be formed out of it.', 'muted: Offered in February 1942, open until August 1942.'], lit(GARRISON)],
    ]);
    // (The design director's point 2: the loans not offered leave as today, and the page says so. UI director to mark.)
    expect(p.sections[1].text).toBe([
      '23rd Army artillery (1st Artillery Group) leaves your command by itself once Hong Kong is yours, when its loan ends.',
      '14th Army artillery (1st and 8th Field Heavy Artillery) leaves your command by itself once Bataan is yours, when its loan ends.',
    ].join('\n'));
    expect(p.sections[2].text.split('\n')).toEqual(expect.arrayContaining([...UK_WAITS, '14th Army (65th Brigade) waits for its war: it forms once you are at war with the United States.']));
    // The month card's row, and the page's place in the list of rooms.
    expect(cardRowOf(g, rowOffers(g))).toEqual({ text: '6 formations offered: they wait for your answer.', amber: false, act: 'open:offers' });
  }, 600000);

  it('FORM IT shut: greyed, its reason its tip and the row\'s last line; DECLINE stays open', () => {
    const g = war();
    months(g, 1, 30);
    fillOkinawa(g);
    const why = 'There is no room for another corps at Okinawa, or near it.';
    const r = open(g).sections[0].rows.find((x: any) => x.title === NOV41[2]);
    expect(words(r)).toEqual([NOV41[2], ['plain: Infantry, 8,000 men, to form on Okinawa.', 'plain: Takes your General reserve (16th Division, home from China) at Osaka, 15,000 men.',
      'plain: The other 8,000 are lost with the old corps.', 'muted: Offered in November 1941, open until May 1942.', `muted: ${why}`],
    [`FORM IT grey (${why}) -> offers:take:${index(NOV41[2])}`, `DECLINE lit -> offers:decline:${index(NOV41[2])}`]]);
  }, 600000);

  it('the last month: the muted line in amber, and the card\'s row amber with its count', () => {
    const g = war();
    months(g, 1, 33);
    const p = open(g);
    expect(p.sections[0].rows.map((r: any) => [r.title, r.lines[r.lines.length - 1], r.tones[r.tones.length - 1]])).toEqual([
      ...NOV41.map((nm) => [nm, 'Offered in November 1941. This is its last month.', 'amber']),
      [GARRISON, 'Offered in February 1942, open until August 1942.', 'muted'],
    ]);
    expect(cardRowOf(g, rowOffers(g))).toEqual({ text: '6 formations offered, 5 in their last month.', amber: true, act: 'open:offers' });
    // One offer: the card's row in the singular.
    const one = war();
    months(one, 1, 2);
    expect(cardRowOf(one, rowOffers(one))).toEqual({ text: '1 formation offered: it waits for your answer.', amber: false, act: 'open:offers' });
  }, 600000);

  it("a loan's row: its three lines, SEND IT HOME and KEEP IT; the head and note with loans", () => {
    const g = singapore();
    const p = open(g);
    expect([p.headline, p.note]).toEqual(['5 formations offered, 2 loans ending', `${MANY_NOTE} A loan's end: send the regiment home into its division, or keep it for good.`]);
    const loan = (name: string, division: string): unknown[] => [name, [
      `plain: ${name}: lent to take Singapore until December 1941.`,
      `plain: Sent home, it goes back into the ${division}. Kept, it stays yours for good.`,
      'muted: Offered in December 1941.',
    ], [`SEND IT HOME lit -> offers:take:${index(name)}`, `KEEP IT lit -> offers:decline:${index(name)}`]];
    expect(p.sections[0].rows.slice(5).map(words)).toEqual([loan('1st Tank Regiment', '1st Tank Division'), loan('6th Tank Regiment', '2nd Tank Division')]);
  }, 600000);

  it('the head asks in place: both questions with their figures, GO BACK first and safe', () => {
    const g = war();
    months(g, 1, 30);
    fillOkinawa(g);
    open(g);
    expect(TC().act('offers:ask:take')).toMatchObject({ ok: true });
    let p = TC().page('offers');
    // Five can form: five corps of his leave the map (15,000, 15,000, 15,000, 4,000 and 15,000 men), and one is passed over.
    expect(p.note).toBe('Form all 5? 5 of your corps leave the map, 64,000 men, and 5 formations form. It cannot be undone. 1 cannot be formed this month and stays open.');
    expect(p.top).toEqual([{ label: 'GO BACK', act: 'offers:back', ok: true, tip: '', safe: true }, { label: 'FORM ALL', act: 'offers:all:take', ok: true, tip: '', danger: true }]);
    expect(TC().act('offers:back')).toMatchObject({ ok: true });
    p = TC().page('offers');
    expect([p.note, p.top]).toEqual([MANY_NOTE, HEAD_BUTTONS]);
    TC().act('offers:ask:decline');
    p = TC().page('offers');
    expect(p.note).toBe('Decline all 6? None of them will be offered again, and you keep the corps they would take.');
    expect(p.top).toEqual([{ label: 'GO BACK', act: 'offers:back', ok: true, tip: '', safe: true }, { label: 'DECLINE ALL', act: 'offers:all:decline', ok: true, tip: '', danger: true }]);
    // With loans, the question names the regiments too.
    open(singapore());
    TC().act('offers:ask:decline');
    expect(TC().page('offers').note).toBe('Decline all 7? None of them will be offered again, and you keep the corps they would take, and the regiments lent to you.');
  }, 600000);

  it("answered through the page's own acts: form one, decline one, then accept all with one that cannot form, which stays open", () => {
    const g = war();
    months(g, 1, 30);
    fillOkinawa(g);
    open(g);
    expect(TC().act(`offers:take:${index(NOV41[0])}`)).toMatchObject({ ok: true, note: `${NOV41[0]} is formed.`, open: '' });
    expect(TC().act(`offers:decline:${index(NOV41[1])}`)).toMatchObject({ ok: true, note: `${NOV41[1]} is not formed: your corps stay as they are.`, open: '' });
    expect(TC().page('offers').sections[0].rows.map((r: any) => r.title)).toEqual([NOV41[2], NOV41[3], NOV41[4], GARRISON]);
    TC().act('offers:ask:take');
    expect(TC().page('offers').note).toBe('Form all 3? 3 of your corps leave the map, 34,000 men, and 3 formations form. It cannot be undone. 1 cannot be formed this month and stays open.');
    const all = TC().act('offers:all:take');
    expect(all).toMatchObject({ ok: false, open: '' });
    expect(all.note).toBe(`3 formations are formed: ${NOV41[3]}, ${NOV41[4]} and ${GARRISON}. ${NOV41[2]} is not formed. There is no room for another corps at Okinawa, or near it.`);
    const p = TC().page('offers');
    expect([p.headline, p.sections[0].rows.map((r: any) => r.title), p.top]).toEqual(['1 formation offered', [NOV41[2]], [{ ...HEAD_BUTTONS[0], ok: false }, HEAD_BUTTONS[1]]]);
    // Declining the last one closes the page.
    expect(TC().act('offers:all:decline')).toMatchObject({ ok: true, note: `${NOV41[2]} is not formed: your corps stay as they are.`, open: 'close' });
    expect(TC().page('offers')).toEqual({ ok: false });
  }, 600000);

  it('a loan sent home and a loan kept, through the acts', () => {
    open(singapore());
    expect(TC().act(`offers:take:${index('1st Tank Regiment')}`)).toMatchObject({ ok: true, note: '1st Tank Regiment is sent home.' });
    expect(TC().act(`offers:decline:${index('6th Tank Regiment')}`)).toMatchObject({ ok: true, note: '6th Tank Regiment stays yours for good.' });
  }, 600000);

  it('not his month: every button greyed, and "It is not your month." once under the head', () => {
    const g = war();
    months(g, 1, 30);
    // (A war loaded out of Japan's month is run on to it, so the page is read on the eve of a war, which is not his
    // month to answer in either: whyNotAnswerOffers.)
    g.flags.eve = 1;
    const p = open(g);
    expect(p.note).toBe(`It is not your month.\n${MANY_NOTE}`);
    expect([...p.top, ...p.sections[0].rows.flatMap((r: any) => r.buttons)].every((b: any) => b.ok === false)).toBe(true);
    expect(p.sections[0].rows.every((r: any) => !r.lines.includes('It is not your month.'))).toBe(true);
    // Asked from out of his month, the head does not ask.
    TC().act('offers:ask:take');
    expect(TC().page('offers').top.map((b: any) => b.label)).toEqual(['ACCEPT ALL', 'DECLINE ALL']);
  }, 600000);
});

// (The skeptic's review of 25 November 2026: the cases the first build left out or said wrongly. Words the marks do
// not give are marked in offers-look.ts for the UI director.)
describe('the offers page, mended after the review', () => {
  it('not his month in the plain case: another country\'s phase. Nothing can be answered, and nothing changes', () => {
    const g = war();
    months(g, 1, 30);
    g.phase = g.order.indexOf('USA');
    expect(g.order[g.phase]).not.toBe('JAP');
    const look = offersLook(g);
    expect([look.count, look.can, look.why, look.rows.every((r) => r.can === false && r.why === 'It is not your month.')]).toEqual([6, false, 'It is not your month.', true]);
    const before = saveGame(g);
    for (const o of rowOffers(g)) { expect(orderTakeFormation(g, o.id)).toBe('It is not your month.'); expect(orderDeclineFormation(g, o.id)).toBe(false); }
    expect(saveGame(g)).toBe(before);
  }, 600000);

  it('ACCEPT ALL with one offer asks in the singular', () => {
    const g = war();
    months(g, 1, 2);
    expect(askForm(g, rowOffers(g))).toBe('Form all 1? 1 of your corps leaves the map, 14,000 men, and 1 formation forms. It cannot be undone.');
  }, 600000);

  it('ACCEPT ALL with loans: the lent regiments go home, and are not counted among the corps that leave the map', () => {
    const g = singapore();
    g.phase = g.order.indexOf('JAP');
    const list = rowOffers(g), go = list.filter((o) => !o.why && !o.lent);
    const men = go.reduce((n, o) => n + o.takes.reduce((m, t) => m + t.men, 0), 0), corps = go.reduce((n, o) => n + o.takes.length, 0);
    expect(list.filter((o) => o.lent).length).toBe(2);
    expect(askForm(g, list)).toBe(`Form all ${go.length + 2}? ${corps} of your corps leave the map, ${(Math.round(men / 1000) * 1000).toLocaleString('en-US')} men, ${go.length} formations form, and 2 lent regiments go home. It cannot be undone.`);
  }, 600000);

  it("the card's row counts a loan's end beside the formations", () => {
    const g = singapore();
    const list = rowOffers(g), forms = list.filter((o) => !o.lent).length;
    expect(list.some((o) => o.last !== null && o.last <= g.turn)).toBe(false);
    expect(cardRowOf(g, list)).toEqual({ text: `${forms} formations offered, 2 loans ending: they wait for your answer.`, amber: false, act: 'open:offers' });
  }, 600000);
});

describe("the month card's row and the reports page", () => {
  it("October 1939's card carries the row, and the reports page lists it", () => {
    TC().start({ human: 'JAP', view: 'grand', world: true });
    const end = TC().endMonth();
    expect(end.month.offersRow).toEqual({ text: '1 formation offered: it waits for your answer.', amber: false, act: 'open:offers' });
    const rep = TC().page('reports');
    expect(rep.sections[0]).toEqual({ heading: 'FORMATIONS OFFERED', text: '', rows: [{ nat: '', title: '', lines: ['1 formation offered: it waits for your answer.'], tones: [''], buttons: [{ label: 'SEE THE OFFERS', act: 'open:offers', ok: true, tip: '' }] }] });
  }, 900000);

  it('with the whole world off: no offers page, and the card\'s row is empty', () => {
    // (Japan is not on the old map: the player there is Germany.)
    TC().start({ human: 'GER', view: 'grand' });
    expect(TC().page('offers')).toEqual({ ok: false });
    const end = TC().endMonth();
    expect(end.month.offersRow).toEqual({ text: '', amber: false, act: '' });
    expect(TC().page('reports').sections.map((x: any) => x.heading)).not.toContain('FORMATIONS OFFERED');
  }, 900000);
});
