// The sandbox at sea as pages (Brian, 19 November 2026, "yes do all 3"): a named ship's card with her battle stars,
// the memorial, the carrier decks as cards of squadrons, and the drawing board as a blueprint whose outline follows
// the dials. The pages are the rules' own (seapages.ts, blueprint.ts), so both games show the same thing; with the
// switch off there is none of them.
import { describe, expect, it } from 'vitest';
import { airPool } from '../src/campaign/air';
import { SHEET, drawShip, drawSub, type Blueprint } from '../src/campaign/blueprint';
import { syncWings } from '../src/campaign/carriers';
import { createGame } from '../src/campaign/game';
import { atSea, fleetsOf, sail } from '../src/campaign/navy';
import { PRESETS, shipsMonth, type Dials, type SubDials } from '../src/campaign/sandbox';
import { SANDBOX_SHIPS } from '../src/campaign/sandbox-data';
import { fleetExtras, memorialDoor, seaAct, seaDoors, seaPage, type Page } from '../src/campaign/seapages';
import { startOpen } from '../src/campaign/staff';
import { techOf } from '../src/campaign/tech';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, ShipHull } from '../src/campaign/types';

/** A grand campaign led by Britain, in its own month, the sandbox on (or off), at war once Germany is. */
function britain(seed = 3, on = true): Game {
  const g = startOpen(createGame(seed, 'UK'));
  if (on) g.flags.sandbox = 1;
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
/** The Home Fleet in its harbour with four carriers and their wings aboard. */
function homeFleet(g: Game): Fleet {
  const f = named(g, 'Home Fleet');
  f.ships.cv = 4;
  sail(g, f, f.home, 'port');
  expect(atSea(f)).toBe(false);
  syncWings(g);
  return f;
}
const learn = (g: Game, nat: string, ...ids: string[]): void => { const t = techOf(g, nat)!; for (const id of ids) if (!t.done.includes(id)) t.done.push(id); };
const hull = (over: Partial<ShipHull>): ShipHull => ({ cls: 'raider', kind: 'ca', label: 'Raider cruiser', name: 'Test', n: 1, born: 1, power: 3.5, tough: 0.8, escort: 0.3, raid: 3, ...over });
const page = (g: Game, id: string): Page => { const p = seaPage(g, id); expect(p, id).not.toBeNull(); return p!; };
const hullOutline = (b: Blueprint): number[] => b.strokes.find((s) => s.ink === 'hull')!.pts;
const words = (() => {
  const b = SANDBOX_SHIPS.board, l = (x: { label: string }[]): string[] => x.map((s) => s.label);
  return { ship: { hull: l(b.hull), guns: l(b.guns), armour: l(b.armour), speed: l(b.speed), aa: l(b.aa), air: l(b.air) }, sub: { size: l(b.sub.size), tubes: l(b.sub.tubes), range: l(b.sub.range), hangar: l(b.sub.hangar) } };
})();

describe('with the switch off', () => {
  it('there is no page, no door, no action and nothing added to a fleet\'s page', () => {
    const g = britain(3, false);
    const f = homeFleet(g);
    for (const id of ['memorial', 'board', 'board:s:2.3.3.2.1.1.1', `deck:${f.id}`]) expect(seaPage(g, id)).toBeNull();
    expect(seaDoors(g)).toEqual([]);
    expect(memorialDoor(g)).toBeNull();
    expect(fleetExtras(g, f.id)).toBeNull();
    expect(seaAct(g, `deckplan:${f.id}:4:2:2`)).toBeNull();
    expect(seaAct(g, 'boardsign:s:2.3.3.2.1.1.1')).toBeNull();
    expect(f.deckPlan).toBeUndefined();
  });
});

describe('the blueprint', () => {
  const inside = (b: Blueprint): void => {
    for (const s of b.strokes) for (let i = 0; i + 1 < s.pts.length; i += 2) {
      expect(s.pts[i]).toBeGreaterThanOrEqual(0); expect(s.pts[i]).toBeLessThanOrEqual(SHEET.w);
      expect(s.pts[i + 1]).toBeGreaterThanOrEqual(0); expect(s.pts[i + 1]).toBeLessThanOrEqual(SHEET.h);
    }
    for (const m of b.marks) { expect(m.x).toBeGreaterThanOrEqual(0); expect(m.x).toBeLessThanOrEqual(SHEET.w); expect(m.y).toBeLessThan(SHEET.h); }
  };

  it('draws every class the board starts from on the sheet, with a word for each dial', () => {
    for (const [k, d] of Object.entries(PRESETS)) {
      const b = 'size' in d ? drawSub(d as SubDials, words.sub, k, 'foot') : drawShip(d as Dials, words.ship, k, 'foot');
      inside(b);
      expect(b.strokes.filter((s) => s.ink === 'hull')).toHaveLength(1);
      expect(b.marks.some((m) => m.big && m.text === k)).toBe(true);
    }
  });

  it('follows each dial: a longer hull, more turrets, a funnel more for speed, a flight deck, a hatched belt', () => {
    const base: Dials = { ...(PRESETS.bb as Dials) };
    const draw = (d: Partial<Dials>): Blueprint => drawShip({ ...base, ...d }, words.ship, 'X', 'foot');
    const span = (b: Blueprint): number => { const p = hullOutline(b), xs = p.filter((_, i) => i % 2 === 0); return Math.max(...xs) - Math.min(...xs); };
    expect(span(draw({ hull: 3 }))).toBeGreaterThan(span(draw({ hull: 2 })));
    expect(span(draw({ hull: 1 }))).toBeLessThan(span(draw({ hull: 2 })));
    const parts = (b: Blueprint): number => b.strokes.filter((s) => s.ink === 'part').length;
    expect(parts(draw({ turrets: 4 }))).toBeGreaterThan(parts(draw({ turrets: 2 })));
    expect(parts(draw({ speed: 2 }))).toBeGreaterThan(parts(draw({ speed: 0 })));
    const fine = (b: Blueprint): number => b.strokes.filter((s) => s.ink === 'fine').length;
    expect(fine(draw({ armour: 2 }))).toBeGreaterThan(fine(draw({ armour: 0 })));
    expect(draw({ air: 3, guns: 0, turrets: 0 }).marks.some((m) => m.text === 'A FULL FLIGHT DECK')).toBe(true);
    expect(draw({ air: 0 }).marks.some((m) => /FLIGHT DECK|CATAPULT/.test(m.text))).toBe(false);
    expect(draw({ guns: 4, turrets: 3 }).marks.some((m) => m.text === 'MAIN GUNS · THREE TURRETS · VERY HEAVY')).toBe(true);
    const sub = (d: Partial<SubDials>): Blueprint => drawSub({ ...(PRESETS.scoutsub as SubDials), ...d }, words.sub, 'X', 'foot');
    expect(sub({ hangar: 0 }).marks.some((m) => m.text === 'A SEAPLANE HANGAR')).toBe(false);
    expect(sub({ hangar: 1 }).marks.some((m) => m.text === 'A SEAPLANE HANGAR')).toBe(true);
  });

  it('keeps the callouts of one margin from running into each other', () => {
    for (const d of Object.values(PRESETS)) {
      const b = 'size' in d ? drawSub(d as SubDials, words.sub, 'X', 'foot') : drawShip(d as Dials, words.ship, 'X', 'foot');
      const row = b.marks.filter((m) => !m.big && m.y < 40).sort((a, c) => a.x - c.x);
      for (let i = 1; i < row.length; i++) expect(row[i].x).toBeGreaterThanOrEqual(row[i - 1].x + row[i - 1].text.length * 6.6);
    }
  });
});

describe('the drawing board as a page', () => {
  it('opens at a battleship, with its blueprint, its seven dials and a step button for each', () => {
    const g = britain();
    const p = page(g, 'board');
    expect(p.id).toBe('board:s:2.3.3.2.1.1.1');
    expect(p.sheet?.strokes.length).toBeGreaterThan(20);
    const dials = p.sections.find((s) => s.heading === 'THE DIALS')!.rows;
    expect(dials).toHaveLength(7);
    // Each step opens the board at that step; the one it stands at is marked and cannot be pressed.
    const guns = dials[1];
    expect(guns.title).toBe('Main guns: Heavy');
    expect(guns.buttons.map((b) => b.act)).toEqual([0, 1, 2, 3, 4].map((i) => `open:board:s:2.${i}.3.2.1.1.1`));
    expect(guns.buttons[3]).toMatchObject({ ok: false, label: '● HEAVY' });
    // A turn of a dial is another page with another drawing.
    const longer = page(g, 'board:s:3.3.3.2.1.1.1');
    expect(hullOutline(longer.sheet!)).not.toEqual(hullOutline(p.sheet!));
    expect(page(g, 'board:u:2.1.2.1').sheet!.marks.some((m) => m.text === 'A SEAPLANE HANGAR')).toBe(true);
    expect(seaPage(g, 'board:s:2.3.3')).toBeNull();
    expect(seaPage(g, 'board:x:1.1.1.1')).toBeNull();
  });

  it('says why a design cannot be signed, and signs one that can: it joins the navy\'s designs', () => {
    const g = britain();
    g.nations.UK.pp = 5000;
    const tooBig = page(g, 'board:s:3.4.4.2.1.1.1');
    expect(tooBig.top[0].ok).toBe(false);
    expect(tooBig.headline).toMatch(/Super battleships|will not fit/);
    const p = page(g, 'board:s:1.2.4.0.2.0.1');
    expect(p.top[0]).toMatchObject({ label: 'SIGN FOR HER', act: 'boardsign:s:1.2.4.0.2.0.1', ok: true });
    const r = seaAct(g, p.top[0].act)!;
    expect(r.ok).toBe(true);
    expect(r.note).toMatch(/^Signed: Design 1, a cruiser/);
    const again = page(g, 'board:s:1.2.4.0.2.0.1');
    const mine = again.sections.find((s) => s.heading === 'THE NAVY\'S DESIGNS')!.rows;
    expect(mine.map((x) => x.title)).toEqual(['Design 1']);
    expect(mine[0].buttons[0].act).toBe('open:board:s:1.2.4.0.2.0.1');
  });

  it('is a door in the list of rooms, and the memorial joins it once a ship is lost', () => {
    const g = britain();
    expect(seaDoors(g).map((d) => d.id)).toEqual(['board']);
    g.nations.UK.memorial = [{ name: 'Thunderer', label: 'Battle carrier', kind: 'cv', turn: g.turn, sea: 0, stars: 2, how: 'in a fleet action' }];
    expect(seaDoors(g).map((d) => d.id)).toEqual(['board', 'memorial']);
  });
});

describe('the carrier decks as cards', () => {
  it('lays the squadrons out in their lanes, with the reserve and every drag the cards allow', () => {
    const g = britain();
    const f = homeFleet(g);
    const p = page(g, `deck:${f.id}`);
    const d = p.deck!;
    expect(d.lanes.map((l) => l.role)).toEqual(['ftr', 'dive', 'torp']);
    expect(d.lanes.reduce((s, l) => s + l.n, 0)).toBe(d.slots);
    expect(d.empty).toBe(0);
    expect(d.changed).toBe(false);
    expect(d.why).toBe('This is the plan the decks fly now.');
    const [ftr, dive, torp] = d.lanes.map((l) => l.n);
    // A fighter squadron dragged to the torpedo lane, or off to the reserve.
    expect(d.moves['ftr>torp']).toBe(`deck:${f.id}:${ftr - 1}.${dive}.${torp + 1}`);
    expect(d.moves['ftr>res']).toBe(`deck:${f.id}:${ftr - 1}.${dive}.${torp}`);
    // The decks are full: nothing comes from the reserve until a place is freed; a fighter never goes in a bomber's lane.
    expect(d.moves['res-ftr>ftr']).toBeUndefined();
    expect(d.refused['res-ftr>ftr']).toMatch(/full/);
    expect(d.refused['res-ftr>torp']).toMatch(/fighters' lane/);
    expect(p.top[0]).toMatchObject({ label: 'SIGN THE DECK PLAN', ok: false });
  });

  it('a draft with a place free asks for it to be filled; filled, it is signed and the aircraft move', () => {
    const g = britain();
    const f = homeFleet(g);
    const d0 = page(g, `deck:${f.id}`).deck!;
    const freed = page(g, d0.moves['ftr>res']);
    expect(freed.deck!.empty).toBe(1);
    expect(freed.deck!.why).toMatch(/1 place on the decks stands empty/);
    expect(freed.deck!.moves['res-bmr>torp']).toBeDefined();
    const draft = page(g, freed.deck!.moves['res-bmr>torp']);
    expect(draft.deck!.changed).toBe(true);
    expect(draft.deck!.why).toBe('');
    expect(draft.sections[0].rows[0].lines.join(' ')).toMatch(/Against battleships: \d+% harder than the decks as they are/);
    const sign = draft.top[0];
    expect(sign.ok).toBe(true);
    const pool0 = airPool(g, 'UK').ftr;
    const r = seaAct(g, sign.act)!;
    expect(r.ok).toBe(true);
    expect(r.open).toBe(`deck:${f.id}`);
    expect(f.deckPlan).toBeDefined();
    // The fighters the decks no longer need went home to the reserve.
    expect(airPool(g, 'UK').ftr).toBeGreaterThan(pool0);
    const after = page(g, `deck:${f.id}`).deck!;
    expect(after.lanes.map((l) => l.n)).toEqual(draft.deck!.lanes.map((l) => l.n));
  });

  it('at sea the decks are shown but cannot be changed over', () => {
    const g = britain();
    const f = homeFleet(g);
    const d0 = page(g, `deck:${f.id}`).deck!;
    sail(g, f, f.zone >= 0 ? f.zone : 0, 'patrol');
    const draft = page(g, d0.moves['ftr>torp']);
    expect(draft.deck!.why).toMatch(/at sea/);
    expect(draft.top[0].ok).toBe(false);
    expect(seaAct(g, draft.top[0].act)!.ok).toBe(false);
  });
});

describe('named ships on their fleet\'s page, and the memorial', () => {
  it('each named ship has a row with her battle stars and her refits; the standard ships one each kind', () => {
    const g = britain();
    learn(g, 'UK', 'radar');
    const f = fleetsOf(g, 'UK').find((x) => x.ships.ca > 0)!;
    f.hulls = [hull({ name: 'Swiftsure', stars: 3, refits: ['aa'] })];
    const x = fleetExtras(g, f.id)!;
    const row = x.rows.find((r) => r.title === 'Swiftsure')!;
    expect(row.stars).toBe(3);
    expect(row.lines.join(' ')).toMatch(/3 battle stars/);
    expect(row.lines.join(' ')).toMatch(/Refitted: more anti-aircraft guns/);
    expect(row.buttons.map((b) => b.act)).toContain(`shiprefit:${f.id}:Swiftsure:radar`);
    expect(x.rows.some((r) => /of the standard pattern$/.test(r.title))).toBe(true);
  });

  it('a ship that has come through five battles gets her crew\'s name, on her card and in the news', () => {
    const g = britain();
    const f = fleetsOf(g, 'UK').find((x) => x.ships.ca > 0)!;
    f.hulls = [hull({ name: 'Swiftsure', stars: 5 }), hull({ name: 'Tenacious', stars: 4 })];
    const h0 = g.headlines.length;
    shipsMonth(g, 'UK');
    const [five, four] = f.hulls!;
    expect(five.nick).toMatch(/^(Old|the Lucky) /);
    expect(four.nick).toBeUndefined();
    expect(g.headlines.slice(h0).map((h) => h.text).join(' ')).toContain(`the crew of the Swiftsure have a name for her: "${five.nick}"`);
    expect(fleetExtras(g, f.id)!.rows[0].title).toBe(`Swiftsure, \u201C${five.nick}\u201D`);
    // Once given, it is hers for good.
    const nick = five.nick;
    five.stars = 9;
    shipsMonth(g, 'UK');
    expect(five.nick).toBe(nick);
  });

  it('a carrier fleet\'s page opens its decks', () => {
    const g = britain();
    g.flags.carriers = 1;
    const f = homeFleet(g);
    expect(fleetExtras(g, f.id)!.top).toEqual([expect.objectContaining({ label: 'THE DECKS', act: `open:deck:${f.id}` })]);
  });

  it('the memorial lists the lost year by year, with their stars and where and how they went down', () => {
    const g = britain();
    g.nations.UK.memorial = [
      { name: 'Thunderer', label: 'Battle carrier', kind: 'cv', turn: 30, sea: 0, stars: 2, how: 'in a fleet action' },
      { name: 'Swiftsure', label: 'Raider cruiser', kind: 'ca', turn: 3, sea: -1, stars: 0, how: 'to air attack' },
    ];
    const p = page(g, 'memorial');
    expect(p.headline).toBe('2 ships lost so far.');
    expect(p.sections.map((s) => s.heading)).toEqual(['1939', '1942']);
    const t = p.sections[1].rows[0];
    expect(t).toMatchObject({ title: 'Thunderer', stars: 2 });
    expect(t.lines[0]).toMatch(/^Battle carrier\. Lost in a fleet action in the .+, February 1942\.$/);
    expect(p.note).toMatch(/2 battle stars/);
  });
});
