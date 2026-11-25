// The raid report (the patch after 1.5.1). Brian, 24 November 2026: "i did a fighter raid from germany and it was
// intercepted by the UK but it didnt't tell me the results of the battle." A raid his crews fly is settled when it is
// ordered and its report is filed; the 3D game showed him one line under the map and nothing more. The report is now
// a page of its own ('airreport:<id>'), the raids of the month are listed with theirs ('airreports'), and a raid that
// has been flown names its report as the page that comes next.
import { describe, expect, it } from 'vitest';
import { AIR, BROKE_OFF_SKY, skyWords } from '../src/campaign/air';
import { FLIP } from '../src/campaign/airfought';
import { airAct, airPage, raidsToRead } from '../src/campaign/airpages';
import { launchRaid, planRaid, raidTargets } from '../src/campaign/airwar';
import { allied, createGame, placeName } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { AirReport, Game } from '../src/campaign/types';
import { turnToDate } from '../src/core/ticket';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Kind = 'bomb' | 'night' | 'ground' | 'superiority';
type Call = (...a: unknown[]) => unknown;
type Seen = NonNullable<ReturnType<typeof airPage>>;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

const grand = (human: string, seed = 7): Game => startOpen(createGame(seed, human));
const page = (g: Game, id: string): Seen => airPage(g, id)!;
const rowsOf = (p: Seen) => p.sections.flatMap((s) => s.rows);
const section = (p: Seen, heading: string) => p.sections.find((s) => s.heading === heading);

/** A raid a nation can fly now, on ground `on` holds if given, met by fighters or not as asked: what to give launchRaid(). */
function raid(g: Game, by: string, kind: Kind, met: boolean, on?: string): { target: number; ids: number[] } {
  for (const t of raidTargets(g, kind, by)) {
    if (on && g.regions[t.id].ctrl !== on) continue;
    const p = planRaid(g, kind, t.id, undefined, by);
    if (!p.why && p.picked.length && !!p.meets.length === met) return { target: t.id, ids: p.picked };
  }
  throw new Error(`no ${kind} raid for ${by}, ${met ? 'met' : 'not met'}`);
}
/** That raid flown, and its report. */
function flown(g: Game, by: string, kind: Kind, met: boolean, on?: string): AirReport {
  const { target, ids } = raid(g, by, kind, met, on);
  const r = launchRaid(g, kind, target, ids, by);
  if (!r) throw new Error(`the ${kind} raid was not flown`);
  expect(r.def.length > 0, 'fighters rose as the plan said they would').toBe(met);
  return r;
}
const lost = (r: AirReport, side: 'att' | 'def'): number => r[side].reduce((n, x) => n + x.lostFtr + x.lostBmr, 0);
const n0 = (n: number): string => Math.round(n).toLocaleString('en-US');
/** A loss as the report words it: the number, or "no". */
const w0 = (n: number): string => (n >= 1 ? n0(n) : 'no');

describe('a raid that has been flown has its report (the patch after 1.5.1)', () => {
  it('a bombing raid his crews flew, met by fighters: what came of it, who flew, who rose, what each lost, and the ticket', () => {
    const g = grand('GER');
    expect(humanUp(g)).toBe(true);
    const r = flown(g, 'GER', 'bomb', true);
    const p = page(g, `airreport:${r.id}`);
    expect(p.id).toBe(`airreport:${r.id}`);
    expect(p.title).toBe(`THE RAID REPORT  ·  ${r.place.toUpperCase()}`);
    expect(p.headline).toBe(`Bombing of ${r.place}: ${r.won ? 'raid succeeded' : 'raid cost too much'}`);
    // The month it was flown in, the report's own words, then the bill for both sides.
    expect(p.note.startsWith('September 1939. ')).toBe(true);
    expect(r.result.length).toBeGreaterThan(10);
    expect(p.note).toContain(r.result);
    expect(p.note).toContain(`The raid lost ${w0(lost(r, 'att'))} aircraft. The defence lost ${w0(lost(r, 'def'))}.`);
    // (Never "of" a number sent: a report keeps a group's whole strength, not the share of it that flew or rose.)
    expect(p.note).not.toMatch(/lost [\d,]+ of /);
    expect(p.sections.map((s) => s.heading)).toEqual(['FLEW THE RAID', 'ROSE TO MEET THEM', 'THE BATTLE TICKET']);
    // Every group that flew and every group that rose has its row, under its own flag.
    const flew = section(p, 'FLEW THE RAID')!.rows, rose = section(p, 'ROSE TO MEET THEM')!.rows;
    expect(flew.map((x) => x.nat)).toEqual(r.att.map((x) => x.nat));
    expect(rose.map((x) => x.nat)).toEqual(r.def.map((x) => x.nat));
    r.att.forEach((x, i) => { expect(flew[i].title.startsWith(`${x.fleet}  ·  `)).toBe(true); expect(flew[i].lines[0]).toMatch(/(Lost \d[\d,]* (fighter|bomber)s?(, \d[\d,]* bombers?)?\.|No losses\.)$/); });
    r.def.forEach((x, i) => expect(rose[i].title.startsWith(`${x.fleet}  ·  `)).toBe(true));
    // (A row titled with a group's bare name is given that group's large picture by the 3D game: these are the report's rows, and are not.)
    for (const row of [...flew, ...rose]) { expect(g.airFleets.some((f) => f.name === row.title)).toBe(false); expect(row.buttons).toEqual([]); }
    // The ticket, line for line.
    expect(r.lines.length).toBeGreaterThan(0);
    expect(section(p, 'THE BATTLE TICKET')!.text.split('\n').length).toBe(r.lines.length);
    expect(section(p, 'THE BATTLE TICKET')!.text).toContain(`${r.lines[0].label}: ${r.lines[0].value}`);
    // From it: the month's raids, the next raid, and the place on the map.
    expect(p.top.map((b) => [b.label, b.act, b.ok])).toEqual([
      ["THE MONTH'S RAIDS", 'open:airreports', true],
      ['PLAN A RAID', 'open:raids', true],
      [`GO TO ${r.place.toUpperCase()}`, `goto:region:${r.region}`, true],
    ]);
  });

  it('aircraft are counted with their nouns: one bomber is not "1 bombers"', () => {
    const g = grand('GER');
    const r = flown(g, 'GER', 'bomb', true);
    const a = r.att.find((x) => x.bmr > 0)!;
    a.lostBmr = 1; a.lostFtr = 0;
    const row = () => section(page(g, `airreport:${r.id}`), 'FLEW THE RAID')!.rows[r.att.indexOf(a)];
    expect(row().lines[0].endsWith('Lost 1 bomber.')).toBe(true);
    a.lostBmr = 12; a.lostFtr = 1;
    expect(row().lines[0].endsWith('Lost 1 fighter, 12 bombers.')).toBe(true);
    a.lostBmr = 0; a.lostFtr = 0;
    expect(row().lines[0].endsWith('No losses.')).toBe(true);
    a.ftr = 1; a.bmr = 1;
    expect(row().title).toBe(`${a.fleet}  ·  1 fighter, 1 bomber`);
  });

  it('a raid no fighter rose to meet: unopposed, and it says no fighters came up', () => {
    const g = grand('GER');
    const r = flown(g, 'GER', 'bomb', false);
    const p = page(g, `airreport:${r.id}`);
    expect(p.headline).toBe(`Bombing of ${r.place}: unopposed`);
    expect(p.note).toContain(`The raid lost ${w0(lost(r, 'att'))} aircraft. No fighters came up.`);
    expect(section(p, 'ROSE TO MEET THEM')).toEqual({ heading: 'ROSE TO MEET THEM', text: 'No fighters came up.', rows: [] });
    expect(section(p, 'THE SHIPS UNDER ATTACK')).toBeUndefined();
  });

  it('a fight for the sky is won or lost outright, and says how the fighters stood against what it takes', () => {
    const g = grand('GER');
    const r = flown(g, 'GER', 'superiority', true);
    const p = page(g, `airreport:${r.id}`);
    // The fight's own level, and beside it what it was flown for: a sweep can win its fight and not win the sky.
    expect(p.headline.startsWith(`The sky over ${r.place}: ${r.level.toLowerCase()}, `)).toBe(true);
    expect(p.headline).toMatch(r.sky ? /sky won$/ : /sky not won$/);
    if (r.sky) expect(p.note).toContain(`The German air force won the sky over ${placeName(r.place)}`);
    else expect(p.note).toContain('The sky was not won: ');
    // The page says the one sentence the browser's report card says (air.ts skyWords): the game and the browser agree.
    expect(p.note).toContain(skyWords(g, r));
    // Either way round, on figures of its own: fighters enough left, and short of what it takes.
    const left = r.att.reduce((n, x) => n + x.ftr - x.lostFtr, 0);
    expect(left).toBeGreaterThanOrEqual(AIR.sky.least);
    r.sky = false; r.hold = 0.67;
    expect(page(g, `airreport:${r.id}`).note).toContain(`The sky was not won: at the end the attackers' fighters stood about 0.67 to 1 against those the defenders could still put up, and it takes ${AIR.sky.win} to 1.`);
    r.sky = true; r.hold = 2.5;
    expect(page(g, `airreport:${r.id}`).note).toContain(`The German air force won the sky over ${placeName(r.place)}: at the end its fighters stood about 2.50 to 1 against those the defenders could still put up.`);
    // The rules write 99 where the defence can put nothing more up. While a defender that rose still flies the figure
    // is given, as "more than 90 to 1" (the UI director's final read, 15:42Z); "no opposition was left" only where
    // none rose or every one that rose was lost.
    r.hold = 99;
    expect(r.def.some((x) => x.lostFtr < x.ftr)).toBe(true);
    expect(page(g, `airreport:${r.id}`).note).toContain(`won the sky over ${placeName(r.place)}: at the end its fighters stood more than 90 to 1 against those the defenders could still put up.`);
    expect(page(g, `airreport:${r.id}`).note).not.toContain('99');
    r.def = r.def.map((x) => ({ ...x, lostFtr: x.ftr }));
    expect(page(g, `airreport:${r.id}`).note).toContain(`won the sky over ${placeName(r.place)}: at the end no opposition was left.`);
    r.def = [];
    expect(page(g, `airreport:${r.id}`).note).toContain(`won the sky over ${placeName(r.place)}: at the end no opposition was left.`);
    expect(page(g, `airreport:${r.id}`).note).not.toContain('times');
  });

  it('says "but" where the fight and the sky disagree, and a comma where they agree, for the side that reads it', () => {
    const g = grand('GER');
    g.turn = 2;
    const r = flown(g, 'GER', 'superiority', true, 'FRA');
    const mine = (level: AirReport['level'], sky: boolean): string => { r.level = level; r.sky = sky; return page(g, `airreport:${r.id}`).headline.replace(`The sky over ${r.place}: `, ''); };
    expect(mine('Close victory', false)).toBe('close victory, but the sky not won');
    expect(mine('Close victory', true)).toBe('close victory, sky won');
    expect(mine('Close defeat', false)).toBe('close defeat, sky not won');
    expect(mine('Decisive defeat', true)).toBe('decisive defeat, but the sky won');
    // The list's row says the same, as a sentence.
    r.level = 'Close victory'; r.sky = false;
    expect(section(page(g, 'airreports'), 'FLOWN BY YOUR SIDE')!.rows[0].lines[0].startsWith('Close victory, but the sky not won. The raid lost ')).toBe(true);
    // Flown against him: the fight is read from his side, and the sky is his to hold or lose.
    const on: AirReport = { ...r, id: 9001, attNat: 'FRA', defNat: 'GER', att: r.def, def: r.att };
    g.airReports.push(on);
    const his = (level: AirReport['level'], sky: boolean): string => { on.level = level; on.sky = sky; return page(g, 'airreport:9001').headline.replace(`The sky over ${on.place}: `, ''); };
    expect(his('Close victory', false)).toBe('close defeat, but the sky held');
    expect(his('Close defeat', false)).toBe('close victory, sky held');
    expect(his('Decisive victory', true)).toBe('decisive defeat, sky lost');
    expect(his('Close defeat', true)).toBe('close victory, but the sky lost');
  });

  it('a sweep that could not win the sky for want of fighters is told so, never a ratio of 99 or 0', () => {
    const g = grand('GER');
    const r = flown(g, 'GER', 'superiority', true);
    // Flown by bombers alone and met by nobody: the rules leave the sky unwon and write the ratio as 99.
    r.sky = false; r.hold = 99; r.def = [];
    r.att = r.att.map((x) => ({ ...x, ftr: 0, lostFtr: 0, bmr: 250, lostBmr: 5 }));
    let note = page(g, `airreport:${r.id}`).note;
    expect(note).toContain(`The sky was not won: the defenders put up no fighters, but it takes at least ${AIR.sky.least} fighters still over the target at the end as well, and the attackers sent none.`);
    expect(note).not.toContain('99');
    expect(note).not.toContain(' to 1');
    // Bombers alone against a defence that rose: the ratio is 0, and is not quoted either.
    r.hold = 0;
    note = page(g, `airreport:${r.id}`).note;
    expect(note).toContain('and none flew.');
    expect(note).not.toContain('0.00');
    // A handful of fighters left, far more than the defence: still too few to hold a sky.
    r.att = r.att.map((x, i) => ({ ...x, ftr: i ? 0 : 30, lostFtr: 0 }));
    r.hold = 4;
    note = page(g, `airreport:${r.id}`).note;
    expect(note).toContain(`The sky was not won: at the end the attackers' fighters stood about 4.00 to 1 against those the defenders could still put up, which is enough, but it takes at least ${AIR.sky.least} fighters still over the target at the end as well, and the attackers had 30 left.`);
    expect(note).not.toContain('none flew');
    // Some flew and every one was lost: none were left, which is not "none flew", and no ratio of nothing is quoted.
    r.att = r.att.map((x, i) => ({ ...x, ftr: i ? 0 : 30, lostFtr: i ? 0 : 30 }));
    r.hold = 0;
    note = page(g, `airreport:${r.id}`).note;
    expect(note).toContain(`The sky was not won: it takes at least ${AIR.sky.least} fighters still over the target at the end, and none were left.`);
    expect(note).not.toContain('0.00');
    // One left reads as one.
    r.att = r.att.map((x, i) => ({ ...x, ftr: i ? 0 : 30, lostFtr: i ? 0 : 29 }));
    r.hold = 4;
    expect(page(g, `airreport:${r.id}`).note).toContain('fighters still over the target at the end as well, and the attackers had 1 left.');
    // Fighters enough left, odds enough, and the sky still not won: the odds are given, and that it takes fighters
    // enough over the target as well (too few could stay over a target at the edge of their range)...
    r.att = r.att.map((x, i) => ({ ...x, ftr: i ? 0 : 158, lostFtr: 0 }));
    r.hold = 1.8; r.result = '';
    note = page(g, `airreport:${r.id}`).note;
    expect(note).toContain(`The sky was not won: at the end the attackers' fighters stood about 1.80 to 1 against those the defenders could still put up, which is enough, but it takes at least ${AIR.sky.least} fighters still over the target at the end as well.`);
    expect(note).not.toContain('the attackers had');
    // ...or they gave the fight up before it was joined (the rules' own sentence for that, which the note also carries).
    r.result = BROKE_OFF_SKY;
    note = page(g, `airreport:${r.id}`).note;
    expect(note).toContain(`${BROKE_OFF_SKY} `);
    expect(note).toContain('The sky was not won: the raid was broken off.');
    expect(note).not.toContain(' to 1');
    // Nothing left to oppose them: no ratio of 99.
    r.hold = 99;
    note = page(g, `airreport:${r.id}`).note;
    expect(note).toContain('The sky was not won: the raid was broken off.');
    expect(note).not.toContain('99');
  });

  it('a raid that did not pay is never said to have been beaten off: its bombs may have landed', () => {
    const g = grand('GER');
    const r = flown(g, 'GER', 'bomb', true);
    r.won = false;
    expect(page(g, `airreport:${r.id}`).headline).toBe(`Bombing of ${r.place}: raid cost too much`);
    r.won = true;
    expect(page(g, `airreport:${r.id}`).headline).toBe(`Bombing of ${r.place}: raid succeeded`);
    const f = grand('FRA');
    const on = flown(f, 'GER', 'bomb', true, 'FRA');
    on.won = false;
    expect(page(f, `airreport:${on.id}`).headline).toBe(`Bombing of ${on.place}: raid made to pay`);
    on.won = true;
    expect(page(f, `airreport:${on.id}`).headline).toBe(`Bombing of ${on.place}: raid got through`);
  });

  it('the report says its month; a strike on ships names the fleet, lists the ships and leads to the sea', () => {
    const g = grand('GER');
    const r = flown(g, 'GER', 'bomb', true);
    expect(page(g, `airreport:${r.id}`).note.startsWith(`${turnToDate(r.turn).label}. `)).toBe(true);
    r.turn = 14;
    expect(page(g, `airreport:${r.id}`).note.startsWith(`${turnToDate(14).label}. `)).toBe(true);
    // A strike on a fleet at sea.
    const ships = { bb: 2, cv: 0, ca: 3, dd: 8, ss: 0 }, none = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 };
    Object.assign(r, { mission: 'fleet', region: -1, zone: 3, place: 'the North Sea', ships: [{ nat: 'UK', fleet: 'Home Fleet', admiral: 'Charles Forbes', ships, sunk: { ...none, dd: 1 } }, { nat: 'FRA', fleet: 'Brest Squadron', admiral: null, ships: { ...none, ca: 1 }, sunk: none }] });
    let p = page(g, `airreport:${r.id}`);
    expect(p.title).toBe('THE RAID REPORT  ·  NORTH SEA');
    expect(p.headline.startsWith('Air strike on the Home Fleet: ')).toBe(true);
    const sea = section(p, 'THE SHIPS UNDER ATTACK')!;
    expect(sea.rows.map((x) => [x.nat, x.title])).toEqual([['UK', 'Home Fleet'], ['FRA', 'Brest Squadron']]);
    expect(sea.rows[0].lines[0]).toMatch(/^2 battleships, 3 \w+ and 8 destroyers\. Charles Forbes commands\. Lost 1 destroyer\.$/);
    expect(sea.rows[1].lines[0]).toMatch(/^1 \w+\. No ships lost\.$/);
    expect(p.top.find((k) => k.act.startsWith('goto:'))).toMatchObject({ label: 'GO TO THE NORTH SEA', act: 'goto:sea:3', ok: true });
    // A raid on a harbour, from the land: the harbour is named, and the way leads to the province.
    Object.assign(r, { mission: 'harbour', region: 5, zone: -1, place: 'Scapa Flow' });
    p = page(g, `airreport:${r.id}`);
    expect(p.headline.startsWith('Raid on the harbour of Scapa Flow: ')).toBe(true);
    expect(p.top.find((k) => k.act.startsWith('goto:'))).toMatchObject({ label: 'GO TO SCAPA FLOW', act: 'goto:region:5' });
    expect(p.note).not.toContain('Caught unready');
    // Caught unready, in the month war was declared.
    r.surprise = true;
    expect(page(g, `airreport:${r.id}`).note).toContain('Caught unready: war was declared this month, so the harbour had no warning, its fighters and guns were not manned, and the ships lay at their moorings.');
    // A town's defences bombed: the town is named, not the province.
    Object.assign(r, { mission: 'town', surprise: false });
    delete r.ships;
    p = page(g, `airreport:${r.id}`);
    expect(p.headline.startsWith('Bombing of the defences of ')).toBe(true);
    expect(section(p, 'THE SHIPS UNDER ATTACK')).toBeUndefined();
  });

  it('every other kind has its own title', () => {
    const g = grand('GER');
    const r = flown(g, 'GER', 'bomb', true);
    const title = (mission: AirReport['mission']): string => { r.mission = mission; return page(g, `airreport:${r.id}`).headline.split(':')[0]; };
    expect(title('night')).toBe(`Night raid on ${r.place}`);
    expect(title('ground')).toBe(`Raid on the troops at ${r.place}`);
    expect(title('line')).toBe(`Bombing of the line at ${r.place}`);
    expect(title('harbour')).toBe(`Raid on the harbour of ${r.place}`);
  });

  it('a raid that fell on his side reads from his side: it got through or was beaten off, and the raiders are named', () => {
    const g = grand('FRA');
    const r = flown(g, 'GER', 'bomb', true, 'FRA');
    const p = page(g, `airreport:${r.id}`);
    expect(p.headline).toBe(`Bombing of ${r.place}: ${r.won ? 'raid got through' : 'raid made to pay'}`);
    expect(p.sections[0].heading).toBe('THE RAIDERS');
    expect(p.sections[0].rows.every((x) => x.nat === 'GER')).toBe(true);
    expect(section(p, 'ROSE TO MEET THEM')!.rows.length).toBe(r.def.length);
    // Both ways it can end.
    r.won = !r.won;
    expect(page(g, `airreport:${r.id}`).headline).toBe(`Bombing of ${r.place}: ${r.won ? 'raid got through' : 'raid made to pay'}`);
    // It is not his month yet (Germany moves first), and the page says so where it offers the next raid.
    expect(humanUp(g)).toBe(false);
    expect(p.top[1]).toEqual({ label: 'PLAN A RAID', act: 'open:raids', ok: false, tip: 'It is not your month.' });
  });

  it('from his side a raid nobody met was not intercepted, and a fight for the sky is his victory when it is the raider\'s defeat', () => {
    const g = grand('FRA');
    const quiet = flown(g, 'GER', 'bomb', false, 'FRA');
    expect(page(g, `airreport:${quiet.id}`).headline).toBe(`Bombing of ${quiet.place}: not intercepted`);
    const sky = flown(g, 'GER', 'superiority', true, 'FRA');
    // (Where the fight and the sky disagree the verdict says "but"; where they agree, a comma.)
    const head = (): string => page(g, `airreport:${sky.id}`).headline;
    expect(head().startsWith(`The sky over ${sky.place}: ${FLIP[sky.level].toLowerCase()}, `)).toBe(true);
    sky.level = 'Decisive defeat'; sky.sky = false;
    expect(head()).toBe(`The sky over ${sky.place}: decisive victory, sky held`);
    sky.level = 'Heroic victory';
    expect(head()).toBe(`The sky over ${sky.place}: crushing defeat, but the sky held`);
    sky.sky = true;
    expect(head()).toBe(`The sky over ${sky.place}: crushing defeat, sky lost`);
  });

  it('an ally\'s raid is his side\'s to read; a raid between others is not, and neither is a report that does not exist', () => {
    const uk = grand('UK');
    expect(allied(uk, 'UK', 'FRA')).toBe(true);
    const onFrance = flown(uk, 'GER', 'bomb', true, 'FRA');
    expect(page(uk, `airreport:${onFrance.id}`).sections[0].heading).toBe('THE RAIDERS');
    expect(raidsToRead(uk).map((x) => [x.r.id, x.side])).toEqual([[onFrance.id, 'def']]);

    const g = grand('SOV');
    for (const n of ['GER', 'FRA']) expect(allied(g, 'SOV', n)).toBe(false);
    const r = flown(g, 'GER', 'bomb', true, 'FRA');
    expect(airPage(g, `airreport:${r.id}`)).toBeNull();
    expect(raidsToRead(g)).toEqual([]);
    expect(airPage(g, 'airreport:99999')).toBeNull();
    expect(airPage(g, 'airreport:x')).toBeNull();
    expect(airPage(g, 'airreport')).toBeNull();
    // The door to the month's raids is dim, and says why; the list itself says none has been flown.
    const door = page(g, 'raids').top.find((b) => b.act === 'open:airreports')!;
    expect(door).toEqual({ label: "THE MONTH'S RAIDS", act: 'open:airreports', ok: false, tip: 'No raid has been flown by your side or against it since you last ended a month.' });
    const list = page(g, 'airreports');
    expect(list.headline).toBe('No raid to read');
    expect(list.sections.map((s) => [s.heading, s.text, s.rows.length])).toEqual([['FLOWN BY YOUR SIDE', 'None yet.', 0], ['FLOWN AGAINST YOUR SIDE', 'None yet.', 0]]);
  });
});

describe('the month\'s raids, each with its report', () => {
  it('lists the raids his side flew and the raids flown against it, newest first, each with the way to its report', () => {
    const g = grand('GER');
    const a = flown(g, 'GER', 'bomb', true);
    const b = flown(g, 'GER', 'superiority', true);
    const c = flown(g, 'GER', 'bomb', false);
    expect(raidsToRead(g).map((x) => [x.r.id, x.side])).toEqual([[c.id, 'att'], [b.id, 'att'], [a.id, 'att']]);
    const list = page(g, 'airreports');
    expect(list.title).toBe("THE MONTH'S RAIDS");
    expect(list.headline).toBe('3 flown by your side, none against it');
    expect(list.top).toEqual([{ label: 'PLAN A RAID', act: 'open:raids', ok: true, tip: '' }]);
    const rows = section(list, 'FLOWN BY YOUR SIDE')!.rows;
    expect(rows.map((x) => x.title)).toEqual([`Bombing of ${c.place}`, `The sky over ${b.place}`, `Bombing of ${a.place}`]);
    expect(rows[0].lines[0].startsWith('Unopposed. The raid lost ')).toBe(true);
    expect(rows[2].lines[0].startsWith(`${a.won ? 'Raid succeeded' : 'Raid cost too much'}. The raid lost `)).toBe(true);
    expect(rows.map((x) => x.buttons.map((k) => [k.label, k.act, k.ok]))).toEqual([c, b, a].map((r) => [['READ THE REPORT', `open:airreport:${r.id}`, true]]));
    for (const row of rows) expect(page(g, row.buttons[0].act.replace(/^open:/, '')).id).toBe(row.buttons[0].act.replace(/^open:/, ''));
    expect(section(list, 'FLOWN AGAINST YOUR SIDE')).toEqual({ heading: 'FLOWN AGAINST YOUR SIDE', text: 'None yet.', rows: [] });
    // The door stands on the airfield and on the page a raid is planned from, lit now there is something to read.
    const base = g.airFleets.find((f) => f.id === a.att[0].id)!.base;
    for (const top of [airPage(g, 'airfield', base)!.top, page(g, 'raids').top]) expect(top.find((k) => k.act === 'open:airreports')).toEqual({ label: "THE MONTH'S RAIDS", act: 'open:airreports', ok: true, tip: 'Every raid flown by your side or against it since you last ended a month, each with its report.' });
  });

  it('holds what has been flown since he last ended a month: this month\'s, and last month\'s by the nations that move after him', () => {
    const g = grand('GER');
    g.turn = 2;
    const at = g.order.indexOf('GER');
    expect(g.order.indexOf('FRA')).toBeGreaterThan(at);
    const real = flown(g, 'GER', 'bomb', true, 'FRA');
    expect(real.turn).toBe(2);
    const as = (id: number, turn: number, back: boolean): AirReport => ({ ...real, id, turn, ...(back ? { attNat: 'FRA', defNat: 'GER', att: real.def, def: real.att } : {}) });
    g.airReports.push(
      as(9001, 1, true),   // France raided him last month, after his month was closed: his to read now
      as(9002, 1, false),  // his own raid of last month: read then
      as(9003, 0, true),   // two months old
      as(9004, 2, true),   // this month
    );
    expect(raidsToRead(g).map((x) => [x.r.id, x.side])).toEqual([[9004, 'def'], [9001, 'def'], [real.id, 'att']]);
    expect(page(g, 'airreports').headline).toBe('1 flown by your side, 2 against it');
    // A nation that moves before him flew last month's raid before he closed his own month.
    const f = grand('FRA');
    f.turn = 2;
    const on = flown(f, 'GER', 'bomb', true, 'FRA');
    expect(f.order.indexOf('GER')).toBeLessThan(f.order.indexOf('FRA'));
    f.airReports.push({ ...on, id: 9101, turn: 1 }, { ...on, id: 9102, turn: 1, attNat: 'UK', defNat: 'GER', att: on.def, def: on.att });
    expect(f.order.indexOf('UK')).toBeGreaterThan(f.order.indexOf('FRA'));
    // (Britain, his ally, moves after him: her raid of last month is his side's, and new to him.)
    expect(raidsToRead(f).map((x) => [x.r.id, x.side])).toEqual([[9102, 'att'], [on.id, 'def']]);
  });

  it('once he has ended a month, reads from the mark made then: the raids his own side flew as his month closed are his to read', () => {
    const g = grand('GER');
    g.turn = 2;
    const real = flown(g, 'GER', 'bomb', true, 'FRA');
    const as = (id: number, turn: number, back: boolean): AirReport => ({ ...real, id, turn, ...(back ? { attNat: 'FRA', defNat: 'GER', att: real.def, def: real.att } : {}) });
    g.airReports.length = 0;
    g.airReports.push(
      as(11, 1, false),  // ordered by hand last month, before he ended it: read then
      as(12, 1, false),  // his own air campaign's raid, flown as his month closed (filed in last month, by his own nation)
      as(13, 1, true),   // France's, after his month
      as(14, 2, false),  // ordered by hand this month
    );
    // No mark yet: by the month, which cannot tell raid 12 from raid 11.
    expect(raidsToRead(g).map((x) => x.r.id)).toEqual([14, 13]);
    // The mark the 3D game makes as he ends his month: the last report filed before it closed.
    g.flags.raidsFrom = 11;
    expect(raidsToRead(g).map((x) => [x.r.id, x.side])).toEqual([[14, 'att'], [13, 'def'], [12, 'att']]);
    expect(page(g, 'airreports').headline).toBe('2 flown by your side, 1 against it');
    // Only raids against him: never a bare 0.
    g.flags.raidsFrom = 12;
    g.airReports.pop();
    expect(raidsToRead(g).map((x) => [x.r.id, x.side])).toEqual([[13, 'def']]);
    expect(page(g, 'airreports').headline).toBe('None flown by your side, 1 against it');
    g.airReports.push(as(14, 2, false));
    g.flags.raidsFrom = 11;
    // It is kept in a saved game.
    expect(raidsToRead(loadGame(saveGame(g))!).map((x) => x.r.id)).toEqual([14, 13, 12]);
    // Nothing filed since: nothing to read, whatever the month.
    g.flags.raidsFrom = 14;
    expect(raidsToRead(g)).toEqual([]);
    expect(page(g, 'airreports').headline).toBe('No raid to read');
  });
});

describe('flying a raid leads to its report', () => {
  /** Germany at the table in its first month, and a bombing raid it can fly that fighters will meet. */
  function table(): { g: Game; target: number; ids: number[] } {
    const g = grand('GER', 3);
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    return { g, ...raid(g, 'GER', 'bomb', true) };
  }

  it('the rules still send him back to the airfield: the report is named beside it', () => {
    const { g, target, ids } = table();
    const res = airAct(g, `raid:bomb:${target}:${ids.join('.')}`)!;
    expect(res.ok).toBe(true);
    expect(res.open).toBe('airfield');
    expect(res.report).toBe(g.airReports[g.airReports.length - 1].id);
    expect(page(g, `airreport:${res.report}`).title.startsWith('THE RAID REPORT')).toBe(true);
  });

  it('the 3D game is told the report is the page that comes next, and its rows carry no group\'s picture', () => {
    const { g, target, ids } = table();
    TC().load(saveGame(g));
    // (A raid being planned is set out as a card in the 3D game, and the button that flies it is the card's.)
    const fly = (TC().page(`plan:bomb:${target}:${ids.join('.')}`) as { card: { fly: { label: string; act: string; ok: boolean } } }).card.fly;
    expect(fly).toMatchObject({ label: 'FLY THE RAID BY DAY', act: `raid:bomb:${target}:${ids.join('.')}`, ok: true });
    const out = TC().act(fly.act) as { ok: boolean; note: string; open: string; flies: unknown[] };
    expect(out.ok).toBe(true);
    expect(out.open).toMatch(/^airreport:\d+$/);
    expect(out.flies.length).toBeGreaterThan(0);
    expect(out.note.length).toBeGreaterThan(10);
    const rep = TC().page(out.open) as Seen;
    expect(rep.id).toBe(out.open);
    expect(rep.title.startsWith('THE RAID REPORT')).toBe(true);
    expect(rep.sections.map((s) => s.heading)).toEqual(['FLEW THE RAID', 'ROSE TO MEET THEM', 'THE BATTLE TICKET']);
    for (const row of rep.sections.flatMap((s) => s.rows) as (Seen['sections'][number]['rows'][number] & { picture?: unknown })[]) expect(row.picture).toBeUndefined();
    // The list is one press from it, and holds the raid.
    const list = TC().page('airreports') as Seen;
    expect(list.sections[0].rows[0].buttons[0].act).toBe(`open:${out.open}`);
    // A second press of the same order flies nothing and names no report.
    const again = TC().act(fly.act) as { ok: boolean; open: string };
    expect(again.ok).toBe(false);
    expect(again.open).toBe('');
  });

  it('a raid he fought out himself keeps the page it had: its after card is the air battle\'s', () => {
    const { g, target, ids } = table();
    TC().load(saveGame(g));
    // (What the air battle hands back: nothing lost, nothing dropped. The booking is the rules' own.)
    const out = TC().act(`raid:bomb:${target}:${ids.join('.')}`, -1, { lost: {}, dropped: 0 }) as { ok: boolean; open: string; flies: unknown[] };
    expect(out.ok).toBe(true);
    expect(out.flies.length).toBeGreaterThan(0);
    expect(out.open).toBe('airfield');
  });

  it('ending the month marks where next month\'s reading starts: the last report filed before it closed', () => {
    const { g, target, ids } = table();
    TC().load(saveGame(g));
    const before = loadGame((TC().save() as { text: string }).text)!;
    expect(before.flags.raidsFrom).toBeUndefined();
    const out = TC().act(`raid:bomb:${target}:${ids.join('.')}`) as { ok: boolean; open: string };
    expect(out.ok).toBe(true);
    const id = Number(out.open.split(':')[1]);
    TC().endMonth();
    const after = loadGame((TC().save() as { text: string }).text)!;
    expect(after.flags.raidsFrom).toBe(id);
    // The raid he ordered and read last month is not in next month's list; whatever was flown since is.
    const list = raidsToRead(after).map((x) => x.r.id);
    expect(list).not.toContain(id);
    for (const x of list) expect(x).toBeGreaterThan(id);
  });
});
