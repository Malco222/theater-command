// Corps inside a town say why they take no marching orders, in one sentence for both games (Version 84.2.4; the
// follow-up to Brian's find of 2026-11-17, "if an enemy army is out of a city i couldnt attack them"). A garrison
// never has a move, so nothing it is pointed at can be ordered; before this version the rules had no sentence for
// it, each interface made its own, and planMarch promised such a corps a march "from next month" that could never
// set off (the 3D builder's find). Now whyNotOrder carries the sentence on the corps' card, whyNotOrderGarrison
// gives it for a province, and a kept march is refused with it. The cure is named: TAKE THE FIELD, or a sortie
// where an enemy army stands round the town.
import { describe, expect, it } from 'vitest';
import { createGame, hostileUnits, relocate } from '../src/campaign/game';
import { orderMarch, planMarch, whyNotMarch } from '../src/campaign/marches';
import { garrison, garrisonOf, roomIn } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import { beginHuman, corpsDetail, garrisonLine, humanUp, myGarrison, orderTakeField, stepAI, whyNotOrder, whyNotOrderGarrison } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const goes = (c: Corps): boolean => c.nat === 'GER' && !c.dead && c.kind === 'inf' && c.joint === undefined && c.town === undefined;
/** A German province whose town has room for two corps and nobody inside it yet, with two infantry corps in its field (the second is brought up from another province). */
function quiet(g: Game): { r: Region; cs: Corps[] } {
  const r = g.regions.find((x) => x.ctrl === 'GER' && !garrisonOf(x).length && roomIn(g, x) >= 2 && x.units.filter(goes).length >= 1 && !hostileUnits(g, x, 'GER').length)!;
  expect(r).toBeDefined();
  const second = g.corps.find((c) => goes(c) && c.reg !== r.id)!;
  expect(second).toBeDefined();
  if (r.units.filter(goes).length < 2) relocate(g, second, r);
  return { r, cs: r.units.filter(goes).slice(0, 2) };
}
/** A German province some way off that a march could be kept to. */
const far = (g: Game, r: Region): Region => g.regions.find((x) => x.ctrl === 'GER' && x.id !== r.id && !r.nb.includes(x.id) && planMarch(g, r.units.find(goes)!, x.id).ok)!;

describe('corps inside a town say why they take no marching orders (Version 84.2.4)', () => {
  it('one corps inside is named, on its card and for its province; a second makes them "They"', () => {
    const g = playing();
    const { r, cs } = quiet(g);
    const [a, b] = cs;
    const town = townName(r);
    expect(whyNotOrder(g, a)).toBeNull();
    expect(whyNotOrderGarrison(g, r.id)).toBeNull();
    expect(garrison(g, a)).toBe(true);
    const alone = `${a.name} is the garrison of ${town}. Order it to take the field first.`;
    expect(whyNotOrder(g, a)).toBe(alone);
    expect(corpsDetail(g, a).noOrders).toBe(alone);
    expect(whyNotOrderGarrison(g, r.id)).toBe(alone);
    expect(garrisonLine(g, [a])).toBe(alone);
    // The corps still in the field takes orders as before.
    expect(whyNotOrder(g, b)).toBeNull();
    expect(corpsDetail(g, b).noOrders).toBeNull();
    expect(garrison(g, b)).toBe(true);
    expect(myGarrison(g, r.id).length).toBe(2);
    expect(whyNotOrder(g, a)).toBe(`${a.name} is in the garrison of ${town}. Order it to take the field first.`);
    expect(whyNotOrder(g, b)).toBe(`${b.name} is in the garrison of ${town}. Order it to take the field first.`);
    expect(whyNotOrderGarrison(g, r.id)).toBe(`They are the garrison of ${town}. Order them to take the field first.`);
    // A province with nobody of his inside has no such sentence, and nor has somebody else's corps.
    const other = g.regions.find((x) => x.ctrl === 'GER' && !garrisonOf(x).length)!;
    expect(whyNotOrderGarrison(g, other.id)).toBeNull();
    const theirs = g.corps.find((c) => c.nat === 'POL' && !c.dead)!;
    expect(whyNotOrder(g, theirs)).toBeNull();
  });

  it('no march is kept for a corps inside a town: it is told to take the field, and once out it marches', () => {
    const g = playing();
    const { r, cs } = quiet(g);
    const [a] = cs;
    const to = far(g, r);
    expect(to).toBeDefined();
    expect(planMarch(g, a, to.id).ok).toBe(true);
    expect(garrison(g, a)).toBe(true);
    expect(a.ap).toBe(0);
    const said = whyNotOrder(g, a)!;
    expect(said).toMatch(/ is the garrison of .+\. Order it to take the field first\.$/);
    expect(whyNotMarch(g, a, to.id)).toBe(said);
    const plan = planMarch(g, a, to.id);
    expect(plan.ok).toBe(false);
    expect(plan.line).toBe(said);
    expect(plan.line).not.toMatch(/marches here/);
    expect(orderMarch(g, a, to.id)).toEqual({ kept: false, moved: 0, months: 0, why: said });
    expect(Object.keys(g.flags).some((k) => k === `march:${a.id}`)).toBe(false);
    // Out of the town it is an army again: the sentence is gone and the march can be ordered.
    expect(orderTakeField(g, a)).toBe(true);
    expect(whyNotOrder(g, a)).toBeNull();
    expect(planMarch(g, a, to.id).ok).toBe(true);
  });

  it('with an enemy army round the town the cure it names is the sortie', () => {
    const g = playing();
    const { r, cs } = quiet(g);
    for (const c of cs) expect(garrison(g, c), c.name).toBe(true);
    // The rest of the field goes away and a Polish army is put in the country round the town.
    const away = g.regions.find((x) => x.ctrl === 'GER' && x.id !== r.id && !r.nb.includes(x.id))!;
    for (const u of [...r.units]) relocate(g, u, away);
    const foe = g.regions.find((x) => x.ctrl === 'POL' && x.units.some((u) => u.nat === 'POL' && u.kind === 'inf'))!;
    for (const u of [...foe.units]) relocate(g, u, r);
    expect(hostileUnits(g, r, 'GER').length).toBeGreaterThan(0);
    const town = townName(r);
    expect(whyNotOrderGarrison(g, r.id)).toBe(`They are the garrison of ${town}, shut in by the army round it. Order a sortie against that army.`);
    expect(whyNotOrder(g, cs[0])).toBe(`${cs[0].name} is in the garrison of ${town}, shut in by the army round it. Order a sortie against that army.`);
    expect(whyNotMarch(g, cs[0], away.id)).toBe(whyNotOrder(g, cs[0]));
  });
});
