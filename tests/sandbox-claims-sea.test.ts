// The sandbox's fixes, finding D (after release 1): where anything goes three small claims fired in most wars
// (Argentina for the Falkland Islands in 20 of 24, Guatemala for British Honduras in 19), because a thin garrison on
// ground across the sea, or a quiet month, was reason enough. The design director's sandbox ruling of 20 November
// 2026: a small country marching across the sea for a claim needs the holder hard pressed, not merely at war. With
// the historian (historian/claims-across-the-sea-hard-pressed.md): "hard pressed" is the measure the claims already
// use for those words (the holder's will to fight down by the claims' 'weak' share of what it can bear), and the
// rule reaches ground the claimant must sail to and ground its holder must reinforce by sea, however the claimant
// travels. In the record no small power marched for such ground while its holder could still send a ship. Claims
// over land on a holder's own country are as they were (Peru's war for the Oriente is the record's own).
import { describe, expect, it } from 'vitest';
import { acrossSea, moment } from '../src/campaign/claims';
import { majorsOf } from '../src/campaign/diplomacy';
import { atWar, collapseScore, createGame, setWar } from '../src/campaign/game';
import { HISTORY, setStop, type Ambition } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const grand = (stop: number, seed = 3): Game => { const g = startOpen(createGame(seed, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const claim = (id: string): Ambition => HISTORY.ambitions.find((a) => a.id === id)!;
const ground = (g: Game, a: Ambition) => a.ground.map((nm) => g.byName[nm]);
/** A month late enough that every claimant's own wait is over (history.json, stop 5, 'wait'). */
const LATE = 40;
/** The holder's own country overrun: its capital and its home provinces in an enemy's hands. */
function overrun(g: Game, nat: string, by: string): void {
  if (!atWar(g, nat, by)) setWar(g, nat, by);
  for (const r of g.regions) if (r.owner === nat && !r.overseas) r.ctrl = by;
}
const pressed = (g: Game, nat: string): boolean => collapseScore(g, nat) / g.nations[nat].resilience >= HISTORY.claims.weak;

describe('a small country\'s claim on ground across the sea', () => {
  it('is told from the map: ground the claimant must sail to, or ground overseas from its holder\'s own country', () => {
    const g = grand(5);
    const at = (id: string, sails: boolean): boolean => acrossSea(g, claim(id), ground(g, claim(id)), sails);
    // Overseas from its holder, whoever travels how: the Falklands, British Honduras, Gibraltar, French Morocco, Rhodes.
    for (const id of ['falklands', 'belize', 'gibraltar', 'morocco', 'rhodes', 'dodecanese']) expect(at(id, false)).toBe(true);
    // Ground in the holder's own country, with a land front to it: as it was.
    for (const id of ['oriente', 'vojvodina', 'macedonia', 'vilnius', 'epirus']) expect(at(id, false)).toBe(false);
    // The same ground with no land front to it must be sailed to.
    expect(at('epirus', true)).toBe(true);
    // A great power's claim is its war aim, weighed by its staff: not this rule's.
    for (const a of HISTORY.ambitions) if (majorsOf(g).includes(a.nat)) expect(acrossSea(g, a, ground(g, a), true)).toBe(false);
    // Nothing held, nothing to weigh.
    expect(acrossSea(g, claim('falklands'), [], true)).toBe(false);
  });

  for (const id of ['falklands', 'belize']) {
    const a = claim(id);
    it(`${id}: a holder at war, with a handful of troops on the ground, is no reason to march`, () => {
      const g = grand(5);
      g.turn = LATE;
      if (!atWar(g, 'UK', 'GER')) setWar(g, 'UK', 'GER');
      expect(g.nations[a.nat].atWar).toEqual([]);
      expect(ground(g, a).every((r) => r.owner === 'UK' && r.ctrl === 'UK')).toBe(true);
      expect(pressed(g, 'UK')).toBe(false);
      expect(moment(g, a)).toBeNull();
    });

    it(`${id}: nor is a quiet month with the holder at peace`, () => {
      const g = grand(5);
      g.turn = LATE;
      for (const e of [...g.nations.UK.atWar]) { g.nations.UK.atWar = g.nations.UK.atWar.filter((x) => x !== e); g.nations[e].atWar = g.nations[e].atWar.filter((x) => x !== 'UK'); }
      expect(moment(g, a)).toBeNull();
    });

    it(`${id}: with the holder hard pressed the moment has come`, () => {
      const g = grand(5);
      g.turn = LATE;
      overrun(g, 'UK', 'GER');
      expect(pressed(g, 'UK')).toBe(true);
      expect(moment(g, a)).toMatch(/is hard pressed by Germany/);
    });
  }

  it('a claim over land on a holder\'s own country is weighed as it was: no war needed where anything goes, only the odds and its month', () => {
    const g = grand(5);
    g.turn = LATE;
    const a = claim('oriente');
    expect(g.nations.ECU.atWar).toEqual([]);
    // Ecuador's Oriente stands empty beside Peru's army: the frontier is thinly held.
    for (const r of ground(g, a)) for (const u of r.units) if (u.nat === 'ECU') u.dead = true;
    expect(moment(g, a)).toBe('the frontier is thinly held');
  });

  it('Rhodes: an island held by a handful of troops is no reason for Greece or Turkey to sail, at peace or with Italy at war', () => {
    const g = grand(5);
    g.turn = LATE;
    for (const id of ['rhodes', 'dodecanese']) expect(moment(g, claim(id))).toBeNull();
    if (!atWar(g, 'ITA', 'UK')) setWar(g, 'ITA', 'UK');
    expect(pressed(g, 'ITA')).toBe(false);
    for (const id of ['rhodes', 'dodecanese']) expect(moment(g, claim(id))).toBeNull();
  });

  it('French Morocco: France at war is no reason for Spain; France hard pressed is', () => {
    const g = grand(5);
    g.turn = LATE;
    if (!atWar(g, 'FRA', 'GER')) setWar(g, 'FRA', 'GER');
    expect(pressed(g, 'FRA')).toBe(false);
    expect(moment(g, claim('morocco'))).toBeNull();
    overrun(g, 'FRA', 'GER');
    expect(moment(g, claim('morocco'))).toBe('France is hard pressed by Germany');
  });
});
