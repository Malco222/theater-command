// AHEAD OF THE RULES: Version 84.2.6, the alarm (the design director's ruling 211(c), from the Playtester's trace of
// Brian's own campaign: one Canadian corps of guns walked through his empty north-east in December 1940 while the
// tray said nothing, and one order, the army round Vancouver by rail, would have cured it). Words:
// /mnt/project-files/ui-director/words-ruling-211.md, sections 1 and 2, passed by the design director at 16:22Z.
//
// One rules reading that both games draw: homeAlarms(g, nat) in danger.ts (the rules builder's name and shape, agreed
// 17:16Z), nearest his capital first, each
//   { case: 'stands' | 'taken' | 'storm' | 'stores', region, by, reach: number[] (his towns it can reach next month,
//     the capital first), help: { region, months, rail } | null, raise (months a new corps takes),
//     lines: [string, string, string] (the row's three sentences; the second left out where nothing is in reach) }.
//
// Every test here waits, skipped, until danger.ts exports homeAlarms; then they all run. The case is Brian's own save
// (tests/ahead/saves/brian-usa-1940-12.json: the United States, seed 89882, December 1940, his phase), so the row is
// tested on the very month that lost him two cities.
import { describe, expect, it } from 'vitest';
import * as danger from '../../src/campaign/danger';
import { armyCard } from '../../src/campaign/armies';
import { addCorps, capture, createGame, moveCorps, relocate, removeCorps } from '../../src/campaign/game';
import { setFog } from '../../src/campaign/intel';
import { loadGame, saveGame } from '../../src/campaign/save';
import { besieged, whyNotStorm } from '../../src/campaign/siege';
import { startOpen } from '../../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../../src/campaign/turn';
import type { Game, Region } from '../../src/campaign/types';
import brianSave from './saves/brian-usa-1940-12.json?raw';

interface HomeAlarm {
  case: 'stands' | 'taken' | 'storm' | 'stores';
  region: number;
  by: string;
  reach: number[];
  help: { region: number; months: number; rail: boolean } | null;
  raise: number;
  lines: string[];
}
// Read through a loose handle, so that this file type-checks on a tree where the reading is not built yet.
const homeAlarms = (danger as unknown as Record<string, unknown>).homeAlarms as ((g: Game, nat: string) => HomeAlarm[]) | undefined;
const waiting = typeof homeAlarms !== 'function';
const alarms = (g: Game, nat = 'USA'): HomeAlarm[] => homeAlarms!(g, nat);
/** The sentences the row says, in order (a sentence left out is an empty string or not there at all). */
const said = (a: HomeAlarm): string[] => a.lines.filter((s) => !!s);

/** Brian's own save, his month of December 1940, as he left it. */
function brian(): Game {
  const g = loadGame(brianSave);
  if (!g) throw new Error('the save did not load');
  return g;
}
const at = (g: Game, name: string): Region => g.byName[name];
/** A Canadian corps of infantry, ready to march: the fighting corps the guns at Buffalo lack. */
const canadians = (g: Game, where: string, name = 'Test Canadian infantry') => {
  const c = addCorps(g, 'CAN', name, 'inf', 2, at(g, where).id, {});
  c.ap = c.apMax;
  return c;
};
/** The sentence that names what the enemy can reach next month, built from the reading's own list (words, section 1). */
function reachLine(g: Game, a: HomeAlarm): string {
  const cap = g.nations.USA.capital;
  const names = a.reach.map((id) => (id === cap ? `${g.regions[id].name}, your capital` : g.regions[id].name));
  // (Two corps are "they": "2 Canadian corps stand at Buffalo, on your home ground. Next month they can reach ...".)
  const they = /^\d+ /.test(a.lines[0]);
  return `Next month ${they ? 'they' : 'it'} can reach ${names.length === 1 ? names[0] : `${names.slice(0, -1).join(', ')} or ${names[names.length - 1]}`}.`;
}

describe.skipIf(waiting)("84.2.6 the alarm (ruling 211(c)): Brian's own save, December 1940", () => {
  it('one red row: the Canadian guns at Buffalo, on his home ground (model 1 of the words, fog lifted)', () => {
    const g = brian();
    setFog(g, false);
    const all = alarms(g);
    // Two rows since the cities version: Rochester, a city of Buffalo's province drawn since the save was made, began
    // in Buffalo's hands, Canada's, with nobody in it (save.ts: a new city starts as its province stands). Buffalo's
    // own row is nearer his capital and comes first.
    expect(all.length).toBe(2);
    expect(all[1]).toMatchObject({ case: 'empty', region: at(g, 'Rochester').id, by: 'CAN' });
    const a = all[0];
    expect(a.case).toBe('taken');
    expect(a.region).toBe(at(g, 'Buffalo').id);
    expect(a.by).toBe('CAN');
    // Buffalo fell in Canada's own phase of November (turn 15), after he had ended his November month: Canada moves
    // after the United States in the order of play, and the save's November news runs his month (his corps closing
    // round Vancouver) before Buffalo's storm. December is the first month he sees it, so it reads "has taken"
    // (the design director, 18:14Z, log b340: counted by the order of play, not by the calendar month).
    expect(at(g, 'Buffalo').since).toBe(15);
    expect(g.turn).toBe(16);
    expect(g.order.indexOf('CAN')).toBeGreaterThan(g.order.indexOf('USA'));
    expect(said(a)[0]).toBe('A Canadian corps of guns has taken Buffalo.');
  });

  it('nearest help: the army round Vancouver, by rail, in the game\'s own count of months', () => {
    const g = brian();
    setFog(g, false);
    const a = alarms(g)[0];
    expect(a.help).not.toBeNull();
    expect(a.help!.region).toBe(at(g, 'Vancouver').id);
    // The game's own count (the rules builder, 84.2.6): no train leaves Vancouver this month, for its line is Canada's
    // and his army stands on enemy ground there (railReachFor is empty), so the help marches, and the march to
    // Pittsburgh rides the railways' costs: "2 months by road", which is what a right-click on Pittsburgh orders.
    expect(a.help!.rail).toBe(false);
    // The time to reach the province where the enemy stands (the design director, 17:22Z). Model 1 says "1 month";
    // planMarch to Pittsburgh next door counted 2 with this month on the old chart. On the cities chart the road from
    // Vancouver runs through 14 provinces and cities where it ran through 8, so it is 4. The row must say the
    // reading's own figure.
    expect(a.help!.months).toBe(4);
    const last = said(a)[said(a).length - 1];
    expect(last).toBe(`Nearest help: the army round Vancouver, ${a.help!.months} ${a.help!.months === 1 ? 'month' : 'months'} by ${a.help!.rail ? 'rail' : 'road'}.`);
  });

  it('guns alone reach none of his ground next month (84.2.5, 211(a)), so the row leaves its second sentence out', () => {
    // Confirmed by the design director at 17:22Z: the row is still red, with two sentences, the enemy and the help
    // (its first sentence "has taken Buffalo" in December, 18:14Z). Model 1's "Next month it can reach Pittsburgh"
    // was written from the trace, before guns were held back.
    const g = brian();
    setFog(g, false);
    const a = alarms(g)[0];
    expect(a.reach).toEqual([]);
    expect(said(a).length).toBe(2);
    expect(said(a).join(' ')).not.toMatch(/Next month it can reach/);
  });

  it('under the fog the row names the enemy exactly as the hover box does: unidentified, it names no kind', () => {
    // The design director, 17:22Z: "The row says exactly what the map's own hover box says for that province that
    // month, never more and never less." In his save the guns are in full sight but were never identified, so the
    // hover box reads "Canadian army: 1 corps, ... not identified" and names no kind.
    const g = brian();
    const card = armyCard(g, at(g, 'Buffalo').id, 'CAN', 'USA')!;
    expect(card.known).not.toBe('full');
    expect(card.unidentified).toBe(card.corpsCount);
    const first = said(alarms(g)[0])[0];
    expect(first).toMatch(/^A Canadian .+ ha(s|ve) taken Buffalo\.$/);
    expect(first).not.toMatch(/guns|artillery|infantry|tanks|armour|engineers/);
    // Lifted, the fog hides nothing, and the hover box and the row both name the guns.
    setFog(g, false);
    expect(armyCard(g, at(g, 'Buffalo').id, 'CAN', 'USA')!.known).toBe('full');
    expect(said(alarms(g)[0])[0]).toBe('A Canadian corps of guns has taken Buffalo.');
  });

  it('from January on, Buffalo "stands at": he has seen it fall', () => {
    const g = brian();
    setFog(g, false);
    g.turn += 1;
    const a = alarms(g).find((x) => x.region === at(g, 'Buffalo').id)!;
    expect(a).toBeDefined();
    expect(a.case).toBe('stands');
    expect(said(a)[0]).toBe('A Canadian corps of guns stands at Buffalo, on your home ground.');
  });

  it('only his home ground raises the alarm: the Mexican armies on Mexican ground he holds raise none', () => {
    const g = brian();
    // (On the cities chart the Mexican armies at Mexico City stand two steps from his conquests: its cities Morelia and
    // Poza Rica, still Mexican, lie between.)
    const near = (r: Region): boolean => r.nb.some((j) => g.regions[j].ctrl === 'USA' || g.regions[j].nb.some((k) => g.regions[k].ctrl === 'USA'));
    const mexicansOnHisConquests = g.regions.some((r) => r.owner === 'MEX' && r.ctrl === 'USA' && r.units.some((u) => !u.dead && u.nat === 'MEX'))
      || g.regions.some((r) => r.owner === 'MEX' && r.units.some((u) => !u.dead && u.nat === 'MEX') && near(r));
    expect(mexicansOnHisConquests).toBe(true);
    for (const a of alarms(g)) expect(g.regions[a.region].owner).toBe('USA');
  });
});

describe.skipIf(waiting)('84.2.6 the alarm (ruling 211(c)): what the enemy can reach, sieges and falls', () => {
  it('a fighting corps on his home ground names the towns it can reach next month, his own towns only, the capital first', () => {
    const g = brian();
    setFog(g, false);
    // January, when Buffalo "stands" (it "has taken" Buffalo in December, the month he first sees it fallen).
    g.turn += 1;
    canadians(g, 'Buffalo');
    const a = alarms(g).find((x) => x.region === at(g, 'Buffalo').id)!;
    expect(a).toBeDefined();
    expect(a.case).toBe('stands');
    expect(a.reach).toContain(at(g, 'Pittsburgh').id);
    for (const id of a.reach) expect(g.regions[id].owner).toBe('USA');
    const cap = g.nations.USA.capital;
    if (a.reach.includes(cap)) expect(a.reach[0]).toBe(cap);
    // Two corps, the guns and the infantry: "2 Canadian corps stand at Buffalo" (words, section 1: {Who} by its count).
    expect(said(a)[0]).toMatch(/Canadian .+ stands? at Buffalo, on your home ground\.$/);
    expect(said(a)[1]).toContain('Pittsburgh');
    if (a.reach.length <= 2) expect(said(a)[1]).toBe(reachLine(g, a));
  });

  it('a home town besieged: "can storm it this month" exactly where the rules would let the besieger storm, else its stores', () => {
    const g = brian();
    setFog(g, false);
    const P = at(g, 'Pittsburgh');
    const inf = canadians(g, 'Buffalo');
    expect(moveCorps(g, inf, P)).toBe(true);
    expect(besieged(g, P)).toBe(true);
    const a = alarms(g).find((x) => x.region === P.id)!;
    expect(a).toBeDefined();
    expect(a.by).toBe('CAN');
    // Pittsburgh holds its town guard alone (0 of 2 corps inside): the storm can come this month.
    const storm = whyNotStorm(g, P, 'CAN') === null;
    expect(storm).toBe(true);
    expect(a.case).toBe('storm');
    expect(said(a)[0]).toMatch(/^A?.*Canadian .+ is round Pittsburgh and can storm it this month\.$/);
  });

  it('a home town besieged that cannot be stormed this month reads its stores, or that ships still feed it', () => {
    const g = brian();
    setFog(g, false);
    const P = at(g, 'Pittsburgh');
    // Guns alone round it: by 211(a) they storm nothing, so the row counts the stores.
    const gun = addCorps(g, 'CAN', 'Test Canadian artillery', 'art', 2, at(g, 'Buffalo').id, {});
    const inf = canadians(g, 'Buffalo');
    gun.ap = gun.apMax;
    expect(moveCorps(g, inf, P)).toBe(true);
    expect(moveCorps(g, gun, P)).toBe(true);
    relocate(g, inf, at(g, 'Buffalo'));
    expect(besieged(g, P)).toBe(true);
    const why = whyNotStorm(g, P, 'CAN');
    const a = alarms(g).find((x) => x.region === P.id)!;
    expect(a).toBeDefined();
    expect(a.case).toBe(why === null ? 'storm' : 'stores');
    if (a.case === 'stores') expect(said(a)[0]).toMatch(/ is round Pittsburgh, which (has stores for \d+ months?|ships still feed)\.$/);
  });

  it('the month a home town falls the row says it has been taken', () => {
    const g = brian();
    setFog(g, false);
    const C = at(g, 'Cleveland');
    for (const u of [...C.units]) relocate(g, u, at(g, 'Washington'));
    capture(g, C, 'CAN');
    expect(C.since).toBe(g.turn);
    canadians(g, 'Cleveland');
    const a = alarms(g).find((x) => x.region === C.id)!;
    expect(a).toBeDefined();
    expect(a.case).toBe('taken');
    expect(said(a)[0]).toMatch(/Canadian .+ has taken Cleveland\.$/);
  });

  // "Has taken": a town that fell since he last ended his month, counted by the order of play, not by the calendar
  // month (the design director, 18:14Z, log b340). Each case sets the phase in which the town falls, then turns to
  // his own phase of the month in question.
  const fallsIn = (g: Game, town: string, phaseOf: string): Region => {
    const T = at(g, town);
    for (const u of [...T.units]) relocate(g, u, at(g, 'Washington'));
    const mine = g.phase;
    g.phase = g.order.indexOf(phaseOf);
    capture(g, T, 'CAN');
    g.phase = mine;
    canadians(g, town);
    return T;
  };
  const caseAt = (g: Game, T: Region): HomeAlarm['case'] | undefined => alarms(g).find((x) => x.region === T.id)?.case;

  it('a town a country that moves after him takes reads "has taken" in his next month, then "stands at"', () => {
    const g = brian();
    setFog(g, false);
    expect(g.order.indexOf('CAN')).toBeGreaterThan(g.order.indexOf('USA'));
    // Canada's phase of December, after he has ended his month.
    const C = fallsIn(g, 'Cleveland', 'CAN');
    g.turn += 1;
    expect(caseAt(g, C)).toBe('taken');
    expect(said(alarms(g).find((x) => x.region === C.id)!)[0]).toMatch(/^A Canadian .+ ha(s|ve) taken Cleveland\.$/);
    g.turn += 1;
    expect(caseAt(g, C)).toBe('stands');
    expect(said(alarms(g).find((x) => x.region === C.id)!)[0]).toMatch(/^A Canadian .+ stands? at Cleveland, on your home ground\.$/);
  });

  it('the reading survives his save: saved and loaded in his next month, the town still reads "has taken"', () => {
    const g = brian();
    setFog(g, false);
    const C = fallsIn(g, 'Cleveland', 'CAN');
    g.turn += 1;
    const back = loadGame(saveGame(g))!;
    expect(back).not.toBeNull();
    setFog(back, false);
    expect(caseAt(back, at(back, 'Cleveland'))).toBe('taken');
    expect(at(back, 'Cleveland').since).toBe(C.since);
  });

  it('a town that falls in his own phase (as his month opens, to hunger) he has seen: the next month it stands', () => {
    const g = brian();
    setFog(g, false);
    const C = fallsIn(g, 'Cleveland', 'USA');
    expect(caseAt(g, C)).toBe('taken');
    g.turn += 1;
    expect(caseAt(g, C)).toBe('stands');
  });

  it('several at once: the one nearest his capital comes first', () => {
    const g = brian();
    setFog(g, false);
    const inf = canadians(g, 'Buffalo');
    expect(moveCorps(g, inf, at(g, 'Pittsburgh'))).toBe(true);
    const all = alarms(g);
    expect(all.length).toBeGreaterThanOrEqual(2);
    expect(all[0].region).toBe(at(g, 'Pittsburgh').id);
    const d = g.capDist.USA;
    for (let k = 1; k < all.length; k++) expect(d[all[k].region]).toBeGreaterThanOrEqual(d[all[k - 1].region]);
  });

  it('where no army of his can come the row says so, and what a new corps takes', () => {
    const g = brian();
    setFog(g, false);
    for (const c of g.corps.filter((x) => x.nat === 'USA' && !x.dead)) removeCorps(g, c);
    const a = alarms(g)[0];
    expect(a.help).toBeNull();
    // The trace: "a new corps takes 5 months" (a corps of infantry on the Arsenal's page).
    expect(a.raise).toBe(5);
    expect(said(a)[said(a).length - 1]).toBe('No army of yours can come. A new corps takes 5 months.');
  });

  it('no alarm at all where no enemy stands on his home ground', () => {
    const g = startOpen(createGame(3, 'USA'));
    for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
    beginHuman(g);
    expect(alarms(g)).toEqual([]);
  });
});

describe.skipIf(waiting)("84.2.6 the alarm (ruling 211(c)): the month card's warning ends with the same help", () => {
  it('the warning ends with the row\'s third sentence, and says the enemy\'s place once (words, section 2)', () => {
    const g = brian();
    setFog(g, false);
    const inf = canadians(g, 'Buffalo');
    expect(moveCorps(g, inf, at(g, 'Pittsburgh'))).toBe(true);
    const w = danger.fallWarning(g, 'USA', false);
    expect(w).not.toBeNull();
    const first = alarms(g)[0];
    const help = said(first)[said(first).length - 1];
    expect(help).toMatch(/^(Nearest help: |No army of yours can come\.)/);
    expect(w!.text.endsWith(help)).toBe(true);
    // Today the card says "Canadian troops stand in Pittsburgh, beside the capital. Canadian troops are a march from
    // Washington, ...": the second goes.
    expect(w!.text.split('Canadian troops').length - 1).toBeLessThanOrEqual(1);
  });
});
