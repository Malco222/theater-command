// The player's sight handed in, worked out once: the 3D table works out what the player can see once a picture and
// hands it to every rule that asks. Each of them must answer exactly as when it works sight out for itself.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { airfields } from '../src/campaign/airwar';
import { homeAlarms } from '../src/campaign/danger';
import { cutKnown, cutSays, cutsKnown, sightOf } from '../src/campaign/intel';
import { loadGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { airfieldsSeen, prizesOnMap, runTurnAI, siegesInSight } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import brianSave from './ahead/saves/brian-usa-1940-12.json?raw';

function same(g: Game): number {
  const seen = sightOf(g, g.human);
  expect([...cutsKnown(g, g.human, seen)]).toEqual([...cutsKnown(g)]);
  for (const r of g.regions) {
    expect(cutKnown(g, r.id, g.human, seen)).toEqual(cutKnown(g, r.id));
    expect(cutSays(g, r.id, g.human, seen)).toEqual(cutSays(g, r.id));
  }
  expect(siegesInSight(g, seen)).toEqual(siegesInSight(g));
  expect(prizesOnMap(g, seen)).toEqual(prizesOnMap(g));
  expect(airfieldsSeen(g, seen)).toEqual(airfieldsSeen(g));
  expect(airfields(g, g.human, seen)).toEqual(airfields(g, g.human));
  expect(homeAlarms(g, g.human, seen)).toEqual(homeAlarms(g, g.human));
  return cutsKnown(g).size + siegesInSight(g).length + prizesOnMap(g).length + homeAlarms(g).length;
}

describe('sight handed in answers as sight worked out', () => {
  it('on Brian\'s December 1940 save, and for three months after', () => {
    const g = loadGame(brianSave)!;
    let found = same(g);
    for (let m = 0; m < 3; m++) { runTurnAI(g); found += same(g); }
    // Something to see: cuts, sieges, prizes or alarms, not an empty map only.
    expect(found).toBeGreaterThan(0);
  });

  it('on a grand war with Poland at the table, through its first months', () => {
    const g = startOpen(createGame(5, 'POL'));
    let found = same(g);
    for (let m = 0; m < 3; m++) { runTurnAI(g); found += same(g); }
    expect(found).toBeGreaterThan(0);
  });
});
