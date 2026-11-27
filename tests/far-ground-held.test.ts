// Ground taken has to be held (rider C of 20:45Z, 20 November 2026, to ruling 234, sized by the design director's
// note of 20:50Z on the historian's paper; src/campaign/staff.ts, garrisonMonth and fronts).
//
// After Wuhan about three quarters of Japan's army in China held ground and about a quarter was free: about half a
// division's worth for each province held. As built a staff's sums counted every corps its row did not hold as free,
// so the first day read 1.64 to 1.71 on the main front against China, over every temper's mark to open, and in the
// wars measured nobody was left behind for fifty provinces taken in eight months. Every province of taken ground
// beyond the old chart now asks a garrison of 0.5 of a division's worth standing in that ground; a computer staff
// names it from its own corps there, the rear first, and its sums count only the corps left free. A garrison is a
// choice and not a vow: a great choice draws on it. The state is set by hand on the first day's chart; one test
// plays Japan's first month.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import stop3 from '../data/campaign/china-stop3.json';
import { computeSupply, createGame, declareWar } from '../src/campaign/game';
import { pacificGround } from '../src/campaign/pacific';
import { fronts, garrisonAsk, garrisonMonth, offensiveWatch, startOpen, takenGround, temper, type Front } from '../src/campaign/staff';
import { release } from '../src/campaign/ai';
import { stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

/** A war nobody plays on the first day, far side on, at the stop where history is loose. */
const war = (): Game => { const g = startOpen(createGame(3, null)); computeSupply(g); return g; };
/** Japan's main front against China. */
const main = (g: Game, gather = false): Front => fronts(g, 'JAP', { gather }).find((f) => f.depth[g.byName['Chungking'].id] >= 1)!;
const worth = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
/** Japan's corps standing on a piece of taken ground. */
const on = (g: Game, ids: number[]): Corps[] => ids.flatMap((id) => g.regions[id].units.filter((c) => c.nat === 'JAP' && !c.dead));
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.nat === 'JAP' && c.name === name)!;
/** The 11th Army round Hankow, the record's field force, and a division of the rear. */
const FIELD = ['11th Army (Nanchang)', '11th Army (Yochow)', '11th Army (3rd Division, Sinyang)', '11th Army (33rd Division)'];
const REAR = ['Tientsin-Pukow railway garrison (21st Division)', '12th Army (5th Division)', '12th Army (Shantung)'];

describe('ground taken has to be held (rider C to ruling 234)', () => {
  it('is the far side\'s row: half a division\'s worth a province, at the stops where the reading decides', () => {
    const G = stop3.open.garrison;
    expect([G.each, G.stops]).toEqual([0.5, [3, 4, 5]]);
    // The record's figure for a province of each region of the Pacific's ground (rider of 22:30Z and the notes of
    // 22:28Z and 22:33Z; the historian's japan-south-garrison-1942.md and pacific-garrison-rest.md).
    expect(G.by.map((b) => [b.name, b.each, b.places.length])).toEqual([
      ['the Philippines', 0.2, 14], ['Malaya with Singapore', 0.2, 7], ['Sumatra', 0.15, 7], ['Java', 0.2, 4],
      ['Borneo and the eastern islands', 0.05, 25], ['Burma', 0.1, 12], ['Thailand, taken against its will', 0.1, 7], ['Indochina', 0.1, 11],
      ['New Guinea and the Bismarcks, the Solomons, the Gilberts and Nauru', 0.05, 15], ['the mandates and Guam', 0.05, 10],
      ['Hong Kong, Okinawa, Karafuto, the Kurils and Formosa', 0.1, 7], ['Iwo Jima, the Bonins and Marcus', 0, 3],
      // The record's figures by region for the ground Japan held in China on the first day (the design director's
      // 05:14Z of 21 November 2026): each place named asks the figure itself.
      ['North China, as held on the first day (13 across its 23 provinces)', 0.5652, 23], ['central China, as held on the first day (about 7 across its 11 provinces)', 0.6364, 11],
    ]);
    expect(G.by.filter((b) => (b as { own?: boolean }).own).map((b) => b.name.slice(0, 13))).toEqual(['North China, ', 'central China']);
    // Every place a row names is a province of the Pacific's ground, and none is named twice.
    const g = war();
    const names = G.by.flatMap((b) => b.places);
    expect(new Set(names).size).toBe(names.length);
    for (const nm of names) { expect(g.byName[nm], nm).toBeTruthy(); expect(pacificGround(g, g.byName[nm]), nm).toBe(true); }
  });

  it('asks each region of the south its own figure, and a province cut finer shares it among its parts', () => {
    const g = war();
    const takes = (...names: string[]): void => { for (const nm of names) { g.byName[nm].ctrl = 'JAP'; } };
    const asked = (name: string): number => takenGround(g, 'JAP').find((p) => p.ids.includes(g.byName[name].id))!.ask;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    // Luzon's Manila alone: 0.2. Java's four provinces: 0.8. Borneo's Tarakan: its part of 0.05.
    takes('Manila');
    expect(asked('Manila')).toBe(0.2);
    takes('Batavia', 'Bandoeng', 'Semarang', 'Soerabaja');
    expect(asked('Batavia')).toBe(0.8);
    // Sumatra's seven provinces are eleven on the chart, and ask the record's seven times 0.15 together.
    const sumatra = ['Atjeh', 'Medan', 'Pakanbaru', 'Padang', 'Djambi', 'Palembang', 'Oosthaven', 'Pematang Siantar', 'Padang Sidempoean', 'Benkoelen', 'Pangkalpinang'];
    expect(Math.round(sumatra.reduce((s, nm) => s + garrisonAsk(g, g.byName[nm]), 0) * 100) / 100).toBe(1.05);
    expect(garrisonAsk(g, g.byName['Pematang Siantar'])).toBeLessThan(0.15);
    takes('Medan', 'Pematang Siantar', 'Atjeh');
    expect(asked('Medan')).toBe(Math.round(['Medan', 'Pematang Siantar', 'Atjeh'].reduce((s, nm) => s + garrisonAsk(g, g.byName[nm]), 0) * 100) / 100);
    // Hong Kong asks 0.1 and Marcus Island nothing; China's ground taken after the first day asks 0.5 as before.
    expect([garrisonAsk(g, g.byName['Hong Kong']), garrisonAsk(g, g.byName['Marcus Island']), garrisonAsk(g, g.byName.Changsha), garrisonAsk(g, g.byName.Tokyo)]).toEqual([0.1, 0, 0.5, 0.5]);
    // The ground held in China on the first day asks its region's figure place by place, whatever the chart's cut:
    // Nanking's province is three places on the chart, and Yencheng, China's on the first day, asks 0.5 if taken.
    expect([garrisonAsk(g, g.byName.Nanking), garrisonAsk(g, g.byName.Wuhu), garrisonAsk(g, g.byName.Yencheng), garrisonAsk(g, g.byName.Peiping), garrisonAsk(g, g.byName.Wuyuan), garrisonAsk(g, g.byName.Canton)]).toEqual([0.6364, 0.6364, 0.5, 0.5652, 0.5, 0.5]);
  });

  it('asks nothing beyond the Pacific\'s ground: Iran, Arabia, Africa and inland India wait for the rule\'s general form', () => {
    const g = war();
    for (const [nat, owner, places] of [['SOV', 'IRN', ['Tehran', 'Hamadan', 'Qazvin']], ['ITA', 'FRA', ['Faya-Largeau', 'Zouar']], ['GER', 'UK', ['Qena', 'Minya']], ['JAP', 'UK', ['Delhi', 'Lahore']]] as [string, string, string[]][]) {
      if (!g.nations[nat].atWar.includes(owner)) declareWar(g, nat, owner);
      for (const nm of places) { expect(g.byName[nm], nm).toBeTruthy(); expect(pacificGround(g, g.byName[nm]), nm).toBe(false); g.byName[nm].ctrl = nat; }
      expect(takenGround(g, nat).filter((p) => p.ids.some((id) => places.includes(g.regions[id].name)))).toEqual([]);
    }
    // The coast of the Bay of Bengal is the Pacific's ground, as the landing rules read it: Madras taken asks its garrison.
    g.byName.Madras.ctrl = 'JAP';
    expect(takenGround(g, 'JAP').find((p) => p.ids.includes(g.byName.Madras.id))!.ask).toBe(0.5);
  });

  it('counts all the ground held on the first day: 39 provinces of China in four pieces ask 22.5, the north 13 and the centre 7', () => {
    const g = war();
    const pieces = takenGround(g, 'JAP');
    expect(pieces.map((p) => p.ids.length).sort((a, b) => b - a)).toEqual([34, 2, 2, 1]);
    expect(pieces.map((p) => [p.ids.length, p.ask]).sort((a, b) => b[0] - a[0] || b[1] - a[1])).toEqual([[34, 20], [2, 1], [2, 1], [1, 0.5]]);
    expect(pieces.reduce((s, p) => s + p.ask, 0)).toBe(22.5);
    for (const p of pieces) for (const id of p.ids) { expect(g.regions[id].owner).toBe('CHN'); expect(g.regions[id].ctrl).toBe('JAP'); }
    // Nothing of Japan's own, of Korea's or of Manchukuo's is taken ground, and China has taken none.
    expect(takenGround(g, 'CHN')).toEqual([]);
  });

  it('names the garrison from the rear, as near the sum asked as whole corps allow', () => {
    const g = war();
    const held = g.corps.filter((c) => c.nat === 'JAP' && c.hold).map((c) => c.id);
    garrisonMonth(g, 'JAP');
    const big = takenGround(g, 'JAP').find((p) => p.ids.length === 34)!;
    const there = on(g, big.ids);
    const posted = there.filter((c) => c.garr !== undefined), free = there.filter((c) => c.garr === undefined);
    // No corps of this piece is worth more than 1.8, and none on garrison more than 0.9: the garrison is within half a division of 17.
    expect(Math.abs(worth(posted) - big.ask)).toBeLessThan(0.45);
    // About a quarter of the army is left free, as in the record.
    expect(worth(free) / worth(there)).toBeGreaterThan(0.2);
    expect(worth(free) / worth(there)).toBeLessThan(0.3);
    // A corps the staff names is held and marked to stay; a row that stood there held counts as it stands.
    for (const c of posted) {
      expect(c.hold).toBe(true);
      expect(c.garr).toBe(held.includes(c.id) ? 0 : 1);
      if (c.garr === 1) expect(c.stay).toBe(true);
    }
    for (const c of free) expect(c.hold).toBe(false);
    // The rear is named and the field force round Hankow, nearest the enemy's capital, is left free; so are the tanks.
    for (const nm of REAR) expect(corps(g, nm).garr).toBe(1);
    for (const nm of FIELD) expect(corps(g, nm).garr).toBeUndefined();
    // (Armour is never named to hold ground, whatever the sum is short of: the director's 05:51Z of 21 November 2026.)
    expect(there.filter((x) => x.kind === 'arm').map((c) => c.name).sort()).toEqual(['12th Tank Regiment', '13th Tank Regiment']);
    for (const c of there.filter((x) => x.kind === 'arm')) expect(c.garr).toBeUndefined();
    // Nobody off the taken ground is named: the armies in Manchukuo, Korea and at home are as they were.
    const ground = new Set(takenGround(g, 'JAP').flatMap((p) => p.ids));
    for (const c of g.corps) if (c.nat === 'JAP' && !ground.has(c.reg)) expect(c.garr).toBeUndefined();
  });

  it('counts only the corps left free: the first day opens no general offensive', () => {
    const g = war();
    const before = main(g);
    // (With nobody named the front reads over this war's mark to open. The bound stood at 1.5 while a front read what
    // the pool spared; since one membership, the design director's 14:08Z and 14:31Z of 21 November 2026, it reads its
    // members alone, 1.32 on the first day, and the bound is the rules' own mark.)
    expect(before.ratio).toBeGreaterThanOrEqual(temper(g, 'JAP').attackAt);
    // (Why the members are under the pool's figure, 193.4 of 265.6; the director's 15:30Z. The pool counted the rows
    // that hold and stay beside the line, and what the Yencheng patch has for its own. Let the rows go and the members
    // are more, but not yet the pool's figure; make Yencheng Japan's as well, so that no patch stands beside the
    // main front, and they are the pool's figure to the last place.)
    expect(before.members!).toBeLessThan(before.atk);
    const h = war();
    for (const c of h.corps) if (c.nat === 'JAP' && c.hold && c.stay) delete c.stay;
    const freed = main(h);
    expect(freed.atk).toBeCloseTo(before.atk, 9);
    expect(freed.members!).toBeGreaterThan(before.members!);
    expect(freed.members!).toBeLessThan(freed.atk);
    const y = h.byName['Yencheng'];
    for (const u of y.units) u.dead = true;
    y.units = []; y.ctrl = 'JAP';
    h.corps = h.corps.filter((c) => !c.dead);
    expect(fronts(h, 'JAP')).toHaveLength(1);
    const alone = main(h);
    expect(alone.atk).toBeCloseTo(before.atk, 9);
    expect(alone.members!).toBeCloseTo(alone.atk, 9);
    garrisonMonth(g, 'JAP');
    const after = main(g);
    expect(after.def).toBe(before.def);
    expect(after.atk).toBeLessThan(0.35 * before.atk);
    // The historian's rough sum was about 0.4; the mark to open is 1.04 at the lowest temper.
    expect(after.ratio).toBeGreaterThan(0.3);
    expect(after.ratio).toBeLessThan(0.6);
    // A great choice weighs what it could gather, garrisons and all.
    expect(main(g, true).ratio).toBeGreaterThan(1.5);
  });

  it('keeps a named corps through a general offensive\'s release', () => {
    const g = war();
    garrisonMonth(g, 'JAP');
    const named = g.corps.filter((c) => c.garr === 1);
    expect(named.length).toBeGreaterThan(10);
    release(g, 'JAP', g.regions.map((r) => r.name));
    for (const c of named) expect(c.hold).toBe(true);
  });

  it('is a choice and not a vow: with less ground to hold the last named are let go, and next month\'s are the same corps', () => {
    const g = war();
    garrisonMonth(g, 'JAP');
    const first = g.corps.filter((c) => c.garr === 1).map((c) => c.id);
    garrisonMonth(g, 'JAP');
    expect(g.corps.filter((c) => c.garr === 1).map((c) => c.id)).toEqual(first);
    // Six empty provinces of the big piece go back to China: three divisions' worth less is asked.
    const big = takenGround(g, 'JAP').find((p) => p.ids.length === 34)!;
    const empty = big.ids.filter((id) => !g.regions[id].units.length).slice(0, 6);
    expect(empty).toHaveLength(6);
    for (const id of empty) g.regions[id].ctrl = 'CHN';
    const was = worth(g.corps.filter((c) => c.nat === 'JAP' && c.garr !== undefined));
    garrisonMonth(g, 'JAP');
    const now = g.corps.filter((c) => c.garr === 1).map((c) => c.id);
    expect(now.length).toBeLessThan(first.length);
    expect(was - worth(g.corps.filter((c) => c.nat === 'JAP' && c.garr !== undefined))).toBeGreaterThan(2);
    // (A corps of last month's is kept before any other is named; a small one may be added to come nearer the sum.)
    expect(now.filter((id) => first.includes(id)).length).toBeGreaterThanOrEqual(now.length - 1);
    const let_go = g.corps.filter((x) => first.includes(x.id) && !now.includes(x.id));
    expect(let_go.length).toBeGreaterThan(2);
    for (const c of let_go) { expect(c.hold).toBe(false); expect(c.stay).toBeUndefined(); expect(c.garr).toBeUndefined(); }
  });

  it('is drawn on by a great choice: while one is pressed against China, China\'s ground asks nothing', () => {
    const g = war();
    const unnamed = main(g).ratio;
    garrisonMonth(g, 'JAP');
    const stayed = g.corps.filter((c) => c.nat === 'JAP' && c.garr === 0 && c.stay).map((c) => c.id);
    expect(g.corps.some((c) => c.garr === 1)).toBe(true);
    g.flags['st:push:JAP'] = g.turn + 3;
    g.flags['st:pushed:JAP:CHN'] = 1;
    expect(takenGround(g, 'JAP').reduce((s, p) => s + p.ask, 0)).toBe(0);
    garrisonMonth(g, 'JAP');
    expect(g.corps.some((c) => c.garr !== undefined)).toBe(false);
    // (The front reads again what it read with nobody named, over this war's mark to open: its members, since one
    // membership. The bound stood at 1.5 while a front read what the pool spared.)
    expect(main(g).ratio).toBeCloseTo(unnamed, 9);
    expect(main(g).ratio).toBeGreaterThanOrEqual(temper(g, 'JAP').attackAt);
    // The rows the record marks to stay keep their own hold: only what the staff named is let go.
    for (const c of g.corps.filter((x) => stayed.includes(x.id))) { expect(c.hold).toBe(true); expect(c.stay).toBe(true); }
    // The push over, the ground asks its garrison again.
    g.flags['st:push:JAP'] = g.turn - 1;
    garrisonMonth(g, 'JAP');
    expect(g.corps.some((c) => c.garr === 1)).toBe(true);
  });

  it('never names a row a plan keeps for itself', () => {
    const g = war();
    garrisonMonth(g, 'JAP', (c) => REAR.includes(c.name));
    for (const nm of REAR) { expect(corps(g, nm).garr).toBeUndefined(); expect(corps(g, nm).hold).toBe(false); }
    expect(g.corps.filter((c) => c.garr === 1).length).toBeGreaterThan(10);
  });

  it('binds no player, and nobody at the book\'s stops', () => {
    const p = startOpen(createGame(3, 'JAP'));
    computeSupply(p);
    garrisonMonth(p, 'JAP');
    expect(p.corps.some((c) => c.garr !== undefined)).toBe(false);
    const g = war();
    g.flags.hist = 1;
    garrisonMonth(g, 'JAP');
    expect(g.corps.some((c) => c.garr !== undefined)).toBe(false);
  });

  it('is named in the staff\'s own month before it reads its fronts: Japan\'s first month reads under every mark', () => {
    const g = war();
    const read: number[] = [];
    offensiveWatch.tell = (e) => { if (e.nat === 'JAP' && g.turn === 1 && e.front.depth[g.byName['Chungking'].id] >= 1) read.push(e.front.ratio); };
    try { const t = g.turn; while (g.turn === t) stepAI(g); } finally { offensiveWatch.tell = null; }
    expect(read).toHaveLength(1);
    // What the record asks is that Japan's China front opens nothing of its own in the first month, so the bound is
    // the rules' own lowest mark to open, 1.04 (the design director's 05:51Z of 21 November 2026; the 0.6 that stood
    // here was a proxy taken from the old rows' 0.53).
    expect(read[0]).toBeLessThan(1.04);
    expect(g.corps.filter((c) => c.nat === 'JAP' && c.garr === 1).length).toBeGreaterThan(10);
  });
});
