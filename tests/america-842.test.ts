// America's air force on land (Version 84.2; rulings 133(b), 175(a), 176(a)). On Brian's words of 2026-11-17, playing
// America: "it says i have planes at an airfield but none are there". The United States had no line in air.json: its
// only aircraft were its carriers' wings. Now its home commands stand on their fields from the first day of a grand
// campaign, the air forces it raised against Germany form at home from January 1942 on history's schedule, and the
// groups that follow its army across the ocean go by sea, one a month: its staff's rule, and the player's order too.
// Its air forces in the Pacific wait for the Pacific. The history campaigns are untouched: those fields are not on
// their map.
import { describe, expect, it } from 'vitest';
import { AIR, AIRWAR, SHIP_AIR_ONE, SHIP_AIR_ROOM, airFleetsOf, airUpkeep, fieldClass, homeCommand, km, overOcean, whyNotMoveAir } from '../src/campaign/air';
import { airAct, airPage } from '../src/campaign/airpages';
import { createGame, setWar } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { airGroupBases, beginHuman, endHuman, humanUp, myAirGroups, orderAirBase, stepAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';

function seat(nat: string, seed = 3): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
const land = (g: Game): AirFleet[] => airFleetsOf(g, 'USA').filter((f) => f.ship === undefined);
const at = (g: Game, place: string): AirFleet[] => land(g).filter((f) => f.base === g.byName[place].id);
const sum = (fs: AirFleet[], k: 'ftr' | 'bmr'): number => fs.reduce((s, f) => s + f[k], 0);
/** America in the war beside Britain, as from December 1941. */
function atWar(g: Game): void {
  setWar(g, 'USA', 'GER');
  g.nations.USA.align = 'allies';
}

describe("America's home commands stand on their fields from the first day", () => {
  it('about 500 first-line aircraft on five fields, as air groups of one kind, beside the carriers wings', () => {
    const g = seat('USA');
    expect(sum(land(g), 'ftr')).toBe(240);
    expect(sum(land(g), 'bmr')).toBe(270);
    // Where each stands: fighters at New York, bombers at Norfolk, both in Louisiana, at Los Angeles and on the Canal.
    expect([sum(at(g, 'New York'), 'ftr'), sum(at(g, 'New York'), 'bmr')]).toEqual([80, 0]);
    expect([sum(at(g, 'Norfolk'), 'ftr'), sum(at(g, 'Norfolk'), 'bmr')]).toEqual([0, 80]);
    expect([sum(at(g, 'Louisiana'), 'ftr'), sum(at(g, 'Louisiana'), 'bmr')]).toEqual([40, 80]);
    expect([sum(at(g, 'Los Angeles'), 'ftr'), sum(at(g, 'Los Angeles'), 'bmr')]).toEqual([80, 80]);
    expect([sum(at(g, 'Canal Zone'), 'ftr'), sum(at(g, 'Canal Zone'), 'bmr')]).toEqual([40, 30]);
    // Every one is a group of one kind, none larger than a group may be, and each field is an air base.
    for (const f of land(g)) {
      expect(f.kind === 'ftr' ? f.bmr : f.ftr, f.name).toBe(0);
      expect(f.ftr + f.bmr).toBeLessThanOrEqual(AIRWAR.groups.most);
      expect(fieldClass(g, f.base)).toBe(AIRWAR.fields.seed.base);
      expect(homeCommand(f), `${f.name} guards its own country`).toBe(true);
    }
    // A command that is one group keeps its own name; one of both kinds is named as America named its own of that
    // strength (ruling 220): forty pursuit planes a squadron, eighty bombers a group.
    expect(at(g, 'New York').map((f) => f.name)).toEqual(['First Air Force']);
    expect(at(g, 'Norfolk').map((f) => f.name)).toEqual(['I Bomber Command']);
    expect(at(g, 'Louisiana').map((f) => f.name).sort()).toEqual(['1st Bombardment Group', '1st Pursuit Squadron']);
    // The carriers' wings are still aboard, and no group is listed at a field where none stands.
    const rows = myAirGroups(g);
    expect(rows.filter((r) => r.base === -1).length).toBe(2);
    for (const r of rows.filter((x) => x.base >= 0)) expect(land(g).some((f) => f.id === r.id && f.base === r.base)).toBe(true);
    // Its bombers reach 1,100 km (ruling 133(b)).
    expect(AIR.nations.USA.bmrRange).toBe(1100);
  });

  it('the history campaigns have no such fields, and no American air force forms in them', () => {
    const g = createGame(1, null);
    expect(airFleetsOf(g, 'USA')).toEqual([]);
    g.turn = 60;
    if (g.nations.USA) airUpkeep(g, 'USA');
    expect(airFleetsOf(g, 'USA')).toEqual([]);
  });

  it('a campaign saved before America had an air force has it raised on loading, once', () => {
    const g = seat('USA');
    const before = land(g).length;
    // The save as Version 84.1 wrote it: no land groups, no mark that they were raised.
    g.airFleets = g.airFleets.filter((f) => f.nat !== 'USA' || f.ship !== undefined);
    delete g.flags.worldair;
    const old = loadGame(saveGame(g))!;
    expect(land(old).length).toBe(before);
    expect(sum(land(old), 'ftr')).toBe(240);
    expect(old.flags.worldair).toBe(1);
    // A player who has since disbanded or lost them does not get them again.
    old.airFleets = old.airFleets.filter((f) => f.nat !== 'USA' || f.ship !== undefined);
    expect(land(loadGame(saveGame(old))!).length).toBe(0);
  });
});

describe('the air forces raised against Germany form on history\'s schedule', () => {
  it('the Eighth Air Force forms in Georgia in June 1942, at part strength, and fills from the factories', () => {
    const g = seat('USA');
    // Dated by the month history sent it overseas: it was activated in January and flew from England that summer.
    g.turn = 33;
    airUpkeep(g, 'USA');
    expect(at(g, 'Georgia')).toEqual([]);
    g.turn = 34;
    airUpkeep(g, 'USA');
    const eighth = at(g, 'Georgia');
    expect(eighth.length).toBe(2);
    expect(eighth.every((f) => f.parent === 'Eighth Air Force' && !homeCommand(f))).toBe(true);
    // Formed with 80 fighters and 100 bombers of the 200 and 250 it is kept at; the month's deliveries have begun to fill it.
    expect(sum(eighth, 'ftr')).toBeGreaterThanOrEqual(80);
    expect(sum(eighth, 'bmr')).toBeGreaterThanOrEqual(100);
    expect(eighth.reduce((s, f) => s + f.est.ftr, 0)).toBe(200);
    expect(eighth.reduce((s, f) => s + f.est.bmr, 0)).toBe(250);
    expect(g.headlines.some((h) => h.text === 'Eighth Air Force forms in Georgia.')).toBe(true);
    // Not twice.
    airUpkeep(g, 'USA');
    expect(at(g, 'Georgia').length).toBe(2);
    // By the summer of 1944 every command has formed and grown (ruling 228, item 4: the Eighth's bombers to 1,600, its
    // fighters to 600, the Fifteenth to 300 and 800): about 1,500 fighters and 3,050 bombers are kept for the war overseas.
    for (let t = 35; t <= 58; t++) { g.turn = t; airUpkeep(g, 'USA'); }
    const abroad = land(g).filter((f) => !homeCommand(f));
    // (Each group is kept at a whole number of aircraft: half an aircraft either way a group.)
    expect(Math.abs(abroad.reduce((s, f) => s + f.est.ftr, 0) - 1500)).toBeLessThanOrEqual(abroad.length / 2);
    expect(Math.abs(abroad.reduce((s, f) => s + f.est.bmr, 0) - 3050)).toBeLessThanOrEqual(abroad.length / 2);
    // A command that grows past what its groups hold forms new groups as it grows: VIII Bomber Command's 1,600 bombers
    // fly as seven groups, none kept at more than the data allows.
    const viii = abroad.filter((f) => f.parent === 'VIII Bomber Command');
    expect(viii.length).toBe(7);
    expect(viii.every((f) => f.kind === 'bmr' && f.est.bmr <= AIRWAR.groups.most)).toBe(true);
    expect(g.headlines.some((h) => /, a new group of VIII Bomber Command\.$/.test(h.text))).toBe(true);
    expect(abroad.every((f) => f.est.ftr + f.est.bmr <= AIRWAR.groups.most)).toBe(true);
  });
});

describe('across an ocean an air group goes by sea, one a month', () => {
  it('at peace America has no friendly field across the ocean, only its own far islands; in the war, Britain\'s are offered by sea', () => {
    const g = seat('USA');
    const grp = at(g, 'New York')[0], london = g.byName.London;
    expect(overOcean(g, grp, london)).toBe(true);
    expect(whyNotMoveAir(g, grp, london)).toBe('That ground is not in friendly hands.');
    // (Since Version 85.1 its own far islands are reached by sea too, the Canal Zone among them: the code checker's
    // America find 2. Nothing of anyone else's.)
    const isles = airGroupBases(g, grp.id).filter((b) => b.sea);
    expect(isles.map((b) => b.name)).toContain('Canal Zone');
    expect(isles.every((b) => g.regions[b.region].owner === 'USA' && g.regions[b.region].overseas)).toBe(true);
    // Inside America nothing changes: a field in a month's flight is flown to, a far one is too far.
    expect(whyNotMoveAir(g, grp, g.byName.Boston)).toBeNull();
    expect(km(g, grp.base, g.byName['Los Angeles'].id)).toBeGreaterThan(AIR.ferry);
    expect(whyNotMoveAir(g, grp, g.byName['Los Angeles'])).toBe('Too far to move in one month.');
    atWar(g);
    expect(whyNotMoveAir(g, grp, london)).toBeNull();
    const bases = airGroupBases(g, grp.id);
    const over = bases.filter((b) => b.sea);
    expect(over.length).toBeGreaterThan(5);
    expect(over.some((b) => b.region === london.id && b.why === null)).toBe(true);
    // The fields in a month's flight still stand first, nearest first.
    expect(bases.findIndex((b) => b.sea)).toBe(bases.filter((b) => !b.sea).length);
  });

  it('the order ships it: one group of a nation in a month, and never onto a field with no room for it', () => {
    const g = seat('USA');
    atWar(g);
    const [a, b] = [at(g, 'New York')[0], at(g, 'Norfolk')[0]];
    const kent = g.byName.Kent, london = g.byName.London;
    const ready = a.ready;
    expect(orderAirBase(g, a.id, london.id)).toBe(true);
    expect(a.base).toBe(london.id);
    expect(a.moved).toBe(g.turn);
    expect(a.ready).toBeLessThan(ready);
    // The second group waits a month.
    expect(whyNotMoveAir(g, b, kent)).toBe(SHIP_AIR_ONE);
    expect(orderAirBase(g, b.id, kent.id)).toBe(false);
    expect(b.base).toBe(g.byName.Norfolk.id);
    nextMonth(g);
    expect(whyNotMoveAir(g, b, kent)).not.toBe(SHIP_AIR_ONE);
    // A field already full takes no group that comes by sea. (Britain's own group is set down there to fill it: since
    // ruling 227 its staff bases its fighters by the threat, and No. 11 Group stands in the Midlands that month.)
    const full = g.airFleets.find((f) => f.nat === 'UK' && f.kind === 'ftr' && f.ship === undefined)!;
    const was = { ftr: full.ftr, base: full.base };
    full.base = kent.id;
    full.ftr = 5000;
    expect(whyNotMoveAir(g, b, kent)).toBe(SHIP_AIR_ROOM);
    full.ftr = was.ftr;
    full.base = was.base;
    expect(orderAirBase(g, b.id, kent.id)).toBe(true);
    // And home again the same way: from Britain, America's fields are across the ocean.
    nextMonth(g);
    expect(overOcean(g, a, g.byName['New York'])).toBe(true);
    expect(whyNotMoveAir(g, a, g.byName['New York'])).toBeNull();
  });

  it('the 3D game\'s page lists the fields across the ocean under their own heading, and the order reports it', () => {
    const g = seat('USA');
    atWar(g);
    const grp = at(g, 'New York')[0];
    const page = airPage(g, `rebase:${grp.id}`)!;
    expect(page.sections.map((s) => s.heading)).toEqual(['AIRFIELDS', 'ACROSS THE OCEAN, BY SEA']);
    // (Britain's fields, across the Atlantic: the heading holds America's own far islands too.)
    const row = page.sections[1].rows.find((r) => r.buttons[0].ok && g.byName[r.title]?.owner === 'UK')!;
    expect(row).toBeDefined();
    expect(page.sections[1].rows.some((r) => g.byName[r.title]?.owner === 'USA')).toBe(true);
    expect(row.buttons[0].label).toBe('SHIP IT HERE');
    const res = airAct(g, row.buttons[0].act)!;
    expect(res.ok).toBe(true);
    expect(res.note).toBe(`The group is shipped across the ocean to ${row.title}.`);
    expect(g.regions[grp.base].owner).toBe('UK');
    expect(overOcean(g, grp, g.byName['New York'])).toBe(true);
    // At peace the heading lists America's own far islands alone.
    const p = seat('USA');
    const peace = airPage(p, `rebase:${at(p, 'New York')[0].id}`)!;
    expect(peace.sections.map((s) => s.heading)).toEqual(['AIRFIELDS', 'ACROSS THE OCEAN, BY SEA']);
    expect(peace.sections[1].rows.length).toBeGreaterThan(0);
    expect(peace.sections[1].rows.every((r) => p.byName[r.title]?.owner === 'USA')).toBe(true);
  });
});
