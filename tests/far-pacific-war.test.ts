// The Pacific war's ties with the far side's switch on (ruling 197, the offer of war: the Pacific planner's paper 12,
// section 4's cases as tests, with the freeze of section 5 and the Soviet month of section 6). The states are set by
// hand on the first day's chart and the month's readings are called as the game calls them: no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { DIPLOMACY, closeDiplomacy, monthlyDiplomacy, sneakOn } from '../src/campaign/diplomacy';
import { FAR_LAID } from '../src/campaign/far-rows';
import { atWar, bloc, createGame, declareWar, setWar } from '../src/campaign/game';
import { HISTORY, setStop } from '../src/campaign/history';
import { ops, startOpen } from '../src/campaign/staff';
import { PACIFIC, freezeAt, pacificOpening, pacificSides, pactAt, pactBinds } from '../src/campaign/pacific';
import { declarable } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A grand campaign on the whole world at a stop of the dial, moved by hand to a month of the war. */
const war = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  return g;
};
/** The month turns by hand: its close, the calendar, its opening. */
const turnMonth = (g: Game): void => { closeDiplomacy(g); g.turn++; monthlyDiplomacy(g); };
const news = (g: Game, turn: number, part: string): boolean => g.headlines.some((h) => h.turn === turn && h.text.includes(part));
const fell = (g: Game, id: string, turn: number, to: string): void => {
  const n = g.nations[id];
  n.alive = false;
  n.fellOn = { turn, week: 1, to };
  for (const x of Object.values(g.nations)) x.atWar = x.atWar.filter((e) => e !== id);
  n.atWar = [];
};
const DOMINIONS = ['AUS', 'NZL', 'CAN', 'SAF'];

describe('the offer of war (ruling 197): the first day', () => {
  it('lays the Pacific war file, and reads its rules', () => {
    expect(FAR_LAID).toContain('far-pacific-war');
    expect(FAR_LAID.indexOf('far-pacific-war')).toBeLessThan(FAR_LAID.indexOf('china-line'));
    expect(PACIFIC?.power).toBe('JAP');
    const entry = DIPLOMACY.events.filter((e) => e.id === 'pearl-harbor-world');
    expect(entry.length).toBe(1);
    expect(entry[0].when).toBe('close');
    expect(entry[0].from).toBe(28);
    expect(DIPLOMACY.events.some((e) => e.id === 'pact-binds')).toBe(true);
    // Japan's great choices in China are still its own (the "+" rule: a later file lays over what stands), and the
    // strike south stands before them: it has the first call of a month.
    expect(ops('JAP').some((op) => op.id === 'ichigo')).toBe(true);
    expect(HISTORY.dates.JAP?.ichigo).toBe(56);
    expect(ops('JAP')[0].id).toBe('south');
    // (Eight, and the four the book lacked for China: Chekiang 1942, the 1944 coast, Kukong and Laohokow, the design
    // director's rulings of 21 November 2026; tests/china-rows-1121.test.ts.)
    expect(ops('JAP').length).toBe(12);
    expect(HISTORY.dates.JAP?.south).toBe(28);
  });

  it('puts Japan outside both blocs leaning to the Axis, at war with China, and every country wanted on its list', () => {
    const g = war(3, 1, 'JAP');
    const J = g.nations.JAP;
    expect(J.align).toBe('leaning_axis');
    expect(bloc(J)).toBeNull();
    expect(J.atWar).toEqual(['CHN']);
    expect(bloc(g.nations.CHN)).toBeNull();
    expect(J.sealift).toBeGreaterThan(0);
    const list = declarable(g);
    for (const id of ['USA', 'UK', 'NED', 'FRA', 'SOV']) expect(list, id).toContain(id);
    for (const [me, on] of [['USA', 'JAP'], ['UK', 'JAP'], ['SOV', 'JAP']]) expect(declarable(war(3, 1, me)), `${me} on ${on}`).toContain(on);
  });
});

describe('the offer of war (ruling 197): who follows whom', () => {
  it("history: America first, then Britain and the Dutch in December 1941, each of the first two caught unready, and Germany's declaration as the month closes", () => {
    const g = war(1, 27);
    turnMonth(g);
    // The month's opening does not bring America in: Japan has not moved yet.
    expect(g.turn).toBe(28);
    expect(g.flags['event:pearl-harbor-world']).toBeUndefined();
    expect(bloc(g.nations.USA)).toBeNull();
    declareWar(g, 'JAP', 'USA');
    expect(sneakOn(g, 'JAP', 'USA')).toBe(true);
    // Striking a neutral America makes Japan nobody's co-belligerent, and America nobody's ally.
    expect(bloc(g.nations.JAP)).toBeNull();
    expect(bloc(g.nations.USA)).toBeNull();
    declareWar(g, 'JAP', 'UK');
    expect(sneakOn(g, 'JAP', 'UK')).toBe(true);
    expect(g.nations.JAP.align).toBe('axis_cobelligerent');
    // The dominions are at war with Japan the day Britain is.
    for (const id of DOMINIONS) expect(atWar(g, id, 'JAP'), id).toBe(true);
    expect(news(g, 28, 'at war with Japan, beside Britain')).toBe(true);
    // The Netherlands, still neutral on this chart's first day, is struck as a neutral is and joins the Allies.
    declareWar(g, 'JAP', 'NED');
    expect(atWar(g, 'JAP', 'NED')).toBe(true);
    expect(bloc(g.nations.NED)).toBe('allies');
    // America is at war with Japan alone until the month closes.
    expect(atWar(g, 'USA', 'GER')).toBe(false);
    closeDiplomacy(g);
    expect(g.flags['event:pearl-harbor-world']).toBe(28);
    expect(bloc(g.nations.USA)).toBe('allies');
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    // The words are true (rider b641 (e) to ruling 234): this war was declared by hand and no raid was flown at
    // Pearl Harbor, so the entry speaks its plain words and does not name the harbour.
    expect(g.flags['harbour:JAP:Pearl Harbor']).toBeUndefined();
    expect(news(g, 28, 'America has entered the war: Germany and Italy declare war on the United States')).toBe(true);
    expect(news(g, 28, 'Pearl Harbor')).toBe(false);
    // China stands with the Allies from the month Japan fights beside the Axis.
    expect(bloc(g.nations.CHN)).toBe('allies');
    expect(atWar(g, 'CHN', 'JAP')).toBe(true);
    // The freeze came on its date, in August.
    expect(freezeAt(g)).toBe(28); // (this war was moved by hand from the first day to November: the date had passed)
  });

  it("the entry names Pearl Harbor where Japan's aircraft raided it, and only there (rider b641 (e))", () => {
    // The flag is the one every harbour raid leaves as it is flown (air.ts, `fly`; a real strike: far-player-navy.test.ts).
    const g = war(1, 27);
    turnMonth(g);
    declareWar(g, 'JAP', 'USA');
    g.flags['harbour:JAP:Pearl Harbor'] = 28;
    closeDiplomacy(g);
    expect(g.flags['event:pearl-harbor-world']).toBe(28);
    expect(news(g, 28, 'Japan attacks the American fleet at Pearl Harbor, and Germany and Italy declare war on the United States')).toBe(true);
    expect(news(g, 28, 'America has entered the war: Germany and Italy declare war')).toBe(false);
    // A raid on another harbour, or another country's raid, is not Japan's on Pearl Harbor.
    const h = war(1, 27);
    turnMonth(h);
    declareWar(h, 'JAP', 'USA');
    h.flags['harbour:JAP:Manila'] = 28;
    h.flags['harbour:GER:Pearl Harbor'] = 28;
    closeDiplomacy(h);
    expect(news(h, 28, 'America has entered the war: Germany and Italy declare war')).toBe(true);
    expect(news(h, 28, 'Pearl Harbor')).toBe(false);
  });

  it('the Dutch, while they stand with the Allies, are at war with Japan the day Britain is', () => {
    const g = war(1, 28);
    g.nations.NED.align = 'allies';
    setWar(g, 'NED', 'GER');
    declareWar(g, 'JAP', 'UK');
    expect(atWar(g, 'NED', 'JAP')).toBe(true);
    expect(g.flags['war:NED:JAP']).toBe(28);
    // A dominion in the player's hands goes to war only when its player says so.
    const h = war(1, 28, 'AUS');
    declareWar(h, 'JAP', 'UK');
    expect(atWar(h, 'AUS', 'JAP')).toBe(false);
    expect(atWar(h, 'NZL', 'JAP')).toBe(true);
  });

  it('Britain and the Dutch alone in June 1941: the freeze the month after, and America from January 1942', () => {
    const g = war(3, 22, 'JAP');
    declareWar(g, 'JAP', 'UK');
    declareWar(g, 'JAP', 'NED');
    expect(g.nations.JAP.align).toBe('axis_cobelligerent');
    expect(freezeAt(g)).toBe(0);
    turnMonth(g);
    expect(freezeAt(g)).toBe(23);
    expect(news(g, 23, 'America has stopped selling oil and steel to Japan.')).toBe(true);
    expect(news(g, 23, 'done the same')).toBe(false); // the south is at war with Japan already: its trade stopped that day
    while (g.turn < 28) { turnMonth(g); expect(atWar(g, 'USA', 'JAP'), `month ${g.turn}`).toBe(false); }
    // December 1941: America enters the war on the event's own date, and its words do not name Pearl Harbor.
    closeDiplomacy(g);
    expect(g.flags['event:pearl-harbor-world']).toBe(28);
    expect(news(g, 28, 'America has entered the war')).toBe(true);
    expect(news(g, 28, 'Pearl Harbor')).toBe(false);
    expect(atWar(g, 'USA', 'JAP')).toBe(false);
    g.turn++;
    monthlyDiplomacy(g);
    // January 1942, the month after it enters the war.
    expect(atWar(g, 'USA', 'JAP')).toBe(true);
    expect(g.flags['war:USA:JAP']).toBe(29);
    expect(sneakOn(g, 'USA', 'JAP')).toBe(false);
  });

  it('America alone in March 1940, before the pact: alone until December 1941, then Germany and Italy by the event, and Britain the month after', () => {
    const g = war(1, 7, 'JAP');
    declareWar(g, 'JAP', 'USA');
    expect(sneakOn(g, 'JAP', 'USA')).toBe(true);
    while (g.turn < 28) {
      turnMonth(g);
      expect(bloc(g.nations.USA), `month ${g.turn}`).toBeNull();
      expect(atWar(g, 'USA', 'GER'), `month ${g.turn}`).toBe(false);
      expect(atWar(g, 'UK', 'JAP'), `month ${g.turn}`).toBe(false);
    }
    // The pact binds from September 1940 at this stop, and not this war: it began before.
    expect(pactAt(g)).toBe(13);
    expect(pactBinds(g, 'JAP', 'USA')).toBe(false);
    expect(g.flags['event:pact-binds']).toBeUndefined();
    closeDiplomacy(g);
    expect(g.flags['event:pearl-harbor-world']).toBe(28);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    // Japan fights beside the Axis from the month America, its enemy, is at war with Germany.
    expect(g.nations.JAP.align).toBe('axis_cobelligerent');
    expect(atWar(g, 'UK', 'JAP')).toBe(false);
    g.turn++;
    monthlyDiplomacy(g);
    expect(atWar(g, 'UK', 'JAP')).toBe(true);
    expect(g.flags['war:UK:JAP']).toBe(29);
    for (const id of DOMINIONS) expect(atWar(g, id, 'JAP'), id).toBe(true);
  });

  it('America alone in November 1940, after the pact: Germany and Italy the month after, Britain the month after that', () => {
    const g = war(1, 15, 'JAP');
    monthlyDiplomacy(g);
    expect(pactAt(g)).toBe(13);
    declareWar(g, 'JAP', 'USA');
    expect(pactBinds(g, 'JAP', 'USA')).toBe(true);
    turnMonth(g);
    expect(g.turn).toBe(16);
    expect(g.flags['event:pact-binds']).toBe(16);
    expect(bloc(g.nations.USA)).toBe('allies');
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(g.nations.JAP.align).toBe('axis_cobelligerent');
    expect(atWar(g, 'UK', 'JAP')).toBe(false);
    turnMonth(g);
    expect(atWar(g, 'UK', 'JAP')).toBe(true);
    expect(g.flags['war:UK:JAP']).toBe(17);
    // The entry has been made: December 1941 brings nothing more.
    while (g.turn < 29) turnMonth(g);
    expect(g.flags['event:pearl-harbor-world']).toBeUndefined();
  });

  it('where history is loose the pact binds from the third month after France is beaten, and only a war begun since', () => {
    const g = war(3, 15, 'JAP');
    monthlyDiplomacy(g);
    expect(pactAt(g)).toBe(0); // France stands
    declareWar(g, 'JAP', 'USA');
    turnMonth(g);
    expect(g.flags['event:pact-binds']).toBeUndefined();
    expect(bloc(g.nations.USA)).toBeNull();
    // France beaten in June 1940: the pact binds from September, and this war of November is bound.
    const h = war(3, 15, 'JAP');
    fell(h, 'FRA', 10, 'GER');
    monthlyDiplomacy(h);
    expect(pactAt(h)).toBe(13);
    declareWar(h, 'JAP', 'USA');
    turnMonth(h);
    expect(h.flags['event:pact-binds']).toBe(16);
    // France beaten in October 1940: the pact binds from January 1941, and a war begun in November stays Japan's alone.
    const k = war(3, 15, 'JAP');
    declareWar(k, 'JAP', 'USA');
    fell(k, 'FRA', 14, 'GER');
    for (let i = 0; i < 6; i++) turnMonth(k);
    expect(pactAt(k)).toBe(17);
    expect(k.flags['event:pact-binds']).toBeUndefined();
    expect(atWar(k, 'USA', 'GER')).toBe(false);
  });

  it('an American player declares on Japan in March 1940: America alone until December 1941', () => {
    const g = war(1, 7, 'USA');
    declareWar(g, 'USA', 'JAP');
    // The surprise is one rule for every country (b499): Japan, at war with no country of America's side, is caught.
    expect(sneakOn(g, 'USA', 'JAP')).toBe(true);
    // Japan, struck by a country outside both blocs, joins nobody, and nobody joins America.
    expect(bloc(g.nations.JAP)).toBeNull();
    expect(bloc(g.nations.USA)).toBeNull();
    while (g.turn < 28) { turnMonth(g); expect(atWar(g, 'UK', 'JAP'), `month ${g.turn}`).toBe(false); expect(atWar(g, 'USA', 'GER'), `month ${g.turn}`).toBe(false); }
    closeDiplomacy(g);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(news(g, 28, 'Pearl Harbor')).toBe(false);
    g.turn++;
    monthlyDiplomacy(g);
    expect(atWar(g, 'UK', 'JAP')).toBe(true);
  });

  it("takes no country in the player's hands to war by a tie", () => {
    const g = war(1, 28, 'UK');
    declareWar(g, 'JAP', 'USA');
    closeDiplomacy(g);
    expect(bloc(g.nations.USA)).toBe('allies');
    g.turn++;
    monthlyDiplomacy(g);
    expect(atWar(g, 'UK', 'JAP')).toBe(false);
  });
});

describe('the offer of war (ruling 197): the freeze, and the Soviet Union', () => {
  it('a Japan that strikes nobody: by the book the freeze comes on its date, August 1941, and nobody is at war', () => {
    const g = war(1, 22);
    turnMonth(g);
    expect(freezeAt(g)).toBe(0);
    turnMonth(g);
    expect(g.turn).toBe(24);
    expect(freezeAt(g)).toBe(24);
    expect(news(g, 24, 'America has stopped selling oil and steel to Japan. Britain and the Dutch have done the same.')).toBe(true);
    expect(g.nations.JAP.atWar).toEqual(['CHN']);
    // It does not lift, and is told once.
    turnMonth(g);
    expect(freezeAt(g)).toBe(24);
    expect(g.headlines.filter((h) => h.text.includes('stopped selling oil')).length).toBe(1);
  });

  it("where history is loose no date decides it: the month after Japan's troops hold Saigon or Cam Ranh", () => {
    const g = war(3, 30);
    turnMonth(g);
    expect(freezeAt(g)).toBe(0);
    g.byName['Cam Ranh'].ctrl = 'JAP';
    expect(freezeAt(g)).toBe(0);
    turnMonth(g);
    expect(freezeAt(g)).toBe(32);
    // By the book the date decides, and holding Saigon sooner does not bring it sooner.
    const h = war(2, 15);
    h.byName.Saigon.ctrl = 'JAP';
    turnMonth(h);
    expect(freezeAt(h)).toBe(0);
  });

  it('the two computers keep the peace while Germany stands; three months after Germany is beaten the Soviet staff goes to war', () => {
    const g = war(3, 60);
    setWar(g, 'JAP', 'USA');
    turnMonth(g);
    expect(atWar(g, 'SOV', 'JAP')).toBe(false);
    fell(g, 'GER', 61, 'SOV');
    turnMonth(g);
    turnMonth(g);
    expect(g.turn).toBe(63);
    expect(atWar(g, 'SOV', 'JAP')).toBe(false);
    turnMonth(g);
    expect(g.turn).toBe(64);
    expect(atWar(g, 'SOV', 'JAP')).toBe(true);
    expect(g.flags['war:SOV:JAP']).toBe(64);
    // Mongolia and Manchukuo are in it the same month, by their ties.
    expect(atWar(g, 'MGL', 'JAP')).toBe(true);
    expect(atWar(g, 'MAN', 'SOV')).toBe(true);
  });

  it('by the book the Soviet staff does not turn east before August 1945, nor against a Japan at peace with America and Britain, nor for a player', () => {
    const g = war(1, 66);
    setWar(g, 'JAP', 'USA');
    fell(g, 'GER', 60, 'SOV');
    while (g.turn < 71) { turnMonth(g); expect(atWar(g, 'SOV', 'JAP'), `month ${g.turn}`).toBe(false); }
    turnMonth(g);
    expect(g.turn).toBe(72);
    expect(atWar(g, 'SOV', 'JAP')).toBe(true);
    const quiet = war(3, 66);
    fell(quiet, 'GER', 60, 'SOV');
    for (let i = 0; i < 4; i++) turnMonth(quiet);
    expect(atWar(quiet, 'SOV', 'JAP')).toBe(false);
    const mine = war(3, 66, 'SOV');
    setWar(mine, 'JAP', 'USA');
    fell(mine, 'GER', 60, 'SOV');
    for (let i = 0; i < 4; i++) turnMonth(mine);
    expect(atWar(mine, 'SOV', 'JAP')).toBe(false);
  });

  it('reads the ties again harmlessly: a second reading in a month changes nothing', () => {
    const g = war(1, 28);
    declareWar(g, 'JAP', 'USA');
    declareWar(g, 'JAP', 'UK');
    closeDiplomacy(g);
    const before = JSON.stringify([g.nations.JAP.align, g.nations.CHN.align, g.nations.USA.align, g.headlines.length]);
    pacificSides(g);
    pacificOpening(g);
    pacificSides(g);
    expect(JSON.stringify([g.nations.JAP.align, g.nations.CHN.align, g.nations.USA.align, g.headlines.length])).toBe(before);
  });
});
