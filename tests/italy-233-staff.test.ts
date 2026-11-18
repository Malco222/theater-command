// Ruling 233, riders (j) and (k) (the design director's b506; the grand campaign, computer staffs only).
// (j) History's list of shores has an order: when the month comes for a later shore of the list, a staff's home army
//     turns to the enemy who holds it nearest the home ports, though its first enemy across the sea still stands.
//     Britain turns to France in June 1944 while Italy fights on. (Before it a staff kept one enemy across the sea
//     for as long as his coast was in reach: in the wars where Italy held out, Britain never landed in France.)
// (k) The computer's Italian fleet keeps its harbours once Mussolini has fallen, and comes out against no landing.
import { describe, expect, it } from 'vitest';
import fleetData from '../data/campaign/fleets.json';
import { createGame, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { atSea, fleetsOf, keepsHarbour, navalAI, sortieAgainst } from '../src/campaign/navy';
import { lodgment, seaFoe, startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const FRANCE_44 = 57;
/** A grand war nobody plays: Britain at war with Germany and Italy, France fallen, and Britain's staff set on Italy since last year. */
const war = (turn: number, stop = 3, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
  setWar(g, 'GER', 'UK');
  setWar(g, 'ITA', 'UK');
  for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas) r.ctrl = 'GER';
  g.turn = turn;
  const ids = Object.keys(g.nations);
  g.flags['st:foe:UK'] = ids.indexOf('ITA') + 1;
  g.flags['st:foeAt:UK'] = turn - 2;
  for (const e of ['GER', 'ITA']) { g.flags[`war:UK:${e}`] = 1; g.nations.UK.lastFight[e] = 1; g.nations[e].lastFight.UK = 1; }
  return g;
};

describe('history\'s list has an order (233(j))', () => {
  it('Britain\'s home army keeps Italy until France\'s month comes, and then turns to Germany on the Channel', () => {
    const before = war(FRANCE_44 - 1);
    expect(seaFoe(before, 'UK')?.foe).toBe('ITA');
    const g = war(FRANCE_44);
    expect(seaFoe(g, 'UK')).toMatchObject({ foe: 'GER', second: true });
    // The choice stands like any other: it is kept the month after, and Italy does not win it back.
    g.turn++;
    expect(seaFoe(g, 'UK')?.foe).toBe('GER');
  });

  it('to the enemy who holds that shore nearest home: Germany on the Channel, not Italy at Marseille', () => {
    const g = war(FRANCE_44);
    g.byName.Marseille.ctrl = 'ITA';
    expect(seaFoe(g, 'UK')?.foe).toBe('GER');
  });

  it('not with men ashore who still have a front to hold, and not at the stops where the list is put away', () => {
    for (const stop of [4, 5]) expect(seaFoe(war(FRANCE_44, stop), 'UK')?.foe, `stop ${stop}`).toBe('ITA');
    for (const stop of [1, 2]) expect(seaFoe(war(FRANCE_44 + 2, stop), 'UK')?.foe, `stop ${stop}`).toBe('GER');
    const g = war(FRANCE_44);
    // A British corps ashore in a harbour of the Italian mainland, with the enemy next door.
    const at = ['Naples', 'Apulia', 'Calabria'].map((nm) => g.byName[nm]).find((r) => r.port)!;
    const c = g.corps.find((x) => x.nat === 'UK' && !x.dead && x.kind === 'inf')!;
    const from = g.regions[c.reg];
    from.units.splice(from.units.indexOf(c), 1);
    c.reg = at.id;
    at.units = [c];
    at.ctrl = 'UK';
    g.flags['st:lodged:UK'] = at.id + 1;
    expect(lodgment(g, 'UK')?.at).toBe(at);
    expect(seaFoe(g, 'UK')?.foe).toBe('ITA');
  });
});

describe('the Italian fleet keeps its harbours once Mussolini has fallen (233(k))', () => {
  it('is in the data, for the computer\'s Italy alone', () => {
    expect(fleetData.harbour).toEqual({ ITA: 'event:mussolini-falls' });
    const g = war(48);
    expect(keepsHarbour(g, 'ITA')).toBe(false);
    g.flags['event:mussolini-falls'] = 48;
    expect(keepsHarbour(g, 'ITA')).toBe(true);
    expect(keepsHarbour(g, 'GER')).toBe(false);
    // A player's Italy sails as he likes, and no history campaign is touched.
    const mine = war(48, 3, 'ITA');
    mine.flags['event:mussolini-falls'] = 48;
    expect(keepsHarbour(mine, 'ITA')).toBe(false);
    const old = createGame(1, null);
    old.flags['event:mussolini-falls'] = 48;
    expect(keepsHarbour(old, 'ITA')).toBe(false);
  });

  it('its surface fleets lie in harbour and come out against no landing; its submarines keep the sea', () => {
    const g = war(48);
    navalAI(g, 'ITA');
    const surface = fleetsOf(g, 'ITA').filter((f) => f.duty !== 'raid');
    expect(surface.length).toBeGreaterThan(0);
    expect(surface.some(atSea)).toBe(true);
    const seas = [...new Set(surface.flatMap((f) => (f.base >= 0 ? g.regions[f.base].seas : [f.home])))];
    for (const f of fleetsOf(g, 'ITA')) f.stance = 'port';
    const out = sortieAgainst(g, 'UK', seas);
    expect(out).toBeGreaterThan(0);
    g.flags['event:mussolini-falls'] = 48;
    expect(sortieAgainst(g, 'UK', seas)).toBeLessThan(out);
    navalAI(g, 'ITA');
    expect(surface.some(atSea)).toBe(false);
    expect(fleetsOf(g, 'ITA').filter((f) => f.duty === 'raid').length).toBeGreaterThan(0);
  });
});
