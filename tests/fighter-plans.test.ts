// The fighter's plans (the design director, 23 Nov 07:00Z, on Brian's 06:50Z ask: "steal technology and blue prints to
// help put the planes fight on par"): under the fighter types switch, any nation with aircraft works may send an agent
// for the plans of the fighter another nation builds most, with 235's places, odds and war chance. Plans give the
// design, research the means: the mark joins the thief's table and is built once its designers reach its year.
import { describe, expect, it } from 'vitest';
import { allied, createGame } from '../src/campaign/game';
import { FIGHTERS, fieldedMark, hasFighterWorks, holdsMark, setFighterTypes, stealMark, stolenMarks, typeMix } from '../src/campaign/fighters';
import { INTEL, MISSIONS, MISSION_TEXT, intelTick, successChance, whyNotMission } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { inOrAt } from '../src/campaign/towns';
import type { Agent, Game, Mission } from '../src/campaign/types';
import { designYear } from '../src/battle/air/doctrine';

function grand(on: boolean, seed = 3): Game {
  const g = createGame(seed, 'SWE');
  if (on) expect(setFighterTypes(g, true)).toBe(true);
  return startOpen(g);
}
function plant(g: Game, at: string, mission: Mission, nat: string): Agent {
  const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp: 0, reg: g.byName[at].id, ready: g.turn, mission, born: g.turn };
  g.intel.agents.push(a);
  return a;
}

describe("the fighter's plans", () => {
  it('is offered only with the fighter types switch on, with the plans thefts\' own odds', () => {
    expect(MISSIONS).toContain('fighterPlans');
    expect(INTEL.missions.fighterPlans).toEqual(INTEL.missions.bomberPlans);
    expect(MISSION_TEXT.fighterPlans.label).toBe("Steal the fighter's plans");
    const off = grand(false), on = grand(true);
    const a = plant(off, 'Berlin', 'watch', 'SWE'), b = plant(on, 'Berlin', 'watch', 'SWE');
    expect(whyNotMission(off, 'SWE', a.id, 'fighterPlans')).not.toBeNull();
    expect(whyNotMission(on, 'SWE', b.id, 'fighterPlans')).toBeNull();
    // (a theft, so Microfilm helps it as it helps the others)
    expect(successChance(b, 'fighterPlans', on)).toBe(successChance(b, 'bomberPlans', on));
  });

  it('needs aircraft works at home, and is kept at the capital or the great aircraft works, in words', () => {
    const g = grand(true);
    expect(hasFighterWorks(g, 'SWE')).toBe(true);
    // A nation with no province of the factories a fighter wing is built in cannot try, and is told why.
    const none = Object.keys(g.nations).find((n) => n !== 'GER' && g.nations[n].alive && !allied(g, n, 'GER') && g.regions.some((r) => r.ctrl === n) && !hasFighterWorks(g, n))!;
    expect(none).toBeTruthy();
    const x = plant(g, 'Berlin', 'watch', none);
    expect(whyNotMission(g, none, x.id, 'fighterPlans')).toMatch(/has no aircraft works that build fighters: plans would give it nothing to build them in\.$/);
    const bare = g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && g.nations.GER.capital !== r.id && r.fac === 0)!;
    const y = plant(g, bare.name, 'watch', 'SWE');
    expect(whyNotMission(g, 'SWE', y.id, 'fighterPlans')).toBe(`There are no great aircraft works ${inOrAt(bare)}: Germany keeps its plans at Berlin and at its great aircraft works.`);
  });

  it('a mark stolen joins the table, is built once the designers reach its year, and leads the mix as the newest', () => {
    const g = grand(true);
    const mark = fieldedMark(g, 'GER')!;
    expect(mark).toBe('bf109_e');
    // Poland (its own works, two air groups) takes Germany's plans.
    expect(holdsMark(g, 'POL', mark)).toBe(false);
    expect(stealMark(g, 'POL', 'GER', mark)).toBe(true);
    expect(stealMark(g, 'POL', 'GER', mark)).toBe(false);
    expect(stolenMarks(g, 'POL')).toEqual([{ mark, from: 'GER', turn: g.turn }]);
    const f = g.airFleets.find((x) => x.nat === 'POL' && x.ship === undefined)!;
    const mix = typeMix(g, f)!;
    // Built once Poland's designers reach its year, as the newest mark in Poland's table it takes the leading share.
    expect(designYear(g, 'POL', 'fighters')).toBeGreaterThanOrEqual(FIGHTERS.types[mark].year!);
    expect(Object.entries(mix).sort((a, b) => b[1] - a[1])[0][0]).toBe(mark);
    // A mark from a year Poland's designers have not reached waits.
    const later = Object.keys(FIGHTERS.types).find((k) => (FIGHTERS.types[k].year ?? 0) > designYear(g, 'POL', 'fighters') && !holdsMark(g, 'POL', k))!;
    stealMark(g, 'POL', 'UK', later);
    expect(typeMix(g, f)![later]).toBeUndefined();
    // Kept through a save.
    const h = loadGame(saveGame(g))!;
    expect(stolenMarks(h, 'POL').map((x) => x.mark)).toEqual([mark, later]);
    // Holding it, the thief is told so.
    const a = plant(g, 'Berlin', 'watch', 'POL');
    expect(whyNotMission(g, 'POL', a.id, 'fighterPlans')).toBe(`Poland already has the ${FIGHTERS.types[mark].short}.`);
  });

  it('an agent in Berlin gets the plans out sooner or later, and the news says when they can be built', () => {
    const g = grand(true);
    let tries = 0;
    while (!stolenMarks(g, 'SWE').length && tries++ < 40) {
      plant(g, 'Berlin', 'fighterPlans', 'SWE');
      g.intel.heat = {};
      intelTick(g, 'SWE');
      g.turn++;
    }
    expect(stolenMarks(g, 'SWE')[0].from).toBe('GER');
    expect(g.headlines.some((h) => /copies the plans of the German Bf 109 E at Berlin and gets them out\. Your aircraft works can build the Bf 109 E (now|once your designers reach \d{4})\.$/.test(h.text))).toBe(true);
  });

  it('with the switch off, a save is as before: no stolen marks, no new mission offered', () => {
    const g = grand(false);
    expect(saveGame(g)).not.toMatch(/stolenMarks|fighterPlans/);
  });
});
