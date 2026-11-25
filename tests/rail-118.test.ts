// Version 82.3, the design director's ruling 118 and the art director's railway brief (sections 2a, 3, 8 and 11, as
// rewritten on 16 November at 15:23 and 15:45 UTC). Display only: no rule of the railways moves.
//   - The Rail move switch is gone, and where it stood one line says what the trains will do for the army in hand
//     this month: that it can ride, that it can but no farther than it marches (and where the line is broken), or
//     the first reason it cannot.
//   - A corps that has not moved and is slowed for want of fuel is not told it has spent part of its month.
//   - A break he can act on (a cut, a siege, bridges down) is marked, so both builds print it before the kept
//     march's line with its opening in the box's emphasis; every other reason stays an aside.
//   - Under the pointer on ground an army can ride to there is the one line built in 82.2, and another for
//     engineers at a cut they can reach.
//   - The line on the panel counts the places a train reaches beyond the march, and the railway the army could ride
//     is lit: every working track from its own province outward.
import { describe, expect, it } from 'vitest';
import { turnToDate } from '../src/core/ticket';
import { addCorps, allied, createGame, litTracks, relocate, setWar, nearestBreak, railReach, railReachFor, railSays, rideLines, roomForAll, whyNoTrain, whyNoTrainFor } from '../src/campaign/game';
import { cutRail, mendRail, onNet, railsAt } from '../src/campaign/rails';
import { besieged, garrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { KIND, type Corps, type Game, type Kind, type Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const date = (t: number): string => turnToDate(t).label;
const BLUE = { one: 'the blue one', many: 'a blue one' };
const corps = (g: Game, nat: string, kind: Kind, at: Region, divs = 3): Corps => addCorps(g, nat, `Test ${KIND[kind].label} Corps ${g.nextCorps}`, kind, divs, at.id, {});
function rider(g: Game): { c: Corps; reach: Set<number>; far: number[] } {
  for (const c of g.corps) {
    if (c.nat !== 'GER' || c.dead || c.kind === 'fort' || c.town || c.ap < 4) continue;
    const reach = railReach(g, c);
    const far = [...reach].filter((id) => !g.regions[c.reg].nb.includes(id) && g.regions[id].ctrl === 'GER');
    if (far.length > 3) return { c, reach, far };
  }
  throw new Error('no corps can ride');
}

describe('the month spent, and the corps that never had a whole one (ruling 118(c))', () => {
  it('a corps that has moved has spent part of its month', () => {
    const g = grand();
    const { c } = rider(g);
    c.ap = c.apMax - 1;
    expect(whyNoTrain(g, c)).toEqual({ code: 'spent', line: `No train for ${c.name} this month: it has spent part of its month, and the railway takes a whole one.` });
  });

  it('a corps slowed for want of fuel has not moved, and is told it is short of fuel', () => {
    const g = grand();
    const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead && !c.town)!;
    g.nations.GER.res!.oilMet = 0.3;
    tank.ap = tank.apMax = 3;
    expect(whyNoTrain(g, tank)).toEqual({ code: 'spent', line: `No train for ${tank.name} this month: short of fuel, it has less than the whole month a train takes.` });
    // Short of a whole month for another cause: one line true of it.
    g.nations.GER.res!.oilMet = 1;
    expect(whyNoTrain(g, tank)!.line).toBe(`No train for ${tank.name} this month: the railway takes a whole month of movement, and it has less.`);
    // A fast corps with five weeks may march one and still ride.
    tank.apMax = 5; tank.ap = 4;
    expect(whyNoTrain(g, tank)?.code).not.toBe('spent');
  });
});

describe('a break he can act on is marked, and an aside is not (the art director, section 8)', () => {
  it('a cut at his own place, at the journey\'s end and on the way each carry the opening that takes the emphasis', () => {
    const g = grand();
    const { c, far } = rider(g);
    const here = g.regions[c.reg];
    cutRail(g, here.id, 'bombed');
    const own = whyNoTrain(g, c)!;
    expect(own.strong).toBe(`The railway at ${here.name} is cut (bombed, ${date(g.turn)}).`);
    expect(own.line).toBe(`${own.strong} No train leaves until engineers relay the track.`);
    mendRail(g, here.id);
    const to = far[0];
    cutRail(g, to, 'saboteurs');
    const end = whyNoTrain(g, c, to)!;
    expect(end).toMatchObject({ code: 'cut', at: to, strong: `The railway at ${g.regions[to].name} is cut (blown by saboteurs, ${date(g.turn)}).` });
    expect(end.line.startsWith(`${end.strong} No train runs into it until`)).toBe(true);
    mendRail(g, to);
    let seen = 0;
    for (const dest of far) {
      for (const mid of railReach(g, c)) {
        if (mid === dest || g.regions[mid].ctrl !== 'GER') continue;
        cutRail(g, mid, 'agent');
        if (!railReach(g, c).has(dest)) {
          const why = whyNoTrain(g, c, dest)!;
          expect(why.strong).toBe(`The line to ${g.regions[dest].name} is broken at ${g.regions[mid].name}:`);
          expect(why.line).toBe(`${why.strong} the railway there is cut (blown by an agent, ${date(g.turn)}).`);
          seen++;
        }
        mendRail(g, mid);
        if (seen) break;
      }
      if (seen) break;
    }
    expect(seen).toBe(1);
  });

  it('the month spent, a fortress, ground not in friendly hands and no room are asides', () => {
    const g = grand();
    const { c, far } = rider(g);
    const foe = g.regions.find((r) => r.ctrl === 'POL' && onNet(r.id))!;
    expect(whyNoTrain(g, c, foe.id)).toMatchObject({ code: 'foe' });
    expect(whyNoTrain(g, c, foe.id)!.strong).toBeUndefined();
    const fort = g.corps.find((x) => x.kind === 'fort' && !x.dead)!;
    expect(whyNoTrain(g, fort)!.strong).toBeUndefined();
    c.ap = 2;
    expect(whyNoTrain(g, c, far[0])!.strong).toBeUndefined();
    // An army with one corps that cannot ride: the sentence about the army leads, and nothing in it is emphasised.
    c.ap = 4;
    const mate = corps(g, 'GER', 'inf', g.regions[c.reg]);
    mate.ap = 1;
    const no = whyNoTrainFor(g, [c, mate], far[0])!;
    expect(no.line.startsWith(`${mate.name} cannot go by rail, so nobody rides. No train for ${mate.name} this month:`)).toBe(true);
    expect(no.strong).toBeUndefined();
  });

  it('a garrison: shut in where the town is besieged, and told to take the field first where it is not (the 3D builder, 18 November)', () => {
    const g = grand();
    setWar(g, 'GER', 'POL');
    const town = g.byName['Lodz'];
    for (const u of [...town.units]) relocate(g, u, g.byName['Warsaw']);
    const c = corps(g, 'POL', 'inf', town);
    expect(garrison(g, c)).toBe(true);
    expect(besieged(g, town)).toBe(false);
    expect(whyNoTrain(g, c)).toEqual({ code: 'town', line: 'A garrison does not take the train: TAKE THE FIELD first.', at: town.id });
    relocate(g, corps(g, 'GER', 'inf', g.byName['Upper Silesia']), town);
    expect(besieged(g, town)).toBe(true);
    expect(whyNoTrain(g, c)!.line).toBe(`${c.name} is shut in the town at Lodz. No train leaves a siege.`);
  });
});

describe('the line on the army\'s panel where the switch stood (ruling 118(b); section 2a)', () => {
  it('says it can ride, in the build\'s own press and sign', () => {
    const g = grand();
    const { c } = rider(g);
    // (It counts the places beyond its march, so he looks for them: the Playtester found Berlin's three under the panel.)
    expect(railSays(g, [c], 3, 'right-click', BLUE)).toEqual({ can: true, strong: 'By rail this month to 3 more places:', line: 'By rail this month to 3 more places: right-click a blue one. It takes the whole month.' });
    expect(railSays(g, [c], 1, 'press and hold', BLUE)).toEqual({ can: true, strong: 'By rail this month to 1 more place:', line: 'By rail this month to 1 more place: press and hold the blue one. It takes the whole month.' });
    expect(railSays(g, [c], 12, 'right-click', { one: 'the station dot', many: 'a station dot' })!.line).toBe('By rail this month to 12 more places: right-click a station dot. It takes the whole month.');
  });

  it('names the places where a train reaches three or fewer beyond the march, the nearest by rail first (Version 82.4, ruling 123)', () => {
    const g = grand();
    const { c, far } = rider(g);
    expect(far.length).toBeGreaterThanOrEqual(4);
    const [a, b, d] = far.map((id) => g.regions[id].name);
    expect(railSays(g, [c], far.slice(0, 3), 'right-click', BLUE)).toEqual({ can: true, strong: `By rail this month to ${a}, ${b} and ${d}:`, line: `By rail this month to ${a}, ${b} and ${d}: right-click a blue one. It takes the whole month.` });
    expect(railSays(g, [c], far.slice(0, 2), 'right-click', BLUE)).toEqual({ can: true, strong: `By rail this month to ${a} and ${b}:`, line: `By rail this month to ${a} and ${b}: right-click a blue one. It takes the whole month.` });
    expect(railSays(g, [c], far.slice(0, 1), 'right-click', BLUE)).toEqual({ can: true, strong: `By rail this month to ${a}:`, line: `By rail this month to ${a}: right-click it. It takes the whole month.` });
    expect(railSays(g, [c], far.slice(0, 1), 'press and hold', BLUE)!.line).toBe(`By rail this month to ${a}: press and hold it. It takes the whole month.`);
    // Over three it counts them as before, and with none it says the train reaches no farther than the march.
    expect(railSays(g, [c], far.slice(0, 4), 'right-click', BLUE)!.line).toBe('By rail this month to 4 more places: right-click a blue one. It takes the whole month.');
    expect(railSays(g, [c], [], 'right-click', BLUE)!.line).toBe('By rail this month, but a train reaches no farther than it can march.');
    // The places as railReachFor hands them are in the order the train comes to them.
    expect([...railReachFor(g, [c])].filter((id) => far.includes(id))).toEqual(far.filter((id) => railReachFor(g, [c]).has(id)));
  });

  it('says a train reaches no farther than the march, and where the line is broken if a break is why', () => {
    const g = grand();
    const { c } = rider(g);
    expect(railSays(g, [c], 0, 'right-click', BLUE)).toEqual({ can: true, strong: '', line: 'By rail this month, but a train reaches no farther than it can march.' });
    // Every line out of its province cut at the next one: the nearest break is named, in whyNoTrain's own words.
    const next = railsAt(c.reg).filter((j) => g.regions[j].ctrl === 'GER');
    expect(next.length).toBeGreaterThan(0);
    for (const j of next) cutRail(g, j, 'bombed');
    const brk = nearestBreak(g, [c])!;
    expect(next).toContain(brk.at);
    expect(brk.line).toBe(`The railway at ${g.regions[brk.at!].name} is cut (bombed, ${date(g.turn)}). No train runs into it until engineers relay the track.`);
    expect(railSays(g, [c], 0, 'right-click', BLUE)!.line).toBe(`By rail this month, but a train reaches no farther than it can march. ${brk.line}`);
  });

  it('says the first reason it cannot, and nothing where nothing in hand could ride', () => {
    const g = grand();
    const { c } = rider(g);
    c.ap = 1;
    expect(railSays(g, [c], 0, 'right-click', BLUE)).toEqual({ can: false, strong: '', line: `No train for ${c.name} this month: it has spent part of its month, and the railway takes a whole one.` });
    expect(railSays(g, [], 0, 'right-click', BLUE)).toBeNull();
  });
});

describe('under the pointer on ground it can ride to (section 8)', () => {
  it('the one line of Version 82.2, which the build sets in the rail\'s own blue', () => {
    const g = grand();
    const { c, far } = rider(g);
    const to = far[0];
    expect(rideLines(g, [c], to, 'Right-click')).toEqual({ line: `Right-click: by rail to ${g.regions[to].name}, there this month. It takes the whole month.`, relay: '' });
    const mate = corps(g, 'GER', 'inf', g.regions[c.reg]);
    expect(rideLines(g, [c, mate], to, 'Right-click').line).toBe(`Right-click: by rail to ${g.regions[to].name}, there this month. It takes the whole month.`);
  });

  it('engineers pointed at a cut they can reach are told they can relay the track there, and nobody else is', () => {
    const g = grand();
    const { c, far } = rider(g);
    const eng = corps(g, 'GER', 'eng', g.regions[c.reg]);
    // (A province takes two engineer corps and no more: one with room for this one.)
    const to = far.find((id) => roomForAll(g, g.regions[id], [eng]))!;
    cutRail(g, to, 'bombed');
    // The works train carries engineers to the break itself, and nobody else.
    expect(railReach(g, eng).has(to)).toBe(true);
    expect(railReach(g, c).has(to)).toBe(false);
    expect(railReachFor(g, [eng]).has(to)).toBe(true);
    expect(railReachFor(g, [eng, c]).has(to)).toBe(false);
    expect(rideLines(g, [eng], to, 'Right-click').relay).toBe('Engineers can relay the track here.');
    // (Version 82.4, the art director's mend: it is the works train that takes them, so the line does not gainsay "no train runs through".)
    expect(rideLines(g, [eng], to, 'Right-click').line).toBe(`Right-click: by works train to ${g.regions[to].name}, there this month. It takes the whole month.`);
    mendRail(g, to);
    expect(rideLines(g, [eng], to, 'Right-click')).toEqual({ line: `Right-click: by rail to ${g.regions[to].name}, there this month. It takes the whole month.`, relay: '' });
  });
});

describe('the lit railway: every working track the army in hand could ride (sections 3 and 11)', () => {
  const key = (a: number, b: number): string => `${Math.min(a, b)}-${Math.max(a, b)}`;
  const keys = (g: Game, cs: Corps[]): Set<string> => new Set(litTracks(g, cs).map(([a, b]) => key(a, b)));

  it('runs from its own province to every place it can ride to, each track once and the lower province first', () => {
    const g = grand();
    const { c, reach } = rider(g);
    const lit = litTracks(g, [c]);
    expect(lit.length).toBeGreaterThan(3);
    expect(new Set(lit.map(([a, b]) => key(a, b))).size).toBe(lit.length);
    for (const [a, b] of lit) {
      expect(a).toBeLessThan(b);
      expect(railsAt(a)).toContain(b);
      // Both ends are ground its train runs on: its own province, a place the walk reaches, or friendly ground it only
      // runs through (a high pass where tanks are not unloaded: since ruling 233's Brenner line, Italy's Alto Adige too).
      for (const id of [a, b]) expect(id === c.reg || reach.has(id) || allied(g, 'GER', g.regions[id].ctrl)).toBe(true);
    }
    // Every place it can ride to has a lit track into it, and so has its own province.
    const ends = new Set(lit.flat());
    for (const id of reach) expect(ends.has(id)).toBe(true);
    expect(ends.has(c.reg)).toBe(true);
  });

  it('stops at a cut, and runs up to it for engineers alone', () => {
    const g = grand();
    const { c, far } = rider(g);
    const eng = corps(g, 'GER', 'eng', g.regions[c.reg]);
    const to = far.find((id) => roomForAll(g, g.regions[id], [eng]))!;
    const into = railsAt(to).map((j) => key(j, to));
    expect(into.some((k) => keys(g, [c]).has(k))).toBe(true);
    cutRail(g, to, 'bombed');
    expect(into.some((k) => keys(g, [c]).has(k))).toBe(false);
    expect(into.some((k) => keys(g, [eng]).has(k))).toBe(true);
    // Nothing is lit beyond the break by way of it, for anyone: no lit track has the cut province as its near end.
    expect(litTracks(g, [eng]).every(([a, b]) => (a !== to && b !== to) || railReach(g, eng).has(to))).toBe(true);
    // An army rides together: with a corps that cannot be carried to the break, the light stops short of it.
    expect(into.some((k) => keys(g, [eng, c]).has(k))).toBe(false);
  });

  it('is dark for an army that cannot ride, and where the trains keep to no lines on the map', () => {
    const g = grand();
    const { c } = rider(g);
    const mate = corps(g, 'GER', 'inf', g.regions[c.reg]);
    mate.ap = 1;
    expect(litTracks(g, [c, mate])).toEqual([]);
    c.ap = 1;
    expect(litTracks(g, [c])).toEqual([]);
    expect(litTracks(g, [])).toEqual([]);
    const plain = createGame(3, 'GER');
    const p = plain.corps.find((x) => x.nat === 'GER' && !x.dead && x.kind !== 'fort' && !x.town)!;
    expect(litTracks(plain, [p])).toEqual([]);
  });
});
