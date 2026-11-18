// The safety net under FIGHT IT YOURSELF (the design director, b418 and b439): a kind of place the field still lands
// more than a tenth off its card is settled by the staff, and the attack card says so in the UI director's words.
import { describe, expect, it } from 'vitest';
import { STAFF_SETTLES, kindsOf, sampleTicket, staffOnly, type PlaceKind } from '../src/battle/land';
import type { Terrain } from '../src/battle/land/field';
import { attackOutlook } from '../src/campaign/turn';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';

const GROUNDS: Terrain[] = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'];

/**
 * A ticket on this ground, with a river or works if asked, against a defence of this many divisions, at these odds
 * (1.3 where not said). 'dry': the first so many of the attack's three corps come on over dry ground and the rest
 * cross the river (b464(5)); 'seen': the odds the card put to the player where it did not know the defence.
 */
function at(terrain: Terrain, o: { river?: boolean; fort?: number; divs?: number; dry?: number; odds?: number; seen?: number } = {}) {
  const t = sampleTicket(terrain, o.odds ?? 1.3);
  t.place.river = o.river || o.dry ? 'Meuse' : null;
  t.place.fort = o.fort ?? 0;
  if (o.divs !== undefined) t.def.forces = t.def.forces.slice(0, 1).map((f) => ({ ...f, divs: o.divs! }));
  if (o.dry) t.att.forces = t.att.forces.map((f, i) => (i < o.dry! ? { ...f, cross: false } : f));
  if (o.seen !== undefined) t.reckoning.seen = o.seen;
  return t;
}
const list = (...kinds: PlaceKind[]) => kinds.map((kind) => ({ kind }));
// One sentence for a close river attack, split or whole (the UI director, 16:47Z), naming the staff's best guess and
// giving this card's figure (17:05Z).
const CLOSE = (odds = '1.3') => `The staff settles every battle across a river where its best guess of the odds is 1.7 to 1 or less, as here (${odds} to 1): the field cannot fight one at the card's odds.`;
const SPLIT = CLOSE();
const WHOLE = SPLIT;

describe('the safety net (b439)', () => {
  it('lists two kinds for release 1, with their measures: a river part or all of the attack must cross, on a close card (b492(1), b494)', () => {
    expect(STAFF_SETTLES).toEqual([
      { kind: 'river-split-close', measured: '60 captured campaign attacks, 10 fights each, 18 November, with the river hold of b494: 15 points under the card' },
      { kind: 'river-whole-close', measured: '32 captured campaign attacks, 10 fights each, 18 November, with the river hold of b494: 13 points under the card' },
    ]);
    expect(staffOnly(at('forest', { dry: 1 }))).toBe(SPLIT);
    expect(staffOnly(at('plains', { dry: 2, fort: 2, divs: 12 }))).toBe(SPLIT);
    expect(staffOnly(at('forest', { river: true }))).toBe(WHOLE);
    expect(staffOnly(at('plains', { river: true, fort: 2, divs: 12, odds: 0.5 }))).toBe(CLOSE('0.5'));
    // The figure is the one the card showed: its best guess where it did not know the defence, to two places.
    expect(staffOnly(at('forest', { river: true, odds: 2.4, seen: 1.4449 }))).toBe(CLOSE('1.44'));
  });
  it('offers every other battle on the field: river crossings on easier cards, and every ground, works and size', () => {
    for (const g of GROUNDS) {
      for (const odds of [0.5, 1.3, 1.7, 3]) {
        // The whole attack crosses: offered on a card easier than close. No river: offered at any odds.
        expect(staffOnly(at(g, { river: true, fort: 2, odds }))).toBe(odds > 1.7 ? null : CLOSE(String(odds)));
        expect(staffOnly(at(g, { odds }))).toBeNull();
        expect(staffOnly(at(g, { fort: 1, divs: 2, odds }))).toBeNull();
        expect(staffOnly(at(g, { river: true, divs: 12, odds: odds + 1.5 }))).toBeNull();
      }
      // Some cross and some come on dry, on a card easier than close: offered.
      for (const odds of [1.71, 2, 2.6, 6]) for (const dry of [1, 2]) expect(staffOnly(at(g, { dry, odds }))).toBeNull();
      // Every corps on dry ground is no crossing at all (the card lays no river then; a hand-made ticket can).
      expect(staffOnly(at(g, { dry: 3 }))).toBeNull();
    }
    // A corps marked dry with no river before the place: nothing is split.
    const t = at('plains', { dry: 1 });
    t.place.river = null;
    expect(staffOnly(t)).toBeNull();
  });
  it('sorts a battle by the defence\'s size, its ground, and a river or works where it has them', () => {
    expect(kindsOf(at('hills', { river: true, divs: 2 }))).toEqual(['smallest', 'hills', 'river', 'river-whole-close']);
    expect(kindsOf(at('hills', { river: true, divs: 2, odds: 1.71 }))).toEqual(['smallest', 'hills', 'river']);
    expect(kindsOf(at('plains', { fort: 1, divs: 6 }))).toEqual(['middle', 'plains', 'works']);
    expect(kindsOf(at('urban', { divs: 11 }))).toEqual(['largest', 'urban']);
    expect(kindsOf(at('marsh', { dry: 1, fort: 1, divs: 6 }))).toEqual(['middle', 'marsh', 'river', 'river-split-close', 'works']);
    expect(kindsOf(at('marsh', { dry: 2, divs: 6, odds: 2 }))).toEqual(['middle', 'marsh', 'river']);
  });
  it('reads the card\'s odds as the player saw them: to the two places the card prints, and the staff\'s best guess where it did not know the defence', () => {
    const split = (o: { odds?: number; seen?: number }) => kindsOf(at('plains', { dry: 1, ...o })).includes('river-split-close');
    expect(split({ odds: 1.7 })).toBe(true);
    // "Odds 1.70 to 1" on the card: close. "1.71": not.
    expect(split({ odds: 1.7049 })).toBe(true);
    expect(split({ odds: 1.7051 })).toBe(false);
    expect(split({ odds: 1.71 })).toBe(false);
    // A band whose middle the card showed at 1.9 is not a close card, whatever the defence truly is; one at 1.6 is.
    expect(split({ odds: 1.5, seen: 1.9 })).toBe(false);
    expect(split({ odds: 2.4, seen: 1.6 })).toBe(true);
  });
  it('says one sentence in the UI director\'s words: the river or works, then the ground, then the size', () => {
    expect(staffOnly(at('mountain'), list('mountain'))).toBe("The staff settles every battle in the mountains: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('hills', { river: true }), list('hills', 'river'))).toBe("The staff settles every battle across a river in hill country: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('mountain', { divs: 2 }), list('smallest', 'mountain'))).toBe("The staff settles every battle in the mountains where two divisions or fewer defend: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('plains', { divs: 12 }), list('largest'))).toBe("The staff settles every battle where eleven or more divisions defend: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('desert', { fort: 1 }), list('works'))).toBe("The staff settles every battle against fortress works: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('hills', { dry: 1 }), list('river-split-close'))).toBe(SPLIT);
    expect(staffOnly(at('hills', { dry: 1 }), list('hills', 'river-split-close'))).toBe("The staff settles every battle in hill country across a river where its best guess of the odds is 1.7 to 1 or less, as here (1.3 to 1): the field cannot fight one at the card's odds.");
    // A kind of the list this battle is not: offered.
    expect(staffOnly(at('plains'), list('mountain', 'river'))).toBeNull();
    // No "yet", no "for now", no "there".
    for (const k of ['river', 'works', 'plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert', 'smallest', 'middle', 'largest', 'river-split-close', 'river-whole-close'] as PlaceKind[]) {
      const ground = (GROUNDS as string[]).includes(k) ? (k as Terrain) : 'plains';
      const t = at(ground, { river: k === 'river' || k === 'river-whole-close', dry: k === 'river-split-close' ? 1 : 0, fort: k === 'works' ? 1 : 0, divs: k === 'smallest' ? 2 : k === 'largest' ? 11 : 5 });
      const said = staffOnly(t, list(k))!;
      expect(said).toMatch(/^The staff settles every battle .+: the field cannot fight one at the card's odds\.$/);
      expect(said).not.toMatch(/\byet\b|for now|\bthere\b/);
    }
  });
  it('the campaign\'s ticket carries the odds its card showed where the card did not know the defence', () => {
    const g = newCampaign();
    let band = 0, exact = 0;
    for (const o of offers(g)) {
      const look = attackOutlook(g, o.corps, o.target)!;
      expect(o.ticket.reckoning.odds).toBeCloseTo(o.odds, 9);
      if (look.exact) { exact++; expect(o.ticket.reckoning.seen).toBeUndefined(); } else { band++; expect(o.ticket.reckoning.seen).toBe(look.odds); }
    }
    expect(band).toBeGreaterThan(0);
  });
  it('closes the 3D game\'s doorway on a listed kind too: battleLook says why, battleStart opens nothing', () => {
    const g = newCampaign();
    const o = offers(g)[0];
    const ids = o.corps.map((c) => c.id);
    const TC = battleCalls({ game: () => g, attack: () => ({ report: null }) });
    expect(TC.battleLook(ids, o.target.id)!.ticket).not.toBeNull();
    const kind = o.ticket.place.terrain as PlaceKind;
    STAFF_SETTLES.push({ kind, measured: 'a test' });
    try {
      const look = TC.battleLook(ids, o.target.id) as { ticket: unknown; why?: string };
      expect(look.ticket).toBeNull();
      expect(look.why).toMatch(/^The staff settles every battle /);
      expect(TC.battleStart(ids, o.target.id)).toBeNull();
    } finally {
      STAFF_SETTLES.pop();
    }
  });
});
