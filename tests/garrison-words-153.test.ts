// Garrisons in the 1.5.3 patch: the 1.5.1 playtest's finds 14 to 18 (this file starts with 17). Since 1.5.1 a garrison comes out of its town as an
// order goes (comeOut, campaign-bridge.ts): the looks see it as if out, and an order keeps it out only if it went.
// What was left over is what the player is told, and the hand of a garrison and a field army in the 3D game:
// - 17: a garrison asked to take ship was told it had moved this month. It is told what it is, as the railway tells it.
import { describe, expect, it } from 'vitest';
import { createGame, hostileUnits, relocate } from '../src/campaign/game';
import { whyNotLand, whyNotShip } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { besieged, garrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Seen = { id: number; name: string; reg: number; town?: boolean; ap: number; apMax: number; apOut?: number; garrison?: string; nat: string };
type Row = { id: number; name: string; going: number[]; left: number };
interface Bridge {
  start(o: unknown): unknown;
  load(text: string): unknown;
  save(): { text: string };
  view(): { eve: boolean; turn: number; corps: Seen[] };
  reach(ids: number[]): { regions: { id: number }[]; spent: boolean; why: string; out: number[] };
  garrisonWhy(region: number): { why: string };
  move(ids: number[], to: number): { moved: { id: number; path: number[] }[]; note: string };
  rail(ids: number[], to: number): { moved: unknown[]; said: string };
  railTip(ids: number[], to: number): { no: string };
  approach(ids: number[], to: number): { ok: boolean; none: string; rows: Row[] };
  bySeaWhy(ids: number[], to: number): { why: string };
}
const TC = (globalThis as Record<string, unknown>).TC as Bridge;

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const goes = (c: Corps): boolean => c.nat === 'GER' && !c.dead && c.kind === 'inf' && c.joint === undefined && c.town === undefined;
/** A month before this one, for a corps that went into its town then (these games are in their opening month). */
const EARLIER = -1;
/** A German infantry corps put inside the town of a province (brought there first if none stands in its field), as having gone in that month. */
function inside(g: Game, at: Region, town: number): Corps {
  const c = at.units.find(goes) ?? g.corps.find((x) => goes(x) && x.reg !== at.id)!;
  if (c.reg !== at.id) relocate(g, c, at);
  expect(garrison(g, c)).toBe(true);
  c.town = town;
  return c;
}
/** A German infantry corps brought into a province's field from elsewhere. */
function bring(g: Game, at: Region): Corps {
  const c = g.corps.find((x) => goes(x) && x.reg !== at.id)!;
  relocate(g, c, at);
  return c;
}
/** German ground next door with no enemy on it. */
const home = (g: Game, r: Region): number => r.nb.find((j) => g.regions[j].ctrl === 'GER' && !hostileUnits(g, g.regions[j], 'GER').length)!;
/** An enemy army next door. */
const foeBy = (g: Game, r: Region): number => r.nb.find((j) => hostileUnits(g, g.regions[j], 'GER').length)!;
const saved = (): string => TC.save().text;
const corpsIn = (text: string, id: number): Corps => loadGame(text)!.corps.find((x) => x.id === id)!;
const seen = (id: number): Seen => TC.view().corps.find((x) => x.id === id)!;
/** The rules' line for a garrison of one, in the first month and in later ones (garrisonLine, armies.ts). */
const told = (c: Corps, r: Region): string => `${c.name} is the garrison of ${townName(r)}. Order it to take the field first.`;
const fresh = (c: Corps, r: Region): string => `${c.name} is the garrison of ${townName(r)}. It went in this month, and can march out next month.`;

describe('find 17: a garrison asked to take ship is told it is a garrison, not that it has moved', () => {
  it('to a port of its own side and to an enemy coast; shut in, no ship leaves the siege; a corps that has marched is told so as before', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'], bremen = g.byName['Bremen'];
    const c = inside(g, hamburg, EARLIER);
    expect(c.ap).toBe(0);
    const take = 'A garrison does not take ship: TAKE THE FIELD first.';
    expect(whyNotShip(g, c, bremen)).toBe(take);
    const coast = g.regions.find((r) => r.ctrl === 'POL' && r.seas.length > 0)!;
    expect(whyNotLand(g, [c], coast)).toBe(take);
    // A corps in the field that has marched part of its month: the old reason, which is its own.
    const f = bring(g, hamburg);
    f.ap = 1;
    expect(whyNotShip(g, f, bremen)).toBe('Only a corps that has not moved this month can embark.');
    expect(whyNotLand(g, [f], coast)).toBe('Only corps that have not moved this month can embark.');
    // The 3D game's by-sea refusal passes it through.
    TC.load(saveGame(g));
    expect(TC.bySeaWhy([c.id], bremen.id).why).toBe(take);
    // Shut in by an army round the town, it cannot take the field either: it is told what the railway tells it.
    for (const u of hamburg.units.filter((u) => u.nat === 'GER')) relocate(g, u, bremen);
    const poles = g.regions.find((r) => r.ctrl === 'POL' && r.units.some((u) => u.nat === 'POL' && u.kind === 'inf'))!;
    relocate(g, poles.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!, hamburg);
    expect(besieged(g, hamburg)).toBe(true);
    expect(whyNotShip(g, c, bremen)).toBe(`${c.name} is shut in the town at Hamburg. No ship leaves a siege.`);
  });
});
