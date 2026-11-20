// Ruling 228, section 3 (the historian's realism audit, item 11): the Middle East of 1941. At stops 1 and 2 Rashid
// Ali takes Iraq into war with Britain in April and signs an armistice at the end of May, so that Britain holds Iraq
// by June in every history war; in August Britain and the Soviet Union enter Iran together. At stops 3 to 5 Iraq
// stays as it is, and the two staffs enter Iran once both are at war with Germany and Iran is not with the Axis.
// Once Iran is entered the aid to the Soviet Union comes by two roads, the Persian Corridor and the Arctic, split
// evenly, the two together no more than the Arctic carried alone. None of it is in the history campaigns.
import { describe, expect, it } from 'vitest';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, createGame, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { laneStopped, lanesTo } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

function war(stop: number, human: string | null = null): Game {
  const g = startOpen(createGame(3, human));
  setStop(g, stop, true);
  setWar(g, 'GER', 'UK');
  return g;
}
function at(g: Game, turn: number): void {
  g.turn = turn;
  monthlyDiplomacy(g);
}
/** The Soviet Union at war with Germany beside the western Allies, as from June 1941. */
function barbarossa(g: Game): void {
  g.nations.SOV.align = 'allies';
  setWar(g, 'GER', 'SOV');
}
const lane = (g: Game, id: string) => lanesTo(g, 'SOV').find((l) => l.id === id)!;

describe('Rashid Ali (stops 1 and 2)', () => {
  it('takes Iraq into war with Britain in April 1941, told as one story', () => {
    const g = war(1);
    at(g, 19);
    expect(atWar(g, 'IRQ', 'UK')).toBe(false);
    expect(laneStopped(g, 'gulf')).toBeNull();
    at(g, 20);
    expect(atWar(g, 'IRQ', 'UK')).toBe(true);
    // On its own account, in no alliance: Germany's fronts are not Iraq's.
    expect(g.nations.IRQ.align).toBe('independent');
    const said = g.headlines.filter((h) => h.turn === 20 && /Iraq/.test(h.text));
    expect(said).toHaveLength(1);
    expect(said[0].kind).toBe('war');
    expect(said[0].nat).toBe('IRQ');
    expect(said[0].text).toMatch(/^Rashid Ali takes Iraq into war with Britain\. /);
    expect(said[0].text).toMatch(/no oil comes by the Gulf route\.$/);
    // The Gulf route's own rule shuts it, in the UI director's words.
    expect(laneStopped(g, 'gulf')).toEqual({ state: 'cut', why: 'Iraq is at war with the United Kingdom.' });
  });

  it('ends in an armistice: Britain holds Iraq by June 1941', () => {
    const g = war(2);
    for (const t of [20, 21]) at(g, t);
    expect(g.nations.IRQ.alive).toBe(true);
    at(g, 22);
    expect(g.nations.IRQ.alive).toBe(false);
    expect(g.nations.IRQ.fellOn?.how).toBe('terms');
    for (const n of ['Iraq', 'Baghdad', 'Rutbah']) expect(g.byName[n].ctrl, n).toBe('UK');
    expect(g.corps.some((c) => c.nat === 'IRQ' && !c.dead)).toBe(false);
    const h = g.headlines.find((x) => /^Iraq has signed an armistice at Baghdad\./.test(x.text))!;
    // Dated to the last week of May, the month it was signed in.
    expect([h.turn, h.week]).toEqual([21, 4]);
    expect(laneStopped(g, 'gulf')).toBeNull();
  });

  it('does not come at stops 3 to 5, where Iraq stays as it is', () => {
    for (const stop of [3, 4, 5]) {
      const g = war(stop);
      for (const t of [20, 21, 22]) at(g, t);
      expect(atWar(g, 'IRQ', 'UK'), `stop ${stop}`).toBe(false);
      expect(g.nations.IRQ.alive, `stop ${stop}`).toBe(true);
    }
  });

  it('leaves a player\'s own Iraq to its player', () => {
    const g = war(1, 'IRQ');
    for (const t of [20, 21, 22]) at(g, t);
    expect(atWar(g, 'IRQ', 'UK')).toBe(false);
    expect(g.nations.IRQ.alive).toBe(true);
  });
});

describe('Iran and the Persian Corridor', () => {
  it('stops 1 and 2: Britain and the Soviet Union enter Iran together in August 1941', () => {
    const g = war(1);
    barbarossa(g);
    at(g, 23);
    expect(g.nations.IRN.alive).toBe(true);
    expect(lane(g, 'persia')).toMatchObject({ state: 'idle', why: 'It opens once British and Soviet troops hold Iran.', oil: 0, sup: 0 });
    const alone = lane(g, 'arctic').full;
    at(g, 24);
    expect(g.nations.IRN.alive).toBe(false);
    expect(g.byName['Iran'].ctrl).toBe('SOV');
    expect(g.byName['Kermanshah'].ctrl).toBe('UK');
    expect(g.corps.some((c) => c.nat === 'IRN' && !c.dead)).toBe(false);
    const h = g.headlines.find((x) => /^British and Soviet troops have entered Iran\./.test(x.text))!;
    expect(h.head).toBe('British and Soviet troops have entered Iran');
    expect(h.nat).toBe('SOV');
    // The road is open, and the aid is split evenly between the two roads: together what the Arctic carried alone.
    const persia = lane(g, 'persia'), arctic = lane(g, 'arctic');
    expect(persia.state).toBe('open');
    expect(persia.full.oil).toBeCloseTo(arctic.full.oil, 9);
    expect(persia.full.sup).toBeCloseTo(arctic.full.sup, 9);
    expect((persia.full.oil + arctic.full.oil) / (persia.full.sup + arctic.full.sup)).toBeCloseTo(alone.oil / alone.sup, 9);
  });

  it('an enemy who holds the road shuts it, and the Arctic carries the whole again', () => {
    const g = war(1);
    barbarossa(g);
    at(g, 24);
    g.byName['Iran'].ctrl = 'GER';
    expect(lane(g, 'persia')).toMatchObject({ state: 'cut', why: 'Germany holds Iran and the road through it is shut.' });
    const arctic = lane(g, 'arctic');
    g.byName['Iran'].ctrl = 'SOV';
    expect(arctic.full.oil).toBeCloseTo(2 * lane(g, 'arctic').full.oil, 9);
  });

  it('is not entered before August 1941 at stops 1 and 2', () => {
    const g = war(2);
    barbarossa(g);
    for (const t of [20, 21, 22, 23]) at(g, t);
    expect(g.nations.IRN.alive).toBe(true);
  });

  it('stops 3 to 5: the staffs go in once both are at war with Germany', () => {
    const g = war(3);
    at(g, 10);
    expect(g.nations.IRN.alive).toBe(true);
    barbarossa(g);
    at(g, 11);
    expect(g.nations.IRN.alive).toBe(false);
    expect(g.byName['Iran'].ctrl).toBe('SOV');
    expect(g.byName['Kermanshah'].ctrl).toBe('UK');
  });

  it('stops 3 to 5: not an Iran with the Axis, and not by a player\'s own Britain', () => {
    const g = war(4);
    g.nations.IRN.align = 'axis_cobelligerent';
    barbarossa(g);
    at(g, 24);
    expect(g.nations.IRN.alive).toBe(true);
    const p = war(3, 'UK');
    barbarossa(p);
    at(p, 24);
    expect(p.nations.IRN.alive).toBe(true);
  });
});

describe('the history campaigns', () => {
  it('have none of it', () => {
    const g = createGame(3, null);
    setWar(g, 'GER', 'UK');
    barbarossa(g);
    for (const t of [20, 21, 22, 24]) at(g, t);
    expect(atWar(g, 'IRQ', 'UK')).toBe(false);
    expect(g.nations.IRQ.alive).toBe(true);
    expect(g.nations.IRN.alive).toBe(true);
  });
});

describe('history\'s calendar', () => {
  it('Rashid Ali\'s moment passes with May 1941: a war that reaches 1943 another way sees no coup', () => {
    const g = war(1);
    at(g, 30);
    expect(atWar(g, 'IRQ', 'UK')).toBe(false);
    expect(g.flags['event:rashid-ali']).toBeUndefined();
  });
});
