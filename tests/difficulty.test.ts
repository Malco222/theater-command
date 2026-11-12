// Campaign difficulty in the grand campaign: five levels named for ranks, Cadet to Field Marshal. The middle one,
// Colonel, is the game with nothing added. A level never touches the battle arithmetic: it changes what each side
// can afford and how the rival staffs think, and on the two hardest levels the rivals bomb the player's factories,
// land on his coasts and time their offensives together. The historical scenarios use none of it.
import { describe, expect, it } from 'vitest';
import { runAI } from '../src/campaign/ai';
import { airAI, airFleetsOf } from '../src/campaign/air';
import { DIFFICULTY, FAIR, asIf, bombingFor, concertFor, draftsMul, incomeMul, landingsFor, levelIndex, levelOf, nerveOf, rival, setLevel, whyNotLevel } from '../src/campaign/difficulty';
import { income, ledger } from '../src/campaign/economy';
import { addCorps, assess, createGame, startPhase } from '../src/campaign/game';
import { landingTargets } from '../src/campaign/navy';
import { railsAt } from '../src/campaign/rails';
import { loadGame, peekSave, saveGame } from '../src/campaign/save';
import { inConcert, seaBase, startOpen, temper } from '../src/campaign/staff';
import { levelNow, levels, orderLevel, runTurnAI, whyNotSetLevel } from '../src/campaign/turn';
import { KIND, type Corps, type Game, type Kind, type Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null, level = 'colonel'): Game => { const g = startOpen(createGame(seed, human)); setLevel(g, level); return g; };
const corps = (g: Game, nat: string, kind: Kind, at: Region, divs = 3): Corps => addCorps(g, nat, `Test ${KIND[kind].label} Corps ${g.nextCorps}`, kind, divs, at.id, {});
const clear = (r: Region): Region => { for (const c of r.units) c.dead = true; r.units = []; return r; };
const IDS = ['cadet', 'captain', 'colonel', 'general', 'marshal'];

describe('the levels', () => {
  it('are five ranks, easiest first, and the middle one adds nothing', () => {
    expect(DIFFICULTY.levels.map((l) => l.id)).toEqual(IDS);
    expect(DIFFICULTY.levels.map((l) => l.name)).toEqual(['Cadet', 'Captain', 'Colonel', 'General', 'Field Marshal']);
    expect(DIFFICULTY.start).toBe('colonel');
    expect(FAIR).toBe(2);
    const fair = DIFFICULTY.levels[FAIR];
    expect(fair.you).toEqual({ income: 1, drafts: 1 });
    expect(fair.rival).toEqual({ income: 1, drafts: 1, nerve: 1, odds: 1 });
    expect(fair.bombing).toBeUndefined();
    expect(fair.landings).toBeUndefined();
    expect(fair.concert).toBeUndefined();
    for (const l of DIFFICULTY.levels) { expect(l.blurb.length).toBeGreaterThan(20); expect(l.lines.length).toBeGreaterThanOrEqual(3); }
  });

  it('get harder step by step: less for the player, more and bolder for the rivals', () => {
    const L = DIFFICULTY.levels;
    for (let k = 1; k < L.length; k++) {
      expect(L[k].you.income).toBeLessThanOrEqual(L[k - 1].you.income);
      expect(L[k].you.drafts).toBeLessThanOrEqual(L[k - 1].you.drafts);
      expect(L[k].rival.income).toBeGreaterThanOrEqual(L[k - 1].rival.income);
      expect(L[k].rival.drafts).toBeGreaterThanOrEqual(L[k - 1].rival.drafts);
      expect(L[k].rival.nerve).toBeLessThan(L[k - 1].rival.nerve);
      expect(L[k].rival.odds).toBeLessThanOrEqual(L[k - 1].rival.odds);
    }
    // The easy levels help the player and the hard ones never do.
    expect(L[0].you.income).toBeGreaterThan(1);
    expect(L[3].you.income).toBe(1);
    expect(L[4].you.income).toBe(1);
    // Bombing begins at General; landings and offensives in concert are the Field Marshal's alone.
    expect(L.map((l) => !!l.bombing)).toEqual([false, false, false, true, true]);
    expect(L.map((l) => !!l.landings)).toEqual([false, false, false, false, true]);
    expect(L.map((l) => l.concert !== undefined)).toEqual([false, false, false, false, true]);
    expect(L[4].bombing!.share).toBeGreaterThan(L[3].bombing!.share);
  });
});

describe('history is untouched', () => {
  it('a historical scenario has no level, and cannot be given one', () => {
    const g = createGame(3, 'GER');
    expect(levelIndex(g)).toBe(FAIR);
    expect(whyNotLevel(g, 'marshal')).toMatch(/grand campaign/);
    expect(setLevel(g, 'marshal')).toBe(false);
    expect(orderLevel(g, 'cadet')).toBe(false);
    expect(g.flags.level).toBeUndefined();
    // Even a level forced into the record changes nothing outside the grand campaign.
    const before = income(g, 'FRA');
    g.flags.level = 4;
    expect(levelIndex(g)).toBe(FAIR);
    expect(rival(g, 'FRA')).toBe(false);
    expect(incomeMul(g, 'FRA')).toBe(1);
    expect(draftsMul(g, 'GER')).toBe(1);
    expect(income(g, 'FRA')).toBe(before);
    expect(bombingFor(g, 'UK')).toBeNull();
  });
});

describe('choosing a level', () => {
  it('a grand campaign starts at Colonel and can be put on any level, in any month', () => {
    const g = startOpen(createGame(3, 'GER'));
    expect(levelOf(g).id).toBe('colonel');
    expect(levelNow(g).name).toBe('Colonel');
    expect(levels().map((l) => l.id)).toEqual(IDS);
    expect(whyNotSetLevel(g, 'cadet')).toBeNull();
    expect(orderLevel(g, 'cadet')).toBe(true);
    expect(levelOf(g).id).toBe('cadet');
    expect(g.flags.level).toBe(0);
    expect(orderLevel(g, 'marshal')).toBe(true);
    expect(levelIndex(g)).toBe(4);
    expect(whyNotSetLevel(g, 'admiral')).toMatch(/no such level/);
    expect(orderLevel(g, 'admiral')).toBe(false);
    expect(levelIndex(g)).toBe(4);
    // Back at the fair level nothing is kept in the record at all: the game is the one it always was.
    expect(orderLevel(g, 'colonel')).toBe(true);
    expect(g.flags.level).toBeUndefined();
  });

  it('with nobody at the table there is nobody to measure a level against', () => {
    const g = startOpen(createGame(3, null));
    expect(whyNotSetLevel(g, 'marshal')).toMatch(/Nobody/);
    g.flags.level = 4;
    for (const id of Object.keys(g.nations)) { expect(rival(g, id), id).toBe(false); expect(incomeMul(g, id), id).toBe(1); expect(draftsMul(g, id), id).toBe(1); }
    expect(nerveOf(g, 'FRA')).toEqual({ nerve: 1, odds: 1 });
  });

  it('is saved with the campaign and shown before it is picked up again', () => {
    const g = grand(3, 'GER', 'general');
    const text = saveGame(g);
    expect(peekSave(text)!.level).toBe(3);
    const back = loadGame(text)!;
    expect(levelOf(back).id).toBe('general');
    expect(income(back, 'UK')).toBe(income(g, 'UK'));
    // A campaign saved before there were levels is a Colonel's, and a historical scenario has none.
    expect(peekSave(saveGame(startOpen(createGame(3, 'GER'))))!.level).toBe(FAIR);
    expect(peekSave(saveGame(createGame(3, 'GER')))!.level).toBeNull();
  });
});

describe('who a level bears on', () => {
  it('the player, and every nation that is not on the player\'s side', () => {
    const g = grand(3, 'GER', 'general');
    expect(rival(g, 'GER')).toBe(false);
    // Italy is Germany's ally from the first day, and Hungary leans to the Axis.
    expect(rival(g, 'ITA')).toBe(false);
    expect(rival(g, 'HUN')).toBe(false);
    for (const id of ['UK', 'FRA', 'POL', 'SOV', 'USA', 'SWE']) expect(rival(g, id), id).toBe(true);
    // From London the same war looks the other way round.
    const h = grand(3, 'UK', 'general');
    for (const id of ['FRA', 'POL', 'USA']) expect(rival(h, id), id).toBe(false);
    for (const id of ['GER', 'ITA', 'SOV', 'HUN']) expect(rival(h, id), id).toBe(true);
  });

  it('a nation that changes sides changes its standing with it', () => {
    const g = grand(3, 'UK', 'general');
    expect(rival(g, 'SOV')).toBe(true);
    g.nations.SOV.align = 'allies_cobelligerent';
    expect(rival(g, 'SOV')).toBe(false);
    expect(incomeMul(g, 'SOV')).toBe(1);
  });
});

describe('what each side can afford', () => {
  it('the player earns more on the easy levels, and the books say why', () => {
    const fair = income(grand(3, 'GER'), 'GER');
    for (const [id, mul] of [['cadet', 1.3], ['captain', 1.15], ['general', 1], ['marshal', 1]] as [string, number][]) {
      const g = grand(3, 'GER', id);
      expect(income(g, 'GER')).toBeCloseTo(fair * mul, 9);
      const L = ledger(g, 'GER');
      expect(L.gross).toBeCloseTo(income(g, 'GER'), 9);
      const line = L.changes.find((l) => l.key === 'level');
      if (mul === 1) expect(line).toBeUndefined();
      else { expect(line!.label).toBe(`Difficulty: ${levelOf(g).name}`); expect(line!.amount).toBeCloseTo(fair * (mul - 1), 9); }
    }
    expect(ledger(grand(3, 'GER'), 'GER').changes.some((l) => l.key === 'level')).toBe(false);
  });

  it('rivals earn less on the easy levels and more on the hard ones; the player\'s allies earn what they always did', () => {
    const base = grand(3, 'GER');
    for (const [id, mul] of [['cadet', 0.85], ['captain', 1], ['general', 1.2], ['marshal', 1.4]] as [string, number][]) {
      const g = grand(3, 'GER', id);
      for (const nat of ['UK', 'FRA', 'SOV']) expect(income(g, nat), `${nat} at ${id}`).toBeCloseTo(income(base, nat) * mul, 9);
      expect(income(g, 'ITA')).toBe(income(base, 'ITA'));
      // A rival's books show the same line the player's would.
      const L = ledger(g, 'FRA');
      expect(L.gross).toBeCloseTo(income(g, 'FRA'), 9);
      expect(L.changes.some((l) => l.key === 'level')).toBe(mul !== 1);
    }
  });

  it('replacements reach the player faster on the easy levels and his rivals faster on the hard ones', () => {
    // The same worn corps, a month later, at each level.
    const gain = (human: string, level: string, nat: string): number => {
      const g = grand(3, human, level);
      const c = g.corps.find((x) => x.nat === nat && x.kind === 'inf' && x.supplied && !g.regions[x.reg].overseas)!;
      for (const x of g.corps) if (x.nat === nat) x.str = 1;
      c.str = 0.4;
      g.nations[nat].pp = 500;
      startPhase(g, nat);
      return c.str - 0.4;
    };
    const fair = gain('GER', 'colonel', 'GER');
    expect(fair).toBeGreaterThan(0.05);
    expect(gain('GER', 'cadet', 'GER')).toBeCloseTo(fair * 1.3, 6);
    expect(gain('GER', 'captain', 'GER')).toBeCloseTo(fair * 1.15, 6);
    expect(gain('GER', 'marshal', 'GER')).toBeCloseTo(fair, 9);
    const theirs = gain('GER', 'colonel', 'FRA');
    expect(gain('GER', 'cadet', 'FRA')).toBeCloseTo(theirs * 0.8, 6);
    expect(gain('GER', 'general', 'FRA')).toBeCloseTo(theirs * 1.25, 6);
    expect(gain('GER', 'marshal', 'FRA')).toBeCloseTo(theirs * 1.5, 6);
    // An ally of the player is left as he was.
    expect(gain('GER', 'marshal', 'ITA')).toBeCloseTo(gain('GER', 'colonel', 'ITA'), 9);
  });

  it('a battle is worth the same at every level', () => {
    const odds = (level: string): number => {
      const g = grand(3, 'GER', level);
      const to = g.byName['Poznan'];
      const from = to.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'GER' && r.units.some((u) => u.nat === 'GER' && u.kind !== 'fort'))!;
      return assess(g, from.units.filter((u) => u.nat === 'GER' && u.kind !== 'fort'), to).odds;
    };
    const fair = odds('colonel');
    expect(fair).toBeGreaterThan(0);
    for (const id of IDS) expect(odds(id), id).toBe(fair);
  });
});

describe('how the rival staffs think', () => {
  it('they are slower to attack on the easy levels and quicker on the hard ones', () => {
    const fair = temper(grand(3, 'GER'), 'FRA');
    for (const l of DIFFICULTY.levels) {
      const g = grand(3, 'GER', l.id);
      const T = temper(g, 'FRA');
      expect(T.attackAt).toBeCloseTo(fair.attackAt * l.rival.nerve, 9);
      expect(T.persist).toBeCloseTo(fair.persist * l.rival.nerve, 9);
      expect(T.minOdds).toBeCloseTo(fair.minOdds * l.rival.odds, 9);
      // The player's allies keep their own counsel.
      expect(temper(g, 'ITA')).toEqual(temper(grand(3, 'GER'), 'ITA'));
    }
  });

  it('on the hardest level a rival joins an ally\'s offensive against the player at shorter odds', () => {
    const g = grand(3, 'FRA', 'marshal');
    const front = { enemies: ['FRA'] };
    expect(concertFor(g, 'ITA')).toBe(0.8);
    // Germany's war on Poland is nothing to Italy. Once Germany has an offensive under way against France it is.
    expect(inConcert(g, 'ITA', front)).toBe(1);
    g.flags['st:on:GER:FRA'] = g.turn;
    expect(inConcert(g, 'ITA', front)).toBe(0.8);
    // Its own offensive is no ally's, a front against somebody else is not the player's, and a dead ally leads nobody.
    expect(inConcert(g, 'GER', front)).toBe(1);
    expect(inConcert(g, 'ITA', { enemies: ['GRE'] })).toBe(1);
    g.nations.GER.alive = false;
    expect(inConcert(g, 'ITA', front)).toBe(1);
    g.nations.GER.alive = true;
    // Below Field Marshal every staff decides alone.
    for (const id of ['cadet', 'colonel', 'general']) { setLevel(g, id); expect(inConcert(g, 'ITA', front), id).toBe(1); }
  });
});

describe('rival bombers', () => {
  it('leave the player\'s factories alone up to Colonel and go for them from General', () => {
    const flying = (human: string, level: string, nat: string) => {
      const g = grand(3, human, level);
      airAI(g, nat);
      return { g, out: airFleetsOf(g, nat).filter((f) => f.mission === 'bomb' || f.mission === 'night') };
    };
    for (const id of ['cadet', 'captain', 'colonel']) expect(flying('GER', id, 'UK').out, id).toEqual([]);
    for (const id of ['general', 'marshal']) {
      const { g, out } = flying('GER', id, 'UK');
      expect(out.length, id).toBeGreaterThan(0);
      for (const f of out) {
        expect(f.bmr).toBeGreaterThanOrEqual(levelOf(g).bombing!.least);
        const r = g.regions[f.target];
        // Always a province of the player's, with factories or a railway junction in it.
        expect(r.ctrl).toBe('GER');
        expect(r.fac > 0 || railsAt(r.id).length >= 3, r.name).toBe(true);
      }
      // Never more than the level's share of the fleets that have bombers, so the armies keep their support.
      const withBombers = airFleetsOf(g, 'UK').filter((f) => f.bmr > 0).length;
      expect(out.length).toBeLessThanOrEqual(Math.max(1, Math.floor(withBombers * levelOf(g).bombing!.share + 1e-9)));
      // The fighters stay at home.
      expect(airFleetsOf(g, 'UK').filter((f) => f.bmr === 0).every((f) => f.mission === 'support')).toBe(true);
    }
  });

  it('never fly against anyone but the player, and the player\'s allies never fly them at all', () => {
    // Britain is France's ally: with France the player its bombers fly for the armies, at any level.
    const g = grand(3, 'FRA', 'marshal');
    airAI(g, 'UK');
    expect(airFleetsOf(g, 'UK').every((f) => f.mission === 'support')).toBe(true);
    // Germany is France's rival and bombs France, not Poland.
    airAI(g, 'GER');
    for (const f of airFleetsOf(g, 'GER')) if (f.mission !== 'support') expect(g.regions[f.target].ctrl).toBe('FRA');
    // A rival at peace with the player bombs nobody.
    const h = grand(3, 'UK', 'marshal');
    airAI(h, 'SOV');
    expect(airFleetsOf(h, 'SOV').every((f) => f.mission === 'support')).toBe(true);
  });
});

describe('rival landings', () => {
  /** Britain the player; the Royal Navy and the RAF are taken off the board, and a German force waits by the quay. */
  const setUp = (level: string) => {
    const g = grand(3, 'UK', level);
    g.fleets = g.fleets.filter((f) => f.nat === 'GER');
    g.airFleets = g.airFleets.filter((f) => f.nat === 'GER');
    return g;
  };

  it('a rival with shipping keeps a force at the home port nearest the player\'s harbours, on the hardest level only', () => {
    const g = setUp('marshal');
    const quay = seaBase(g, 'GER')!;
    expect(quay).toBeTruthy();
    expect(quay.port).toBeTruthy();
    expect(quay.owner).toBe('GER');
    expect(landingsFor(g, 'GER')!.force).toBe(2);
    // Poland has no shipping, the Soviet Union is not at war with Britain, and France is Britain's ally.
    expect(seaBase(g, 'POL')).toBeNull();
    expect(seaBase(g, 'SOV')).toBeNull();
    expect(seaBase(g, 'FRA')).toBeNull();
    for (const id of ['cadet', 'colonel', 'general']) expect(seaBase(setUp(id), 'GER'), id).toBeNull();
  });

  it('it lands where the player has left a harbour unguarded, and then waits before it tries again', () => {
    const run = (level: string) => {
      const g = setUp(level);
      const quay = seaBase(setUp('marshal'), 'GER')!;
      const at = g.regions[quay.id];
      const force = [corps(g, 'GER', 'inf', at, 3), corps(g, 'GER', 'inf', at, 3)];
      const shores = landingTargets(g, force).filter((r) => r.ctrl === 'UK' && r.port);
      // Every harbour in reach is left empty, and so is the country behind it.
      for (const r of shores) { clear(r); for (const j of r.nb) if (g.regions[j].ctrl === 'UK') clear(g.regions[j]); }
      const was = DIFFICULTY.levels[4].landings!.chance;
      DIFFICULTY.levels[4].landings!.chance = 1;
      try { runAI(g, 'GER'); } finally { DIFFICULTY.levels[4].landings!.chance = was; }
      return { g, shores, force };
    };
    const { g, shores, force } = run('marshal');
    expect(shores.length).toBeGreaterThan(0);
    const taken = shores.filter((r) => r.ctrl === 'GER');
    expect(taken.length).toBe(1);
    expect(force.every((c) => c.reg === taken[0].id)).toBe(true);
    expect(g.headlines.some((h) => h.text === `German troops land at ${taken[0].name} from the sea.`)).toBe(true);
    expect(g.flags['st:lodged:GER']).toBe(taken[0].id + 1);
    expect(g.flags['st:seaNext:GER']).toBe(g.turn + DIFFICULTY.levels[4].landings!.every);
    // Below Field Marshal the same empty coast is left alone.
    const calm = run('general');
    expect(calm.shores.length).toBeGreaterThan(0);
    expect(calm.shores.every((r) => r.ctrl === 'UK')).toBe(true);
  });

  it('it does not land under the guns of an army, or across a sea the player still holds', () => {
    // The Royal Navy at sea: no lane is open, so nothing is in reach at all.
    const g = grand(3, 'UK', 'marshal');
    const quay = seaBase(g, 'GER');
    if (quay) {
      const force = [corps(g, 'GER', 'inf', quay, 3), corps(g, 'GER', 'inf', quay, 3)];
      const was = DIFFICULTY.levels[4].landings!.chance;
      DIFFICULTY.levels[4].landings!.chance = 1;
      try { runAI(g, 'GER'); } finally { DIFFICULTY.levels[4].landings!.chance = was; }
      expect(g.regions.some((r) => r.owner === 'UK' && r.ctrl === 'GER')).toBe(false);
      expect(force.every((c) => g.regions[c.reg].owner !== 'UK')).toBe(true);
    }
    // The sea is open but every harbour in reach is strongly held: the force stays in port.
    const h = setUp('marshal');
    const at = seaBase(h, 'GER')!;
    const force = [corps(h, 'GER', 'inf', at, 3), corps(h, 'GER', 'inf', at, 3)];
    for (const r of landingTargets(h, force).filter((x) => x.ctrl === 'UK' && x.port)) { corps(h, 'UK', 'inf', r, 4); corps(h, 'UK', 'inf', r, 4); corps(h, 'UK', 'inf', r, 4); }
    const was = DIFFICULTY.levels[4].landings!.chance;
    DIFFICULTY.levels[4].landings!.chance = 1;
    try { runAI(h, 'GER'); } finally { DIFFICULTY.levels[4].landings!.chance = was; }
    expect(h.regions.some((r) => r.owner === 'UK' && r.ctrl === 'GER')).toBe(false);
  });
});

describe('a war with nobody measuring', () => {
  it('runs the same at the fair level whoever is counted as the player, and at any level with nobody counted', () => {
    const play = (level: string, as: string | null): string => {
      const g = startOpen(createGame(5, null));
      if (as) asIf.set(g, as);
      setLevel(g, level);
      if (!as && level !== 'colonel') g.flags.level = IDS.indexOf(level);
      for (let k = 0; k < 4; k++) runTurnAI(g);
      delete g.flags.level;
      return saveGame(g);
    };
    const fair = play('colonel', null);
    expect(play('colonel', 'GER')).toBe(fair);
    expect(play('marshal', null)).toBe(fair);
    // With somebody counted as the player, the hardest level is a different war.
    expect(play('marshal', 'GER')).not.toBe(fair);
  }, 60000);
});
