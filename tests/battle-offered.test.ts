// The safety net under FIGHT IT YOURSELF (the design director, b418 and b439): a kind of place the field still lands
// more than a tenth off its card is settled by the staff, and the attack card says so in the UI director's words.
import { describe, expect, it } from 'vitest';
import { STAFF_SETTLES, kindsOf, sampleTicket, staffOnly, type PlaceKind } from '../src/battle/land';
import type { Terrain } from '../src/battle/land/field';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';

/** A ticket on this ground, with a river or works if asked, against a defence of this many divisions. */
function at(terrain: Terrain, o: { river?: boolean; fort?: number; divs?: number } = {}) {
  const t = sampleTicket(terrain, 1.3);
  t.place.river = o.river ? 'Meuse' : null;
  t.place.fort = o.fort ?? 0;
  if (o.divs !== undefined) t.def.forces = t.def.forces.slice(0, 1).map((f) => ({ ...f, divs: o.divs! }));
  return t;
}
const list = (...kinds: PlaceKind[]) => kinds.map((kind) => ({ kind }));

describe('the safety net (b439)', () => {
  it('starts empty: every battle is offered on the field today', () => {
    expect(STAFF_SETTLES).toEqual([]);
    for (const g of ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as Terrain[]) expect(staffOnly(at(g, { river: true, fort: 2 }))).toBeNull();
  });
  it('sorts a battle by the defence\'s size, its ground, and a river or works where it has them', () => {
    expect(kindsOf(at('hills', { river: true, divs: 2 }))).toEqual(['smallest', 'hills', 'river']);
    expect(kindsOf(at('plains', { fort: 1, divs: 6 }))).toEqual(['middle', 'plains', 'works']);
    expect(kindsOf(at('urban', { divs: 11 }))).toEqual(['largest', 'urban']);
  });
  it('says one sentence in the UI director\'s words: the river or works, then the ground, then the size', () => {
    expect(staffOnly(at('mountain'), list('mountain'))).toBe("The staff settles every battle in the mountains: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('hills', { river: true }), list('hills', 'river'))).toBe("The staff settles every battle across a river in hill country: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('mountain', { divs: 2 }), list('smallest', 'mountain'))).toBe("The staff settles every battle in the mountains where two divisions or fewer defend: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('plains', { divs: 12 }), list('largest'))).toBe("The staff settles every battle where eleven or more divisions defend: the field cannot fight one at the card's odds.");
    expect(staffOnly(at('desert', { fort: 1 }), list('works'))).toBe("The staff settles every battle against fortress works: the field cannot fight one at the card's odds.");
    // A kind of the list this battle is not: offered.
    expect(staffOnly(at('plains'), list('mountain', 'river'))).toBeNull();
    // No "yet", no "for now", no "there".
    for (const k of ['river', 'works', 'plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert', 'smallest', 'middle', 'largest'] as PlaceKind[]) {
      const t = at(k === 'river' || k === 'works' || k === 'smallest' || k === 'middle' || k === 'largest' ? 'plains' : (k as Terrain), { river: k === 'river', fort: k === 'works' ? 1 : 0, divs: k === 'smallest' ? 2 : k === 'largest' ? 11 : 5 });
      const said = staffOnly(t, list(k))!;
      expect(said).toMatch(/^The staff settles every battle .+: the field cannot fight one at the card's odds\.$/);
      expect(said).not.toMatch(/\byet\b|for now|\bthere\b/);
    }
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
