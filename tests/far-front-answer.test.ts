// Held armies answer an attack by land, with the far side's switch on (the design director's ruling B11 of 25
// November 2026, design-director/pacific-japan-1125.md; the Pacific audit's finding on a player's Japan).
//
// "One rule for every nation: when an enemy army enters a front's ground by land, every held row on that front is
// freed for that front's defence (not beyond it), as b641 does from the sea. A war declared on a nation frees its
// holds on the front that faces that enemy." A player's Japan that struck north from Manchukuo met the Soviet Far
// East's rows on hold: where the staff opened no offensive of its own they stood and watched. The Dutch divisions
// Britain keeps on Java stood the same way while an army that had landed marched past them.
//
// The held rows on a far-side front answer for the month's fighting, tied to that front's ground, and hold again
// where they then stand. A landing alone is no entry by land (home ground reached from the sea is b641 (f)'s, and
// its tests are far-home-answer's). Never a fortress, engineers, a row marked to stay or a row somebody still waits
// to send somewhere; never for the player, whose corps no hold binds; nothing on the old chart.
//
// The skeptic's findings of 25 November 2026, mended here: the tie is to its side's own ground on the front and
// never to what its side took in the enemy's country; an answering row is not lifted off its front by sea; and a
// shore is a beach only where the landing itself won it. (The months-long reading is far-front-answer-months'.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { answering, frontAnswering, homeBars, wantsOut, withAnswer } from '../src/campaign/ai';
import { clientMonth } from '../src/campaign/far';
import { addCorps, atWar, capitulate, createGame, declareWar, moveCorps, relocate, setWar, whyNotEvacuate } from '../src/campaign/game';
import { landCorps } from '../src/campaign/navy';
import { offensiveWatch, startOpen } from '../src/campaign/staff';
import { beginHuman, canOrder, current, endHuman, orderAttack, orderMove, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { farOn, shoreOf } from '../src/campaign/world';

const wide = (human: string | null = null): Game => startOpen(createGame(1, human));
const names = (cs: Corps[]): string[] => cs.map((c) => c.name).sort();
const called = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
/** The Soviet Union's corps beyond the old chart. */
const farEast = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'SOV' && !c.dead && shoreOf(c.reg) === 2);
/** The ground each answering row may stand on this month, read as the staff's month reads it. */
const grounds = (g: Game, nat: string, rows: Corps[]): Map<Corps, Set<number>> =>
  withAnswer(g, nat, () => new Map(rows.map((c) => [c, new Set(g.regions.filter((r) => !homeBars(c, r)).map((r) => r.id))])));
/** Plays the months of the computer's nations up to the one named, and stops before it. */
const upTo = (g: Game, nat: string): void => { for (let k = 0; current(g) !== nat && k < 200; k++) stepAI(g); expect(current(g)).toBe(nat); };

/** The rows of the Soviet Far East that stand on a front with Japan or Manchukuo, or next behind one. */
const FRONTIER = [
  '1st Army Group (Zhukov; the 57th Special Corps)', '1st Army Group tanks',
  '1st Red Banner Army (Posyet and the Khasan border)', '1st Red Banner Army (Primorye)', '1st Red Banner Army (the Ussuri)',
  '2nd Red Banner Army (Birobidzhan)', '2nd Red Banner Army (Khabarovsk)', '2nd Red Banner Army (Skovorodino)', '2nd Red Banner Army (Svobodny)', '2nd Red Banner Army (the Amur)',
  'North Sakhalin garrison',
  'Transbaikal Military District (Chita)', 'Transbaikal Military District (Nerchinsk)', 'Transbaikal Military District (facing Manchouli)', 'Transbaikal tank brigades',
];

describe('held armies answer an attack by land (the design director\'s B11)', () => {
  it('a front no war has opened frees nobody: the Soviet Far East stands held through a month of peace with Japan', () => {
    const g = wide();
    expect(farOn(g)).toBe(true);
    const rows = farEast(g);
    expect(rows).toHaveLength(18);
    expect(rows.every((c) => c.hold && !c.stay)).toBe(true);
    // Manchukuo is across the river and Japan across the strait, and the Union is at war with neither.
    expect([atWar(g, 'SOV', 'JAP'), atWar(g, 'SOV', 'MAN')]).toEqual([false, false]);
    expect(frontAnswering(g, 'SOV')).toEqual([]);
    const stood = new Map(rows.map((c) => [c, c.reg]));
    upTo(g, 'SOV');
    stepAI(g);
    for (const c of rows) expect([c.name, c.hold, c.reg]).toEqual([c.name, true, stood.get(c)]);
  }, 300000);

  it('a war declared frees the holds on the front that faces that enemy, and on no other', () => {
    const g = wide('JAP');
    upTo(g, 'JAP');
    beginHuman(g);
    declareWar(g, 'JAP', 'SOV');
    // The month Japan declares, Manchukuo is not yet in (a client follows its patron as the world's month turns):
    // the fronts that face Japan's own ground are Korea's with the Maritime Province and Karafuto's with North
    // Sakhalin. The armies along the Amur and in Transbaikal face a neighbour the Union is not at war with.
    expect([atWar(g, 'SOV', 'JAP'), atWar(g, 'SOV', 'MAN')]).toEqual([true, false]);
    expect(names(frontAnswering(g, 'SOV'))).toEqual(['1st Red Banner Army (Posyet and the Khasan border)', '1st Red Banner Army (Primorye)', 'North Sakhalin garrison']);
    clientMonth(g, true);
    expect(atWar(g, 'SOV', 'MAN')).toBe(true);
    // With Manchukuo in, every row on the frontier answers, before a Japanese soldier has crossed. Not the fortress
    // of Vladivostok, not Kamchatka's garrison (no front reaches it), not the reserve at Ulan-Ude, behind the front.
    expect(names(frontAnswering(g, 'SOV'))).toEqual([...FRONTIER].sort());
    // The sea's answer is its own rule and reads nothing here: nobody has landed on Soviet home ground.
    expect(answering(g, 'SOV')).toEqual([]);
    // Nothing answers for the player, whose corps no hold binds: a held corps of his takes his order.
    expect(frontAnswering(g, 'JAP')).toEqual([]);
    const amur = called(g, '4th Army (the Amur)');
    expect([amur.hold, canOrder(g, amur)]).toEqual([true, true]);
    expect(orderMove(g, amur, g.byName['Zavitaya'].id)).toBeGreaterThan(0);
    expect([g.regions[amur.reg].name, amur.hold]).toEqual(['Zavitaya', true]);
  }, 300000);

  it('an answering row is off hold for the month and tied to its front\'s ground; the hold is back when the month is over', () => {
    const g = wide();
    setWar(g, 'JAP', 'SOV');
    clientMonth(g, true);
    expect(names(frontAnswering(g, 'SOV'))).toEqual([...FRONTIER].sort());
    const dauria = called(g, 'Transbaikal Military District (facing Manchouli)');
    const at = (nm: string): Region => g.byName[nm];
    // Japan has crossed the Amur into Zavitaya: ground the Union held until he came is its front's to win back.
    // (What Japan holds is a front of its own to the staff, beside Manchukuo's: the rows that stand round it answer
    // on both.)
    const zavitaya = at('Zavitaya'), svobodny = called(g, '2nd Red Banner Army (Svobodny)');
    zavitaya.prev = zavitaya.ctrl; zavitaya.ctrl = 'JAP'; zavitaya.since = g.turn;
    expect([dauria.hold, homeBars(dauria, at('Hailar'))]).toEqual([true, false]);
    const seen = withAnswer(g, 'SOV', () => ({
      hold: dauria.hold,
      // Its own post, the front along the Amur and the Ussuri, and the ground next behind it.
      own: ['Dauria', 'Chita', 'Blagoveshchensk', 'Vladivostok'].map((nm) => homeBars(dauria, at(nm))),
      // Manchukuo and Korea across the line; the rear beyond the front's ground; the old chart.
      off: ['Hailar', 'Harbin', 'Seoul', 'Ulan-Ude', 'Irkutsk', 'Moscow'].map((nm) => homeBars(dauria, at(nm))),
      // The army beside the lost province may go in and take it back, and no farther: not over the river to Heiho.
      beside: ['Svobodny', 'Zavitaya', 'Heiho'].map((nm) => homeBars(svobodny, at(nm))),
      // Sakhalin's garrison answers on its own front, the island and the shore behind it, and not on the Amur's.
      island: ['North Sakhalin', 'Nikolayevsk', 'Karafuto', 'Khabarovsk'].map((nm) => homeBars(called(g, 'North Sakhalin garrison'), at(nm))),
      // A row that does not answer is not tied.
      reserve: homeBars(called(g, 'Transbaikal reserve'), at('Hailar')),
    }));
    expect(seen).toEqual({ hold: false, own: [false, false, false, false], off: [true, true, true, true, true, true], beside: [false, false, true], island: [false, false, true, true], reserve: false });
    expect([dauria.hold, homeBars(dauria, at('Hailar'))]).toEqual([true, false]);
  });

  it('a held row far behind the line is on no front: the Korea Army answers nothing for the war in China, nor Kamchatka for Manchukuo\'s front', () => {
    // The edge left open for the design director. "Every held row on that front" is read as the staff's sums read a
    // held row on a front: on its provinces or next behind them. Read by the staff's membership (the nearest front,
    // however far), the Korea Army's two held divisions were in China by the third month of a war nobody played.
    const g = wide();
    expect([atWar(g, 'JAP', 'CHN'), atWar(g, 'JAP', 'SOV')]).toEqual([true, false]);
    const japan = names(frontAnswering(g, 'JAP'));
    for (const nm of ['Korea Army (19th Division)', 'Korea Army (20th Division)', 'Kwantung Army reserve', '4th Army (the Amur)']) {
      expect([nm, called(g, nm).hold, japan.includes(nm)]).toEqual([nm, true, false]);
    }
    // At war with the Union, the Korea Army stands on the front that faces it and answers there; the reserve at
    // Ulan-Ude and Kamchatka's garrison, behind the Union's own line, stay held until the front comes to them.
    setWar(g, 'JAP', 'SOV');
    clientMonth(g, true);
    expect(names(frontAnswering(g, 'JAP'))).toEqual(expect.arrayContaining(['Korea Army (19th Division)', '4th Army (the Amur)']));
    const union = names(frontAnswering(g, 'SOV'));
    for (const nm of ['Transbaikal reserve', 'Kamchatka garrison']) expect([nm, called(g, nm).hold, union.includes(nm)]).toEqual([nm, true, false]);
    // The front comes to the reserve: with Chita lost, Ulan-Ude is next behind the line and its row answers.
    const chita = g.byName['Chita'], reserve = called(g, 'Transbaikal reserve');
    const near = g.regions[reserve.reg].nb.includes(chita.id) || g.regions[reserve.reg].nb.some((j) => g.regions[j].nb.includes(chita.id) && g.regions[j].owner === 'SOV');
    expect(near).toBe(true);
    for (const u of [...chita.units]) relocate(g, u, g.byName['Irkutsk']);
    chita.prev = chita.ctrl; chita.ctrl = 'JAP'; chita.since = g.turn;
    addCorps(g, 'JAP', 'Test army at Chita', 'inf', 3, 'Chita', {});
    expect(names(frontAnswering(g, 'SOV'))).toContain(reserve.name);
  });

  it('the tie is to its side\'s own ground: never ground its side took in the enemy\'s country, held or lost again', () => {
    const g = wide();
    setWar(g, 'JAP', 'SOV');
    clientMonth(g, true);
    const at = (nm: string): Region => g.byName[nm];
    const amur = called(g, '2nd Red Banner Army (the Amur)');
    const heiho = at('Heiho'), blago = at('Blagoveshchensk');
    expect([g.regions[amur.reg].name, heiho.owner, blago.nb.includes(heiho.id)]).toEqual(['Blagoveshchensk', 'MAN', true]);
    const bars = (c: Corps, list: string[]): boolean[] => { const on = grounds(g, 'SOV', [c]).get(c)!; return list.map((nm) => !on.has(at(nm).id)); };
    // The first day of the war: its own bank, and nothing of Manchukuo's.
    expect(bars(amur, ['Blagoveshchensk', 'Svobodny', 'Heiho'])).toEqual([false, false, true]);

    // The Red Army's own offensive has taken Heiho over the river (the staff's release sends free corps, not these).
    // It is a province of the front now, and it is Manchukuo's country: no held row is sent onto it.
    for (const u of [...heiho.units]) relocate(g, u, at('Harbin'));
    const taken = (r: Region, by: string): void => { r.prev = r.ctrl; r.ctrl = by; r.since = g.turn; };
    taken(heiho, 'SOV');
    expect(names(frontAnswering(g, 'SOV'))).toContain(amur.name);
    expect(bars(amur, ['Blagoveshchensk', 'Svobodny', 'Heiho'])).toEqual([false, false, true]);

    // Manchukuo takes it back. The Union held it last, and it is still no ground of the Union's side: nobody goes
    // to win it back. (Read by who held it last, the held rows followed the staff's offensive province by province:
    // in one war Chita's army, a held row all the while, stood at Tatung in China in its fifth month.)
    taken(heiho, 'MAN');
    expect([heiho.ctrl, heiho.prev]).toEqual(['MAN', 'SOV']);
    expect(bars(amur, ['Blagoveshchensk', 'Svobodny', 'Heiho'])).toEqual([false, false, true]);
    // The Union's own province, lost and to be won back, is its ground as before.
    taken(at('Zavitaya'), 'JAP');
    expect(bars(called(g, '2nd Red Banner Army (Svobodny)'), ['Svobodny', 'Zavitaya', 'Heiho'])).toEqual([false, false, true]);

    // A held row that already stands on ground taken answers where it stands, and goes no deeper.
    taken(heiho, 'SOV');
    relocate(g, amur, heiho);
    const beyond = heiho.nb.map((j) => g.regions[j]).filter((r) => r.owner === 'MAN');
    expect(beyond.length).toBeGreaterThan(0);
    for (const r of beyond) { for (const u of [...r.units]) relocate(g, u, at('Harbin')); taken(r, 'SOV'); }
    expect(names(frontAnswering(g, 'SOV'))).toContain(amur.name);
    const on = grounds(g, 'SOV', [amur]).get(amur)!;
    expect([on.has(heiho.id), on.has(blago.id), beyond.map((r) => on.has(r.id))]).toEqual([true, true, beyond.map(() => false)]);
  });

  it('never a fortress, engineers, a row marked to stay or one the ships have waiting; none for the player; nothing on the old chart', () => {
    const g = wide();
    setWar(g, 'JAP', 'SOV');
    clientMonth(g, true);
    const plain = addCorps(g, 'SOV', 'Test division at Dauria', 'inf', 1, 'Dauria', { hold: 1 });
    const waits = addCorps(g, 'SOV', 'Test division held for a shore', 'inf', 1, 'Dauria', { hold: 1 });
    g.flags[`st:hopUp:${waits.id}`] = g.turn;
    addCorps(g, 'SOV', 'Test fortified region', 'fort', 1, 'Dauria', { hold: 1 });
    addCorps(g, 'SOV', 'Test engineers', 'eng', 1, 'Dauria', { hold: 1 });
    addCorps(g, 'SOV', 'Test garrison marked to stay', 'inf', 1, 'Dauria', { hold: 1, stay: 1 });
    addCorps(g, 'SOV', 'Test field division', 'inf', 1, 'Dauria', {});
    // (A garrison the record keeps inside its town, and a corps that marches with another's army at a player's
    // asking and takes that army's orders: the two the rule also passes over.)
    addCorps(g, 'SOV', 'Test garrison kept inside its town', 'inf', 1, 'Dauria', { hold: 1 }).fortress = true;
    addCorps(g, 'SOV', 'Test division lent to an ally', 'inf', 1, 'Dauria', { hold: 1 }).joint = 'MGL';
    expect(names(frontAnswering(g, 'SOV'))).toEqual([...FRONTIER, plain.name].sort());
    // One rule for every nation: Japan's own held rows on that front answer under a computer's staff, and they are
    // none of them a row marked to stay.
    const japan = frontAnswering(g, 'JAP');
    expect(japan.length).toBeGreaterThan(0);
    expect(japan.every((c) => c.hold && !c.stay && c.kind !== 'fort' && shoreOf(c.reg) === 2)).toBe(true);
    // The old chart's fronts are as they were: a held division on France's frontier with Germany, at war, answers nothing.
    setWar(g, 'GER', 'FRA');
    const frontier = g.regions.find((r) => r.owner === 'FRA' && shoreOf(r) === 0 && r.nb.some((j) => g.regions[j].owner === 'GER'))!;
    addCorps(g, 'FRA', 'Test fortress division', 'inf', 1, frontier.id, { hold: 1 });
    expect(frontAnswering(g, 'FRA').filter((c) => shoreOf(c.reg) !== 2)).toEqual([]);
    // The player's Union gets none of it.
    const mine = wide('SOV');
    setWar(mine, 'JAP', 'SOV');
    clientMonth(mine, true);
    expect(frontAnswering(mine, 'SOV')).toEqual([]);
  });

  it('a player\'s Japan strikes north from Manchukuo: the held rows counter-attack on their front, stay on it, and hold again', () => {
    const g = wide('JAP');
    upTo(g, 'JAP');
    beginHuman(g);
    // A player who massed before he struck: two armies on each province of Manchukuo's frontier, so that the
    // Soviet staff opens no offensive of its own, and a detachment beside them at Heiho.
    const border = g.regions.filter((r) => r.owner === 'MAN' && r.nb.some((j) => g.regions[j].owner === 'SOV'));
    let k = 0;
    for (const r of border) for (let i = 0; i < 2; i++) addCorps(g, 'JAP', `Test army ${++k}`, 'inf', 3, r.id, {});
    const detachment = addCorps(g, 'JAP', 'Test detachment', 'inf', 1, 'Heiho', {});
    declareWar(g, 'JAP', 'SOV');
    clientMonth(g, true);
    // His orders, overland from Heiho: the armies there storm Blagoveshchensk across the Amur and the detachment
    // walks into Zavitaya beside it.
    const heiho = g.byName['Heiho'], blago = g.byName['Blagoveshchensk'], zavitaya = g.byName['Zavitaya'];
    const storm = [...heiho.units.filter((c) => c.nat === 'JAP' && c.name.startsWith('Test army')), called(g, '4th Army (the Amur)')];
    expect(orderAttack(g, storm, blago)?.taken).toBe(true);
    expect(orderMove(g, detachment, zavitaya.id)).toBeGreaterThan(0);
    expect([blago.ctrl, zavitaya.ctrl, g.regions[detachment.reg].name]).toEqual(['JAP', 'JAP', 'Zavitaya']);
    endHuman(g);

    upTo(g, 'SOV');
    const rows = frontAnswering(g, 'SOV');
    expect(names(rows)).toEqual([...FRONTIER].sort());
    expect(rows.every((c) => c.hold)).toBe(true);
    const ground = grounds(g, 'SOV', rows);
    const posts = new Map(rows.map((c) => [c, c.reg]));
    // The staff's own word on every far-side front, to tell an answer from an offensive of its own (whose release
    // takes a row's hold off for good).
    const opened: string[] = [];
    offensiveWatch.tell = (e) => { if (e.nat === 'SOV' && e.how !== 'quiet') opened.push(`${e.how}:${e.front.edge.map((id) => g.regions[id].name).join(',')}`); };
    try { stepAI(g); } finally { offensiveWatch.tell = null; }
    expect(opened).toEqual([]);

    // The counter-attack: the armies beside Zavitaya took it back and threw the detachment over the river.
    expect(zavitaya.ctrl).toBe('SOV');
    expect(g.regions[detachment.reg].owner).toBe('MAN');
    const fought = rows.filter((c) => !c.dead && c.fought);
    expect(fought.length).toBeGreaterThanOrEqual(2);
    expect(zavitaya.units.filter((c) => c.nat === 'SOV').every((c) => rows.includes(c))).toBe(true);
    expect(zavitaya.units.some((c) => c.nat === 'SOV')).toBe(true);
    // Blagoveshchensk, under three armies, was not to be had at the odds: it is still Japan's.
    expect(blago.ctrl).toBe('JAP');
    for (const c of rows) {
      if (c.dead) continue;
      // The hold is back on each where it then stands.
      expect(c.hold, c.name).toBe(true);
      // None left its front's ground: not into Manchukuo, not into Korea, not off to another front.
      expect(ground.get(c)!.has(c.reg), `${c.name} at ${g.regions[c.reg].name}`).toBe(true);
      expect(['SOV', 'MGL'], `${c.name} at ${g.regions[c.reg].name}`).toContain(g.regions[c.reg].owner);
    }
    // Sakhalin's garrison did not leave its island for a need it could not reach.
    expect(g.regions[called(g, 'North Sakhalin garrison').reg].name).toBe('North Sakhalin');
    // The rows that do not answer stood where they were, held.
    for (const nm of ['Kamchatka garrison', 'Transbaikal reserve', 'Vladivostok fortified region']) expect([nm, called(g, nm).hold]).toEqual([nm, true]);
    // Some row moved or fought: the front was defended, not watched.
    expect(rows.some((c) => !c.dead && (c.fought || c.reg !== posts.get(c)))).toBe(true);
  }, 600000);

  /**
   * Java in Britain's keeping, Japan at war with Britain, and Britain's month about to open: the three Dutch
   * divisions at their establishment. `first` runs with Central Java's division still at Semarang, before it is
   * drawn east to Soerabaja so that Semarang stands open. Then two Japanese armies and a detachment land at Bandoeng
   * from Saigon, and the detachment marches overland into Semarang.
   */
  const java = (first?: (g: Game, at: (nm: string) => Region) => void): { g: Game; at: (nm: string) => Region; knil: Corps[]; ashore: Corps[]; land: () => void; march: () => void } => {
    const g = wide();
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    setWar(g, 'JAP', 'UK');
    upTo(g, 'UK');
    const at = (nm: string): Region => g.byName[nm];
    const knil = ['KNIL 1st Division (West Java)', 'KNIL 2nd Division (Central Java)', 'KNIL 3rd Division (East Java)'].map((nm) => called(g, nm));
    // Kept at their posts, Britain's to command, on hold: Batavia, Semarang, Soerabaja.
    expect(knil.map((c) => [c.nat, c.raised, c.hold, !!c.stay, g.regions[c.reg].name])).toEqual([['UK', 'NED', true, false, 'Batavia'], ['UK', 'NED', true, false, 'Semarang'], ['UK', 'NED', true, false, 'Soerabaja']]);
    for (const c of knil) { c.str = 1; c.org = 1; c.supplied = true; }
    first?.(g, at);
    relocate(g, knil[1], at('Soerabaja'));
    const ashore = [3, 3, 1].map((divs, i) => addCorps(g, 'JAP', i < 2 ? `Test army ${i + 1}` : 'Test detachment', 'inf', divs, 'Saigon', {}));
    const land = (): void => { expect(landCorps(g, ashore, at('Bandoeng')).taken).toBe(true); };
    const march = (): void => { ashore[2].ap = ashore[2].apMax; expect(moveCorps(g, ashore[2], at('Semarang'))).toBe(true); };
    return { g, at, knil, ashore, land, march };
  };

  it('the Dutch divisions Britain keeps on Java stand through a landing and answer once the army ashore marches into the next province', () => {
    const { g, at, knil, ashore, land, march } = java();
    expect(frontAnswering(g, 'UK').filter((c) => knil.includes(c))).toEqual([]);

    // Two Japanese armies and a detachment land at Bandoeng from Saigon: the shore is won from the sea, this month.
    land();
    expect([at('Bandoeng').ctrl, at('Bandoeng').prev, g.flags[`shoreWon:${at('Bandoeng').id}`]]).toEqual(['JAP', 'UK', g.turn]);
    // A landing alone is no entry by land: the beach beside Batavia frees nobody.
    expect(frontAnswering(g, 'UK').filter((c) => knil.includes(c))).toEqual([]);

    // The detachment marches overland into Semarang, the next province: the front is entered.
    const detachment = ashore[2];
    march();
    expect([at('Semarang').ctrl, g.flags[`shoreWon:${at('Semarang').id}`]]).toEqual(['JAP', undefined]);
    expect(names(frontAnswering(g, 'UK').filter((c) => knil.includes(c)))).toEqual(names(knil));
    // Their ground is the front's: Java's own provinces, what was lost there, and the friendly ground next behind
    // (Bali, and Oosthaven across the strait). Not Sumatra's oil beyond it, not Malaya, not the enemy's harbour.
    const ground = grounds(g, 'UK', knil);
    for (const c of knil) {
      expect(['Batavia', 'Bandoeng', 'Semarang', 'Soerabaja', 'Bali', 'Oosthaven'].map((nm) => ground.get(c)!.has(at(nm).id)), c.name).toEqual([true, true, true, true, true, true]);
      expect(['Palembang', 'Singapore', 'Saigon'].map((nm) => ground.get(c)!.has(at(nm).id)), c.name).toEqual([false, false, false]);
      expect(c.hold, c.name).toBe(true);
    }

    // Britain's month. Its staff opens no offensive on Java against two armies; the divisions beside Semarang
    // counter-attack the detachment and take the town back, and are held again where they stand, on Java.
    const opened: string[] = [];
    offensiveWatch.tell = (e) => { if (e.nat === 'UK' && e.how !== 'quiet' && e.front.edge.some((id) => ['Bandoeng', 'Semarang'].includes(g.regions[id].name))) opened.push(e.how); };
    try { stepAI(g); } finally { offensiveWatch.tell = null; }
    expect(opened).toEqual([]);
    expect(at('Semarang').ctrl).toBe('UK');
    expect(g.regions[detachment.reg].name).not.toBe('Semarang');
    for (const c of knil) {
      if (c.dead) continue;
      expect(c.hold, c.name).toBe(true);
      expect(ground.get(c)!.has(c.reg), `${c.name} at ${g.regions[c.reg].name}`).toBe(true);
    }
    expect(names(at('Semarang').units.filter((c) => c.nat === 'UK'))).toEqual(['KNIL 2nd Division (Central Java)', 'KNIL 3rd Division (East Java)']);
  }, 600000);

  it('a shore is a beach only where the landing won it: a landing thrown back at Semarang does not make the march into it a landing', () => {
    // A weak party sails for Semarang while Central Java's division still stands there, and is thrown back into
    // the sea. Then, in the same month, the army lands at Bandoeng and its detachment marches into Semarang, which
    // the division has left. (Read from the mark a landing leaves as it sails, whatever becomes of it, the march
    // into Semarang was a landing at Semarang, everything Japan held on Java was a beach, and nobody answered.)
    const { g, at, knil, land, march } = java((g, at) => {
      const party = addCorps(g, 'JAP', 'Test landing party', 'inf', 1, 'Saigon', {});
      const res = landCorps(g, [party], at('Semarang'));
      expect([res.ok, res.taken, at('Semarang').ctrl]).toEqual([true, false, 'UK']);
      // (The coast watch's mark, left as the landing sailed: it says a landing came, not that it won.)
      expect(g.flags[`landedAt:${at('Semarang').id}:JAP`]).toBe(g.turn);
    });
    land();
    expect(frontAnswering(g, 'UK').filter((c) => knil.includes(c))).toEqual([]);
    march();
    expect([at('Semarang').ctrl, at('Semarang').since]).toEqual(['JAP', g.turn]);
    expect(names(frontAnswering(g, 'UK').filter((c) => knil.includes(c)))).toEqual(names(knil));
    // The shore the army won is marked with its month; the province it marched into is not.
    expect([g.flags[`shoreWon:${at('Bandoeng').id}`], g.flags[`shoreWon:${at('Semarang').id}`]]).toEqual([g.turn, undefined]);
  }, 600000);

  it('an answering row is not lifted off its front by sea, broken as it may be: West Java\'s division stays at Batavia', () => {
    const { g, at, knil, land, march } = java();
    land();
    march();
    // West Java's division was broken in a fight last month, at a harbour on ground that is not Britain's own: the
    // staff's standing rule for a corps abroad sails such a corps home (`wantsOut`, which reads no hold: a held row
    // that does not answer is lifted off as it always was). As first built the tie barred an answering row's marches
    // and trains and not this step, and the division ended Britain's month in London, a held row there.
    const west = knil[0];
    west.org = 0;
    west.fought = true;
    expect([g.regions[west.reg].name, at('Batavia').port, at('Batavia').owner, whyNotEvacuate(g, west)]).toEqual(['Batavia', true, 'NED', null]);
    expect(names(frontAnswering(g, 'UK'))).toContain(west.name);
    expect(wantsOut(g, west)).toBe(true);
    stepAI(g);
    // It answered where it stood and is a held row on Java still: not in London, not in Singapore.
    expect([west.dead, west.hold, g.regions[west.reg].name]).toEqual([false, true, 'Batavia']);
  }, 600000);
});
