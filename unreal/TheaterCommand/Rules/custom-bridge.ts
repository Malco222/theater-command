// A battle of your own (release 2; Brian, 24 November 2026 18:36Z): "an option on the screen just to do an air, land, sea
// battle. you pick the two sides. one you play and the other is the computer. you then pick the units you want for the
// battle or the computer can pick their own. both sides has a certain treasury budget they have to fit in between".
//
// Nothing here decides a battle. It builds a throwaway war (a "sandbox": the grand campaign's own first month, set up
// afresh and never saved) with only the two armies in it, on a battlefield of the real map, and asks the campaign's own
// rules for the battle's ticket, exactly as FIGHT IT YOURSELF and FLY IT YOURSELF do for a campaign's attack or raid. The
// 3D game's land rules and air battle fight it; the odds come from the same causes as every battle in the game.
//
// The core theme (Brian, 23 November 07:13Z): the units decide, with no historical handcuffs. So in a battle of your own
// every cause is his to set and nothing is keyed to a nation's name or the calendar:
//   - forces: what each side buys from the same price list the grand campaign raises corps and wings at;
//   - kit: each side's equipment generation (the tech tree's four eras), picked for each side on its own;
//   - training: one ladder for every nation (raw, trained, seasoned, veteran), its first setting the nation's own record;
//   - doctrine: what that kit generation brings (mobile warfare, battle groups, deep operations, winter kit);
//   - supply: both sides fully supplied; terrain and weather: the battlefield and its month;
//   - air: the fighters and bombers bought, in an air battle.
// A nation's own battle abilities (traits.json), its blitz tempo, slow command and staff limit stay out of it (the
// sandbox's flag 'custom', read in traits.ts): the same army fights the same whichever flag it carries.
//
// The calls, on the global object TC (wired in campaign-bridge.ts):
//   TC.customOffer()          -> what can be picked: nations, kit eras, training, sizes, the price lists, battlefields
//                                and air targets. Static; the 3D game hides a battlefield whose look has not passed.
//   TC.customArmy(spec, side) -> the computer's own pick for side 'me' or 'them' of spec: { buys, works, spent, room }.
//   TC.customCheck(spec)      -> { ok, why, me: { spent, room }, them: { spent, room } }: whether the picks can be fought.
//   TC.customLook(spec)       -> a land battle's { ticket, live } for the 3D game's land rules (TC.landStartTicket), the
//                                player's side named by spec.role; { ticket: null, why } where it cannot be fought.
//   TC.customAirStart(spec)   -> an air battle's { sky, view }, opened on the battle bridge's shared clock (TC.battleTick
//                                and the rest then drive it, as for FLY IT YOURSELF); null with TC.customWhy() saying why.
// A spec: { domain: 'land' | 'air', place, size, role: 'att' | 'def', kind?: 'bomb' | 'superiority', seed?,
//           me: { nat, kit?, train?, buys?, works? }, them: { nat, kit?, train?, buys?, works? } }
// A side's buys: { inf: 2, arm: 1, ... } on land, { ftr: 2, bmr: 3 } in the air; null or absent: the computer picks.

import { addCorps, computeSupply, createGame, frontage, setWar, DEFENCE } from '../../../src/campaign/game';
import { startOpen } from '../../../src/campaign/staff';
import { PRODUCTION, corpsName } from '../../../src/campaign/economy';
import { ARSENAL } from '../../../src/campaign/arsenal';
import { STACK, type Corps, type Game, type Kind, type Region } from '../../../src/campaign/types';
import { landTicket } from '../../../src/battle/land/ticket';
import { staffOnly } from '../../../src/battle/land/offered';
import { foundAirFleet } from '../../../src/campaign/air';
import { riverBetween } from '../../../src/campaign/rivers';
import { airAct } from '../../../src/campaign/airpages';
import { afterCard, type FoughtAir } from '../../../src/campaign/airfought';
import techData from '../../../data/campaign/tech.json';
import nationsData from '../../../data/campaign/nations.json';
import grandData from '../../../data/campaign/grand.json';

// ---- What can be picked ------------------------------------------------------------------------------------------

type Domain = 'land' | 'air';
type Role = 'att' | 'def';

export interface CustomSide { nat: string; kit?: number; train?: number; buys?: Record<string, number> | null; works?: number }
export interface CustomSpec { domain: Domain; place: string; size: string; role: Role; kind?: string; seed?: number; me: CustomSide; them: CustomSide }

const TECH = techData as unknown as {
  eras: string[];
  techs: { id: string; branch: string; tier: number; needs: string[] }[];
  start: Record<string, string[]>;
};
const NATIONS = nationsData as unknown as Record<string, { name?: string; adj?: string; color?: string; quality?: number; dormant?: boolean }>;
const GRAND = grandData as unknown as { nations: { id: string; small?: boolean }[] };

/** The equipment generations, as the tech tree's own eras name them: a side fields everything its era and those before bring. */
export const KITS = [1, 2, 3, 4].map((tier) => ({ tier, name: TECH.eras[tier] ?? `Era ${tier}`, year: [1939, 1940, 1941, 1943, 1944][tier] }));

/**
 * Training: one ladder for every nation (the design director's ladder for release 2's 'Units decide', 0.85/1.05/1.25 by
 * rows, with a raw rung under it). It stands where the grand campaign keeps a nation's quality. A side's first setting
 * is its own nation's record, the highest rung at or under it; he can change it either way.
 */
export const TRAINING = [
  { id: 'raw', name: 'Raw', q: 0.7, blurb: 'Short training and few experienced officers.' },
  { id: 'trained', name: 'Trained', q: 0.85, blurb: 'A full course of training; officers who know their trade.' },
  { id: 'seasoned', name: 'Seasoned', q: 1.05, blurb: 'Hard training, good officers and sergeants, drill that holds under fire.' },
  { id: 'veteran', name: 'Veteran', q: 1.25, blurb: 'The best-trained army of its day: initiative at every level.' },
];

/** How big a battle: each side's treasury, in the campaign's production points (the screens show them times a hundred). */
export const SIZES = [
  { id: 'clash', name: 'A clash', land: 120, air: 60, blurb: 'Two or three corps a side, or a raid of a few hundred aircraft.' },
  { id: 'battle', name: 'A battle', land: 200, air: 100, blurb: 'Three or four corps a side, or a raid of five or six hundred aircraft.' },
  { id: 'great', name: 'A great battle', land: 300, air: 150, blurb: 'As many corps as the ground has room for, or a raid of near a thousand aircraft.' },
];

/** The land price list: the grand campaign's own corps (production.json builds and the arsenal's guns and engineers). */
const LAND_KINDS: Kind[] = ['inf', 'mtn', 'mot', 'arm', 'art', 'eng'];
type Buy = { kind: string; label: string; divs: number; cost: number; blurb: string; fights: boolean; count?: number };
function landBuys(): Buy[] {
  const builds = PRODUCTION.builds as Record<string, { label: string; divs: number; cost: number; blurb: string }>;
  const arsenal = (ARSENAL as unknown as { land: Record<string, { label: string; divs: number; cost: number; blurb: string }> }).land;
  return LAND_KINDS.map((k) => {
    const b = builds[k] ?? arsenal[k];
    return { kind: k, label: b.label, divs: b.divs, cost: b.cost, blurb: b.blurb, fights: k !== 'art' && k !== 'eng' };
  });
}
/** Fieldworks: the defence dug in before the battle, at the price of the campaign's first level of fortification. */
const WORKS_COST = (PRODUCTION.fort as unknown as { cost: number[] }).cost[1] ?? 24;

/** The air price list: the arsenal's wings, a hundred aircraft each. */
function airBuys(): Buy[] {
  const air = ARSENAL.air as Record<string, { label: string; count: number; cost: number; blurb: string }>;
  return (['ftr', 'bmr'] as const).map((k) => ({ kind: k, label: air[k].label, divs: 0, count: air[k].count, cost: air[k].cost, blurb: air[k].blurb, fights: true }));
}
/** Anti-aircraft guns round the target, a level at a time: the arsenal's own works. */
const FLAK = (ARSENAL.works as unknown as Record<string, { cost: number[] }>).aa;
/** Levels of guns cost what the arsenal charges for each level in turn (8, 12, 18): k levels, the first k summed. */
const flakCost = (k: number): number => { let s = 0; for (let i = 1; i <= Math.min(3, k); i++) s += FLAK?.cost?.[i] ?? 10; return s; };
const FLAK_COST = flakCost(1);

/**
 * The battlefields: real places on the campaign map, each fought in one month of the year (its season and weather). The
 * 3D game works out each one's look as the attack card does (LandScape::Pick from its latitude, longitude, terrain and
 * month) and offers it only once that look has passed the Mac's 45 frames a second and the art director
 * (Content/Rules/fight-yourself-looks.txt): so the list grows by itself as looks pass. 'from' is the province next
 * door the attack comes from.
 */
export const LAND_PLACES = [
  { id: 'maine', region: 'Maine', from: 'Orleans', month: 6, name: 'The hedgerows of Maine', blurb: 'Small fields boxed in by tall hedges and sunken lanes: short sight lines and close fighting.' },
  { id: 'pomorze', region: 'Pomorze', from: 'Poznan', month: 9, name: 'The Pomeranian plain', blurb: 'Open farmland and villages at harvest time: room for tanks to move and guns to see.' },
  { id: 'voronezh', region: 'Voronezh', from: 'Kursk', month: 7, name: 'The fields before Voronezh', blurb: 'High summer on the open farmland of the Don: long views and little cover.' },
  { id: 'orel', region: 'Orel', from: 'Kursk', month: 1, name: 'Orel in deep winter', blurb: 'Snow on open farmland. The cold slows any army without winter kit.' },
  { id: 'valdai', region: 'Valdai', from: 'Novgorod', month: 12, name: 'The Valdai woods in winter', blurb: 'Snow, forest and frozen lakes: hard going for the attack.' },
  { id: 'sidi-barrani', region: 'Sidi Barrani', from: 'Mersa Matruh', month: 9, name: 'The desert at Sidi Barrani', blurb: 'Flat open desert with no flanks: whatever moves on wheels or tracks can go round.' },
  { id: 'tobruk', region: 'Tobruk', from: 'Bardia', month: 5, name: 'The desert before Tobruk', blurb: 'Stony desert and a ridge or two: little cover and a long way to see.' },
  { id: 'calabria', region: 'Calabria', from: 'Apulia', month: 9, name: 'The hills of Calabria', blurb: 'Steep mountain country with few roads: room for only a few corps, and hard on tanks.' },
  { id: 'algiers', region: 'Algiers', from: 'Oran', month: 12, name: 'The hills above Algiers', blurb: 'Mediterranean hills, olive groves and terraces in the winter rains.' },
  { id: 'monterrey', region: 'Monterrey', from: 'Nuevo Laredo', month: 6, name: 'The dry hills of Monterrey', blurb: 'Scrub and dry ground under a hot sun.' },
  // (offered once their looks pass: the open steppe, the jungle)
  { id: 'kalmyk', region: 'Kalmyk Steppe', from: 'Astrakhan', month: 8, name: 'The Kalmyk steppe', blurb: 'Bare rolling grassland to the horizon: nowhere to hide.' },
  { id: 'panama', region: 'Panama City', from: 'Colon', month: 9, name: 'The jungle at Panama', blurb: 'Thick jungle and rain: fighting at a few metres.' },
];

/**
 * The air targets: places the 3D air battle has been seen over and passed (the art director's raid stills of 24
 * November), each in its month. The raid comes from 'from', the defending fighters rise from the target itself.
 */
export const AIR_PLACES = [
  { id: 'brussels', region: 'Brussels', from: 'Kent', month: 6, name: 'Over Brussels', blurb: 'A city and its railway yards, with the Channel behind the raid.' },
  { id: 'rhineland', region: 'Rhineland', from: 'Lorraine', month: 5, name: 'Over the Rhineland', blurb: 'Factories and towns along the Rhine, far inland.' },
  { id: 'hampshire', region: 'Hampshire', from: 'Normandy', month: 1, name: 'Over Hampshire', blurb: 'Ports and airfields on the south coast of England, in winter.' },
  { id: 'dnepropetrovsk', region: 'Dnepropetrovsk', from: 'Zaporozhye', month: 6, name: 'Over the Dnieper bend', blurb: 'Steelworks and bridges on the great river, deep inland.' },
  { id: 'epirus', region: 'Epirus', from: 'Apulia', month: 4, name: 'Over Epirus', blurb: 'Mountains running down to the Ionian Sea.' },
];

const byId = <T extends { id: string }>(list: T[], id: string): T | undefined => list.find((x) => x.id === id);

/** A nation's own place on the training ladder: the highest rung at or under its record (nations.json quality). */
export function trainingOf(nat: string): number {
  const q = NATIONS[nat]?.quality ?? 0.85;
  let at = 0;
  TRAINING.forEach((t, i) => { if (t.q <= q + 1e-6) at = i; });
  return at;
}

/** The nations that can fight: every one the campaign knows, the great powers first in the grand campaign's order. */
function nationList() {
  const order = GRAND.nations.filter((n) => !n.small).map((n) => n.id);
  return Object.entries(NATIONS)
    .filter(([id, n]) => !id.startsWith('_') && !!n?.name)
    .map(([id, n]) => ({ nat: id, name: n.name!, adj: n.adj ?? id, color: n.color ?? '#888888', major: order.includes(id), train: trainingOf(id) }))
    .sort((a, b) => (a.major !== b.major ? (a.major ? -1 : 1) : a.major ? order.indexOf(a.nat) - order.indexOf(b.nat) : a.name.localeCompare(b.name)));
}

export function customOffer() {
  return {
    nations: nationList(),
    kits: KITS,
    kit: 3,
    training: TRAINING.map(({ id, name, blurb }) => ({ id, name, blurb })),
    sizes: SIZES,
    size: 'battle',
    land: { buys: landBuys(), works: { label: 'Fieldworks', cost: WORKS_COST, blurb: 'The defence digs in before the battle: trenches, wire and gun pits. Only the defending side can have them.' } },
    air: {
      buys: airBuys(),
      flak: { label: 'Anti-aircraft guns', costs: [1, 2, 3].map(flakCost), most: 3, blurb: 'Levels of heavy guns round the target. Only the defending side can have them.' },
      kinds: [
        { id: 'bomb', name: 'A bombing raid by day', blurb: 'Bombers, with or without escorts, against the target; fighters rise to meet them.' },
        { id: 'superiority', name: 'A fight for the sky', blurb: 'Fighters sweep over the target to bring the defending fighters up and beat them.' },
      ],
    },
    places: LAND_PLACES.map(({ id, region, month, name, blurb }) => ({ id, region, month, name, blurb })),
    airPlaces: AIR_PLACES.map(({ id, region, month, name, blurb }) => ({ id, region, month, name, blurb })),
    // (the sea: the 3D sea scene is watched, not yet commanded, and has not passed the Mac's 45 frames a second: none yet)
    sea: { open: false, why: 'Sea battles of your own come once the 3D sea battle can be commanded and has passed the Mac\'s speed check and the art director.' },
  };
}

// ---- The rooms and the prices --------------------------------------------------------------------------------------

function budgetOf(spec: CustomSpec): number {
  const s = byId(SIZES, spec.size) ?? SIZES[1];
  return spec.domain === 'air' ? s.air : s.land;
}

const landCost = (kind: string): number => landBuys().find((b) => b.kind === kind)?.cost ?? Infinity;
const airCost = (kind: string): number => airBuys().find((b) => b.kind === kind)?.cost ?? Infinity;

/** The most air groups a side can bring: each a wing of a hundred, as the campaign's arsenal sells them. */
const AIR_ROOM = 9;

/**
 * How many corps a side can bring onto the field: the defence as many as a province holds (5); the attack as many as the
 * roads from one province carry onto that ground (frontage: plains 4, mountains 2, else 3), and two corps of guns more
 * against a dug-in defence, as the grand campaign lets guns join over the infantry's heads.
 */
function roomOf(spec: CustomSpec, who: 'me' | 'them', terrain: string, defWorks: boolean): number {
  if (spec.domain === 'air') return AIR_ROOM;
  const attacking = (who === 'me') === (spec.role === 'att');
  if (!attacking) return STACK;
  return frontage({ terrain } as Region) + (defWorks ? DEFENCE.frontage.guns : 0);
}

/** The attack's room for corps that are not guns: the roads alone (the guns' extra room is for guns only). */
function roadsOf(spec: CustomSpec, who: 'me' | 'them', terrain: string): number {
  const attacking = (who === 'me') === (spec.role === 'att');
  return spec.domain === 'land' && attacking ? frontage({ terrain } as Region) : Infinity;
}

function spentOf(spec: CustomSpec, side: CustomSide, defending: boolean): number {
  const cost = spec.domain === 'air' ? airCost : landCost;
  let total = 0;
  for (const [k, n] of Object.entries(side.buys ?? {})) total += cost(k) * Math.max(0, Math.floor(n || 0));
  if (defending && side.works) total += spec.domain === 'air' ? flakCost(Math.floor(side.works)) : WORKS_COST * Math.min(1, Math.max(0, Math.floor(side.works)));
  return total;
}

function terrainOf(spec: CustomSpec): string {
  const p = spec.domain === 'air' ? byId(AIR_PLACES, spec.place) : byId(LAND_PLACES, spec.place);
  return p ? (REGION_TERRAIN.get(p.region) ?? 'plains') : 'plains';
}
/** Each battlefield's ground, read once from a fresh campaign (the map's own terrain). */
const REGION_TERRAIN = new Map<string, string>();
function readTerrain(): void {
  if (REGION_TERRAIN.size) return;
  const g = createGame(1, null);
  for (const r of g.regions) REGION_TERRAIN.set(r.name, r.terrain);
}

// ---- The computer's own pick ---------------------------------------------------------------------------------------

/**
 * The computer's army: its treasury spent the way a staff that has to win on this ground would spend it. Infantry the
 * backbone; tanks where the ground is open; mountain troops where it is steep; guns against a dug-in defence; the
 * defence digs in first. A share for each kind, filled a corps at a time where it is furthest under its share, while
 * the treasury and the room last. The player's own army is never read: the computer picks for the ground, not to beat
 * what he bought.
 */
function pickLand(budget: number, room: number, roads: number, attacking: boolean, terrain: string, foeWorks: boolean): { buys: Record<string, number>; works: number } {
  const shares: Record<string, number> =
    terrain === 'mountain' ? { mtn: 0.55, inf: 0.25, art: 0.2 }
      : terrain === 'plains' || terrain === 'desert' ? { inf: 0.35, arm: 0.35, mot: 0.2, art: 0.1 }
        : terrain === 'urban' ? { inf: 0.5, art: 0.25, eng: 0.1, arm: 0.15 }
          : { inf: 0.45, arm: 0.2, mot: 0.1, art: 0.25 };
  if (attacking && foeWorks) { shares.art = (shares.art ?? 0) + 0.1; shares.eng = (shares.eng ?? 0) + 0.05; }
  const buys: Record<string, number> = {};
  let left = budget, works = 0;
  // A defence digs in before anything else, where it can still afford a corps after.
  if (!attacking && left >= WORKS_COST + landCost('inf')) { works = 1; left -= WORKS_COST; }
  const spent: Record<string, number> = {};
  let count = 0, fighting = 0;
  for (let guard = 0; guard < 20 && count < room; guard++) {
    const total = Object.values(spent).reduce((a, b) => a + b, 0);
    // The kinds it can still afford, furthest under their share first; the last corps it can bring must fight.
    const lastRoom = count === room - 1 && fighting === 0;
    // (past the roads' room only guns can join, against a dug-in defence)
    const roadsFull = count - (spent.art ?? 0) / landCost('art') >= roads;
    const can = Object.keys(shares).filter((k) => landCost(k) <= left && (!lastRoom || (k !== 'art' && k !== 'eng')) && (!roadsFull || k === 'art'));
    if (!can.length) break;
    // (how far each kind would stand over its share with one more corps of it: the least over first)
    const over = (k: string) => ((spent[k] ?? 0) + landCost(k)) / (total + landCost(k)) - shares[k];
    can.sort((a, b) => over(a) - over(b) || landCost(a) - landCost(b));
    // (the first corps always fights)
    const k = fighting === 0 ? (can.find((x) => x !== 'art' && x !== 'eng') ?? can[0]) : can[0];
    buys[k] = (buys[k] ?? 0) + 1;
    spent[k] = (spent[k] ?? 0) + landCost(k);
    left -= landCost(k);
    count++;
    if (k !== 'art' && k !== 'eng') fighting++;
  }
  return { buys, works };
}

/**
 * The computer's air army: the raid as bombers under as many escorts as it can afford (a fighter to two bombers),
 * or all fighters for a fight for the sky; the defence all fighters, and a level of guns once it has two wings up.
 */
function pickAir(budget: number, attacking: boolean, kind: string): { buys: Record<string, number>; works: number } {
  const f = airCost('ftr'), b = airCost('bmr');
  const buys: Record<string, number> = {};
  let left = budget, works = 0;
  if (attacking && kind === 'bomb') {
    // two bombers' wings to each fighter wing, bombers first
    let nb = 0, nf = 0;
    while (nb + nf < AIR_ROOM) {
      const wantF = nf * 2 < nb;
      if (wantF && left >= f) { nf++; left -= f; } else if (left >= b) { nb++; left -= b; } else if (left >= f) { nf++; left -= f; } else break;
    }
    if (nb) buys.bmr = nb;
    if (nf) buys.ftr = nf;
  } else {
    let nf = 0;
    while (nf < AIR_ROOM && left >= f) {
      nf++; left -= f;
      if (!attacking && nf === 2 && works < 1 && left >= FLAK_COST + f) { works = 1; left -= FLAK_COST; }
    }
    if (nf) buys.ftr = nf;
  }
  return { buys, works };
}

export function customArmy(spec: CustomSpec, who: 'me' | 'them' = 'them') {
  readTerrain();
  const terrain = terrainOf(spec);
  const attacking = (who === 'me') === (spec.role === 'att');
  const other = who === 'me' ? spec.them : spec.me;
  const budget = budgetOf(spec);
  const foeWorks = !attacking ? false : !!(other?.buys && other.works);
  const room = roomOf(spec, who, terrain, attacking ? foeWorks : false);
  const pick = spec.domain === 'air' ? pickAir(budget, attacking, spec.kind ?? 'bomb') : pickLand(budget, room, roadsOf(spec, who, terrain), attacking, terrain, foeWorks);
  const side: CustomSide = { ...(who === 'me' ? spec.me : spec.them), buys: pick.buys, works: pick.works };
  return { buys: pick.buys, works: pick.works, spent: spentOf(spec, side, !attacking), budget, room };
}

// ---- Whether the picks can be fought -------------------------------------------------------------------------------

/** Each side's army as it will fight: its own picks, or the computer's where it left them to the computer. */
function settled(spec: CustomSpec): CustomSpec {
  const out: CustomSpec = { ...spec, me: { ...spec.me }, them: { ...spec.them } };
  // (the defence's works first: the attack's room for guns follows them)
  const order: ('me' | 'them')[] = spec.role === 'att' ? ['them', 'me'] : ['me', 'them'];
  for (const who of order) {
    // (an army left to the computer is null; an empty one is his, with nothing in it yet)
    if (out[who].buys) continue;
    const a = customArmy(out, who);
    out[who] = { ...out[who], buys: a.buys, works: a.works };
  }
  return out;
}

export function customCheck(raw: CustomSpec) {
  readTerrain();
  const spec = settled(raw);
  const budget = budgetOf(spec);
  const terrain = terrainOf(spec);
  const place = spec.domain === 'air' ? byId(AIR_PLACES, spec.place) : byId(LAND_PLACES, spec.place);
  const defSide = spec.role === 'att' ? spec.them : spec.me;
  const defWorks = !!defSide.works;
  const sideState = (who: 'me' | 'them') => {
    const s = spec[who];
    const defending = (who === 'me') !== (spec.role === 'att');
    const n = Object.values(s.buys ?? {}).reduce((a, b) => a + Math.max(0, Math.floor(b || 0)), 0);
    const guns = Math.max(0, Math.floor(s.buys?.art ?? 0));
    return { spent: spentOf(spec, s, defending), budget, room: roomOf(spec, who, terrain, defending ? false : defWorks), roads: roadsOf(spec, who, terrain), count: n, guns, buys: s.buys ?? {}, works: s.works ?? 0 };
  };
  const me = sideState('me'), them = sideState('them');
  const why = whyNot(spec, place, me, them);
  return { ok: !why, why, me, them };
}

function whyNot(spec: CustomSpec, place: unknown, me: { spent: number; budget: number; room: number; roads: number; count: number; guns: number; buys: Record<string, number> }, them: typeof me): string | null {
  if (!place) return 'Pick a battlefield.';
  if (!NATIONS[spec.me.nat]?.name || !NATIONS[spec.them.nat]?.name) return 'Pick a nation for each side.';
  if (spec.me.nat === spec.them.nat) return 'Pick two different nations: one for you and one for the computer.';
  for (const [s, who] of [[me, 'Your army'], [them, 'The computer\'s army']] as const) {
    if (s.count === 0) return `${who} has nothing in it yet.`;
    if (s.spent > s.budget) return `${who} costs ${(s.spent * 100).toLocaleString('en-US')}, more than the treasury of ${(s.budget * 100).toLocaleString('en-US')}.`;
    if (s.count > s.room) return `${who} has ${s.count} ${spec.domain === 'air' ? 'wings' : 'corps'}; there is room on this ground for ${s.room}.`;
    if (s.count - s.guns > s.roads) return `${who} has ${s.count - s.guns} corps on the roads in; there is room for ${s.roads} (only artillery can join past that, against a dug-in defence).`;
    if (spec.domain === 'land' && !Object.entries(s.buys).some(([k, n]) => n > 0 && k !== 'art' && k !== 'eng')) return `${who} needs infantry or tanks: guns and engineers cannot take or hold ground alone.`;
    if (spec.domain === 'air' && spec.kind === 'bomb' && s === (spec.role === 'att' ? me : them) && !(s.buys.bmr > 0)) return 'A bombing raid needs bombers.';
  }
  return null;
}

// ---- The sandbox: a war with only the two armies in it --------------------------------------------------------------

/** The tech tree's land, air, doctrine and naval steps up to a kit era, with the nation's own start. */
function kitTechs(nat: string, tier: number): string[] {
  const out = new Set<string>(TECH.start[nat] ?? []);
  for (const t of TECH.techs) if (t.tier <= tier && ['land', 'air', 'doctrine', 'naval'].includes(t.branch)) out.add(t.id);
  return [...out];
}

/** The month the battle is fought in: the battlefield's month, in the year of the newer of the two kit eras. */
function turnOf(year: number, month: number): number {
  // (turn 1 is September 1939)
  const t = (year - 1939) * 12 + (month - 1) - 7;
  return t >= 1 ? t : t + 12;
}

let lastWhy = '';

interface Sandbox { g: Game; target: Region; from: Region; att: Corps[]; def: Corps[]; attNat: string; defNat: string; spec: CustomSpec }

function setSide(g: Game, nat: string, s: CustomSide): void {
  const n = g.nations[nat];
  n.alive = true;
  // Training stands where the campaign keeps a nation's quality; the national handcuffs stay out (see the head of the file).
  n.quality = TRAINING[Math.max(0, Math.min(TRAINING.length - 1, s.train ?? trainingOf(nat)))].q;
  // (no air force standing behind the army: in a land battle of his own nothing flies over the field but what he buys,
  // and today that is nothing; fighters on call over a land battle are a later step)
  n.air = 0;
  n.blitz = false;
  n.slowCommand = false;
  n.maxAttack = 0;
  // (no league, no bloc: the two are at war with each other and allied to nobody)
  (n as { align?: string | null }).align = null;
  (n as { league?: unknown }).league = undefined;
  n.atWar = [];
  if (n.tech) n.tech = { ...n.tech, done: kitTechs(nat, Math.max(1, Math.min(4, s.kit ?? 3))), work: [] };
}

/**
 * The province the attack comes from: the battlefield's own pick where no river lies between (an attack over a river at
 * these odds is one the staff settles, src/battle/land/offered.ts), else the first province next door on dry ground with
 * none, the same ground first.
 */
function approach(g: Game, target: Region, wanted: Region | undefined): Region | null {
  const dry = (r: Region | undefined): boolean => !!r && target.nb.includes(r.id) && !riverBetween(g, r.id, target.id) && !(r as { sea?: boolean }).sea;
  if (dry(wanted)) return wanted!;
  const next = target.nb.map((i) => g.regions[i]).filter((r) => dry(r));
  next.sort((a, b) => (a.terrain === target.terrain ? 0 : 1) - (b.terrain === target.terrain ? 0 : 1) || a.id - b.id);
  return next[0] ?? (wanted && target.nb.includes(wanted.id) ? wanted : null);
}

function buildSandbox(raw: CustomSpec): Sandbox | null {
  lastWhy = '';
  const check = customCheck(raw);
  if (!check.ok) { lastWhy = check.why ?? 'This battle cannot be fought.'; return null; }
  const spec = settled(raw);
  const air = spec.domain === 'air';
  const place = air ? byId(AIR_PLACES, spec.place)! : byId(LAND_PLACES, spec.place)!;
  const attSide = spec.role === 'att' ? spec.me : spec.them, defSide = spec.role === 'att' ? spec.them : spec.me;
  const attNat = attSide.nat, defNat = defSide.nat;
  const seed = (spec.seed ?? 1939) >>> 0;
  // The grand campaign's first month, nobody else's army in it. The player's side is the one the attack is ordered for
  // while the ticket is made (landTicket and airTicket make the attack the player could order); the ticket's sides are
  // then marked for who commands which.
  const g = startOpen(createGame(seed, attNat), true);
  const tier = Math.max(attSide.kit ?? 3, defSide.kit ?? 3);
  g.turn = turnOf(KITS[Math.max(0, Math.min(3, tier - 1))].year, place.month);
  g.flags.custom = 1;
  g.human = attNat;
  g.order = [attNat, defNat, ...g.order.filter((x) => x !== attNat && x !== defNat)];
  g.phase = 0;
  // Nobody else's armies, fleets or air: the battle is the two armies alone.
  g.corps = [];
  for (const r of g.regions) r.units = [];
  g.airFleets = [];
  g.fleets = [];
  // (and nobody's sky won before the battle: what flies is what is bought)
  g.sky = {};
  g.raids = {};
  setSide(g, attNat, attSide);
  setSide(g, defNat, defSide);
  setWar(g, attNat, defNat);
  const target = g.byName[place.region];
  const from = target ? approach(g, target, g.byName[place.from]) : null;
  if (!target || !from) { lastWhy = `${place.name} is not on this map.`; return null; }
  // The ground: the defence's own (its province, a depot, its town's guard stood down: the field army alone fights), the
  // attack's next door, each fed from where it stands.
  for (const [r, nat] of [[target, defNat], [from, attNat]] as const) {
    r.owner = nat; r.ctrl = nat; r.fort = null as unknown as Region['fort'];
    g.flags['depot:' + r.id] = 1;
    g.flags[`guard:${r.id}`] = 100;
    // (an air base on each, room for every wing either side can buy)
    if (air) g.flags[`af:${r.id}`] = 3;
  }
  const att: Corps[] = [], def: Corps[] = [];
  if (!air) {
    const raise = (nat: string, s: CustomSide, r: Region, out: Corps[], dug: boolean) => {
      let no = 1;
      for (const k of LAND_KINDS) {
        const n = Math.max(0, Math.floor(s.buys?.[k] ?? 0));
        const divs = landBuys().find((b) => b.kind === k)!.divs;
        for (let i = 0; i < n; i++) {
          const c = addCorps(g, nat, corpsName(nat, k, no++), k, divs, r.id, {});
          c.home = r.id;
          if (dug) c.dug = true;
          out.push(c);
        }
      }
    };
    raise(attNat, attSide, from, att, false);
    raise(defNat, defSide, target, def, !!defSide.works);
  }
  g.supplyDirty = true;
  computeSupply(g);
  for (const c of [...att, ...def]) c.supplied = true;
  return { g, target, from, att, def, attNat, defNat, spec };
}

/** A land battle of his own, as the 3D game's land rules take it (TC.landStartTicket), or why not. */
export function customLook(raw: CustomSpec) {
  if (raw?.domain !== 'land') return { ticket: null, why: 'That is not a land battle.' };
  const box = buildSandbox(raw);
  if (!box) return { ticket: null, why: lastWhy };
  const t = landTicket(box.g, box.att, box.target);
  if (!t) return { ticket: null, why: 'The two armies cannot meet on this ground.' };
  const why = staffOnly(t);
  if (why) return { ticket: null, why };
  // The sides: his and the computer's. The attack was made as his while the ticket was written; now it says who is who.
  const meAtt = box.spec.role === 'att';
  const p = LAND_PLACES.find((x) => x.id === box.spec.place)!;
  // (no fog in a battle of his own: both sides' armies are known, so the odds are the card's own, never a staff's band)
  const { seen: _seen, ...reckoning } = t.reckoning;
  const ticket = {
    ...t,
    reckoning,
    title: `A battle of your own: ${p.name.replace(/^The /, 'the ')}`.replace(/: the /, ': The '),
    att: { ...t.att, human: meAtt },
    def: { ...t.def, human: !meAtt },
    // (no history dial in a sandbox: the computer general fights in the style the situation calls for)
    hist: 4,
    attempt: 0,
  };
  const designYears: Record<string, number> = {};
  for (const [nat, s] of [[box.attNat, box.spec.role === 'att' ? box.spec.me : box.spec.them], [box.defNat, box.spec.role === 'att' ? box.spec.them : box.spec.me]] as const) {
    designYears[nat] = KITS[Math.max(0, Math.min(3, (s.kit ?? 3) - 1))].year;
  }
  return { ticket, live: { fog: false, seen: [], designYears }, custom: { place: box.spec.place, region: box.target.id, turn: box.g.turn } };
}

/** Why the last sandbox could not be built. */
export const customWhy = (): string => lastWhy;

/**
 * An air battle's sandbox: the raid's groups on airfields next door, the defence's fighters on the target's own fields
 * and its guns round it; for the battle bridge's airStart on this game instead of the campaign's.
 */
export function airSandbox(raw: CustomSpec): { g: Game; kind: string; target: number; ids: number[] } | null {
  if (raw?.domain !== 'air') { lastWhy = 'That is not an air battle.'; return null; }
  const box = buildSandbox(raw);
  if (!box) return null;
  const { g, target, from, attNat, defNat, spec } = box;
  const attSide = spec.role === 'att' ? spec.me : spec.them, defSide = spec.role === 'att' ? spec.them : spec.me;
  const ids: number[] = [];
  const wings = (nat: string, s: CustomSide, r: Region, out: number[] | null) => {
    for (const k of ['bmr', 'ftr'] as const) {
      const n = Math.max(0, Math.floor(s.buys?.[k] ?? 0));
      const count = airBuys().find((b) => b.kind === k)!.count ?? 100;
      for (let i = 0; i < n; i++) {
        const f = foundAirFleet(g, nat, r, `${g.nations[nat].adj} ${k === 'ftr' ? 'Fighter' : 'Bomber'} Wing ${i + 1}`, k, count);
        if (k === 'ftr') { f.ftr = count; f.est = { ftr: count, bmr: 0 }; } else { f.bmr = count; f.est = { ftr: 0, bmr: count }; }
        // (the defence's fighters rise against raids in reach on 'support', as a defending air force's do)
        f.mission = (out ? 'defend' : 'support') as typeof f.mission;
        f.ready = 1;
        out?.push(f.id);
      }
    }
  };
  wings(attNat, attSide, from, ids);
  wings(defNat, defSide, target, null);
  // The guns round the target.
  const flak = Math.max(0, Math.min(3, Math.floor(defSide.works ?? 0)));
  if (flak) g.flags[`wk:aa:${target.id}`] = flak;
  void defNat;
  return { g, kind: spec.kind === 'superiority' ? 'superiority' : 'bomb', target: target.id, ids };
}

/**
 * The raid paid on the war of your own (the raider's order, airAct), its after card told from his side: when he defends,
 * the card is made again with him as the defender ('your fighters', the raiders' losses as the enemy's).
 */
export function customAirSettle(g: Game, kind: string, target: number, ids: number[], me: Role, fought?: FoughtAir): unknown {
  const out = airAct(g, `raid:${kind}:${target}:${ids.join('.')}`, fought);
  if (!out || !out.ok || me !== 'def' || !fought) return out;
  const rep = g.airReports.find((r) => r.id === out.report);
  if (!rep) return out;
  const was = g.human;
  g.human = rep.defNat;
  try { return { ...out, after: afterCard(g, rep, fought) }; } finally { g.human = was; }
}
export function customCalls(h: { airStartOn?: (g: Game, kind: string, target: number, ids: number[], nat: string, me: Role, settle?: (fought?: FoughtAir) => unknown) => unknown }) {
  return {
    customOffer,
    customArmy: (spec: CustomSpec, who?: 'me' | 'them') => customArmy(spec, who === 'me' ? 'me' : 'them'),
    customCheck,
    customLook,
    customWhy,
    customAirStart(spec: CustomSpec) {
      const box = airSandbox(spec);
      if (!box || !h.airStartOn) return null;
      // (the raid is the attacker's: his when he attacks, the computer's when he defends)
      const me: Role = spec.role === 'def' ? 'def' : 'att';
      return h.airStartOn(box.g, box.kind, box.target, box.ids, box.g.human ?? '', me, (f) => customAirSettle(box.g, box.kind, box.target, box.ids, me, f));
    },
  };
}
