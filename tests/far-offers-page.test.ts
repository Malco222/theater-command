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
import { addCorps, createGame, men, placed, relocate, removeCorps, rowCorps, rowOffers, setWar, startPhase } from '../src/campaign/game';
import { fits, islandOf, roomLeft } from '../src/campaign/islands';
import { chartDist } from '../src/campaign/wrap';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { MEN_PER_DIV, STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import { menWords } from '../src/campaign/manpower';
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
/**
 * The 14th Army's 16th Division as the rule reads its room: the corps it forms (game.ts `rowCorps`) and the corps of
 * his it takes, which leave as it forms: the offer's own `takes` once it is offered, and before that the corps its row
 * is made from (`from`), as the rule's `rowOut` reads them.
 */
const sixteenth = (g: Game): { going: Corps[]; out: Corps[] } => {
  const row = rows[index(NOV41[2])], o = rowOffers(g).find((x) => x.id === index(NOV41[2]));
  return { going: rowCorps(g, 'JAP', row, row[6] ?? {}), out: o ? o.takes.map((t) => g.corps.find((c) => c.id === t.id)!) : g.corps.filter((c) => c.nat === 'JAP' && !c.dead && (row[6].from as string[]).includes(c.name)) };
};
/** The 14th Army's 16th Division's offer as the rule gives it. */
const of = (x: Game) => rowOffers(x).find((o) => o.id === index(NOV41[2]))!;
/** A ground's fighting corps of Japan's side, as the five are counted on any ground (engineers apart). */
const standing = (g: Game, r: Region): number => placed(g, r, 'JAP').filter((u) => u.kind !== 'eng').length;
/**
 * Okinawa full for the 14th Army's 16th Division, read the way the rule reads room: full in men by the rule's own
 * reader (`fits` refuses the row's own corps) with five fighting corps standing there, so that it stays full when a
 * place among the five is asked as well (the design director, 14:13Z, 27 November 2026, (2)). Every harbour of Japan's
 * is left as it is: the row is sent on by sea to the nearest held harbour with room, Kagoshima (game.ts roomNear, the
 * design director's D1 of 00:20Z, 27 November 2026), and an offer of the player's with it, "to form at Kagoshima" with
 * FORM IT lit, as a computer's row would: her ruling of 17:53Z, 27 November 2026, (1).
 */
const okinawaFull = (g: Game): void => {
  const { going, out } = sixteenth(g), okinawa = g.byName.Okinawa;
  for (let k = 0; standing(g, okinawa) < STACK || fits(g, okinawa, 'JAP', going, { out }); k++) addCorps(g, 'JAP', `Test garrison ${k}`, 'inf', 1, 'Okinawa', {});
};
/**
 * No room for the 14th Army's 16th Division "at Okinawa, or near it": Okinawa full (`okinawaFull`), and every other
 * harbour of Japan's filled too until `fits`, asked as roomNear asks it, refuses the row there, so that no held ground
 * anywhere has room and the row waits (D1: "Where no held ground anywhere has room, it waits until one does."). The
 * board of top-up 1's harbour fill: the one offer that truly cannot form this month, and stays open.
 */
const noRoomForSixteenth = (g: Game): void => {
  const { going, out } = sixteenth(g), okinawa = g.byName.Okinawa;
  okinawaFull(g);
  for (const r of g.regions) {
    if (!r.port || r === okinawa || r.ctrl !== 'JAP') continue;
    for (let k = 0; k < 12 && fits(g, r, 'JAP', going, { rows: standing(g, r), out }); k++) addCorps(g, 'JAP', `Test harbour ${r.name} ${k}`, 'inf', 1, r.id, {});
  }
};
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
/** The UI director's line for an offer sent on by sea, her paper of 18:04Z, 27 November 2026, on this board's places. */
const MOVED_LINE = 'Okinawa has no room for it. Kagoshima is your nearest harbour with room.';
const NOV41 = ['25th Army (18th Division)', '14th Army (48th Division)', '14th Army (16th Division)', '16th Army (2nd Division)', '4th Tank Regiment'];
const GARRISON = 'Tientsin-Pukow railway garrison (21st Division, less its Bataan detachment)';
const ONE_NOTE = 'It would be formed out of corps you have, as history formed it. It waits for your answer until its last month, or until a corps it needs is gone.';
const MANY_NOTE = 'Each would be formed out of corps you have, as history formed it. Each waits for your answer until its last month, or until a corps it needs is gone.';
const HEAD_BUTTONS = [{ label: 'ACCEPT ALL', act: 'offers:ask:take', ok: true, tip: '' }, { label: 'DECLINE ALL', act: 'offers:ask:decline', ok: true, tip: '' }];
const UK_WAITS = [
  '2nd Tank Regiment waits for its war: it forms once you are at war with the United Kingdom.',
  // (This one would take his 11th Army (33rd Division): when its war begins it comes as an offer, and says so. The second marks, section 6.)
  '15th Army (33rd and 55th Divisions) is offered once you are at war with the United Kingdom.',
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
        // (The design director's point 7, a row waiting for its war: muted, in the words of the UI director's second marks of 15:23Z, section 6.)
        { heading: 'WAITING FOR A WAR', text: UK_WAITS.join('\n'), rows: [] },
      ],
    });
  }, 600000);

  it('November 1941 and after (month 30), six offers: every row, its lines and its buttons; "on" for an island; the loans that end by themselves', () => {
    const g = war();
    months(g, 1, 30);
    const p = open(g);
    expect([p.headline, p.note, p.top]).toEqual(['6 formations offered', MANY_NOTE, HEAD_BUTTONS]);
    expect(p.sections.map((x: any) => x.heading)).toEqual(['', 'LENT TO YOU', 'WAITING FOR A WAR']);
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
    // (The design director's point 2: the loans not offered leave as today, and the page says so. Its lines stand by the second marks, section 6.)
    expect(p.sections[1].text).toBe([
      '23rd Army artillery (1st Artillery Group) leaves your command by itself once Hong Kong is yours, when its loan ends.',
      '14th Army artillery (1st and 8th Field Heavy Artillery) leaves your command by itself once Bataan is yours, when its loan ends.',
    ].join('\n'));
    expect(p.sections[2].text.split('\n')).toEqual(expect.arrayContaining([...UK_WAITS, '14th Army (65th Brigade) waits for its war: it forms once you are at war with the United States.',
      '25th Army (Imperial Guards Division) is offered once you are at war with France.']));
    // The month card's row, and the page's place in the list of rooms.
    expect(cardRowOf(g, rowOffers(g))).toEqual({ text: '6 formations offered: they wait for your answer.', amber: false, act: 'open:offers' });
  }, 600000);

  it('FORM IT shut: greyed, its reason its tip and the row\'s last line; DECLINE stays open', () => {
    const g = war();
    months(g, 1, 30);
    noRoomForSixteenth(g);
    // (The no-room case comes from the fill and is read by the rule's own readers: Okinawa refuses the row's own corps,
    // has less room left than the row's men, and five fighting corps stand there.)
    const { going, out } = sixteenth(g), okinawa = g.byName.Okinawa;
    expect([fits(g, okinawa, 'JAP', going, { out }), roomLeft(g, okinawa, 'JAP') * MEN_PER_DIV < men(going[0]), standing(g, okinawa)]).toEqual([false, true, STACK]);
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
      // (The row's title is the regiment's name, so the line does not say it again: the second marks, section 3.)
      'plain: Lent to take Singapore until December 1941.',
      `plain: Sent home, it goes back into the ${division}. Kept, it stays yours for good.`,
      'muted: Offered in December 1941.',
    ], [`SEND IT HOME lit -> offers:take:${index(name)}`, `KEEP IT lit -> offers:decline:${index(name)}`]];
    expect(p.sections[0].rows.slice(5).map(words)).toEqual([loan('1st Tank Regiment', '1st Tank Division'), loan('6th Tank Regiment', '2nd Tank Division')]);
  }, 600000);

  it('the head asks in place: both questions with their figures, GO BACK first and safe', () => {
    const g = war();
    months(g, 1, 30);
    okinawaFull(g);
    // (Moved by D1, the design director's ruling of 17:53Z, 27 November 2026, (1): "Okinawa full means 'to form at
    // Kagoshima' with FORM IT lit, as a computer's row would." Okinawa refuses the row's own corps and five stand there;
    // Kagoshima, a harbour of Japan's, has room for it. So all six can form, and none is passed over. Before her ruling
    // this read "Form all 5? ... 1 cannot be formed this month and stays open.": that case is now tested on a board
    // with no room anywhere, below, "answered through the page's own acts".)
    const { going, out } = sixteenth(g), okinawa = g.byName.Okinawa, kagoshima = g.byName.Kagoshima;
    expect([fits(g, okinawa, 'JAP', going, { out }), roomLeft(g, okinawa, 'JAP') * MEN_PER_DIV < men(going[0]), standing(g, okinawa), fits(g, kagoshima, 'JAP', going, { rows: standing(g, kagoshima), out })]).toEqual([false, true, STACK, true]);
    const r = open(g).sections[0].rows.find((x: any) => x.title === NOV41[2]);
    // (The second line is the UI director's, 18:04Z, 27 November 2026: why the place moved.)
    expect(words(r)).toEqual([NOV41[2], ['plain: Infantry, 8,000 men, to form at Kagoshima.', `plain: ${MOVED_LINE}`, 'plain: Takes your General reserve (16th Division, home from China) at Osaka, 15,000 men.',
      'plain: The other 8,000 are lost with the old corps.', 'muted: Offered in November 1941, open until May 1942.'],
    [`FORM IT lit -> offers:take:${index(NOV41[2])}`, `DECLINE lit -> offers:decline:${index(NOV41[2])}`]]);
    expect(TC().act('offers:ask:take')).toMatchObject({ ok: true });
    let p = TC().page('offers');
    // The head's figures from the rule's own readers: the offers that can form this month (no `why`), each corps of his
    // they take counted once, with its men by the rule's `men`, said in the page's round figures (`menWords`).
    const all = rowOffers(g), go = all.filter((o) => !o.why);
    const struck = [...new Set(go.flatMap((o) => o.takes.map((t) => t.id)))].map((id) => g.corps.find((c) => c.id === id && !c.dead)!);
    const gone = struck.reduce((n, c) => n + men(c), 0);
    expect([all.length, go.length, struck.length]).toEqual([6, 6, 6]);
    expect(p.note).toBe(`Form all ${go.length}? ${struck.length} of your corps leave the map, ${menWords(gone)} men, and ${go.length} formations form. It cannot be undone.`);
    // Six can form: six corps of his leave the map (15,000, 15,000, 15,000, 15,000, 4,000 and 15,000 men), and none is passed over.
    expect(p.note).toBe('Form all 6? 6 of your corps leave the map, 79,000 men, and 6 formations form. It cannot be undone.');
    expect(p.top).toEqual([{ label: 'GO BACK', act: 'offers:back', ok: true, tip: '', safe: true }, { label: 'FORM ALL', act: 'offers:all:take', ok: true, tip: '', danger: true }]);
    expect(TC().act('offers:back')).toMatchObject({ ok: true });
    p = TC().page('offers');
    expect([p.note, p.top]).toEqual([MANY_NOTE, HEAD_BUTTONS]);
    TC().act('offers:ask:decline');
    p = TC().page('offers');
    expect(p.note).toBe('Decline all 6? None of them will be offered again. You keep the corps they would take.');
    expect(p.top).toEqual([{ label: 'GO BACK', act: 'offers:back', ok: true, tip: '', safe: true }, { label: 'DECLINE ALL', act: 'offers:all:decline', ok: true, tip: '', danger: true }]);
    // With loans, the question names the regiments too.
    open(singapore());
    TC().act('offers:ask:decline');
    expect(TC().page('offers').note).toBe('Decline all 7? None of them will be offered again. You keep the corps they would take, and the regiments lent to you.');
  }, 600000);

  it("answered through the page's own acts: form one, decline one, then accept all with one that cannot form, which stays open", () => {
    const g = war();
    months(g, 1, 30);
    // (Moved by D1, the design director's ruling of 17:53Z, 27 November 2026, (1): with Okinawa alone full the 16th
    // Division is sent on to Kagoshima and can form, so the one that cannot form is made on a board where no held
    // ground anywhere has room for it, top-up 1's harbour fill: Okinawa full and every other harbour of Japan's too.
    // Read by the rule's own readers: Okinawa refuses the row's own corps, and no harbour of Japan's takes it.)
    noRoomForSixteenth(g);
    const { going, out } = sixteenth(g), okinawa = g.byName.Okinawa;
    expect([fits(g, okinawa, 'JAP', going, { out }), standing(g, okinawa), g.regions.filter((r) => r.port && r.ctrl === 'JAP' && fits(g, r, 'JAP', going, { rows: standing(g, r), out })).map((r) => r.name)]).toEqual([false, STACK, []]);
    open(g);
    expect(TC().act(`offers:take:${index(NOV41[0])}`)).toMatchObject({ ok: true, note: `${NOV41[0]} is formed.`, open: '' });
    expect(TC().act(`offers:decline:${index(NOV41[1])}`)).toMatchObject({ ok: true, note: `${NOV41[1]} is declined: your corps stay as they are.`, open: '' });
    expect(TC().page('offers').sections[0].rows.map((r: any) => r.title)).toEqual([NOV41[2], NOV41[3], NOV41[4], GARRISON]);
    TC().act('offers:ask:take');
    // The head's figures from the rule's own readers: the offers that can form (no `why`), each corps of his they take
    // counted once with its men by `men`; the one passed over is the 16th Division, for want of room. (Read on the war
    // as the page's acts have left it: the bridge's own, through its save.)
    const h = loadGame(TC().save().text)!;
    const left = rowOffers(h), go = left.filter((o) => !o.why), passed = left.filter((o) => o.why);
    const struck = [...new Set(go.flatMap((o) => o.takes.map((t) => t.id)))].map((id) => h.corps.find((c) => c.id === id && !c.dead)!);
    expect([go.map((o) => o.name), passed.map((o) => [o.name, o.why])]).toEqual([[NOV41[3], NOV41[4], GARRISON], [[NOV41[2], 'There is no room for another corps at Okinawa, or near it.']]]);
    expect(TC().page('offers').note).toBe(`Form all ${go.length}? ${struck.length} of your corps leave the map, ${menWords(struck.reduce((n, c) => n + men(c), 0))} men, and ${go.length} formations form. It cannot be undone. ${passed.length} cannot be formed this month and stays open.`);
    expect(TC().page('offers').note).toBe('Form all 3? 3 of your corps leave the map, 34,000 men, and 3 formations form. It cannot be undone. 1 cannot be formed this month and stays open.');
    const all = TC().act('offers:all:take');
    expect(all).toMatchObject({ ok: false, open: '' });
    expect(all.note).toBe(`3 formations are formed: ${NOV41[3]}, ${NOV41[4]} and ${GARRISON}. ${NOV41[2]} is not formed. There is no room for another corps at Okinawa, or near it.`);
    const p = TC().page('offers');
    expect([p.headline, p.sections[0].rows.map((r: any) => r.title), p.top]).toEqual(['1 formation offered', [NOV41[2]], [{ ...HEAD_BUTTONS[0], ok: false }, HEAD_BUTTONS[1]]]);
    // Declining the last one closes the page.
    expect(TC().act('offers:all:decline')).toMatchObject({ ok: true, note: `${NOV41[2]} is declined: your corps stay as they are.`, open: 'close' });
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

// (The design director's ruling of 17:53Z, 27 November 2026, (1): "D1 reaches a formation offered to the player: one
// rule for every seat. Okinawa full means 'to form at Kagoshima' with FORM IT lit, as a computer's row would. ... Card
// and result agree.")
describe('an offer sent on by sea: card and result agree, in every seat', () => {
  it('where the page says "to form at Kagoshima", FORM IT puts the corps at Kagoshima and nowhere else, and a computer\'s Japan forms the same row there in the same month', () => {
    // The player: Okinawa full before the row's month, November 1941 (month 27), so the offer is made sent on.
    const g = war();
    months(g, 1, 26);
    okinawaFull(g);
    months(g, 27, 27);
    const r = open(g).sections[0].rows.find((x: any) => x.title === NOV41[2]);
    expect([r?.lines[0], r?.buttons[0].label, r?.buttons[0].ok]).toEqual(['Infantry, 8,000 men, to form at Kagoshima.', 'FORM IT', true]);
    expect(TC().act(`offers:take:${index(NOV41[2])}`)).toMatchObject({ ok: true, note: `${NOV41[2]} is formed.` });
    const h = loadGame(TC().save().text)!;
    const at = (x: Game): string[] => x.corps.filter((c) => c.nat === 'JAP' && !c.dead && c.name === NOV41[2]).map((c) => x.regions[c.reg].name);
    expect([h.turn, at(h)]).toEqual([27, ['Kagoshima']]);
    // The computer: the same row on the same board (Okinawa filled the same way, in the same month) forms at
    // Kagoshima in month 27: one rule for every seat.
    const c = startOpen(createGame(5, null));
    months(c, 1, 26);
    okinawaFull(c);
    const k = c.byName.Kagoshima, ok = c.byName.Okinawa, gk = g.byName.Kagoshima;
    expect([standing(c, ok), standing(c, k)]).toEqual([standing(g, g.byName.Okinawa), standing(g, gk) - 1]);
    months(c, 27, 27);
    expect([c.flags[`sent:${index(NOV41[2])}`], at(c)]).toEqual([1, ['Kagoshima']]);
  }, 600000);

  it('the fact the page is handed: from Okinawa to Kagoshima when Okinawa is full; none with room on Okinawa; none with no room anywhere, where FORM IT is grey with the old tip', () => {
    const g = war();
    months(g, 1, 30);
    const okinawa = g.byName.Okinawa, kagoshima = g.byName.Kagoshima;
    // Room on Okinawa: it forms there, and nothing moved.
    expect(of(g)).toMatchObject({ region: okinawa.id, why: null, moved: null });
    // Okinawa full: the rule sends it on to Kagoshima, and says so once, in its own placing (`rowPlace`).
    okinawaFull(g);
    expect(of(g)).toMatchObject({ region: kagoshima.id, why: null });
    expect(of(g).moved).toEqual({ from: okinawa.id, to: kagoshima.id, by: 'sea' });
    // No room anywhere: it is not sent on; it waits at Okinawa, FORM IT grey with the old tip.
    const n = war();
    months(n, 1, 30);
    noRoomForSixteenth(n);
    const why = 'There is no room for another corps at Okinawa, or near it.';
    expect(of(n)).toMatchObject({ region: okinawa.id, why, moved: null });
    const r = open(n).sections[0].rows.find((x: any) => x.title === NOV41[2]);
    expect([r.lines[0], r.buttons[0]]).toEqual(['Infantry, 8,000 men, to form on Okinawa.', { label: 'FORM IT', act: `offers:take:${index(NOV41[2])}`, ok: false, tip: why }]);
  }, 600000);
});

// (The UI director's line for an offer sent on by sea, her paper of 18:04Z, 27 November 2026,
// pacific-japan/wip/island-room-in-men-UI-MARKS.md, "The offer moved to a harbour": "the row's second line, plain:
// Okinawa has no room for it. Kagoshima is your nearest harbour with room." ... "in every month the offer stands
// moved. It is gone in the month Okinawa has room for it again, when the first line reads 'on Okinawa'.")
describe("the UI director's line for an offer sent on by sea", () => {
  it('the row\'s second line, plain, in every month the offer stands moved, and gone in the month Okinawa has room again', () => {
    const g = war();
    months(g, 1, 26);
    okinawaFull(g);
    const lines = (): unknown[] => { const r = open(g).sections[0].rows.find((x: any) => x.title === NOV41[2]); return words(r)[1] as unknown[]; };
    for (const t of [27, 28]) {
      months(g, t, t);
      expect([t, ...lines().slice(0, 2)]).toEqual([t, 'plain: Infantry, 8,000 men, to form at Kagoshima.', `plain: ${MOVED_LINE}`]);
    }
    // Room on Okinawa again in month 29: the first line reads "on Okinawa", and the line is gone.
    for (const c of g.corps.filter((x) => /^Test garrison/.test(x.name) && !x.dead)) removeCorps(g, c);
    months(g, 29, 29);
    expect(of(g).moved).toBeNull();
    expect(lines()).toEqual(['plain: Infantry, 8,000 men, to form on Okinawa.', 'plain: Takes your General reserve (16th Division, home from China) at Osaka, 15,000 men.',
      'plain: The other 8,000 are lost with the old corps.', 'muted: Offered in November 1941, open until May 1942.']);
  }, 600000);

  // (Her fact to confirm, by a run: "'with room' must mean room for the whole formation ... If the rule can choose a
  // harbour with less room than the formation takes, tell me before the line ships." The sea branch asks `fits` with
  // the row's own corps: a harbour with room for less than the whole formation is passed over, and the line names the
  // next one, where the corps forms whole.)
  it('"your nearest harbour with room" is room for the whole formation: a nearer harbour with less room is passed over, the line names the next, and the corps forms there whole', () => {
    // Japan's harbours by the sea's own distance from Okinawa, at the board's month, as the sea branch orders them.
    const board = (): Game => { const g = war(); months(g, 1, 30); okinawaFull(g); return g; };
    const near = (g: Game): Region[] => g.regions.filter((r) => r.port && r.ctrl === 'JAP' && r !== g.byName.Okinawa).sort((a, b) => chartDist(g, a, g.byName.Okinawa) - chartDist(g, b, g.byName.Okinawa) || a.id - b.id);
    const formed = (g: Game, name: string): unknown[] => {
      const r = open(g).sections[0].rows.find((x: any) => x.title === name);
      const said = (words(r)[1] as string[]).slice(0, 2), ok = r.buttons[0].ok;
      TC().act(`offers:take:${index(name)}`);
      const h = loadGame(TC().save().text)!;
      const c = h.corps.filter((x) => x.nat === 'JAP' && !x.dead && x.name === name);
      return [said, ok, c.map((x) => [h.regions[x.reg].name, men(x)])];
    };
    // (a) A mainland harbour with five corps standing: Kagoshima, the nearest, packed by count until the rule's own
    // `fits` (five rows) refuses the row there. It is passed over for Taihoku, the next, which has room for it whole.
    const a = board();
    const [k1, t1] = near(a);
    expect([k1.name, !!islandOf(a, k1), t1.name, !!islandOf(a, t1)]).toEqual(['Kagoshima', false, 'Taihoku', true]);
    const { going, out } = sixteenth(a), whole = men(going[0]);
    for (let k = 0; standing(a, k1) < STACK || fits(a, k1, 'JAP', going, { rows: standing(a, k1), out }); k++) addCorps(a, 'JAP', `Pack ${k}`, 'inf', 1, k1.id, {});
    expect([standing(a, k1), fits(a, k1, 'JAP', going, { rows: standing(a, k1), out }), fits(a, t1, 'JAP', going, { rows: standing(a, t1), out }), roomLeft(a, t1, 'JAP') * MEN_PER_DIV >= whole]).toEqual([STACK, false, true, true]);
    expect(formed(a, NOV41[2])).toEqual([['plain: Infantry, 8,000 men, to form at Taihoku.', 'plain: Okinawa has no room for it. Taihoku is your nearest harbour with room.'], true, [['Taihoku', whole]]]);
    // (b) And an island harbour of the table with room for one man fewer than the formation takes: Taihoku, the next
    // after Kagoshima, so filled by men that its room left is the row's men less one. It is passed over too, for the
    // next harbour, Nagasaki, which has room for the whole formation; the corps forms there whole.
    const b = board();
    const [k2, t2, n2] = near(b);
    for (let k = 0; standing(b, k2) < STACK || fits(b, k2, 'JAP', going, { rows: standing(b, k2), out }); k++) addCorps(b, 'JAP', `Pack ${k}`, 'inf', 1, k2.id, {});
    const left = roomLeft(b, t2, 'JAP') * MEN_PER_DIV, fill = addCorps(b, 'JAP', 'Fill Taihoku', 'inf', 1, t2.id, {});
    fill.str = (left - (whole - 1)) / MEN_PER_DIV;
    expect([t2.name, Math.round(roomLeft(b, t2, 'JAP') * MEN_PER_DIV), fits(b, t2, 'JAP', going, { rows: standing(b, t2), out }), n2.name, fits(b, n2, 'JAP', going, { rows: standing(b, n2), out })]).toEqual(['Taihoku', whole - 1, false, 'Nagasaki', true]);
    expect(formed(b, NOV41[2])).toEqual([['plain: Infantry, 8,000 men, to form at Nagasaki.', 'plain: Okinawa has no room for it. Nagasaki is your nearest harbour with room.'], true, [['Nagasaki', whole]]]);
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
    // The way back to the card, as a battle report has it; never beside a question, whose two answers stand alone.
    expect(TC().page('offers').top.map((b: any) => [b.label, b.act])).toEqual([["BACK TO THE MONTH'S REPORT", 'open:reports'], ['ACCEPT ALL', 'offers:ask:take'], ['DECLINE ALL', 'offers:ask:decline']]);
    TC().act('offers:ask:decline');
    expect(TC().page('offers').top.map((b: any) => b.label)).toEqual(['GO BACK', 'DECLINE ALL']);
    TC().act('offers:back');
    // The card opened once more shows the row as the offers stand now: declined, it is gone from the card and the list.
    expect(rep.month.offersRow.text).toBe('1 formation offered: it waits for your answer.');
    TC().act('offers:ask:decline');
    TC().act('offers:all:decline');
    expect(TC().page('offers')).toEqual({ ok: false });
    const again = TC().page('reports');
    expect(again.month.offersRow).toEqual({ text: '', amber: false, act: '' });
    expect(again.sections.map((x: any) => x.heading)).not.toContain('FORMATIONS OFFERED');
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
