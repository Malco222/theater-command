// The design director's b624 (ruling 232's rider, from b615's trace of Moscow): works dug by a nation count only for
// that nation and its side, never for its enemy against that nation's own attacks. The Moscow lines faced west; a ring
// that had taken the ground behind them stood in them backwards against the city's relief. Every nation, every stop:
// the works in the field, a town's walls, the town guard's, the shore batteries, a nation's works traits, and what the
// staffs reckon of them all.
import { describe, expect, it } from 'vitest';
import { addCorps, assess, createGame, relocate, setWar, worksAgainst, worksBy, worksFor } from '../src/campaign/game';
import { garrison, townBonus } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { traitDefence } from '../src/campaign/traits';
import { FORT_MUL, type Corps, type Game, type Region } from '../src/campaign/types';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const empty = (g: Game, r: Region): void => { for (const c of [...r.units]) relocate(g, c, g.regions.find((x) => x.ctrl === c.nat && x.id !== r.id && !r.nb.includes(x.id))!); };
const line = (a: { lines: { label: string; value: string; effect?: string }[] }, label: string) => a.lines.find((l) => l.label === label);

describe('who dug the works', () => {
  it('the works on the map from the first day are their country\'s', () => {
    const g = grand();
    for (const [place, by] of [['Lorraine', 'FRA'], ['Saar', 'GER'], ['Moscow', 'SOV'], ['Tobruk', 'ITA'], ['Malta', 'UK']] as const) {
      expect(g.byName[place].fort?.by, place).toBe(by);
      expect(worksBy(g.byName[place]), place).toBe(by);
    }
  });

  it('works with no name on them (an old save) are the owner\'s', () => {
    const g = grand();
    const r = g.byName['Lorraine'];
    r.fort = { lv: r.fort!.lv, vs: r.fort!.vs };
    expect(worksBy(r)).toBe('FRA');
  });

  it('count for the nation that dug them and its allies, and for nobody else', () => {
    const g = grand();
    const r = g.byName['Lorraine'];
    expect(worksFor(g, r, 'FRA')).toBe(true);
    expect(worksFor(g, r, 'UK')).toBe(true);
    expect(worksFor(g, r, 'GER')).toBe(false);
    expect(worksFor(g, r, 'ITA')).toBe(false);
    expect(worksFor(g, g.byName['Minsk'], 'SOV')).toBe(true);
    expect(worksFor(g, g.byName['Lodz'], 'POL')).toBe(false); // no works there at all
  });
});

describe('in the field: the Moscow case', () => {
  /** Germany at war with the Soviet Union, one German corps in Moscow's field, one Soviet corps next door on Soviet ground. */
  function moscow(g: Game, inField: 'GER' | 'SOV'): { M: Region; att: Corps } {
    setWar(g, 'GER', 'SOV');
    const M = g.byName['Moscow'];
    empty(g, M);
    const door = g.byName['Vladimir'];
    empty(g, door);
    addCorps(g, inField, 'Holding corps', 'inf', 3, M.id, { xp: 0.3 });
    const att = addCorps(g, inField === 'GER' ? 'SOV' : 'GER', 'Attacking corps', 'inf', 3, door.id, { xp: 0.3 });
    return { M, att };
  }

  it('Moscow\'s works face whoever comes from Soviet ground', () => {
    const g = grand();
    expect(g.byName['Moscow'].fort!.vs).toContain('SOV');
    expect(g.byName['Vladimir'].owner).toBe('SOV');
  });

  it('a Soviet army in Moscow\'s field has its own lines against a German attack', () => {
    const g = grand();
    const { M, att } = moscow(g, 'SOV');
    expect(worksAgainst(g, M, 'GER')).toBe(true);
    const a = assess(g, [att], M);
    expect(line(a, 'Fortifications')?.effect).toMatch(/Attacked head-on/);
  });

  it('a German ring in Moscow\'s field does not stand in the Soviet lines against the Soviet relief', () => {
    const g = grand();
    const { M, att } = moscow(g, 'GER');
    expect(worksAgainst(g, M, 'SOV')).toBe(false);
    const a = assess(g, [att], M);
    expect(line(a, 'Fortifications')?.value).toMatch(/dug by the Soviet Union/);
    expect(line(a, 'Fortifications')?.effect).toBe('Works count only for the side that dug them: no help to the defence here');
    // The same attack with the works counted would be cut to the works' share, so the odds stand that much higher without.
    const with_ = (() => { M.fort!.by = 'GER'; const b = assess(g, [att], M); M.fort!.by = 'SOV'; return b; })();
    expect(line(with_, 'Fortifications')?.effect).toMatch(/Attacked head-on/);
    expect(a.odds / with_.odds).toBeCloseTo(FORT_MUL[M.fort!.lv], 1);
  });
});

describe('in a town', () => {
  it('a town\'s fortress works stand with a garrison of the side that dug them, not with its enemy\'s', () => {
    const g = grand();
    const r = g.byName['Lorraine'];
    const own = townBonus(g, r);
    expect(townBonus(g, r, 0, false)).toBeLessThan(own);
    r.ctrl = 'GER';
    expect(worksFor(g, r, r.ctrl)).toBe(false);
    expect(townBonus(g, r)).toBeCloseTo(townBonus(g, r, 0, false), 9);
    r.ctrl = 'UK';
    expect(townBonus(g, r)).toBeCloseTo(own, 9);
  });

  it('a storm reckons the walls the garrison may count', () => {
    const g = grand();
    setWar(g, 'GER', 'SOV');
    const M = g.byName['Moscow'];
    empty(g, M);
    const inside = addCorps(g, 'SOV', 'Moscow garrison', 'inf', 3, M.id, {});
    expect(garrison(g, inside)).toBe(true);
    const ger = addCorps(g, 'GER', 'Storming corps', 'inf', 3, g.byName['Vyazma'].id, {});
    const ours = assess(g, [ger], M, { town: true }).odds;
    M.fort!.by = 'GER';
    const theirs = assess(g, [ger], M, { town: true });
    expect(theirs.odds).toBeGreaterThan(ours);
    expect(line(theirs, 'The town')?.value).toMatch(/dug by Germany: they give its defenders no cover/);
  });
});

describe('the nations\' works traits', () => {
  it('France\'s fortress line helps a French corps only in works France\'s side dug', () => {
    const g = grand();
    const r = g.byName['Lorraine'];
    const fr = addCorps(g, 'FRA', 'Fortress corps', 'inf', 3, r.id, {});
    const inOwn = traitDefence(g, fr, r);
    expect(inOwn).toBeGreaterThan(1);
    r.fort!.by = 'GER';
    expect(traitDefence(g, fr, r)).toBeLessThan(inOwn);
  });
});
