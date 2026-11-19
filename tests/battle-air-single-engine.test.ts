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

/** A battle's history without what may differ by the bombers' engines: the straggler's words, engines and airscrew. */
const bare = (h: FieldEvent[]) => h.map((e) => {
  const { eng: _e, prop: _p, ...extra } = (e.extra ?? {}) as Record<string, unknown>;
  return { t: e.t, what: e.what, unit: e.unit, side: e.side, cue: e.cue, text: e.what === 'air:straggle' ? '' : e.text, extra };
});

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
    expect(subjectOf('Home Fleet strike wing')).toEqual({ who: "The Home Fleet's strike wing", whom: "the Home Fleet's strike wing", many: false });
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
      for (const e of h.filter((e) => e.what === 'air:lost' && box.has(e.unit))) expect(e.text).toMatch(def ? /^An aircraft of the British Home Fleet's strike wing / : /^An aircraft of the Home Fleet's strike wing /);
    }
    const tk = ticketOf(1);
    tk.att.human = false; tk.def.human = true; tk.att.adj = 'Uruguayan';
    const h = fight(tk).history();
    expect(h.filter((e) => e.what === 'air:straggle').length).toBeGreaterThan(0);
    for (const e of h.filter((e) => e.what === 'air:straggle' || (e.what === 'air:limped' && e.text))) expect(e.text).toMatch(/(^A|a) (damaged )?Uruguayan bomber of /);
  });
});
