// The grand campaign's navy as pieces on the map. A fleet is picked up like an army, its ships are laid out as cards
// by class, and it is given an order: patrol a sea, raid shipping, escort a shipping lane, hunt submarines, or lie in
// harbour. It can stand in to a coast and shell it, and it can be split in two or joined to another.
//
// The rules of the sea itself (who guards a lane, who sinks a submarine, who is torpedoed) are in navy.ts. This module
// holds the orders beyond sailing, the forecasts both interfaces show before an order is given, and the cards they
// draw. Every number is in data/campaign/fleets.json. None of it touches the history campaigns.

import { byHand } from './depth';
import { tenthPct } from '../core/math';
import { turnToDate, type TicketLine } from '../core/ticket';
import fleetData from '../../data/campaign/fleets.json';
import { AIRWAR, carrierRules, raid as stopOutput } from './air';
import { bareBrief, bareLine, bareOf, deckWorth, fleetKnown, moveWings, seaPicture, wingCards, wingShare, wingsAshore, type WingCard } from './carriers';
import { allied, atWar, headline, routineNews, worksFor } from './game';
import { SIGHT, fogOn, sightOf } from './intel';
import {
  laneCounts,
  CLASSES, FLEETS, NAVY, atSea, boatsOut, patrolRow, fuelLine, torpedoWords, escort, fit, afloatWatch, fleetPower, fleetReach, fleetsOf, harbourPatrol, huntShareOf, huntShareSunk, huntersIn, keptAfloat, keptByHand, laneLoss, openFleetRows, quality, screenGap, seaRng,
  passageWhy, sail, seaRoutes, seaSide, seaWar, shipCount, shipList, sinkShips, torpedoRisk, whyNotEscort,
} from './navy';
import { lanesTo } from './resources';
import { SIEGE, besieged, besiegerOf, breachOf, hitTown, planTownHit, siegeRules, siegeView, townHeld, townHitText, townShare, type TownHit } from './siege';
import { dcOf } from './tech';
import { townName } from './towns';
import { cityHit, type CityHit } from './repairs';
import { joinStars, splitStars, starRow, type StarRow } from './stars';
import type { Corps, Fleet, Game, Region, ShipClass, Ships, ShoreReport, Stance, FleetStance } from './types';
import { skyAtSea, skyBattleLine, zoneOfSea } from './weather';
import { seaWhat, seaWhere } from './sea-words';
import { FAR_SEAS_FIRST } from './world';

// Read straight from the data, so that nothing here depends on the order the modules are loaded in.
const B = (fleetData as unknown as typeof FLEETS).bombard;
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;

// ---- The cards -----------------------------------------------------------------------------------------

export interface ShipCard {
  cls: ShipClass;
  label: string;
  count: number;
  lost: number;
  /** What this class is for, in a sentence, and in three words for the face of a card. */
  role: string;
  brief: string;
  /** Battle stars (b477): the row of the most decorated ship of the class, and the actions behind it. Absent where none has one. */
  stars?: StarRow;
}

export interface FleetCard {
  id: number;
  nat: string;
  name: string;
  nick: string | null;
  admiral: { name: string; level: number } | null;
  port: string;
  base: number;
  zone: number;
  sea: string;
  stance: FleetStance;
  /** The order it is under, as the interfaces name it: Patrol, Raid shipping, Escort a lane, Hunt submarines, In harbour (and In passage, on the far chart). */
  order: string;
  /** The lane it escorts, if it escorts one. */
  lane: string | null;
  /** What it is doing, in a phrase: "Escorting the Atlantic lane". */
  doing: string;
  ready: number;
  power: number;
  ships: number;
  /** The far side's one measure: how many of its submarines are out on patrol, for a fleet at sea where the measure reads it (`boatsOut`, `patrolRow`); none and all are said too. */
  boatsOut?: number;
  /** The fleet's fuel line (sheet 24, box 4): fuelled from the wells of a named field, or short of fuel by this many per cent of its fitness for sea. `fuelLine`. */
  fuel?: { kind: 'wells'; field: string } | { kind: 'short'; less: number };
  /** Faulty torpedoes (sheet 24, box 3): the sentence for a fleet whose boats raid at a share of their worth, shown to its own side. `torpedoWords`. */
  torpedoes?: string;
  /** The far side's harbour patrols: the share of its destroyers that hunt submarines in the sea off its port while a fleet laid for harbour lies there. `harbourPatrol`. */
  harbour?: number;
  /** One card for each class it has ships of, heaviest first. */
  cards: ShipCard[];
  /** The weight of its guns against a coast, its worth with a convoy, and its weight hunting submarines. */
  guns: number;
  escort: number;
  hunters: number;
  /** The destroyers round its heavy ships: how many it has, how many it needs, and what the gap risks this month where it is. */
  screen: { have: number; need: number; gap: number; risk: number; word: string };
  /** True when its guns have already spoken this month. */
  shelled: boolean;
  /** The aircraft its carriers bring, as cards (fighters first), and how full its decks are, 0..1. Empty and 1 for a fleet without carriers. */
  wings: WingCard[];
  decks: number;
  /** The aircraft aboard, and the aircraft its decks hold: "0 of 78 aboard" while its wings are ashore. Both 0 for a fleet without carriers. */
  aboard: number;
  deckRoom: number;
  /** Its wings ordered ashore (ruling 175(b)): each one's group id, name, where it is and its aircraft. And the line the card carries while deck room stands empty for them, or ''. */
  ashore: { id: number; name: string; kind: 'ftr' | 'bmr'; label: string; at: string; aircraft: number }[];
  bare: string;
  /** The same line in few words, for a card with one short line. */
  bareBrief: string;
  /** Weather (ruling 143(e)): at sea under a closed sky, "The sky is closed over the North Sea this month: no aircraft will fly for either side." Only then. */
  sky?: string;
}

const gunsRaw = (s: Ships): number => CLASSES.reduce((n, c) => n + s[c] * B.guns[c], 0);
/** The weight of a fleet's guns against a coast. */
export const gunWeight = (g: Game, f: Fleet): number => gunsRaw(f.ships) * quality(g, f.nat) * fit(g, f);

/** What a fleet is doing, in a phrase. */
export function fleetDoing(g: Game, f: Fleet): string {
  const sea = g.seas[f.zone]?.name;
  const where = sea ? seaWhere(sea) : 'at sea', what = sea ? seaWhat(sea) : 'the sea';
  if (!atSea(f)) return `In harbour at ${f.port}`;
  // (Release 2, the far chart: on its way to a new base.)
  if (f.stance === 'passage') return `In passage to ${f.port} ${where}${f.passage ? `, in by ${turnToDate(f.passage.due).label}` : ''}`;
  if (f.stance === 'raid') return `Raiding shipping ${where}`;
  if (f.stance === 'hunt') return `Hunting submarines ${where}`;
  if (f.stance === 'escort') return `Escorting ${seaRoutes(g).find((r) => r.id === f.lane)?.name ?? 'a shipping lane'}`;
  return `Patrolling ${what}`;
}

/** A fleet as both interfaces show it: its admiral, where it is and what it is doing, and its ships as cards. */
export function fleetCard(g: Game, f: Fleet): FleetCard {
  const heavy = f.ships.bb + f.ships.cv + f.ships.ca;
  const need = Math.ceil(heavy * FLEETS.screen.per);
  const gap = screenGap(f);
  let escortW = 0;
  for (const c of CLASSES) escortW += f.ships[c] * NAVY.classes[c].escort * (c === 'cv' ? deckWorth(g, f) : 1);
  return {
    id: f.id, nat: f.nat, name: f.name, nick: f.nick ?? null,
    admiral: f.admiral ? { name: f.admiral.name, level: f.admiral.level } : null,
    port: f.port, base: f.base, zone: f.zone, sea: g.seas[f.zone]?.name ?? '', stance: f.stance,
    order: FLEETS.orders[f.stance].label, lane: f.stance === 'escort' ? f.lane ?? null : null, doing: fleetDoing(g, f),
    ready: f.ready, power: fleetPower(g, f), ships: shipCount(f.ships),
    ...(atSea(f) && f.ships.ss > 0 && patrolRow(g, f) ? { boatsOut: Math.round(boatsOut(g, f)) } : {}),
    ...(fuelLine(g, f) ? { fuel: fuelLine(g, f)! } : {}),
    ...(seaWar(g) && harbourPatrol(g, f) ? { harbour: FLEETS.hunt.loose } : {}),
    ...(f.ships.ss > 0 && f.zone >= FAR_SEAS_FIRST && torpedoWords(g, f.nat, 'these boats') ? { torpedoes: torpedoWords(g, f.nat, 'these boats')! } : {}),
    cards: CLASSES.filter((c) => f.ships[c] > 0).map((c) => {
      const card: ShipCard = { cls: c, label: NAVY.classes[c].label, count: f.ships[c], lost: f.lost[c], role: FLEETS.roles[c], brief: FLEETS.brief[c] };
      const stars = starRow(g, f, c);
      if (stars) card.stars = stars;
      return card;
    }),
    guns: gunWeight(g, f), escort: escortW * quality(g, f.nat) * fit(g, f),
    hunters: (f.ships.dd + f.ships.cv * FLEETS.hunt.carrier * deckWorth(g, f)) * quality(g, f.nat) * fit(g, f),
    screen: {
      have: f.ships.dd, need, gap, risk: atSea(f) ? torpedoRisk(g, f, f.zone) : 0,
      word: heavy <= 0 ? 'No heavy ships to screen' : gap <= 0 ? 'Fully screened' : gap < 0.4 ? 'Thinly screened' : f.ships.dd > 0 ? 'Badly short of destroyers' : 'No destroyers at all',
    },
    shelled: f.shelled === g.turn,
    wings: wingCards(g, f), decks: wingShare(g, f),
    aboard: wingCards(g, f).reduce((s, w) => s + w.aircraft, 0), deckRoom: wingCards(g, f).reduce((s, w) => s + w.est, 0) + bareOf(f, 'ftr') + bareOf(f, 'bmr'),
    ashore: wingsAshore(g, f.id).map((w) => ({ id: w.id, name: w.name, kind: w.kind === 'bmr' ? 'bmr' as const : 'ftr' as const, label: w.kind === 'bmr' ? 'Strike aircraft' : 'Carrier fighters', at: g.regions[w.base].name, aircraft: w.ftr + w.bmr })), bare: bareLine(g, f), bareBrief: bareBrief(g, f),
    ...(atSea(f) && skyAtSea(g, f.zone) === 'closed' ? { sky: skyBattleLine(g, zoneOfSea(g, f.zone), `the ${g.seas[f.zone].name}`)! } : {}),
  };
}

// ---- Escorting a lane ------------------------------------------------------------------------------------

export interface EscortLane {
  id: string;
  name: string;
  /** The nation it feeds. */
  to: string;
  zones: number[];
  /** 'open', 'raided', 'cut' or 'idle'. Nothing sails on a lane that is cut or idle. */
  state: 'open' | 'raided' | 'cut' | 'idle';
  why: string;
  /** Raiders against escorts on it now, and the share of the cargoes being sunk. */
  raid: number;
  guard: number;
  loss: number;
  /** The fleets bound to it as escorts. */
  escorts: number[];
}

/** The lanes a nation has a stake in guarding: its own and those of its allies at war, as they stand this month. */
export function escortLanes(g: Game, nat: string): EscortLane[] {
  if (!seaWar(g)) return [];
  const out: EscortLane[] = [];
  for (const rt of seaRoutes(g)) {
    const to = g.nations[rt.to];
    if (!to?.alive || (rt.to !== nat && !allied(g, nat, rt.to))) continue;
    const lane = lanesTo(g, rt.to).find((l) => l.id === rt.id);
    const m = to.atWar.length ? laneLoss(g, rt.to, rt.zones, rt.id) : { raid: 0, guard: 0, loss: 0 };
    const state = lane ? lane.state : !to.atWar.length ? 'idle' : m.loss >= 0.02 ? 'raided' : 'open';
    out.push({
      id: rt.id, name: rt.name, to: rt.to, zones: rt.zones, state, why: lane?.why ?? '',
      raid: m.raid, guard: m.guard, loss: state === 'cut' || state === 'idle' ? 0 : m.loss,
      escorts: g.fleets.filter((f) => f.stance === 'escort' && f.lane === rt.id).map((f) => f.id),
    });
  }
  return out;
}

export interface EscortPlan {
  why: string | null;
  lane: string;
  name: string;
  to: string;
  /** The share of the lane's cargoes being sunk as things stand, and what it would be with this fleet bound to the lane. */
  now: number;
  then: number;
  /** Raiders on the lane, and its guard now and with this fleet. */
  raid: number;
  guardNow: number;
  guardThen: number;
  /** True when the fleet is already this lane's escort. */
  bound: boolean;
}

/** What binding a fleet to a lane would do for its cargoes. Nothing is changed. */
export function planEscort(g: Game, f: Fleet, lane: string): EscortPlan {
  const rt = seaRoutes(g).find((r) => r.id === lane);
  const plan: EscortPlan = { why: whyNotEscort(g, f, lane), lane, name: rt?.name ?? lane, to: rt?.to ?? '', now: 0, then: 0, raid: 0, guardNow: 0, guardThen: 0, bound: f.stance === 'escort' && f.lane === lane };
  if (!rt) return plan;
  const now = laneLoss(g, rt.to, rt.zones, rt.id);
  plan.now = now.loss; plan.raid = now.raid; plan.guardNow = now.guard;
  plan.then = now.loss; plan.guardThen = now.guard;
  if (plan.why || plan.bound) return plan;
  // Counted as it would be counted with the fleet on the lane, and then everything is put back.
  const keep = { zone: f.zone, stance: f.stance, lane: f.lane };
  f.stance = 'escort'; f.lane = lane;
  const then = laneLoss(g, rt.to, rt.zones, rt.id);
  f.zone = keep.zone; f.stance = keep.stance;
  if (keep.lane === undefined) delete f.lane; else f.lane = keep.lane;
  plan.then = then.loss; plan.guardThen = then.guard;
  return plan;
}

/** Binds a fleet to a lane as its escort: it sails with the convoys, wherever on the lane the raiders are thickest. */
export const escortLane = (g: Game, f: Fleet, lane: string): boolean => escort(g, f, lane);

// ---- Where a fleet can go ----------------------------------------------------------------------------------

export interface SeaOption {
  zone: number;
  name: string;
  here: boolean;
  /** Each side's strength there now: fleets at sea and aircraft from the coasts. */
  ours: number;
  theirs: number;
  /** Enemy surface warships and enemy submarines at sea there, as far as they are identified; and enemy forces there known only from a report. */
  surface: number;
  subs: number;
  reported: number;
  /** How well the viewer's side has that sea searched, in a phrase. */
  search: string;
  /** What a patrol there would meet: nobody, a fleet it would bring to battle, or one it would not face. */
  meet: 'clear' | 'battle' | 'shy';
  /** The share of those submarines this fleet's side would sink in a month if it hunted there, and as things stand. */
  hunt: number;
  huntNow: number;
  /** The chance this fleet loses a heavy ship to a submarine there this month. */
  torpedo: number;
  /** True for a sea beyond any fleet's reach that this fleet's submarines can still be sent to raid in (navy.ts ownBoatsReach; the far side's beats alone). */
  raidOnly?: boolean;
}

/**
 * The seas a fleet can be sent to, with what it would find in each. For a viewer under the fog of war (the player),
 * only the enemy ships his side has identified are counted, and forces known only from a report are a number.
 */
export function seaOptions(g: Game, f: Fleet, viewer: string | null = null): SeaOption[] {
  const mine = fleetPower(g, f);
  const fog = viewer !== null && carrierRules(g) && fogOn(g, viewer);
  // (The player's boats raid farther than any fleet patrols, where his navy's group does from that base: those seas follow the others.)
  const near = fleetReach(g, f), far = fleetReach(g, f, false, true).filter((z) => !near.includes(z));
  return [...near, ...far].map((z) => {
    const here = atSea(f) && f.zone === z;
    const us = seaSide(g, z, f.nat, false), them = seaSide(g, z, f.nat, true);
    const pic = fog ? seaPicture(g, z, viewer) : null;
    if (pic) them.total = pic.theirs;
    let surface = 0, subs = 0, reported = 0;
    const quarry: Fleet[] = [];
    for (const x of g.fleets) {
      if (x.zone !== z || !atSea(x) || !atWar(g, x.nat, f.nat)) continue;
      const k = fog ? fleetKnown(g, x, viewer) : 'exact';
      if (k !== 'exact') { if (k === 'report') reported++; continue; }
      subs += boatsOut(g, x);
      if (x.ships.ss > 0) quarry.push(x);
      surface += CLASSES.reduce((n, c) => n + (c === 'ss' ? 0 : x.ships[c]), 0);
    }
    const own = us.total + (here ? 0 : mine);
    // (Read as the hunt reads it: with Units decide, its kit and the boats left to the convoys' escorts, huntExpected.)
    const huntNow = huntShareOf(g, f.nat, z, quarry);
    // What its own destroyers and carriers would add if it hunted there, counted as the hunt counts them.
    let hunt = huntNow;
    if (subs > 0 && seaWar(g)) {
      const keep = { zone: f.zone, stance: f.stance, lane: f.lane };
      f.zone = z; f.stance = 'hunt'; delete f.lane;
      hunt = huntShareOf(g, f.nat, z, quarry);
      f.zone = keep.zone; f.stance = keep.stance;
      if (keep.lane !== undefined) f.lane = keep.lane;
    }
    return {
      // (The far side's one measure gives boats out in parts of a boat: the card counts whole boats, never none where some are out.)
      zone: z, name: g.seas[z].name, here, ours: us.total, theirs: them.total, surface, subs: subs > 0 ? Math.max(1, Math.round(subs)) : 0, reported, search: pic?.word ?? '',
      meet: surface <= 0 ? 'clear' : own >= NAVY.battle.seek * Math.max(1, them.total) ? 'battle' : 'shy',
      hunt, huntNow, torpedo: seaWar(g) ? torpedoRisk(g, f, z) : 0,
      ...(far.includes(z) ? { raidOnly: true } : {}),
    };
  });
}

// ---- Shelling a coast ----------------------------------------------------------------------------------------

/** What a fleet's guns are laid on: the troops in the open, the factories and docks, or the defences of the town (the grand campaign's sieges). */
export type ShoreTarget = 'troops' | 'city' | 'town';

const foesAt = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((u) => !u.dead && atWar(g, nat, u.nat));

/**
 * Why a fleet cannot shell a coast, or null if it can. With no target named it asks only whether there is anything
 * there to shell. 'viewer' is the nation giving the order when the fog of war applies to it: troops it has not seen cannot be aimed at.
 */
export function whyNotBombard(g: Game, f: Fleet, r: Region, what?: ShoreTarget, viewer: string | null = null): string | null {
  if (!seaWar(g)) return 'Fleets shell a coast only in the grand campaign.';
  if (!shipCount(f.ships)) return `The ${f.name} has no ships.`;
  // (A fleet in passage to a new base does nothing on its way: release 2, the far chart.)
  if (f.stance === 'passage') return passageWhy(g, f);
  if (!atSea(f)) return `The ${f.name} is in harbour. Send it to sea off the coast first.`;
  if (!r.seas.includes(f.zone)) return `The ${f.name} is ${seaWhere(g.seas[f.zone].name)}, and ${r.name} is not on that sea.`;
  if (!atWar(g, f.nat, r.ctrl)) return `${r.name} is not in enemy hands.`;
  if (gunsRaw(f.ships) < B.least) return `The ${f.name} has too few heavy guns to shell a coast: that is work for battleships and cruisers.`;
  if (f.shelled === g.turn) return `The ${f.name} has already shelled a coast this month.`;
  if (g.flags[`shell:${r.id}`] === g.turn) return `${r.name} has already been shelled this month.`;
  if (f.ready < B.minReady) return `The ${f.name} is too worn for it. Bring it home to refit.`;
  const seen = viewer === null || sightOf(g, viewer)[r.id] !== SIGHT.none;
  const troops = seen && foesAt(g, r, f.nat).length > 0;
  if (what === 'troops' && !troops) return viewer === null ? `No enemy troops stand at ${r.name}.` : `No enemy troops have been seen at ${r.name}.`;
  if (what === 'city' && r.fac <= 0) return `${r.name} has no industry for the guns to hit.`;
  const town = siegeRules(g) && townHeld(g, r);
  if (what === 'town' && !siegeRules(g)) return 'Towns are shelled only in the grand campaign.';
  if (what === 'town' && !town) return `Nobody holds ${townName(r)}: an army can walk in.`;
  if (!what && !troops && r.fac <= 0 && !town) return `There is nothing at ${r.name} worth the shells: no enemy troops in sight and no industry.`;
  return null;
}

export interface ShorePlan {
  why: string | null;
  fleet: number;
  region: number;
  what: ShoreTarget;
  /** Which targets are there to choose between. */
  can: { troops: boolean; city: boolean; town: boolean };
  /** The town's defences: what the shelling would do to its works and to the troops inside. Null where there is no held town. */
  town: TownHit | null;
  guns: number;
  /** Troops: the share of their organisation the shelling would cost troops in the open, how many corps and divisions stand there. */
  cut: number;
  corps: number;
  divs: number;
  /** City: the share of the province's output stopped for a month, and of the enemy's whole production. */
  output: number;
  nation: number;
  /**
   * The level of the coast's fortress, and the share of the fleet's cruisers and destroyers its guns are likely to sink
   * (ruling 240: less by the fleet's own damage control, where its nation holds the research).
   */
  fort: number;
  reply: number;
  /** The fleet's readiness afterwards. */
  ready: number;
  lines: TicketLine[];
}

/** The cut in organisation for troops in the open at this coast under guns of this weight. */
function shellCut(r: Region, foes: Corps[], weight: number): number {
  const T = B.troops;
  const divs = foes.reduce((s, c) => s + c.divs, 0);
  return divs > 0 ? Math.min(T.max, weight / (T.scale + T.perDiv * divs)) * (AIRWAR.ground.cover[r.terrain] ?? 1) : 0;
}
const shelter = (c: Corps): number => (c.kind === 'fort' ? B.troops.fort : c.dug ? B.troops.dug : 1);
/** The share of the cruisers and destroyers standing in that a fortress of this level sinks, before the ships' own damage control. */
const shoreReply = (fort: number): number => Math.min(0.5, B.reply * fort);

/** What shelling a coast would do. Nothing is changed. With no target named, the troops are chosen if any are there. */
export function planBombard(g: Game, f: Fleet, r: Region, what?: ShoreTarget, viewer: string | null = null): ShorePlan {
  const seen = viewer === null || sightOf(g, viewer)[r.id] !== SIGHT.none;
  const foes = seen ? foesAt(g, r, f.nat) : [];
  const can = { troops: whyNotBombard(g, f, r, 'troops', viewer) === null, city: whyNotBombard(g, f, r, 'city', viewer) === null, town: whyNotBombard(g, f, r, 'town', viewer) === null };
  // With no target named: the troops in the open if there are any, then a town under siege, then the factories.
  const pick: ShoreTarget = what ?? (can.troops ? 'troops' : can.town && besieged(g, r) ? 'town' : can.city ? 'city' : can.town ? 'town' : 'troops');
  const guns = gunWeight(g, f);
  const town = siegeRules(g) && townHeld(g, r) ? planTownHit(g, r, 'shell', townShare('shell', guns)) : null;
  const total = g.regions.reduce((s, x) => s + (x.ctrl === r.ctrl ? x.fac : 0), 0);
  const output = r.fac > 0 ? Math.min(B.city.max, guns / B.city.scale) : 0;
  const fort = r.fort && worksFor(g, r, r.ctrl) ? r.fort.lv : 0;
  const plan: ShorePlan = {
    why: whyNotBombard(g, f, r, what ?? (can.troops || can.city || can.town ? pick : undefined), viewer), fleet: f.id, region: r.id, what: pick, can, town, guns,
    cut: shellCut(r, foes, guns), corps: foes.length, divs: foes.reduce((s, c) => s + c.divs, 0),
    output, nation: total > 0 ? output * (r.fac / total) : 0,
    fort, reply: shoreReply(fort) / dcOf(g, f.nat), ready: Math.max(0, f.ready - B.ready), lines: [],
  };
  const heavy = `${shipList({ ...none(), bb: f.ships.bb, ca: f.ships.ca })}`;
  plan.lines.push({ label: 'Guns', value: cap(heavy), effect: `Weight of fire ${Math.round(guns)}: a battleship counts ${B.guns.bb}, a cruiser ${B.guns.ca}` });
  if (pick === 'troops') {
    const cover = AIRWAR.ground.cover[r.terrain] ?? 1;
    if (cover < 1) plan.lines.push({ label: 'Ground', value: cap(r.terrain), effect: `Cover takes ${Math.round((1 - cover) * 100)}% off the effect` });
    plan.lines.push({ label: 'Troops', value: `${plural(plan.corps, 'corps', 'corps')}, ${plural(plan.divs, 'division')}`, effect: plan.cut > 0 ? `Organisation cut by about ${Math.round(plan.cut * 100)}%` : 'Nothing to hit' });
    if (foes.some((c) => c.dug || c.kind === 'fort')) plan.lines.push({ label: 'Shelter', value: foes.some((c) => c.kind === 'fort') ? 'Fortress troops under concrete' : 'Troops dug in', effect: `Dug in they lose ${Math.round(B.troops.dug * 100)}% as much, in a fortress ${Math.round(B.troops.fort * 100)}%` });
  } else if (pick === 'town') {
    const v = siegeView(g, r.id)!;
    plan.lines.push({ label: 'The town', value: `${townName(r)}, ${v.sizeWord.toLowerCase()}: troops inside fight ${Math.round((v.bonus - 1) * 100)}% stronger`, effect: `A weight of fire of ${SIEGE.bombard.shellFull} breaks down ${SIEGE.breach.shell}% of its works, less its share; never more than ${SIEGE.breach.most}% in all` });
    if (town) plan.lines.push({ label: 'The shelling', value: town.works >= 0.5 ? `About ${Math.round(town.works)}% of the works broken down` : 'Little more can be broken', effect: townHitText(g, r, town, 'shells', false) });
  } else {
    plan.lines.push({ label: 'Industry', value: plural(r.fac, 'factory', 'factories'), effect: output > 0 ? `${Math.round(output * 100)}% of its output stopped for a month: ${tenthPct(plan.nation)} of ${g.nations[r.ctrl].adj} production` : 'Nothing to hit' });
  }
  if (fort > 0) plan.lines.push({ label: 'Shore batteries', value: `Fortress level ${fort}`, effect: `About ${Math.round(plan.reply * 100)}% of the cruisers and destroyers that stand in are likely to be sunk` });
  plan.lines.push({ label: 'Readiness', value: `${Math.round(f.ready * 100)}% now`, effect: `${Math.round(plan.ready * 100)}% afterwards: a bombardment wears the ships and empties the magazines` });
  return plan;
}

/** Stands a fleet in to a coast and shells the troops there, or the works of its city. Returns the report, or null if it cannot be done. */
export function bombard(g: Game, f: Fleet, r: Region, what: ShoreTarget, viewer: string | null = null): ShoreReport | null {
  if (whyNotBombard(g, f, r, what, viewer)) return null;
  const plan = planBombard(g, f, r, what, viewer);
  const rng = seaRng(g, `shell:${f.id}:${r.id}`);
  const before = { ...f.ships };
  const defNat = r.ctrl;
  let cut = 0, divs = 0, output = 0, nation = 0, works: number | undefined, result: string;
  let city: CityHit | null = null;
  if (what === 'troops') {
    const foes = foesAt(g, r, f.nat);
    const base = shellCut(r, foes, plan.guns) * rng.range(0.85, 1.15);
    for (const c of foes) {
      const k = base * shelter(c);
      c.org = Math.max(Math.min(c.org, B.troops.floor), c.org - k);
      c.str = Math.max(0.05, c.str * (1 - k * B.troops.str));
      divs += c.divs;
    }
    cut = base;
    result = cut > 0
      ? `The shelling cut the organisation of ${foes.length === 1 ? foes[0].name : `the ${foes.length} corps`} at ${r.name} by ${Math.round(cut * 100)}%${foes.some((c) => c.dug || c.kind === 'fort') ? ', less where they are dug in or under concrete' : ''}. They fight the weaker for it until they have rested.`
      : 'The guns found little to hit.';
  } else if (what === 'town') {
    const hit = hitTown(g, r, 'shell', townShare('shell', plan.guns) * rng.range(0.85, 1.15));
    works = hit.works; cut = hit.org; divs = hit.divs;
    result = townHitText(g, r, hit, 'shells', true);
    // Damaged cities: the shells that break down its works fall on its streets too.
    city = cityHit(g, r, 'shell', hit.share);
  } else {
    output = plan.output; nation = plan.nation;
    stopOutput(g, f.nat, r.ctrl, nation);
    result = output > 0 ? `The shelling stopped ${Math.round(output * 100)}% of ${r.name}'s output for a month: ${tenthPct(nation)} of ${g.nations[r.ctrl].adj} production.` : '';
    // Damaged cities: the shells on the works leave the town damaged until it is repaired.
    city = cityHit(g, r, 'shellCity', plan.guns);
  }
  if (city) result = `${result} ${city.words}`.trim();
  // Ruling 240 (C3): the fleet's damage control eases the batteries' toll, on the same draws (the forecast's `reply`
  // is the eased figure); the ships it kept afloat are told in the report and in the news.
  const kept = none();
  const sunk = plan.reply > 0 ? sinkShips(f, shoreReply(plan.fort), rng, ['ca', 'dd'], dcOf(g, f.nat), kept) : none();
  if (shipCount(sunk)) result += ` The shore batteries sank ${shipList(sunk)}.`;
  afloatWatch.tell?.({ route: 'shore', nat: f.nat, sunk, saved: kept });
  // (Rider 3 of 22 November: the report tells what happened, so its line is in the past, in the fleet form.)
  const afloat = keptAfloat(f, kept, 'fleet', true);
  if (afloat) result += ` ${afloat}`;
  f.shelled = g.turn;
  f.ready = Math.max(0, f.ready - B.ready);
  // A fleet that stands in to shell a coast is seen from it: the coast's holder has it identified this month.
  if (carrierRules(g)) (f.seen ??= {})[defNat] = g.turn;
  g.flags[`shell:${r.id}`] = g.turn;
  g.flags.shoreReports = (g.flags.shoreReports ?? 0) + 1;
  const rep: ShoreReport = {
    id: g.flags.shoreReports, turn: g.turn, nat: f.nat, fleet: f.name, admiral: f.admiral?.name ?? null, ships: before, sunk,
    region: r.id, place: r.name, zone: f.zone, defNat, what, guns: plan.guns, cut, divs, output, nation, lines: plan.lines, result,
  };
  if (works !== undefined) rep.works = works;
  if (city) rep.city = { region: city.region, added: city.added, now: city.now };
  g.shoreReports.push(rep);
  const adj = g.nations[defNat].adj;
  // (Rider 3: 'its' only where the sentence just before names the fleet; after the batteries' line, "the fleet's".)
  const news = `${what === 'troops'
    ? `The ${f.name} shells the ${adj} troops at ${r.name} from ${seaWhat(g.seas[f.zone].name)}.${shipCount(sunk) ? ` The shore batteries sink ${shipList(sunk)}.` : ''}`
    : what === 'town' ? `The ${f.name} shells the defences of ${townName(r)} from ${seaWhat(g.seas[f.zone].name)}.${shipCount(sunk) ? ` The shore batteries sink ${shipList(sunk)}.` : ''}`
    : `The ${f.name} shells the factories and docks of ${r.name}.${shipCount(sunk) ? ` The shore batteries sink ${shipList(sunk)}.` : ''}`}${afloat ? ` ${keptAfloat(f, kept, shipCount(sunk) ? "fleet's" : 'its')}` : ''}`;
  // The same ships shelling the same shore month after month are one line of the month's summary; a ship lost is news.
  if (shipCount(sunk)) headline(g, 'battle', news, f.nat, r.id, { arm: 'sea', won: defNat, lost: f.nat });
  else routineNews(g, `shell:${f.nat}:${what}:${r.id}`, 'battle', news, what === 'troops' ? `the ${adj} troops at ${r.name} shelled from the sea` : what === 'town' ? `the defences of ${townName(r)} shelled from the sea` : `${r.name} shelled from the sea`, f.nat, r.id, { arm: 'sea' });
  return rep;
}

/** The enemy coasts a fleet could shell from where it is. */
export function bombardTargets(g: Game, f: Fleet, viewer: string | null = null): Region[] {
  if (!seaWar(g) || !atSea(f)) return [];
  return g.seas[f.zone].coast.map((id) => g.regions[id]).filter((r) => whyNotBombard(g, f, r, undefined, viewer) === null);
}

// ---- Splitting and joining -------------------------------------------------------------------------------------

const ORD = ['1st', '2nd', '3rd', '4th', '5th', '6th', '7th', '8th', '9th', '10th', '11th', '12th'];

/** A name for ships split off from a fleet, by what they are made of: "2nd Escort Fleet". Every group of ships is called a fleet. */
export function squadronName(g: Game, nat: string, s: Ships): string {
  const word = s.ss > 0 && shipCount(s) === s.ss ? 'Submarine Fleet' : s.bb + s.cv > 0 ? 'Battle Fleet' : s.dd >= s.ca ? 'Escort Fleet' : 'Cruiser Fleet';
  for (let k = 0; ; k++) {
    const name = `${ORD[k] ?? `${k + 1}th`} ${word}`;
    if (!g.fleets.some((f) => f.nat === nat && f.name === name)) return name;
  }
}

const taken = (take: Partial<Ships>): Ships => {
  const s = none();
  for (const c of CLASSES) s[c] = Math.max(0, Math.floor(take[c] ?? 0));
  return s;
};

/** Why these ships cannot be split off from a fleet as a squadron of their own, or null if they can. */
export function whyNotDetach(g: Game, f: Fleet, take: Partial<Ships>): string | null {
  if (!seaWar(g)) return 'Fleets are split only in the grand campaign.';
  if (f.stance === 'passage') return `${passageWhy(g, f)} It is split once it is in.`;
  const s = taken(take);
  if (!shipCount(s)) return 'Choose the ships that are to go.';
  for (const c of CLASSES) if (s[c] > f.ships[c]) return `The ${f.name} has only ${f.ships[c]} ${f.ships[c] === 1 ? NAVY.classes[c].one : NAVY.classes[c].many}.`;
  if (shipCount(s) >= shipCount(f.ships)) return 'Some ships have to stay: to move the whole fleet, give the fleet the order.';
  return null;
}

/** Splits ships off from a fleet as a new squadron, in the same place and under the same order. Returns it, or null. */
export function detach(g: Game, f: Fleet, take: Partial<Ships>, name?: string): Fleet | null {
  if (whyNotDetach(g, f, take)) return null;
  const s = taken(take);
  // The ships that go take their battle stars with them: the last of each class (stars.ts).
  const stars = splitStars(f, s);
  for (const c of CLASSES) f.ships[c] -= s[c];
  const wanted = name?.trim();
  const sq: Fleet = {
    id: g.nextFleet++, nat: f.nat, name: wanted && !g.fleets.some((x) => x.nat === f.nat && x.name === wanted) ? wanted : squadronName(g, f.nat, s),
    ships: s, lost: none(), base: f.base, port: f.port, home: f.home, zone: f.zone, stance: f.stance,
    station: f.zone, duty: shipCount(s) === s.ss ? 'raid' : 'patrol', tied: false, ready: f.ready, admiral: null, fought: f.fought,
  };
  if (f.stance === 'escort' && f.lane !== undefined) sq.lane = f.lane;
  if (f.shelled !== undefined) sq.shelled = f.shelled;
  // A squadron split from a fleet that attacked this month has attacked with it (release 2, fleetattack.ts).
  if (f.attacked !== undefined) sq.attacked = f.attacked;
  if (f.seen) sq.seen = { ...f.seen };
  if (f.struck) sq.struck = [f.struck[0], f.struck[1], f.struck[2]];
  // (Release 2, the far chart: its strikes at anchor go with it, for its staff's weighing of a move: navy.ts anchorLoss.)
  if (f.anchor) sq.anchor = f.anchor.map((e) => ({ ...e, from: e.from.map((o) => ({ ...o })) }));
  if (stars) sq.stars = stars;
  g.fleets.push(sq);
  // Carriers that go take their aircraft with them.
  moveWings(g, f, sq, s.cv, f.ships.cv + s.cv);
  g.supplyDirty = true;
  return sq;
}

/** Why one fleet cannot join another, or null if it can: they must lie in the same harbour, or be at sea in the same sea. */
export function whyNotMerge(g: Game, a: Fleet, b: Fleet): string | null {
  if (!seaWar(g)) return 'Fleets are joined only in the grand campaign.';
  if (a.id === b.id || a.nat !== b.nat) return 'Only two fleets of the same navy can be joined.';
  if (!shipCount(a.ships)) return `The ${a.name} has no ships.`;
  // (Release 2, the far chart: a fleet in passage joins none, nor is joined, on its way.)
  if (a.stance === 'passage' || b.stance === 'passage') return `${passageWhy(g, a.stance === 'passage' ? a : b)} Fleets are joined once it is in.`;
  const together = atSea(a) && atSea(b) ? a.zone === b.zone : !atSea(a) && !atSea(b) && a.base === b.base && a.home === b.home;
  if (!together) return `The ${a.name} and the ${b.name} are not in the same place: bring them into the same harbour, or the same sea.`;
  return null;
}

/** Joins fleet a to fleet b: its ships sail under b's flag and its admiral, and a is struck from the list. */
export function merge(g: Game, a: Fleet, b: Fleet): boolean {
  if (whyNotMerge(g, a, b)) return false;
  const na = shipCount(a.ships), nb = shipCount(b.ships);
  b.ready = nb + na > 0 ? (b.ready * nb + a.ready * na) / (nb + na) : b.ready;
  // Its ships bring their battle stars, in at the back of b's (stars.ts).
  joinStars(a, b);
  for (const c of CLASSES) { b.ships[c] += a.ships[c]; b.lost[c] += a.lost[c]; }
  if (!b.admiral && a.admiral) b.admiral = a.admiral;
  if (a.shelled !== undefined && (b.shelled ?? -1) < a.shelled) b.shelled = a.shelled;
  if (a.attacked !== undefined && (b.attacked ?? -1) < a.attacked) b.attacked = a.attacked;
  b.fought = Math.max(b.fought, a.fought);
  for (const [k, t] of Object.entries(a.seen ?? {})) if ((b.seen?.[k] ?? -99) < t) (b.seen ??= {})[k] = t;
  if (a.struck && (!b.struck || b.struck[0] < a.struck[0] || (b.struck[0] === a.struck[0] && b.struck[1] < a.struck[1]))) b.struck = [a.struck[0], a.struck[1], a.struck[2]];
  // (Release 2, the far chart: their strikes at anchor go into the one fleet's book, each loss now a share of the
  // whole fleet's surface ships, for its staff's weighing of a move: navy.ts anchorLoss.)
  if (a.anchor?.length || b.anchor?.length) {
    // (b's ships already count a's here.)
    const all = shipCount(b.ships) - b.ships.ss, sa = shipCount(a.ships) - a.ships.ss;
    const of = (x: NonNullable<Fleet['anchor']>, n: number): NonNullable<Fleet['anchor']> =>
      x.map((e) => ({ ...e, lost: all > 0 ? (e.lost * n) / all : e.lost, from: e.from.map((o) => ({ ...o })) }));
    b.anchor = [...of(b.anchor ?? [], all - sa), ...of(a.anchor ?? [], sa)];
  }
  g.fleets.splice(g.fleets.indexOf(a), 1);
  moveWings(g, a, b, a.ships.cv, a.ships.cv);
  g.supplyDirty = true;
  return true;
}

/** The fleets this one could join where it is. */
export const mergeTargets = (g: Game, f: Fleet): Fleet[] => g.fleets.filter((x) => x.id !== f.id && whyNotMerge(g, f, x) === null);

// ---- The start, and the computer's admirals ------------------------------------------------------------------------

/**
 * At the opening of a grand campaign the commands tied to a station on their own side's shipping lane are already
 * sailing with the convoys. This covers the player's navy too, which no computer admiral orders about. First the
 * fleets already formed take what their rows say for the grand campaign (Force H's beat, ruling 233 rider b564).
 */
export function openEscorts(g: Game): void {
  if (!seaWar(g)) return;
  openFleetRows(g);
  for (const f of g.fleets) {
    if (!f.tied || !atSea(f) || f.stance !== 'patrol') continue;
    // (Only a lane that carries something laid: navy.ts laneCounts.)
    const rt = seaRoutes(g).find((r) => laneCounts(r) && r.zones.includes(f.zone) && (r.to === f.nat || allied(g, r.to, f.nat)) && whyNotEscort(g, f, r.id) === null);
    if (rt) escort(g, f, rt.id);
  }
}

/** A computer admiral shells the troops on a coast its own army, or an ally's, stands beside. */
export function shoreAI(g: Game, nat: string): void {
  if (!seaWar(g) || !FLEETS.ai.bombard || byHand(g, nat, 'navy') || !g.nations[nat]?.atWar.length) return;
  // Only where the army can follow it up: enemy troops on a coast with one of its own side's armies beside them.
  // A town its own side stands round counts too: the guns break down its works for the assault.
  const sieged = (r: Region): boolean => besieged(g, r) && allied(g, nat, besiegerOf(g, r) ?? '') && townHeld(g, r) && breachOf(g, r) * 100 <= SIEGE.breach.most - 10;
  const aim = (r: Region): ShoreTarget => (sieged(r) ? 'town' : 'troops');
  const worth = (r: Region): number => {
    if (!atWar(g, nat, r.ctrl) || g.flags[`shell:${r.id}`] === g.turn) return 0;
    if (sieged(r)) return planTownHit(g, r, 'shell', 1).divs;
    if (!r.nb.some((j) => g.regions[j].units.some((u) => !u.dead && u.kind !== 'fort' && allied(g, u.nat, nat)))) return 0;
    return foesAt(g, r, nat).reduce((s, c) => s + c.divs, 0);
  };
  for (const f of fleetsOf(g, nat)) {
    if (f.ready < FLEETS.ai.ready || f.shelled === g.turn || gunsRaw(f.ships) < B.least) continue;
    if (atSea(f)) {
      if (f.stance !== 'patrol' || seaSide(g, f.zone, nat, false).total < seaSide(g, f.zone, nat, true).total) continue;
      let best: Region | null = null, bv = 0;
      for (const id of g.seas[f.zone].coast) {
        const r = g.regions[id];
        if (whyNotBombard(g, f, r, aim(r))) continue;
        const v = worth(r);
        if (v > bv) { bv = v; best = r; }
      }
      if (best) bombard(g, f, best, aim(best));
      continue;
    }
    // A fleet lying in harbour with the guns for it puts to sea for the month to shell such a coast, but only into
    // a sea where its side would be much the stronger with it there. Raiders keep to their own work, and a fleet the
    // player keeps in harbour by his own order stays there (1.5.1).
    if (f.duty === 'raid' || !(FLEETS.ai.sortie > 0) || keptByHand(g, f)) continue;
    const mine = fleetPower(g, f);
    let at = -1, best: Region | null = null, bv = 0;
    for (const z of fleetReach(g, f)) {
      if (seaSide(g, z, nat, false).total + mine < FLEETS.ai.sortie * seaSide(g, z, nat, true).total) continue;
      for (const id of g.seas[z].coast) {
        const v = worth(g.regions[id]);
        if (v > bv) { bv = v; best = g.regions[id]; at = z; }
      }
    }
    if (best && sail(g, f, at, 'patrol')) bombard(g, f, best, aim(best));
  }
}
