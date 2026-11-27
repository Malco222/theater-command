// Version 82.2, the player's ask of 16 November: "troops should be able to move around farther when trains and rail
// roads are working. if rail roads get bombed or destroyed by sabotage than the troops cant move as far". All of that
// was the rule since Version 35 and hid behind a small button, so this is display (the design director's rulings 110
// and 112 to 116) and no rule of the railways moves:
//   - what cut a line is kept beside the cut and said: bombed, blown by an agent, blown by saboteurs, torn up in a
//     retreat, burnt in a hasty retreat, cut by order; a cut made before the cause was kept says the month alone;
//   - one function says why no train will take a corps, and where the line is broken, for both builds;
//   - an army in hand rides together or not at all, and the sentence names the corps that cannot;
//   - the fog lies on a cut line: the map shows what the player last knew of an enemy line, with its month;
//   - NEXT ARMY and the question before END THE MONTH count the same armies in the same words.
import { describe, expect, it } from 'vitest';
import { turnToDate } from '../src/core/ticket';
import { addCorps, allied, createGame, hasRoom, railMove, railReach, roomForAll, whyNoTrain, whyNoTrainFor } from '../src/campaign/game';
import { SIGHT, cutKnown, cutSays, cutsKnown, seeCuts, setFog, sightOf } from '../src/campaign/intel';
import { CAUSE_WORDS, bombRail, cutCause, cutOn, cutRail, cutRailHasty, mendRail, onNet, railCut, railOnCapture, railsAt, tearUp, type RailCause } from '../src/campaign/rails';
import { roundQuestion, roundWords } from '../src/campaign/round';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { KIND, type Corps, type Game, type Kind, type Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const date = (t: number): string => turnToDate(t).label;
const corps = (g: Game, nat: string, kind: Kind, at: Region, divs = 3): Corps => addCorps(g, nat, `Test ${KIND[kind].label} Corps ${g.nextCorps}`, kind, divs, at.id, {});
/** A German corps with its whole month, on the net, that a train can take somewhere two provinces or more away. */
function rider(g: Game): { c: Corps; reach: Set<number>; far: number[] } {
  for (const c of g.corps) {
    if (c.nat !== 'GER' || c.dead || c.kind === 'fort' || c.town || c.ap < 4) continue;
    const reach = railReach(g, c);
    const far = [...reach].filter((id) => !g.regions[c.reg].nb.includes(id) && g.regions[id].ctrl === 'GER');
    if (far.length > 3) return { c, reach, far };
  }
  throw new Error('no corps can ride');
}

describe('what cut a railway is kept beside the cut (ruling 112(c))', () => {
  it('keeps each cause, forgets it when the line is relaid, and moves nothing else', () => {
    const g = grand();
    const berlin = g.byName['Berlin'].id;
    expect(cutCause(g, berlin)).toBeNull();
    for (const cause of Object.keys(CAUSE_WORDS) as RailCause[]) {
      expect(cutRail(g, berlin, cause)).toBe(true);
      expect(railCut(g, berlin)).toBe(true);
      expect(cutOn(g, berlin)).toBe(g.turn);
      expect(cutCause(g, berlin)).toBe(cause);
      expect(cutRail(g, berlin, 'order'), 'a cut line is not cut twice').toBe(false);
      expect(cutCause(g, berlin)).toBe(cause);
      expect(mendRail(g, berlin)).toBe(true);
      expect(cutCause(g, berlin)).toBeNull();
      expect(Object.keys(g.flags).some((k) => k.startsWith('railwhy:'))).toBe(false);
    }
  });

  it('is written by every way a line is cut', () => {
    const g = grand();
    // By order: troops tear it up.
    const mine = g.corps.find((c) => c.nat === 'GER' && !c.dead && onNet(c.reg) && !railCut(g, c.reg))!;
    expect(tearUp(g, 'GER', mine.reg)).toBe(true);
    expect(cutCause(g, mine.reg)).toBe('order');
    // Bombed.
    let bombed = 0;
    for (const r of g.regions) if (r.ctrl === 'GER' && onNet(r.id) && !railCut(g, r.id) && bombRail(g, 'UK', r.id, 5000)) { bombed++; expect(cutCause(g, r.id)).toBe('bombed'); }
    expect(bombed).toBeGreaterThan(0);
    // Torn up as the troops fall back.
    let torn = 0;
    for (const r of g.regions) if (r.ctrl === 'POL' && onNet(r.id) && !railCut(g, r.id)) { railOnCapture(g, r, 'GER', 'POL'); if (railCut(g, r.id)) { torn++; expect(cutCause(g, r.id)).toBe('retreat'); } }
    expect(torn).toBeGreaterThan(0);
    // Burnt in a hasty retreat.
    const hasty = g.regions.find((r) => r.ctrl === 'FRA' && onNet(r.id) && !railCut(g, r.id))!;
    expect(cutRailHasty(g, hasty.id, 2)).toBe(true);
    expect(cutCause(g, hasty.id)).toBe('hasty');
    // A save keeps it.
    const back = loadGame(saveGame(g))!;
    expect(cutCause(back, mine.reg)).toBe('order');
    expect(cutCause(back, hasty.id)).toBe('hasty');
  });

  it('says the month alone for a cut made before the cause was kept, never a guessed cause', () => {
    const g = grand();
    const berlin = g.byName['Berlin'].id;
    g.turn += 3;
    g.flags[`rail:${berlin}`] = g.turn - 2;
    expect(cutCause(g, berlin)).toBeNull();
    const said = cutSays(g, berlin)!;
    expect(said.head).toBe('Railway cut');
    expect(said.line.startsWith(`Cut since ${date(g.turn - 2)}. No train runs into, out of or through Berlin until engineers relay the track.`)).toBe(true);
    for (const w of Object.values(CAUSE_WORDS)) expect(said.line.toLowerCase()).not.toContain(w);
    expect(said.short).toBe(`railway cut (${date(g.turn - 2)})`);
  });

  it('says the cause and the month, and for a hasty cut the month the railwaymen will have it running', () => {
    const g = grand();
    const berlin = g.byName['Berlin'].id, saxony = g.byName['Saxony'].id;
    cutRail(g, berlin, 'saboteurs');
    expect(cutSays(g, berlin)).toEqual({
      head: 'Railway cut',
      line: `Blown by saboteurs in ${date(g.turn)}. No train runs into, out of or through Berlin until engineers relay the track. Only engineers can be carried up to the break.`,
      short: `railway cut (blown by saboteurs, ${date(g.turn)})`,
    });
    cutRailHasty(g, saxony, 2);
    expect(cutSays(g, saxony)!.line).toBe(`Burnt in a hasty retreat in ${date(g.turn)}. No train runs into, out of or through Saxony until the railwaymen have it running, in ${date(g.turn + 2)}, or engineers relay the track sooner. Only engineers can be carried up to the break.`);
    expect(cutSays(g, g.byName['Hamburg'].id)).toBeNull();
  });
});

describe('why no train will take a corps (ruling 112(b))', () => {
  it('answers null exactly where the train runs, for every corps and every province', () => {
    const g = grand();
    let yes = 0, no = 0;
    for (const c of g.corps.filter((x) => x.nat === 'GER' && !x.dead).slice(0, 14)) {
      const reach = railReach(g, c);
      for (const r of g.regions) {
        if (r.id === c.reg) continue;
        const why = whyNoTrain(g, c, r.id);
        const can = reach.has(r.id) && hasRoom(g, r, c) && !c.town;
        expect(why === null, `${c.name} to ${r.name}: ${why?.line}`).toBe(can);
        if (why) { no++; expect(why.line.length).toBeGreaterThan(12); expect(why.line.endsWith('.')).toBe(true); } else yes++;
      }
    }
    expect(yes).toBeGreaterThan(20);
    expect(no).toBeGreaterThan(20);
    // And where it answers null the train does run.
    const { c, far } = rider(g);
    expect(whyNoTrain(g, c, far[0])).toBeNull();
    expect(railMove(g, c, g.regions[far[0]])).toBe(true);
    expect(c.reg).toBe(far[0]);
    expect(c.ap).toBe(0);
  });

  it('names the reason that belongs to the corps: its month part spent, its own line cut, a fortress', () => {
    const g = grand();
    const { c, far } = rider(g);
    expect(whyNoTrain(g, c)).toBeNull();
    c.ap = 3;
    expect(whyNoTrain(g, c, far[0])).toMatchObject({ code: 'spent', line: `No train for ${c.name} this month: it has spent part of its month, and the railway takes a whole one.` });
    c.ap = 4;
    cutRail(g, c.reg, 'bombed');
    const cut = whyNoTrain(g, c, far[0])!;
    expect(cut.code).toBe('cut');
    expect(cut.at).toBe(c.reg);
    expect(cut.line).toBe(`The railway at ${g.regions[c.reg].name} is cut (bombed, ${date(g.turn)}). No train leaves until engineers relay the track.`);
    mendRail(g, c.reg);
    const fort = g.corps.find((x) => x.kind === 'fort' && !x.dead)!;
    expect(whyNoTrain(g, fort)!.code).toBe('fort');
  });

  it('names where the line is broken, and what broke it', () => {
    const g = grand();
    const { c, far } = rider(g);
    let found = 0;
    for (const to of far) {
      // A province whose cut takes the destination out of reach.
      for (const mid of railReach(g, c)) {
        if (mid === to || g.regions[mid].ctrl !== 'GER') continue;
        cutRail(g, mid, 'agent');
        if (!railReach(g, c).has(to)) {
          const why = whyNoTrain(g, c, to)!;
          expect(why.code).toBe('broken');
          expect(why.at).toBe(mid);
          expect(why.line).toBe(`The line to ${g.regions[to].name} is broken at ${g.regions[mid].name}: the railway there is cut (blown by an agent, ${date(g.turn)}).`);
          found++;
        }
        mendRail(g, mid);
        if (found) break;
      }
      if (found) break;
    }
    expect(found).toBe(1);
    // Enemy ground is named as enemy ground, cut or whole: nothing here tells him what the fog hides.
    const foe = g.regions.find((r) => r.ctrl === 'POL' && onNet(r.id))!;
    const whole = whyNoTrain(g, c, foe.id)!;
    expect(whole.code).toBe('foe');
    cutRail(g, foe.id, 'retreat');
    expect(whyNoTrain(g, c, foe.id)).toEqual(whole);
  });

  it('keeps an army together: if one corps cannot ride nobody does, and the sentence names it (ruling 116)', () => {
    const g = grand();
    const { c, far } = rider(g);
    const here = g.regions[c.reg];
    const mate = here.units.find((u) => u !== c && u.nat === 'GER' && !u.dead && u.kind !== 'fort') ?? corps(g, 'GER', 'inf', here);
    mate.ap = 4;
    const to = far.find((id) => roomForAll(g, g.regions[id], [c, mate]))!;
    expect(whyNoTrainFor(g, [c, mate], to)).toBeNull();
    mate.ap = 2;
    const no = whyNoTrainFor(g, [c, mate], to)!;
    expect(no.code).toBe('spent');
    expect(no.line.startsWith(`${mate.name} cannot go by rail, so nobody rides. `)).toBe(true);
    expect(no.line).not.toContain(c.name);
    // Where none can, it is said once and names nobody in particular.
    c.ap = 2;
    expect(whyNoTrainFor(g, [c, mate], to)!.line.startsWith('No train for')).toBe(true);
    c.ap = mate.ap = 4;
    // No room for them all: nobody rides.
    const full = g.regions[to];
    while (roomForAll(g, full, [c, mate])) corps(g, 'GER', 'inf', full);
    expect(whyNoTrainFor(g, [c, mate], to)).toMatchObject({ code: 'room', at: to });
  });
});

describe('the fog lies on a cut line (rulings 113 and 114)', () => {
  /** An enemy province with a railway that Germany has no sight of. */
  const unseen = (g: Game): Region => { const s = sightOf(g); return g.regions.find((r) => s[r.id] === SIGHT.none && onNet(r.id) && railsAt(r.id).length > 0 && !allied(g, r.ctrl, 'GER'))!; };

  it('hides an enemy cut he cannot see, and shows every cut to a computer, to nobody at the table and with the fog lifted', () => {
    const g = grand();
    const far = unseen(g);
    expect(cutRail(g, far.id, 'retreat')).toBe(true);
    expect(cutKnown(g, far.id)).toBeNull();
    expect(cutSays(g, far.id)).toBeNull();
    expect(cutsKnown(g).has(far.id)).toBe(false);
    expect(cutsKnown(g, null).get(far.id)!.how).toBe('now');
    expect(cutsKnown(g, 'SOV').get(far.id)!.how).toBe('now');
    setFog(g, false);
    expect(cutKnown(g, far.id)).toMatchObject({ how: 'now', cause: 'retreat', since: g.turn });
    setFog(g, true);
    expect(cutKnown(g, far.id)).toBeNull();
    // His own ground always shows.
    const berlin = g.byName['Berlin'].id;
    cutRail(g, berlin, 'bombed');
    expect(cutKnown(g, berlin)).toMatchObject({ how: 'now', cause: 'bombed' });
  });

  it('keeps the report of a cut his own raid or agent made, with its month, until he sees the place again', () => {
    const g = grand();
    const far = unseen(g);
    const t0 = g.turn;
    expect(cutRail(g, far.id, 'bombed', 'GER')).toBe(true);
    expect(cutKnown(g, far.id)).toEqual({ how: 'report', asOf: t0, cause: 'bombed', hasty: 0 });
    expect(cutSays(g, far.id)).toEqual({ head: 'Railway reported cut', line: `Your bombers cut it in ${date(t0)}. The line has not been seen since.`, short: `railway reported cut (${date(t0)})` });
    // Months pass, and the line is relaid out of his sight: his map goes on showing what he last knew.
    g.turn += 2;
    seeCuts(g);
    mendRail(g, far.id);
    expect(cutKnown(g, far.id)).toEqual({ how: 'report', asOf: t0, cause: 'bombed', hasty: 0 });
    // He photographs the province: the truth replaces the report.
    (g.intel.scout ??= {})[far.id] = g.turn;
    expect(cutKnown(g, far.id)).toBeNull();
    seeCuts(g);
    delete g.intel.scout![far.id];
    expect(cutKnown(g, far.id)).toBeNull();
    expect(g.intel.cuts?.[far.id]).toBeUndefined();
    // Another country's raid is not his report.
    cutRail(g, far.id, 'bombed', 'ITA');
    expect(cutKnown(g, far.id)).toBeNull();
  });

  it('goes on showing a cut he saw, as last seen in that month, when he loses sight of it (ruling 114)', () => {
    const g = grand();
    const far = unseen(g);
    cutRail(g, far.id, 'agent');
    (g.intel.scout ??= {})[far.id] = g.turn;
    expect(cutKnown(g, far.id)).toMatchObject({ how: 'now', cause: 'agent' });
    seeCuts(g);
    const t0 = g.turn;
    g.turn++;
    expect(cutKnown(g, far.id)).toEqual({ how: 'seen', asOf: t0, cause: 'agent', hasty: 0 });
    expect(cutSays(g, far.id)).toEqual({ head: 'Railway cut when last seen', line: `Last seen cut in ${date(t0)} (blown by an agent). What has been done to the line since is not known.`, short: `railway cut when last seen (${date(t0)})` });
    // Looking again at what he cannot see changes nothing he knows.
    seeCuts(g);
    expect(cutKnown(g, far.id)).toEqual({ how: 'seen', asOf: t0, cause: 'agent', hasty: 0 });
  });

  it('is a memory for the map: it writes nothing but the record, and nothing at all in a war nobody plays', () => {
    const g = grand();
    cutRail(g, g.byName['Berlin'].id, 'bombed');
    const flags = JSON.stringify(g.flags), army = JSON.stringify(g.corps.map((c) => [c.id, c.reg, c.ap, c.str, c.org]));
    seeCuts(g);
    expect(JSON.stringify(g.flags)).toBe(flags);
    expect(JSON.stringify(g.corps.map((c) => [c.id, c.reg, c.ap, c.str, c.org]))).toBe(army);
    expect(g.intel.cuts?.[g.byName['Berlin'].id]).toEqual({ t: g.turn, how: 'seen', why: 'bombed' });
    const back = loadGame(saveGame(g))!;
    expect(back.intel.cuts).toEqual(g.intel.cuts);
    const w = grand(5, null);
    const x = w.regions.find((r) => onNet(r.id))!;
    cutRail(w, x.id, 'bombed', 'GER');
    seeCuts(w);
    expect(w.intel.cuts).toBeUndefined();
    expect(cutsKnown(w).get(x.id)!.how).toBe('now');
  });
});

describe('NEXT ARMY and the question before END THE MONTH say the same thing (rulings 111(f) and 116)', () => {
  it('names the two kinds, in words true of every army counted', () => {
    expect(roundWords({ bright: 11, quiet: 4, front: 10 })).toBe('10 at the front and 1 newly raised');
    expect(roundWords({ bright: 3, quiet: 0, front: 3 })).toBe('3 at the front');
    expect(roundWords({ bright: 2, quiet: 0, front: 0 })).toBe('2 newly raised');
    expect(roundWords({ bright: 0, quiet: 5, front: 0 })).toBe('');
    expect(roundQuestion({ bright: 11, quiet: 4, front: 10 })).toBe('11 armies still have moves left: 10 at the front and 1 newly raised');
    expect(roundQuestion({ bright: 3, quiet: 0, front: 3 })).toBe('3 armies at the front still have moves left');
    expect(roundQuestion({ bright: 1, quiet: 0, front: 1 })).toBe('1 army at the front still has moves left');
    expect(roundQuestion({ bright: 1, quiet: 0, front: 0 })).toBe('1 army newly raised still has moves left');
  });
});
