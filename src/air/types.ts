import type { AircraftType, FlightState } from './flight';
import type { Rng } from '../core/rng';
import type { BattleResult, BattleTicketHeader } from '../core/ticket';

export type SideId = 0 | 1;

export interface SideDef {
  id: SideId;
  key: string;
  name: string;
  short: string;
  role: 'attacker' | 'defender';
  /** Fighter formation doctrine. The loose pairs of the "schwarm" see more than tight vics. */
  formation: 'vic' | 'schwarm';
  lookout: number;
  namePool: string;
}

export interface Pilot {
  name: string;
  rank: string;
  /** 0 = just out of training, 1 = the best there is. */
  skill: number;
  kills: number;
  killsNow: number;
  fatigue: number;
  fate: 'killed' | 'wounded' | 'captured' | 'missing' | 'rescued' | 'safe' | null;
  note?: string;
}

export type AcMode = 'ground' | 'form' | 'solo' | 'fight' | 'out';

export interface Aircraft extends FlightState {
  id: number;
  sq: Squadron;
  side: SideId;
  pilot: Pilot;
  px: number; py: number; pz: number;
  mode: AcMode;
  /** Flying and able to act. */
  alive: boolean;
  /** Shot down and on its way to the ground. */
  falling: boolean;
  /** No longer in the battle: landed, left the map or crashed. */
  gone: boolean;
  hp: number;
  fuel: number;
  ammo: number[];
  onFire: boolean;
  leak: number;
  enginesOut: number;
  smoking: boolean;
  damaged: boolean;
  slotX: number; slotY: number; slotZ: number;
  tgt: Aircraft | null;
  threat: Aircraft | null;
  ai: 'attack' | 'extend' | 'evade';
  aiT: number;
  thinkAt: number;
  firing: boolean;
  aware: boolean;
  bombs: boolean;
  /** How many enemies currently have this aircraft as their target. */
  hunters: number;
  spin: number;
}

export type Order =
  | { k: 'hold' }
  | { k: 'patrol'; x: number; y: number }
  | { k: 'intercept'; raid?: string; sq?: number }
  | { k: 'escort'; sq: number; mode: 'close' | 'free' }
  | { k: 'bomb' }
  | { k: 'rtb' };

export type SqState = 'ready' | 'scramble' | 'transit' | 'engaged' | 'reform' | 'rtb' | 'landed' | 'lost';

export interface Squadron {
  id: number;
  side: SideId;
  name: string;
  group: string;
  /** The raid this unit flies in, as the other side's plotters would label it. */
  raid: string;
  type: AircraftType;
  ac: Aircraft[];
  leader: Aircraft | null;
  base: { name: string; x: number; y: number };
  order: Order;
  queue: Order[];
  alt: number;
  stance: 'bombers' | 'fighters';
  /** True while the side's controller AI may re-task this unit. A direct order from the player clears it. */
  auto: boolean;
  state: SqState;
  stateT: number;
  morale: number;
  engaged: Squadron[];
  tgtSq: Squadron | null;
  x: number; y: number; z: number;
  hdg: number;
  spd: number;
  alive: number;
  started: number;
  lost: number;
  kills: number;
  quality: number;
  route: { x: number; y: number }[] | null;
  routeIdx: number;
  bombed: boolean;
  bombsHit: number;
  slot: number;
  /** Weave amplitude in radians, for escorts holding station on slower bombers. */
  weave: number;
  nav: { x: number; y: number; alt: number; spd: number; g: number };
  bingo: boolean;
  surprisedT: number;
  shocked: boolean;
  thinkAt: number;
  scrambleSec: number;
  isBomber: boolean;
  /** Seconds of fighting time left before fuel forces the unit home. */
  combatSec: number;
  sawEnemy: boolean;
}

export interface Contact {
  /** 0 = nothing, 1 = a plot from radar or ground observers, 2 = seen by friendly aircraft. */
  level: 0 | 1 | 2;
  src: 'radar' | 'observer' | 'visual' | '';
  x: number; y: number; z: number;
  hdg: number;
  spd: number;
  est: number;
  /** Whether the plotters know what it is (bombers or fighters). */
  known: boolean;
  t: number;
  nextPlot: number;
}

export interface Place {
  name: string;
  kind: 'city' | 'town' | 'airfield' | 'field' | 'radar' | 'target';
  x: number;
  y: number;
  side?: SideId;
}

export interface SquadronDef {
  side: SideId;
  name: string;
  type: string;
  count: number;
  base: string;
  group: string;
  raid?: string;
  quality: number;
  pool?: string;
  /** Seconds from the order to wheels-up. Left out for units that start in the air. */
  scramble?: number;
  air?: { lat: number; lon: number; alt: number; hdgDeg: number };
  order?: 'bomb' | 'close' | 'free' | 'top';
  charge?: string;
  stance?: 'bombers' | 'fighters';
  fuelFrac?: number;
}

export interface ScenarioDef {
  id: string;
  title: string;
  clock: string;
  ticket: BattleTicketHeader;
  sides: [SideDef, SideDef];
  places: { name: string; kind: Place['kind']; lat: number; lon: number; side?: SideId }[];
  radars: { name: string; lat: number; lon: number; rangeKm: number }[];
  flak: { name: string; lat: number; lon: number; radiusKm: number; power: number }[];
  target: { name: string; lat: number; lon: number };
  route: [number, number][];
  bombAlt: number;
  sun: { azDeg: number; elDeg: number };
  mods: {
    scrambleMul: number;
    defenderFatigue: number;
    bombAccuracy: number;
    spotMul: number;
    /** Morale hit to the attacker when the defender puts up more than expected. */
    shockAt: number;
    shockMorale: number;
    radarKm: number;
  };
  squadrons: SquadronDef[];
}

export type SimEvent =
  | { t: 'shot'; a: Aircraft; b: Aircraft; heavy: boolean }
  | { t: 'gun'; ax: number; ay: number; az: number; b: Aircraft }
  | { t: 'boom'; x: number; y: number; z: number; big: boolean }
  | { t: 'flak'; x: number; y: number; z: number }
  | { t: 'chute'; x: number; y: number; z: number; side: SideId }
  | { t: 'bombs'; x: number; y: number; n: number }
  | { t: 'crash'; x: number; y: number };

export interface LogLine {
  time: number;
  side: SideId | -1;
  text: string;
  kind: 'info' | 'good' | 'bad' | 'warn';
  x?: number;
  y?: number;
}

export interface World {
  def: ScenarioDef;
  seed: number;
  rng: Rng;
  time: number;
  tick: number;
  clock0: number;
  sides: [SideDef, SideDef];
  squadrons: Squadron[];
  aircraft: Aircraft[];
  contacts: [Contact[], Contact[]];
  places: Place[];
  radars: { name: string; x: number; y: number; range: number }[];
  flak: { name: string; x: number; y: number; r: number; power: number }[];
  target: { name: string; x: number; y: number };
  sun: { x: number; y: number; z: number };
  events: SimEvent[];
  log: LogLine[];
  /** Which side a person is commanding (-1 for a watched or auto-resolved battle). */
  human: SideId | -1;
  /** Let the controller AI run any of the player's units that have no direct order. */
  assist: boolean;
  bombsDropped: number;
  bombsHit: number;
  bombsCarried: number;
  shockDone: boolean;
  over: boolean;
  result: BattleResult | null;
  /** Land test supplied by the map: is this point over the defender's home soil? */
  overHome: (x: number, y: number) => 0 | 1 | 2;
}
