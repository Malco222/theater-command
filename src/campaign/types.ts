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
  /** Turn this region last changed hands, and who held it before. */
  since: number;
  prev: string | null;
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
  /** The most corps this nation's staffs can bring into one battle. 0 means no limit. */
  maxAttack: number;
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
  /** Public opinion, from -100 (wants the Allies) to +100 (wants the Axis), and the leaning it drifts back to. */
  opinion: number;
  home: number;
  /** Political capital: what a government can spend on diplomacy. */
  pc: number;
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
  /** Id of the general in command, if the corps has one. */
  general: number | null;
  hold: boolean;
  dyle: string | null;
  supplied: boolean;
  fought: boolean;
  dead: boolean;
}

export type Skill = 'offense' | 'defense' | 'logistics' | 'leadership';

/** A general's service record: what he has done, which is what his traits are earned from. */
export interface ServiceRecord {
  battles: number;
  won: number;
  attacks: number;
  defences: number;
  wounds: number;
  /** Decisive wins at the head of fast troops. */
  fast: number;
  /** Attacks won when the odds were close. */
  close: number;
  /** Wins that ended with the defenders laying down their arms. */
  enc: number;
  /** Positions held against a stronger attacker. */
  held: number;
  /** Battles fought through a Russian winter without winter equipment. */
  winter: number;
  /** Failed assaults that cost his corps dearly. */
  costly: number;
  /** Defeats in a row. */
  streak: number;
}

export interface General {
  id: number;
  nat: string;
  name: string;
  /** A real commander of the war, or an officer who rose from the ranks in this one. */
  historical: boolean;
  level: number;
  /** Experience toward the next level. */
  xp: number;
  /** Skill points not yet spent. */
  points: number;
  skills: Record<Skill, number>;
  traits: string[];
  /** Id of the corps he commands, or -1 while he waits for a command. */
  corps: number;
  status: 'active' | 'wounded' | 'captured' | 'dead' | 'sacked';
  /** Month a wounded general is fit again. */
  backOn: number;
  /** Month he joined the list, and the month he was last given a new command. */
  joined: number;
  moved: number;
  rec: ServiceRecord;
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
  /** Garrisons in these places are stood down and join the field army. */
  release?: string[];
  staging?: Record<string, number>;
  minOdds?: number;
  force?: number;
  /** Places the plan means to starve out rather than storm: never assaulted while the phase lasts. */
  siege?: string[];
  /** A limited offensive: only places the plan has named as objectives so far are attacked. */
  limited?: boolean;
  /** A single stroke: only these places are attacked, whatever else is in reach. */
  targets?: string[];
  /** The phase only applies while these two nations are at war with each other. */
  ifAtWar?: [string, string];
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
  /** Every general of the war, by id. Generals who are killed, captured or sacked stay on the list. */
  generals: General[];
  nextCorps: number;
  nextBuild: number;
  human: string | null;
  reports: BattleReport[];
  headlines: Headline[];
  flags: Record<string, number>;
  supplyDirty: boolean;
  /** Regions each nation can draw supply in, rebuilt when the map changes. */
  supply: Record<string, Uint8Array>;
  /** How long each nation's supply line is in every region deep inside the Soviet Union. Rebuilt with supply. */
  reach: Record<string, Float32Array>;
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
