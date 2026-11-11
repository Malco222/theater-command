import type { Rng } from '../core/rng';
import type { TicketLine, VictoryLevel } from '../core/ticket';

export type Kind = 'inf' | 'mot' | 'arm' | 'cav' | 'mtn' | 'fort';
export type Terrain = 'plains' | 'forest' | 'hills' | 'mountain' | 'marsh' | 'urban';
export type Align = 'allies' | 'allies_cobelligerent' | 'leaning_allies' | 'independent' | 'leaning_axis' | 'axis_cobelligerent' | 'axis';

export interface Region {
  id: number;
  name: string;
  owner: string;
  ctrl: string;
  terrain: Terrain;
  vp: number;
  fac: number;
  cap: boolean;
  port: boolean;
  fort: { lv: number; vs: string[] } | null;
  vichy: boolean;
  sov: boolean;
  oil: boolean;
  x: number;
  y: number;
  nb: number[];
  polys: [number, number][][][];
  units: Corps[];
  /** Turn this region last changed hands. */
  since: number;
}

export interface Nation {
  id: string;
  name: string;
  adj: string;
  color: string;
  quality: number;
  air: number;
  resilience: number;
  align: Align;
  blitz: boolean;
  slowCommand: boolean;
  seaSupply: boolean;
  atWar: string[];
  alive: boolean;
  dormant: boolean;
  capital: number;
  /** Production points in hand. Losses are made good from these first; the rest can be spent on new formations. */
  pp: number;
  /** New formations and works on order. */
  queue: Build[];
  /** Number the next new corps will carry. */
  corpsNo: number;
  armyPeak: number;
  /** Turn each current offensive began, by enemy. */
  offStart: Record<string, number>;
  lastFight: Record<string, number>;
  fellOn?: { turn: number; week: number; to: string };
}

export interface Build {
  id: number;
  what: 'corps' | 'fort';
  kind?: Kind;
  divs?: number;
  name: string;
  region: number;
  /** Turn it was ordered and turn it arrives. */
  ordered: number;
  eta: number;
  cost: number;
}

export interface Corps {
  id: number;
  nat: string;
  name: string;
  kind: Kind;
  divs: number;
  /** 0..1: how much of its men and equipment the corps still has. */
  str: number;
  /** 0..1: cohesion. Drops in battle, comes back with rest and supply. */
  org: number;
  xp: number;
  reg: number;
  /** Weeks of action left this month. */
  ap: number;
  /** Action points this corps started the month with. */
  apMax: number;
  general: string | null;
  trait: 'panzer' | 'attacker' | 'defender' | null;
  hold: boolean;
  dyle: string | null;
  supplied: boolean;
  fought: boolean;
  dead: boolean;
}

export interface ReportUnit {
  name: string;
  nat: string;
  kind: Kind;
  divs: number;
  men: number;
  lost: number;
  fate: '' | 'retreated' | 'surrendered' | 'destroyed' | 'advanced' | 'evacuated';
}

export interface BattleReport {
  id: number;
  turn: number;
  week: number;
  region: number;
  regionName: string;
  attNat: string;
  defNat: string;
  attackers: ReportUnit[];
  defenders: ReportUnit[];
  /** The Battle Ticket: what the campaign map fed into this fight. */
  lines: TicketLine[];
  aPower: number;
  dPower: number;
  odds: number;
  level: VictoryLevel;
  taken: boolean;
  attLost: number;
  defLost: number;
  prisoners: number;
}

export interface Headline {
  turn: number;
  week: number;
  text: string;
  kind: 'war' | 'fall' | 'battle' | 'event' | 'info';
  nat?: string;
  region?: number;
}

export interface PlanPhase {
  name?: string;
  from: number;
  to: number;
  posture: 'attack' | 'defend' | 'redeploy';
  enemies?: string[];
  declare?: string[];
  declareWhenCollapsing?: string;
  collapseAt?: number;
  objectives?: string[];
  /** The line the armoured thrust follows. */
  axis?: string[];
  /** Ground the plan leaves alone in its first month. */
  haltOrder?: string[];
  assembly?: string[];
  armour?: string[];
  garrison?: string[];
  staging?: Record<string, number>;
  minOdds?: number;
  force?: number;
}

export interface Game {
  seed: number;
  rng: Rng;
  turn: number;
  /** Index into order of the nation whose phase it is. */
  phase: number;
  order: string[];
  nations: Record<string, Nation>;
  regions: Region[];
  byName: Record<string, Region>;
  corps: Corps[];
  nextCorps: number;
  nextBuild: number;
  human: string | null;
  reports: BattleReport[];
  headlines: Headline[];
  flags: Record<string, number>;
  supplyDirty: boolean;
  /** Regions each nation can draw supply in, rebuilt when the map changes. */
  supply: Record<string, Uint8Array>;
  capDist: Record<string, Int16Array>;
  week: number;
  links: [number, number][];
  width: number;
  height: number;
  land: [number, number][][][];
}

export const MEN_PER_DIV = 15000;

export const KIND: Record<Kind, { label: string; atk: number; def: number; move: number; mobile: boolean }> = {
  inf: { label: 'Infantry', atk: 10, def: 14, move: 2, mobile: false },
  mot: { label: 'Motorized', atk: 12, def: 12, move: 1, mobile: true },
  arm: { label: 'Armor', atk: 24, def: 11, move: 1, mobile: true },
  cav: { label: 'Cavalry', atk: 8, def: 8, move: 1, mobile: true },
  mtn: { label: 'Mountain', atk: 9, def: 15, move: 2, mobile: false },
  fort: { label: 'Fortress', atk: 1, def: 15, move: 99, mobile: false },
};

export const TERRAIN_DEF: Record<Terrain, number> = { plains: 1, forest: 1.2, hills: 1.25, mountain: 1.6, marsh: 1.3, urban: 1.35 };
export const FORT_MUL = [1, 1.3, 1.7, 2.6];
export const STACK = 5;
