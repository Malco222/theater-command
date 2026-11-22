// A single-engined bomber falling out of formation (the first Mac run of the Pearl Harbor scene, 19 November: "A
// bomber of Shokaku and Zuikaku dive bombers has lost an engine and is falling out of formation" was said of a
// single-engined aircraft). A carrier or dive bomber has no engine to lose and fly on: its one engine is hit, runs
// rough with its airscrew turning, and it falls back as a twin does. A box named for its bombers is "one of the ...".
// All of it is shown, never counted: the same battle flown by single-engined bombers ends the same.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { enginesOf } from '../src/battle/air/doctrine';
import { airBattle, oneOf, subjectOf, told, type AirSim } from '../src/battle/air/sim';
import { aCraftOf, adjFor } from '../src/battle/air/names';
import { airTicket, raidGroups } from '../src/battle/air/ticket';
import type { FieldEvent } from '../src/battle/core/types';

const SINGLE = ['B5N', 'B6N', 'D3A', 'D4Y', 'TBD', 'TBF', 'SBD', 'SB2C', 'Swordfish', 'Barracuda', 'Skua', 'Stuka', 'Torpedo:ITA', 'Dive:FRA'];
const RUNS = 8;

/** The k-th German raid on Alsace (as tests/battle-air-realism.test.ts), its bombers flown as 'model'. */
function ticketOf(k: number, model?: string) {
  const g = startOpen(createGame(7, 'GER'));
  setWar(g, 'GER', 'UK'); setWar(g, 'GER', 'FRA');
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const t = raidTargets(g, 'bomb', 'GER').find((x) => planRaid(g, 'bomb', x.id, undefined, 'GER').place === 'Alsace')!;
  const tk = structuredClone(airTicket(g, 'bomb', t.id, raidGroups(g, 'bomb', t.id, 'GER'), 'GER').ticket!);
  if (model) tk.place.models!.GER.bmr = model;
  return tk;
}

function fight(tk: ReturnType<typeof ticketOf>, name?: string): AirSim {
  const s = airBattle(tk, { show: true });
  s.ai.att = true; s.ai.def = true;
  // As the set piece's record names its boxes once the battle is set up (tools/air-record/record.ts).
  if (name) for (const u of s.units) if (u.side === 'att' && u.air === 'bmr') u.name = name;
  while (!s.over) s.step();
  return s;
}

/** The ending of a loss's line that says who got out: ', one parachute', ', 3 parachutes', ', 2 men climbing out'. */
const WHO_OUT = /(, (one|\d+) parachutes?)?(, (one man|\d+ men) climbing out)?\.$/;

/**
 * A battle's history without what may differ by the bombers' type: the straggler's words, engines and airscrew, and
 * who is seen to get out of a bomber that is lost (Brian's yes of 20 November 2026: a dive bomber shows its real crew
 * of two, not up to five): its parachutes, when each opens and their cue, the men climbing out, the prisoners they
 * make, and that ending of its line (a collision's line carries its bomber's). Everything else stays pinned, and who
 * gets out is pinned in its own way below.
 */
const bare = (h: FieldEvent[]) => h.map((e) => {
  const { eng: _e, prop: _p, ...all } = (e.extra ?? {}) as Record<string, unknown>;
  const lost = e.what === 'air:lost', bomber = lost && all.air === 'bmr';
  const { chutes: _c, chuteAt: _a, chuteCue: _q, out: _o, captured: _k, ...rest } = all;
  const text = e.what === 'air:straggle' ? '' : bomber || (lost && all.how === 'collide') ? e.text.replace(WHO_OUT, '.') : e.text;
  return { t: e.t, what: e.what, unit: e.unit, side: e.side, cue: e.cue, text, extra: bomber ? rest : all };
});

interface Out { chutes: number; chuteAt?: number[]; out?: number; captured: number; air: string; how: string }
/** Who got out of each bomber lost, in the order they fell. */
const bombersLost = (s: AirSim) => s.history().filter((e) => e.what === 'air:lost' && (e.extra as unknown as Out).air === 'bmr');

describe('engines on a bomber model', () => {
  it('the carrier and dive bombers have one; the He 111 and the plain bomber two; the B-17 and the Lancaster four', () => {
    for (const m of SINGLE) expect(enginesOf(m), m).toBe(1);
    expect(['He111', 'Bomber:GER', 'Bomber:UK', 'Torpedo', 'Dive'].map(enginesOf)).toEqual([2, 2, 2, 2, 2]);
    expect([enginesOf('B17'), enginesOf('Lancaster')]).toEqual([4, 4]);
  });
});

describe('a single-engined bomber falling out', () => {
  it('has its one engine hit, the airscrew turning, in its own words; and the battle ends as the twins\' does', () => {
    let seen = 0;
    for (let k = 0; k < RUNS; k++) {
      const twin = fight(ticketOf(k)), one = fight(ticketOf(k, 'D3A'));
      expect(one.outcome(), `run ${k}`).toEqual(twin.outcome());
      expect(one.over!.lines).toEqual(twin.over!.lines);
      expect(bare(one.history())).toEqual(bare(twin.history()));
      for (const e of one.history().filter((e) => e.what === 'air:straggle')) {
        seen++;
        expect(e.extra).toMatchObject({ eng: [1], prop: 'windmill' });
        expect(e.text).toMatch(/^(A bomber of .+ has taken a hit in the engine and|Flak has hit the engine of a bomber of .+, and it) is falling out of formation\.$/);
        expect(e.text).not.toMatch(/lost an engine|an engine on/);
      }
      for (const e of one.history().filter((e) => e.what === 'air:lost' && (e.extra as { strag?: number }).strag !== undefined)) expect((e.extra as { eng: number[] }).eng).toEqual([1]);
      for (const e of twin.history().filter((e) => e.what === 'air:straggle')) expect(e.text).toMatch(/lost an engine|knocked out an engine/);
    }
    expect(seen).toBeGreaterThan(5);
  });

  it('shows its real crew of two get out, never the twin\'s four or five: the first two of the same men, by the same dice', () => {
    let losses = 0, capped = 0, both = 0;
    for (let k = 0; k < RUNS; k++) {
      const twin = bombersLost(fight(ticketOf(k))), one = bombersLost(fight(ticketOf(k, 'D3A')));
      expect(one.length).toBe(twin.length);
      one.forEach((e, i) => {
        const a = e.extra as unknown as Out, b = twin[i].extra as unknown as Out;
        losses++;
        // Never more seen out than are aboard: two in the dive bomber, four in the He 111 of 1939.
        expect(a.chutes + (a.out ?? 0)).toBeLessThanOrEqual(2);
        expect(b.chutes + (b.out ?? 0)).toBeLessThanOrEqual(4);
        // The two-seater's men are the first two of the twin's: no more parachutes than the twin shows, opening when the twin's first ones do.
        expect(a.chutes).toBeLessThanOrEqual(b.chutes);
        expect(a.chuteAt ?? []).toEqual((b.chuteAt ?? []).slice(0, a.chutes));
        expect((a.chuteAt ?? []).length).toBe(a.chutes);
        // Over the enemy's ground, everyone who gets out is taken.
        expect(a.captured).toBe(a.chutes + (a.out ?? 0));
        // Its own line says its own count (a collision's line carries both aircraft's, and is checked with the collisions).
        if (a.how !== 'collide') {
          const end = `${a.chutes === 0 ? '' : a.chutes === 1 ? ', one parachute' : `, ${a.chutes} parachutes`}${!a.out ? '' : a.out === 1 ? ', one man climbing out' : `, ${a.out} men climbing out`}.`;
          expect(e.text.endsWith(end), `${e.text} | ${end}`).toBe(true);
          expect(e.text).not.toMatch(/, [3-9] (parachutes|men)/);
        }
        if (b.chutes + (b.out ?? 0) > 2) capped++;
        if (a.chutes + (a.out ?? 0) === 2) both++;
      });
    }
    // Seen to bite: the twin often shows more than two, and the two-seater does show both its men.
    expect(losses).toBeGreaterThan(100);
    expect(capped).toBeGreaterThan(10);
    expect(both).toBeGreaterThan(5);
  });

  it('is told as "one of the" a box named for its bombers, the enemy\'s with its nation, and "damaged" after the name', () => {
    const NAME = 'Shokaku and Zuikaku dive bombers';
    let fell = 0, home = 0, lost = 0;
    for (let k = 0; k < RUNS; k++) for (const theirs of [false, true]) {
      const tk = ticketOf(k, 'D3A');
      // The player on the other side: the raid is the enemy's, and its box takes its nation.
      if (theirs) { tk.att.human = false; tk.def.human = true; }
      const who = theirs ? `the German ${NAME}` : `the ${NAME}`;
      const h = fight(tk, NAME).history();
      for (const e of h.filter((e) => e.what === 'air:straggle')) {
        fell++;
        expect([`One of ${who} has taken a hit in the engine and is falling out of formation.`, `Flak has hit the engine of one of ${who}, and it is falling out of formation.`]).toContain(e.text);
      }
      for (const e of h.filter((e) => e.what === 'air:limped' && e.text)) { home++; expect(e.text).toBe(`One of ${who}, damaged, has turned for home on its own.`); }
      // Its losses name it the same way a few seconds on: "One of the Shokaku and Zuikaku dive bombers blows up in the air."
      const box = new Set(h.filter((e) => e.what === 'air:straggle').map((e) => e.unit));
      for (const e of h.filter((e) => e.what === 'air:lost' && box.has(e.unit))) { lost++; expect(e.text.startsWith(`One of ${who} `), e.text).toBe(true); }
      for (const e of h) expect(e.text).not.toMatch(/(bomber|aircraft) of .*bombers/);
    }
    expect(fell).toBeGreaterThan(5);
    expect(home).toBeGreaterThan(0);
    expect(lost).toBeGreaterThan(5);
  });

  it('keeps "a bomber of" for a box named otherwise, whatever it flies', () => {
    const h = fight(ticketOf(1, 'TBD'), 'Torpedo Three, Six and Eight').history();
    const out = h.filter((e) => e.what === 'air:straggle');
    expect(out.length).toBeGreaterThan(0);
    for (const e of out) expect(e.text).toMatch(/^(A bomber of Torpedo Three, Six and Eight has taken a hit in the engine and|Flak has hit the engine of a bomber of Torpedo Three, Six and Eight, and it) is falling out of formation\.$/);
  });
});

describe('the campaign\'s carrier groups by name (the UI director\'s air review 10, carried to the strike wing)', () => {
  it('a strike wing is its fleet\'s, singular; a fleet called by its letter takes no article', () => {
    expect(subjectOf('Home Fleet strike wing')).toEqual({ who: "The Home Fleet's strike wing", whom: "the Home Fleet's strike wing", many: false, craft: false });
    expect(told('Home Fleet strike wing', '{N} [is|are] in a fight.')).toBe("The Home Fleet's strike wing is in a fight.");
    expect(told('Force H strike wing', '{N} [is|are] in a fight.')).toBe("Force H's strike wing is in a fight.");
    expect(told('Force H carrier fighters', '{N} [is|are] in a fight.')).toBe("Force H's carrier fighters are in a fight.");
    expect(told('British Force H carrier fighters', '{N} [is|are] in a fight.')).toBe("British Force H's carrier fighters are in a fight.");
    expect(subjectOf('Home Fleet carrier fighters').who).toBe("The Home Fleet's carrier fighters");
  });
  it('one of a group named for its aircraft', () => {
    expect(oneOf('Shokaku and Zuikaku dive bombers')).toBe('one of the Shokaku and Zuikaku dive bombers');
    expect(oneOf('Shokaku and Zuikaku dive bombers', 'Japanese')).toBe('one of the Japanese Shokaku and Zuikaku dive bombers');
    expect(oneOf('Home Fleet carrier fighters', 'British')).toBe("one of the British Home Fleet's carrier fighters");
    expect(oneOf('Force H carrier fighters')).toBe("one of Force H's carrier fighters");
  });
  it('tells a strike wing\'s straggler and loss with the article, and takes "a" before Uruguayan', () => {
    for (const [att, def, pre] of [[true, false, 'A bomber of'], [false, true, 'A British bomber of']] as const) {
      const tk = ticketOf(1, 'Swordfish');
      tk.att.human = att; tk.def.human = def; tk.att.adj = 'British';
      const h = fight(tk, 'Home Fleet strike wing').history();
      const out = h.filter((e) => e.what === 'air:straggle');
      expect(out.length).toBeGreaterThan(0);
      for (const e of out) expect([`${pre} the Home Fleet's strike wing has taken a hit in the engine and is falling out of formation.`, `Flak has hit the engine of ${pre[0].toLowerCase()}${pre.slice(1)} the Home Fleet's strike wing, and it is falling out of formation.`]).toContain(e.text);
      const box = new Set(out.map((e) => e.unit));
      // The nation word on the aircraft (the UI director, 14:26Z, ruling 2).
      for (const e of h.filter((e) => e.what === 'air:lost' && box.has(e.unit))) expect(e.text).toMatch(def ? /^A British aircraft of the Home Fleet's strike wing / : /^An aircraft of the Home Fleet's strike wing /);
    }
    const tk = ticketOf(1);
    tk.att.human = false; tk.def.human = true; tk.att.adj = 'Uruguayan';
    const h = fight(tk).history();
    expect(h.filter((e) => e.what === 'air:straggle').length).toBeGreaterThan(0);
    for (const e of h.filter((e) => e.what === 'air:straggle' || (e.what === 'air:limped' && e.text))) expect(e.text).toMatch(/(^A|a) (damaged )?Uruguayan bomber of /);
  });
});

describe('the UI director\'s rule of 14:48Z: a name takes "the" unless written as a code or a proper name of its own', () => {
  const fight1 = (n: string) => told(n, '{N} [is|are] in a fight.');
  it('takes "the"', () => {
    expect(fight1('Japanese Akagi fighter patrol')).toBe('The Japanese Akagi fighter patrol is in a fight.');
    expect(fight1('Polish Pursuit Brigade')).toBe('The Polish Pursuit Brigade is in a fight.');
    expect(fight1('American Caribbean Air Force')).toBe('The American Caribbean Air Force is in a fight.');
    expect(aCraftOf('Northern Air Zone', 'bomber')).toBe('a bomber of the Northern Air Zone');
    expect(fight1('US Ninth Air Force')).toBe('The US Ninth Air Force is in a fight.');
    expect(subjectOf('Mobile Force strike wing').who).toBe("The Mobile Force's strike wing");
  });
  it('stays bare: codes, "No." names, the American navy\'s squadrons, Force H, names in their own language, the RAF\'s commands', () => {
    for (const n of ['KG 2', 'German KG 2', 'JG 26', 'VMF-221', 'No. 3 Group', 'Bombing Three', 'Fighting Six', 'Torpedo Eight', 'Jagdgeschwader 2', '4° Stormo', 'Tainan Kokutai', 'Japanese Tainan Kokutai', 'Luftflotte 2', 'Groupe de Chasse 3', 'Bomber Command', 'Fighter Command']) {
      expect(fight1(n), n).toBe(`${n} is in a fight.`);
    }
    expect(subjectOf('Force H strike wing').whom).toBe("Force H's strike wing");
    expect(subjectOf('Kido Butai carrier fighters').whom).toBe("Kido Butai's carrier fighters");
  });
  it('the enemy\'s fleet wing by its aircraft and fleet; the player\'s own as "one of"', () => {
    expect(aCraftOf('Home Fleet carrier fighters', 'aircraft', 'British')).toBe('a British carrier fighter of the Home Fleet');
    expect(aCraftOf('Force H carrier fighters', 'aircraft', 'British')).toBe('a British carrier fighter of Force H');
    expect(aCraftOf('Home Fleet carrier fighters', 'aircraft')).toBe("one of the Home Fleet's carrier fighters");
    expect(aCraftOf('Force H carrier fighters', 'aircraft')).toBe("one of Force H's carrier fighters");
    // The strike wing keeps its passed form, the nation word on the aircraft.
    expect(aCraftOf('Home Fleet strike wing', 'bomber', 'British')).toBe("a British bomber of the Home Fleet's strike wing");
    // A fleet whose name says its nation: still the enemy's form, the nation word said once (ruling 5).
    expect(aCraftOf('Brazilian Fleet carrier fighters', 'aircraft', 'Brazilian')).toBe('a carrier fighter of the Brazilian Fleet');
    expect(aCraftOf('Royal Norwegian Navy carrier fighters', 'aircraft', 'Norwegian')).toBe('a carrier fighter of the Royal Norwegian Navy');
    expect(aCraftOf('Brazilian Fleet carrier fighters', 'aircraft')).toBe("one of the Brazilian Fleet's carrier fighters");
    expect(aCraftOf('Polish Navy strike wing', 'bomber', 'Polish', 'damaged')).toBe("a damaged bomber of the Polish Navy's strike wing");
    expect(aCraftOf('Japanese Shokaku and Zuikaku dive bombers', 'bomber', 'Japanese')).toBe('one of the Japanese Shokaku and Zuikaku dive bombers');
  });
});

describe('the UI director\'s six rulings of 14:26Z (words-air-battle-dispatches.md)', () => {
  it('1. an ordinal group takes "the"; "No." names and the German and Italian forms stay bare', () => {
    expect(told('1st Bomber Group', '{N} [is|are] in a fight.')).toBe('The 1st Bomber Group is in a fight.');
    expect(told('American 15th Pursuit Group', '{N} [is|are] in a fight.')).toBe('The American 15th Pursuit Group is in a fight.');
    expect(told('Eighth Air Force', '{N} [is|are] in a fight.')).toBe('The Eighth Air Force is in a fight.');
    expect(aCraftOf('8th Bomb Group', 'bomber')).toBe('a bomber of the 8th Bomb Group');
    // A Roman numeral is an ordinal too, and 'I Bomber Command' must never read as the pronoun.
    expect(told('I Bomber Command', '{N} [is|are] in a fight.')).toBe('The I Bomber Command is in a fight.');
    expect(told('American VIII Fighter Command', '{N} [is|are] in a fight.')).toBe('The American VIII Fighter Command is in a fight.');
    for (const bare of ['No. 3 Group', 'No. 11 Bomber Group', 'KG 2', 'JG 26', 'Kampfgeschwader 2', 'Stormo Bombardieri 4', 'Groupe de Chasse 3', 'Groupe de Chasse I', 'Bomber Command', 'Italian Stormo Caccia 2']) {
      expect(told(bare, '{N} [is|are] in a fight.'), bare).toBe(`${bare} is in a fight.`);
    }
  });
  it('2. the nation word sits on the aircraft where a line names one, and before the name where the group is the subject', () => {
    expect(aCraftOf('Bombing Three and Scouting Six', 'aircraft', 'American')).toBe('an American aircraft of Bombing Three and Scouting Six');
    expect(aCraftOf('15th Pursuit Group', 'aircraft', 'American')).toBe('an American aircraft of the 15th Pursuit Group');
    expect(told('American Bombing Three and Scouting Six', '{N} [is|are] in a fight.')).toBe('American Bombing Three and Scouting Six are in a fight.');
  });
  it('3. a name joined by "and" or a comma, or ending in a plural noun, takes a plural verb; a single unit the singular', () => {
    expect(told('Torpedo Three, Six and Eight', '{N} [is|are] in a fight.')).toBe('Torpedo Three, Six and Eight are in a fight.');
    expect(told('Fighting Three and Fighting Six', '{N} [has|have] broken away from')).toBe('Fighting Three and Fighting Six have broken away from');
    expect(told('First Air Fleet fighters', '{N} [is|are] in a fight.')).toBe('The First Air Fleet fighters are in a fight.');
    expect(told('KG 2', '{N} [is|are] in a fight.')).toBe('KG 2 is in a fight.');
    // Joined, but not named for its aircraft: one of it is still 'a bomber of'.
    expect(aCraftOf('Torpedo Three, Six and Eight', 'bomber')).toBe('a bomber of Torpedo Three, Six and Eight');
  });
  it('4. a box named for its aircraft takes "the" wherever it stands', () => {
    expect(subjectOf('Akagi and Kaga torpedo bombers').whom).toBe('the Akagi and Kaga torpedo bombers');
    expect(told('Japanese Shokaku and Zuikaku dive bombers', '{N} [is|are] in a fight.')).toBe('The Japanese Shokaku and Zuikaku dive bombers are in a fight.');
  });
  it('5. no nation word twice', () => {
    expect([adjFor('Polish Navy strike wing', 'Polish'), adjFor('Royal Norwegian Navy carrier fighters', 'Norwegian'), adjFor('Home Fleet strike wing', 'British'), adjFor('KG 2', ''), adjFor('US Ninth Air Force', 'British')]).toEqual(['', '', 'British', '', '']);
  });
  it('in a battle: the enemy\'s joined and nation-named groups, and an ordinal one', () => {
    const lines = (adj: string, name: string) => {
      const tk = ticketOf(1, 'D3A');
      tk.att.human = false; tk.def.human = true; tk.att.adj = adj;
      const s = fight(tk, name), ids = new Set(s.units.filter((u) => u.side === 'att' && u.air === 'bmr').map((u) => u.id));
      return s.history().filter((e) => e.unit !== undefined && ids.has(e.unit));
    };
    const joined = lines('American', 'Bombing Three and Scouting Six');
    expect(joined.some((e) => e.what === 'air:straggle')).toBe(true);
    expect(joined.some((e) => e.what === 'air:lost')).toBe(true);
    expect(joined.some((e) => e.what === 'contact')).toBe(true);
    for (const e of joined.filter((e) => e.what === 'air:straggle')) expect(e.text).toMatch(/^(An American bomber of Bombing Three and Scouting Six has|Flak has hit the engine of an American bomber of Bombing Three and Scouting Six,)/);
    for (const e of joined.filter((e) => e.what === 'air:lost')) expect(e.text).toMatch(/^An American aircraft of Bombing Three and Scouting Six /);
    for (const e of joined.filter((e) => e.what === 'contact')) expect(e.text).toBe('American Bombing Three and Scouting Six are in a fight.');
    const navy = lines('Polish', 'Polish Navy strike wing');
    expect(navy.some((e) => e.what === 'air:lost')).toBe(true);
    for (const e of navy) expect(e.text).not.toMatch(/Polish Polish|Polish bomber|Polish aircraft/);
    for (const e of navy.filter((e) => e.what === 'contact')) expect(e.text).toBe("The Polish Navy's strike wing is in a fight.");
    for (const e of navy.filter((e) => e.what === 'air:lost')) expect(e.text).toMatch(/^An aircraft of the Polish Navy's strike wing /);
    // An escort and its box are one side: its break names the box bare, so the nation word is said once.
    // Three escort groups in 1940, so close escort flies (as tests/battle-air-realism.test.ts splitEscort).
    const tk = ticketOf(0, 'D3A');
    tk.place.when = { year: 1940, month: 8 };
    const f = tk.att.forces.find((x) => x.kind !== 'bmr')!, each = Math.floor(f.men / 3);
    for (let i = 1; i < 3; i++) { tk.att.forces.push({ ...f, corps: 90_000 + i, men: each }); tk.place.starts[90_000 + i] = { ...tk.place.starts[f.corps] }; }
    f.men -= each * 2;
    tk.att.human = false; tk.def.human = true;
    const esc = fight(tk, 'Shokaku and Zuikaku dive bombers').history().filter((e) => e.what === 'air:engage' && e.text);
    expect(esc.length).toBeGreaterThan(0);
    // The escort by the game's own name for it ("the" as its name takes it); the box bare after it.
    const called = f.name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    for (const e of esc) expect(e.text).toMatch(new RegExp(`^(The )?German ${called} (has|have) broken away from the Shokaku and Zuikaku dive bombers to take on the attackers\\.$`));
    const nth = lines('American', '8th Bomb Group');
    expect(['contact', 'air:lost', 'air:straggle'].every((w) => nth.some((e) => e.what === w))).toBe(true);
    for (const e of nth.filter((e) => e.what === 'contact')) expect(e.text).toBe('The American 8th Bomb Group is in a fight.');
    for (const e of nth.filter((e) => e.what === 'air:lost')) expect(e.text).toMatch(/^An American aircraft of the 8th Bomb Group /);
    for (const e of nth.filter((e) => e.what === 'air:straggle')) expect(e.text).toMatch(/an American bomber of the 8th Bomb Group/i);
  });
});

describe('ruling 6 (the UI director, 14:26Z): torpedo bombers attack, dive and level bombers bomb', () => {
  it('says "attack" for a torpedo box over the target, "bomb" for the others', () => {
    for (const [strike, name, verb] of [['torpedo', 'Akagi and Kaga torpedo bombers', 'attack'], ['dive', 'Shokaku and Zuikaku dive bombers', 'bomb'], ['level', 'Soryu and Hiryu level bombers', 'bomb']] as const) {
      const s = airBattle(ticketOf(1, 'B5N'), { show: true });
      s.ai.att = true; s.ai.def = true;
      for (const u of s.units) if (u.side === 'att' && u.air === 'bmr') { u.name = name; u.strike = strike; }
      while (!s.over) s.step();
      const over = s.history().filter((e) => e.what === 'air:bombs');
      expect(over.length, strike).toBeGreaterThan(0);
      for (const e of over) expect(e.text.startsWith(`The ${name} ${verb} `), e.text).toBe(true);
    }
  });
});
